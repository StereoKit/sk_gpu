# sk_gpu.h

sk_gpu.h is a mid-level cross-platform graphics library focused on Mixed Reality rendering, in an amalgamated single file header! It currently uses D3D11 on Windows, GLES on Android, and WebGL on the Web, and works very well with OpenXR.

I'm the author, [Nick Klingensmith](https://twitter.com/koujaku), and I've spent two decades doing graphics, tools and tech art as a professional game developer! I currently work at Microsoft on the Mixed Reality team.

## Repository

There are 3 major sections to this repository!

### 1. [sk_gpu.h source](https://github.com/maluoi/sk_gpu/tree/master/src)

These files are the core of the project, and they get squished into a single file at the root directory by a Python script! The sk_gpu_flat project in the examples folder is set up to squish these files automatically every time it's built.

### 2. [skshaderc](https://github.com/maluoi/sk_gpu/tree/master/skshaderc)

This is a shader compiler that uses [DXShaderCompiler](https://github.com/microsoft/DirectXShaderCompiler) and [Spirv-Cross](https://github.com/KhronosGroup/SPIRV-Cross) to compile real HLSL shader code into a single file containing HLSL bytecode, SPIRV, GLSL, and GLSL Web, along with some metadata about buffer layout and uniforms. sk_gpu.h loads these files and picks the right chunk to use on the right platform :)

### 3. [Examples](https://github.com/maluoi/sk_gpu/tree/master/examples)

These are maybe more my tests or development environments, but these are a set of project I use to test out sk_gpu's features on different platforms! There's an OpenXR powered Oculus Quest project, an OpenXR powered Windows project, and a flatscreen project that will also compile to WASM.

## License

License is MIT! Have fun :)