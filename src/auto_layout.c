#include "app.h"
#include "auto_layout.h"
#include <stdlib.h>
#include <math.h>
#include <stdio.h>

//------------------------------------------------------------------------------
// Auto-Layout Implementation
//------------------------------------------------------------------------------

void InitAutoLayout(AppState *app) {
    app->auto_layout.target_area = 6.0f;
    app->auto_layout.min_normal_angle = 62.0f;
    app->auto_layout.max_normal_angle = 90.0f;
    app->auto_layout.surface_threshold = 30.0f;
    app->auto_layout.time_samples = 12;
    app->auto_layout.optimize_occlusion = true;
    app->auto_layout.preview_surface = false;
    app->auto_layout.use_height_constraint = true;
    app->auto_layout.auto_detect_height = true;
    app->auto_layout.height_tolerance = 0.3f;
    app->auto_layout.min_height = 0.0f;
    app->auto_layout.max_height = 10.0f;
    app->auto_layout.use_grid_layout = true;
    app->auto_layout.grid_spacing = 0.0f;
    app->auto_layout_running = false;
    app->auto_layout_progress = 0;
}

bool IsPointOnMesh(AppState *app, Vector3 position, float tolerance) {
    if (!app->mesh_loaded)
        return false;

    Ray ray;
    ray.position = (Vector3){position.x, app->mesh_bounds.max.y + 1.0f, position.z};
    ray.direction = (Vector3){0, -1, 0};

    RayCollision hit = GetRayCollisionMesh(ray, app->vehicle_mesh, app->vehicle_model.transform);

    if (!hit.hit)
        return false;

    float heightDiff = fabsf(hit.point.y - position.y);
    return heightDiff < tolerance;
}

bool IsCellFootprintValid(AppState *app, Vector3 position, Vector3 normal, float cellWidth, float cellHeight) {
    if (!app->mesh_loaded)
        return false;

    // Calculate cell corner directions
    Vector3 right;
    Vector3 ref = {0, 0, 1};
    right = Vector3CrossProduct(ref, normal);
    if (Vector3Length(right) < 0.001f) {
        ref = (Vector3){1, 0, 0};
        right = Vector3CrossProduct(ref, normal);
    }
    right = Vector3Normalize(right);

    Vector3 forward = Vector3Normalize(Vector3CrossProduct(normal, right));

    // Scale to half cell size
    Vector3 halfRight = Vector3Scale(right, cellWidth / 2.0f);
    Vector3 halfForward = Vector3Scale(forward, cellHeight / 2.0f);

    // Check corners and edge midpoints (9 points for better coverage)
    Vector3 checkPoints[9];
    checkPoints[0] = position;
    checkPoints[1] = Vector3Add(position, Vector3Add(halfRight, halfForward));
    checkPoints[2] = Vector3Add(position, Vector3Add(Vector3Negate(halfRight), halfForward));
    checkPoints[3] = Vector3Add(position, Vector3Add(halfRight, Vector3Negate(halfForward)));
    checkPoints[4] = Vector3Add(position, Vector3Add(Vector3Negate(halfRight), Vector3Negate(halfForward)));
    checkPoints[5] = Vector3Add(position, halfRight);
    checkPoints[6] = Vector3Add(position, Vector3Negate(halfRight));
    checkPoints[7] = Vector3Add(position, halfForward);
    checkPoints[8] = Vector3Add(position, Vector3Negate(halfForward));

    float tolerance = 0.05f;

    for (int i = 0; i < 9; i++) {
        Vector3 checkPos = checkPoints[i];

        // Check that this point is on the mesh
        Ray rayDown;
        rayDown.position = (Vector3){checkPos.x, app->mesh_bounds.max.y + 1.0f, checkPos.z};
        rayDown.direction = (Vector3){0, -1, 0};

        RayCollision hitDown = GetRayCollisionMesh(rayDown, app->vehicle_mesh, app->vehicle_model.transform);

        if (!hitDown.hit) {
            return false;
        }

        float expectedY = position.y;
        float surfaceY = hitDown.point.y;

        if (fabsf(surfaceY - expectedY) > tolerance * 2.0f) {
            return false;
        }

        float normalDot = Vector3DotProduct(normal, hitDown.normal);
        if (normalDot < 0.5f) {
            return false;
        }

        // Check for mesh geometry above this point
        Ray rayUp;
        rayUp.position = Vector3Add(checkPos, (Vector3){0, 0.01f, 0});
        rayUp.direction = (Vector3){0, 1, 0};

        RayCollision hitUp = GetRayCollisionMesh(rayUp, app->vehicle_mesh, app->vehicle_model.transform);

        float clearance_required = 0.05f;
        if (hitUp.hit && hitUp.distance < clearance_required) {
            return false;
        }
    }

    return true;
}

bool IsValidSurface(AppState *app, Vector3 position, Vector3 normal) {
    float angle_from_vertical = acosf(Clampf(normal.y, -1.0f, 1.0f)) * RAD2DEG;
    float angle_from_horizontal = 90.0f - angle_from_vertical;

    if (angle_from_horizontal < app->auto_layout.min_normal_angle ||
        angle_from_horizontal > app->auto_layout.max_normal_angle) {
        return false;
    }

    if (position.y < 0.01f)
        return false;

    if (app->auto_layout.use_height_constraint) {
        if (position.y < app->auto_layout.min_height || position.y > app->auto_layout.max_height) {
            return false;
        }
    }

    CellPreset *preset = (CellPreset *)&CELL_PRESETS[app->selected_preset];
    if (!IsCellFootprintValid(app, position, normal, preset->width, preset->height)) {
        return false;
    }

    return true;
}

float CalculateOcclusionScore(AppState *app, Vector3 position, Vector3 normal) {
    if (!app->mesh_loaded)
        return 0.0f;

    int occluded_count = 0;
    int total_samples = 0;

    SimSettings original = app->sim_settings;
    int heading_samples = 10;

    for (int heading_idx = 0; heading_idx < heading_samples; heading_idx++) {
        float heading_angle = (360.0f * heading_idx) / heading_samples;
        float heading_rad = heading_angle * DEG2RAD;

        for (int hour_idx = 0; hour_idx < app->auto_layout.time_samples; hour_idx++) {
            float hour = 6.0f + (12.0f * hour_idx / (app->auto_layout.time_samples - 1));
            app->sim_settings.hour = hour;

            float altitude, azimuth;
            Vector3 sun_dir = CalculateSunDirection(&app->sim_settings, &altitude, &azimuth);

            if (altitude <= 0)
                continue;

            total_samples++;

            Vector3 rotated_sun_dir = {
                sun_dir.x * cosf(-heading_rad) - sun_dir.z * sinf(-heading_rad),
                sun_dir.y,
                sun_dir.x * sinf(-heading_rad) + sun_dir.z * cosf(-heading_rad)
            };

            float facing = Vector3DotProduct(normal, rotated_sun_dir);
            if (facing <= 0) {
                occluded_count++;
                continue;
            }

            Ray ray;
            ray.position = Vector3Add(position, Vector3Scale(normal, 0.01f));
            ray.direction = rotated_sun_dir;

            RayCollision hit = GetRayCollisionMesh(ray, app->vehicle_mesh, app->vehicle_model.transform);
            if (hit.hit && hit.distance > 0.02f) {
                occluded_count++;
            }
        }
    }

    app->sim_settings = original;
    return (total_samples > 0) ? (float)occluded_count / total_samples : 1.0f;
}

void AutoDetectHeightRange(AppState *app) {
    if (!app->mesh_loaded)
        return;

    Mesh *mesh = &app->vehicle_mesh;
    Matrix transform = app->vehicle_model.transform;
    float *vertices = mesh->vertices;
    unsigned short *indices = mesh->indices;
    int triangleCount = mesh->triangleCount;

    float tolerance = app->auto_layout.height_tolerance;

    float *heights = (float *)malloc(MAX_HEIGHT_SAMPLES * sizeof(float));
    int heightCount = 0;

    int step = (triangleCount > MAX_HEIGHT_SAMPLES) ? triangleCount / MAX_HEIGHT_SAMPLES : 1;

    for (int i = 0; i < triangleCount && heightCount < MAX_HEIGHT_SAMPLES; i += step) {
        int idx0, idx1, idx2;
        if (indices) {
            idx0 = indices[i * 3 + 0];
            idx1 = indices[i * 3 + 1];
            idx2 = indices[i * 3 + 2];
        } else {
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

        if (normal.y < MIN_UPWARD_NORMAL)
            continue;

        float center_y = (v0.y + v1.y + v2.y) / 3.0f;
        heights[heightCount++] = center_y;
    }

    if (heightCount == 0) {
        free(heights);
        return;
    }

    // Sort heights
    for (int i = 0; i < heightCount - 1; i++) {
        for (int j = i + 1; j < heightCount; j++) {
            if (heights[j] < heights[i]) {
                float temp = heights[i];
                heights[i] = heights[j];
                heights[j] = temp;
            }
        }
    }

    // Sliding window to find best height range
    int bestCount = 0;
    float bestMinY = heights[0];
    float bestMaxY = heights[0] + tolerance;

    for (int i = 0; i < heightCount; i++) {
        float windowMin = heights[i];
        float windowMax = windowMin + tolerance;

        int count = 0;
        for (int j = i; j < heightCount && heights[j] <= windowMax; j++) {
            count++;
        }

        if (count > bestCount) {
            bestCount = count;
            bestMinY = windowMin;
            bestMaxY = windowMax;
        }
    }

    app->auto_layout.min_height = bestMinY;
    app->auto_layout.max_height = bestMaxY;

    free(heights);

    SetStatus(app, "Auto-detected height: %.2f - %.2f m (%d surfaces)", bestMinY, bestMaxY, bestCount);
}

int RunAutoLayout(AppState *app) {
    if (!app->mesh_loaded) {
        SetStatus(app, "No mesh loaded");
        return 0;
    }

    if (app->auto_layout.use_height_constraint && app->auto_layout.auto_detect_height) {
        AutoDetectHeightRange(app);
    }

    app->auto_layout_running = true;
    app->auto_layout_progress = 0;

    CellPreset *preset = (CellPreset *)&CELL_PRESETS[app->selected_preset];
    float cell_area = preset->width * preset->height;
    int target_cells = (int)(app->auto_layout.target_area / cell_area);

    if (target_cells > MAX_CELLS - app->cell_count) {
        target_cells = MAX_CELLS - app->cell_count;
    }

    SetStatus(app, "Auto-layout: finding %d cell positions...", target_cells);

    Mesh *mesh = &app->vehicle_mesh;
    Matrix transform = app->vehicle_model.transform;

    LayoutCandidate *candidates = (LayoutCandidate *)malloc(MAX_CANDIDATES * sizeof(LayoutCandidate));
    int candidate_count = 0;

    float grid_spacing = app->auto_layout.grid_spacing;
    if (grid_spacing <= 0) {
        grid_spacing = fmaxf(preset->width, preset->height) * MIN_CELL_DISTANCE_FACTOR;
    }

    float min_spacing = grid_spacing;

    if (app->auto_layout.use_grid_layout) {
        float minX = app->mesh_bounds.min.x;
        float maxX = app->mesh_bounds.max.x;
        float minZ = app->mesh_bounds.min.z;
        float maxZ = app->mesh_bounds.max.z;

        int gridX = (int)((maxX - minX) / grid_spacing) + 1;
        int gridZ = (int)((maxZ - minZ) / grid_spacing) + 1;
        int totalGridPoints = gridX * gridZ;
        SetStatus(app, "Auto-layout: scanning %dx%d grid...", gridX, gridZ);

        int processed = 0;
        for (int gx = 0; gx < gridX && candidate_count < MAX_CANDIDATES; gx++) {
            for (int gz = 0; gz < gridZ && candidate_count < MAX_CANDIDATES; gz++) {
                float x = minX + gx * grid_spacing;
                float z = minZ + gz * grid_spacing;

                Ray ray;
                ray.position = (Vector3){x, app->mesh_bounds.max.y + 1.0f, z};
                ray.direction = (Vector3){0, -1, 0};

                RayCollision hit = GetRayCollisionMesh(ray, *mesh, transform);
                if (!hit.hit)
                    continue;

                Vector3 position = hit.point;
                Vector3 normal = hit.normal;

                if (!IsValidSurface(app, position, normal))
                    continue;

                bool too_close = false;
                for (int c = 0; c < candidate_count; c++) {
                    if (Vector3Distance(position, candidates[c].position) < min_spacing * 0.9f) {
                        too_close = true;
                        break;
                    }
                }
                if (too_close)
                    continue;

                for (int c = 0; c < app->cell_count; c++) {
                    Vector3 existingPos = CellGetWorldPosition(app, &app->cells[c]);
                    if (Vector3Distance(position, existingPos) < min_spacing) {
                        too_close = true;
                        break;
                    }
                }
                if (too_close)
                    continue;

                candidates[candidate_count].position = position;
                candidates[candidate_count].normal = normal;
                candidates[candidate_count].occlusion_score = 0.0f;
                candidates[candidate_count].valid = true;
                candidate_count++;

                processed++;
                if (processed % 100 == 0) {
                    app->auto_layout_progress = (processed * 30) / totalGridPoints;
                }
            }
        }
    } else {
        float *vertices = mesh->vertices;
        unsigned short *indices = mesh->indices;
        int triangleCount = mesh->triangleCount;

        for (int i = 0; i < triangleCount && candidate_count < MAX_CANDIDATES; i++) {
            int idx0, idx1, idx2;
            if (indices) {
                idx0 = indices[i * 3 + 0];
                idx1 = indices[i * 3 + 1];
                idx2 = indices[i * 3 + 2];
            } else {
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

            if (!IsValidSurface(app, center, normal))
                continue;

            bool too_close = false;
            for (int c = 0; c < candidate_count; c++) {
                if (Vector3Distance(center, candidates[c].position) < min_spacing) {
                    too_close = true;
                    break;
                }
            }
            if (too_close)
                continue;

            for (int c = 0; c < app->cell_count; c++) {
                Vector3 existingPos = CellGetWorldPosition(app, &app->cells[c]);
                if (Vector3Distance(center, existingPos) < min_spacing) {
                    too_close = true;
                    break;
                }
            }
            if (too_close)
                continue;

            candidates[candidate_count].position = center;
            candidates[candidate_count].normal = normal;
            candidates[candidate_count].occlusion_score = 0.0f;
            candidates[candidate_count].valid = true;
            candidate_count++;

            app->auto_layout_progress = (i * 30) / triangleCount;
        }
    }

    SetStatus(app, "Auto-layout: scoring %d candidates...", candidate_count);

    if (app->auto_layout.optimize_occlusion && candidate_count > 0) {
        for (int i = 0; i < candidate_count; i++) {
            candidates[i].occlusion_score = CalculateOcclusionScore(app, candidates[i].position, candidates[i].normal);
            app->auto_layout_progress = 30 + (i * 50) / candidate_count;
        }

        // Sort by occlusion score (lowest first)
        for (int i = 0; i < candidate_count - 1; i++) {
            for (int j = i + 1; j < candidate_count; j++) {
                if (candidates[j].occlusion_score < candidates[i].occlusion_score) {
                    LayoutCandidate temp = candidates[i];
                    candidates[i] = candidates[j];
                    candidates[j] = temp;
                }
            }
        }
    }

    // Place cells at best positions
    int placed = 0;
    for (int i = 0; i < candidate_count && placed < target_cells; i++) {
        if (!candidates[i].valid)
            continue;

        int id = PlaceCell(app, candidates[i].position, candidates[i].normal);
        if (id >= 0) {
            placed++;

            for (int j = i + 1; j < candidate_count; j++) {
                if (Vector3Distance(candidates[i].position, candidates[j].position) < min_spacing) {
                    candidates[j].valid = false;
                }
            }
        }

        if (target_cells > 0) {
            app->auto_layout_progress = 80 + (placed * 20) / target_cells;
        }
    }

    free(candidates);

    app->auto_layout_running = false;
    app->auto_layout_progress = 100;

    SetStatus(app, "Auto-layout: placed %d cells (%.2f m²)", placed, placed * cell_area);

    return placed;
}

void DrawAutoLayoutPreview(AppState *app) {
    if (!app->auto_layout.preview_surface)
        return;
    if (!app->mesh_loaded)
        return;

    Mesh *mesh = &app->vehicle_mesh;
    Matrix transform = app->vehicle_model.transform;
    float *vertices = mesh->vertices;
    unsigned short *indices = mesh->indices;
    int triangleCount = mesh->triangleCount;

    Color validColor = (Color){0, 200, 0, 100};

    int step = (triangleCount > 2000) ? triangleCount / 500 : 1;

    for (int i = 0; i < triangleCount; i += step) {
        int idx0, idx1, idx2;
        if (indices) {
            idx0 = indices[i * 3 + 0];
            idx1 = indices[i * 3 + 1];
            idx2 = indices[i * 3 + 2];
        } else {
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

        Vector3 offset = Vector3Scale(normal, 0.003f);
        Vector3 sv0 = Vector3Add(v0, offset);
        Vector3 sv1 = Vector3Add(v1, offset);
        Vector3 sv2 = Vector3Add(v2, offset);

        if (IsValidSurface(app, center, normal)) {
            DrawTriangle3D(sv0, sv1, sv2, validColor);
        }
    }
}

void DrawHeightBoundsPlanes(AppState *app, int dragging_bound) {
    float minX = app->mesh_bounds.min.x - 0.5f;
    float maxX = app->mesh_bounds.max.x + 0.5f;
    float minZ = app->mesh_bounds.min.z - 0.5f;
    float maxZ = app->mesh_bounds.max.z + 0.5f;

    float minY = app->auto_layout.min_height;
    float maxY = app->auto_layout.max_height;

    Color minColor = (Color){0, 150, 255, 100};
    Color maxColor = (Color){255, 100, 0, 100};
    Color minLineColor = (Color){0, 100, 200, 255};
    Color maxLineColor = (Color){200, 80, 0, 255};

    if (dragging_bound == 1) {
        minColor = (Color){0, 200, 255, 150};
        minLineColor = (Color){0, 255, 255, 255};
    } else if (dragging_bound == 2) {
        maxColor = (Color){255, 150, 0, 150};
        maxLineColor = (Color){255, 200, 0, 255};
    }

    // Draw min height plane
    DrawTriangle3D((Vector3){minX, minY, minZ}, (Vector3){maxX, minY, minZ}, (Vector3){maxX, minY, maxZ}, minColor);
    DrawTriangle3D((Vector3){minX, minY, minZ}, (Vector3){maxX, minY, maxZ}, (Vector3){minX, minY, maxZ}, minColor);

    // Draw max height plane
    DrawTriangle3D((Vector3){minX, maxY, minZ}, (Vector3){maxX, maxY, maxZ}, (Vector3){maxX, maxY, minZ}, maxColor);
    DrawTriangle3D((Vector3){minX, maxY, minZ}, (Vector3){minX, maxY, maxZ}, (Vector3){maxX, maxY, maxZ}, maxColor);

    // Border lines for min plane
    DrawLine3D((Vector3){minX, minY, minZ}, (Vector3){maxX, minY, minZ}, minLineColor);
    DrawLine3D((Vector3){maxX, minY, minZ}, (Vector3){maxX, minY, maxZ}, minLineColor);
    DrawLine3D((Vector3){maxX, minY, maxZ}, (Vector3){minX, minY, maxZ}, minLineColor);
    DrawLine3D((Vector3){minX, minY, maxZ}, (Vector3){minX, minY, minZ}, minLineColor);

    // Border lines for max plane
    DrawLine3D((Vector3){minX, maxY, minZ}, (Vector3){maxX, maxY, minZ}, maxLineColor);
    DrawLine3D((Vector3){maxX, maxY, minZ}, (Vector3){maxX, maxY, maxZ}, maxLineColor);
    DrawLine3D((Vector3){maxX, maxY, maxZ}, (Vector3){minX, maxY, maxZ}, maxLineColor);
    DrawLine3D((Vector3){minX, maxY, maxZ}, (Vector3){minX, maxY, minZ}, maxLineColor);

    // Vertical connecting lines
    Color vertColor = (Color){100, 100, 100, 150};
    DrawLine3D((Vector3){minX, minY, minZ}, (Vector3){minX, maxY, minZ}, vertColor);
    DrawLine3D((Vector3){maxX, minY, minZ}, (Vector3){maxX, maxY, minZ}, vertColor);
    DrawLine3D((Vector3){maxX, minY, maxZ}, (Vector3){maxX, maxY, maxZ}, vertColor);
    DrawLine3D((Vector3){minX, minY, maxZ}, (Vector3){minX, maxY, maxZ}, vertColor);
}

void RunHeightBoundsEditor(AppState *app) {
    if (!app->mesh_loaded) {
        SetStatus(app, "Load a mesh first");
        return;
    }

    Vector3 center = {
        (app->mesh_bounds.min.x + app->mesh_bounds.max.x) / 2,
        (app->mesh_bounds.min.y + app->mesh_bounds.max.y) / 2,
        (app->mesh_bounds.min.z + app->mesh_bounds.max.z) / 2
    };
    Vector3 size = Vector3Subtract(app->mesh_bounds.max, app->mesh_bounds.min);
    float maxDim = fmaxf(fmaxf(size.x, size.y), size.z);

    Camera3D sideCamera = {0};
    sideCamera.position = (Vector3){center.x + maxDim * 2.0f, center.y, center.z};
    sideCamera.target = center;
    sideCamera.up = (Vector3){0, 1, 0};
    sideCamera.fovy = maxDim * 1.2f;
    sideCamera.projection = CAMERA_ORTHOGRAPHIC;

    int dragging_bound = 0;
    bool done = false;

    int viewX = app->sidebar_width;
    int viewW = app->screen_width - app->sidebar_width;
    int viewH = app->screen_height - 30;

    int sliderBarX = app->screen_width - 80;
    int sliderBarY = 80;
    int sliderBarW = 40;
    int sliderBarH = viewH - 160;
    int handleH = 30;

    float meshMinY = app->mesh_bounds.min.y;
    float meshMaxY = app->mesh_bounds.max.y;
    float meshRange = meshMaxY - meshMinY;

    SetStatus(app, "Drag the sliders on the right to adjust height bounds.");

    int panelX = viewX + 20;
    int panelY = 20;
    int panelW = 260;
    int panelH = 120;
    Rectangle doneBtn = {panelX + panelW / 2 - 40, panelY + panelH - 35, 80, 25};

    while (!done && !WindowShouldClose()) {
        Vector2 mouse = GetMousePosition();

        float minSliderY, maxSliderY;
        if (meshRange > 0.001f) {
            minSliderY = sliderBarY + sliderBarH - ((app->auto_layout.min_height - meshMinY) / meshRange) * sliderBarH;
            maxSliderY = sliderBarY + sliderBarH - ((app->auto_layout.max_height - meshMinY) / meshRange) * sliderBarH;
        } else {
            minSliderY = sliderBarY + sliderBarH;
            maxSliderY = sliderBarY;
        }

        float halfH = (float)handleH / 2.0f;
        Rectangle minHandle = {(float)sliderBarX - 10, minSliderY - halfH, (float)sliderBarW + 20, (float)handleH};
        Rectangle maxHandle = {(float)sliderBarX - 10, maxSliderY - halfH, (float)sliderBarW + 20, (float)handleH};

        if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
            dragging_bound = 0;
        }

        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            if (CheckCollisionPointRec(mouse, doneBtn)) {
                done = true;
            } else if (CheckCollisionPointRec(mouse, maxHandle)) {
                dragging_bound = 2;
            } else if (CheckCollisionPointRec(mouse, minHandle)) {
                dragging_bound = 1;
            }
        }

        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && dragging_bound > 0) {
            float t = 1.0f - (mouse.y - (float)sliderBarY) / (float)sliderBarH;
            t = Clampf(t, 0.0f, 1.0f);
            float worldY = meshMinY + t * meshRange;

            if (dragging_bound == 1) {
                app->auto_layout.min_height = Clampf(worldY, meshMinY, app->auto_layout.max_height - 0.01f);
            } else if (dragging_bound == 2) {
                app->auto_layout.max_height = Clampf(worldY, app->auto_layout.min_height + 0.01f, meshMaxY);
            }
        }

        if (IsKeyPressed(KEY_ESCAPE)) {
            done = true;
        }

        BeginDrawing();
        ClearBackground(COLOR_BACKGROUND);

        BeginScissorMode(viewX, 0, viewW - 100, viewH);
        BeginMode3D(sideCamera);

        DrawGrid(20, 0.5f);
        DrawModel(app->vehicle_model, (Vector3){0, 0, 0}, 1.0f, COLOR_MESH);
        DrawHeightBoundsPlanes(app, dragging_bound);

        EndMode3D();
        EndScissorMode();

        // Draw slider UI
        DrawRectangle(sliderBarX - 10, sliderBarY - 20, sliderBarW + 20, sliderBarH + 40, (Color){50, 50, 50, 220});
        DrawRectangleLines(sliderBarX - 10, sliderBarY - 20, sliderBarW + 20, sliderBarH + 40, GRAY);
        DrawRectangle(sliderBarX + sliderBarW / 2 - 3, sliderBarY, 6, sliderBarH, (Color){80, 80, 80, 255});
        DrawRectangle(sliderBarX + sliderBarW / 2 - 8, sliderBarY, 16, sliderBarH, (Color){100, 100, 100, 100});

        float regionTop = maxSliderY;
        float regionBot = minSliderY;
        DrawRectangle(sliderBarX + 5, regionTop, sliderBarW - 10, regionBot - regionTop, (Color){100, 200, 100, 100});

        Color minHandleColor = (dragging_bound == 1) ? (Color){100, 200, 255, 255} : (Color){50, 150, 255, 255};
        DrawRectangleRec(minHandle, minHandleColor);
        DrawRectangleLinesEx(minHandle, 2, (Color){0, 100, 200, 255});
        DrawText("MIN", minHandle.x + 15, minHandle.y + 8, 14, WHITE);

        Color maxHandleColor = (dragging_bound == 2) ? (Color){255, 180, 100, 255} : (Color){255, 120, 50, 255};
        DrawRectangleRec(maxHandle, maxHandleColor);
        DrawRectangleLinesEx(maxHandle, 2, (Color){200, 80, 0, 255});
        DrawText("MAX", maxHandle.x + 15, maxHandle.y + 8, 14, WHITE);

        char minText[32], maxText[32];
        snprintf(minText, sizeof(minText), "%.2fm", app->auto_layout.min_height);
        snprintf(maxText, sizeof(maxText), "%.2fm", app->auto_layout.max_height);
        DrawText(minText, sliderBarX - 5, minSliderY + handleH / 2 + 5, 12, (Color){100, 180, 255, 255});
        DrawText(maxText, sliderBarX - 5, maxSliderY - handleH / 2 - 18, 12, (Color){255, 150, 100, 255});

        DrawRectangle(panelX, panelY, panelW, panelH, (Color){40, 40, 40, 240});
        DrawRectangleLines(panelX, panelY, panelW, panelH, WHITE);
        DrawText("HEIGHT BOUNDS", panelX + 60, panelY + 15, 18, WHITE);
        DrawText("Drag sliders on right", panelX + 20, panelY + 45, 14, LIGHTGRAY);
        DrawText("Press ESC or Done to exit", panelX + 20, panelY + 65, 14, LIGHTGRAY);

        DrawRectangleRec(doneBtn, GREEN);
        DrawRectangleLinesEx(doneBtn, 1, DARKGREEN);
        DrawText("Done", doneBtn.x + 22, doneBtn.y + 5, 16, BLACK);

        DrawRectangle(0, app->screen_height - 25, app->screen_width, 25, (Color){220, 220, 220, 255});
        DrawText(app->status_msg, 10, app->screen_height - 22, 16, DARKGRAY);

        EndDrawing();
    }

    SetStatus(app, "Height bounds set: %.2f - %.2f m", app->auto_layout.min_height, app->auto_layout.max_height);
}
