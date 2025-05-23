# sk_gpu.h

sk_gpu.h is a mid-level cross-platform graphics library focused on Mixed Reality rendering, in an amalgamated single file header! It currently uses D3D11 on Windows, GLES on Android, and WebGL on the Web, and works very well with OpenXR.

## Consuming

To use this project as a dependency, it's recommended use the amalgamated header file and pre-compiled skshaderc executables from the releases hosted here. The release files come with a useful cmake wrapper that makes this pretty straightforward to use in a cmake project:

```cmake
cmake_minimum_required(VERSION 3.14)
project(sk_gpu_test VERSION "0.1.0" LANGUAGES CXX C)

include(FetchContent)
FetchContent_Declare(
  sk_gpu
  URL https://github.com/StereoKit/sk_gpu/releases/download/v2024.3.31/sk_gpu.v2024.3.31.zip )
FetchContent_MakeAvailable(sk_gpu)

add_executable(sk_gpu_test
  src/main.cpp )

skshaderc_compile_headers(sk_gpu_test
  ${CMAKE_BINARY_DIR}/shaders/
  "-O3 -t xge"
  src/test.hlsl
  src/test2.hlsl )

target_link_libraries(sk_gpu_test
  PRIVATE sk_gpu )
```

## Building

sk_gpu uses a cmake based workflow, so standard cmake builds will work. This repository also comes with a number of cmake presets to make this process a bit easier!

```sh
cmake --preset test_Win32_x64
cmake --build --preset test_Win32_x64_Debug

bin/intermediate/Win32_x64/Debug/skg_flatscreen.exe
```

VSCode with the cmake plugin works well as an IDE for this project.

### Prerequisites

Python is used for header amalgamation on all platforms.
Ninja is used by the presets for Linux and Mac

## Repository

There are 3 major sections to this repository!

### 1. [sk_gpu.h source](https://github.com/maluoi/sk_gpu/tree/master/src)

These files are the core of the project, and they get squished into a single file at the root directory by a Python script! The sk_gpu_flat project in the examples folder is set up to squish these files automatically every time it's built.

### 2. [skshaderc](https://github.com/maluoi/sk_gpu/tree/master/skshaderc)

This is a shader compiler that uses [glslang](https://github.com/KhronosGroup/glslang) and [Spirv-Cross](https://github.com/KhronosGroup/SPIRV-Cross) and [SPIRV-Tools](https://github.com/KhronosGroup/SPIRV-Tools) to compile and optimize real HLSL shader code into a single file containing HLSL bytecode, SPIRV, GLSL, GLSL ES, and GLSL Web, along with some metadata about buffer layout and uniforms. sk_gpu.h loads these files and picks the right chunk to use on the right platform :)

### 3. [Examples](https://github.com/maluoi/sk_gpu/tree/master/examples)

These are maybe more my tests or development environments, but these are a set of project I use to test out sk_gpu's features on different platforms! There's an OpenXR powered Oculus Quest project, an OpenXR powered Windows project, and a flatscreen project that will also compile to WASM.

## License

License is MIT! Have fun :)