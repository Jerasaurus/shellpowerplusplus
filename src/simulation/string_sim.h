#ifndef STRING_SIM_H
#define STRING_SIM_H

#include "iv_trace.h"
#include <stdbool.h>

//------------------------------------------------------------------------------
// Constants
//------------------------------------------------------------------------------
#define STRING_SIM_MAX_CELLS 100
#define STRING_SIM_SAMPLES 200

//------------------------------------------------------------------------------
// String simulation structures
//------------------------------------------------------------------------------

// Result of simulating a single string
typedef struct {
    float power_out;         // Power output at MPP (W)
    float voltage;           // String voltage at MPP (V)
    float current;           // String current at MPP (A)
    float power_ideal;       // Ideal power if all cells were in full sun
    int cells_bypassed;      // Number of cells being bypassed
    IVTrace iv_trace;        // Full IV trace for the string
} StringSimResult;

// Cell operating state within a string
typedef struct {
    bool is_bypassed;        // True if bypass diode is conducting
    float voltage;           // Cell voltage at operating point
    float current;           // Cell current (same as string current when not bypassed)
} CellOperatingState;

//------------------------------------------------------------------------------
// Functions
//------------------------------------------------------------------------------

// Calculate the IV trace for a series string of cells
// cell_traces: Array of IV traces for each cell (already scaled for irradiance)
// n_cells: Number of cells in the string
// bypass_v_drop: Bypass diode forward voltage (typically 0.3-0.7V)
// has_bypass: Array indicating which cells have bypass diodes
// result: Output string simulation result
void StringSim_CalcStringIV(const IVTrace *cell_traces, int n_cells,
                            float bypass_v_drop, const bool *has_bypass,
                            StringSimResult *result);

// Simplified string power calculation (faster, less accurate)
// Returns string power accounting for current limiting
// cell_currents: Photo-generated current for each cell
// cell_vmp: Voltage at max power for each cell
// n_cells: Number of cells
// bypass_v_drop: Bypass diode forward voltage
// has_bypass: Array indicating which cells have bypass diodes
// out_bypassed: Output array indicating which cells are bypassed (can be NULL)
float StringSim_CalcPowerSimple(const float *cell_currents, const float *cell_vmp,
                                 int n_cells, float bypass_v_drop,
                                 const bool *has_bypass, bool *out_bypassed);

// Calculate cell photo-current based on conditions
// isc_stc: Short circuit current at STC (1000 W/m2)
// irradiance: Actual irradiance (W/m2)
// cos_angle: Cosine of angle between sun and cell normal
// Returns: Photo-generated current (A)
float StringSim_CalcCellCurrent(float isc_stc, float irradiance, float cos_angle);

// Calculate cell voltage at a given operating current
// Uses simplified single-diode model
float StringSim_CalcCellVoltage(float voc, float isc, float n_ideal,
                                 float operating_current, float irradiance_ratio);

#endif // STRING_SIM_H
