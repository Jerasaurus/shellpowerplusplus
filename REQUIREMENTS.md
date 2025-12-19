# Solar Array Designer - Raylib Version

## Overview
Desktop application for designing and simulating solar arrays on 3D vehicle surfaces.
Built with Raylib (C) for 3D rendering and raygui for UI.

## Dependencies

### Required
- **Raylib 5.0+** - 3D rendering, mesh loading, raycasting
- **raygui** - Immediate mode GUI (included with Raylib)
- **tinyfiledialogs** - Native file dialogs
- **C compiler** - gcc, clang, or MSVC

### Optional
- **assimp** - For better mesh format support (if needed)

## Installation

### macOS (Homebrew)
```bash
brew install raylib
```

### Linux (apt)
```bash
sudo apt install libraylib-dev
```

### Windows
Download from https://github.com/raysan5/raylib/releases

## Project Structure
```
shellpower/
├── src/
│   ├── main.c              # Entry point, main loop
│   ├── app.h               # Application state and types
│   ├── mesh_loader.c/h     # STL/OBJ loading
│   ├── camera.c/h          # Camera controls
│   ├── cells.c/h           # Cell placement and management
│   ├── wiring.c/h          # String wiring system
│   ├── simulation.c/h      # Solar simulation
│   ├── gui.c/h             # raygui interface
│   └── project.c/h         # Save/load projects
├── include/
│   ├── raygui.h            # raygui single header
│   └── tinyfiledialogs.h   # File dialogs
├── assets/
│   └── sample_car.obj      # Sample mesh for testing
├── Makefile
└── README.md
```

## Data Structures

### SolarCell
```c
typedef struct {
    int id;
    Vector3 position;       // World position
    Vector3 normal;         // Surface normal
    int string_id;          // -1 = unwired
    int order_in_string;    // Position in string
    bool has_bypass_diode;
} SolarCell;
```

### CellString
```c
typedef struct {
    int id;
    Color color;
    int cell_ids[MAX_CELLS_PER_STRING];
    int cell_count;
} CellString;
```

### CellPreset
```c
typedef struct {
    const char* name;
    float width;            // meters
    float height;           // meters
    float efficiency;       // 0-1
    float voc;              // Open circuit voltage
    float isc;              // Short circuit current
    float vmp;              // Voltage at max power
    float imp;              // Current at max power
} CellPreset;
```

### AppState
```c
typedef enum {
    MODE_IMPORT,
    MODE_CELL_PLACEMENT,
    MODE_WIRING,
    MODE_SIMULATION
} AppMode;

typedef struct {
    // Mode
    AppMode mode;

    // Mesh
    Model vehicle_model;
    bool mesh_loaded;
    float mesh_scale;

    // Cells
    SolarCell cells[MAX_CELLS];
    int cell_count;
    CellPreset current_preset;

    // Strings
    CellString strings[MAX_STRINGS];
    int string_count;
    int current_string_id;      // -1 = none

    // Camera
    Camera3D camera;
    bool orthographic;

    // Simulation
    SimSettings sim_settings;
    SimResults sim_results;
    Vector3 sun_direction;

    // UI state
    bool show_file_dialog;
    char status_message[256];
} AppState;
```

### SimSettings
```c
typedef struct {
    float latitude;         // degrees
    float longitude;        // degrees
    int year, month, day;
    float hour;             // 0-24 decimal
    float irradiance;       // W/m²
} SimSettings;
```

### SimResults
```c
typedef struct {
    float total_power;
    float shaded_percentage;
    float cell_powers[MAX_CELLS];
    bool cell_shaded[MAX_CELLS];
    float string_powers[MAX_STRINGS];
} SimResults;
```

## Features

### 1. Mesh Import
- Load OBJ files (native Raylib)
- Load STL files (custom loader)
- Scale factor input (default 0.001 for mm→m)
- Auto-center: X/Z at origin, bottom at Y=0
- Display mesh dimensions

### 2. Camera System
**Perspective Mode (Import/Simulation):**
- Orbit around mesh center
- Left drag: Rotate
- Scroll: Zoom
- Middle drag: Pan

**Orthographic Top-Down (Cell Placement/Wiring):**
- Camera above mesh, looking down (-Y)
- Scroll: Zoom
- Middle drag: Pan

**Controls:**
- `T` key: Toggle camera mode
- `R` key: Reset camera

### 3. Cell Placement
- Click mesh surface to place cell
- Raycast to get hit point and normal
- Orient cell quad to surface normal
- Offset slightly above surface (0.002m)
- Prevent overlapping cells (distance check)
- Only place on upward surfaces (normal.y > 0.3)
- Right-click to remove cell

**Visual:**
- Blue quad for unwired cells
- String color for wired cells
- Gray for shaded (in simulation)

### 4. Wiring Mode
- Click unwired cell to add to current string
- Auto-start new string if none active
- Right-click or `E` key to end string
- `Escape` to cancel/unwire current string
- Draw lines connecting cells in order
- Each string gets random saturated color

### 5. Simulation
**Inputs:**
- Latitude/Longitude
- Date (year/month/day)
- Time (hour slider 0-24)
- Irradiance (W/m², default 1000)

**Sun Position:**
- Calculate altitude and azimuth from location/time
- Simplified solar position algorithm (no external deps)
- Display sun direction arrow in 3D view

**Shading Calculation:**
```c
for each cell:
    ray = {cell.position + cell.normal * 0.01, sun_direction}
    if (GetRayCollisionMesh(ray, mesh).hit)
        cell is shaded
```

**Power Calculation:**
```c
if (!shaded) {
    cos_angle = Vector3DotProduct(cell.normal, sun_direction);
    cos_angle = fmaxf(0, cos_angle);
    power = irradiance * cell_area * cos_angle * efficiency;
}
```

**Display:**
- Color cells by power (green=high, red=low, gray=shaded)
- Show total power, shaded percentage
- Show per-string power breakdown

### 6. GUI Layout (raygui)
```
┌─────────────────────────────────────────────────────────┐
│ [Import] [Cells] [Wiring] [Simulate]           [?] [X] │
├────────────────┬────────────────────────────────────────┤
│                │                                        │
│  MESH IMPORT   │                                        │
│  [Load Mesh]   │                                        │
│  Scale: [____] │                                        │
│  Info: ...     │              3D VIEW                   │
│                │                                        │
│  CELL PRESET   │                                        │
│  [▼ Maxeon 3 ] │                                        │
│  125x125mm     │                                        │
│  22.7% eff     │                                        │
│                │                                        │
│  PLACEMENT     │                                        │
│  Cells: 45     │                                        │
│  [Clear All]   │                                        │
│                │                                        │
│  WIRING        │                                        │
│  Strings: 3    │                                        │
│  Current: 2    │                                        │
│  [New] [End]   │                                        │
│                │                                        │
│  SIMULATION    │                                        │
│  Lat: [37.4  ] │                                        │
│  Lon: [-122.2] │                                        │
│  Date: 6/21    │                                        │
│  Hour: [===●=] │                                        │
│  [Run Sim]     │                                        │
│  Power: 342W   │                                        │
│                │                                        │
├────────────────┴────────────────────────────────────────┤
│ Status: Placed cell #45                                 │
└─────────────────────────────────────────────────────────┘
```

### 7. Save/Load Projects
**JSON format:**
```json
{
    "mesh_path": "car.obj",
    "mesh_scale": 0.001,
    "preset": "Maxeon Gen 3",
    "cells": [
        {"id": 0, "pos": [1.2, 0.5, 0.3], "normal": [0, 1, 0], "string": 0, "order": 0}
    ],
    "strings": [
        {"id": 0, "color": [255, 100, 50], "cells": [0, 1, 2]}
    ],
    "sim_settings": {
        "lat": 37.4, "lon": -122.2,
        "month": 6, "day": 21, "hour": 12.0,
        "irradiance": 1000
    }
}
```

## Keyboard Shortcuts
| Key | Action |
|-----|--------|
| 1 | Import mode |
| 2 | Cell placement mode |
| 3 | Wiring mode |
| 4 | Simulation mode |
| T | Toggle camera (ortho/perspective) |
| R | Reset camera |
| N | New string (wiring mode) |
| E | End string (wiring mode) |
| Escape | Cancel current string |
| S | Run simulation |
| Ctrl+S | Save project |
| Ctrl+O | Open project |
| Delete | Remove selected/hovered cell |

## Cell Presets (Built-in)
| Name | Size (mm) | Efficiency | Voc | Isc | Vmp | Imp |
|------|-----------|------------|-----|-----|-----|-----|
| Maxeon Gen 3 | 125x125 | 22.7% | 0.68V | 6.24A | 0.58V | 6.01A |
| Maxeon Gen 5 | 125x125 | 24.0% | 0.70V | 6.50A | 0.60V | 6.20A |
| Generic Silicon | 156x156 | 20.0% | 0.64V | 9.50A | 0.54V | 9.00A |

## Build Commands

### macOS/Linux
```bash
make
./shellpower
```

### Windows (MSVC)
```bash
cl main.c -I./include -lraylib -lopengl32 -lgdi32 -lwinmm
```

## Limits
```c
#define MAX_CELLS 1000
#define MAX_STRINGS 50
#define MAX_CELLS_PER_STRING 100
```

## Future Enhancements
- [ ] Undo/redo system
- [ ] Cell rotation/custom orientation
- [ ] Multi-cell selection and move
- [ ] IV curve simulation
- [ ] Time-lapse simulation (full day)
- [ ] Export to CSV/PDF report
- [ ] Import from solar design files
