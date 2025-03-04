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
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR})

project(skshaderc VERSION 1.0
                  DESCRIPTION "Shader compiler for sk_gpu.h"
                  LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)

if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "AppleClang")
    add_compile_options(-fcolor-diagnostics)
endif()

# See: https://github.com/cpm-cmake/CPM.cmake
include(../cmake/CPM.cmake)

# For converting SPIR-V to flavors of GLSL
CPMAddPackage(
    NAME SPIRV-Cross
    GITHUB_REPOSITORY KhronosGroup/SPIRV-Cross
    GIT_TAG vulkan-sdk-1.3.296.0
    OPTIONS 
    "SPIRV_CROSS_CLI OFF" 
    "SPIRV_CROSS_ENABLE_TESTS OFF"
    "SPIRV_CROSS_ENABLE_MSL OFF"
    "SPIRV_CROSS_ENABLE_CPP OFF"
    "SPIRV_CROSS_ENABLE_REFLECT OFF"
    "SPIRV_CROSS_ENABLE_C_API OFF"
    "SPIRV_CROSS_ENABLE_UTIL OFF"
)

# For SPIRV-Tools
CPMAddPackage(
    NAME SPIRV-Headers
    GITHUB_REPOSITORY KhronosGroup/SPIRV-Headers
    GIT_TAG vulkan-sdk-1.3.296.0 # This version should match up with the SPIRV-Tools version
)

# For optimizing SPIR-V shaders, a baseline amount of
# optimization is crucial for meta compatability with
# HLSL compilers.
CPMAddPackage(
    NAME SPIRV-Tools
    GITHUB_REPOSITORY KhronosGroup/SPIRV-Tools
    GIT_TAG vulkan-sdk-1.3.296.0 # This version should match up with the SPIRV-Headers version
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
    GIT_TAG vulkan-sdk-1.3.296.0
    OPTIONS 
    "GLSLANG_ENABLE_INSTALL OFF"
    "GLSLANG_TESTS OFF"
    "ENABLE_GLSLANG_BINARIES OFF"
    "ENABLE_SPVREMAPPER OFF"
    "ENABLE_GLSLANG_JS OFF"
    "ENABLE_RTTI OFF"
)

if (UNIX)
    set(LINUX_LIBS)
endif()

add_executable(skshaderc
    main.cpp
    sksc.cpp
    sksc.h
    miniz.cpp
    miniz.h
)
set(SKSHADERC_EXE_NAME "skshaderc_exe")
set_target_properties(skshaderc PROPERTIES OUTPUT_NAME ${SKSHADERC_EXE_NAME})

target_link_libraries(skshaderc
    PRIVATE
    spirv-cross-core
    spirv-cross-glsl
    spirv-cross-hlsl
    SPIRV-Tools-opt
    glslang
    SPIRV
    ${LINUX_LIBS}
)
add_dependencies(skshaderc sk_gpu_header)


if (NOT MSVC AND NOT APPLE)
    add_custom_command(TARGET skshaderc POST_BUILD
        COMMAND ${CMAKE_OBJCOPY} --only-keep-debug $<TARGET_FILE:skshaderc> $<TARGET_FILE_DIR:skshaderc>/$<TARGET_FILE_BASE_NAME:skshaderc>.dbg
        COMMAND ${CMAKE_STRIP} --strip-debug $<TARGET_FILE:skshaderc>
        COMMAND ${CMAKE_OBJCOPY} --add-gnu-debuglink=$<TARGET_FILE_DIR:skshaderc>/$<TARGET_FILE_BASE_NAME:skshaderc>.dbg $<TARGET_FILE:skshaderc>
    )
endif()

set(SKSHADERC_EXE_PATH $<TARGET_FILE:skshaderc> PARENT_SCOPE)

# functions for compiling shaders easily
function(SKSHADERC_COMPILE_ASSETS TARGET COMMAND_STRING OUT_LIST)
    message(STATUS "sk_gpu compiling shader assets with args '${SKSHADERC_EXE_PATH} ${COMMAND_STRING}'")
    set(SKSHADERC_COMPILE_COMMANDS ${COMMAND_STRING})
    separate_arguments(SKSHADERC_COMPILE_COMMANDS)

    set(SHADER_LIST)
    foreach(SHADER IN LISTS ARGN)
        add_custom_command(
            TARGET ${TARGET} PRE_BUILD
            COMMAND ${SKSHADERC_EXE_PATH} ${SKSHADERC_COMPILE_COMMANDS} ${CMAKE_CURRENT_SOURCE_DIR}/${SHADER}
            VERBATIM
        )
        list(APPEND SHADER_LIST ${SHADER})
    endforeach(SHADER)

    set(${OUT_LIST} ${SHADER_LIST} PARENT_SCOPE)
endfunction()

function(SKSHADERC_COMPILE_HEADERS ADD_TARGET OUTPUT_FOLDER COMMAND_STRING)
    set(SKSHADERC_COMPILE_COMMANDS "-h -o ${OUTPUT_FOLDER} ${COMMAND_STRING}")
    message(STATUS "sk_gpu compiling shader headers with args '${SKSHADERC_EXE_PATH} ${SKSHADERC_COMPILE_COMMANDS}'")
    separate_arguments(SKSHADERC_COMPILE_COMMANDS)

    set(SHADER_LIST)
    foreach(SHADER IN LISTS ARGN)
        get_filename_component(SHADER_NAME ${SHADER} NAME)
        add_custom_command(
            OUTPUT ${OUTPUT_FOLDER}/${SHADER_NAME}.h
            COMMAND ${SKSHADERC_EXE_PATH} ${SKSHADERC_COMPILE_COMMANDS} ${CMAKE_CURRENT_SOURCE_DIR}/${SHADER}
            VERBATIM
        )
        list(APPEND SHADER_LIST ${OUTPUT_FOLDER}/${SHADER_NAME}.h)
    endforeach(SHADER)

    target_include_directories(${ADD_TARGET} PRIVATE ${OUTPUT_FOLDER})
    target_sources            (${ADD_TARGET} PRIVATE ${SHADER_LIST})
endfunction()