cmake_minimum_required(VERSION 3.9.2)

project(skshaderc VERSION 1.0
                  DESCRIPTION "Shader compiler for sk_gpu.h"
                  LANGUAGES CXX)

# See: https://github.com/cpm-cmake/CPM.cmake
include(../cmake/CPM.cmake)

CPMAddPackage(
    NAME SPIRV-Cross
    GITHUB_REPOSITORY KhronosGroup/SPIRV-Cross
    GIT_TAG 1ae2b58f19a935494363559f2fb41d7154261933
    OPTIONS 
    "SPIRV_CROSS_CLI OFF" 
    "SPIRV_CROSS_ENABLE_TESTS OFF" 
    "SPIRV_CROSS_ENABLE_MSL OFF"
)

CPMAddPackage(
    NAME glslang
    GITHUB_REPOSITORY KhronosGroup/glslang
    GIT_TAG ae2a562936cc8504c9ef2757cceaff163147834f
    OPTIONS 
    "SKIP_GLSLANG_INSTALL ON"
)

add_executable(skshaderc
    main.cpp
    sksc.cpp
    sksc.h
)

target_link_libraries(skshaderc
    PRIVATE
    spirv-cross-c
    glslang
    SPIRV
)