# Find packages used by all the tests.
find_package(glm REQUIRED) # Find the GLM vector maths package.
find_package(PNG REQUIRED)
find_package(stb REQUIRED)
find_package(OpenImageIO REQUIRED)
find_package(OpenGL REQUIRED)
find_package(GTest REQUIRED) # Find the GoogleTest unit test package.

find_package(pxr REQUIRED)

# Build list of backends
set(AURORA_BACKENDS_LIST "")
if(ENABLE_DIRECTX_BACKEND)
    list(APPEND AURORA_BACKENDS_LIST \"DirectX\")
endif()
if(ENABLE_HGI_BACKEND)
    list(APPEND AURORA_BACKENDS_LIST \"HGI\")
endif()
set(AURORA_BACKENDS_DEFINE "")

# Convert to string
foreach(ITEM ${AURORA_BACKENDS_LIST})
if(AURORA_BACKENDS_DEFINE STREQUAL "")
    set(AURORA_BACKENDS_DEFINE ${ITEM})
else()
    set(AURORA_BACKENDS_DEFINE "${AURORA_BACKENDS_DEFINE},${ITEM}")
endif()
endforeach()

# Add preprocessor definition for backend list.
add_compile_definitions(AURORA_BACKENDS=${AURORA_BACKENDS_DEFINE})


if(WIN32)
    add_custom_target(CopyOpenImageIODLLs ALL
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:OpenImageIO::OpenImageIO> ${RUNTIME_OUTPUT_DIR}
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:OpenImageIO::OpenImageIO_Util> ${RUNTIME_OUTPUT_DIR}
    )
    set_property(TARGET CopyOpenImageIODLLs PROPERTY FOLDER "Deployment")
endif()

# Enable testing in CMake
enable_testing()

# Add variable to locate the test helpers.
set(TEST_HELPERS_FOLDER ${CMAKE_CURRENT_LIST_DIR}/Helpers)

add_compile_definitions(AURORA_ROOT_PATH=${AURORA_ROOT_DIR})

# Recurse subfolders containing actual tests.
add_subdirectory(Foundation)
add_subdirectory(AuroraInternals)
add_subdirectory(Aurora)
