/*
 * Solar Array Designer
 * Core application implementation
 */

#include "app.h"
#include "raygui.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>
#include <time.h>
#include <sys/stat.h>

#ifdef _WIN32
    #include <windows.h>
    #include <direct.h>
#else
    #include <dirent.h>
    #include <unistd.h>
#endif

//------------------------------------------------------------------------------
// Cell Presets
//------------------------------------------------------------------------------
const CellPreset CELL_PRESETS[] = {
    {
        .name = "Maxeon Gen 3",
        .width = 0.125f,
        .height = 0.125f,
        .efficiency = 0.227f,
        .voc = 0.68f,
        .isc = 6.24f,
        .vmp = 0.58f,
        .imp = 6.01f
    },
    {
        .name = "Maxeon Gen 5",
        .width = 0.125f,
        .height = 0.125f,
        .efficiency = 0.24f,
        .voc = 0.70f,
        .isc = 6.50f,
        .vmp = 0.60f,
        .imp = 6.20f
    },
    {
        .name = "Generic Silicon",
        .width = 0.156f,
        .height = 0.156f,
        .efficiency = 0.20f,
        .voc = 0.64f,
        .isc = 9.5f,
        .vmp = 0.54f,
        .imp = 9.0f
    }
};
const int CELL_PRESET_COUNT = sizeof(CELL_PRESETS) / sizeof(CELL_PRESETS[0]);

//------------------------------------------------------------------------------
// Utility Functions
//------------------------------------------------------------------------------
Color LerpColor(Color a, Color b, float t)
{
    return (Color){
        (unsigned char)(a.r + (b.r - a.r) * t),
        (unsigned char)(a.g + (b.g - a.g) * t),
        (unsigned char)(a.b + (b.b - a.b) * t),
        (unsigned char)(a.a + (b.a - a.a) * t)
    };
}

float Clampf(float value, float min, float max)
{
    if (value < min) return min;
    if (value > max) return max;
    return value;
}

void SetStatus(AppState* app, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vsnprintf(app->status_msg, sizeof(app->status_msg), fmt, args);
    va_end(args);
}

Color GenerateStringColor(void)
{
    // Generate saturated random color
    float hue = (float)(rand() % 360);
    float sat = 0.7f + (float)(rand() % 30) / 100.0f;
    float val = 0.8f + (float)(rand() % 20) / 100.0f;

    // HSV to RGB conversion
    float c = val * sat;
    float x = c * (1.0f - fabsf(fmodf(hue / 60.0f, 2.0f) - 1.0f));
    float m = val - c;

    float r, g, b;
    if (hue < 60) { r = c; g = x; b = 0; }
    else if (hue < 120) { r = x; g = c; b = 0; }
    else if (hue < 180) { r = 0; g = c; b = x; }
    else if (hue < 240) { r = 0; g = x; b = c; }
    else if (hue < 300) { r = x; g = 0; b = c; }
    else { r = c; g = 0; b = x; }

    return (Color){
        (unsigned char)((r + m) * 255),
        (unsigned char)((g + m) * 255),
        (unsigned char)((b + m) * 255),
        230
    };
}

//------------------------------------------------------------------------------
// App Lifecycle
//------------------------------------------------------------------------------
void AppInit(AppState* app)
{
    srand((unsigned int)time(NULL));
    app->time_sim_run = false;
    memset(&app->time_sim_results, 0, sizeof(TimeSimResults));//TODO: fix this lol?
    // Initialize mode
    app->mode = MODE_IMPORT;
    app->sidebar_width = 280;

    // Mesh
    app->mesh_loaded = false;
    app->mesh_scale = 0.01f;
    app->mesh_rotation = (Vector3){0, 0, 0};

    // Cells
    app->cell_count = 0;
    app->next_cell_id = 0;
    app->selected_preset = 0;

    // Strings
    app->string_count = 0;
    app->next_string_id = 0;
    app->active_string_id = -1;

    // Modules
    InitModules(app);

    // Auto-layout
    InitAutoLayout(app);

    // Snap settings
    InitSnap(app);

    // Simulation defaults
    app->sim_settings.latitude = 37.4f;
    app->sim_settings.longitude = -87.2f;
    app->sim_settings.year = 2024;
    app->sim_settings.month = 6;
    app->sim_settings.day = 21;
    app->sim_settings.hour = 12.0f;
    app->sim_settings.irradiance = 1000.0f;
    app->sim_run = false;

    // UI
    app->hovered_cell_id = -1;
    SetStatus(app, "Welcome! Load a mesh file to begin.");

    // Camera
    CameraInit(&app->cam);

    // raygui style
    GuiSetStyle(DEFAULT, TEXT_SIZE, 16);
}

void AppClose(AppState* app)
{
    if (app->mesh_loaded)
    {
        UnloadModel(app->vehicle_model);
    }
}

//------------------------------------------------------------------------------
// Camera
//------------------------------------------------------------------------------
void CameraInit(CameraController* cam)
{
    cam->target = (Vector3){0, 0, 0};
    cam->distance = 5.0f;
    cam->azimuth = 45.0f;
    cam->elevation = 30.0f;
    cam->is_orthographic = false;
    cam->ortho_scale = 2.0f;

    cam->camera.position = (Vector3){3, 3, 3};
    cam->camera.target = cam->target;
    cam->camera.up = (Vector3){0, 1, 0};
    cam->camera.fovy = 45.0f;
    cam->camera.projection = CAMERA_PERSPECTIVE;
}

void CameraUpdatePosition(CameraController* cam)
{
    if (cam->is_orthographic)
    {
        // Top-down view
        cam->camera.position = (Vector3){
            cam->target.x,
            cam->target.y + cam->distance * 2,
            cam->target.z
        };
        cam->camera.up = (Vector3){0, 0, -1};
        cam->camera.projection = CAMERA_ORTHOGRAPHIC;
        cam->camera.fovy = cam->ortho_scale * 2;
    }
    else
    {
        // Perspective orbit
        float az = cam->azimuth * DEG2RAD;
        float el = cam->elevation * DEG2RAD;

        cam->camera.position = (Vector3){
            cam->target.x + cam->distance * cosf(el) * sinf(az),
            cam->target.y + cam->distance * sinf(el),
            cam->target.z + cam->distance * cosf(el) * cosf(az)
        };
        cam->camera.up = (Vector3){0, 1, 0};
        cam->camera.projection = CAMERA_PERSPECTIVE;
        cam->camera.fovy = 45.0f;
    }
    cam->camera.target = cam->target;
}

void CameraUpdate(CameraController* cam, AppState* app)
{
    // Only handle camera input when not over GUI
    Vector2 mouse = GetMousePosition();
    if (mouse.x < app->sidebar_width) return;

    // Orbit with left mouse drag (perspective mode)
    // Or pan with left mouse drag (orthographic mode)
    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT))
    {
        Vector2 delta = GetMouseDelta();

        if (cam->is_orthographic)
        {
            // Pan in orthographic mode
            float panSpeed = cam->ortho_scale * 0.003f;
            cam->target.x -= delta.x * panSpeed;
            cam->target.z -= delta.y * panSpeed;
        }
        else
        {
            // Orbit in perspective mode
            cam->azimuth -= delta.x * 0.5f;
            cam->elevation = Clampf(cam->elevation + delta.y * 0.5f, -89.0f, 89.0f);
        }
    }

    // Pan with middle mouse drag (both modes)
    if (IsMouseButtonDown(MOUSE_BUTTON_MIDDLE))
    {
        Vector2 delta = GetMouseDelta();
        float panSpeed = cam->distance * 0.002f;

        // Pan in camera-relative directions
        Vector3 right = Vector3Normalize(Vector3CrossProduct(
            Vector3Subtract(cam->camera.target, cam->camera.position),
            cam->camera.up
        ));
        Vector3 up = cam->camera.up;

        cam->target = Vector3Add(cam->target, Vector3Scale(right, -delta.x * panSpeed));
        cam->target = Vector3Add(cam->target, Vector3Scale(up, delta.y * panSpeed));
    }

    // Rotate with right mouse drag (perspective mode)
    if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT) && !cam->is_orthographic)
    {
        Vector2 delta = GetMouseDelta();
        cam->azimuth += delta.x * 0.5f;
        cam->elevation = Clampf(cam->elevation - delta.y * 0.5f, -89.0f, 89.0f);
    }

    // Zoom with scroll
    float wheel = GetMouseWheelMove();
    if (wheel != 0)
    {
        if (cam->is_orthographic)
        {
            cam->ortho_scale *= (1.0f - wheel * 0.1f);
            cam->ortho_scale = Clampf(cam->ortho_scale, 0.1f, 50.0f);
        }
        else
        {
            cam->distance *= (1.0f - wheel * 0.1f);
            cam->distance = Clampf(cam->distance, 0.1f, 100.0f);
        }
    }

    // Arrow keys for rotation
    float rotSpeed = 2.0f;
    if (IsKeyDown(KEY_LEFT)) cam->azimuth -= rotSpeed;
    if (IsKeyDown(KEY_RIGHT)) cam->azimuth += rotSpeed;
    if (IsKeyDown(KEY_UP)) cam->elevation = Clampf(cam->elevation + rotSpeed, -89.0f, 89.0f);
    if (IsKeyDown(KEY_DOWN)) cam->elevation = Clampf(cam->elevation - rotSpeed, -89.0f, 89.0f);

    CameraUpdatePosition(cam);
}

void CameraSetOrthographic(CameraController* cam, bool ortho)
{
    cam->is_orthographic = ortho;
    CameraUpdatePosition(cam);
}

void CameraReset(CameraController* cam, BoundingBox bounds)
{
    cam->azimuth = 45.0f;
    cam->elevation = 30.0f;
    CameraFitToBounds(cam, bounds);
}

void CameraFitToBounds(CameraController* cam, BoundingBox bounds)
{
    Vector3 size = Vector3Subtract(bounds.max, bounds.min);
    float maxDim = fmaxf(fmaxf(size.x, size.y), size.z);

    cam->target = (Vector3){
        (bounds.min.x + bounds.max.x) / 2,
        (bounds.min.y + bounds.max.y) / 2,
        (bounds.min.z + bounds.max.z) / 2
    };
    cam->distance = maxDim * 1.5f;
    cam->ortho_scale = maxDim * 0.6f;

    CameraUpdatePosition(cam);
}

//------------------------------------------------------------------------------
// Mesh Loading
//------------------------------------------------------------------------------
bool LoadVehicleMesh(AppState* app, const char* path)
{
    // Unload existing mesh
    if (app->mesh_loaded)
    {
        UnloadModel(app->vehicle_model);
        app->mesh_loaded = false;
    }

    // Load model
    app->vehicle_model = LoadModel(path);
    if (app->vehicle_model.meshCount == 0)
    {
        SetStatus(app, "Error: Failed to load mesh");
        return false;
    }

    // Get raw bounds (identity transform)
    app->vehicle_model.transform = MatrixIdentity();
    app->mesh_bounds_raw = GetModelBoundingBox(app->vehicle_model);

    // Calculate raw center (pivot point for rotation)
    app->mesh_center_raw = (Vector3){
        (app->mesh_bounds_raw.min.x + app->mesh_bounds_raw.max.x) / 2.0f,
        (app->mesh_bounds_raw.min.y + app->mesh_bounds_raw.max.y) / 2.0f,
        (app->mesh_bounds_raw.min.z + app->mesh_bounds_raw.max.z) / 2.0f
    };

    // Keep a copy of mesh for raycasting
    app->vehicle_mesh = app->vehicle_model.meshes[0];

    // Store path
    strncpy(app->mesh_path, path, MAX_PATH_LENGTH - 1);
    app->mesh_loaded = true;

    // Apply transform (scale, rotation, centering)
    UpdateMeshTransform(app);

    // Reset camera to fit mesh
    CameraFitToBounds(&app->cam, app->mesh_bounds);

    // Clear existing cells
    ClearAllCells(app);

    SetStatus(app, "Loaded mesh: %s", GetFileName(path));
    return true;
}

void UpdateMeshTransform(AppState* app)
{
    if (!app->mesh_loaded) return;

    // Build transform: translate to origin -> scale -> rotate -> translate to final position
    // This ensures rotation happens about the mesh center

    Vector3 center = app->mesh_center_raw;
    float scale = app->mesh_scale;
    Vector3 rot = app->mesh_rotation;

    // Step 1: Translate mesh center to origin
    Matrix toOrigin = MatrixTranslate(-center.x, -center.y, -center.z);

    // Step 2: Apply scale
    Matrix scaleM = MatrixScale(scale, scale, scale);

    // Step 3: Apply rotation (Euler angles: X, Y, Z order)
    Matrix rotX = MatrixRotateX(rot.x * DEG2RAD);
    Matrix rotY = MatrixRotateY(rot.y * DEG2RAD);
    Matrix rotZ = MatrixRotateZ(rot.z * DEG2RAD);
    Matrix rotation = MatrixMultiply(MatrixMultiply(rotX, rotY), rotZ);

    // Combine: toOrigin -> scale -> rotate
    Matrix transform = MatrixMultiply(toOrigin, scaleM);
    transform = MatrixMultiply(transform, rotation);

    // Calculate where the bounds end up after this transform
    // Transform the 8 corners of the raw bounding box
    Vector3 corners[8] = {
        {app->mesh_bounds_raw.min.x, app->mesh_bounds_raw.min.y, app->mesh_bounds_raw.min.z},
        {app->mesh_bounds_raw.max.x, app->mesh_bounds_raw.min.y, app->mesh_bounds_raw.min.z},
        {app->mesh_bounds_raw.min.x, app->mesh_bounds_raw.max.y, app->mesh_bounds_raw.min.z},
        {app->mesh_bounds_raw.max.x, app->mesh_bounds_raw.max.y, app->mesh_bounds_raw.min.z},
        {app->mesh_bounds_raw.min.x, app->mesh_bounds_raw.min.y, app->mesh_bounds_raw.max.z},
        {app->mesh_bounds_raw.max.x, app->mesh_bounds_raw.min.y, app->mesh_bounds_raw.max.z},
        {app->mesh_bounds_raw.min.x, app->mesh_bounds_raw.max.y, app->mesh_bounds_raw.max.z},
        {app->mesh_bounds_raw.max.x, app->mesh_bounds_raw.max.y, app->mesh_bounds_raw.max.z},
    };

    Vector3 newMin = {1e9f, 1e9f, 1e9f};
    Vector3 newMax = {-1e9f, -1e9f, -1e9f};

    for (int i = 0; i < 8; i++)
    {
        Vector3 p = Vector3Transform(corners[i], transform);
        newMin.x = fminf(newMin.x, p.x);
        newMin.y = fminf(newMin.y, p.y);
        newMin.z = fminf(newMin.z, p.z);
        newMax.x = fmaxf(newMax.x, p.x);
        newMax.y = fmaxf(newMax.y, p.y);
        newMax.z = fmaxf(newMax.z, p.z);
    }

    // Step 4: Translate so mesh is centered at X=0, Z=0 and bottom at Y=0
    float finalX = -(newMin.x + newMax.x) / 2.0f;
    float finalY = -newMin.y;
    float finalZ = -(newMin.z + newMax.z) / 2.0f;
    Matrix toFinal = MatrixTranslate(finalX, finalY, finalZ);

    // Final transform
    app->vehicle_model.transform = MatrixMultiply(transform, toFinal);

    // Update bounds to final position
    app->mesh_bounds.min = (Vector3){
        newMin.x + finalX,
        newMin.y + finalY,
        newMin.z + finalZ
    };
    app->mesh_bounds.max = (Vector3){
        newMax.x + finalX,
        newMax.y + finalY,
        newMax.z + finalZ
    };
}

//------------------------------------------------------------------------------
// Cell Placement
//------------------------------------------------------------------------------

// Get world position of a cell (transforms local coords by mesh transform)
Vector3 CellGetWorldPosition(AppState* app, SolarCell* cell)
{
    return Vector3Transform(cell->local_position, app->vehicle_model.transform);
}

// Get world normal of a cell (transforms and normalizes)
Vector3 CellGetWorldNormal(AppState* app, SolarCell* cell)
{
    // For normals, we need to use the inverse transpose of the transform
    // But for uniform scale and rotation only, we can just transform and normalize
    Matrix transform = app->vehicle_model.transform;
    // Zero out translation for normal transformation
    Matrix normalTransform = transform;
    normalTransform.m12 = 0; normalTransform.m13 = 0; normalTransform.m14 = 0;

    Vector3 worldNormal = Vector3Transform(cell->local_normal, normalTransform);
    return Vector3Normalize(worldNormal);
}
// Get world tangent of a cell (transforms and normalizes)
Vector3 CellGetWorldTangent(AppState* app, SolarCell* cell)
{
    Matrix transform = app->vehicle_model.transform;
    // Zero out translation for direction transformation
    Matrix dirTransform = transform;
    dirTransform.m12 = 0;
    dirTransform.m13 = 0;
    dirTransform.m14 = 0;

    Vector3 worldTangent = Vector3Transform(cell->local_tangent, dirTransform);
    return Vector3Normalize(worldTangent);
}
int PlaceCell(AppState* app, Vector3 world_position, Vector3 world_normal)
{
    if (app->cell_count >= MAX_CELLS)
    {
        SetStatus(app, "Maximum cell count reached");
        return -1;
    }

    // Check minimum surface angle (in world space)
    if (world_normal.y < MIN_UPWARD_NORMAL)
    {
        SetStatus(app, "Surface too steep for cell placement");
        return -1;
    }

    // Check for overlapping cells (in world space)
    CellPreset* preset = (CellPreset*)&CELL_PRESETS[app->selected_preset];
    float minDist = fmaxf(preset->width, preset->height) * MIN_CELL_DISTANCE_FACTOR;

    for (int i = 0; i < app->cell_count; i++)
    {
        Vector3 existingWorldPos = CellGetWorldPosition(app, &app->cells[i]);
        float dist = Vector3Distance(world_position, existingWorldPos);
        if (dist < minDist)
        {
            SetStatus(app, "Too close to existing cell");
            return -1;
        }
    }

    // Compute tangent (right vector) in world space
    // Use a consistent reference direction, but handle the degenerate case
    Vector3 world_tangent;
    Vector3 ref = {0, 0, 1};
    world_tangent = Vector3CrossProduct(ref, world_normal);
    if (Vector3Length(world_tangent) < 0.001f)
    {
        ref = (Vector3){1, 0, 0};
        world_tangent = Vector3CrossProduct(ref, world_normal);
    }
    world_tangent = Vector3Normalize(world_tangent);

    // Convert world coords to local (inverse of mesh transform)
    Matrix invTransform = MatrixInvert(app->vehicle_model.transform);
    Vector3 local_position = Vector3Transform(world_position, invTransform);

    // Transform normal and tangent to local space
    Matrix normalInvTransform = invTransform;
    normalInvTransform.m12 = 0; normalInvTransform.m13 = 0; normalInvTransform.m14 = 0;
    Vector3 local_normal = Vector3Normalize(Vector3Transform(world_normal, normalInvTransform));
    Vector3 local_tangent = Vector3Normalize(Vector3Transform(world_tangent, normalInvTransform));

    // Create cell with local coordinates
    SolarCell* cell = &app->cells[app->cell_count];
    cell->id = app->next_cell_id++;
    cell->local_position = local_position;
    cell->local_normal = local_normal;
    cell->local_tangent = local_tangent;
    cell->string_id = -1;
    cell->order_in_string = -1;
    cell->has_bypass_diode = false;
    cell->is_shaded = false;
    cell->power_output = 0;

    app->cell_count++;

    SetStatus(app, "Placed cell #%d", cell->id);
    return cell->id;
}
void RemoveCell(AppState* app, int cell_id)
{
    int idx = -1;
    for (int i = 0; i < app->cell_count; i++)
    {
        if (app->cells[i].id == cell_id)
        {
            idx = i;
            break;
        }
    }

    if (idx < 0) return;

    SolarCell* cell = &app->cells[idx];

    // Remove from string if wired
    if (cell->string_id >= 0)
    {
        for (int s = 0; s < app->string_count; s++)
        {
            if (app->strings[s].id == cell->string_id)
            {
                CellString* str = &app->strings[s];
                // Remove from cell list
                for (int c = 0; c < str->cell_count; c++)
                {
                    if (str->cell_ids[c] == cell_id)
                    {
                        // Shift remaining
                        for (int j = c; j < str->cell_count - 1; j++)
                        {
                            str->cell_ids[j] = str->cell_ids[j + 1];
                        }
                        str->cell_count--;
                        break;
                    }
                }
                break;
            }
        }
    }

    // Remove cell by shifting array
    for (int i = idx; i < app->cell_count - 1; i++)
    {
        app->cells[i] = app->cells[i + 1];
    }
    app->cell_count--;

    SetStatus(app, "Removed cell");
}

void ClearAllCells(AppState* app)
{
    app->cell_count = 0;
    app->string_count = 0;
    app->active_string_id = -1;
    app->sim_run = false;
    SetStatus(app, "Cleared all cells");
}

int FindCellAtPosition(AppState* app, Vector3 pos, float threshold)
{
    for (int i = 0; i < app->cell_count; i++)
    {
        Vector3 worldPos = CellGetWorldPosition(app, &app->cells[i]);
        if (Vector3Distance(pos, worldPos) < threshold)
        {
            return app->cells[i].id;
        }
    }
    return -1;
}

int FindCellNearRay(AppState* app, Ray ray, float* out_distance)
{
    int closest_id = -1;
    float closest_dist = 1000000.0f;

    CellPreset* preset = (CellPreset*)&CELL_PRESETS[app->selected_preset];
    float threshold = fmaxf(preset->width, preset->height) * 0.7f;

    for (int i = 0; i < app->cell_count; i++)
    {
        // Simple distance from ray to point (use world position)
        Vector3 cellPos = CellGetWorldPosition(app, &app->cells[i]);
        Vector3 toCell = Vector3Subtract(cellPos, ray.position);
        float t = Vector3DotProduct(toCell, ray.direction);

        if (t > 0)
        {
            Vector3 closest = Vector3Add(ray.position, Vector3Scale(ray.direction, t));
            float dist = Vector3Distance(closest, cellPos);

            if (dist < threshold && t < closest_dist)
            {
                closest_dist = t;
                closest_id = app->cells[i].id;
            }
        }
    }

    if (out_distance) *out_distance = closest_dist;
    return closest_id;
}

//------------------------------------------------------------------------------
// Wiring
//------------------------------------------------------------------------------
int StartNewString(AppState* app)
{
    if (app->string_count >= MAX_STRINGS)
    {
        SetStatus(app, "Maximum string count reached");
        return -1;
    }

    CellString* str = &app->strings[app->string_count];
    str->id = app->next_string_id++;
    str->color = GenerateStringColor();
    str->cell_count = 0;
    str->total_power = 0;

    app->active_string_id = str->id;
    app->string_count++;

    SetStatus(app, "Started string #%d", str->id);
    return str->id;
}

void AddCellToString(AppState* app, int cell_id)
{
    // Find cell
    SolarCell* cell = NULL;
    for (int i = 0; i < app->cell_count; i++)
    {
        if (app->cells[i].id == cell_id)
        {
            cell = &app->cells[i];
            break;
        }
    }
    if (!cell) return;

    // Check if already wired
    if (cell->string_id >= 0)
    {
        SetStatus(app, "Cell already wired to string #%d", cell->string_id);
        return;
    }

    // Start new string if needed
    if (app->active_string_id < 0)
    {
        if (StartNewString(app) < 0) return;
    }

    // Find active string
    CellString* str = NULL;
    for (int i = 0; i < app->string_count; i++)
    {
        if (app->strings[i].id == app->active_string_id)
        {
            str = &app->strings[i];
            break;
        }
    }
    if (!str) return;

    if (str->cell_count >= MAX_CELLS_PER_STRING)
    {
        SetStatus(app, "String is full");
        return;
    }

    // Add cell to string
    cell->string_id = str->id;
    cell->order_in_string = str->cell_count;
    str->cell_ids[str->cell_count++] = cell_id;

    SetStatus(app, "Added cell #%d to string #%d (%d cells)",
        cell_id, str->id, str->cell_count);
}

void EndCurrentString(AppState* app)
{
    if (app->active_string_id < 0)
    {
        SetStatus(app, "No active string");
        return;
    }

    // Find string and check if empty
    for (int i = 0; i < app->string_count; i++)
    {
        if (app->strings[i].id == app->active_string_id)
        {
            if (app->strings[i].cell_count == 0)
            {
                // Remove empty string
                for (int j = i; j < app->string_count - 1; j++)
                {
                    app->strings[j] = app->strings[j + 1];
                }
                app->string_count--;
            }
            break;
        }
    }

    SetStatus(app, "Ended string #%d", app->active_string_id);
    app->active_string_id = -1;
}

void CancelCurrentString(AppState* app)
{
    if (app->active_string_id < 0) return;

    // Find string
    int strIdx = -1;
    for (int i = 0; i < app->string_count; i++)
    {
        if (app->strings[i].id == app->active_string_id)
        {
            strIdx = i;
            break;
        }
    }
    if (strIdx < 0) return;

    CellString* str = &app->strings[strIdx];

    // Unwire all cells in string
    for (int i = 0; i < str->cell_count; i++)
    {
        for (int c = 0; c < app->cell_count; c++)
        {
            if (app->cells[c].id == str->cell_ids[i])
            {
                app->cells[c].string_id = -1;
                app->cells[c].order_in_string = -1;
                break;
            }
        }
    }

    // Remove string
    for (int i = strIdx; i < app->string_count - 1; i++)
    {
        app->strings[i] = app->strings[i + 1];
    }
    app->string_count--;

    SetStatus(app, "Cancelled string");
    app->active_string_id = -1;
}

void ClearAllWiring(AppState* app)
{
    // Unwire all cells
    for (int i = 0; i < app->cell_count; i++)
    {
        app->cells[i].string_id = -1;
        app->cells[i].order_in_string = -1;
    }

    app->string_count = 0;
    app->active_string_id = -1;
    app->sim_run = false;

    SetStatus(app, "Cleared all wiring");
}

//------------------------------------------------------------------------------
// Modules
//------------------------------------------------------------------------------
void InitModules(AppState* app)
{
    app->module_count = 0;
    app->selected_module = -1;
    app->placing_module = false;

    // Create modules directory if it doesn't exist
    #ifdef _WIN32
        _mkdir(MODULES_DIRECTORY);
    #else
        mkdir(MODULES_DIRECTORY, 0755);
    #endif

    // Load existing modules
    LoadAllModules(app);
}

int CreateModuleFromCells(AppState* app, const char* name)
{
    if (app->cell_count == 0)
    {
        SetStatus(app, "No cells to create module from");
        return -1;
    }

    if (app->module_count >= MAX_MODULES)
    {
        SetStatus(app, "Maximum module count reached");
        return -1;
    }

    CellModule* mod = &app->modules[app->module_count];
    strncpy(mod->name, name, MAX_MODULE_NAME - 1);
    mod->name[MAX_MODULE_NAME - 1] = '\0';
    mod->preset_index = app->selected_preset;
    mod->cell_count = 0;

    // Calculate center of all cells (in world coordinates)
    Vector3 center = {0, 0, 0};
    for (int i = 0; i < app->cell_count && i < MAX_CELLS_PER_MODULE; i++)
    {
        Vector3 worldPos = CellGetWorldPosition(app, &app->cells[i]);
        center = Vector3Add(center, worldPos);
    }
    int count = (app->cell_count < MAX_CELLS_PER_MODULE) ? app->cell_count : MAX_CELLS_PER_MODULE;
    center = Vector3Scale(center, 1.0f / count);

    // Store cells relative to center
    float minX = 1e9f, maxX = -1e9f;
    float minZ = 1e9f, maxZ = -1e9f;

    for (int i = 0; i < count; i++)
    {
        Vector3 worldPos = CellGetWorldPosition(app, &app->cells[i]);
        Vector3 worldNormal = CellGetWorldNormal(app, &app->cells[i]);

        mod->cells[i].offset = Vector3Subtract(worldPos, center);
        mod->cells[i].normal = worldNormal;

        minX = fminf(minX, mod->cells[i].offset.x);
        maxX = fmaxf(maxX, mod->cells[i].offset.x);
        minZ = fminf(minZ, mod->cells[i].offset.z);
        maxZ = fmaxf(maxZ, mod->cells[i].offset.z);

        mod->cell_count++;
    }

    mod->width = maxX - minX;
    mod->height = maxZ - minZ;

    // Save module to disk
    char filename[MAX_PATH_LENGTH];
    snprintf(filename, sizeof(filename), "%s/%s.json", MODULES_DIRECTORY, name);
    SaveModule(mod, filename);

    app->module_count++;
    SetStatus(app, "Created module '%s' with %d cells", name, mod->cell_count);
    return app->module_count - 1;
}

bool SaveModule(CellModule* module, const char* filename)
{
    FILE* f = fopen(filename, "w");
    if (!f) return false;

    fprintf(f, "{\n");
    fprintf(f, "  \"name\": \"%s\",\n", module->name);
    fprintf(f, "  \"preset_index\": %d,\n", module->preset_index);
    fprintf(f, "  \"width\": %.6f,\n", module->width);
    fprintf(f, "  \"height\": %.6f,\n", module->height);
    fprintf(f, "  \"cell_count\": %d,\n", module->cell_count);
    fprintf(f, "  \"cells\": [\n");

    for (int i = 0; i < module->cell_count; i++)
    {
        CellTemplate* ct = &module->cells[i];
        fprintf(f, "    {\n");
        fprintf(f, "      \"offset\": [%.6f, %.6f, %.6f],\n", ct->offset.x, ct->offset.y, ct->offset.z);
        fprintf(f, "      \"normal\": [%.6f, %.6f, %.6f]\n", ct->normal.x, ct->normal.y, ct->normal.z);
        fprintf(f, "    }%s\n", (i < module->cell_count - 1) ? "," : "");
    }

    fprintf(f, "  ]\n");
    fprintf(f, "}\n");

    fclose(f);
    return true;
}

bool LoadModule(CellModule* module, const char* filename)
{
    FILE* f = fopen(filename, "r");
    if (!f) return false;

    // Simple JSON parsing (not robust, but works for our format)
    char line[256];
    module->cell_count = 0;

    while (fgets(line, sizeof(line), f))
    {
        if (strstr(line, "\"name\":"))
        {
            char* start = strchr(line, ':');
            if (start)
            {
                start = strchr(start, '"') + 1;
                char* end = strrchr(line, '"');
                if (start && end && end > start)
                {
                    int len = end - start;
                    if (len >= MAX_MODULE_NAME) len = MAX_MODULE_NAME - 1;
                    strncpy(module->name, start, len);
                    module->name[len] = '\0';
                }
            }
        }
        else if (strstr(line, "\"preset_index\":"))
        {
            sscanf(line, " \"preset_index\": %d", &module->preset_index);
        }
        else if (strstr(line, "\"width\":"))
        {
            sscanf(line, " \"width\": %f", &module->width);
        }
        else if (strstr(line, "\"height\":"))
        {
            sscanf(line, " \"height\": %f", &module->height);
        }
        else if (strstr(line, "\"cell_count\":"))
        {
            sscanf(line, " \"cell_count\": %d", &module->cell_count);
        }
        else if (strstr(line, "\"offset\":"))
        {
            int idx = module->cell_count;
            if (idx < MAX_CELLS_PER_MODULE)
            {
                float x, y, z;
                char* bracket = strchr(line, '[');
                if (bracket && sscanf(bracket, "[%f, %f, %f]", &x, &y, &z) == 3)
                {
                    module->cells[idx].offset = (Vector3){x, y, z};
                }
            }
        }
        else if (strstr(line, "\"normal\":"))
        {
            // Find which cell this belongs to by counting previous offsets
            static int cellIdx = -1;
            cellIdx++;
            if (cellIdx < MAX_CELLS_PER_MODULE)
            {
                float x, y, z;
                char* bracket = strchr(line, '[');
                if (bracket && sscanf(bracket, "[%f, %f, %f]", &x, &y, &z) == 3)
                {
                    module->cells[cellIdx].normal = (Vector3){x, y, z};
                }
            }
        }
    }

    fclose(f);
    return module->cell_count > 0;
}

void LoadAllModules(AppState* app)
{
    app->module_count = 0;

    #ifdef _WIN32
        WIN32_FIND_DATA fd;
        char pattern[MAX_PATH_LENGTH];
        snprintf(pattern, sizeof(pattern), "%s\\*.json", MODULES_DIRECTORY);
        HANDLE hFind = FindFirstFile(pattern, &fd);
        if (hFind != INVALID_HANDLE_VALUE)
        {
            do {
                if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
                {
                    char filepath[MAX_PATH_LENGTH];
                    snprintf(filepath, sizeof(filepath), "%s\\%s", MODULES_DIRECTORY, fd.cFileName);
                    if (app->module_count < MAX_MODULES)
                    {
                        if (LoadModule(&app->modules[app->module_count], filepath))
                        {
                            app->module_count++;
                        }
                    }
                }
            } while (FindNextFile(hFind, &fd));
            FindClose(hFind);
        }
    #else
        DIR* dir = opendir(MODULES_DIRECTORY);
        if (dir)
        {
            struct dirent* entry;
            while ((entry = readdir(dir)) != NULL && app->module_count < MAX_MODULES)
            {
                if (entry->d_type == DT_REG || entry->d_type == DT_UNKNOWN)
                {
                    const char* ext = strrchr(entry->d_name, '.');
                    if (ext && strcmp(ext, ".json") == 0)
                    {
                        char filepath[MAX_PATH_LENGTH];
                        snprintf(filepath, sizeof(filepath), "%s/%s", MODULES_DIRECTORY, entry->d_name);
                        if (LoadModule(&app->modules[app->module_count], filepath))
                        {
                            app->module_count++;
                        }
                    }
                }
            }
            closedir(dir);
        }
    #endif

    if (app->module_count > 0)
    {
        TraceLog(LOG_INFO, "Loaded %d modules", app->module_count);
    }
}

int PlaceModule(AppState* app, int module_index, Vector3 world_position, Vector3 world_normal)
{
    if (module_index < 0 || module_index >= app->module_count) return 0;

    CellModule* mod = &app->modules[module_index];    int placed = 0;

    // Calculate rotation to align module with surface normal
    // For simplicity, we just offset cells - proper rotation would need more math
    for (int i = 0; i < mod->cell_count; i++)
    {
        Vector3 cellPos = Vector3Add(world_position, mod->cells[i].offset);

        // Try to place cell (may fail if overlapping)
        int id = PlaceCell(app, cellPos, mod->cells[i].normal);
        if (id >= 0) placed++;
    }

    SetStatus(app, "Placed module '%s' (%d/%d cells)", mod->name, placed, mod->cell_count);
    return placed;
}

void DeleteModule(AppState* app, int module_index)
{
    if (module_index < 0 || module_index >= app->module_count) return;

    // Delete file
    char filename[MAX_PATH_LENGTH];
    snprintf(filename, sizeof(filename), "%s/%s.json", MODULES_DIRECTORY, app->modules[module_index].name);
    remove(filename);

    // Shift modules
    for (int i = module_index; i < app->module_count - 1; i++)
    {
        app->modules[i] = app->modules[i + 1];
    }
    app->module_count--;

    if (app->selected_module >= app->module_count)
    {
        app->selected_module = app->module_count - 1;
    }

    SetStatus(app, "Deleted module");
}

//------------------------------------------------------------------------------
// Auto-Layout
//------------------------------------------------------------------------------
void InitAutoLayout(AppState* app)
{
    app->auto_layout.target_area = 6.0f;           // 1 m^2 default
    app->auto_layout.min_normal_angle = 62.0f;      // Flat surfaces OK
    app->auto_layout.max_normal_angle = 90.0f;     // Up to 45 degrees from horizontal
    app->auto_layout.surface_threshold = 30.0f;    // Adjacent triangles within 30 degrees
    app->auto_layout.time_samples = 12;            // Sample every 2 hours (6am-6pm)
    app->auto_layout.optimize_occlusion = true;
    app->auto_layout.preview_surface = false;
    app->auto_layout.use_height_constraint = true;  // Height constraint enabled by default
    app->auto_layout.auto_detect_height = true;    // Auto-detect optimal height range
    app->auto_layout.height_tolerance = 0.3f;      // 10cm vertical tolerance
    app->auto_layout.min_height = 0.0f;
    app->auto_layout.max_height = 10.0f;
    app->auto_layout.use_grid_layout = true;       // Grid layout enabled by default
    app->auto_layout.grid_spacing = 0.0f;          // 0 = auto based on cell size
    app->auto_layout_running = false;
    app->auto_layout_progress = 0;
}
// Check if a surface point is valid for cell placement
// Check if a point is on the mesh surface (cast ray down and verify hit is close to expected position)
bool IsPointOnMesh(AppState* app, Vector3 position, float tolerance)
{
    if (!app->mesh_loaded) return false;

    // Cast ray downward from above the position
    Ray ray;
    ray.position = (Vector3){position.x, app->mesh_bounds.max.y + 1.0f, position.z};
    ray.direction = (Vector3){0, -1, 0};

    RayCollision hit = GetRayCollisionMesh(ray, app->vehicle_mesh, app->vehicle_model.transform);

    if (!hit.hit) return false;

    // Check if the hit point is close to our expected position
    float heightDiff = fabsf(hit.point.y - position.y);
    return heightDiff < tolerance;
}

// Check if the cell footprint is fully supported by the mesh (no overhanging edges)
// Also checks that the cell doesn't clip through any mesh geometry above it
bool IsCellFootprintValid(AppState* app, Vector3 position, Vector3 normal, float cellWidth, float cellHeight)
{
    if (!app->mesh_loaded) return false;

    // Calculate cell corner directions (similar to how we draw cells)
    Vector3 right;
    Vector3 ref = {0, 0, 1};
    right = Vector3CrossProduct(ref, normal);
    if (Vector3Length(right) < 0.001f)
    {
        ref = (Vector3){1, 0, 0};
        right = Vector3CrossProduct(ref, normal);
    }
    right = Vector3Normalize(right);

    Vector3 forward = Vector3Normalize(Vector3CrossProduct(normal, right));

    // Scale to half cell size
    Vector3 halfRight = Vector3Scale(right, cellWidth / 2.0f);
    Vector3 halfForward = Vector3Scale(forward, cellHeight / 2.0f);

    // Check corners and edge midpoints (8 points total for better coverage)
    Vector3 checkPoints[9];
    checkPoints[0] = position; // Center
    checkPoints[1] = Vector3Add(position, Vector3Add(halfRight, halfForward));           // +X +Z
    checkPoints[2] = Vector3Add(position, Vector3Add(Vector3Negate(halfRight), halfForward));     // -X +Z
    checkPoints[3] = Vector3Add(position, Vector3Add(halfRight, Vector3Negate(halfForward)));     // +X -Z
    checkPoints[4] = Vector3Add(position, Vector3Add(Vector3Negate(halfRight), Vector3Negate(halfForward))); // -X -Z
    checkPoints[5] = Vector3Add(position, halfRight);                                     // +X edge
    checkPoints[6] = Vector3Add(position, Vector3Negate(halfRight));                      // -X edge
    checkPoints[7] = Vector3Add(position, halfForward);                                   // +Z edge
    checkPoints[8] = Vector3Add(position, Vector3Negate(halfForward));                    // -Z edge

    float tolerance = 0.05f; // 5cm tolerance for surface matching

    for (int i = 0; i < 9; i++)
    {
        Vector3 checkPos = checkPoints[i];

        // 1. Check that this point is on the mesh (ray cast down finds surface nearby)
        Ray rayDown;
        rayDown.position = (Vector3){checkPos.x, app->mesh_bounds.max.y + 1.0f, checkPos.z};
        rayDown.direction = (Vector3){0, -1, 0};

        RayCollision hitDown = GetRayCollisionMesh(rayDown, app->vehicle_mesh, app->vehicle_model.transform);

        if (!hitDown.hit)
        {
            // No mesh surface below this point - cell would hang off edge
            return false;
        }

        // Check if the surface is at approximately the same height as our cell position
        float expectedY = position.y; // Cell center height
        float surfaceY = hitDown.point.y;

        // Allow some tolerance for curved surfaces, but not too much
        if (fabsf(surfaceY - expectedY) > tolerance * 2.0f)
        {
            // Surface height differs too much - might be a different surface or edge
            return false;
        }

        // Also check that the surface normal is similar (we're on the same face)
        float normalDot = Vector3DotProduct(normal, hitDown.normal);
        if (normalDot < 0.7f)
        {
            // Normal differs too much - we're on a different surface
            return false;
        }

        // 2. Check for mesh geometry ABOVE this point that would clip the cell
        // Cast ray upward from just above the surface to check for obstructions
        Ray rayUp;
        rayUp.position = Vector3Add(checkPos, Vector3Scale(normal, 0.01f)); // Start just above surface
        rayUp.direction = normal; // Cast along surface normal (upward for upward-facing surfaces)

        RayCollision hitUp = GetRayCollisionMesh(rayUp, app->vehicle_mesh, app->vehicle_model.transform);

        // If we hit something close above us, the cell would clip into it
        // Use a distance based on how "thick" a cell would be (say 5mm) plus some margin
        float cellThickness = 0.02f; // 2cm clearance above cell
        if (hitUp.hit && hitUp.distance < cellThickness)
        {
            // Mesh geometry too close above - cell would clip
            return false;
        }
    }

    return true;
}

// Check if a surface point is valid for cell placement
bool IsValidSurface(AppState* app, Vector3 position, Vector3 normal)
{
    // Normal must point upward within the angle constraints
    // normal.y = cos(angle from vertical), so angle from horizontal = 90 - acos(normal.y)
    float angle_from_vertical = acosf(Clampf(normal.y, -1.0f, 1.0f)) * RAD2DEG;
    float angle_from_horizontal = 90.0f - angle_from_vertical;

    if (angle_from_horizontal < app->auto_layout.min_normal_angle ||
        angle_from_horizontal > app->auto_layout.max_normal_angle)
    {
        return false;
    }

    // Position must be above ground
    if (position.y < 0.01f) return false;

    // Check height constraint if enabled (to exclude canopy or other areas)
    if (app->auto_layout.use_height_constraint)
    {
        if (position.y < app->auto_layout.min_height || position.y > app->auto_layout.max_height)
        {
            return false;
        }
    }

    // Check that the full cell footprint is valid on the mesh
    CellPreset* preset = (CellPreset*)&CELL_PRESETS[app->selected_preset];
    if (!IsCellFootprintValid(app, position, normal, preset->width, preset->height))
    {
        return false;
    }

    return true;
}
// Calculate occlusion score for a position (0 = never occluded, 1 = always occluded)
// Calculate occlusion score for a position (0 = never occluded, 1 = always occluded)
// Now samples multiple vehicle headings to simulate driving in different directions
float CalculateOcclusionScore(AppState* app, Vector3 position, Vector3 normal)
{
    if (!app->mesh_loaded) return 0.0f;

    int occluded_count = 0;
    int total_samples = 0;

    // Save current simulation settings
    SimSettings original = app->sim_settings;

    int heading_samples = 10;

    // Sample multiple vehicle headings (0 to 360 degrees)
    for (int heading_idx = 0; heading_idx < heading_samples; heading_idx++)
    {
        float heading_angle = (360.0f * heading_idx) / heading_samples;
        float heading_rad = heading_angle * DEG2RAD;

        // Sample throughout the day
        for (int hour_idx = 0; hour_idx < app->auto_layout.time_samples; hour_idx++)
        {
            // Sample from 6am to 6pm
            float hour = 6.0f + (12.0f * hour_idx / (app->auto_layout.time_samples - 1));
            app->sim_settings.hour = hour;

            float altitude, azimuth;
            Vector3 sun_dir = CalculateSunDirection(&app->sim_settings, &altitude, &azimuth);

            // Skip if sun is below horizon
            if (altitude <= 0) continue;

            total_samples++;

            // Rotate sun direction to simulate vehicle heading change
            // This is equivalent to rotating the vehicle in the opposite direction
            // We rotate sun_dir around the Y axis by -heading_angle
            Vector3 rotated_sun_dir = {
                sun_dir.x * cosf(-heading_rad) - sun_dir.z * sinf(-heading_rad),
                sun_dir.y,
                sun_dir.x * sinf(-heading_rad) + sun_dir.z * cosf(-heading_rad)
            };

            // Check if cell would face the sun (cosine > 0)
            float facing = Vector3DotProduct(normal, rotated_sun_dir);
            if (facing <= 0)
            {
                occluded_count++; // Not facing sun counts as "occluded"
                continue;
            }

            // Cast ray toward sun to check for occlusion
            Ray ray;
            ray.position = Vector3Add(position, Vector3Scale(normal, 0.01f));
            ray.direction = rotated_sun_dir;

            RayCollision hit = GetRayCollisionMesh(ray, app->vehicle_mesh, app->vehicle_model.transform);
            if (hit.hit && hit.distance > 0.02f)
            {
                occluded_count++;
            }
        }
    }

    // Restore original settings
    app->sim_settings = original;

    return (total_samples > 0) ? (float)occluded_count / total_samples : 1.0f;
}
// Structure to hold candidate positions during auto-layout
#define MAX_CANDIDATES 10000
#define MAX_HEIGHT_SAMPLES 5000

// Find the optimal height range that contains the most valid upward-facing surfaces
// within the specified tolerance (e.g., 10cm vertical band)
void AutoDetectHeightRange(AppState* app)
{
    if (!app->mesh_loaded) return;

    Mesh* mesh = &app->vehicle_mesh;
    Matrix transform = app->vehicle_model.transform;
    float* vertices = mesh->vertices;
    unsigned short* indices = mesh->indices;
    int triangleCount = mesh->triangleCount;

    float tolerance = app->auto_layout.height_tolerance;

    // Collect Z heights of valid upward-facing surfaces
    float* heights = (float*)malloc(MAX_HEIGHT_SAMPLES * sizeof(float));
    int heightCount = 0;

    int step = (triangleCount > MAX_HEIGHT_SAMPLES) ? triangleCount / MAX_HEIGHT_SAMPLES : 1;

    for (int i = 0; i < triangleCount && heightCount < MAX_HEIGHT_SAMPLES; i += step)
    {
        int idx0, idx1, idx2;
        if (indices)
        {
            idx0 = indices[i * 3 + 0];
            idx1 = indices[i * 3 + 1];
            idx2 = indices[i * 3 + 2];
        }
        else
        {
            idx0 = i * 3 + 0;
            idx1 = i * 3 + 1;
            idx2 = i * 3 + 2;
        }

        Vector3 v0 = {vertices[idx0 * 3], vertices[idx0 * 3 + 1], vertices[idx0 * 3 + 2]};
        Vector3 v1 = {vertices[idx1 * 3], vertices[idx1 * 3 + 1], vertices[idx1 * 3 + 2]};
        Vector3 v2 = {vertices[idx2 * 3], vertices[idx2 * 3 + 1], vertices[idx2 * 3 + 2]};

        v0 = Vector3Transform(v0, transform);
        v1 = Vector3Transform(v1, transform);
        v2 = Vector3Transform(v2, transform);

        // Calculate normal
        Vector3 edge1 = Vector3Subtract(v1, v0);
        Vector3 edge2 = Vector3Subtract(v2, v0);
        Vector3 normal = Vector3Normalize(Vector3CrossProduct(edge1, edge2));

        // Only consider upward-facing surfaces
        if (normal.y < MIN_UPWARD_NORMAL) continue;

        // Triangle center height
        float center_y = (v0.y + v1.y + v2.y) / 3.0f;
        heights[heightCount++] = center_y;
    }

    if (heightCount == 0)
    {
        free(heights);
        return;
    }

    // Sort heights (simple bubble sort for now)
    for (int i = 0; i < heightCount - 1; i++)
    {
        for (int j = i + 1; j < heightCount; j++)
        {
            if (heights[j] < heights[i])
            {
                float temp = heights[i];
                heights[i] = heights[j];
                heights[j] = temp;
            }
        }
    }

    // Sliding window to find the height range with most samples
    int bestCount = 0;
    float bestMinY = heights[0];
    float bestMaxY = heights[0] + tolerance;

    for (int i = 0; i < heightCount; i++)
    {
        float windowMin = heights[i];
        float windowMax = windowMin + tolerance;

        // Count samples in this window
        int count = 0;
        for (int j = i; j < heightCount && heights[j] <= windowMax; j++)
        {
            count++;
        }

        if (count > bestCount)
        {
            bestCount = count;
            bestMinY = windowMin;
            bestMaxY = windowMax;
        }
    }

    // Set the detected height range
    app->auto_layout.min_height = bestMinY;
    app->auto_layout.max_height = bestMaxY;

    free(heights);

    SetStatus(app, "Auto-detected height: %.2f - %.2f m (%d surfaces)",
        bestMinY, bestMaxY, bestCount);
}

int RunAutoLayout(AppState* app)
{
    if (!app->mesh_loaded)
    {
        SetStatus(app, "No mesh loaded");
        return 0;
    }

    // Auto-detect optimal height range if enabled
    if (app->auto_layout.use_height_constraint && app->auto_layout.auto_detect_height)
    {
        AutoDetectHeightRange(app);
    }

    app->auto_layout_running = true;
    app->auto_layout_progress = 0;

    CellPreset* preset = (CellPreset*)&CELL_PRESETS[app->selected_preset];
    float cell_area = preset->width * preset->height;
    int target_cells = (int)(app->auto_layout.target_area / cell_area);

    if (target_cells > MAX_CELLS - app->cell_count)
    {
        target_cells = MAX_CELLS - app->cell_count;
    }

    SetStatus(app, "Auto-layout: finding %d cell positions...", target_cells);

    Mesh* mesh = &app->vehicle_mesh;
    Matrix transform = app->vehicle_model.transform;

    // Allocate candidates array
    LayoutCandidate* candidates = (LayoutCandidate*)malloc(MAX_CANDIDATES * sizeof(LayoutCandidate));
    int candidate_count = 0;

    // Calculate grid spacing - use cell dimensions to ensure no overlap
    float grid_spacing = app->auto_layout.grid_spacing;
    if (grid_spacing <= 0)
    {
        // Auto-calculate: use the larger cell dimension with small gap
        grid_spacing = fmaxf(preset->width, preset->height) * MIN_CELL_DISTANCE_FACTOR;
    }

    // Minimum spacing for overlap check (slightly larger than grid to be safe)
    float min_spacing = grid_spacing;

    if (app->auto_layout.use_grid_layout)
    {
        // Grid-based layout: generate regular grid and project onto mesh
        float minX = app->mesh_bounds.min.x;
        float maxX = app->mesh_bounds.max.x;
        float minZ = app->mesh_bounds.min.z;
        float maxZ = app->mesh_bounds.max.z;
        // Note: height constraint is applied later in IsValidSurface()

        // Calculate grid dimensions
        int gridX = (int)((maxX - minX) / grid_spacing) + 1;
        int gridZ = (int)((maxZ - minZ) / grid_spacing) + 1;
        int totalGridPoints = gridX * gridZ;
        SetStatus(app, "Auto-layout: scanning %dx%d grid...", gridX, gridZ);
        // Iterate over grid points
        int processed = 0;
        for (int gx = 0; gx < gridX && candidate_count < MAX_CANDIDATES; gx++)
        {
            for (int gz = 0; gz < gridZ && candidate_count < MAX_CANDIDATES; gz++)
            {
                float x = minX + gx * grid_spacing;
                float z = minZ + gz * grid_spacing;

                // Cast ray downward from above to find mesh surface
                Ray ray;
                ray.position = (Vector3){x, app->mesh_bounds.max.y + 1.0f, z};
                ray.direction = (Vector3){0, -1, 0};

                RayCollision hit = GetRayCollisionMesh(ray, *mesh, transform);
                if (!hit.hit) continue;

                Vector3 position = hit.point;
                Vector3 normal = hit.normal;

                // Check if this is a valid surface
                if (!IsValidSurface(app, position, normal)) continue;

                // Check minimum spacing from existing candidates
                bool too_close = false;
                for (int c = 0; c < candidate_count; c++)
                {
                    if (Vector3Distance(position, candidates[c].position) < min_spacing * 0.9f)
                    {
                        too_close = true;
                        break;
                    }
                }
                if (too_close) continue;

                // Check minimum spacing from existing cells
                for (int c = 0; c < app->cell_count; c++)
                {
                    Vector3 existingPos = CellGetWorldPosition(app, &app->cells[c]);
                    if (Vector3Distance(position, existingPos) < min_spacing)
                    {
                        too_close = true;
                        break;
                    }
                }
                if (too_close) continue;

                // Add candidate
                candidates[candidate_count].position = position;
                candidates[candidate_count].normal = normal;
                candidates[candidate_count].occlusion_score = 0.0f;
                candidates[candidate_count].valid = true;
                candidate_count++;

                processed++;
                if (processed % 100 == 0)
                {
                    app->auto_layout_progress = (processed * 30) / totalGridPoints;
                }
            }
        }
    }
    else
    {
        // Original triangle-based layout
        float* vertices = mesh->vertices;
        unsigned short* indices = mesh->indices;
        int triangleCount = mesh->triangleCount;

        for (int i = 0; i < triangleCount && candidate_count < MAX_CANDIDATES; i++)
        {
            int idx0, idx1, idx2;
            if (indices)
            {
                idx0 = indices[i * 3 + 0];
                idx1 = indices[i * 3 + 1];
                idx2 = indices[i * 3 + 2];
            }
            else
            {
                idx0 = i * 3 + 0;
                idx1 = i * 3 + 1;
                idx2 = i * 3 + 2;
            }

            Vector3 v0 = {vertices[idx0 * 3], vertices[idx0 * 3 + 1], vertices[idx0 * 3 + 2]};
            Vector3 v1 = {vertices[idx1 * 3], vertices[idx1 * 3 + 1], vertices[idx1 * 3 + 2]};
            Vector3 v2 = {vertices[idx2 * 3], vertices[idx2 * 3 + 1], vertices[idx2 * 3 + 2]};

            v0 = Vector3Transform(v0, transform);
            v1 = Vector3Transform(v1, transform);
            v2 = Vector3Transform(v2, transform);

            Vector3 edge1 = Vector3Subtract(v1, v0);
            Vector3 edge2 = Vector3Subtract(v2, v0);
            Vector3 normal = Vector3Normalize(Vector3CrossProduct(edge1, edge2));

            Vector3 center = {
                (v0.x + v1.x + v2.x) / 3.0f,
                (v0.y + v1.y + v2.y) / 3.0f,
                (v0.z + v1.z + v2.z) / 3.0f
            };

            if (!IsValidSurface(app, center, normal)) continue;

            bool too_close = false;
            for (int c = 0; c < candidate_count; c++)
            {
                if (Vector3Distance(center, candidates[c].position) < min_spacing)
                {
                    too_close = true;
                    break;
                }
            }
            if (too_close) continue;

            for (int c = 0; c < app->cell_count; c++)
            {
                Vector3 existingPos = CellGetWorldPosition(app, &app->cells[c]);
                if (Vector3Distance(center, existingPos) < min_spacing)
                {
                    too_close = true;
                    break;
                }
            }
            if (too_close) continue;

            candidates[candidate_count].position = center;
            candidates[candidate_count].normal = normal;
            candidates[candidate_count].occlusion_score = 0.0f;
            candidates[candidate_count].valid = true;
            candidate_count++;

            app->auto_layout_progress = (i * 30) / triangleCount;
        }
    }

    SetStatus(app, "Auto-layout: scoring %d candidates...", candidate_count);

    // Second pass: calculate occlusion scores if optimization is enabled
    if (app->auto_layout.optimize_occlusion && candidate_count > 0)
    {
        for (int i = 0; i < candidate_count; i++)
        {
            candidates[i].occlusion_score = CalculateOcclusionScore(
                app,
                candidates[i].position,
                candidates[i].normal
            );
            app->auto_layout_progress = 30 + (i * 50) / candidate_count;
        }

        // Sort candidates by occlusion score (lowest first = best positions)
        for (int i = 0; i < candidate_count - 1; i++)
        {
            for (int j = i + 1; j < candidate_count; j++)
            {
                if (candidates[j].occlusion_score < candidates[i].occlusion_score)
                {
                    LayoutCandidate temp = candidates[i];
                    candidates[i] = candidates[j];
                    candidates[j] = temp;
                }
            }
        }
    }

    // Third pass: place cells at best positions
    int placed = 0;
    for (int i = 0; i < candidate_count && placed < target_cells; i++)
    {
        if (!candidates[i].valid) continue;

        // Try to place cell
        int id = PlaceCell(app, candidates[i].position, candidates[i].normal);
        if (id >= 0)
        {
            placed++;

            // Mark nearby candidates as invalid to ensure spacing
            for (int j = i + 1; j < candidate_count; j++)
            {
                if (Vector3Distance(candidates[i].position, candidates[j].position) < min_spacing)
                {
                    candidates[j].valid = false;
                }
            }
        }

        if (target_cells > 0)
        {
            app->auto_layout_progress = 80 + (placed * 20) / target_cells;
        }
    }

    free(candidates);

    app->auto_layout_running = false;
    app->auto_layout_progress = 100;

    SetStatus(app, "Auto-layout: placed %d cells (%.2f m²)",
        placed, placed * cell_area);

    return placed;
}

// Draw preview of valid surfaces for auto-layout
void DrawAutoLayoutPreview(AppState* app)
{
    if (!app->auto_layout.preview_surface) return;
    if (!app->mesh_loaded) return;

    Mesh* mesh = &app->vehicle_mesh;
    Matrix transform = app->vehicle_model.transform;
    float* vertices = mesh->vertices;
    unsigned short* indices = mesh->indices;
    int triangleCount = mesh->triangleCount;

    Color validColor = (Color){0, 200, 0, 100};    // Green for valid surfaces

    // Sample triangles for preview (for performance)
    int step = (triangleCount > 2000) ? triangleCount / 500 : 1;

    for (int i = 0; i < triangleCount; i += step)
    {
        int idx0, idx1, idx2;
        if (indices)
        {
            idx0 = indices[i * 3 + 0];
            idx1 = indices[i * 3 + 1];
            idx2 = indices[i * 3 + 2];
        }
        else
        {
            idx0 = i * 3 + 0;
            idx1 = i * 3 + 1;
            idx2 = i * 3 + 2;
        }

        Vector3 v0 = {vertices[idx0 * 3], vertices[idx0 * 3 + 1], vertices[idx0 * 3 + 2]};
        Vector3 v1 = {vertices[idx1 * 3], vertices[idx1 * 3 + 1], vertices[idx1 * 3 + 2]};
        Vector3 v2 = {vertices[idx2 * 3], vertices[idx2 * 3 + 1], vertices[idx2 * 3 + 2]};

        v0 = Vector3Transform(v0, transform);
        v1 = Vector3Transform(v1, transform);
        v2 = Vector3Transform(v2, transform);

        Vector3 edge1 = Vector3Subtract(v1, v0);
        Vector3 edge2 = Vector3Subtract(v2, v0);
        Vector3 normal = Vector3Normalize(Vector3CrossProduct(edge1, edge2));

        Vector3 center = {
            (v0.x + v1.x + v2.x) / 3.0f,
            (v0.y + v1.y + v2.y) / 3.0f,
            (v0.z + v1.z + v2.z) / 3.0f
        };

        // Offset slightly to avoid z-fighting
        Vector3 offset = Vector3Scale(normal, 0.003f);
        Vector3 sv0 = Vector3Add(v0, offset);
        Vector3 sv1 = Vector3Add(v1, offset);
        Vector3 sv2 = Vector3Add(v2, offset);

        if (IsValidSurface(app, center, normal))
        {
            DrawTriangle3D(sv0, sv1, sv2, validColor);
        }
    }
}

//------------------------------------------------------------------------------
// Snap Controls
//------------------------------------------------------------------------------
void InitSnap(AppState* app)
{
    app->snap.grid_snap_enabled = false;
    app->snap.grid_size = 0.125f;  // Default to cell size (125mm)
    app->snap.align_to_surface = true;
    app->snap.show_grid = false;
}

Vector3 ApplyGridSnap(AppState* app, Vector3 position)
{
    if (!app->snap.grid_snap_enabled) return position;

    float grid = app->snap.grid_size;
    if (grid <= 0) return position;

    // Snap X and Z to grid (Y follows surface)
    Vector3 snapped = position;
    snapped.x = roundf(position.x / grid) * grid;
    snapped.z = roundf(position.z / grid) * grid;

    // If mesh is loaded, project snapped position back onto surface
    if (app->mesh_loaded)
    {
        // Cast ray downward from above the snapped position
        Ray ray;
        ray.position = (Vector3){snapped.x, app->mesh_bounds.max.y + 1.0f, snapped.z};
        ray.direction = (Vector3){0, -1, 0};

        RayCollision hit = GetRayCollisionMesh(ray, app->vehicle_mesh, app->vehicle_model.transform);
        if (hit.hit)
        {
            snapped.y = hit.point.y;
        }
    }

    return snapped;
}

void DrawSnapGrid(AppState* app)
{
    if (!app->snap.show_grid) return;
    if (!app->mesh_loaded) return;

    float grid = app->snap.grid_size;
    if (grid <= 0) return;

    // Draw grid lines on the XZ plane at mesh height
    float y = app->mesh_bounds.max.y + 0.01f;
    float extentX = (app->mesh_bounds.max.x - app->mesh_bounds.min.x) * 0.6f;
    float extentZ = (app->mesh_bounds.max.z - app->mesh_bounds.min.z) * 0.6f;

    // Center of mesh
    float centerX = (app->mesh_bounds.min.x + app->mesh_bounds.max.x) / 2.0f;
    float centerZ = (app->mesh_bounds.min.z + app->mesh_bounds.max.z) / 2.0f;

    Color gridColor = (Color){100, 100, 255, 80};

    // Snap extents to grid
    float startX = floorf((centerX - extentX) / grid) * grid;
    float endX = ceilf((centerX + extentX) / grid) * grid;
    float startZ = floorf((centerZ - extentZ) / grid) * grid;
    float endZ = ceilf((centerZ + extentZ) / grid) * grid;

    // Limit number of lines for performance
    int maxLines = 50;
    int linesX = (int)((endX - startX) / grid);
    int linesZ = (int)((endZ - startZ) / grid);

    if (linesX > maxLines)
    {
        float newGrid = (endX - startX) / maxLines;
        grid = newGrid;
        startX = floorf((centerX - extentX) / grid) * grid;
        endX = ceilf((centerX + extentX) / grid) * grid;
    }
    if (linesZ > maxLines)
    {
        float newGrid = (endZ - startZ) / maxLines;
        if (newGrid > grid) grid = newGrid;
        startZ = floorf((centerZ - extentZ) / grid) * grid;
        endZ = ceilf((centerZ + extentZ) / grid) * grid;
    }

    // Draw X-parallel lines (varying Z)
    for (float z = startZ; z <= endZ; z += grid)
    {
        DrawLine3D(
            (Vector3){startX, y, z},
            (Vector3){endX, y, z},
            gridColor
        );
    }

    // Draw Z-parallel lines (varying X)
    for (float x = startX; x <= endX; x += grid)
    {
        DrawLine3D(
            (Vector3){x, y, startZ},
            (Vector3){x, y, endZ},
            gridColor
        );
    }
}

//------------------------------------------------------------------------------
// Simulation
//------------------------------------------------------------------------------
Vector3 CalculateSunDirection(SimSettings* s, float* out_alt, float* out_az)
{
    // Simplified solar position algorithm (NOAA method)
    float lat = s->latitude * DEG2RAD;

    // Clamp latitude to avoid edge cases at poles
    lat = Clampf(lat, -89.0f * DEG2RAD, 89.0f * DEG2RAD);

    // Day of year (approximate)
    int doy = (s->month - 1) * 30 + s->day;
    doy = (doy < 1) ? 1 : (doy > 365) ? 365 : doy;

    // Fractional year (gamma) for equation of time and declination
    float gamma = 2.0f * PI / 365.0f * (doy - 1);

    // Equation of time (in minutes) - accounts for Earth's elliptical orbit
    float eqtime = 229.18f * (0.000075f + 0.001868f * cosf(gamma)
        - 0.032077f * sinf(gamma)
        - 0.014615f * cosf(2 * gamma)
        - 0.040849f * sinf(2 * gamma));

    // Solar declination (in radians)
    float decl = 0.006918f - 0.399912f * cosf(gamma)
        + 0.070257f * sinf(gamma)
        - 0.006758f * cosf(2 * gamma)
        + 0.000907f * sinf(2 * gamma)
        - 0.002697f * cosf(3 * gamma)
        + 0.00148f * sinf(3 * gamma);

    // Calculate timezone offset from longitude (approximate standard timezone)
    // Each 15 degrees of longitude = 1 hour timezone difference
    // This assumes standard time (not daylight saving)
    float timezone_offset = roundf(s->longitude / 15.0f);  // hours from UTC

    // Convert local clock time to solar time
    // Solar time = clock time + 4*(longitude - timezone*15) + equation_of_time
    // The 4 is because Earth rotates 1 degree every 4 minutes
    float longitude_correction = 4.0f * (s->longitude - timezone_offset * 15.0f); // minutes
    float solar_time_minutes = s->hour * 60.0f + longitude_correction + eqtime;

    // Hour angle: solar noon is 0 degrees, morning is negative, afternoon is positive
    // At solar noon (12:00 solar time), hour angle = 0
    float ha = (solar_time_minutes / 4.0f) - 180.0f;  // degrees
    float ha_rad = ha * DEG2RAD;

    // Solar zenith angle calculation
    float cos_zen = sinf(lat) * sinf(decl) + cosf(lat) * cosf(decl) * cosf(ha_rad);
    cos_zen = Clampf(cos_zen, -1.0f, 1.0f);

    float zenith = acosf(cos_zen);
    float altitude = 90.0f - zenith * RAD2DEG;

    // Azimuth calculation
    float sin_zen = sinf(zenith);
    float azimuth = 0.0f;

    if (fabsf(sin_zen) > 0.001f)
    {
        float cos_az = (sinf(decl) - sinf(lat) * cos_zen) / (cosf(lat) * sin_zen);
        cos_az = Clampf(cos_az, -1.0f, 1.0f);
        azimuth = acosf(cos_az) * RAD2DEG;
        if (ha > 0) azimuth = 360.0f - azimuth;
    }
    else
    {
        azimuth = 180.0f;
    }

    if (out_alt) *out_alt = altitude;
    if (out_az) *out_az = azimuth;

    // Return direction vector toward the sun
    // If sun is below horizon, return downward vector
    if (altitude <= 0)
    {
        return (Vector3){0, -1, 0};
    }

    float alt_rad = altitude * DEG2RAD;
    float az_rad = azimuth * DEG2RAD;

    Vector3 dir = {
        cosf(alt_rad) * sinf(az_rad),
        sinf(alt_rad),
        -cosf(alt_rad) * cosf(az_rad)
    };

    return Vector3Normalize(dir);
}
bool CheckCellShading(AppState* app, SolarCell* cell, Vector3 sun_dir)
{
    if (!app->mesh_loaded) return false;

    // Get world coordinates
    Vector3 worldPos = CellGetWorldPosition(app, cell);
    Vector3 worldNormal = CellGetWorldNormal(app, cell);

    // Ray from cell toward sun
    Ray ray;
    ray.position = Vector3Add(worldPos, Vector3Scale(worldNormal, 0.01f));
    ray.direction = sun_dir;

    // Check collision with mesh
    RayCollision hit = GetRayCollisionMesh(ray, app->vehicle_mesh, app->vehicle_model.transform);

    return hit.hit;
}

float CalculateCellPower(AppState* app, SolarCell* cell, Vector3 sun_dir, CellPreset* preset, float irradiance)
{
    if (cell->is_shaded) return 0.0f;

    // Get world normal for angle calculation
    Vector3 worldNormal = CellGetWorldNormal(app, cell);

    // Cosine of angle between normal and sun
    float cosAngle = Vector3DotProduct(worldNormal, sun_dir);
    cosAngle = fmaxf(0.0f, cosAngle);

    // Power calculation
    float area = preset->width * preset->height;
    float power = irradiance * area * cosAngle * preset->efficiency;

    return power;
}

void RunStaticSimulation(AppState* app)
{
    if (app->cell_count == 0)
    {
        SetStatus(app, "No cells to simulate");
        return;
    }

    CellPreset* preset = (CellPreset*)&CELL_PRESETS[app->selected_preset];

    // Calculate sun position

    app->sim_results.sun_direction = CalculateSunDirection(
        &app->sim_settings,
        &app->sim_results.sun_altitude,
        &app->sim_results.sun_azimuth
    );

    app->sim_results.is_daytime = (app->sim_results.sun_altitude > 0);

    // Reset results
    app->sim_results.total_power = 0;
    app->sim_results.shaded_count = 0;

    // Reset string powers
    for (int s = 0; s < app->string_count; s++)
    {
        app->strings[s].total_power = 0;
    }

    // Calculate for each cell
    for (int i = 0; i < app->cell_count; i++)
    {
        SolarCell* cell = &app->cells[i];

        if (!app->sim_results.is_daytime)
        {
            cell->is_shaded = true;
            cell->power_output = 0;
        }
        else
        {
            cell->is_shaded = CheckCellShading(app, cell, app->sim_results.sun_direction);
            cell->power_output = CalculateCellPower(
                app,
                cell,
                app->sim_results.sun_direction,
                preset,
                app->sim_settings.irradiance
            );
        }

        if (cell->is_shaded) app->sim_results.shaded_count++;
        app->sim_results.total_power += cell->power_output;

        // Add to string power
        if (cell->string_id >= 0)
        {
            for (int s = 0; s < app->string_count; s++)
            {
                if (app->strings[s].id == cell->string_id)
                {
                    app->strings[s].total_power += cell->power_output;
                    break;
                }
            }
        }
    }

    app->sim_results.shaded_percentage =
        (app->cell_count > 0) ? (100.0f * app->sim_results.shaded_count / app->cell_count) : 0;

    app->sim_run = true;

    SetStatus(app, "Simulation: %.1fW total, %.1f%% shaded",
        app->sim_results.total_power, app->sim_results.shaded_percentage);
}
void RunTimeSimulationAnimated(AppState* app)
{
    if (app->cell_count == 0 || !app->mesh_loaded) {
        SetStatus(app, "No cells or mesh to simulate");
        return;
    }

    CellPreset* preset = (CellPreset*)&CELL_PRESETS[app->selected_preset];

    const int TIME_SAMPLES = 48;
    const int HEADING_SAMPLES = 36;
    const float START_HOUR = 6.0f;
    const float DURATION = 12.0f;
    const float dt_hours = DURATION / (float)(TIME_SAMPLES - 1);
    const float heading_step = 360.0f / HEADING_SAMPLES;

    // Accumulators - these accumulate across ALL time steps and headings
    float* cell_energy = (float*)calloc(app->cell_count, sizeof(float));
    float* string_energy = (float*)calloc(app->string_count, sizeof(float));
    if (!cell_energy) return;

    float total_energy = 0.0f;
    float peak_power = 0.0f;
    int total_samples = 0;
    int shaded_samples = 0;

    // Clear hourly data
    for (int h = 0; h < 24; h++) {
        app->time_sim_results.energy_by_hour[h] = 0;
    }

    int step = 0;
    int total_steps = TIME_SAMPLES * HEADING_SAMPLES;

    // Main simulation loop: TIME x HEADING
    for (int ti = 0; ti < TIME_SAMPLES; ti++)
    {
        float hour = START_HOUR + (DURATION * ti / (float)(TIME_SAMPLES - 1));

        // Calculate sun direction once per time step
        app->sim_settings.hour = hour;
        float altitude, azimuth;
        Vector3 sun_dir = CalculateSunDirection(&app->sim_settings, &altitude, &azimuth);

        float effective_irradiance = 0.0f;
        //janky air mass approximation
        if (altitude > 0.0f)
        {
            float sin_alt = sinf(altitude * DEG2RAD);
            float air_mass = 1.0f / fmaxf(sin_alt, 0.01f);  //awesome hardcoded constants. todo, remove these
            float atmospheric_factor = powf(0.7f, powf(air_mass, 0.678f));
            effective_irradiance = app->sim_settings.irradiance * atmospheric_factor;
        }
        // Store for visualization
        app->sim_results.sun_altitude = altitude;
        app->sim_results.sun_azimuth = azimuth;
        app->sim_results.is_daytime = (altitude > 0);

        // Skip night time
        if (altitude <= 0) {
            step += HEADING_SAMPLES;
            continue;
        }

        // Accumulator for this time step (sum across all headings, then average)
        float time_step_power_sum = 0.0f;

        // Temporary accumulators for this time step's cell energy
        // (we average over headings, then add to total cell_energy)
        float* cell_power_this_timestep = (float*)calloc(app->cell_count, sizeof(float));

        // Inner loop: HEADING (vehicle rotation)
        for (int hi = 0; hi < HEADING_SAMPLES; hi++)
        {
            float heading_deg = hi * heading_step;
            float heading_rad = heading_deg * DEG2RAD;

            // Check for cancel
            PollInputEvents();
            if (WindowShouldClose() || IsKeyDown(KEY_ESCAPE))
            {
                free(cell_energy);
                free(cell_power_this_timestep);
                if (string_energy) free(string_energy);
                SetStatus(app, "Simulation cancelled");
                return;
            }

            // Rotate sun direction relative to vehicle heading
            Vector3 rotated_sun = {
                sun_dir.x * cosf(-heading_rad) - sun_dir.z * sinf(-heading_rad),
                sun_dir.y,
                sun_dir.x * sinf(-heading_rad) + sun_dir.z * cosf(-heading_rad)
            };

            // Set for visualization
            app->sim_results.sun_direction = rotated_sun;

            float instant_power = 0.0f;

            for (int c = 0; c < app->cell_count; c++)
            {
                SolarCell* cell = &app->cells[c];
                Vector3 pos = CellGetWorldPosition(app, cell);
                Vector3 norm = CellGetWorldNormal(app, cell);

                total_samples++;
                float facing = Vector3DotProduct(norm, rotated_sun);

                if (facing <= 0) {
                    shaded_samples++;
                    cell->is_shaded = true;
                    cell->power_output = 0;
                    continue;
                }

                // Occlusion check
                Ray ray = { Vector3Add(pos, Vector3Scale(norm, 0.01f)), rotated_sun };
                RayCollision hit = GetRayCollisionMesh(ray, app->vehicle_mesh, app->vehicle_model.transform);

                if (hit.hit && hit.distance > 0.02f) {
                    shaded_samples++;
                    cell->is_shaded = true;
                    cell->power_output = 0;
                    continue;
                }

                cell->is_shaded = false;
                float area = preset->width * preset->height;
                float power_w = effective_irradiance * area * facing * preset->efficiency;

                instant_power += power_w;
                cell->power_output = power_w;

                // Accumulate this cell's power for this time step (will average over headings)
                cell_power_this_timestep[c] += power_w;
            }

            // Track peak instantaneous power
            if (instant_power > peak_power) {
                peak_power = instant_power;
            }

            time_step_power_sum += instant_power;
            step++;

            // Redraw periodically
            if (hi % 3 == 0)
            {
                int progress = (step * 100) / total_steps;

                BeginDrawing();
                ClearBackground(BLACK);
                AppDraw(app);

                // Draw progress overlay
                int cx = app->screen_width / 2;
                int cy = app->screen_height / 2 - 200;
                DrawRectangle(0, 0, app->screen_width, app->screen_height, (Color){0,0,0,100});

                DrawRectangle(cx - 175, cy - 55, 350, 110, (Color){30,30,30,245});
                DrawRectangleLines(cx - 175, cy - 55, 350, 110, WHITE);

                DrawText("Time Sim (esc to cancel)", cx - 70, cy - 45, 20, WHITE);
                DrawText(TextFormat("Time: %.1f:00", hour), cx - 140, cy - 15, 16, LIGHTGRAY);
                DrawText(TextFormat("Heading: %.0f deg", heading_deg), cx + 20, cy - 15, 16, LIGHTGRAY);
                DrawText(TextFormat("Energy so far: %.1f Wh", total_energy), cx - 80, cy + 5, 16, YELLOW);

                // Progress bar
                int barY = cy + 30;
                DrawRectangle(cx - 150, barY, 300, 18, DARKGRAY);
                DrawRectangle(cx - 150, barY, (300 * progress) / 100, 18, GREEN);
                DrawRectangleLines(cx - 150, barY, 300, 18, WHITE);
                DrawText(TextFormat("%d%%", progress), cx - 12, barY + 2, 14, WHITE);

                EndDrawing();
            }
        }

        // Average power over all headings for this time step
        float avg_power_this_timestep = time_step_power_sum / (float)HEADING_SAMPLES;

        // Energy for this time step = average power * time duration
        float energy_this_timestep = avg_power_this_timestep * dt_hours;
        total_energy += energy_this_timestep;

        // Store in hourly bucket
        int hour_bucket = (int)hour;
        if (hour_bucket >= 0 && hour_bucket < 24) {
            app->time_sim_results.energy_by_hour[hour_bucket] += energy_this_timestep;
        }

        // Now add this time step's per-cell energy contribution
        for (int c = 0; c < app->cell_count; c++) {
            // Average power for this cell at this time step, then convert to energy
            float avg_cell_power = cell_power_this_timestep[c] / (float)HEADING_SAMPLES;
            float cell_energy_step = avg_cell_power * dt_hours;
            cell_energy[c] += cell_energy_step;

            // Add to string energy
            SolarCell* cell = &app->cells[c];
            if (cell->string_id >= 0 && string_energy) {
                for (int s = 0; s < app->string_count; s++) {
                    if (app->strings[s].id == cell->string_id) {
                        string_energy[s] += cell_energy_step;
                        break;
                    }
                }
            }
        }

        free(cell_power_this_timestep);
    }

    // Finalize results
    float daylight_hours = DURATION;

    for (int i = 0; i < app->cell_count; i++) {
        // Average power = total energy / hours
        app->cells[i].power_output = cell_energy[i] / daylight_hours;

        // Mark as shaded if got less than 30% of theoretical max
        float theoretical_max = preset->width * preset->height * preset->efficiency
                              * app->sim_settings.irradiance * daylight_hours * 0.5f;
        app->cells[i].is_shaded = (cell_energy[i] < theoretical_max * 0.3f);
    }

    for (int s = 0; s < app->string_count; s++) {
        app->strings[s].total_energy_wh = string_energy[s];
        app->strings[s].total_power = string_energy[s] / daylight_hours;
    }

    app->time_sim_results.total_energy_wh = total_energy;
    app->time_sim_results.average_power_w = total_energy / daylight_hours;
    app->time_sim_results.peak_power_w = peak_power;
    app->time_sim_results.average_shaded_pct = (total_samples > 0)
        ? (100.0f * shaded_samples / total_samples) : 0.0f;

    app->sim_results.total_power = app->time_sim_results.average_power_w;
    app->sim_results.shaded_percentage = app->time_sim_results.average_shaded_pct;

    // Count shaded cells
    app->sim_results.shaded_count = 0;
    for (int i = 0; i < app->cell_count; i++) {
        if (app->cells[i].is_shaded) app->sim_results.shaded_count++;
    }

    // Reset sun to noon for final view
    app->sim_settings.hour = 12.0f;
    app->sim_results.sun_direction = CalculateSunDirection(
        &app->sim_settings, &app->sim_results.sun_altitude, &app->sim_results.sun_azimuth);
    app->sim_results.is_daytime = true;

    app->sim_run = true;
    app->time_sim_run = true;

    free(cell_energy);
    if (string_energy) free(string_energy);

    SetStatus(app, "Daily: %.1f Wh total, %.1f W avg, %.1f W peak",
        total_energy, total_energy / daylight_hours, peak_power);
}
//------------------------------------------------------------------------------
// Update & Draw
//------------------------------------------------------------------------------
void AppUpdate(AppState* app)
{
    // Keyboard shortcuts
    // if (IsKeyPressed(KEY_ONE)) app->mode = MODE_IMPORT;
    // if (IsKeyPressed(KEY_TWO)) app->mode = MODE_CELL_PLACEMENT;
    // if (IsKeyPressed(KEY_THREE)) app->mode = MODE_WIRING;
    // if (IsKeyPressed(KEY_FOUR)) app->mode = MODE_SIMULATION;

    if (IsKeyPressed(KEY_T))
    {
        CameraSetOrthographic(&app->cam, !app->cam.is_orthographic);
    }

    if (IsKeyPressed(KEY_R))
    {
        CameraReset(&app->cam, app->mesh_bounds);
    }

    if (IsKeyPressed(KEY_S) && !IsKeyDown(KEY_LEFT_CONTROL))
    {
        if (app->mode == MODE_SIMULATION)
        {
            RunStaticSimulation(app);
        }
    }

    if (IsKeyPressed(KEY_N) && app->mode == MODE_WIRING)
    {
        StartNewString(app);
    }

    if (IsKeyPressed(KEY_E) && app->mode == MODE_WIRING)
    {
        EndCurrentString(app);
    }

    if (IsKeyPressed(KEY_ESCAPE) && app->mode == MODE_WIRING)
    {
        CancelCurrentString(app);
    }

    // Auto switch camera mode based on app mode
    if (app->mode == MODE_CELL_PLACEMENT || app->mode == MODE_WIRING)
    {
        if (!app->cam.is_orthographic)
        {
            CameraSetOrthographic(&app->cam, true);
        }
    }

    // Update camera
    CameraUpdate(&app->cam, app);

    // Handle mouse picking (only when over 3D view)
    Vector2 mouse = GetMousePosition();
    if (mouse.x > app->sidebar_width && app->mesh_loaded)
    {
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        {
            Ray ray = GetMouseRay(mouse, app->cam.camera);

            if (app->mode == MODE_CELL_PLACEMENT)
            {
                if (app->placing_module && app->selected_module >= 0)
                {
                    // Place module at clicked location
                    RayCollision hit = GetRayCollisionMesh(ray, app->vehicle_mesh, app->vehicle_model.transform);
                    if (hit.hit)
                    {
                        PlaceModule(app, app->selected_module, hit.point, hit.normal);
                        // Stay in placing mode for multiple placements
                    }
                }
                else
                {
                    // Check for existing cell first (to remove)
                    int cell_id = FindCellNearRay(app, ray, NULL);
                    if (cell_id >= 0)
                    {
                        RemoveCell(app, cell_id);
                    }
                    else
                    {
                        // Try to place new cell
                        RayCollision hit = GetRayCollisionMesh(ray, app->vehicle_mesh, app->vehicle_model.transform);
                        if (hit.hit)
                        {
                            PlaceCell(app, hit.point, hit.normal);
                        }
                    }
                }
            }
            else if (app->mode == MODE_WIRING)
            {
                int cell_id = FindCellNearRay(app, ray, NULL);
                if (cell_id >= 0)
                {
                    AddCellToString(app, cell_id);
                }
            }
        }

        if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT))
        {
            if (app->mode == MODE_CELL_PLACEMENT)
            {
                Ray ray = GetMouseRay(mouse, app->cam.camera);
                int cell_id = FindCellNearRay(app, ray, NULL);
                if (cell_id >= 0)
                {
                    RemoveCell(app, cell_id);
                }
            }
            else if (app->mode == MODE_WIRING)
            {
                EndCurrentString(app);
            }
        }
    }
}

void DrawCell(AppState* app, SolarCell* cell)
{
    CellPreset* preset = (CellPreset*)&CELL_PRESETS[app->selected_preset];

    // Determine color
    Color color = COLOR_CELL_UNWIRED;

    if (app->sim_run && cell->is_shaded)
    {
        color = COLOR_CELL_SHADED;
    }
    else if (app->sim_run && app->sim_results.total_power > 0)
    {
        // Color by power output (red to green)
        float maxPower = preset->width * preset->height * preset->efficiency * app->sim_settings.irradiance;
        float ratio = cell->power_output / maxPower;
        color = LerpColor(RED, GREEN, ratio);
        color.a = 230;
    }
    else if (cell->string_id >= 0)
    {
        // Use string color
        for (int s = 0; s < app->string_count; s++)
        {
            if (app->strings[s].id == cell->string_id)
            {
                color = app->strings[s].color;
                break;
            }
        }
    }

    // Draw cell as a quad (use world coordinates)
    Vector3 worldPos = CellGetWorldPosition(app, cell);
    Vector3 worldNormal = CellGetWorldNormal(app, cell);
    Vector3 pos = Vector3Add(worldPos, Vector3Scale(worldNormal, CELL_SURFACE_OFFSET));

    // Get the stored tangent transformed to world space
    Vector3 right = CellGetWorldTangent(app, cell);

    // Forward is perpendicular to both normal and right
    Vector3 forward = Vector3CrossProduct(worldNormal, right);

    // Scale to cell size
    right = Vector3Scale(right, preset->width / 2);
    forward = Vector3Scale(forward, preset->height / 2);

    // Draw quad
    Vector3 p1 = Vector3Add(pos, Vector3Add(Vector3Scale(right, -1), Vector3Scale(forward, -1)));
    Vector3 p2 = Vector3Add(pos, Vector3Add(right, Vector3Scale(forward, -1)));
    Vector3 p3 = Vector3Add(pos, Vector3Add(right, forward));
    Vector3 p4 = Vector3Add(pos, Vector3Add(Vector3Scale(right, -1), forward));

    DrawTriangle3D(p1, p2, p3, color);
    DrawTriangle3D(p1, p3, p4, color);

    // Draw outline
    Color outline = BLACK;
    outline.a = 100;
    DrawLine3D(p1, p2, outline);
    DrawLine3D(p2, p3, outline);
    DrawLine3D(p3, p4, outline);
    DrawLine3D(p4, p1, outline);
}
void DrawWiring(AppState* app)
{
    for (int s = 0; s < app->string_count; s++)
    {
        CellString* str = &app->strings[s];
        if (str->cell_count < 2) continue;

        // Collect positions in order
        Vector3 positions[MAX_CELLS_PER_STRING];

        for (int i = 0; i < str->cell_count; i++)
        {
            // Find cell by id and order
            for (int c = 0; c < app->cell_count; c++)
            {
                if (app->cells[c].string_id == str->id && app->cells[c].order_in_string == i)
                {
                    Vector3 worldPos = CellGetWorldPosition(app, &app->cells[c]);
                    Vector3 worldNormal = CellGetWorldNormal(app, &app->cells[c]);
                    positions[i] = Vector3Add(
                        worldPos,
                        Vector3Scale(worldNormal, CELL_SURFACE_OFFSET + 0.001f)
                    );
                    break;
                }
            }
        }

        // Draw lines
        for (int i = 0; i < str->cell_count - 1; i++)
        {
            DrawLine3D(positions[i], positions[i + 1], str->color);
        }
    }
}

// Project a point onto the ground plane (Y=0) along the sun direction
static Vector3 ProjectToGround(Vector3 point, Vector3 sun_dir)
{
    // Avoid division by zero if sun is horizontal
    if (fabsf(sun_dir.y) < 0.001f) return point;

    // How far along -sun_dir to reach Y=0
    float t = point.y / sun_dir.y;

    return (Vector3){
        point.x - sun_dir.x * t,
        0.001f, // Slightly above ground to avoid z-fighting
        point.z - sun_dir.z * t
    };
}

void DrawMeshShadow(AppState* app)
{
    if (!app->sim_run || !app->sim_results.is_daytime) return;
    if (!app->mesh_loaded) return;

    Vector3 sun_dir = app->sim_results.sun_direction;

    // Skip if sun too low (shadows too long)
    if (sun_dir.y < 0.1f) return;

    Mesh* mesh = &app->vehicle_mesh;
    Matrix transform = app->vehicle_model.transform;

    // Get mesh vertex data
    float* vertices = mesh->vertices;
    unsigned short* indices = mesh->indices;
    int triangleCount = mesh->triangleCount;

    Color shadowColor = (Color){0, 0, 0, 60};

    // Draw shadow triangles on ground (sample every Nth triangle for performance)
    int step = (triangleCount > 5000) ? triangleCount / 1000 : 1;

    for (int i = 0; i < triangleCount; i += step)
    {
        int idx0, idx1, idx2;
        if (indices)
        {
            idx0 = indices[i * 3 + 0];
            idx1 = indices[i * 3 + 1];
            idx2 = indices[i * 3 + 2];
        }
        else
        {
            idx0 = i * 3 + 0;
            idx1 = i * 3 + 1;
            idx2 = i * 3 + 2;
        }

        // Get vertices and transform them
        Vector3 v0 = {vertices[idx0 * 3], vertices[idx0 * 3 + 1], vertices[idx0 * 3 + 2]};
        Vector3 v1 = {vertices[idx1 * 3], vertices[idx1 * 3 + 1], vertices[idx1 * 3 + 2]};
        Vector3 v2 = {vertices[idx2 * 3], vertices[idx2 * 3 + 1], vertices[idx2 * 3 + 2]};

        v0 = Vector3Transform(v0, transform);
        v1 = Vector3Transform(v1, transform);
        v2 = Vector3Transform(v2, transform);

        // Only project triangles that are above ground and facing somewhat upward
        Vector3 edge1 = Vector3Subtract(v1, v0);
        Vector3 edge2 = Vector3Subtract(v2, v0);
        Vector3 normal = Vector3CrossProduct(edge1, edge2);
        if (normal.y < 0) continue; // Skip downward-facing triangles

        // Project to ground
        Vector3 s0 = ProjectToGround(v0, sun_dir);
        Vector3 s1 = ProjectToGround(v1, sun_dir);
        Vector3 s2 = ProjectToGround(v2, sun_dir);

        // Draw shadow triangle
        DrawTriangle3D(s0, s1, s2, shadowColor);
    }
}

// Draw shadows ON the mesh itself (occluded regions shown darker)
void DrawMeshShadowsOnSurface(AppState* app)
{
    if (!app->sim_run || !app->sim_results.is_daytime) return;
    if (!app->mesh_loaded) return;

    Vector3 sun_dir = app->sim_results.sun_direction;
    if (sun_dir.y < 0.05f) return;

    Mesh* mesh = &app->vehicle_mesh;
    Matrix transform = app->vehicle_model.transform;

    float* vertices = mesh->vertices;
    unsigned short* indices = mesh->indices;
    int triangleCount = mesh->triangleCount;

    Color shadowOnMesh = (Color){0, 0, 50, 120}; // Dark blue tint for shadowed areas

    // Sample triangles for shadow testing (for performance)
    int step = (triangleCount > 2000) ? triangleCount / 500 : 1;

    for (int i = 0; i < triangleCount; i += step)
    {
        int idx0, idx1, idx2;
        if (indices)
        {
            idx0 = indices[i * 3 + 0];
            idx1 = indices[i * 3 + 1];
            idx2 = indices[i * 3 + 2];
        }
        else
        {
            idx0 = i * 3 + 0;
            idx1 = i * 3 + 1;
            idx2 = i * 3 + 2;
        }

        // Get vertices and transform them
        Vector3 v0 = {vertices[idx0 * 3], vertices[idx0 * 3 + 1], vertices[idx0 * 3 + 2]};
        Vector3 v1 = {vertices[idx1 * 3], vertices[idx1 * 3 + 1], vertices[idx1 * 3 + 2]};
        Vector3 v2 = {vertices[idx2 * 3], vertices[idx2 * 3 + 1], vertices[idx2 * 3 + 2]};

        v0 = Vector3Transform(v0, transform);
        v1 = Vector3Transform(v1, transform);
        v2 = Vector3Transform(v2, transform);

        // Calculate triangle normal
        Vector3 edge1 = Vector3Subtract(v1, v0);
        Vector3 edge2 = Vector3Subtract(v2, v0);
        Vector3 normal = Vector3Normalize(Vector3CrossProduct(edge1, edge2));

        // Only check triangles facing toward the sun (could receive light)
        float facingSun = Vector3DotProduct(normal, sun_dir);
        if (facingSun < 0.1f) continue; // Not facing sun, skip

        // Triangle center
        Vector3 center = {
            (v0.x + v1.x + v2.x) / 3.0f,
            (v0.y + v1.y + v2.y) / 3.0f,
            (v0.z + v1.z + v2.z) / 3.0f
        };

        // Offset slightly along normal to avoid self-intersection
        Vector3 rayStart = Vector3Add(center, Vector3Scale(normal, 0.005f));

        // Cast ray toward sun
        Ray ray = {rayStart, sun_dir};
        RayCollision hit = GetRayCollisionMesh(ray, *mesh, transform);

        // If ray hits something, this triangle is in shadow
        if (hit.hit && hit.distance > 0.01f)
        {
            // Draw shadow overlay on this triangle (slightly offset to avoid z-fighting)
            Vector3 offset = Vector3Scale(normal, 0.002f);
            Vector3 sv0 = Vector3Add(v0, offset);
            Vector3 sv1 = Vector3Add(v1, offset);
            Vector3 sv2 = Vector3Add(v2, offset);

            DrawTriangle3D(sv0, sv1, sv2, shadowOnMesh);
        }
    }
}

void DrawSunIndicator(AppState* app)
{
    if (!app->sim_run || !app->sim_results.is_daytime) return;

    Vector3 center = {
        (app->mesh_bounds.min.x + app->mesh_bounds.max.x) / 2,
        (app->mesh_bounds.min.y + app->mesh_bounds.max.y) / 2,
        (app->mesh_bounds.min.z + app->mesh_bounds.max.z) / 2
    };

    float size = fmaxf(
        app->mesh_bounds.max.x - app->mesh_bounds.min.x,
        app->mesh_bounds.max.z - app->mesh_bounds.min.z
    ) * 0.5f;

    Vector3 sunPos = Vector3Add(center, Vector3Scale(app->sim_results.sun_direction, size * 2.0f));

    // Draw sun sphere
    DrawSphere(sunPos, size * 0.08f, YELLOW);

    // Draw rays from sun to mesh corners
    DrawLine3D(sunPos, center, (Color){255, 255, 0, 150});
}

void AppDraw(AppState* app)
{
    // 3D View
    int viewX = app->sidebar_width;
    int viewW = app->screen_width - app->sidebar_width;
    int viewH = app->screen_height - 30; // Leave room for status bar

    BeginScissorMode(viewX, 0, viewW, viewH);
    BeginMode3D(app->cam.camera);

    // Draw grid
    DrawGrid(20, 0.5f);

    // Draw colored coordinate axes at origin
    float axisLength = 1.0f;
    // X axis - Red
    DrawLine3D((Vector3){0, 0, 0}, (Vector3){axisLength, 0, 0}, RED);
    DrawCylinderEx((Vector3){axisLength, 0, 0}, (Vector3){axisLength + 0.1f, 0, 0}, 0.03f, 0.0f, 8, RED);
    // Y axis - Green (UP in raylib)
    DrawLine3D((Vector3){0, 0, 0}, (Vector3){0, axisLength, 0}, GREEN);
    DrawCylinderEx((Vector3){0, axisLength, 0}, (Vector3){0, axisLength + 0.1f, 0}, 0.03f, 0.0f, 8, GREEN);
    // Z axis - Blue
    DrawLine3D((Vector3){0, 0, 0}, (Vector3){0, 0, axisLength}, BLUE);
    DrawCylinderEx((Vector3){0, 0, axisLength}, (Vector3){0, 0, axisLength + 0.1f}, 0.03f, 0.0f, 8, BLUE);

    // Draw mesh shadow on ground (before mesh so it's behind)
    // DrawMeshShadow(app);

    // Draw mesh
    if (app->mesh_loaded)
    {
        DrawModel(app->vehicle_model, (Vector3){0, 0, 0}, 1.0f, COLOR_MESH);
        DrawModelWires(app->vehicle_model, (Vector3){0, 0, 0}, 1.0f, (Color){100, 100, 100, 50});

        // Draw shadows on the mesh surface (occluded areas)
        // DrawMeshShadowsOnSurface(app);

        // Draw auto-layout surface preview
        DrawAutoLayoutPreview(app);
    }

    // Draw cells
    for (int i = 0; i < app->cell_count; i++)
    {
        DrawCell(app, &app->cells[i]);
    }

    // Draw wiring
    DrawWiring(app);

    // Draw sun indicator
    DrawSunIndicator(app);

    EndMode3D();
    EndScissorMode();

    // Draw coordinate axes legend in bottom-right corner of 3D view
    int legendX = app->screen_width - 90;
    int legendY = viewH - 70;
    DrawRectangle(legendX - 5, legendY - 5, 85, 65, (Color){240, 240, 240, 200});
    DrawRectangleLines(legendX - 5, legendY - 5, 85, 65, DARKGRAY);
    DrawText("Axes:", legendX, legendY, 14, DARKGRAY);
    DrawRectangle(legendX, legendY + 16, 12, 12, RED);
    DrawText("X", legendX + 16, legendY + 15, 14, DARKGRAY);
    DrawRectangle(legendX, legendY + 30, 12, 12, GREEN);
    DrawText("Y (up)", legendX + 16, legendY + 29, 14, DARKGRAY);
    DrawRectangle(legendX, legendY + 44, 12, 12, BLUE);
    DrawText("Z", legendX + 16, legendY + 43, 14, DARKGRAY);

    // Draw GUI
    DrawGUI(app);
}
