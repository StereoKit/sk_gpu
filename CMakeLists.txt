cmake_minimum_required(VERSION 3.12)

project(sk_gpu VERSION 0.1
               DESCRIPTION "Single header cross-platform graphics api."
               LANGUAGES CXX)

# Make switches for each of the subdirectories
option(SK_BUILD_SHADERC  "Build the shader compiler" ON)
option(SK_BUILD_EDITOR   "Build the shader editor" ON)
option(SK_BUILD_EXAMPLES "Build the examples" ON)

add_subdirectory(src)
if (SK_BUILD_SHADERC)
    add_subdirectory(skshaderc)
endif()
if (SK_BUILD_EDITOR)
    add_subdirectory(skshader_editor)
endif()
if (SK_BUILD_EXAMPLES)
    add_subdirectory(examples/sk_gpu_flat)
endif()

include(CMakePackageConfigHelpers)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/sk_gpuConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY AnyNewerVersion
)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/sk_gpuConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/sk_gpuConfig.cmake"
    INSTALL_DESTINATION lib/cmake/sk_gpu
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/sk_gpuConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/sk_gpuConfigVersion.cmake"
    DESTINATION lib/cmake/sk_gpu
)
