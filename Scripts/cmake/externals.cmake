# Configure the search paths of find_package()

# All externals will be properly found by find_package() with the following setup.
if(DEFINED EXTERNALS_DIR)
    # We need to use get_filename_component to get rid of the trailing separator
    get_filename_component(EXTERNALS_DIR ${EXTERNALS_DIR} ABSOLUTE)
    cmake_path(GET EXTERNALS_DIR FILENAME BUILD_TYPE_SUBFOLDER)

    # On Linux, the build type of externals is not required to match the build type
    # of Aurora. We want to allow users to explicitly choose the build type of
    # externals.
    if(BUILD_TYPE_SUBFOLDER STREQUAL "Debug" OR BUILD_TYPE_SUBFOLDER STREQUAL "Release")
        message(STATUS "Build type of EXTERNALS_DIR is specified: ${BUILD_TYPE_SUBFOLDER}")
        message(STATUS "Using externals from ${EXTERNALS_DIR}")
    else()
        message(STATUS "Build type of EXTERNALS_DIR is not specified.")
        message(STATUS "Using externals from ${EXTERNALS_DIR}/${CMAKE_BUILD_TYPE} as CMAKE_BUILD_TYPE is ${CMAKE_BUILD_TYPE}")

        # If the user does not specify the build type of externals, we will append the
        # the build type with the build type of Aurora
        cmake_path(APPEND EXTERNALS_DIR ${CMAKE_BUILD_TYPE})
    endif()

    if(NOT IS_DIRECTORY "${EXTERNALS_DIR}")
        message(FATAL_ERROR "EXTERNALS_DIR does not exist: ${EXTERNALS_DIR}")
    endif()

    list(APPEND CMAKE_PREFIX_PATH "${EXTERNALS_DIR}")

    if(NOT DEFINED NRI_ROOT)
        set(NRI_ROOT "${EXTERNALS_DIR}/NRI")
    endif()

    if(NOT DEFINED NRD_ROOT)
        set(NRD_ROOT "${EXTERNALS_DIR}/NRD")
    endif()

    if(NOT DEFINED Slang_ROOT)
        set(Slang_ROOT "${EXTERNALS_DIR}/Slang")
    endif()

    # GLM cmake config file is not coded properly, we have to set GLM_DIR pointed
    # to the dir that contains glmConfig.cmake
    if(NOT DEFINED glm_ROOT AND NOT DEFINED glm_DIR)
        set(glm_DIR "${EXTERNALS_DIR}/cmake/glm")
    endif()
endif()

# If you want to use you own build of certain external library, simply set <pkg>_ROOT or <pkg>_DIR
# to guide find_package() to locate your own build. External libraries used by Aurora are:
#     pxr
#     MaterialX
#     NRI
#     NRD
#     OpenImageIO
#     glm
#     Slang
#     stb
#     PNG
#     ZLIB
#     TinyGLTF
#     tinyobjloader
#     glew
#     glfw3
#     cxxopts
#     GTest

# Debug finding the external libraries
# find_package_verbose(D3D12 REQUIRED)
# find_package_verbose(pxr REQUIRED)
# find_package_verbose(MaterialX REQUIRED)
# find_package_verbose(NRI REQUIRED)
# find_package_verbose(NRD REQUIRED)
# find_package_verbose(OpenImageIO REQUIRED)
# find_package_verbose(Slang REQUIRED)
# find_package_verbose(glm REQUIRED)
# find_package_verbose(stb REQUIRED)
# find_package_verbose(PNG REQUIRED)
# find_package_verbose(ZLIB REQUIRED)
# find_package_verbose(TinyGLTF REQUIRED)
# find_package_verbose(tinyobjloader REQUIRED)
# find_package_verbose(GLEW REQUIRED)
# find_package_verbose(glfw3 REQUIRED)
# find_package_verbose(cxxopts REQUIRED)
# find_package_verbose(GTest REQUIRED)
