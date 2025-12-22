#include "string_sim.h"
#include <math.h>
#include <float.h>
#include <string.h>
#include <stdlib.h>

//------------------------------------------------------------------------------
// Helper Functions
//------------------------------------------------------------------------------

float StringSim_CalcCellCurrent(float isc_stc, float irradiance, float cos_angle) {
    if (cos_angle <= 0 || irradiance <= 0) return 0;
    // Current is proportional to irradiance and cosine of incidence angle
    float irradiance_ratio = irradiance / 1000.0f; // STC = 1000 W/m2
    return isc_stc * irradiance_ratio * cos_angle;
}

float StringSim_CalcCellVoltage(float voc, float isc, float n_ideal,
                                 float operating_current, float irradiance_ratio) {
    if (irradiance_ratio <= 0.001f || isc <= 0) return 0;

    // Scale Isc with irradiance
    float Iph = isc * irradiance_ratio;

    // If operating current exceeds photo-current, cell is reverse biased
    if (operating_current >= Iph) return -FLT_MAX;

    // Simplified single-diode model: V = Voc + n*Vt*ln(1 - I/Iph)
    float Vt = 0.026f; // Thermal voltage at 25C
    float ratio = 1.0f - (operating_current / Iph);
    if (ratio <= 0) return -FLT_MAX;

    // Adjust Voc for irradiance
    float scaled_voc = voc;
    if (irradiance_ratio > 0.01f) {
        scaled_voc = voc + n_ideal * Vt * logf(irradiance_ratio);
        if (scaled_voc < 0) scaled_voc = 0;
    }

    float voltage = scaled_voc + n_ideal * Vt * logf(ratio);
    return voltage > 0 ? voltage : 0;
}

//------------------------------------------------------------------------------
// Main String Simulation Functions
//------------------------------------------------------------------------------

void StringSim_CalcStringIV(const IVTrace *cell_traces, int n_cells,
                            float bypass_v_drop, const bool *has_bypass,
                            StringSimResult *result) {
    memset(result, 0, sizeof(StringSimResult));

    if (n_cells <= 0) return;

    // Find maximum Isc across all cells (string can't exceed this)
    float max_isc = 0;
    for (int i = 0; i < n_cells; i++) {
        if (cell_traces[i].Isc > max_isc) {
            max_isc = cell_traces[i].Isc;
        }
    }

    if (max_isc <= 0) return;

    // Sweep current and compute string voltage at each point
    int n_samples = STRING_SIM_SAMPLES;
    int n_good_samples = n_samples;

    float *vec_i = result->iv_trace.I;
    float *vec_v = result->iv_trace.V;

    // Node voltages (one between each cell, plus one at each end)
    float *node_v = (float *)malloc((n_cells + 1) * sizeof(float));

    for (int i = 0; i < n_samples; i++) {
        float current = (float)i * max_isc / (float)(n_samples - 1);

        // Calculate voltage at each node
        node_v[0] = 0; // String starts at ground

        for (int j = 1; j <= n_cells; j++) {
            // Voltage assuming cell is active (not bypassed)
            float cell_v;
            if (current < cell_traces[j - 1].Isc) {
                cell_v = IVTrace_InterpV(&cell_traces[j - 1], current);
            } else {
                // Current exceeds cell's capability - infinite negative voltage
                cell_v = -FLT_MAX;
            }

            float node_v_active = node_v[j - 1] + cell_v;

            // Check if bypass diode gives better voltage
            if (has_bypass && has_bypass[j - 1]) {
                float node_v_bypass = node_v[j - 1] - bypass_v_drop;
                node_v[j] = fmaxf(node_v_active, node_v_bypass);
            } else {
                node_v[j] = node_v_active;
            }
        }

        vec_i[i] = current;
        vec_v[i] = fmaxf(node_v[n_cells], 0);

        // Track where the trace becomes invalid (current too high)
        if (node_v[n_cells] >= 0) {
            // Valid point
        } else if (n_good_samples == n_samples) {
            n_good_samples = i;
        }
    }

    free(node_v);

    // Limit to valid samples
    n_good_samples = (n_good_samples < n_samples) ? n_good_samples + 1 : n_samples;
    if (n_good_samples < 2) n_good_samples = 2;
    result->iv_trace.n_samples = n_good_samples;

    // Find maximum power point
    float max_power = 0;
    int mp_idx = 0;
    for (int i = 0; i < n_good_samples; i++) {
        float power = vec_i[i] * vec_v[i];
        if (power > max_power) {
            max_power = power;
            mp_idx = i;
        }
    }

    // Set trace summary values
    result->iv_trace.Isc = vec_i[n_good_samples - 1];
    result->iv_trace.Voc = vec_v[0];
    result->iv_trace.Imp = vec_i[mp_idx];
    result->iv_trace.Vmp = vec_v[mp_idx];

    // Set result values
    result->power_out = max_power;
    result->voltage = vec_v[mp_idx];
    result->current = vec_i[mp_idx];

    // Count bypassed cells at MPP
    result->cells_bypassed = 0;
    float mpp_current = vec_i[mp_idx];
    for (int i = 0; i < n_cells; i++) {
        if (has_bypass && has_bypass[i] && mpp_current >= cell_traces[i].Isc) {
            result->cells_bypassed++;
        }
    }
}

float StringSim_CalcPowerSimple(const float *cell_currents, const float *cell_vmp,
                                 int n_cells, float bypass_v_drop,
                                 const bool *has_bypass, bool *out_bypassed) {
    if (n_cells <= 0) return 0;

    // Find the minimum current (this limits the string)
    float min_current = FLT_MAX;
    for (int i = 0; i < n_cells; i++) {
        // Only consider cells without bypass or with enough current
        if (!has_bypass || !has_bypass[i]) {
            if (cell_currents[i] < min_current) {
                min_current = cell_currents[i];
            }
        }
    }

    // If all cells have bypass diodes, find min among cells producing current
    if (min_current == FLT_MAX) {
        for (int i = 0; i < n_cells; i++) {
            if (cell_currents[i] < min_current && cell_currents[i] > 0) {
                min_current = cell_currents[i];
            }
        }
    }

    if (min_current == FLT_MAX || min_current <= 0) {
        // All cells are shaded
        if (out_bypassed) {
            for (int i = 0; i < n_cells; i++) out_bypassed[i] = true;
        }
        return 0;
    }

    // Calculate total voltage
    float total_voltage = 0;
    int bypassed_count = 0;

    for (int i = 0; i < n_cells; i++) {
        bool bypassed = false;

        if (cell_currents[i] < min_current) {
            // This cell can't provide string current
            if (has_bypass && has_bypass[i]) {
                // Bypass diode conducts
                bypassed = true;
                total_voltage -= bypass_v_drop;
                bypassed_count++;
            } else {
                // No bypass - this cell is limiting
                // (shouldn't happen since we found min, but handle it)
                total_voltage += cell_vmp[i] * (cell_currents[i] / min_current);
            }
        } else {
            // Cell can provide the current
            total_voltage += cell_vmp[i];
        }

        if (out_bypassed) out_bypassed[i] = bypassed;
    }

    if (total_voltage < 0) total_voltage = 0;

    return min_current * total_voltage;
}
