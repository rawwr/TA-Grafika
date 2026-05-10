cmake_minimum_required(VERSION 3.8)
project(PBR)
if(POLICY CMP0177)
  cmake_policy(SET CMP0177 NEW)
endif()

# ── Paths ─────────────────────────────────────────────────────────────────────
set(PROJECT_ROOT     ${PROJECT_SOURCE_DIR}/../..)
set(PROJECT_DATA_DIR ${PROJECT_ROOT}/data)
set(LIB_DIR          ${PROJECT_ROOT}/lib)
set(SRC_DIR          ${PROJECT_ROOT}/src)

# ── Build type ────────────────────────────────────────────────────────────────
if(NOT CMAKE_BUILD_TYPE)
    message(STATUS "Building in Release mode by default")
    set(CMAKE_BUILD_TYPE "Release")
endif()

# ── Source files ─────────────────────────────────────────────────────────────
set(srcCommon
    ${SRC_DIR}/common/application.cpp
    ${SRC_DIR}/common/application.hpp
    ${SRC_DIR}/common/image.cpp
    ${SRC_DIR}/common/image.hpp
    ${SRC_DIR}/common/main.cpp
    ${SRC_DIR}/common/mesh.cpp
    ${SRC_DIR}/common/mesh.hpp
    ${SRC_DIR}/common/optimus.cpp
    ${SRC_DIR}/common/renderer.hpp
    ${SRC_DIR}/common/utils.cpp
    ${SRC_DIR}/common/utils.hpp
)

set(srcOpenGL
    ${SRC_DIR}/opengl.cpp
    ${SRC_DIR}/opengl.hpp
)

# Bundled C libraries (compiled directly into the executable)
set(srcLibraries
    ${LIB_DIR}/stb/src/libstb.c
    ${LIB_DIR}/glad/src/glad.c
    ${LIB_DIR}/imgui/imgui.cpp
    ${LIB_DIR}/imgui/imgui_draw.cpp
    ${LIB_DIR}/imgui/imgui_widgets.cpp
    ${LIB_DIR}/imgui/imgui_tables.cpp
    ${LIB_DIR}/imgui/backends/imgui_impl_glfw.cpp
    ${LIB_DIR}/imgui/backends/imgui_impl_opengl3.cpp
)

# ── Include paths ─────────────────────────────────────────────────────────────
set(includePaths
    ${LIB_DIR}/glm/include
    ${LIB_DIR}/stb/include
    ${LIB_DIR}/glad/include
    ${LIB_DIR}/glfw/include
    ${LIB_DIR}/assimp/include
    ${LIB_DIR}/imgui
    ${LIB_DIR}/imgui/backends
)

# ── Prebuilt Windows libraries ────────────────────────────────────────────────
# GLFW (Direct DLL linkage for MinGW)
add_library(glfw3 SHARED IMPORTED)
set_target_properties(glfw3 PROPERTIES
    IMPORTED_LOCATION  "${LIB_DIR}/glfw/win64/glfw3.dll"
    IMPORTED_IMPLIB    "${LIB_DIR}/glfw/win64/glfw3.dll"
    INTERFACE_INCLUDE_DIRECTORIES "${LIB_DIR}/glfw/include"
)

# Assimp (Direct DLL linkage for MinGW)
add_library(assimp SHARED IMPORTED)
set_target_properties(assimp PROPERTIES
    IMPORTED_LOCATION  "${LIB_DIR}/assimp/win64/assimp.dll"
    IMPORTED_IMPLIB    "${LIB_DIR}/assimp/win64/assimp.dll"
    INTERFACE_INCLUDE_DIRECTORIES "${LIB_DIR}/assimp/include"
)

# OpenGL (system)
find_package(OpenGL REQUIRED)

# ── Executable ────────────────────────────────────────────────────────────────
add_executable(PBR ${srcCommon} ${srcOpenGL} ${srcLibraries})

target_compile_features(PBR PRIVATE cxx_std_14)

target_compile_definitions(PBR PRIVATE
    GLFW_INCLUDE_NONE
    GLM_ENABLE_EXPERIMENTAL
)

target_include_directories(PBR PRIVATE ${includePaths})

# Link statically with MinGW runtime to avoid needing extra DLLs
target_link_options(PBR PRIVATE "-static-libgcc" "-static-libstdc++")

target_link_libraries(PBR
    glfw3
    assimp
    OpenGL::GL
)

# ── Install (copy exe + DLLs into data/ for running) ─────────────────────────
install(TARGETS PBR DESTINATION ${PROJECT_DATA_DIR})
install(FILES
    "${LIB_DIR}/glfw/win64/glfw3.dll"
    "${LIB_DIR}/assimp/win64/assimp.dll"
    DESTINATION ${PROJECT_DATA_DIR}
)
