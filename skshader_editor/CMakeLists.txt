cmake_minimum_required(VERSION 3.14)

project(skshader_editor VERSION 1.0
                        DESCRIPTION "A shader editor with a GUI!"
                        LANGUAGES CXX)


set(CMAKE_CXX_STANDARD 17)

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
    GIT_TAG vulkan-sdk-1.3.275.0
    OPTIONS 
    "SPIRV_CROSS_CLI OFF" 
    "SPIRV_CROSS_ENABLE_TESTS OFF" 
    "SPIRV_CROSS_ENABLE_MSL OFF"
)

# For SPIRV-Tools
CPMAddPackage(
    NAME SPIRV-Headers
    GITHUB_REPOSITORY KhronosGroup/SPIRV-Headers
    GIT_TAG vulkan-sdk-1.3.275.0 # This version should match up with the SPIRV-Tools version
)

# For optimizing SPIR-V shaders, a baseline amount of
# optimization is crucial for meta compatability with
# HLSL compilers.
CPMAddPackage(
    NAME SPIRV-Tools
    GITHUB_REPOSITORY KhronosGroup/SPIRV-Tools
    GIT_TAG vulkan-sdk-1.3.275.0 # This version should match up with the SPIRV-Headers version
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
    GIT_TAG 14.1.0
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
    ../skshaderc/sksc_glsl.cpp
    ../skshaderc/sksc_hlsl.cpp
    ../skshaderc/sksc_log.cpp
    ../skshaderc/sksc_meta.cpp
    TextEditor.cpp)

add_custom_command(TARGET skshader_editor POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/CascadiaMono.ttf ${CMAKE_BINARY_DIR}/${CMAKE_BUILD_TYPE}/CascadiaMono.ttf
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/CascadiaMono.ttf ${CMAKE_BINARY_DIR}/${PROJECT_NAME}/CascadiaMono.ttf
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/test.png ${CMAKE_BINARY_DIR}/${CMAKE_BUILD_TYPE}/test.png
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/test.png ${CMAKE_BINARY_DIR}/${PROJECT_NAME}/test.png
)

if (UNIX)
    set( GL_LIBRARIES
        ${X11_LIBRARIES}
        dl
        GL
        GLX)
endif()

target_link_libraries(skshader_editor
    PRIVATE
    spirv-cross-hlsl
    SPIRV-Tools-opt
    glslang
    SPIRV
    ${GL_LIBRARIES})
add_dependencies(skshader_editor
    sk_gpu_header
    skshaderc)

skshaderc_compile_headers(skshader_editor
    ${CMAKE_BINARY_DIR}/shaders_editor/
    "-e -t xge"
    imgui_shader.hlsl )