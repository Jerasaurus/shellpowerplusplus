#include "iv_trace.h"
#include <math.h>
#include <string.h>

//------------------------------------------------------------------------------
// IV Trace Functions
//------------------------------------------------------------------------------

float IVTrace_Pmp(const IVTrace *trace) {
    return trace->Vmp * trace->Imp;
}

float IVTrace_FillFactor(const IVTrace *trace) {
    if (trace->Isc <= 0 || trace->Voc <= 0) return 0;
    return IVTrace_Pmp(trace) / (trace->Isc * trace->Voc);
}

// Binary search interpolation helper
static float LinInterp(const float *xs, const float *ys, int n, float x0, bool ascending) {
    if (n < 2) return ys[0];

    // Binary search
    int ix0 = 0, ix1 = n - 1;
    while (ix0 < ix1 - 1) {
        int ixmid = (ix1 + ix0) / 2;
        if ((x0 > xs[ixmid]) == ascending) {
            ix0 = ixmid;
        } else {
            ix1 = ixmid;
        }
    }

    // Linear interpolation
    float dx = xs[ix1] - xs[ix0];
    if (fabsf(dx) < 1e-9f) return ys[ix0];

    float t = (x0 - xs[ix0]) / dx;
    return t * ys[ix1] + (1.0f - t) * ys[ix0];
}

float IVTrace_InterpV(const IVTrace *trace, float current) {
    if (trace->n_samples < 2) return 0;
    // I array is descending (Isc to 0), so ascending=false
    return LinInterp(trace->I, trace->V, trace->n_samples, current, false);
}

float IVTrace_InterpI(const IVTrace *trace, float voltage) {
    if (trace->n_samples < 2) return 0;
    // V array is ascending (0 to Voc), so ascending=true
    return LinInterp(trace->V, trace->I, trace->n_samples, voltage, true);
}

void IVTrace_CreateSimple(IVTrace *trace, float voc, float isc, float vmp, float imp,
                          float irradiance_ratio) {
    memset(trace, 0, sizeof(IVTrace));

    // Scale Isc with irradiance (current is proportional to light)
    float scaled_isc = isc * irradiance_ratio;
    // Voc changes logarithmically with irradiance, but for simplicity we keep it close
    float scaled_voc = voc;
    if (irradiance_ratio > 0.01f) {
        // Voc = Voc_stc + n*Vt*ln(G/G_stc), simplified
        scaled_voc = voc + 0.026f * logf(irradiance_ratio);
        if (scaled_voc < 0) scaled_voc = 0;
    } else {
        scaled_voc = 0;
    }

    // Scale Vmp and Imp similarly
    float scaled_imp = imp * irradiance_ratio;
    float scaled_vmp = vmp;
    if (irradiance_ratio > 0.01f) {
        scaled_vmp = vmp + 0.026f * logf(irradiance_ratio);
        if (scaled_vmp < 0) scaled_vmp = 0;
        if (scaled_vmp > scaled_voc) scaled_vmp = scaled_voc * 0.85f;
    } else {
        scaled_vmp = 0;
    }

    // Create a simple 3-point IV curve
    // Point 0: Short circuit (V=0, I=Isc)
    // Point 1: Max power point
    // Point 2: Open circuit (V=Voc, I=0)

    int n = 50; // Number of samples for smoother curve
    trace->n_samples = n;

    for (int i = 0; i < n; i++) {
        float t = (float)i / (float)(n - 1);
        // Parametric curve from (0, Isc) to (Voc, 0) through (Vmp, Imp)
        // Use a simple exponential model: I = Isc * (1 - exp((V - Voc) / Vt))
        float v = t * scaled_voc;
        float current;

        if (scaled_voc > 0 && scaled_isc > 0) {
            // Simple single-diode model approximation
            float vt = 0.026f * 1.3f; // Thermal voltage * ideality
            current = scaled_isc * (1.0f - expf((v - scaled_voc) / (vt * 10.0f)));
            if (current < 0) current = 0;
            if (current > scaled_isc) current = scaled_isc;
        } else {
            current = 0;
        }

        trace->V[i] = v;
        trace->I[i] = current;
    }

    // Arrays are already in correct order: V ascending (0 to Voc), I descending (Isc to 0)
    trace->Voc = scaled_voc;
    trace->Isc = scaled_isc;
    trace->Vmp = scaled_vmp;
    trace->Imp = scaled_imp;
}

void IVTrace_CreateCellTrace(IVTrace *trace, float voc, float isc, float n_ideal,
                              float series_r, float irradiance_ratio) {
    memset(trace, 0, sizeof(IVTrace));

    if (irradiance_ratio <= 0.001f) {
        trace->n_samples = 2;
        trace->I[0] = 0;
        trace->V[0] = 0;
        trace->I[1] = 0;
        trace->V[1] = 0;
        return;
    }

    // Scale parameters with irradiance
    float Iph = isc * irradiance_ratio; // Photo-generated current
    float Vt = 0.026f; // Thermal voltage at 25C

    // Voc adjustment with irradiance
    float scaled_voc = voc;
    if (irradiance_ratio > 0.01f) {
        scaled_voc = voc + n_ideal * Vt * logf(irradiance_ratio);
        if (scaled_voc < 0) scaled_voc = 0;
    }

    // Generate IV curve using single-diode model
    // I = Iph - I0 * (exp((V + I*Rs)/(n*Vt)) - 1)
    // Simplified: I = Iph * (1 - exp((V - Voc)/(n*Vt)))

    int n = IV_TRACE_MAX_SAMPLES;
    trace->n_samples = n;

    float max_power = 0;
    int mp_idx = 0;

    for (int i = 0; i < n; i++) {
        float t = (float)i / (float)(n - 1);
        float v = t * scaled_voc;

        // Single diode equation (simplified, neglecting series resistance in iteration)
        float exponent = (v - scaled_voc) / (n_ideal * Vt);
        if (exponent > 20.0f) exponent = 20.0f; // Clamp to avoid overflow
        float current = Iph * (1.0f - expf(exponent));

        // Account for series resistance (first-order correction)
        if (series_r > 0 && current > 0) {
            v = v - current * series_r;
            if (v < 0) v = 0;
        }

        if (current < 0) current = 0;
        if (current > Iph) current = Iph;

        trace->V[i] = v;
        trace->I[i] = current;

        float power = v * current;
        if (power > max_power) {
            max_power = power;
            mp_idx = i;
        }
    }

    // Arrays are already in correct order: V ascending (0 to Voc), I descending (Isc to 0)
    trace->Voc = scaled_voc;
    trace->Isc = Iph;
    trace->Vmp = trace->V[mp_idx];
    trace->Imp = trace->I[mp_idx];
}
