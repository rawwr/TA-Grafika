# Physically Based Rendering - Demo Pembelajaran
(c) 2017 - 2018 Michał Siejak ([@Nadrin](https://twitter.com/Nadrin))

Implementasi standalone dari model shading berbasis fisika (PBR) dan image-based lighting menggunakan OpenGL 4.5.

![Screenshot](https://raw.githubusercontent.com/Nadrin/PBR/master/data/screenshot.jpg)

## Tentang Proyek Ini

Proyek ini bertujuan untuk **mendemonstrasikan perbandingan antara shading standar modern (PBR) dengan Phong shading**, serta **memvisualisasikan efek dari masing-masing komponen tekstur** (albedo, normal, roughness, metallic) terhadap hasil render akhir. 

Program ini ditujukan untuk **membantu pemula di bidang 3D** agar dapat memahami fungsi setiap jenis tekstur dan perbedaan antara model shading klasik dengan modern.

### Fitur Utama

- **Perbandingan Split-Screen**: Lihat perbedaan langsung antara PBR dan Phong shading
- **Toggle Komponen Tekstur**: Aktifkan/nonaktifkan setiap komponen untuk melihat pengaruhnya
- **Image-Based Lighting (IBL)**: Pencahayaan realistis dari environment map
- **Debug Visualization**: Mode visualisasi untuk setiap komponen material
- **Multi-Mesh Support**: Mendukung loading model FBX dengan multiple sub-meshes

### Implementasi Teknis

Implementasi inti terdapat dalam `src/opengl.cpp` dan `src/opengl.hpp`. Fungsi pendukung seperti loading gambar, model 3D, dan application loop dapat ditemukan di direktori `src/common`.

Shader dilengkapi dengan komentar detail untuk menjelaskan model shading dan teknik IBL yang digunakan.

## Cara Build

### Windows

#### Prasyarat

- Windows 10 atau lebih baru (x64)
- MinGW-w64 toolchain
- CMake 3.8 atau lebih baru

#### Cara Build (MinGW)

1. Buka PowerShell terminal
2. Jalankan script build:
```powershell
.\build.bat
```

Atau secara manual:
```powershell
mkdir projects/cmake/build
cd projects/cmake/build
cmake -G "MinGW Makefiles" ..
mingw32-make install
```

3. Executable dan semua asset akan berada di direktori `data`

### Linux

#### Prasyarat

 - C/C++ compiler dengan dukungan C++14
 - CMake 3.8 atau lebih baru
 - pkg-config
 - Development files untuk GLFW3, Assimp, dan OpenGL
  
#### Cara Build

1. Install prasyarat; untuk Debian/Ubuntu:
```bash
sudo apt install build-essential cmake pkg-config libglfw3-dev libassimp-dev libgl1-mesa-dev
```
    
2. Configure & build:
```bash
mkdir -p projects/cmake/build
cd projects/cmake/build
cmake ..
make install
```

3. Setelah build berhasil, executable dapat ditemukan di direktori `data`

## Menjalankan Program

Jalankan `PBR.exe` dari dalam direktori `data`. Resolusi layar diset ke HD standar (1920x1080).

### Kontrol

Input        | Aksi
-------------|-------
LMB drag     | Rotasi kamera
RMB drag     | Rotasi model 3D
**WASD**     | **Rotasi model 3D (keyboard)**
**← →**      | **Geser posisi split screen (kiri/kanan)**
Scroll wheel | Zoom in/out
F1-F3        | Toggle lampu directional on/off (default: OFF)
Space        | Toggle split-screen (PBR vs Phong)
1            | Toggle komponen tekstur Albedo
2            | Toggle komponen Normal Map
3            | Toggle komponen Metalness
4            | Toggle komponen Roughness

**Catatan**: 
- WASD memberikan kontrol rotasi model yang lebih halus dan intuitif. Tahan tombol untuk rotasi berkelanjutan.
- Arrow keys kiri/kanan menggeser pembagi split screen untuk melihat lebih banyak dari sisi PBR atau Phong.

### Memahami Komponen Tekstur

- **Albedo (1)**: Warna dasar material tanpa informasi pencahayaan
- **Normal Map (2)**: Detail permukaan mikro yang mempengaruhi arah pantulan cahaya
- **Metalness (3)**: Menentukan apakah material adalah logam (putih) atau dielektrik (hitam)
- **Roughness (4)**: Tingkat kekasaran permukaan - halus (hitam) vs kasar (putih)

### Mode Debug Visualization

Gunakan panel kontrol untuk mengaktifkan mode visualisasi:
- **None**: Render normal dengan semua komponen
- **Albedo Only**: Hanya menampilkan warna albedo
- **Normals Only**: Visualisasi normal map (RGB = XYZ)
- **Metalness Only**: Visualisasi nilai metalness (grayscale)
- **Roughness Only**: Visualisasi nilai roughness (grayscale)

## Fitur Tambahan

### Multi-Mesh Loading
Program ini telah dimodifikasi untuk mendukung loading model FBX dengan multiple sub-meshes. Semua mesh dalam file akan digabungkan secara otomatis dengan vertex offset yang benar.

### Konfigurasi Scene
- **FOV**: 75° (field of view yang lebar)
- **Model Scale**: Dapat disesuaikan via kode (default: 40x untuk model kecil)
- **Model Rotation**: Dapat disesuaikan via kode (default: 90° rotasi X)
- **Lights**: 3 directional lights (default OFF, gunakan IBL saja)

## Referensi

Implementasi physically based shading ini sebagian besar berdasarkan informasi dari course berikut:

- [Real Shading in Unreal Engine 4](http://blog.selfshadow.com/publications/s2013-shading-course/karis/s2013_pbs_epic_notes_v2.pdf), Brian Karis, SIGGRAPH 2013
- [Moving Frostbite to Physically Based Rendering](https://seblagarde.wordpress.com/2015/07/14/siggraph-2014-moving-frostbite-to-physically-based-rendering/), Sébastien Lagarde, Charles de Rousiers, SIGGRAPH 2014

Sumber lain yang membantu dalam riset & implementasi:

- [Adopting Physically Based Shading Model](https://seblagarde.wordpress.com/2011/08/17/hello-world/), Sébastien Lagarde
- [Microfacet Models for Refraction through Rough Surfaces](https://www.cs.cornell.edu/~srm/publications/EGSR07-btdf.pdf), Bruce Walter et al., Eurographics, 2007
- [An Inexpensive BRDF Model for Physically-Based Rendering](http://igorsklyar.com/system/documents/papers/28/Schlick94.pdf), Christophe Schlick, Eurographics, 1994
- [GPU-Based Importance Sampling](https://developer.nvidia.com/gpugems/GPUGems3/gpugems3_ch20.html), Mark Colbert, Jaroslav Křivánek, GPU Gems 3, 2007
- [Specular BRDF Reference](http://graphicrants.blogspot.com/2013/08/specular-brdf-reference.html), Brian Karis
- [To PI or not to PI in game lighting equation](https://seblagarde.wordpress.com/2012/01/08/pi-or-not-to-pi-in-game-lighting-equation/), Sébastien Lagarde
- [Physically Based Rendering: From Theory to Implementation, 2nd ed.](https://www.amazon.com/Physically-Based-Rendering-Second-Implementation/dp/0123750792), Matt Pharr, Greg Humphreys, 2010

## Library Pihak Ketiga

Proyek ini menggunakan library open source berikut:

- [Open Asset Import Library](http://assimp.sourceforge.net/)
- [stb_image](https://github.com/nothings/stb)
- [GLFW](http://www.glfw.org/)
- [GLM](https://glm.g-truc.net/)
- [glad](https://github.com/Dav1dde/glad) (untuk generate OpenGL function loader)
- [Dear ImGui](https://github.com/ocornut/imgui) (untuk GUI panel)

## Asset yang Disertakan

Asset berikut dibundel dengan proyek:

- Model "Cerberus" gun oleh [Andrew Maximov](http://artisaverb.info)
- HDR environment map oleh [Bob Groothuis](http://www.bobgroothuis.com/blog/) dari [HDRLabs sIBL archive](http://www.hdrlabs.com/sibl/archive.html) (distributed under [CC-BY-NC-SA 3.0](https://creativecommons.org/licenses/by-nc-sa/3.0/us/))

## Modifikasi dari Versi Original

- ✅ Multi-mesh FBX loading support
- ✅ Adjustable model scale dan rotation
- ✅ Wider FOV (75°) untuk viewing yang lebih baik
- ✅ Directional lights default OFF (pure IBL)
- ✅ Default tangent/UV handling untuk model tanpa data lengkap
- ✅ **WASD keyboard controls untuk rotasi model yang lebih intuitif**
- ✅ **Arrow keys untuk menggeser posisi split screen secara smooth**
- ✅ README dalam Bahasa Indonesia untuk pembelajaran

## Lisensi

Lihat file `COPYING.txt` untuk informasi lisensi.
