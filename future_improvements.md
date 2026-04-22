# Future Improvements: PBR/Phong Virtual Lab

This document outlines planned enhancements to transition this project from a technical demo into a fully-featured interactive Virtual Laboratory for Computer Graphics.

## 1. GUI-Based Interaction
Currently, the lab is controlled via keyboard shortcuts. A major improvement would be the integration of a graphical user interface (GUI).

-   **Integration**: Integrate a library like **Dear ImGui**.
-   **Features**:
    -   **Control Panel**: A floating window with checkboxes for Albedo, Normals, Metalness, and Roughness.
    -   **Slider Controls**: Real-time adjustment of light radiance, skybox exposure, and even the Phong shininess exponent.
    -   **Mode Switching**: Radio buttons to switch between Split-Screen, Full PBR, and Full Phong.
    -   **Stats Overlay**: Display frame time, FPS, and GPU information.

## 2. Texture Map Visualization (Overlays)
To improve the educational value, the lab should allow users to inspect the "raw data" behind the render.

-   **Debug Views**: Add a mode to view individual texture maps mapped onto the geometry:
    -   **Albedo Only**: Render the object with only the diffuse color (no lighting).
    -   **Normals Only**: Visualize the world-space or tangent-space normals as colors.
    -   **Metalness/Roughness**: Grayscale visualization of the material properties.
-   **Picture-in-Picture**: Add small overlay windows in the corners of the screen showing the 2D texture files used for the current model.

## 3. Advanced Lighting Features
-   **Shadow Mapping**: Implement shadow maps to allow objects to cast shadows on themselves and each other.
-   **Light Probes**: Add support for dynamic light probes to allow for local reflection changes.
-   **Additional Models**: A selection menu to swap between different models (e.g., the Cerberus gun, a simple sphere, or a complex architectural model).

## 4. Educational Lab Reports
-   **Screenshot Export**: A button to capture side-by-side comparison screenshots with metadata (which maps were active).
-   **Auto-Labeller**: Overlay text on the screen clearly marking "SIDE A: PBR (Cook-Torrance)" and "SIDE B: Classic Phong."
