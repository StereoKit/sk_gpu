cmake_minimum_required(VERSION 3.7)

project(skg_flatscreen VERSION 1.0
                       DESCRIPTION "An sk_gpu.h demo that illustrates usage in a flatscreen app."
                       LANGUAGES CXX)

set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR})

find_package(X11 REQUIRED)

if (NOT DEFINED SKGPU_TEST_USE_GL)
    if (UNIX)
        set(SKGPU_TEST_USE_GL ON)
    else()
        set(SKGPU_TEST_USE_GL OFF)
    endif()
endif()

## Compile shaders

set(SKGPU_TEST_SHADERS_HLSL
    ../common/compute_test.hlsl
    ../common/cubemap.hlsl
    ../common/test.hlsl )
skshaderc_compile_headers(SKGPU_TEST_SHADERS_HLSL "-t xge" SKGPU_TEST_SHADERS_H)

## Target definition

add_executable(skg_flatscreen
    main.cpp
    ../common/app.h
    ../common/app.cpp
    ../common/micro_ply.h
    ../common/HandmadeMath.h
    ../common/xr_app.h
    ../../src/sk_gpu_dev.h
    ../../src/sk_gpu_common.h
    ../../src/sk_gpu_common.cpp
    ../../src/sk_gpu_dx11.h
    ../../src/sk_gpu_dx11.cpp
    ../../src/sk_gpu_gl.h
    ../../src/sk_gpu_gl.cpp
    ${SKGPU_TEST_SHADERS_H} )

target_include_directories(skg_flatscreen PRIVATE
    ../common
    ../../src)

if (SKGPU_TEST_USE_GL)
    if (UNIX)
        set( GL_LIBRARIES
            ${X11_LIBRARIES}
            dl
            GL
            GLX)
    elseif(WIN32)
        target_compile_definitions(skg_flatscreen PUBLIC SKG_FORCE_OPENGL)
    endif()
endif()

target_link_libraries(skg_flatscreen
    ${GL_LIBRARIES})