cmake_minimum_required(VERSION 3.24)
# 3.24 is required to have Vulkan::shaderc_combined


# Forbid the in-source build
if(${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})
    message(FATAL_ERROR “In-source build is not allowed!”)
endif()

# Set the project name and project variables
project(Aurora VERSION 0.0.0.1)

set(AURORA_ROOT_DIR ${PROJECT_SOURCE_DIR})

# Set the default output directories
set(RUNTIME_OUTPUT_DIR "${PROJECT_BINARY_DIR}/bin/$<CONFIG>")
set(LIBRARY_OUTPUT_DIR "${PROJECT_BINARY_DIR}/lib/$<CONFIG>")

# Set scripts folder.
set(SCRIPTS_DIR ${AURORA_ROOT_DIR}/Scripts)

# Add the search path of CMake modules
list(APPEND CMAKE_MODULE_PATH
    "${SCRIPTS_DIR}/cmake"
    "${SCRIPTS_DIR}/cmake/modules"
)

# Import the cmake utility functions
include(toolbox)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Default build type: Release" FORCE)
endif()

# The default compile definition used by all packages.
set(DEFAULT_COMPILE_DEFINITIONS NOMINMAX UNICODE _USE_MATH_DEFINES _HAS_STD_BYTE=0)

# Enable folders for project code organisation in IDE.
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# specify the C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)


if(MSVC)
    # Add warning level 4 and warnings as errors
    add_compile_options(/W4 /WX -wd4068) # -wd4068 disables unknown-pragmas warnings
    # Enable multiple-processor compilation
    add_compile_options(/MP)
else()
    # Enables strict standard conformance and warning as errors
    add_compile_options(-Wall -Wextra -Wpedantic -Werror -Wno-unknown-pragmas -Wno-gnu-zero-variadic-macro-arguments)
endif()

include(externalsConfig OPTIONAL RESULT_VARIABLE EXTERNALS_CONFIG)
if (NOT EXTERNALS_CONFIG AND DEFINED EXTERNALS_ROOT)
    include(externalsDefaultConfig RESULT_VARIABLE EXTERNALS_CONFIG)
endif()
if (EXTERNALS_CONFIG)
    message(STATUS "Load externals config file: ${EXTERNALS_CONFIG}")
else()
    message(FATAL_ERROR "Failed to load externals config file. If you have run `installExternals.py` from an early release, please re-run the script to generate the externals and the config file. Run `python Scripts/installExternals.py -h` to learn more options.")
endif()

#Setup the backend flags used by Aurora libray and tests.
if(WIN32)
    option(ENABLE_DIRECTX_BACKEND "Build with DirectX renderer backend." ON)
    option(ENABLE_HGI_BACKEND "Build with HGI renderer backend (requires Vulkan SDK)." OFF)
else()
    # Always disable DirectX backend on non-Windows platforms
    set(ENABLE_DIRECTX_BACKEND OFF)
    # Always enable HGI backend on non-Windows platforms
    set(ENABLE_HGI_BACKEND ON)
endif()

add_subdirectory(Libraries)
add_subdirectory(Tests)
add_subdirectory(Applications)
