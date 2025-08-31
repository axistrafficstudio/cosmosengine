# Cosmos Engine

High-performance astrophysics simulator in C++17 + OpenGL 4.5.

## Features
- Barnes-Hut N-body gravity (O(n log n))
- Particle collisions (naive; optional)
- Modules: Galaxy, Black Hole, Supernova, Interactions (initial implementations)
- OpenGL rendering with HDR + Bloom
- Dear ImGui UI for live controls

## Build (Windows, MSVC) with vcpkg
1. Install vcpkg and integrate with CMake.
2. Install dependencies:
```
vcpkg install glfw3 glm glad imgui[glfw-binding,opengl3-binding]
```
3. Configure and build with CMake (x64):
```
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE="<vcpkg-root>/scripts/buildsystems/vcpkg.cmake" -DVCPKG_TARGET_TRIPLET=x64-windows
cmake --build build --config Release
```
4. Run:
```
./build/bin/cosmosengine.exe
```

## Controls
- Right mouse drag: orbit camera
- Middle mouse drag: pan
- WASD: translate camera
- UI panel: simulation parameters and reset

## Notes
- Start with 50k-200k particles to gauge performance. Increase gradually.
- For millions of particles, consider GPU compute-based integrators and spatial binning.