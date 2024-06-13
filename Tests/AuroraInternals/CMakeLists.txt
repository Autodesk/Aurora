project(AuroraInternalsTests)

# Add the preprocessor definition for MaterialX flag.
add_compile_definitions(ENABLE_MATERIALX=1)

find_package(MaterialX REQUIRED) # MaterialX SDK
# Alias the namespace to meet the cmake convention on imported targets
add_library(MaterialX::GenGlsl ALIAS MaterialXGenGlsl)

# List of common helper files shared by all tests. TEST_HELPERS_FOLDER variable is set in parent cmake file.
set(HELPER_FILES
    "${TEST_HELPERS_FOLDER}/TestHelpers.cpp"
    "${TEST_HELPERS_FOLDER}/TestHelpers.h"
)

# List of actual test files.
set(TEST_FILES
    "Common/TestAssetManager.cpp"
    "Common/TestProperties.cpp"
    "Common/TestResources.cpp"
    "Common/TestMaterialGenerator.cpp"
    "Common/TestUniformBuffer.cpp"
)

set(AURORA_DIR "${CMAKE_SOURCE_DIR}/Libraries/Aurora")

set(AURORA_FILES
    "${AURORA_DIR}/Source/AssetManager.cpp"
    "${AURORA_DIR}/Source/AssetManager.h"
    "${AURORA_DIR}/Source/DLL.cpp"
    "${AURORA_DIR}/Source/EnvironmentBase.cpp"
    "${AURORA_DIR}/Source/EnvironmentBase.h"
    "${AURORA_DIR}/Source/MaterialBase.cpp"
    "${AURORA_DIR}/Source/MaterialBase.h"
    "${AURORA_DIR}/Source/MaterialDefinition.cpp"
    "${AURORA_DIR}/Source/MaterialDefinition.h"
    "${AURORA_DIR}/Source/MaterialShader.cpp"
    "${AURORA_DIR}/Source/MaterialShader.h"
    "${AURORA_DIR}/Source/pch.h"
    "${AURORA_DIR}/Source/Properties.h"
    "${AURORA_DIR}/Source/RendererBase.cpp"
    "${AURORA_DIR}/Source/RendererBase.h"
    "${AURORA_DIR}/Source/Resources.cpp"
    "${AURORA_DIR}/Source/Resources.h"
    "${AURORA_DIR}/Source/ResourceStub.cpp"
    "${AURORA_DIR}/Source/ResourceStub.h"
    "${AURORA_DIR}/Source/SceneBase.cpp"
    "${AURORA_DIR}/Source/SceneBase.h"
    "${AURORA_DIR}/Source/UniformBuffer.cpp"
    "${AURORA_DIR}/Source/UniformBuffer.h"
    "${AURORA_DIR}/Source/Aurora.cpp"
    "${AURORA_DIR}/Source/AuroraNames.cpp"
    "${AURORA_DIR}/Source/WindowsHeaders.h"
    "${AURORA_DIR}/Source/MaterialX/MaterialGenerator.h"
    "${AURORA_DIR}/Source/MaterialX/MaterialGenerator.cpp"
    "${AURORA_DIR}/Source/MaterialX/BSDFCodeGenerator.h"
    "${AURORA_DIR}/Source/MaterialX/BSDFCodeGenerator.cpp"
)


# Add test executable with all source files.
add_executable(${PROJECT_NAME}
    ${HELPER_FILES}
    ${TEST_FILES}
    ${AURORA_FILES}
    "AuroraInternalsMain.cpp"
)

# Put test files and helpers in seperate folders.
source_group("Helpers" FILES ${HELPER_FILES})
source_group("Tests" FILES ${TEST_FILES})
source_group("Aurora" FILES ${AURORA_FILES})

# Set custom output properties.
set_target_properties(${PROJECT_NAME} PROPERTIES
	FOLDER "Tests"
    RUNTIME_OUTPUT_DIRECTORY "${RUNTIME_OUTPUT_DIR}"
    LIBRARY_OUTPUT_DIRECTORY "${LIBRARY_OUTPUT_DIR}"
    ARCHIVE_OUTPUT_DIRECTORY "${LIBRARY_OUTPUT_DIR}"
    PDB_OUTPUT_DIRECTORY "${RUNTIME_OUTPUT_DIR}"
    BUILD_WITH_INSTALL_RPATH TRUE
    INSTALL_RPATH_USE_LINK_PATH TRUE
    INSTALL_RPATH "${TBB_LIBRARY_DIR};${PXR_LIBRARY_DIRS};${INSTALL_RPATH}"
    VS_DEBUGGER_WORKING_DIRECTORY "${RUNTIME_OUTPUT_DIR}"
    VS_DEBUGGER_ENVIRONMENT "${VS_DEBUGGING_ENV}"
    XCODE_SCHEME_ENVIRONMENT "${XCODE_DEBUGGING_ENV}")

# Add dependencies.
target_link_libraries(${PROJECT_NAME}
PRIVATE
    GTest::gtest
    GTest::gmock
    glm::glm
    stb::stb
    Foundation
    MaterialXGenGlsl
)

# Add helpers include folder.
target_include_directories(${PROJECT_NAME}
PRIVATE
    "${AURORA_DIR}/API"
    "${AURORA_DIR}/Source"
    ${TEST_HELPERS_FOLDER}
)

# Add default compile definitions (set in root CMakefile)
target_compile_definitions(${PROJECT_NAME} PRIVATE ${DEFAULT_COMPILE_DEFINITIONS})

# Run gtest discover tests function.
gtest_discover_tests(${PROJECT_NAME}
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${RUNTIME_OUTPUT_DIR}"
)
