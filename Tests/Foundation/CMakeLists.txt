# Specify the library (for dependents), project (IDE), and output (binary file) names.
project(FoundationTests)

# List of common helper files shared by all tests. TEST_HELPERS_FOLDER variable is set in parent cmake file.
set(HELPER_FILES
    "${TEST_HELPERS_FOLDER}/TestHelpers.cpp"
    "${TEST_HELPERS_FOLDER}/TestHelpers.h")

# List of actual test files.
set(TEST_FILES
    "Tests/TestLogger.cpp"
    "Tests/TestMath.cpp"
    "Tests/TestUtilities.cpp")

# Add test executable with all source files.
add_executable(${PROJECT_NAME}
    ${HELPER_FILES}
    ${TEST_FILES}
    "FoundationMain.cpp"
)

# Put test files and helpers in seperate folders.
source_group("Helpers" FILES ${HELPER_FILES})
source_group("Tests" FILES ${TEST_FILES})

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
    glm::glm
    GTest::gtest
    GTest::gmock
    ${CMAKE_DL_LIBS}
    Foundation
)

# Add helpers include folder.
target_include_directories(${PROJECT_NAME} PRIVATE ${TEST_HELPERS_FOLDER})

# Add default compile definitions (set in root CMakefile)
target_compile_definitions(${PROJECT_NAME} PRIVATE ${DEFAULT_COMPILE_DEFINITIONS})

# Run gtest discover tests function.
gtest_discover_tests(${PROJECT_NAME}
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${RUNTIME_OUTPUT_DIR}"
)
