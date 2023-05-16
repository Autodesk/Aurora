# Specify the library (for dependents), project (IDE), and output (binary file) names.
project(hdAurora)

find_package(glm REQUIRED) # Find the GLM vector maths package.
find_package(OpenGL REQUIRED)
find_package(GLEW REQUIRED)
find_package(MaterialX REQUIRED) # MaterialX SDK
find_package(pxr REQUIRED) # Find the Universal Scene Description (USD) library
# Alias the namespace to meet the cmake convention on imported targets
add_library(MaterialX::GenGlsl ALIAS MaterialXGenGlsl)

# On windows, the version resource and header files that add the version information to the DLL.
if(WIN32)
set(VERSION_FILES
    ${VERSION_FOLDER}/AuroraVersion.h
    HdAurora.rc
)
endif()

# Add shard library with all source files.
add_library(${PROJECT_NAME} SHARED
    "DLL.cpp"
    "HdAuroraImageCache.cpp"
    "HdAuroraImageCache.h"
    "HdAuroraInstancer.cpp"
    "HdAuroraInstancer.h"
    "HdAuroraLight.cpp"
    "HdAuroraLight.h"
    "HdAuroraMaterial.cpp"
    "HdAuroraMaterial.h"
    "HdAuroraMesh.cpp"
    "HdAuroraMesh.h"
    "HdAuroraPlugin.cpp"
    "HdAuroraPlugin.h"
    "HdAuroraRenderBuffer.cpp"
    "HdAuroraRenderBuffer.h"
    "HdAuroraRenderDelegate.cpp"
    "HdAuroraRenderDelegate.h"
    "HdAuroraRenderPass.cpp"
    "HdAuroraRenderPass.h"
    "HdAuroraTokens.h"
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

    # Follow the USD convention of lower-case initial on libraries.
    OUTPUT_NAME "hdAurora"
)

# Add dependencies.
target_link_libraries(${PROJECT_NAME}
PRIVATE
    glm::glm
    OpenGL::GL
    GLEW::glew
    MaterialX::GenGlsl
    pxr::usd
    pxr::hf
    pxr::hd
    pxr::hdx
    Foundation
    Aurora
)

# Add default compile definitions (set in root CMakefile)
target_compile_definitions(${PROJECT_NAME} PRIVATE ${DEFAULT_COMPILE_DEFINITIONS})

if(WIN32)
    # Create a custom target that will copy USD executables to the runtime folder.
    file(GLOB USD_EXECUTABLES ${pxr_ROOT}/bin/*)
    set(USD_PYTHON_LIB_DIR "${pxr_ROOT}/lib/python")
    set(USD_LIB_PLUGIN_DIR "${pxr_ROOT}/lib/usd")
    set(USD_PLUGIN_DIR "${pxr_ROOT}/plugin/usd")
    add_custom_target(CopyUSDExecutables ALL
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${USD_EXECUTABLES} ${RUNTIME_OUTPUT_DIR}
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${USD_LIB_PLUGIN_DIR} ${RUNTIME_OUTPUT_DIR}/usd
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${USD_PLUGIN_DIR} ${RUNTIME_OUTPUT_DIR}/usd
    )
    set_property(TARGET CopyUSDExecutables PROPERTY FOLDER "Deployment")
    add_dependencies(CopyUSDExecutables MakeRuntimeDir)
    add_dependencies(${PROJECT_NAME} CopyUSDExecutables)

    if(ENABLE_USDVIEW)
        add_custom_target(CopyUSDPythonExecutables ALL
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${USD_PYTHON_LIB_DIR} ${RUNTIME_OUTPUT_DIR}/python
        )
        set_property(TARGET CopyUSDPythonExecutables PROPERTY FOLDER "Deployment")
        add_dependencies(CopyUSDPythonExecutables MakeRuntimeDir)
        add_dependencies(${PROJECT_NAME} CopyUSDPythonExecutables)
    endif()

    set(HDAURORA_PLUGIN_RESOURCES_DIR ${CMAKE_CURRENT_SOURCE_DIR}/resources)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:${PROJECT_NAME}> $<$<CONFIG:Debug>:$<TARGET_PDB_FILE:${PROJECT_NAME}>> ${RUNTIME_OUTPUT_DIR}/usd
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${HDAURORA_PLUGIN_RESOURCES_DIR} ${RUNTIME_OUTPUT_DIR}/usd/hdAurora/resources
    )

else() # Linux
    #TODO anything we need to set to support the Aurora execution?
endif()

# Add the version folder required by HdAurora.rc
target_include_directories(${PROJECT_NAME}
PRIVATE
    ${VERSION_FOLDER}
)

# Install the binaries.
install(TARGETS ${PROJECT_NAME}  DESTINATION ${INSTALL_BIN}/usd)
install(FILES "resources/plugInfo.json" DESTINATION ${INSTALL_BIN}/usd/hdAurora/resources)