project(Plasma)

if(WIN32)
    option(ENABLE_INTERACTIVE_PLASMA "Build Plasma as an interactive viewer." ON)
else()
    set(ENABLE_INTERACTIVE_PLASMA OFF)
endif()

# Find packages used by Plasma application.
find_package(glm REQUIRED) # Find the GLM vector maths package.
find_package(cxxopts REQUIRED)
find_package(stb REQUIRED)
find_package(TinyGLTF REQUIRED)
find_package(tinyobjloader REQUIRED)

# Add test executable with all source files.
add_executable(${PROJECT_NAME}
    "Camera.cpp"
    "Camera.h"
    "Plasma.cpp"
    "Plasma.h"
    "glTFLoader.cpp"
    "Libraries.cpp"
    "Loaders.h"
    "OBJLoader.cpp"
    "pch.h"
    "PerformanceMonitor.h"
    "resource.h"
    "SceneContents.cpp"
    "SceneContents.h"
)

# Set custom ouput properties.
set_target_properties(${PROJECT_NAME} PROPERTIES
    FOLDER "Applications"
    RUNTIME_OUTPUT_DIRECTORY "${RUNTIME_OUTPUT_DIR}"
    LIBRARY_OUTPUT_DIRECTORY "${LIBRARY_OUTPUT_DIR}"
    ARCHIVE_OUTPUT_DIRECTORY "${LIBRARY_OUTPUT_DIR}"
    PDB_OUTPUT_DIRECTORY "${RUNTIME_OUTPUT_DIR}"
)

if(WIN32 AND ENABLE_INTERACTIVE_PLASMA)
    set(WINSDK_LIB Pathcch.lib Shlwapi.lib)
else()
    set(WINSDK_LIB "")
endif()

# Add dependencies.
target_link_libraries(${PROJECT_NAME}
PRIVATE
    glm::glm
    cxxopts::cxxopts
    stb::stb
    tinyobjloader::tinyobjloader
    Foundation
    Aurora
    ${WINSDK_LIB}
    ${CMAKE_DL_LIBS}
)

target_include_directories(${PROJECT_NAME}
PRIVATE
    "${TinyGLTF_INCLUDE_DIR}"
)

if(WIN32 AND ENABLE_INTERACTIVE_PLASMA)
    # set windows-specific properties including WIN32 executable (gui app)
    set_target_properties(${PROJECT_NAME} PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

# Add default compile definitions (set in root CMakefile)
if(ENABLE_INTERACTIVE_PLASMA)
    set(PLASMA_DEFINITIONS INTERACTIVE_PLASMA)
else()
    set(PLASMA_DEFINITIONS "")
endif()
target_compile_definitions(${PROJECT_NAME}
    PRIVATE
        ${DEFAULT_COMPILE_DEFINITIONS}
        ${PLASMA_DEFINITIONS}
)
