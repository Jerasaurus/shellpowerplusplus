# Shellpower++ User Manual
## Solar Array Designer - Standard Operating Procedure

A comprehensive guide for designing and simulating solar panel arrays on vehicle bodies.

---

## Table of Contents

1. [Getting Started](#1-getting-started)
2. [Interface Overview](#2-interface-overview)
3. [Step 1: Importing a Mesh](#3-step-1-importing-a-mesh)
4. [Step 2: Selecting Solar Cell Presets](#4-step-2-selecting-solar-cell-presets)
5. [Step 3: Placing Solar Cells](#5-step-3-placing-solar-cells)
6. [Step 4: Auto-Layout System](#6-step-4-auto-layout-system)
7. [Step 5: Wiring Cells into Strings](#7-step-5-wiring-cells-into-strings)
8. [Step 6: Bypass Diodes](#8-step-6-bypass-diodes)
9. [Step 7: Running Simulations](#9-step-7-running-simulations)
10. [Working with Modules](#10-working-with-modules)
11. [Camera Controls](#11-camera-controls)
12. [Keyboard Shortcuts](#12-keyboard-shortcuts)
13. [Auto-Updates](#13-auto-updates)
14. [Troubleshooting](#14-troubleshooting)

---

## 1. Getting Started

### What is Shellpower++?

Shellpower++ is a cross-platform desktop application for designing and simulating solar panel arrays on vehicle surfaces. It allows you to:

- Import 3D vehicle models (OBJ or STL format)
- Place solar cells on the surface manually or automatically
- Wire cells into series strings
- Simulate power generation at different times and orientations
- Optimize array layouts for maximum efficiency

### Supported File Formats

| Format | Description |
|--------|-------------|
| `.obj` | Wavefront OBJ files (standard 3D format) |
| `.stl` | STL files (both ASCII and binary supported) |

**Note:** If your CAD software exports STEP or IGES files, convert them to OBJ first.

### Launching the Application

Run the compiled executable for your platform:
- **Linux:** `./shellpower`
- **macOS:** `./shellpower` or double-click the app bundle
- **Windows:** `shellpower.exe`

---

## 2. Interface Overview

The application window is divided into two main areas:

```
+------------------+----------------------------------------+
|                  |                                        |
|    SIDEBAR       |           3D VIEWPORT                  |
|    (Controls)    |           (Mesh View)                  |
|    [Scrollable]  |                                        |
|                  |                                        |
|  [Mode Tabs]     |                                        |
|  - Import        |                                        |
|  - Cells         |                                        |
|  - Wire          |                                        |
|  - Sim           |                                        |
|                  |                                        |
|  [Panel          |                                        |
|   Controls]      |                                        |
|                  |                                        |
+------------------+----------------------------------------+
|                    STATUS BAR                             |
+-----------------------------------------------------------+
```

**Note:** The sidebar is scrollable. Use the mouse wheel when hovering over the sidebar to scroll if content extends beyond the visible area.

### Mode Tabs

| Mode | Purpose |
|------|---------|
| **Import** | Load and transform 3D mesh files |
| **Cells** | Place solar cells manually or with auto-layout |
| **Wire** | Create wiring strings between cells |
| **Sim** | Configure and run power simulations |

---

## 3. Step 1: Importing a Mesh

### 3.1 Loading a Mesh File

1. Click the **Import** tab in the sidebar
2. Click **Load Mesh File** button
3. Navigate to your OBJ or STL file
4. Click **Open**

The mesh will appear in the 3D viewport.

### 3.2 Setting the Scale

**IMPORTANT:** Most CAD software exports models in millimeters. You must set the correct scale.

| Original Units | Scale Value |
|----------------|-------------|
| Millimeters (mm) | `0.001` |
| Centimeters (cm) | `0.01` |
| Meters (m) | `1.0` |
| Inches | `0.0254` |

**To set scale:**
1. Locate the **Scale** text box in the Import panel
2. Enter the appropriate value (e.g., `0.001` for mm to meters)
3. The mesh will update in real-time

### 3.3 Rotating the Mesh

If your mesh is oriented incorrectly:

1. Use the **X**, **Y**, **Z** rotation sliders to adjust orientation
2. Use the **-90** and **+90** buttons for quick 90-degree rotations
3. The rotation is applied around the mesh center

**Common adjustments:**
- If the car is upside-down: Rotate X by 180
- If the car is facing sideways: Rotate Z by 90 or -90

### 3.4 Verifying Import

After importing, check the mesh info display:
- **Filename** shown in panel
- **Dimensions** (X, Y, Z in meters)
- Visually confirm the mesh looks correct

---

## 4. Step 2: Selecting Solar Cell Presets

Before placing cells, select the solar cell type you'll be using.

### 4.1 Available Presets

| Preset | Dimensions | Efficiency |
|--------|------------|------------|
| **Maxeon Gen 3** | 125 x 125 mm | 22.7% |
| **Maxeon Gen 5** | 125 x 125 mm | 24.0% |
| **Generic Silicon** | 156 x 156 mm | 20.0% |

### 4.2 Selecting a Preset

1. Locate the **Cell Preset** dropdown in the sidebar (below the mode-specific panel)
2. Click the dropdown and select your cell type
3. The cell info panel will display the selected cell's specifications

**Note:** All cells placed after changing the preset will use the new cell type. Existing cells retain their original type.

---

## 5. Step 3: Placing Solar Cells

### Method A: Manual Placement

1. Click the **Cells** tab in the sidebar
2. **Left-click** on any surface of the mesh to place a cell
3. The cell will automatically orient to match the surface normal
4. Repeat to place additional cells

**Placement Rules:**
- Cells cannot overlap (minimum spacing is enforced)
- Cells only place on upward-facing surfaces
- A small offset (2mm) keeps cells above the mesh surface

### Method B: Auto-Layout (Recommended)

See [Step 4: Auto-Layout System](#6-step-4-auto-layout-system) for automated placement.

### 5.1 Clearing All Cells

To remove all placed cells:
1. Click the **Cells** tab
2. Click **Clear All Cells** button
3. All cells will be removed from the mesh

---

## 6. Step 4: Auto-Layout System

The auto-layout system intelligently places cells to maximize coverage while respecting surface constraints.

### 6.1 Configuration Options

| Setting | Description | Default |
|---------|-------------|---------|
| **Target Area** | Total cell area to place (m) | 6.0 |
| **Min Angle** | Minimum surface angle from horizontal | 62 |
| **Max Angle** | Maximum surface angle from horizontal | 90 |
| **Surface Threshold** | Max angle between adjacent triangles | 30 |
| **Optimize Occlusion** | Consider shading during placement | On |
| **Use Grid Layout** | Use grid pattern instead of mesh-based | On |

### 6.2 Height Constraints

The height constraint prevents cells from being placed on certain areas (like the canopy):

1. **Auto-Detect:** Check "Auto-detect shell top" to automatically determine optimal height range
2. **Manual Sliders:** Use the min/max height sliders to set bounds numerically
3. **Visual Editor:** Click **Set Bounds Visually** to open an interactive side-view editor:
   - Drag the **MIN** handle (blue) to set the lower bound
   - Drag the **MAX** handle (orange) to set the upper bound
   - The 3D view shows the mesh with height planes
   - Press **ESC** or click **Done** to confirm

### 6.3 Running Auto-Layout

1. Click the **Cells** tab
2. Configure the auto-layout settings
3. (Optional) Check **Preview valid surfaces** to see where cells can be placed
4. Click **Run Auto-Layout**
5. A progress bar will show completion status
6. Cells will appear on valid surfaces

### 6.4 Understanding Surface Angle

```
        Surface Normal
             ^
             |
             |  (angle measured from horizontal)
        _____|_____
       /     |     \
      /      |      \
     /       |       \
    /_________|_______\
          Surface
```

- **90** = perfectly horizontal (facing up)
- **0** = vertical (facing sideways)
- **62-90** = typical range for solar panels (captures mostly upward surfaces)

---

## 7. Step 5: Wiring Cells into Strings

Cells must be wired into series strings to simulate realistic electrical behavior.

### 7.1 Understanding Strings

- A **string** is a group of cells connected in series
- Maximum of **50 strings** supported
- Each cell can belong to only **one string**
- Cells are color-coded by string assignment

### 7.2 Creating a String

**Method 1: Click Individual Cells**
1. Click the **Wire** tab
2. Click **New String** (or press **N**) to start a new string
3. Click on cells to add them to the current string
4. Click **End String** (or press **E** / **Right-click**) when finished

**Method 2: Group Select (Recommended for Many Cells)**
1. Click the **Wire** tab
2. Click **Group Select Cells** button
3. Click and drag to draw a rectangle around the cells you want to select
4. Release to add all unwired cells in the rectangle to the current string
5. Cells are automatically wired in a **snake pattern** (left-to-right, then right-to-left on next row)
6. Repeat as needed, then end the string

The snake pattern ensures efficient series wiring by minimizing wire lengths between adjacent cells.

### 7.3 String Visualization

**Before simulation:** Cells show their assigned string in distinct colors (green, blue, purple, etc.)

**After simulation:** In **String Power** mode (default), cells are colored by their string's power output:

| Color | Meaning |
|-------|---------|
| Green | String producing near maximum power |
| Yellow | String producing partial power |
| Red | String producing little or no power |
| Gray | Shaded cells |
| Blue | Unwired cells |

### 7.4 Clearing Wiring

To remove all wiring:
1. Click **Wire** tab
2. Click **Clear All Wiring**
3. All cells will be un-assigned from strings

---

## 8. Step 6: Bypass Diodes

Bypass diodes protect strings from power loss when individual cells are shaded.

### 8.1 Understanding Bypass Diodes

In a series string, a shaded cell limits the current of the entire string. Bypass diodes allow current to flow around shaded cells, preventing them from dragging down the whole string.

- A bypass diode spans a **segment** of cells (from cell A to cell B)
- When any cell in the segment can't provide enough current, the **entire segment** is bypassed
- You can have multiple bypass diodes on a single string, including nested segments

### 8.2 Adding a Bypass Diode

1. Click the **Wire** tab
2. Scroll down to the **BYPASS DIODES** section
3. Click **Place Bypass Diode** to enter placement mode (button shows "[Active]")
4. Click on the **first cell** of the segment you want to bypass
5. Click on the **last cell** of the segment
6. The bypass diode is created and shown as a purple arc between the cells

### 8.3 Nested Bypass Diodes

You can create bypass diodes within other bypass diodes for finer-grained control:

**Example:** Cells 1-10 in a string
- Large bypass diode: cells 1-10 (safety net for entire section)
- Small bypass diode: cells 4-6 (targeted protection)

If cell 5 is shaded, only cells 4-6 are bypassed (the smallest segment covering the shaded cell). Cells 1-3 and 7-10 remain active.

### 8.4 Clearing Bypass Diodes

1. Click **Wire** tab
2. Click **Clear All Bypass Diodes**
3. All bypass diodes will be removed

### 8.5 Bypass Diode Visualization

- Bypass diodes appear as **purple arcs** connecting the start and end cells
- After simulation, use **Bypass Status** visualization mode to see which cells are being bypassed (red = bypassed, green = active)

---

## 9. Step 7: Running Simulations

### 9.1 Setting Location and Date

1. Click the **Sim** tab
2. Configure location:
   - **Latitude:** Enter decimal degrees (-90 to +90)
   - **Longitude:** Enter decimal degrees (-180 to +180)
3. Configure date:
   - **Month:** Use spinner (1-12)
   - **Day:** Use spinner (1-31)
4. Set **Irradiance** (typically 1000 W/m for standard testing)

### 9.2 Instant Simulation (Single Time Point)

Use this for quick analysis at a specific time:

1. Adjust the **Hour** slider (0-24, where 12 = solar noon)
2. Click **Run Instant Simulation**
3. View results:
   - **Total Power (W):** Power output at this instant
   - **Shading %:** Percentage of cells in shadow
   - **Sun Altitude:** Angle of sun above horizon
   - **Sun Azimuth:** Compass direction of sun

### 9.3 Daily Energy Simulation (Full Day)

Use this for comprehensive analysis:

1. Configure simulation parameters:
   - **Time Samples:** Number of time points (12-96, more = higher accuracy)
   - **Heading Samples:** Number of vehicle orientations (4-36)
2. Click **Run Daily Simulation**
3. Wait for progress bar to complete
4. View results:

| Result | Description |
|--------|-------------|
| **Daily Energy (Wh)** | Total energy generated over the day |
| **Average Power (W)** | Mean power output |
| **Peak Power (W)** | Maximum instantaneous power |
| **Average Shading %** | Mean shading across all times |
| **Capture Efficiency** | Actual vs. ideal tracking performance |

### 9.4 Per-String Results

After running a daily simulation, view the energy breakdown by string:
- Each string's contribution to total energy is displayed
- Helps identify underperforming strings

### 9.5 Cell Visualization Modes

After running a simulation, use the **Cell Color Mode** dropdown to visualize different aspects:

| Mode | Description |
|------|-------------|
| **String Power** | Colors cells by their string's power output (green = max, red = low) |
| **Cell Flux** | Colors cells by irradiance flux based on angle to sun (green = facing sun, red = facing away) |
| **Cell Current** | Colors cells by photo-generated current (blue = low, yellow = high) |
| **Shading** | Shows shaded (dark gray) vs sunlit (yellow) cells |
| **Bypass Status** | Shows bypassed cells (red) vs active cells (green) |

### 9.6 Understanding Simulation Physics

The simulation uses a **full IV trace model** for accurate string power calculation:

1. **Sun Position:** Calculated from date, time, latitude, and longitude
2. **Cell IV Curves:** Each cell generates a current-voltage curve based on:
   - Irradiance level
   - Angle of incidence (cosine of angle between sun and cell normal)
   - Single-diode model with series resistance
3. **Shading:** Ray-casting detects which cells are blocked by the vehicle body
4. **String Simulation:** Series-connected cells are simulated together:
   - The string's IV curve is computed by summing cell voltages at each current
   - Maximum Power Point (MPP) is found by sweeping the IV curve
   - Mismatched cells (shaded or poorly angled) limit the string current
5. **Bypass Diodes:** When a cell can't provide the string current:
   - The smallest bypass diode segment covering that cell activates
   - All cells in that segment are bypassed together
   - String current is set by the remaining active cells

---

## 10. Working with Modules

Modules let you save and reuse cell layout patterns.

### 10.1 Creating a Module

1. Place cells in the desired pattern
2. Click the **Cells** tab
3. Enter a name in the **Module Name** text box
4. Click **Create Module**
5. The module is saved to the `modules/` directory

### 10.2 Loading a Module

1. Click the **Cells** tab
2. Find your module in the **Saved Modules** list
3. Click on the module name to select it
4. Click **Place Module** button
5. Click on the mesh to place the module

### 10.3 Deleting a Module

1. Click the **X** button next to the module name
2. The module file will be deleted

### 10.4 Module Storage

Modules are stored as JSON files in:
```
shellpower/modules/<module_name>.json
```

---

## 11. Camera Controls

### 11.1 Mouse Controls

| Action | Result |
|--------|--------|
| **Left-click + Drag** | Rotate view (orbit around mesh) |
| **Scroll Wheel** | Zoom in/out |
| **Middle-click + Drag** | Pan view |

### 11.2 Camera Modes

| Mode | Description | Best For |
|------|-------------|----------|
| **Perspective** | 3D view with depth | General viewing |
| **Orthographic** | Top-down flat view | Precise cell placement |

Toggle between modes:
- Click the **Top-Down View** checkbox in the sidebar
- Or press **T** on the keyboard

### 11.3 Reset Camera

To reset the camera to fit the entire mesh:
- Click **Reset Camera** button
- Or press **R** on the keyboard

---

## 12. Keyboard Shortcuts

| Key | Action |
|-----|--------|
| **N** | Start new wiring string |
| **E** | End current wiring string |
| **T** | Toggle perspective/orthographic camera |
| **R** | Reset camera to fit mesh |
| **ESC** | Cancel bypass diode placement (in Wire mode) |
| **Right-click** | End current wiring string (in Wire mode) |

---

## 13. Auto-Updates

Shellpower++ automatically checks for updates on startup.

### 13.1 Update Notifications

When a new version is available:
1. A dialog will appear showing the new version number
2. Click **Yes** to open the GitHub releases page
3. Download the appropriate version for your platform
4. Replace the old executable with the new one

### 13.2 Offline Mode

If you don't have an internet connection:
- The update check runs in the background and won't block the application
- You can continue using the current version without interruption

---

## 14. Troubleshooting

### Mesh Not Visible

**Problem:** Loaded mesh doesn't appear in viewport

**Solutions:**
1. Check the scale value (if too small, mesh may be invisible)
2. Press **R** to reset camera
3. Try scale values like `1.0`, `0.1`, `0.01`, `0.001`

### Cells Not Placing

**Problem:** Clicking on mesh doesn't place cells

**Solutions:**
1. Ensure you're in **Cells** mode
2. Click on upward-facing surfaces only
3. Check if maximum cell count (1000) is reached
4. Ensure you're not clicking too close to existing cells

### Simulation Shows Zero Power

**Problem:** Simulation returns 0W output

**Solutions:**
1. Verify cells are placed on the mesh
2. Check that simulation hour is during daylight (roughly 6-18)
3. Verify latitude/longitude are reasonable values
4. Check date settings (month 1-12, day 1-31)

### STL File Not Loading

**Problem:** Error when loading STL file

**Solutions:**
1. Verify file is valid STL format
2. Try re-exporting from CAD software
3. Check file permissions
4. Try OBJ format instead

### Auto-Layout Places No Cells

**Problem:** Auto-layout completes but no cells appear

**Solutions:**
1. Increase target area
2. Widen angle constraints (try 30-90)
3. Disable height constraint or adjust bounds
4. Check that mesh has valid upward-facing surfaces
5. Check **Preview valid surfaces** to verify placement areas

---

## Appendix A: Default Values Reference

| Parameter | Default Value |
|-----------|---------------|
| Scale | 0.001 (mm to m) |
| Latitude | 37.4 |
| Longitude | -122.2 |
| Irradiance | 1000 W/m |
| Simulation Month | June (6) |
| Simulation Day | 21 |
| Hour | 12.0 |
| Target Area | 6.0 m |
| Min Normal Angle | 62 |
| Max Normal Angle | 90 |
| Surface Threshold | 30 |
| Time Samples | 48 |
| Heading Samples | 12 |

---

## Appendix B: Technical Specifications

| Limit | Value |
|-------|-------|
| Maximum Cells | 1000 |
| Maximum Strings | 50 |
| Cells per String | 100 |
| Maximum Bypass Diodes | 100 |
| Maximum Modules | 50 |
| Cells per Module | 100 |

---

## Appendix C: Solar Cell Specifications

### Maxeon Gen 3
- **Dimensions:** 125 x 125 mm
- **Area:** 0.015625 m
- **Efficiency:** 22.7%
- **Peak Power (1000 W/m):** ~3.55 W

### Maxeon Gen 5
- **Dimensions:** 125 x 125 mm
- **Area:** 0.015625 m
- **Efficiency:** 24.0%
- **Peak Power (1000 W/m):** ~3.75 W

### Generic Silicon
- **Dimensions:** 156 x 156 mm
- **Area:** 0.024336 m
- **Efficiency:** 20.0%
- **Peak Power (1000 W/m):** ~4.87 W

---

## Workflow Summary

```
1. IMPORT      2. CONFIGURE     3. PLACE         4. WIRE        5. BYPASS      6. SIMULATE
   Mesh    -->    Scale     -->   Cells      -->  Strings   -->  Diodes    -->   Power
                  Rotate         (Manual or       (Series)      (Optional)      (Instant or
                                  Auto)                                          Daily)
```

**Typical Workflow:**
1. Load your vehicle mesh (OBJ/STL)
2. Set correct scale (usually 0.001 for mm)
3. Rotate if needed to orient correctly
4. Select cell preset (Maxeon Gen 5 recommended)
5. Use auto-layout or manually place cells
6. Wire cells into strings (use Group Select for snake pattern)
7. Add bypass diodes to protect against shading (optional)
8. Set location and date
9. Run daily simulation
10. Iterate on layout to optimize results

---

*Shellpower++ - A C port of the original C# Shellpower project with additional features*
