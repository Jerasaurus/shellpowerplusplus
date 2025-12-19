/*
 * Solar Array Designer
 * GUI implementation using raygui
 */

#include "app.h"
#include "raygui.h"
#include <stdio.h>
#include <string.h>

// For file dialogs
#ifdef _WIN32
    #include <windows.h>
    #include <commdlg.h>
#else
    #include <stdlib.h>
#endif

//------------------------------------------------------------------------------
// File Dialog (platform-specific)
//------------------------------------------------------------------------------
bool OpenFileDialog(char* outPath, int maxLen, const char* filter)
{
#ifdef _WIN32
    OPENFILENAMEA ofn;
    char szFile[MAX_PATH] = {0};

    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = NULL;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = filter ? filter : "All Files\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

    if (GetOpenFileNameA(&ofn))
    {
        strncpy(outPath, szFile, maxLen - 1);
        return true;
    }
    return false;
#else
    // macOS/Linux: Use zenity or osascript
    #ifdef __APPLE__
        FILE* pipe = popen("osascript -e 'POSIX path of (choose file of type {\"obj\", \"stl\", \"OBJ\", \"STL\"} with prompt \"Select Mesh File\")'", "r");
    #else
        FILE* pipe = popen("zenity --file-selection --file-filter='Mesh files|*.obj *.stl *.OBJ *.STL' 2>/dev/null", "r");
    #endif

    if (pipe)
    {
        if (fgets(outPath, maxLen, pipe))
        {
            // Remove trailing newline
            size_t len = strlen(outPath);
            if (len > 0 && outPath[len-1] == '\n') outPath[len-1] = '\0';
            pclose(pipe);
            return strlen(outPath) > 0;
        }
        pclose(pipe);
    }
    return false;
#endif
}

//------------------------------------------------------------------------------
// GUI Drawing
//------------------------------------------------------------------------------
void DrawGUI(AppState* app)
{
    DrawSidebar(app);
    DrawStatusBar(app);
}

void DrawSidebar(AppState* app)
{
    int sw = app->sidebar_width;
    int sh = app->screen_height - 30;

    // Background
    DrawRectangle(0, 0, sw, sh, COLOR_PANEL);
    DrawLine(sw, 0, sw, sh, DARKGRAY);

    int y = 10;
    int padding = 10;
    int w = sw - 2 * padding;

    // Title
    GuiSetStyle(LABEL, TEXT_ALIGNMENT, TEXT_ALIGN_CENTER);
    GuiLabel((Rectangle){padding, y, w, 25}, "SOLAR ARRAY DESIGNER");
    y += 30;

    GuiLine((Rectangle){padding, y, w, 1}, NULL);
    y += 10;

    // Mode buttons
    GuiSetStyle(LABEL, TEXT_ALIGNMENT, TEXT_ALIGN_LEFT);
    GuiLabel((Rectangle){padding, y, w, 20}, "Mode:");
    y += 22;

    int bw = (w - 6) / 4;
    if (GuiButton((Rectangle){padding, y, bw, 25}, app->mode == MODE_IMPORT ? "#12#Import" : "Import"))
        app->mode = MODE_IMPORT;
    if (GuiButton((Rectangle){padding + bw + 2, y, bw, 25}, app->mode == MODE_CELL_PLACEMENT ? "#12#Cells" : "Cells"))
        app->mode = MODE_CELL_PLACEMENT;
    if (GuiButton((Rectangle){padding + 2*(bw + 2), y, bw, 25}, app->mode == MODE_WIRING ? "#12#Wire" : "Wire"))
        app->mode = MODE_WIRING;
    if (GuiButton((Rectangle){padding + 3*(bw + 2), y, bw, 25}, app->mode == MODE_SIMULATION ? "#12#Sim" : "Sim"))
        app->mode = MODE_SIMULATION;
    y += 35;

    GuiLine((Rectangle){padding, y, w, 1}, NULL);
    y += 10;

    // Mode-specific panels
    switch (app->mode)
    {
        case MODE_IMPORT:
            y = DrawImportPanel(app, padding, y, w);
            break;
        case MODE_CELL_PLACEMENT:
            y = DrawCellPanel(app, padding, y, w);
            break;
        case MODE_WIRING:
            y = DrawWiringPanel(app, padding, y, w);
            break;
        case MODE_SIMULATION:
            y = DrawSimulationPanel(app, padding, y, w);
            break;
    }

    // Always show cell preset selector
    y += 10;
    GuiLine((Rectangle){padding, y, w, 1}, NULL);
    y += 10;

    GuiLabel((Rectangle){padding, y, w, 20}, "Cell Preset:");
    y += 22;

    // Preset dropdown
    static bool presetDropdown = false;
    static int presetActive = 0;

    if (GuiDropdownBox(
        (Rectangle){padding, y, w, 25},
        "Maxeon Gen 3;Maxeon Gen 5;Generic Silicon",
        &presetActive,
        presetDropdown))
    {
        presetDropdown = !presetDropdown;
        app->selected_preset = presetActive;
    }
    y += 30;

    // Show preset info
    CellPreset* preset = (CellPreset*)&CELL_PRESETS[app->selected_preset];
    char presetInfo[128];
    snprintf(presetInfo, sizeof(presetInfo),
        "%.0fx%.0fmm, %.1f%% eff\nVmp: %.2fV, Imp: %.2fA",
        preset->width * 1000, preset->height * 1000,
        preset->efficiency * 100,
        preset->vmp, preset->imp);
    GuiLabel((Rectangle){padding, y, w, 40}, presetInfo);
    y += 45;

    // Camera info
    GuiLine((Rectangle){padding, y, w, 1}, NULL);
    y += 10;

    GuiLabel((Rectangle){padding, y, w, 20}, "Camera:");
    y += 22;

    bool ortho = app->cam.is_orthographic;
    if (GuiCheckBox((Rectangle){padding, y, 20, 20}, "Top-Down View", &ortho))
    {
        CameraSetOrthographic(&app->cam, ortho);
    }
    y += 25;

    if (GuiButton((Rectangle){padding, y, w, 25}, "Reset Camera (R)"))
    {
        CameraReset(&app->cam, app->mesh_bounds);
    }
    y += 30;

    // Help text
    GuiLabel((Rectangle){padding, y, w, 60},
        "Drag: Rotate\nScroll: Zoom\nMiddle: Pan");
}

int DrawImportPanel(AppState* app, int x, int y, int w)
{
    GuiLabel((Rectangle){x, y, w, 20}, "MESH IMPORT");
    y += 25;

    if (GuiButton((Rectangle){x, y, w, 30}, "#05#Load Mesh File..."))
    {
        char path[MAX_PATH_LENGTH] = {0};
        if (OpenFileDialog(path, MAX_PATH_LENGTH, NULL))
        {
            LoadVehicleMesh(app, path);
        }
    }
    y += 35;

    // Scale input
    GuiLabel((Rectangle){x, y, 50, 20}, "Scale:");
    static char scaleText[16] = "0.001";
    static bool scaleEditMode = false;
    static float lastScale = 0.001f;

    // Only update text from app state when not editing and scale changed externally
    if (!scaleEditMode && app->mesh_scale != lastScale)
    {
        snprintf(scaleText, sizeof(scaleText), "%.6f", app->mesh_scale);
        lastScale = app->mesh_scale;
    }

    if (GuiTextBox((Rectangle){x + 55, y, w - 55, 25}, scaleText, 16, scaleEditMode))
    {
        scaleEditMode = !scaleEditMode;
        // When exiting edit mode, apply the new scale
        if (!scaleEditMode)
        {
            float newScale = atof(scaleText);
            if (newScale > 0)
            {
                app->mesh_scale = newScale;
                lastScale = newScale;
                if (app->mesh_loaded)
                {
                    UpdateMeshTransform(app);
                    CameraFitToBounds(&app->cam, app->mesh_bounds);
                }
            }
            else
            {
                // Invalid input, restore previous value
                snprintf(scaleText, sizeof(scaleText), "%.6f", app->mesh_scale);
            }
        }
    }
    y += 28;

    GuiLabel((Rectangle){x, y, w, 20}, "(0.001 = mm to meters)");
    y += 25;

    // Rotation controls
    if (app->mesh_loaded)
    {
        GuiLabel((Rectangle){x, y, w, 20}, "Rotation (degrees):");
        y += 22;

        bool transformChanged = false;
        int btnW = 35;
        int sliderW = w - 90;

        // X rotation
        GuiLabel((Rectangle){x, y, 15, 20}, "X:");
        if (GuiButton((Rectangle){x + 18, y, btnW, 20}, "-90"))
        {
            app->mesh_rotation.x -= 90;
            transformChanged = true;
        }
        float rotX = app->mesh_rotation.x;
        if (GuiSlider((Rectangle){x + 18 + btnW + 2, y, sliderW, 20}, NULL, NULL, &rotX, -180, 180))
        {
            app->mesh_rotation.x = rotX;
            transformChanged = true;
        }
        if (GuiButton((Rectangle){x + 18 + btnW + 4 + sliderW, y, btnW, 20}, "+90"))
        {
            app->mesh_rotation.x += 90;
            transformChanged = true;
        }
        char rotXText[16];
        snprintf(rotXText, sizeof(rotXText), "%.0f", app->mesh_rotation.x);
        GuiLabel((Rectangle){x + w - 30, y, 30, 20}, rotXText);
        y += 24;

        // Y rotation
        GuiLabel((Rectangle){x, y, 15, 20}, "Y:");
        if (GuiButton((Rectangle){x + 18, y, btnW, 20}, "-90"))
        {
            app->mesh_rotation.y -= 90;
            transformChanged = true;
        }
        float rotY = app->mesh_rotation.y;
        if (GuiSlider((Rectangle){x + 18 + btnW + 2, y, sliderW, 20}, NULL, NULL, &rotY, -180, 180))
        {
            app->mesh_rotation.y = rotY;
            transformChanged = true;
        }
        if (GuiButton((Rectangle){x + 18 + btnW + 4 + sliderW, y, btnW, 20}, "+90"))
        {
            app->mesh_rotation.y += 90;
            transformChanged = true;
        }
        char rotYText[16];
        snprintf(rotYText, sizeof(rotYText), "%.0f", app->mesh_rotation.y);
        GuiLabel((Rectangle){x + w - 30, y, 30, 20}, rotYText);
        y += 24;

        // Z rotation
        GuiLabel((Rectangle){x, y, 15, 20}, "Z:");
        if (GuiButton((Rectangle){x + 18, y, btnW, 20}, "-90"))
        {
            app->mesh_rotation.z -= 90;
            transformChanged = true;
        }
        float rotZ = app->mesh_rotation.z;
        if (GuiSlider((Rectangle){x + 18 + btnW + 2, y, sliderW, 20}, NULL, NULL, &rotZ, -180, 180))
        {
            app->mesh_rotation.z = rotZ;
            transformChanged = true;
        }
        if (GuiButton((Rectangle){x + 18 + btnW + 4 + sliderW, y, btnW, 20}, "+90"))
        {
            app->mesh_rotation.z += 90;
            transformChanged = true;
        }
        char rotZText[16];
        snprintf(rotZText, sizeof(rotZText), "%.0f", app->mesh_rotation.z);
        GuiLabel((Rectangle){x + w - 30, y, 30, 20}, rotZText);
        y += 27;

        // Reset rotation button
        if (GuiButton((Rectangle){x, y, w, 22}, "Reset Rotation"))
        {
            app->mesh_rotation = (Vector3){0, 0, 0};
            transformChanged = true;
        }
        y += 27;

        if (transformChanged)
        {
            UpdateMeshTransform(app);
        }
    }

    // Mesh info
    if (app->mesh_loaded)
    {
        char info[256];
        Vector3 size = Vector3Subtract(app->mesh_bounds.max, app->mesh_bounds.min);
        snprintf(info, sizeof(info),
            "Mesh: %s\nSize: %.2f x %.2f x %.2f m\nCells: %d",
            GetFileName(app->mesh_path),
            size.x, size.y, size.z,
            app->cell_count);
        GuiLabel((Rectangle){x, y, w, 60}, info);
        y += 65;
    }
    else
    {
        GuiLabel((Rectangle){x, y, w, 20}, "No mesh loaded");
        y += 25;
    }

    return y;
}

int DrawCellPanel(AppState* app, int x, int y, int w)
{
    GuiLabel((Rectangle){x, y, w, 20}, "CELL PLACEMENT");
    y += 25;

    // Mode toggle: single cell vs module placement
    if (app->placing_module && app->selected_module >= 0)
    {
        GuiLabel((Rectangle){x, y, w, 20}, "Placing module:");
        y += 20;
        GuiLabel((Rectangle){x, y, w, 20}, app->modules[app->selected_module].name);
        y += 22;

        if (GuiButton((Rectangle){x, y, w, 25}, "Cancel Module Placement"))
        {
            app->placing_module = false;
        }
        y += 30;
    }
    else
    {
        GuiLabel((Rectangle){x, y, w, 40}, "Click on mesh to place\nRight-click to remove");
        y += 45;
    }

    char cellInfo[64];
    snprintf(cellInfo, sizeof(cellInfo), "Cells placed: %d", app->cell_count);
    GuiLabel((Rectangle){x, y, w, 20}, cellInfo);
    y += 25;

    if (GuiButton((Rectangle){x, y, w, 25}, "Clear All Cells"))
    {
        ClearAllCells(app);
    }
    y += 35;

    // Module section
    GuiLine((Rectangle){x, y, w, 1}, NULL);
    y += 10;

    GuiLabel((Rectangle){x, y, w, 20}, "MODULES");
    y += 25;

    // Create module from current cells
    static char moduleNameText[MAX_MODULE_NAME] = "Module1";
    static bool moduleNameEdit = false;

    GuiLabel((Rectangle){x, y, w, 20}, "Create from cells:");
    y += 22;

    if (GuiTextBox((Rectangle){x, y, w - 60, 22}, moduleNameText, MAX_MODULE_NAME, moduleNameEdit))
    {
        moduleNameEdit = !moduleNameEdit;
    }

    if (GuiButton((Rectangle){x + w - 55, y, 55, 22}, "Create"))
    {
        if (strlen(moduleNameText) > 0 && app->cell_count > 0)
        {
            CreateModuleFromCells(app, moduleNameText);
            // Generate next module name
            static int moduleNum = 2;
            snprintf(moduleNameText, sizeof(moduleNameText), "Module%d", moduleNum++);
        }
    }
    y += 27;

    // List saved modules
    if (app->module_count > 0)
    {
        GuiLabel((Rectangle){x, y, w, 20}, "Saved modules:");
        y += 22;

        // Simple list (show up to 5 modules)
        for (int i = 0; i < app->module_count && i < 5; i++)
        {
            CellModule* mod = &app->modules[i];
            char modLabel[128];
            snprintf(modLabel, sizeof(modLabel), "%s (%d cells)", mod->name, mod->cell_count);

            bool selected = (app->selected_module == i);
            if (GuiToggle((Rectangle){x, y, w - 30, 20}, modLabel, &selected))
            {
                app->selected_module = selected ? i : -1;
            }

            // Delete button
            if (GuiButton((Rectangle){x + w - 25, y, 25, 20}, "X"))
            {
                DeleteModule(app, i);
                if (app->selected_module == i) app->selected_module = -1;
            }
            y += 22;
        }

        if (app->module_count > 5)
        {
            char moreText[32];
            snprintf(moreText, sizeof(moreText), "...and %d more", app->module_count - 5);
            GuiLabel((Rectangle){x, y, w, 20}, moreText);
            y += 22;
        }

        // Place selected module button
        if (app->selected_module >= 0)
        {
            if (GuiButton((Rectangle){x, y, w, 25}, "Place Selected Module"))
            {
                app->placing_module = true;
            }
            y += 28;
        }
    }
    else
    {
        GuiLabel((Rectangle){x, y, w, 20}, "No saved modules");
        y += 22;
    }

    // Reload modules button
    if (GuiButton((Rectangle){x, y, w, 22}, "Reload Modules"))
    {
        LoadAllModules(app);
    }
    y += 27;

    // Auto-layout section
    GuiLine((Rectangle){x, y, w, 1}, NULL);
    y += 10;

    GuiLabel((Rectangle){x, y, w, 20}, "AUTO-LAYOUT");
    y += 25;

    // Target area
    GuiLabel((Rectangle){x, y, 80, 20}, "Target area:");
    static char areaText[16] = "1.0";
    static bool areaEditMode = false;
    static float lastArea = 1.0f;

    if (!areaEditMode && app->auto_layout.target_area != lastArea)
    {
        snprintf(areaText, sizeof(areaText), "%.2f", app->auto_layout.target_area);
        lastArea = app->auto_layout.target_area;
    }

    if (GuiTextBox((Rectangle){x + 85, y, 60, 20}, areaText, 16, areaEditMode))
    {
        areaEditMode = !areaEditMode;
        if (!areaEditMode)
        {
            float newArea = atof(areaText);
            if (newArea > 0)
            {
                app->auto_layout.target_area = newArea;
                lastArea = newArea;
            }
            else
            {
                snprintf(areaText, sizeof(areaText), "%.2f", app->auto_layout.target_area);
            }
        }
    }
    GuiLabel((Rectangle){x + 150, y, 30, 20}, "m2");
    y += 24;

    // Surface angle constraints
    GuiLabel((Rectangle){x, y, w, 20}, "Surface angle (from horizontal):");
    y += 20;

    GuiLabel((Rectangle){x, y, 30, 20}, "Min:");
    GuiSlider((Rectangle){x + 35, y, w - 80, 20}, NULL, NULL, &app->auto_layout.min_normal_angle, 0, 90);
    char minAngleText[16];
    snprintf(minAngleText, sizeof(minAngleText), "%.0f", app->auto_layout.min_normal_angle);
    GuiLabel((Rectangle){x + w - 40, y, 40, 20}, minAngleText);
    y += 22;

    GuiLabel((Rectangle){x, y, 30, 20}, "Max:");
    GuiSlider((Rectangle){x + 35, y, w - 80, 20}, NULL, NULL, &app->auto_layout.max_normal_angle, 0, 90);
    char maxAngleText[16];
    snprintf(maxAngleText, sizeof(maxAngleText), "%.0f", app->auto_layout.max_normal_angle);
    GuiLabel((Rectangle){x + w - 40, y, 40, 20}, maxAngleText);
    y += 24;

    // Optimization toggle
    GuiCheckBox((Rectangle){x, y, 20, 20}, "Optimize for min occlusion", &app->auto_layout.optimize_occlusion);
    y += 24;

    // Preview surface toggle
    GuiCheckBox((Rectangle){x, y, 20, 20}, "Preview valid surfaces", &app->auto_layout.preview_surface);
    y += 24;

    // Grid layout toggle
    GuiCheckBox((Rectangle){x, y, 20, 20}, "Use grid layout", &app->auto_layout.use_grid_layout);
    y += 24;

    // Height constraint section
    GuiCheckBox((Rectangle){x, y, 20, 20}, "Limit height (exclude canopy)", &app->auto_layout.use_height_constraint);
    y += 22;

    if (app->auto_layout.use_height_constraint)
    {
        // Auto-detect checkbox
        GuiCheckBox((Rectangle){x, y, 20, 20}, "Auto-detect shell top", &app->auto_layout.auto_detect_height);
        y += 22;

        if (!app->auto_layout.auto_detect_height)
        {
            // Manual height range controls
            float z_min_bound = app->mesh_loaded ? app->mesh_bounds.min.z - 0.1f : 0.0f;
            float z_max_bound = app->mesh_loaded ? app->mesh_bounds.max.z + 0.1f : 10.0f;

            GuiLabel((Rectangle){x, y, 60, 20}, "Min height:");
            GuiSlider((Rectangle){x + 65, y, w - 110, 20}, NULL, NULL,
                &app->auto_layout.min_height, z_min_bound, z_max_bound);
            char minHText[16];
            snprintf(minHText, sizeof(minHText), "%.2f", app->auto_layout.min_height);
            GuiLabel((Rectangle){x + w - 40, y, 40, 20}, minHText);
            y += 22;

            GuiLabel((Rectangle){x, y, 60, 20}, "Max height:");
            GuiSlider((Rectangle){x + 65, y, w - 110, 20}, NULL, NULL,
                &app->auto_layout.max_height, z_min_bound, z_max_bound);
            char maxHText[16];
            snprintf(maxHText, sizeof(maxHText), "%.2f", app->auto_layout.max_height);
            GuiLabel((Rectangle){x + w - 40, y, 40, 20}, maxHText);
            y += 24;
        }
        else
        {
            // Show current detected range (read-only)
            char rangeText[64];
            snprintf(rangeText, sizeof(rangeText), "Range: %.2f - %.2f m",
                app->auto_layout.min_height, app->auto_layout.max_height);
            GuiLabel((Rectangle){x, y, w, 20}, rangeText);
            y += 24;
        }
    }

    y += 4;

    // Run auto-layout button
    if (app->auto_layout_running)
    {
        // Progress bar
        float progress = (float)app->auto_layout_progress;
        GuiProgressBar((Rectangle){x, y, w, 25}, NULL, NULL, &progress, 0, 100);
        y += 28;
    }
    else
    {
        if (GuiButton((Rectangle){x, y, w, 25}, "Run Auto-Layout"))
        {
            RunAutoLayout(app);
        }
        y += 28;
    }

    return y;
}

int DrawWiringPanel(AppState* app, int x, int y, int w)
{
    GuiLabel((Rectangle){x, y, w, 20}, "WIRING");
    y += 25;

    GuiLabel((Rectangle){x, y, w, 40}, "Click cells to add to string\nRight-click to end string");
    y += 45;

    char stringInfo[64];
    snprintf(stringInfo, sizeof(stringInfo), "Strings: %d", app->string_count);
    GuiLabel((Rectangle){x, y, w, 20}, stringInfo);
    y += 22;

    if (app->active_string_id >= 0)
    {
        // Find active string cell count
        int cellCount = 0;
        for (int s = 0; s < app->string_count; s++)
        {
            if (app->strings[s].id == app->active_string_id)
            {
                cellCount = app->strings[s].cell_count;
                break;
            }
        }
        snprintf(stringInfo, sizeof(stringInfo), "Current: #%d (%d cells)", app->active_string_id, cellCount);
    }
    else
    {
        snprintf(stringInfo, sizeof(stringInfo), "Current: None");
    }
    GuiLabel((Rectangle){x, y, w, 20}, stringInfo);
    y += 25;

    int bw = (w - 4) / 2;
    if (GuiButton((Rectangle){x, y, bw, 25}, "New (N)"))
    {
        StartNewString(app);
    }
    if (GuiButton((Rectangle){x + bw + 4, y, bw, 25}, "End (E)"))
    {
        EndCurrentString(app);
    }
    y += 30;

    if (GuiButton((Rectangle){x, y, w, 25}, "Clear All Wiring"))
    {
        ClearAllWiring(app);
    }
    y += 30;

    return y;
}

int DrawSimulationPanel(AppState* app, int x, int y, int w)
{
    GuiLabel((Rectangle){x, y, w, 20}, "SIMULATION");
    y += 25;

    // Location - text fields for precise input
    GuiLabel((Rectangle){x, y, 60, 20}, "Latitude:");
    static char latText[16] = "37.4";
    static bool latEditMode = false;
    static float lastLat = 37.4f;

    if (!latEditMode && app->sim_settings.latitude != lastLat)
    {
        snprintf(latText, sizeof(latText), "%.2f", app->sim_settings.latitude);
        lastLat = app->sim_settings.latitude;
    }

    if (GuiTextBox((Rectangle){x + 65, y, w - 65, 20}, latText, 16, latEditMode))
    {
        latEditMode = !latEditMode;
        if (!latEditMode)
        {
            float newLat = atof(latText);
            if (newLat >= -90 && newLat <= 90)
            {
                app->sim_settings.latitude = newLat;
                lastLat = newLat;
            }
            else
            {
                snprintf(latText, sizeof(latText), "%.2f", app->sim_settings.latitude);
            }
        }
    }
    y += 24;

    GuiLabel((Rectangle){x, y, 60, 20}, "Longitude:");
    static char lonText[16] = "-122.2";
    static bool lonEditMode = false;
    static float lastLon = -122.2f;

    if (!lonEditMode && app->sim_settings.longitude != lastLon)
    {
        snprintf(lonText, sizeof(lonText), "%.2f", app->sim_settings.longitude);
        lastLon = app->sim_settings.longitude;
    }

    if (GuiTextBox((Rectangle){x + 65, y, w - 65, 20}, lonText, 16, lonEditMode))
    {
        lonEditMode = !lonEditMode;
        if (!lonEditMode)
        {
            float newLon = atof(lonText);
            if (newLon >= -180 && newLon <= 180)
            {
                app->sim_settings.longitude = newLon;
                lastLon = newLon;
            }
            else
            {
                snprintf(lonText, sizeof(lonText), "%.2f", app->sim_settings.longitude);
            }
        }
    }
    y += 27;

    // Date
    GuiLabel((Rectangle){x, y, 50, 20}, "Month:");
    GuiSpinner((Rectangle){x + 55, y, 50, 20}, NULL, &app->sim_settings.month, 1, 12, false);
    GuiLabel((Rectangle){x + 115, y, 30, 20}, "Day:");
    GuiSpinner((Rectangle){x + 150, y, 50, 20}, NULL, &app->sim_settings.day, 1, 31, false);
    y += 25;

    // Time slider - auto-run simulation when changed
    GuiLabel((Rectangle){x, y, 50, 20}, "Hour:");
    static float lastHour = 12.0f;
    GuiSlider((Rectangle){x + 55, y, w - 90, 20}, "0", "24", &app->sim_settings.hour, 0, 24);
    char hourText[8];
    snprintf(hourText, sizeof(hourText), "%.1f", app->sim_settings.hour);
    GuiLabel((Rectangle){x + w - 30, y, 30, 20}, hourText);

    // Auto-run simulation when time changes
    if (app->sim_settings.hour != lastHour && app->cell_count > 0)
    {
        RunSimulation(app);
        lastHour = app->sim_settings.hour;
    }
    y += 25;

    // Irradiance
    GuiLabel((Rectangle){x, y, 70, 20}, "Irradiance:");
    static char irrText[16] = "1000";
    static bool irrEditMode = false;
    static float lastIrr = 1000.0f;

    if (!irrEditMode && app->sim_settings.irradiance != lastIrr)
    {
        snprintf(irrText, sizeof(irrText), "%.0f", app->sim_settings.irradiance);
        lastIrr = app->sim_settings.irradiance;
    }

    if (GuiTextBox((Rectangle){x + 75, y, 60, 20}, irrText, 16, irrEditMode))
    {
        irrEditMode = !irrEditMode;
        if (!irrEditMode)
        {
            float newIrr = atof(irrText);
            if (newIrr >= 0)
            {
                app->sim_settings.irradiance = newIrr;
                lastIrr = newIrr;
            }
            else
            {
                snprintf(irrText, sizeof(irrText), "%.0f", app->sim_settings.irradiance);
            }
        }
    }
    GuiLabel((Rectangle){x + 140, y, 50, 20}, "W/m2");
    y += 30;

    // Run button
    if (GuiButton((Rectangle){x, y, w, 30}, "#04#Run Simulation (S)"))
    {
        RunSimulation(app);
    }
    y += 35;

    // Results
    if (app->sim_run)
    {
        GuiLine((Rectangle){x, y, w, 1}, NULL);
        y += 10;

        char results[256];
        snprintf(results, sizeof(results),
            "RESULTS\n"
            "Total Power: %.1f W\n"
            "Shaded: %.1f%% (%d cells)\n"
            "Sun Alt: %.1f° Az: %.1f°",
            app->sim_results.total_power,
            app->sim_results.shaded_percentage,
            app->sim_results.shaded_count,
            app->sim_results.sun_altitude,
            app->sim_results.sun_azimuth);

        GuiLabel((Rectangle){x, y, w, 80}, results);
        y += 85;

        // Per-string breakdown
        if (app->string_count > 0)
        {
            GuiLabel((Rectangle){x, y, w, 20}, "String Power:");
            y += 20;

            for (int s = 0; s < app->string_count && s < 5; s++)
            {
                char strPower[32];
                snprintf(strPower, sizeof(strPower), "#%d: %.1fW (%d cells)",
                    app->strings[s].id,
                    app->strings[s].total_power,
                    app->strings[s].cell_count);
                GuiLabel((Rectangle){x + 10, y, w - 10, 18}, strPower);
                y += 18;
            }
        }
    }
    else
    {
        GuiLabel((Rectangle){x, y, w, 20}, "Click 'Run Simulation'");
        y += 25;
    }

    return y;
}

void DrawStatusBar(AppState* app)
{
    int y = app->screen_height - 25;

    DrawRectangle(0, y, app->screen_width, 25, (Color){220, 220, 220, 255});
    DrawLine(0, y, app->screen_width, y, DARKGRAY);

    // Status message
    GuiLabel((Rectangle){10, y + 3, app->screen_width - 200, 20}, app->status_msg);

    // Mode indicator
    const char* modeNames[] = {"IMPORT", "CELLS", "WIRING", "SIMULATE"};
    char modeText[32];
    snprintf(modeText, sizeof(modeText), "Mode: %s", modeNames[app->mode]);
    GuiLabel((Rectangle){app->screen_width - 150, y + 3, 140, 20}, modeText);
}
