# The skshaderc cmake file exports a few useful items for
# building shaders in your own project! The first is the
# skshaderc_compile_headers macro, which can compile a list
# of shader files, and return a list with the same files
# and .h appended to them.
#
# Example:
# set(PROJECT_SHADERS_HLSL
#     compute_test.hlsl
#     cubemap.hlsl
#     test.hlsl )
# skshaderc_compile_headers(PROJECT_SHADERS_HLSL "-t xge" PROJECT_SHADERS_H)
#
# PROJECT_SHADERS_H now contains a list of .h files that
# contain shader binaries in arrays.
#
# skshaderc_compile_assets is similar, but is not specifically
# for header files. It's best if you're looking for side-by-side
# .sks files. It has no output list.
#
# skshaderc_compile_assets(PROJECT_SHADERS_HLSL "-t xge")
#
# If you wish to design your own shader compiling function,
# this file also exports SKSHADERC_EXE_PATH, which is a 
# generator for the path to the skshaderc compiler executable
# file.

cmake_minimum_required(VERSION 3.9.2)

project(skshaderc VERSION 1.0
                  DESCRIPTION "Shader compiler for sk_gpu.h"
                  LANGUAGES CXX)

# See: https://github.com/cpm-cmake/CPM.cmake
include(../cmake/CPM.cmake)

# For converting SPIR-V to flavors of GLSL
CPMAddPackage(
    NAME SPIRV-Cross
    GITHUB_REPOSITORY KhronosGroup/SPIRV-Cross
    GIT_TAG 50b4d5389b6a06f86fb63a2848e1a7da6d9755ca # May 27, 2022
    OPTIONS 
    "SPIRV_CROSS_CLI OFF" 
    "SPIRV_CROSS_ENABLE_TESTS OFF" 
    "SPIRV_CROSS_ENABLE_MSL OFF"
)

# For SPIRV-Tools
CPMAddPackage(
    NAME SPIRV-Headers
    GITHUB_REPOSITORY KhronosGroup/SPIRV-Headers
    GIT_TAG 5a121866927a16ab9d49bed4788b532c7fcea766 # May 25, 2022
)

# For optimizing SPIR-V shaders, a baseline amount of
# optimization is crucial for meta compatability with
# HLSL compilers.
CPMAddPackage(
    NAME SPIRV-Tools
    GITHUB_REPOSITORY KhronosGroup/SPIRV-Tools
    GIT_TAG v2022.2
    OPTIONS
    "SPIRV_SKIP_TESTS ON"
    "SPIRV_SKIP_EXECUTABLES ON"
    "SPIRV_WERROR OFF"
)

# This is used to convert HLSL to SPIR-V. DXC is an
# alternative, but is far more difficult to compile,
# and generates a much larger binary.
CPMAddPackage(
    NAME glslang
    GITHUB_REPOSITORY KhronosGroup/glslang
    GIT_TAG 7dda6a6347b0bd550e202942adee475956ef462a # May 26, 2022
    OPTIONS 
    "SKIP_GLSLANG_INSTALL ON"
)

if (UNIX)
    set(LINUX_LIBS
        GL
        GLEW)
endif()

add_executable(skshaderc
    main.cpp
    sksc.cpp
    sksc.h
)

target_link_libraries(skshaderc
    PRIVATE
    spirv-cross-c
    SPIRV-Tools-opt
    glslang
    SPIRV
    ${LINUX_LIBS}
)

set(SKSHADERC_EXE_PATH $<TARGET_FILE:skshaderc> PARENT_SCOPE)

macro(SKSHADERC_COMPILE_HEADERS LIST COMMAND_STRING OUT_LIST)
    set(SKSHADERC_COMPILE_COMMANDS ${COMMAND_STRING})
    separate_arguments(SKSHADERC_COMPILE_COMMANDS)

    foreach(SHADER ${${LIST}})
        add_custom_command(
            OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/${SHADER}.h
            COMMAND ${SKSHADERC_EXE_PATH} -h ${SKSHADERC_COMPILE_COMMANDS} ${CMAKE_CURRENT_SOURCE_DIR}/${SHADER}
            DEPENDS 
                ${SHADER} 
                ${SKSHADERC_EXE_PATH}
            COMMENT "skshaderc -h ${COMMAND_STRING} ${CMAKE_CURRENT_SOURCE_DIR}/${SHADER}")
        list(APPEND ${OUT_LIST} ${SHADER}.h)
    endforeach(SHADER)
endmacro()

macro(SKSHADERC_COMPILE_ASSETS LIST COMMAND_STRING)
    set(SKSHADERC_COMPILE_COMMANDS ${COMMAND_STRING})
    separate_arguments(SKSHADERC_COMPILE_COMMANDS)

    foreach(SHADER ${${LIST}})
        add_custom_command(
            OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/${SHADER}.sks
            COMMAND ${SKSHADERC_EXE_PATH} ${SKSHADERC_COMPILE_COMMANDS} ${CMAKE_CURRENT_SOURCE_DIR}/${SHADER}
            DEPENDS 
                ${SHADER} 
                ${SKSHADERC_EXE_PATH}
            COMMENT "skshaderc ${COMMAND_STRING} ${CMAKE_CURRENT_SOURCE_DIR}/${SHADER}")
    endforeach(SHADER)
endmacro()