cmake_minimum_required(VERSION 3.7)

project(skg_flatscreen VERSION 1.0
                       DESCRIPTION "An sk_gpu.h demo that illustrates usage in a flatscreen app."
                       LANGUAGES CXX)

set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR})

find_package(X11 REQUIRED)

add_executable(skg_flatscreen
    main.cpp
    ../common/app.h
    ../common/app.cpp
    ../common/compute_test.hlsl.h
    ../common/cubemap.hlsl.h
    ../common/test.hlsl.h
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
    )

target_include_directories(skg_flatscreen PRIVATE
    ../common
    ../../src)

target_link_libraries(skg_flatscreen
    ${X11_LIBRARIES}
    GL
    GLEW
    GLX)