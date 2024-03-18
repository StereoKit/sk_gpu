cmake_minimum_required(VERSION 3.7)

project(sk_gpu VERSION 0.1
               DESCRIPTION "Single header cross-platform graphics api."
               LANGUAGES CXX)

# Make switches for each of the subdirectories
option(SK_BUILD_SHADERC  "Build the shader compiler" ON)
option(SK_BUILD_EDITOR   "Build the shader editor" ON)
option(SK_BUILD_EXAMPLES "Build the examples" ON)

add_subdirectory(src)
if (SK_BUILD_SHADERC)
    add_subdirectory(skshaderc)
endif()
if (SK_BUILD_EDITOR)
    add_subdirectory(skshader_editor)
endif()
if (SK_BUILD_EXAMPLES)
    add_subdirectory(examples/sk_gpu_flat)
endif()