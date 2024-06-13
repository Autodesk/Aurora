project(Plasma)

if((WIN32 AND NOT ENABLE_HGI_BACKEND) OR APPLE)
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
set(optionalLibraries "")

if (APPLE)
    # Find and link the Metal framework
    find_library(METAL_FRAMEWORK Metal)
    find_library(METAL_KIT MetalKit)
    find_library(APP_KIT AppKit)
    find_library(GAME_CONTROLLER GameController)
    list(APPEND optionalLibraries ${METAL_FRAMEWORK} ${METAL_KIT} ${APP_KIT} ${GAME_CONTROLLER})

    set(APP_BUNDLE_IDENTIFIER "com.autodesk.${PROJECT_NAME}") 
    set(DEPLOYMENT_TARGET 14.0)

    set(PRODUCT_NAME ${PROJECT_NAME})
    set(EXECUTABLE_NAME ${PROJECT_NAME})
    set(MACOSX_BUNDLE_EXECUTABLE_NAME ${PROJECT_NAME})
    set(MACOSX_BUNDLE_INFO_STRING ${APP_BUNDLE_IDENTIFIER})
    set(MACOSX_BUNDLE_GUI_IDENTIFIER ${APP_BUNDLE_IDENTIFIER})
    set(MACOSX_BUNDLE_BUNDLE_NAME ${APP_BUNDLE_IDENTIFIER})
    set(MACOSX_BUNDLE_ICON_FILE "")
    set(MACOSX_BUNDLE_LONG_VERSION_STRING "1.0")
    set(MACOSX_BUNDLE_SHORT_VERSION_STRING "1.0")
    set(MACOSX_BUNDLE_BUNDLE_VERSION "1.0")
    set(MACOSX_BUNDLE_COPYRIGHT "Copyright")
    set(MACOSX_DEPLOYMENT_TARGET ${DEPLOYMENT_TARGET})

    # TODO: These environment variables are to aid development of Metal ray tracing shaders, and will eventually be removed.
    set(CMAKE_XCODE_GENERATE_SCHEME YES)
    set(CMAKE_XCODE_SCHEME_ENVIRONMENT
        HGI_TEMP_RAY_GEN_SHADER_DIR=${CMAKE_SOURCE_DIR}/Libraries/Aurora/Source/HGI/MetalShaders
        HGIMETAL_DEBUG=1
        CACHE STRING "" FORCE)

    # Add test executable with all source files.
    add_executable(${PROJECT_NAME} MACOSX_BUNDLE
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
        "AppDelegate.h"
        "AppDelegate.m"
        "Renderer.h"
        "Renderer.mm"
        "PlasmaViewController.h"
        "PlasmaViewController.mm"
        "PlasmaView.h"
        "PlasmaView.mm"
        "Shaders.metal"
        "ShaderTypes.h"
    )

    set_source_files_properties(Plasma.cpp PROPERTIES LANGUAGE OBJCXX)

    # Add the .storyboard file and set its properties
    set(UI_STORYBOARD Main.storyboard)
    set_source_files_properties(${UI_STORYBOARD} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
    #set(ENTITLEMENTS_FILE Plasma_Interactive.entitlements)
    #set_source_files_properties(${ENTITLEMENTS_FILE} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")

    set_source_files_properties(Shaders.metal PROPERTIES LANGUAGE METAL)

    target_include_directories(${PROJECT_NAME}
        PRIVATE
        "${TinyGLTF_INCLUDE_DIR}"
    )

    # Add the .storyboard file to the target
    target_sources(${PROJECT_NAME} PRIVATE ${UI_STORYBOARD})

    #set_target_properties(${PROJECT_NAME} PROPERTIES XCODE_ATTRIBUTE_CODE_SIGN_ENTITLEMENTS "${CMAKE_CURRENT_SOURCE_DIR}/${ENTITLEMENTS_FILE}")

    set_target_properties(${PROJECT_NAME} PROPERTIES
        BUNDLE True
        MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/info.plist.in
	    XCODE_SCHEME_ARGUMENTS "--scene ${CMAKE_SOURCE_DIR}/Applications/Assets/CornellBoxMetal.obj" # TODO: Temp sample model for demo. Need to be removed.
    )
else()
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
endif()

# Set custom output properties.
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
    tinygltf::tinygltf
    Foundation
    Aurora
    ${WINSDK_LIB}
    ${CMAKE_DL_LIBS}
    ${optionalLibraries}
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
