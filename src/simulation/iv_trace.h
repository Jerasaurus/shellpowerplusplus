#ifndef IV_TRACE_H
#define IV_TRACE_H

#include <stdbool.h>

//------------------------------------------------------------------------------
// Constants
//------------------------------------------------------------------------------
#define IV_TRACE_MAX_SAMPLES 200

//------------------------------------------------------------------------------
// IV Trace - Current-Voltage characteristic curve
//------------------------------------------------------------------------------
typedef struct {
    float I[IV_TRACE_MAX_SAMPLES]; // Current samples (A)
    float V[IV_TRACE_MAX_SAMPLES]; // Voltage samples (V)
    int n_samples;                  // Number of valid samples
    float Voc;                      // Open circuit voltage
    float Isc;                      // Short circuit current
    float Vmp;                      // Voltage at max power
    float Imp;                      // Current at max power
} IVTrace;

//------------------------------------------------------------------------------
// Functions
//------------------------------------------------------------------------------

// Get maximum power point power (Vmp * Imp)
float IVTrace_Pmp(const IVTrace *trace);

// Get fill factor (Pmp / (Isc * Voc))
float IVTrace_FillFactor(const IVTrace *trace);

// Interpolate voltage at a given current
float IVTrace_InterpV(const IVTrace *trace, float current);

// Interpolate current at a given voltage
float IVTrace_InterpI(const IVTrace *trace, float voltage);

// Create an IV trace for a single cell given irradiance conditions
// irradiance_ratio: fraction of full sun (0-1) based on angle and shading
// Returns the trace with properly scaled Isc
void IVTrace_CreateCellTrace(IVTrace *trace, float voc, float isc, float n_ideal,
                              float series_r, float irradiance_ratio);

// Create a simple linear approximation trace (faster, less accurate)
void IVTrace_CreateSimple(IVTrace *trace, float voc, float isc, float vmp, float imp,
                          float irradiance_ratio);

#endif // IV_TRACE_H
