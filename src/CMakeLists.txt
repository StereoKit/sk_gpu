cmake_minimum_required(VERSION 3.7)

project(sk_gpu_header VERSION 0.1
                      DESCRIPTION "Single header cross-platform graphics api."
                      LANGUAGES CXX)

find_package(PythonInterp REQUIRED)

add_custom_command(
    OUTPUT "${CMAKE_CURRENT_SOURCE_DIR}/../sk_gpu.h"
    COMMAND "${PYTHON_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/build_header.py" --argument
    DEPENDS 
        ${CMAKE_CURRENT_SOURCE_DIR}/sk_gpu_common.h
        ${CMAKE_CURRENT_SOURCE_DIR}/sk_gpu_common.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/sk_gpu_dev.h
        ${CMAKE_CURRENT_SOURCE_DIR}/sk_gpu_dx11.h
        ${CMAKE_CURRENT_SOURCE_DIR}/sk_gpu_dx11.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/sk_gpu_gl.h
        ${CMAKE_CURRENT_SOURCE_DIR}/sk_gpu_gl.cpp
)

add_custom_target(sk_gpu_header ALL
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/../sk_gpu.h")

install(FILES ${CMAKE_CURRENT_SOURCE_DIR}../sk_gpu.h DESTINATION include)