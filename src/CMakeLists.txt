cmake_minimum_required(VERSION 3.7)
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR})

project(sk_gpu_header VERSION 0.1
                      DESCRIPTION "Single header cross-platform graphics api."
                      LANGUAGES CXX)

find_package(PythonInterp REQUIRED)

add_custom_command(
    OUTPUT "${CMAKE_CURRENT_SOURCE_DIR}/../sk_gpu.h"
    COMMAND "${PYTHON_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/build_header.py" --argument
    DEPENDS
        sk_gpu_dev.h
        sk_gpu_common.h
        sk_gpu_common.cpp
        sk_gpu_dx11.h
        sk_gpu_dx11.cpp
        sk_gpu_gl.h
        sk_gpu_gl.cpp
        sk_gpu_null.h
        sk_gpu_null.cpp
)

add_custom_target(sk_gpu_header ALL
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/../sk_gpu.h"
)

add_library(sk_gpu INTERFACE)

add_dependencies(sk_gpu sk_gpu_header)

target_include_directories(sk_gpu INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    $<INSTALL_INTERFACE:include>
)

install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/../sk_gpu.h"
        DESTINATION include)

install(TARGETS sk_gpu
        EXPORT sk_gpuTargets
        INCLUDES DESTINATION include)

install(EXPORT sk_gpuTargets
        FILE sk_gpuTargets.cmake
        NAMESPACE sk_gpu::
        DESTINATION lib/cmake/sk_gpu)

install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/../cmake/sk_gpuFunctions.cmake"
    DESTINATION lib/cmake/sk_gpu)
