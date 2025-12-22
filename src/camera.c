#include "app.h"
#include <math.h>

//------------------------------------------------------------------------------
// Camera Implementation
//------------------------------------------------------------------------------

void CameraInit(CameraController *cam) {
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

void CameraUpdatePosition(CameraController *cam) {
    if (cam->is_orthographic) {
        // Top-down view
        cam->camera.position = (Vector3){
            cam->target.x,
            cam->target.y + cam->distance * 2,
            cam->target.z
        };
        cam->camera.up = (Vector3){0, 0, -1};
        cam->camera.projection = CAMERA_ORTHOGRAPHIC;
        cam->camera.fovy = cam->ortho_scale * 2;
    } else {
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

void CameraUpdate(CameraController *cam, AppState *app) {
    // Only handle camera input when not over GUI
    Vector2 mouse = GetMousePosition();
    if (mouse.x < app->sidebar_width)
        return;

    // Orbit with left mouse drag (perspective mode)
    // Or pan with left mouse drag (orthographic mode)
    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        Vector2 delta = GetMouseDelta();

        if (cam->is_orthographic) {
            // Pan in orthographic mode
            float panSpeed = cam->ortho_scale * 0.003f;
            cam->target.x -= delta.x * panSpeed;
            cam->target.z -= delta.y * panSpeed;
        } else {
            // Orbit in perspective mode
            cam->azimuth -= delta.x * 0.5f;
            cam->elevation = Clampf(cam->elevation + delta.y * 0.5f, -89.0f, 89.0f);
        }
    }

    // Pan with middle mouse drag (both modes)
    if (IsMouseButtonDown(MOUSE_BUTTON_MIDDLE)) {
        Vector2 delta = GetMouseDelta();
        float panSpeed = cam->distance * 0.002f;

        // Pan in camera-relative directions
        Vector3 right = Vector3Normalize(
            Vector3CrossProduct(Vector3Subtract(cam->camera.target, cam->camera.position), cam->camera.up));
        Vector3 up = cam->camera.up;

        cam->target = Vector3Add(cam->target, Vector3Scale(right, -delta.x * panSpeed));
        cam->target = Vector3Add(cam->target, Vector3Scale(up, delta.y * panSpeed));
    }

    // Rotate with right mouse drag (perspective mode)
    if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT) && !cam->is_orthographic) {
        Vector2 delta = GetMouseDelta();
        cam->azimuth += delta.x * 0.5f;
        cam->elevation = Clampf(cam->elevation - delta.y * 0.5f, -89.0f, 89.0f);
    }

    // Zoom with scroll
    float wheel = GetMouseWheelMove();
    if (wheel != 0) {
        if (cam->is_orthographic) {
            cam->ortho_scale *= (1.0f - wheel * 0.1f);
            cam->ortho_scale = Clampf(cam->ortho_scale, 0.1f, 50.0f);
        } else {
            cam->distance *= (1.0f - wheel * 0.1f);
            cam->distance = Clampf(cam->distance, 0.1f, 100.0f);
        }
    }

    // Arrow keys for rotation (skip when editing text fields)
    if (!app->gui_text_editing) {
        float rotSpeed = 2.0f;
        if (IsKeyDown(KEY_LEFT))
            cam->azimuth -= rotSpeed;
        if (IsKeyDown(KEY_RIGHT))
            cam->azimuth += rotSpeed;
        if (IsKeyDown(KEY_UP))
            cam->elevation = Clampf(cam->elevation + rotSpeed, -89.0f, 89.0f);
        if (IsKeyDown(KEY_DOWN))
            cam->elevation = Clampf(cam->elevation - rotSpeed, -89.0f, 89.0f);
    }

    CameraUpdatePosition(cam);
}

void CameraSetOrthographic(CameraController *cam, bool ortho) {
    cam->is_orthographic = ortho;
    CameraUpdatePosition(cam);
}

void CameraReset(CameraController *cam, BoundingBox bounds) {
    cam->azimuth = 45.0f;
    cam->elevation = 30.0f;
    CameraFitToBounds(cam, bounds);
}

void CameraFitToBounds(CameraController *cam, BoundingBox bounds) {
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
