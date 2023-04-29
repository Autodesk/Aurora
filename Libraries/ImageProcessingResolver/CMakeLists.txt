# Specify the library (for dependents), project (IDE), and output (binary file) names.
project(ImageProcessingResolver)

# Add dependent packages.
find_package(glm REQUIRED) # Find the GLM vector maths package.
find_package(stb REQUIRED) # Find the Universal Scene Description (USD) library
find_package(pxr REQUIRED) # Find the Universal Scene Description (USD) library
find_package(OpenImageIO REQUIRED)
find_package(miniz REQUIRED)
find_package(uriparser REQUIRED)

# On windows, the version resource and header files that add the version information to the DLL.
if(WIN32)
set(VERSION_FILES
    ${VERSION_FOLDER}/AuroraVersion.h
    ImageProcessingResolver.rc
)
endif()

# Add shared library with all source files.
add_library(${PROJECT_NAME} SHARED
    "DLL.cpp"
    "Resolver.cpp"
    "Resolver.h"
    "Linearize.cpp"
    "Linearize.h"
    "ConvertEnvMapLayout.cpp"
    "ConvertEnvMapLayout.h"
    "pch.h"
    ${VERSION_FILES}
)

# Set custom output properties.
set_target_properties(${PROJECT_NAME} PROPERTIES
	FOLDER "Libraries"
    RUNTIME_OUTPUT_DIRECTORY "${RUNTIME_OUTPUT_DIR}"
    LIBRARY_OUTPUT_DIRECTORY "${LIBRARY_OUTPUT_DIR}"
    ARCHIVE_OUTPUT_DIRECTORY "${LIBRARY_OUTPUT_DIR}"
    PDB_OUTPUT_DIRECTORY "${RUNTIME_OUTPUT_DIR}"
)

# Add dependencies.
target_link_libraries(${PROJECT_NAME}
PRIVATE
    uriparser::uriparser
    miniz::miniz
    glm::glm
    stb::stb
    OpenImageIO::OpenImageIO
    pxr::usd
    pxr::hf
    pxr::hd
    pxr::hdx
    Foundation
    Aurora
)

# Add OpenEXR to the include path.
target_include_directories(${PROJECT_NAME}
PRIVATE
    "${tinyexr_ROOT}/include"
    ${VERSION_FOLDER}
)

# Add default compile definitions (set in root CMakefile)
target_compile_definitions(${PROJECT_NAME} PRIVATE ${DEFAULT_COMPILE_DEFINITIONS})

if(WIN32)
    # Create post-build step to copy to USD folder.
    set(IMAGEPROCESSINGRESOLVER_PLUGIN_RESOURCES_DIR ${CMAKE_CURRENT_SOURCE_DIR}/resources)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:${PROJECT_NAME}> $<$<CONFIG:Debug>:$<TARGET_PDB_FILE:${PROJECT_NAME}>> ${RUNTIME_OUTPUT_DIR}/usd
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${IMAGEPROCESSINGRESOLVER_PLUGIN_RESOURCES_DIR} ${RUNTIME_OUTPUT_DIR}/usd/ImageProcessingResolver/resources
    )
endif()

# Install the binaries.
install(TARGETS ${PROJECT_NAME}  DESTINATION ${INSTALL_BIN}/usd)
install(FILES "resources/plugInfo.json" DESTINATION ${INSTALL_BIN}/usd/ImageProcessingResolver/resources)