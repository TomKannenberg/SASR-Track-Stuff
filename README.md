CppSLib C++ port of tools from https://github.com/ennuo/SlMod (direct port). Thanks to ennuo.

## Building

- Install dependencies via vcpkg:
  ```bash
  vcpkg install zlib glfw3 imgui glad
  ```
- Configure CMake with your vcpkg toolchain (static triplet recommended):
  ```bash
  cmake -S . -B build_mingw -G Ninja -DCMAKE_TOOLCHAIN_FILE=<path-to-vcpkg>/scripts/buildsystems/vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x64-mingw-static
  ```
- Build and run:
  ```bash
  cmake --build build_mingw
  ./build_mingw/CppSLib.exe
  ```
- Release build (smaller/faster, recommended):
  ```bash
  cmake -S . -B build_release -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=<path-to-vcpkg>/scripts/buildsystems/vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x64-mingw-static
  cmake --build build_release
  ./build_release/CppSLib.exe
  ```
  
- Optional CLI tool targets:
  ```bash
  cmake --build build_mingw --target sif_to_unity
  cmake --build build_mingw --target sif_unpacker
  ```

## Usage

- **File -> Load SIF/ZIF/SIG/ZIG File...** to load a SIF/ZIF and its GPU pair (SIG/ZIG).
- **SIF Viewer** shows chunks, sizes, relocations, and GPU status (right-side tab).
- **Hierarchy** tab switches to forest/navigation hierarchies for the selected chunk.
- **CppSLib Stuff** (left panel): use **Unpack XPAC...** to extract into `CppSLib_Stuff/xpac`.
  - ZIF/ZIG are converted to SIF/SIG during unpack.
  - The **SIF Files** virtual tree lets you right-click and load a SIF quickly.
- **Scene** view: WASD move, IJKL rotate, mouse drag rotate, Z/X speed.
