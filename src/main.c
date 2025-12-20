/*
 * Main entry point
 */

#include "raylib.h"

#define RAYGUI_IMPLEMENTATION
#include "raygui.h"

#include <stdio.h>
#include <stdlib.h>
#include "app.h"

// Global font
static Font appFont = {0};

int main(int argc, char *argv[]) {
    (void) argc;
    (void) argv;

    // Initialize window
    const int screenWidth = 1280;
    const int screenHeight = 800;

    SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_WINDOW_RESIZABLE);
    InitWindow(screenWidth, screenHeight, "Solar Array Designer");
    SetTargetFPS(60);

    // Load custom font
    appFont = LoadFontEx("assets/Inter-Regular.otf", 18, NULL, 256);
    if (appFont.texture.id == 0) {
        // Fallback to default font if custom font not found
        appFont = GetFontDefault();
        TraceLog(LOG_WARNING, "Custom font not found, using default");
    } else {
        // Enable font filtering for smoother text
        SetTextureFilter(appFont.texture, TEXTURE_FILTER_BILINEAR);
    }

    // Set font for raygui
    GuiSetFont(appFont);
    GuiSetStyle(DEFAULT, TEXT_SIZE, 16);
    GuiSetStyle(DEFAULT, TEXT_SPACING, 1);

    // Initialize application state
    AppState app = {0};
    app.screen_width = screenWidth;
    app.screen_height = screenHeight;
    AppInit(&app);

    // Main loop
    while (!WindowShouldClose()) {
        // Handle window resize
        if (IsWindowResized()) {
            app.screen_width = GetScreenWidth();
            app.screen_height = GetScreenHeight();
        }

        // Update
        AppUpdate(&app);

        // Draw
        BeginDrawing();
        ClearBackground(COLOR_BACKGROUND);

        AppDraw(&app);

        EndDrawing();
    }

    // Cleanup
    AppClose(&app);
    UnloadFont(appFont);
    CloseWindow();

    return 0;
}
