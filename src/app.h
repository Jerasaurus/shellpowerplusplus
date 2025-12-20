#ifndef APP_H
#define APP_H

#include "raylib.h"
#include "raymath.h"
#include <stdbool.h>

//------------------------------------------------------------------------------
// Constants
//------------------------------------------------------------------------------
#define MAX_CELLS 1000
#define MAX_STRINGS 50
#define MAX_CELLS_PER_STRING 100
#define MAX_PATH_LENGTH 512
#define MAX_MODULES 50
#define MAX_CELLS_PER_MODULE 100
#define MAX_MODULE_NAME 64
#define MODULES_DIRECTORY "modules"

#define CELL_SURFACE_OFFSET 0.002f  // Offset above mesh surface
#define MIN_CELL_DISTANCE_FACTOR 1.05f  // Slightly more than 1.0 to prevent any overlap
#define MIN_UPWARD_NORMAL 0.3f

//------------------------------------------------------------------------------
// Colors
//------------------------------------------------------------------------------
#define COLOR_MESH (Color){204, 204, 204, 230}
#define COLOR_CELL_UNWIRED (Color){51, 102, 204, 230}
#define COLOR_CELL_SHADED (Color){128, 128, 128, 230}
#define COLOR_BACKGROUND (Color){245, 245, 245, 255}
#define COLOR_PANEL (Color){230, 230, 230, 255}

//------------------------------------------------------------------------------
// Enums
//------------------------------------------------------------------------------
typedef enum {
    MODE_IMPORT = 0,
    MODE_CELL_PLACEMENT,
    MODE_WIRING,
    MODE_SIMULATION
} AppMode;

//------------------------------------------------------------------------------
// Data Structures
//------------------------------------------------------------------------------

// Solar cell preset specifications
typedef struct {
    const char* name;
    float width;        // meters
    float height;       // meters
    float efficiency;   // 0-1
    float voc;          // Open circuit voltage
    float isc;          // Short circuit current
    float vmp;          // Voltage at max power
    float imp;          // Current at max power
} CellPreset;

// Individual solar cell instance
typedef struct {
    int id;
    Vector3 local_position;  // Position in mesh-local coordinates (before transform)
    Vector3 local_tangent;
    Vector3 local_normal;    // Normal in mesh-local coordinates
    int string_id;          // -1 = unwired
    int order_in_string;    // Order within string for series connection
    bool has_bypass_diode;
    bool is_shaded;
    float power_output;     // Calculated during simulation
} SolarCell;

// A string of series-connected cells
typedef struct {
    int id;
    Color color;
    int cell_ids[MAX_CELLS_PER_STRING];
    int cell_count;
    float total_power;      // Calculated during simulation
    float total_energy_wh;
} CellString;

// Cell template within a module (relative position)
typedef struct {
    Vector3 offset;         // Position relative to module origin
    Vector3 normal;         // Normal direction (usually up)
} CellTemplate;

// A module is a reusable pattern of cells
typedef struct {
    char name[MAX_MODULE_NAME];
    CellTemplate cells[MAX_CELLS_PER_MODULE];
    int cell_count;
    int preset_index;       // Which cell preset this module uses
    float width;            // Bounding width for preview
    float height;           // Bounding height for preview
} CellModule;

// Simulation settings
typedef struct {
    float latitude;         // degrees
    float longitude;        // degrees
    int year;
    int month;
    int day;
    float hour;             // 0-24 decimal hours
    float irradiance;       // W/m^2
} SimSettings;

// Auto-layout settings
typedef struct {
    float target_area;          // Target coverage area in m^2
    float min_normal_angle;     // Minimum angle from horizontal (degrees) to consider
    float max_normal_angle;     // Maximum angle from horizontal (degrees)
    float surface_threshold;    // Maximum angle between adjacent triangles (degrees)
    int time_samples;           // Number of time samples for occlusion scoring
    bool optimize_occlusion;    // Whether to optimize for minimal occlusion
    bool preview_surface;       // Show surface selection preview
    bool use_height_constraint; // Enable height constraint (to exclude canopy)
    bool auto_detect_height;    // Automatically find optimal height range
    float height_tolerance;     // Vertical tolerance for auto-detect (default 0.1m)
    float min_height;           // Minimum height for cell placement
    float max_height;           // Maximum height for cell placement
    bool use_grid_layout;       // Use grid-based layout instead of mesh triangles
    float grid_spacing;         // Grid spacing for layout (0 = auto based on cell size)
} AutoLayoutSettings;

// Candidate position for auto-layout
typedef struct {
    Vector3 position;
    Vector3 normal;
    float occlusion_score;      // 0 = no occlusion, 1 = always occluded
    bool valid;
} LayoutCandidate;

// Snap settings for cell placement
typedef struct {
    bool grid_snap_enabled;     // Snap to grid
    float grid_size;            // Grid cell size in meters
    bool align_to_surface;      // Align cell orientation to surface
    bool show_grid;             // Show grid overlay
} SnapSettings;

// Simulation results
typedef struct {
    float total_power;
    float shaded_percentage;
    int shaded_count;
    Vector3 sun_direction;
    float sun_altitude;
    float sun_azimuth;
    bool is_daytime;
} SimResults;
typedef struct TimeSimResults {
    float total_energy_wh;        // Total energy over the day (Watt-hours)
    float average_power_w;        // Average power over daylight hours
    float peak_power_w;           // Maximum instantaneous power
    float average_shaded_pct;     // Average shading percentage
    float min_power_w;            // Minimum power (when not zero)
    float energy_by_hour[24];     // Energy breakdown by hour (optional)
} TimeSimResults;
// Camera controller state
typedef struct {
    Camera3D camera;
    Vector3 target;         // Orbit center
    float distance;         // Distance from target
    float azimuth;          // Horizontal angle (degrees)
    float elevation;        // Vertical angle (degrees)
    bool is_orthographic;
    float ortho_scale;      // Zoom for orthographic
} CameraController;

// Main application state
typedef struct {
    // Application mode
    AppMode mode;

    // Mesh
    Model vehicle_model;
    Mesh vehicle_mesh;      // Copy for raycasting
    BoundingBox mesh_bounds;
    BoundingBox mesh_bounds_raw;  // Original bounds before transform
    Vector3 mesh_center_raw;      // Original center for rotation pivot
    bool mesh_loaded;
    float mesh_scale;
    Vector3 mesh_rotation;        // Euler angles in degrees (X, Y, Z)
    char mesh_path[MAX_PATH_LENGTH];

    // Cells
    SolarCell cells[MAX_CELLS];
    int cell_count;
    int next_cell_id;
    int selected_preset;    // Index into CELL_PRESETS

    // Strings
    CellString strings[MAX_STRINGS];
    int string_count;
    int next_string_id;
    int active_string_id;   // Currently building string, -1 = none

    // Modules
    CellModule modules[MAX_MODULES];
    int module_count;
    int selected_module;    // Currently selected module for placement, -1 = none
    bool placing_module;    // True when in module placement mode

    // Auto-layout
    AutoLayoutSettings auto_layout;
    bool auto_layout_running;
    int auto_layout_progress;   // 0-100 progress percentage

    // Snap settings
    SnapSettings snap;

    // Camera
    CameraController cam;

    // Simulation
    SimSettings sim_settings;
    SimResults sim_results;
    bool sim_run;           // Has simulation been run?
    bool time_sim_run;
    TimeSimResults time_sim_results;

    // UI state
    bool show_file_dialog;
    int hovered_cell_id;    // -1 = none
    char status_msg[256];

    // Window
    int screen_width;
    int screen_height;
    int sidebar_width;

} AppState;

//------------------------------------------------------------------------------
// Cell Presets (defined in app.c)
//------------------------------------------------------------------------------
extern const CellPreset CELL_PRESETS[];
extern const int CELL_PRESET_COUNT;

//------------------------------------------------------------------------------
// Function Declarations
//------------------------------------------------------------------------------

// App lifecycle
void AppInit(AppState* app);
void AppUpdate(AppState* app);
void AppDraw(AppState* app);
void AppClose(AppState* app);

// Mesh loading
bool LoadVehicleMesh(AppState* app, const char* path);
void UpdateMeshTransform(AppState* app);

// Camera
void CameraInit(CameraController* cam);
void CameraUpdate(CameraController* cam, AppState* app);
void CameraSetOrthographic(CameraController* cam, bool ortho);
void CameraReset(CameraController* cam, BoundingBox bounds);
void CameraFitToBounds(CameraController* cam, BoundingBox bounds);

// Cells
int PlaceCell(AppState* app, Vector3 world_position, Vector3 world_normal);
void RemoveCell(AppState* app, int cell_id);
void ClearAllCells(AppState* app);
int FindCellAtPosition(AppState* app, Vector3 pos, float threshold);
int FindCellNearRay(AppState* app, Ray ray, float* out_distance);
void UpdateCellVisuals(AppState* app);
Vector3 CellGetWorldPosition(AppState* app, SolarCell* cell);
Vector3 CellGetWorldNormal(AppState* app, SolarCell* cell);

// Wiring
int StartNewString(AppState* app);
void AddCellToString(AppState* app, int cell_id);
void EndCurrentString(AppState* app);
void CancelCurrentString(AppState* app);
void ClearAllWiring(AppState* app);
Color GenerateStringColor(void);

// Modules
void InitModules(AppState* app);
int CreateModuleFromCells(AppState* app, const char* name);
bool SaveModule(CellModule* module, const char* filename);
bool LoadAppModule(CellModule* module, const char* filename);
void LoadAllModules(AppState* app);
int PlaceModule(AppState* app, int module_index, Vector3 world_position, Vector3 world_normal);
void DeleteModule(AppState* app, int module_index);

// Simulation
void RunStaticSimulation(AppState* app);
void RunTimeSimulationAnimated(AppState* app);
Vector3 CalculateSunDirection(SimSettings* settings, float* altitude, float* azimuth);
bool CheckCellShading(AppState* app, SolarCell* cell, Vector3 sun_dir);
float CalculateCellPower(AppState* app, SolarCell* cell, Vector3 sun_dir, CellPreset* preset, float irradiance);

// Auto-layout
void InitAutoLayout(AppState* app);
int RunAutoLayout(AppState* app);
float CalculateOcclusionScore(AppState* app, Vector3 position, Vector3 normal);
bool IsValidSurface(AppState* app, Vector3 position, Vector3 normal);
void DrawAutoLayoutPreview(AppState* app);

// Snap
void InitSnap(AppState* app);
Vector3 ApplyGridSnap(AppState* app, Vector3 position);
void DrawSnapGrid(AppState* app);

// GUI
void DrawGUI(AppState* app);
void DrawSidebar(AppState* app);
void DrawStatusBar(AppState* app);
void SetStatus(AppState* app, const char* fmt, ...);
int DrawImportPanel(AppState* app, int x, int y, int w);
int DrawCellPanel(AppState* app, int x, int y, int w);
int DrawWiringPanel(AppState* app, int x, int y, int w);
int DrawSimulationPanel(AppState* app, int x, int y, int w);
bool OpenFileDialog(char* outPath, int maxLen, const char* filter);

// Project save/load
bool SaveProject(AppState* app, const char* path);
bool LoadProject(AppState* app, const char* path);

// Utility
Color LerpColor(Color a, Color b, float t);
float Clampf(float value, float min, float max);

#endif // APP_H
