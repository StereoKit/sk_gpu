cmake_minimum_required(VERSION 3.7)

project(sk_gpu VERSION 0.1
               DESCRIPTION "Single header cross-platform graphics api."
               LANGUAGES CXX)

add_subdirectory(src)
add_subdirectory(examples/sk_gpu_flat)