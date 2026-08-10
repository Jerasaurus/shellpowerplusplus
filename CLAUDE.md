# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

Shellpower++ is a C99 cross-platform desktop app for designing and simulating solar arrays on solar-car body meshes. C# port of the original [sscp/shellpower](https://github.com/sscp/shellpower), with added auto-layout. Built on raylib 5.5 + raygui (single-header) and libcurl.

## Build & Run

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/shellpower            # macOS/Linux
build\Release\shellpower.exe  # Windows
```

- raylib and curl are fetched via `FetchContent` (tag `5.5` and `curl-8_5_0`) if not found on the system. First build downloads them — expect a slow initial configure.
- Version is derived from `git describe --tags --abbrev=0` and substituted into `src/version.h.in` → `build/generated/version.h`. The auto-updater uses this. Building from a non-tagged commit yields `v0.0.0`.
- `assets/` is copied next to the binary post-build (Inter font is also embedded directly via `src/inter_font.h`).
- No tests, no linter target, no formatter target. `.clang-format` is checked in (LLVM base, 4-space indent, 120-col limit) — run `clang-format -i src/**/*.c src/**/*.h` manually if formatting matters.

## Releases

Tags matching `v*` trigger `.github/workflows/workflow.yml`, which builds Linux x64, macOS arm64, macOS x64 (on `macos-15`), and Windows x64, then attaches binaries plus a `SHA256SUMS.txt` to the release. The in-app updater (`src/updater.c`) polls GitHub releases for the owner/repo parsed from `git remote get-url origin` at build time.

## Architecture

This is a single-binary, single-`AppState` application. All state lives in `AppState` (`src/app.h`), passed by pointer everywhere. There is no separate document/model layer — the struct *is* the document.

```
main.c → AppInit → loop { AppUpdate; AppDraw } → AppClose
                       │           │
                       │           ├── DrawGUI (gui.c)         sidebar tabs
                       │           ├── 3D viewport (raylib)    mesh + cells + wires
                       │           └── status bar
                       │
                       ├── input handling per AppMode
                       ├── auto-layout job (auto_layout.c)
                       └── simulation (simulation/*.c)
```

### Modes (`AppMode` in app.h)

The sidebar swaps panels based on `app->mode`: `Import` → `Cells` → `Wire` → `Sim`. Each panel is a `Draw*Panel` function in `gui.c`. The 3D viewport's input handling in `app.c` also branches on mode (e.g. left-click in `MODE_CELL_PLACEMENT` places a cell; in `MODE_WIRING` it adds the cell to the active string).

### Coordinate system

Cells store `local_position`/`local_normal` in **mesh-local** coordinates (pre-transform). World coordinates are derived on the fly via `CellGetWorldPosition` / `CellGetWorldNormal`, so the user can rescale or rotate the imported mesh and existing cells follow correctly. Anywhere you need world positions for raycasting/rendering/simulation, call those helpers — never assume `local_position` is the world position.

### Simulation (`src/simulation/`)

The simulation is a real single-diode IV-curve model, not a cosine-only approximation:

1. `IVTrace_CreateCellTrace` builds a per-cell IV curve scaled by `irradiance × cos(angle to sun)` and modulated by ray-cast shading.
2. `StringSim_CalcStringIV` (or `StringSim_CalcStringIVSegments` for bypass diode segments) sums per-cell voltages at each current sample to get the string IV curve, then sweeps for MPP.
3. Bypass diodes operate on **segments** (a contiguous range of cells in a string). Multiple segments per string are allowed and may nest — the smallest segment covering a shaded cell is the one that activates. See `BypassDiode` in `app.h` and `SegmentBypass` in `string_sim.h`.

`RunStaticSimulation` is the instant-time path; `RunTimeSimulationAnimated` integrates over a day using `time_samples × heading_samples` from the auto-layout/sim settings.

### Auto-layout (`auto_layout.c`)

Two layout strategies, selected by `auto_layout.use_grid_layout`:
- **Grid**: project a grid down onto the mesh, place a cell at each grid point that hits a valid surface.
- **Mesh-based**: walk mesh triangles whose normals are within `[min_normal_angle, max_normal_angle]` of vertical *and* whose neighbors are within `surface_threshold`.

Both go through `IsValidSurface` which also checks the optional `min_height`/`max_height` band (used to exclude the canopy). `optimize_occlusion` runs a multi-time-sample raycast occlusion score per candidate and prefers less-shaded ones.

### Modules (`modules/*.json`)

A "module" is a saved cell pattern (positions + normals relative to a placement origin). Used to stamp a designed sub-array onto the mesh repeatedly. Stored as JSON in `modules/` (gitignored). Loaded at startup via `LoadAllModules`.

## Key constants (`app.h`)

```
MAX_CELLS              1000
MAX_STRINGS            50
MAX_CELLS_PER_STRING   500
MAX_BYPASS_DIODES      100
MAX_MODULES            50
CELL_SURFACE_OFFSET    0.002f   // m above mesh surface
MIN_UPWARD_NORMAL      0.3f     // dot(normal, up) threshold for placement
```

These are fixed-size array bounds, not soft limits — bumping them grows the `AppState` struct directly. `MAX_CELLS_PER_STRING` was raised from a smaller value recently (see commit `3fc7244`); `STRING_SIM_MAX_CELLS` in `string_sim.h` is a separate, smaller bound (100) used by the sim's stack arrays — keep them aligned if you raise either.

## Things to know before editing

- `SaveProject` / `LoadProject` are declared in `app.h` (lines 402–403) but **not implemented**. Adding project save/load means writing both functions plus GUI hooks in the Import panel.
- `app.c` is ~3100 lines and holds nearly all logic outside GUI/sim/auto-layout. Search by section comment header (`// Cells`, `// Wiring`, `// Bypass diodes`, etc.) rather than scrolling.
- `gui.c` uses raygui's "expanded dropdown draws inline" quirk — dropdowns are drawn *last* in `DrawGUI` (after the rest of the sidebar) so their open list isn't covered. Keep that ordering if you add another dropdown.
- File dialogs go through `tinyfiledialogs` (`src/lib/`). On Windows it requires `comdlg32` and `ole32` (already wired in `CMakeLists.txt`).
- The mesh is loaded via raylib's `LoadModel` for OBJ, and via `src/stl_loader.c` for STL (handles both ASCII and binary). STEP/IGES are not supported — convert externally.
- When adding a new field to `AppState`, initialize it in `AppInit` — `AppState app = {0}` in `main.c` zero-inits, but anything that needs a non-zero default (e.g. `-1` for "none" sentinels like `active_string_id`, `selected_module`, `hovered_cell_id`) must be set explicitly.
