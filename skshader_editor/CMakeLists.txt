cmake_minimum_required(VERSION 3.7)

project(skshader_editor VERSION 1.0
                        DESCRIPTION "A shader editor with a GUI!"
                        LANGUAGES CXX)

set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR})
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

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
    "SPIRV_CROSS_ENABLE_CPP OFF"
    "SPIRV_CROSS_ENABLE_REFLECT OFF"
    "SPIRV_CROSS_ENABLE_UTIL OFF"
    "SPIRV_CROSS_SKIP_INSTALL ON"
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
    "SKIP_SPIRV_TOOLS_INSTALL OFF"
    "SPIRV_CHECK_CONTEXT OFF"
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

## Target definition

add_executable(skshader_editor
    main.cpp
    app_shader.cpp
    imgui_impl_skg.cpp
    imgui_impl_win32.cpp
    imgui/imgui_demo.cpp
    imgui/imgui_draw.cpp
    imgui/imgui_widgets.cpp
    imgui/imgui.cpp
    ../skshaderc/sksc.cpp
    TextEditor.cpp)

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/CascadiaMono.ttf 
    ${CMAKE_BINARY_DIR}/${CMAKE_BUILD_TYPE}/CascadiaMono.ttf COPYONLY)

if (UNIX)
    set( GL_LIBRARIES
        ${X11_LIBRARIES}
        dl
        GL
        GLX)
endif()

target_link_libraries(skshader_editor
    PRIVATE
    spirv-cross-c
    SPIRV-Tools-opt
    glslang
    SPIRV
    ${GL_LIBRARIES})