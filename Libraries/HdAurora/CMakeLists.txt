# Specify the library (for dependents), project (IDE), and output (binary file) names.
project(hdAurora)

find_package(glm REQUIRED) # Find the GLM vector maths package.
find_package(OpenGL REQUIRED)
find_package(Vulkan REQUIRED)
find_package(GLEW REQUIRED)

find_package(pxr REQUIRED) # Find the Universal Scene Description (USD) library

# Add shard library with all source files.
add_library(${PROJECT_NAME} SHARED
    "DLL.cpp"
    "HdAuroraImageCache.cpp"
    "HdAuroraImageCache.h"
    "HdAuroraCamera.cpp"
    "HdAuroraCamera.h"
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
)

# Set custom ouput properties.
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
    glm::glm
    OpenGL::GL
    GLEW::glew
    pxr::usd
    pxr::hf
    pxr::hd
    pxr::hdx
    Foundation
    Aurora
)

# Add default compile definitions (set in root CMakefile)
target_compile_definitions(${PROJECT_NAME} PRIVATE ${DEFAULT_COMPILE_DEFINITIONS})