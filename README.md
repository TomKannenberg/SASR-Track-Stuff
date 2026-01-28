CppSLib C++ port of tools from https://github.com/ennuo/SlMod (direct port). Thanks to ennuo.

## Building (Windows, MinGW)

- Install dependencies via vcpkg:
  ```bash
  vcpkg install zlib glfw3 imgui glad
  ```
- Configure CMake (Release, fastest):
  ```bash
  cmake -S . -B build_mingw -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release ^
    -DCMAKE_MAKE_PROGRAM=C:\msys64\mingw64\bin\mingw32-make.exe ^
    -DCMAKE_C_COMPILER=C:\msys64\mingw64\bin\gcc.exe ^
    -DCMAKE_CXX_COMPILER=C:\msys64\mingw64\bin\c++.exe ^
    -DCMAKE_PREFIX_PATH=F:\vcpkg\installed\x64-mingw-static ^
    -Dglad_DIR=F:\vcpkg\installed\x64-mingw-static\share\glad ^
    -Dglfw3_DIR=F:\vcpkg\installed\x64-mingw-static\share\glfw3 ^
    -Dimgui_DIR=F:\vcpkg\installed\x64-mingw-static\share\imgui
  ```
- Build and run:
  ```bash
  cmake --build build_mingw --config Release --parallel 16
  .\build_mingw\CppSLib.exe
  ```

- Optional CLI tool targets:
  ```bash
  cmake --build build_mingw --config Release --target sif_to_unity
  cmake --build build_mingw --config Release --target sif_unpacker
  ```

## Usage

- **File -> Load SIF/ZIF/SIG/ZIG File...** to load a SIF/ZIF and its GPU pair (SIG/ZIG).
- **SIF Viewer** shows chunks, sizes, relocations, and GPU status (right-side tab).
- **Hierarchy** tab switches to forest/navigation hierarchies for the selected chunk.
- **CppSLib Stuff** (left panel): use **Unpack XPAC...** to extract into `CppSLib_Stuff/xpac`.
  - ZIF/ZIG are converted to SIF/SIG during unpack.
  - The **SIF Files** virtual tree lets you right-click and load a SIF quickly.
- **Scene** view: WASD move, IJKL rotate, mouse drag rotate, Z/X speed.
