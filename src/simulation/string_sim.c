#include "string_sim.h"
#include <math.h>
#include <float.h>
#include <string.h>
#include <stdlib.h>

float StringSim_CalcCellCurrent(float isc_stc, float irradiance, float cos_angle) {
    if (cos_angle <= 0 || irradiance <= 0) return 0;
    float irradiance_ratio = irradiance / 1000.0f;  // STC = 1000 W/m2
    return isc_stc * irradiance_ratio * cos_angle;
}

float StringSim_CalcCellVoltage(float voc, float isc, float n_ideal,
                                 float operating_current, float irradiance_ratio) {
    if (irradiance_ratio <= 0.001f || isc <= 0) return 0;

    float Iph = isc * irradiance_ratio;
    if (operating_current >= Iph) return -FLT_MAX;  // Reverse biased

    // Single-diode model: V = Voc + n*Vt*ln(1 - I/Iph)
    float Vt = 0.026f;  // Thermal voltage at 25C
    float ratio = 1.0f - (operating_current / Iph);
    if (ratio <= 0) return -FLT_MAX;

    float scaled_voc = voc;
    if (irradiance_ratio > 0.01f) {
        scaled_voc = voc + n_ideal * Vt * logf(irradiance_ratio);
        if (scaled_voc < 0) scaled_voc = 0;
    }

    float voltage = scaled_voc + n_ideal * Vt * logf(ratio);
    return voltage > 0 ? voltage : 0;
}

void StringSim_CalcStringIV(const IVTrace *cell_traces, int n_cells,
                            float bypass_v_drop, const bool *has_bypass,
                            StringSimResult *result) {
    memset(result, 0, sizeof(StringSimResult));
    if (n_cells <= 0) return;

    float max_isc = 0;
    for (int i = 0; i < n_cells; i++) {
        if (cell_traces[i].Isc > max_isc) max_isc = cell_traces[i].Isc;
    }
    if (max_isc <= 0) return;

    int n_samples = STRING_SIM_SAMPLES;
    int n_good_samples = n_samples;
    float *vec_i = result->iv_trace.I;
    float *vec_v = result->iv_trace.V;
    float *node_v = (float *)malloc((n_cells + 1) * sizeof(float));

    for (int i = 0; i < n_samples; i++) {
        float current = (float)i * max_isc / (float)(n_samples - 1);
        node_v[0] = 0;

        for (int j = 1; j <= n_cells; j++) {
            float cell_v;
            if (current < cell_traces[j - 1].Isc) {
                cell_v = IVTrace_InterpV(&cell_traces[j - 1], current);
            } else {
                cell_v = -FLT_MAX;
            }

            float node_v_active = node_v[j - 1] + cell_v;

            if (has_bypass && has_bypass[j - 1]) {
                float node_v_bypass = node_v[j - 1] - bypass_v_drop;
                node_v[j] = fmaxf(node_v_active, node_v_bypass);
            } else {
                node_v[j] = node_v_active;
            }
        }

        vec_i[i] = current;
        vec_v[i] = fmaxf(node_v[n_cells], 0);

        if (node_v[n_cells] < 0 && n_good_samples == n_samples) {
            n_good_samples = i;
        }
    }

    free(node_v);

    n_good_samples = (n_good_samples < n_samples) ? n_good_samples + 1 : n_samples;
    if (n_good_samples < 2) n_good_samples = 2;
    result->iv_trace.n_samples = n_good_samples;

    float max_power = 0;
    int mp_idx = 0;
    for (int i = 0; i < n_good_samples; i++) {
        float power = vec_i[i] * vec_v[i];
        if (power > max_power) {
            max_power = power;
            mp_idx = i;
        }
    }

    result->iv_trace.Isc = vec_i[n_good_samples - 1];
    result->iv_trace.Voc = vec_v[0];
    result->iv_trace.Imp = vec_i[mp_idx];
    result->iv_trace.Vmp = vec_v[mp_idx];
    result->power_out = max_power;
    result->voltage = vec_v[mp_idx];
    result->current = vec_i[mp_idx];

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

    // String current limited by lowest non-bypassed cell
    float min_current = FLT_MAX;
    for (int i = 0; i < n_cells; i++) {
        if (!has_bypass || !has_bypass[i]) {
            if (cell_currents[i] < min_current) min_current = cell_currents[i];
        }
    }

    if (min_current == FLT_MAX) {
        for (int i = 0; i < n_cells; i++) {
            if (cell_currents[i] < min_current && cell_currents[i] > 0) {
                min_current = cell_currents[i];
            }
        }
    }

    if (min_current == FLT_MAX || min_current <= 0) {
        if (out_bypassed) {
            for (int i = 0; i < n_cells; i++) out_bypassed[i] = true;
        }
        return 0;
    }

    float total_voltage = 0;

    for (int i = 0; i < n_cells; i++) {
        bool bypassed = false;

        if (cell_currents[i] < min_current) {
            if (has_bypass && has_bypass[i]) {
                bypassed = true;
                total_voltage -= bypass_v_drop;
            } else {
                total_voltage += cell_vmp[i] * (cell_currents[i] / min_current);
            }
        } else {
            total_voltage += cell_vmp[i];
        }

        if (out_bypassed) out_bypassed[i] = bypassed;
    }

    if (total_voltage < 0) total_voltage = 0;
    return min_current * total_voltage;
}

// Segment bypass: activates smallest segment covering any weak cell
void StringSim_CalcStringIVSegments(const IVTrace *cell_traces, int n_cells,
                                     const SegmentBypass *segments, int n_segments,
                                     StringSimResult *result, bool *segment_bypassed) {
    memset(result, 0, sizeof(StringSimResult));
    if (n_cells <= 0) return;

    float max_isc = 0;
    for (int i = 0; i < n_cells; i++) {
        if (cell_traces[i].Isc > max_isc) max_isc = cell_traces[i].Isc;
    }
    if (max_isc <= 0) return;

    int seg_size[STRING_SIM_MAX_SEGMENTS];
    for (int s = 0; s < n_segments; s++) {
        seg_size[s] = segments[s].end_idx - segments[s].start_idx + 1;
    }

    int n_samples = STRING_SIM_SAMPLES;
    float *vec_i = result->iv_trace.I;
    float *vec_v = result->iv_trace.V;
    bool seg_bypassed_at_mpp[STRING_SIM_MAX_SEGMENTS] = {false};
    float max_power = 0;
    int mp_idx = 0;

    for (int sample = 0; sample < n_samples; sample++) {
        float current = (float)sample * max_isc / (float)(n_samples - 1);

        // Find cells that can't provide this current
        bool cell_needs_bypass[STRING_SIM_MAX_CELLS] = {false};
        for (int i = 0; i < n_cells; i++) {
            if (current >= cell_traces[i].Isc) cell_needs_bypass[i] = true;
        }

        // Activate smallest segment covering each weak cell
        bool seg_bypassed[STRING_SIM_MAX_SEGMENTS] = {false};
        for (int i = 0; i < n_cells; i++) {
            if (!cell_needs_bypass[i]) continue;

            int best_seg = -1;
            int best_size = n_cells + 1;
            for (int s = 0; s < n_segments; s++) {
                if (i >= segments[s].start_idx && i <= segments[s].end_idx) {
                    if (seg_size[s] < best_size) {
                        best_size = seg_size[s];
                        best_seg = s;
                    }
                }
            }
            if (best_seg >= 0) seg_bypassed[best_seg] = true;
        }

        // Determine which cells are actually bypassed (smallest segment wins)
        bool cell_is_bypassed[STRING_SIM_MAX_CELLS] = {false};
        for (int i = 0; i < n_cells; i++) {
            int smallest_bypassed_size = n_cells + 1;
            int smallest_active_size = n_cells + 1;

            for (int s = 0; s < n_segments; s++) {
                if (i >= segments[s].start_idx && i <= segments[s].end_idx) {
                    if (seg_bypassed[s]) {
                        if (seg_size[s] < smallest_bypassed_size)
                            smallest_bypassed_size = seg_size[s];
                    } else {
                        if (seg_size[s] < smallest_active_size)
                            smallest_active_size = seg_size[s];
                    }
                }
            }

            if (smallest_bypassed_size < n_cells + 1 &&
                (smallest_active_size >= smallest_bypassed_size)) {
                cell_is_bypassed[i] = true;
            }
        }

        // Sum string voltage
        float total_voltage = 0;
        bool diode_counted[STRING_SIM_MAX_SEGMENTS] = {false};

        for (int i = 0; i < n_cells; i++) {
            if (cell_is_bypassed[i]) {
                for (int s = 0; s < n_segments; s++) {
                    if (seg_bypassed[s] && i >= segments[s].start_idx && i <= segments[s].end_idx) {
                        if (!diode_counted[s]) {
                            total_voltage -= segments[s].v_drop;
                            diode_counted[s] = true;
                        }
                        break;
                    }
                }
            } else {
                float cell_v;
                if (current < cell_traces[i].Isc) {
                    cell_v = IVTrace_InterpV(&cell_traces[i], current);
                } else {
                    cell_v = -FLT_MAX;  // No bypass available
                }
                total_voltage += cell_v;
            }
        }

        if (total_voltage < 0) total_voltage = 0;

        vec_i[sample] = current;
        vec_v[sample] = total_voltage;

        float power = current * total_voltage;
        if (power > max_power) {
            max_power = power;
            mp_idx = sample;
            for (int s = 0; s < n_segments; s++) {
                seg_bypassed_at_mpp[s] = seg_bypassed[s];
            }
        }
    }

    result->iv_trace.n_samples = n_samples;
    result->iv_trace.Isc = vec_i[n_samples - 1];
    result->iv_trace.Voc = vec_v[0];
    result->iv_trace.Imp = vec_i[mp_idx];
    result->iv_trace.Vmp = vec_v[mp_idx];
    result->power_out = max_power;
    result->voltage = vec_v[mp_idx];
    result->current = vec_i[mp_idx];

    // Count bypassed cells at MPP
    result->cells_bypassed = 0;
    for (int i = 0; i < n_cells; i++) {
        for (int s = 0; s < n_segments; s++) {
            if (seg_bypassed_at_mpp[s] && i >= segments[s].start_idx && i <= segments[s].end_idx) {
                bool has_smaller_active = false;
                for (int s2 = 0; s2 < n_segments; s2++) {
                    if (!seg_bypassed_at_mpp[s2] &&
                        i >= segments[s2].start_idx && i <= segments[s2].end_idx &&
                        seg_size[s2] < seg_size[s]) {
                        has_smaller_active = true;
                        break;
                    }
                }
                if (!has_smaller_active) {
                    result->cells_bypassed++;
                    break;
                }
            }
        }
    }

    if (segment_bypassed) {
        for (int s = 0; s < n_segments; s++) {
            segment_bypassed[s] = seg_bypassed_at_mpp[s];
        }
    }
}
