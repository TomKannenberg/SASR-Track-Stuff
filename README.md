CppSLib C++ port of tools from https://github.com/ennuo/SlMod (direct port). Thanks to ennuo.

## Building

- Install dependencies via vcpkg:
  ```bash
  vcpkg install zlib glfw3 imgui glad
  ```
- Configure CMake with your vcpkg toolchain:
  ```bash
  cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=<path-to-vcpkg>/scripts/buildsystems/vcpkg.cmake
  ```
- Build and run:
  ```bash
  cmake --build build
  ./build/CppSLib.exe
  ```

## Usage

- **File -> Load SIF/ZIF/SIG/ZIG File...** to load a SIF/ZIF and its GPU pair (SIG/ZIG).
- **SIF Viewer** shows chunks, sizes, relocations, and GPU status.
- **Hierarchy** panel: toggle forest/tree/branch visibility and select nodes to inspect.
- **Scene** view: WASD move, IJKL rotate, mouse drag rotate, Z/X speed.
