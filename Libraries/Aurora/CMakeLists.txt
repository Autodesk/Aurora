project(Aurora)

if(ENABLE_DIRECTX_BACKEND)
    # Add the preprocessor definition for directX backend flag.
    add_compile_definitions(DIRECTX_SUPPORT=1)

    find_package(D3D12 REQUIRED) # DirectX 12

    set(MINIMUM_WINDOWS_SDK_VERSION "10.0.22000.0")
    if(WINDOWS_SDK_VERSION VERSION_LESS MINIMUM_WINDOWS_SDK_VERSION)
        message(FATAL_ERROR
                "  Windows SDK version ${WINDOWS_SDK_VERSION} is found but version ${MINIMUM_WINDOWS_SDK_VERSION} or later is required.\n"
                "  Windows SDK version ${MINIMUM_WINDOWS_SDK_VERSION} or later can be installed from https://developer.microsoft.com/en-us/windows/downloads/sdk-archive/"
        )
    else()
        message(STATUS "Found Windows SDK version ${WINDOWS_SDK_VERSION}")
    endif()
else()
    add_compile_definitions(DIRECTX_SUPPORT=0)
endif()

if(ENABLE_HGI_BACKEND)
    # Add the preprocessor definition for HGI backend flag.
    add_compile_definitions(HGI_SUPPORT=1)

    find_package(pxr REQUIRED) # Pixar Universal Scene Description

    if (NOT TARGET pxr::hgiVulkan)
        message(FATAL_ERROR "USD hgiVulkan library is required to build Aurora with HGI backend enabled.")
    endif()

    find_package(Vulkan REQUIRED) # Vulkan SDK
else()
    add_compile_definitions(HGI_SUPPORT=0)
endif()

find_package(Slang REQUIRED) # The Slang library
find_package(glm REQUIRED) # The OpenGL Mathematics library.
find_package(stb REQUIRED) # The single file image/font librray.

# Optionally enable MaterialX code generator.
option(ENABLE_MATERIALX "Build with support for MaterialX material types." ON)
if(ENABLE_MATERIALX)
    # Add the preprocessor definition for MaterialX flag.
    add_compile_definitions(ENABLE_MATERIALX=1)

    find_package(MaterialX REQUIRED) # MaterialX SDK
    # Alias the namespace to meet the cmake convention on imported targets
    add_library(MaterialX::GenGlsl ALIAS MaterialXGenGlsl)
endif()

# Optionally enable NVIDIA denoiser.
option(ENABLE_DENOISER "Build with support for NVIDIA denoiser." OFF)
if(ENABLE_DENOISER)
    add_compile_definitions(ENABLE_DENOISER=1)

    find_package(NRD REQUIRED) # NVIDIA Real-Time Denoisers
    find_package(NRI REQUIRED) # NVIDIA Render Interface
endif()


# Public header files
set(API_HEADERS
    "API/Aurora/Aurora.h"
    "API/Aurora/AuroraNames.h"
)

# Shared source used by all backends.
set(COMMON_SOURCE
    "Source/AliasMap.cpp"
    "Source/AliasMap.h"
    "Source/AssetManager.cpp"
    "Source/AssetManager.h"
    "Source/Aurora.cpp"
    "Source/AuroraNames.cpp"
    "Source/DLL.cpp"
    "Source/EnvironmentBase.cpp"
    "Source/EnvironmentBase.h"
    "Source/GeometryBase.cpp"
    "Source/GeometryBase.h"
    "Source/MaterialBase.cpp"
    "Source/MaterialBase.h"
    "Source/pch.h"
    "Source/Properties.h"
    "Source/RendererBase.cpp"
    "Source/RendererBase.h"
    "Source/Resources.cpp"
    "Source/Resources.h"
    "Source/ResourceTracker.h"
    "Source/ResourceStub.cpp"
    "Source/ResourceStub.h"
    "Source/SceneBase.cpp"
    "Source/SceneBase.h"
    "Source/UniformBuffer.cpp"
    "Source/UniformBuffer.h"
    "Source/Transpiler.h"
    "Source/Transpiler.cpp"
    "Source/WindowsHeaders.h"
)

# Create the CompiledShaders directory in advance while cmake is generating the project files
set(COMPILED_SHADERS_DIR "${PROJECT_BINARY_DIR}/CompiledShaders")
file(MAKE_DIRECTORY "${COMPILED_SHADERS_DIR}")

# The slang shaders that are shared by DX and HGI backends.
set(COMMON_SHADERS
    "Source/Shaders/BSDF.slang"
    "Source/Shaders/BSDFCommon.slang"
    "Source/Shaders/Colors.slang"
    "Source/Shaders/Environment.slang"
    "Source/Shaders/Frame.slang"
    "Source/Shaders/Geometry.slang"
    "Source/Shaders/Globals.slang"
    "Source/Shaders/GLSLToHLSL.slang"
    "Source/Shaders/GroundPlane.slang"
    "Source/Shaders/ClosestHitEntryPointTemplate.slang"
    "Source/Shaders/ShadowHitEntryPointTemplate.slang"
    "Source/Shaders/LayerShaderEntryPointTemplate.slang"
    "Source/Shaders/Material.slang"
    "Source/Shaders/MaterialXCommon.slang"
    "Source/Shaders/PathTracingCommon.slang"
    "Source/Shaders/BackgroundMissShader.slang"
    "Source/Shaders/ShadowMissShader.slang"
    "Source/Shaders/RadianceMissShader.slang"
    "Source/Shaders/RayGenShader.slang"
    "Source/Shaders/ShadeFunctions.slang"
    "Source/Shaders/Random.slang"
    "Source/Shaders/RayTrace.slang"
    "Source/Shaders/ReferenceBSDF.slang"
    "Source/Shaders/StandardSurfaceBSDF.slang"
    "Source/Shaders/InitializeDefaultMaterialType.slang"
    "Source/Shaders/Sampling.slang"
)

# Set MINIFIED_SHADERS_HEADER to filename of auto-generated header file containing minified Slang shader strings.
set(MINIFIED_COMMON_SHADERS_HEADER "${COMPILED_SHADERS_DIR}/CommonShaders.h")
# Add commands to minifiy common shaders.
minify_shaders("${MINIFIED_COMMON_SHADERS_HEADER}" "${PROJECT_SOURCE_DIR}/Source/Shaders" "${COMMON_SHADERS}")


if(ENABLE_DIRECTX_BACKEND)
    if(ENABLE_MATERIALX)
        set(DIRECTX_MATERIALX_SOURCE
            "Source/MaterialX/BSDFCodeGenerator.cpp"
            "Source/MaterialX/BSDFCodeGenerator.h"
            "Source/MaterialX/MaterialGenerator.cpp"
            "Source/MaterialX/MaterialGenerator.h"
        )
    endif()

    if(ENABLE_DENOISER)
        set(DIRECTX_DENOISER_SOURCE
            "Source/DirectX/Denoiser.cpp"
            "Source/DirectX/Denoiser.h"
        )
    endif()

    # DirectX backend source.
    set(DIRECTX_SOURCE
        "Source/DirectX/MemoryPool.h"
        "Source/DirectX/PTDevice.cpp"
        "Source/DirectX/PTDevice.h"
        "Source/DirectX/PTEnvironment.cpp"
        "Source/DirectX/PTEnvironment.h"
        "Source/DirectX/PTGeometry.cpp"
        "Source/DirectX/PTGeometry.h"
        "Source/DirectX/PTGroundPlane.cpp"
        "Source/DirectX/PTGroundPlane.h"
        "Source/DirectX/PTImage.cpp"
        "Source/DirectX/PTImage.h"
        "Source/DirectX/PTMaterial.cpp"
        "Source/DirectX/PTMaterial.h"
        "Source/DirectX/PTRenderer.cpp"
        "Source/DirectX/PTRenderer.h"
        "Source/DirectX/PTSampler.cpp"
        "Source/DirectX/PTSampler.h"
        "Source/DirectX/PTScene.cpp"
        "Source/DirectX/PTScene.h"
        "Source/DirectX/PTShaderLibrary.cpp"
        "Source/DirectX/PTShaderLibrary.h"
        "Source/DirectX/PTTarget.cpp"
        "Source/DirectX/PTTarget.h"
        "${DIRECTX_DENOISER_SOURCE}"
        "${DIRECTX_MATERIALX_SOURCE}"
    )

    set(DIRECTX_PRECOMPILED_SHADERS
        "Source/DirectX/Shaders/Accumulation.hlsl"
        "Source/DirectX/Shaders/PostProcessing.hlsl"
    )
endif()

if(ENABLE_HGI_BACKEND)
    set(HGI_SOURCE
        "Source/HGI/HGIEnvironment.cpp"
        "Source/HGI/HGIEnvironment.h"
        "Source/HGI/HGIGroundPlane.cpp"
        "Source/HGI/HGIGroundPlane.h"
        "Source/HGI/HGIImage.cpp"
        "Source/HGI/HGIImage.h"
        "Source/HGI/HGIMaterial.cpp"
        "Source/HGI/HGIMaterial.h"
        "Source/HGI/HGIGeometry.cpp"
        "Source/HGI/HGIGeometry.h"
        "Source/HGI/HGIHandleWrapper.cpp"
        "Source/HGI/HGIHandleWrapper.h"
        "Source/HGI/HGIRenderBuffer.cpp"
        "Source/HGI/HGIRenderBuffer.h"
        "Source/HGI/HGIRenderer.cpp"
        "Source/HGI/HGIRenderer.h"
        "Source/HGI/HGIScene.cpp"
        "Source/HGI/HGIScene.h"
        "Source/HGI/HGIWindow.cpp"
        "Source/HGI/HGIWindow.h"
    )

    set(HGI_SHADERS
        "Source/HGI/Shaders/Accumulation.glsl"
        "Source/HGI/Shaders/PostProcessing.glsl"
        "Source/HGI/Shaders/InstanceData.glsl"
    )

    # Set MINIFIED_SHADERS_HEADER to filename of auto-generated header file containing minified HLSL shader strings.
    set(MINIFIED_HGI_SHADERS_HEADER "${COMPILED_SHADERS_DIR}/HGIShaders.h")
    # Add commands to minifiy hgi shaders.
    minify_shaders("${MINIFIED_HGI_SHADERS_HEADER}" "${CMAKE_CURRENT_SOURCE_DIR}/Source/HGI/Shaders" "${HGI_SHADERS}")
endif()

# Create shared library for the project containing all the sources.
add_library(${PROJECT_NAME} SHARED
    ${API_HEADERS}
    ${COMMON_SOURCE}
    ${DIRECTX_SOURCE}
    ${COMMON_SHADERS}
    ${MINIFIED_COMMON_SHADERS_HEADER}
    ${DIRECTX_PRECOMPILED_SHADERS}
    ${HGI_SOURCE}
    ${HGI_SHADERS}
    ${MINIFIED_HGI_SHADERS_HEADER}
)

# Add to libraries folder
set_property(TARGET ${PROJECT_NAME} PROPERTY FOLDER "Libraries")

# Specify a header file from which to generate precompiled headers.
# NOTE: This will be used by all source files.
target_precompile_headers(${PROJECT_NAME} PRIVATE "Source/pch.h")

# Add the source to groups within IDE.
source_group("API" FILES ${API_HEADERS})
source_group("Common" FILES ${COMMON_SOURCE})
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/Source" FILES ${DIRECTX_SOURCE})
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/Source" FILES ${HGI_SOURCE})
source_group("Shaders" FILES ${COMMON_SHADERS})
source_group("DirectX/Shaders" FILES ${DIRECTX_PRECOMPILED_SHADERS})
source_group("HGI/Shaders" FILES ${HGI_SHADERS})

if(WIN32)
    if(ENABLE_DIRECTX_BACKEND)
        # Create custom targets that will copy DLLs of the DirectX shader compiler to the runtime folder.
        add_custom_target(CopyDXCompilerDLLs ALL
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${DXCOMPILER_DLLS} ${RUNTIME_OUTPUT_DIR}
        )
        set_property(TARGET CopyDXCompilerDLLs PROPERTY FOLDER "Deployment")
        add_dependencies(CopyDXCompilerDLLs MakeRuntimeDir)
        add_dependencies(${PROJECT_NAME} CopyDXCompilerDLLs)
    endif()

    # Copy dlls of the aurora dependencies to our runtime directory
    if(DEFINED AURORA_DEPENDENCIES)
        set(COMMON_DEPENDENCIES ${AURORA_DEPENDENCIES})
        # Exclude NRD and NRI as they will be handled separatly
        list(REMOVE_ITEM COMMON_DEPENDENCIES ${NRD_ROOT} ${NRI_ROOT})

        # Create a do-nothing command so we can append copy commands later
        add_custom_command(OUTPUT CopyPackageDLLs
            COMMAND ${CMAKE_COMMAND} -E true
        )
        # CopyPackageDLLs is not actually generated so set it to symbolic
        set_source_files_properties(CopyPackageDLLs PROPERTIES SYMBOLIC "true")
        foreach(pkg ${COMMON_DEPENDENCIES})
            file(GLOB LIB_DLLS ${pkg}/lib/*.dll)
            if(LIB_DLLS)
                add_custom_command(OUTPUT CopyPackageDLLs APPEND
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${LIB_DLLS} ${RUNTIME_OUTPUT_DIR}
                )
            endif()
            file(GLOB BIN_DLLS ${pkg}/bin/*.dll)
            if (BIN_DLLS)
                add_custom_command(OUTPUT CopyPackageDLLs APPEND
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${BIN_DLLS} ${RUNTIME_OUTPUT_DIR}
                )
            endif()
        endforeach()
        add_custom_target(CopyExternalDLLs ALL DEPENDS CopyPackageDLLs)
        set_property(TARGET CopyExternalDLLs PROPERTY FOLDER "Deployment")
        add_dependencies(CopyExternalDLLs MakeRuntimeDir)
        add_dependencies(${PROJECT_NAME} CopyExternalDLLs)
    endif()

    # Create a custom target that will copy NVIDIA NRD and NRI DLLs to the runtime folder, if the
    # denoiser is enabled.
    if(ENABLE_DENOISER)
        add_custom_target(CopyNvidiaDenoiserDLLs ALL
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${NRD_LIBRARY_DLLS} ${RUNTIME_OUTPUT_DIR}
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${NRI_LIBRARY_DLLS} ${RUNTIME_OUTPUT_DIR}
        )
        set_property(TARGET CopyNvidiaDenoiserDLLs PROPERTY FOLDER "Deployment")
        add_dependencies(CopyNvidiaDenoiserDLLs MakeRuntimeDir)
        add_dependencies(${PROJECT_NAME} CopyNvidiaDenoiserDLLs)
    endif()

    # Create a custom target that will copy MaterialX libraries to the runtime folder, if MaterialX
    # is enabled.
    if(ENABLE_MATERIALX)
        add_custom_target(CopyMaterialXLibrary ALL
            COMMAND ${CMAKE_COMMAND} -E make_directory ${RUNTIME_OUTPUT_DIR}/MaterialX/libraries
            COMMAND ${CMAKE_COMMAND} -E copy_directory ${MATERIALX_STDLIB_DIR} ${RUNTIME_OUTPUT_DIR}/MaterialX/libraries
        )
        set_property(TARGET CopyMaterialXLibrary PROPERTY FOLDER "Deployment")
        add_dependencies(${PROJECT_NAME} CopyMaterialXLibrary)
    endif()

    if(ENABLE_HGI_BACKEND)
        # Copy additional HGI files of the Pixar USD to our runtime directory
        add_custom_target(CopyUSDDLLs ALL
            COMMAND ${CMAKE_COMMAND} -E make_directory ${RUNTIME_OUTPUT_DIR}/usd/hgiVulkan/resources
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${PXR_LIBRARY_DIR}/usd/plugInfo.json ${RUNTIME_OUTPUT_DIR}/usd
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${PXR_LIBRARY_DIR}/usd/hgiVulkan/resources/plugInfo.json ${RUNTIME_OUTPUT_DIR}/usd/hgiVulkan/resources
        )
        set_property(TARGET CopyUSDDLLs PROPERTY FOLDER "Deployment")
        add_dependencies(CopyUSDDLLs MakeRuntimeDir)
        add_dependencies(${PROJECT_NAME} CopyUSDDLLs)
    endif()

    # Create a custom target that will copy Slang DLLs to the runtime folder.
    add_custom_target(CopySlangDLLs ALL
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${Slang_LIBRARY_DLL} ${RUNTIME_OUTPUT_DIR}
    )
    set_property(TARGET CopySlangDLLs PROPERTY FOLDER "Deployment")
    add_dependencies(CopySlangDLLs MakeRuntimeDir)
    add_dependencies(${PROJECT_NAME} CopySlangDLLs)

else() # Linux
    #TODO anything we need to set to support the Aurora execution?
endif()

# Set custom ouput properties.
set_target_properties(${PROJECT_NAME} PROPERTIES
	FOLDER "Libraries"
    RUNTIME_OUTPUT_DIRECTORY "${RUNTIME_OUTPUT_DIR}"
    LIBRARY_OUTPUT_DIRECTORY "${LIBRARY_OUTPUT_DIR}"
    ARCHIVE_OUTPUT_DIRECTORY "${LIBRARY_OUTPUT_DIR}"
    PDB_OUTPUT_DIRECTORY "${RUNTIME_OUTPUT_DIR}"
)

if(ENABLE_MATERIALX)
    set(MATERIALX_TARGETS MaterialX::GenGlsl)
else()
    set(MATERIALX_TARGETS "")
endif()

if(ENABLE_DENOISER)
    set(DENOISER_TARGETS NRD::NRD NRI::NRI)
else()
    set(DENOISER_TARGETS "")
endif()

if(ENABLE_HGI_BACKEND)
    set(USD_TARGETS pxr::usd pxr::hgi)
endif()

if(ENABLE_DIRECTX_BACKEND)
    set(D2D12_TARGETS D3D12::D3D12 D3D12::compiler)
else()
    set(D2D12_TARGETS "")
endif()

target_link_libraries(${PROJECT_NAME}
PRIVATE
    Foundation
    ${MATERIALX_TARGETS} # MaterialX needs to be in front of USD due to the conflicts of MaterialX.h in USD
    ${D2D12_TARGETS}
    ${USD_TARGETS}
    ${DENOISER_TARGETS}
    stb::stb
    glm::glm
    Slang::Slang
)

target_include_directories(${PROJECT_NAME}
PUBLIC
    "API"                     # Public include path
PRIVATE
    "Source"                  # Source folder for PCH
    "${PROJECT_BINARY_DIR}"
)

# Set the default compile definitions.
target_compile_definitions(${PROJECT_NAME} PRIVATE ${DEFAULT_COMPILE_DEFINITIONS})

if(ENABLE_DIRECTX_BACKEND)
    # Compile the standalone compute shaders.
    # NOTE: These are compiled with DXC at build time unlike all the other shaders.
    # TODO: Runtime compile this too, see OGSMOD-1215.
    set_source_files_properties(Source/DirectX/Shaders/Accumulation.hlsl PROPERTIES
        VS_SHADER_TYPE Compute
        VS_SHADER_MODEL 6.3
        VS_SHADER_ENTRYPOINT "Accumulation"
            VS_SHADER_OUTPUT_HEADER_FILE "${COMPILED_SHADERS_DIR}/Accumulation.hlsl.h"
        VS_SHADER_VARIABLE_NAME "g_pAccumulationShader"
        VS_SHADER_OBJECT_FILE_NAME " "
    )
    set_source_files_properties(Source/DirectX/Shaders/PostProcessing.hlsl PROPERTIES
        VS_SHADER_TYPE Compute
        VS_SHADER_MODEL 6.3
        VS_SHADER_ENTRYPOINT "PostProcessing"
            VS_SHADER_OUTPUT_HEADER_FILE "${COMPILED_SHADERS_DIR}/PostProcessing.hlsl.h"
        VS_SHADER_VARIABLE_NAME "g_pPostProcessingShader"
        VS_SHADER_OBJECT_FILE_NAME " "
    )

    # TODO: do we have Vulkan computer shaders amd need to do similiar things for them?
endif()

install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION "${INSTALL_BIN}"
    ARCHIVE DESTINATION "${INSTALL_LIB}"
    LIBRARY DESTINATION "${INSTALL_LIB}"
    COMPONENT ${PROJECT_NAME}
)
