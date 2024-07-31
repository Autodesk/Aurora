# Specify the library (for dependents), project (IDE), and output (binary file) names.
project(HdAuroraTests)

find_package(GLEW REQUIRED)
find_package(MaterialX REQUIRED)
find_package(pxr REQUIRED)
# Alias the namespace to meet the cmake convention on imported targets
add_library(MaterialX::GenGlsl ALIAS MaterialXGenGlsl)

# List of common helper files shared by all tests. TEST_HELPERS_FOLDER variable is set in parent cmake file.
set(HELPER_FILES
    "${TEST_HELPERS_FOLDER}/TestHelpers.cpp"
    "${TEST_HELPERS_FOLDER}/TestHelpers.h")

# List of actual test files.
set(TEST_FILES
    "Tests/TestStability.cpp")

# Avoid using ${CMAKE_SOURCE_DIR} as it may break the generation when the project is included as a subdirectory.
# ImageProcessingResolver as a runtime dependency is not needed for the test build.
set(HDAURORA_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../Libraries/HdAurora")
file(GLOB HDAURORA_FILES
    ${HDAURORA_SOURCE_DIR}/*.cpp
    ${HDAURORA_SOURCE_DIR}/*.h
)

# Add test executable with all source files.
add_executable(${PROJECT_NAME}
    ${HELPER_FILES}
    ${TEST_FILES}
    ${HDAURORA_FILES}
    "HdAuroraMain.cpp"
)

# Put test files and helpers in seperate folders.
source_group("Helpers" FILES ${HELPER_FILES})
source_group("Tests" FILES ${TEST_FILES})
source_group("HdAurora" FILES ${HDAURORA_FILES})

# Set custom output properties.
set_target_properties(${PROJECT_NAME} PROPERTIES
	FOLDER "Tests"
    RUNTIME_OUTPUT_DIRECTORY "${RUNTIME_OUTPUT_DIR}"
    LIBRARY_OUTPUT_DIRECTORY "${LIBRARY_OUTPUT_DIR}"
    ARCHIVE_OUTPUT_DIRECTORY "${LIBRARY_OUTPUT_DIR}"
    PDB_OUTPUT_DIRECTORY "${RUNTIME_OUTPUT_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${RUNTIME_OUTPUT_DIR}"
    VS_DEBUGGER_ENVIRONMENT "${VS_DEBUGGING_ENV}"
    XCODE_SCHEME_ENVIRONMENT "${XCODE_DEBUGGING_ENV}")

# Add dependencies.
target_link_libraries(${PROJECT_NAME}
PRIVATE
    glm::glm
    GTest::gtest
    GTest::gmock
    ${CMAKE_DL_LIBS}
    stb::stb
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

# Add helpers include folder.
target_include_directories(${PROJECT_NAME} PRIVATE ${HDAURORA_SOURCE_DIR} ${TEST_HELPERS_FOLDER})

# Add default compile definitions (set in root CMakefile)
target_compile_definitions(${PROJECT_NAME} PRIVATE ${DEFAULT_COMPILE_DEFINITIONS})

# Run gtest discover tests function.
gtest_discover_tests(${PROJECT_NAME}
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${RUNTIME_OUTPUT_DIR}"
)
