project(AuroraTests)

# List of common helper files shared by all tests. TEST_HELPERS_FOLDER variable is set in parent cmake file.
set(HELPER_FILES
    "${TEST_HELPERS_FOLDER}/TestHelpers.cpp"
    "${TEST_HELPERS_FOLDER}/TestHelpers.h"
    "${TEST_HELPERS_FOLDER}/TeapotModel.h"
    "${TEST_HELPERS_FOLDER}/BaselineImageHelpers.cpp"
    "${TEST_HELPERS_FOLDER}/BaselineImageHelpers.h"
    "${TEST_HELPERS_FOLDER}/RendererTestHelpers.cpp"
    "${TEST_HELPERS_FOLDER}/RendererTestHelpers.h"
    "${TEST_HELPERS_FOLDER}/AuroraTestHelpers.cpp"
    "${TEST_HELPERS_FOLDER}/AuroraTestHelpers.h"
)

# List of actual test files.
set(TEST_FILES
    "Tests/TestBenchmarks.cpp"
    "Tests/TestImage.cpp"
    "Tests/TestLight.cpp"
    "Tests/TestMaterial.cpp"
    "Tests/TestPaths.cpp"
    "Tests/TestRenderer.cpp"
)

# Add test executable with all source files.
add_executable(${PROJECT_NAME}
    ${HELPER_FILES}
    ${TEST_FILES}
    "AuroraMain.cpp"
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
    VS_DEBUGGER_WORKING_DIRECTORY "${RUNTIME_OUTPUT_DIR}"
    VS_DEBUGGER_ENVIRONMENT "${VS_DEBUGGING_ENV}"
    XCODE_SCHEME_ENVIRONMENT "${XCODE_DEBUGGING_ENV}")

# Add dependencies.
target_link_libraries(${PROJECT_NAME}
PRIVATE
    GTest::gtest
    GTest::gmock
    glm::glm
    PNG::PNG
    stb::stb
    OpenImageIO::OpenImageIO
    pxr::usd
    pxr::hgi
    Foundation
    Aurora
)

# TODO: there are still missing DLLs with this custom command. Likely the
#       dependentcies of the dependencies. Need to research on the proper
#       way to do it.
# Copy the dependent DLLs to the folder of the target executable
# $<TARGET_RUNTIME_DLLS:XXX> reuires cmake 3.21+
# add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
#   COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_RUNTIME_DLLS:${PROJECT_NAME}> $<TARGET_FILE_DIR:${PROJECT_NAME}>
#   COMMAND_EXPAND_LISTS
# )

# Add helpers include folder.
target_include_directories(${PROJECT_NAME}
PRIVATE
    ${TEST_HELPERS_FOLDER}
)

# Add to libraries folder
set_property(TARGET ${PROJECT_NAME} PROPERTY FOLDER "Tests")

# Add default compile definitions (set in root CMakefile)
target_compile_definitions(${PROJECT_NAME} PRIVATE ${DEFAULT_COMPILE_DEFINITIONS})

if(WIN32)
    add_dependencies(CopyOpenImageIODLLs MakeRuntimeDir)
    add_dependencies(${PROJECT_NAME} CopyOpenImageIODLLs)
endif()

# Run gtest discover tests function.
if(RUN_TESTS_FROM_PROJECT)
    gtest_discover_tests(${PROJECT_NAME}
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${RUNTIME_OUTPUT_DIR}"
    )
endif()