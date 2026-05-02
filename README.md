# Physically Based Rendering
(c) 2017 - 2018 Michał Siejak ([@Nadrin](https://twitter.com/Nadrin))

A standalone implementation of physically based shading model & image based lighting using OpenGL 4.5.

![Screenshot](https://raw.githubusercontent.com/Nadrin/PBR/master/data/screenshot.jpg)

## About

The goal of this project is to showcase the use of OpenGL 4.5 for high-fidelity real-time rendering. It implements a physically based shading model with image-based lighting (IBL).

The implementation is self-contained within `src/opengl.cpp` and `src/opengl.hpp`. Shared functionality such as loading images, 3D models, and the application loop can be found in the `src/common` directory.

Shaders are heavily commented to explain the shading model and IBL techniques used.

## Building

### Windows

#### Prerequisites

- Windows 10 or newer (x64)
- MinGW-w64 toolchain
- CMake 3.8 or newer

#### How to build (MinGW)

1. Open a PowerShell terminal.
2. Configure and build using CMake:
```powershell
mkdir projects/cmake/build
cd projects/cmake/build
cmake -G "MinGW Makefiles" ..
mingw32-make install
```
3. The resulting executable and all needed assets will be in the `data` directory.

### Linux

#### Prerequisites

 - C/C++ compiler supporting C++14
 - CMake 3.8 or newer
 - pkg-config
 - Development files for GLFW3, Assimp, and OpenGL
  
#### How to build

1. Install prerequisites; for Debian/Ubuntu:
```
sudo apt install build-essential cmake pkg-config libglfw3-dev libassimp-dev libgl1-mesa-dev
```
    
2. Configure & build the project:
```
mkdir -p projects/cmake/build
cd projects/cmake/build
cmake ..
make install
```

3. After successful build the resulting executable can be found in `data` directory.

## Running

The application runs as a standalone OpenGL 4.5 demo. Run `PBR.exe` from within the `data` directory. 
The screen resolution is set to standard HD (1920x1080).

### Controls

Input        | Action
-------------|-------
LMB drag     | Rotate camera
RMB drag     | Rotate 3D model
Scroll wheel | Zoom in/out
F1-F3        | Toggle analytical lights on/off
Space        | Toggle split-screen (PBR vs Classic Phong)
1            | Toggle Albedo texture component
2            | Toggle Normal Map component
3            | Toggle Metalness component
4            | Toggle Roughness component

## Bibliography

This implementation of physically based shading is largely based on information obtained from the following courses:

- [Real Shading in Unreal Engine 4](http://blog.selfshadow.com/publications/s2013-shading-course/karis/s2013_pbs_epic_notes_v2.pdf), Brian Karis, SIGGRAPH 2013
- [Moving Frostbite to Physically Based Rendering](https://seblagarde.wordpress.com/2015/07/14/siggraph-2014-moving-frostbite-to-physically-based-rendering/), Sébastien Lagarde, Charles de Rousiers, SIGGRAPH 2014

Other resources that helped me in research & implementation:

- [Adopting Physically Based Shading Model](https://seblagarde.wordpress.com/2011/08/17/hello-world/), Sébastien Lagarde
- [Microfacet Models for Refraction through Rough Surfaces](https://www.cs.cornell.edu/~srm/publications/EGSR07-btdf.pdf), Bruce Walter et al., Eurographics, 2007
- [An Inexpensive BRDF Model for Physically-Based Rendering](http://igorsklyar.com/system/documents/papers/28/Schlick94.pdf), Christophe Schlick, Eurographics, 1994
- [GPU-Based Importance Sampling](https://developer.nvidia.com/gpugems/GPUGems3/gpugems3_ch20.html), Mark Colbert, Jaroslav Křivánek, GPU Gems 3, 2007
- [Hammersley Points on the Hemisphere](http://holger.dammertz.org/stuff/notes_HammersleyOnHemisphere.html), Holger Dammertz
- [Notes on Importance Sampling](http://blog.tobias-franke.eu/2014/03/30/notes_on_importance_sampling.html), Tobias Franke
- [Specular BRDF Reference](http://graphicrants.blogspot.com/2013/08/specular-brdf-reference.html), Brian Karis
- [To PI or not to PI in game lighting equation](https://seblagarde.wordpress.com/2012/01/08/pi-or-not-to-pi-in-game-lighting-equation/), Sébastien Lagarde
- [Physically Based Rendering: From Theory to Implementation, 2nd ed.](https://www.amazon.com/Physically-Based-Rendering-Second-Implementation/dp/0123750792), Matt Pharr, Greg Humphreys, 2010
- [Advanced Global Illumination, 2nd ed.](https://www.amazon.com/Advanced-Global-Illumination-Second-Philip/dp/1568813074), Philip Dutré, Kavita Bala, Philippe Bekaert, 2006
- [Photographic Tone Reproduction for Digital Images](https://www.cs.utah.edu/~reinhard/cdrom/), Erik Reinhard et al., 2002

## Third party libraries

This project makes use of the following open source libraries:

- [Open Asset Import Library](http://assimp.sourceforge.net/)
- [stb_image](https://github.com/nothings/stb)
- [GLFW](http://www.glfw.org/)
- [GLM](https://glm.g-truc.net/)
- [glad](https://github.com/Dav1dde/glad) (used to generate OpenGL function loader)

## Included assets

The following assets are bundled with the project:

- "Cerberus" gun model by [Andrew Maximov](http://artisaverb.info).
- HDR environment map by [Bob Groothuis](http://www.bobgroothuis.com/blog/) obtained from [HDRLabs sIBL archive](http://www.hdrlabs.com/sibl/archive.html) (distributed under [CC-BY-NC-SA 3.0](https://creativecommons.org/licenses/by-nc-sa/3.0/us/)).
/us/)).
