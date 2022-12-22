# Prevent re-defining the package target
if(TARGET D3D12::D3D12)
  return()
endif()

# Work out Windows 10 SDK path and version
# NOTE: Based on https://github.com/microsoft/DirectXShaderCompiler/blob/master/cmake/modules/FindD3D12.cmake
if("$ENV{WINDOWS_SDK_PATH}$ENV{WINDOWS_SDK_VERSION}" STREQUAL "" )
    if(CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION)
        get_filename_component(WINDOWS_SDK_PATH "[HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows Kits\\Installed Roots;KitsRoot10]" ABSOLUTE CACHE)
        set (WINDOWS_SDK_VERSION ${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION})
    else()
        message(FATAL_ERROR
            "The Windows SDK cannot be found and it is required to build the DirectX backend for "
            "Aurora. If you have the Windows SDK installed, set these environment variables with "
            "values for your installation: WINDOWS_SDK_PATH and WINDOWS_SDK_VERSION."
        )
    endif()
else()
    set (WINDOWS_SDK_PATH $ENV{WINDOWS_SDK_PATH})
    set (WINDOWS_SDK_VERSION $ENV{WINDOWS_SDK_VERSION})
endif()

# WINDOWS_SDK_PATH will be something like C:\Program Files (x86)\Windows Kits\10
# WINDOWS_SDK_VERSION will be something like 10.0.14393 or 10.0.14393.0; we need the
# one that matches the directory name.
if(IS_DIRECTORY "${WINDOWS_SDK_PATH}/Include/${WINDOWS_SDK_VERSION}.0")
    set(WINDOWS_SDK_VERSION "${WINDOWS_SDK_VERSION}.0")
endif()

# Find the d3d12 and dxgi include path, it will typically look something like this.
# C:\Program Files (x86)\Windows Kits\10\Include\10.0.10586.0\um\d3d12.h
# C:\Program Files (x86)\Windows Kits\10\Include\10.0.10586.0\shared\dxgi1_6.h
find_path(D3D12_INCLUDE_DIR    # Set variable D3D12_INCLUDE_DIR
    d3d12.h                    # Find a path with d3d12.h
    HINTS "${WINDOWS_SDK_PATH}/Include/${WINDOWS_SDK_VERSION}/um"
    DOC "path to WINDOWS SDK header files"
)

find_path(DXGI_INCLUDE_DIR    # Set variable DXGI_INCLUDE_DIR
    dxgi1_6.h                 # Find a path with dxgi1_6.h
    HINTS "${WINDOWS_SDK_PATH}/Include/${WINDOWS_SDK_VERSION}/shared"
    DOC "path to WINDOWS SDK header files"
)

find_library(D3D12_LIBRARY
    NAMES d3d12.lib
    HINTS ${WINDOWS_SDK_PATH}/Lib/${WINDOWS_SDK_VERSION}/um/x64
)

find_library(DXGI_LIBRARY
    NAMES dxgi.lib
    HINTS ${WINDOWS_SDK_PATH}/Lib/${WINDOWS_SDK_VERSION}/um/x64
)

find_library(D3DCOMPILER_LIBRARY
    NAMES d3dcompiler
    HINTS ${WINDOWS_SDK_PATH}/Lib/${WINDOWS_SDK_VERSION}/um/x64
)
find_library(DXGUID_LIBRARY
    NAMES dxguid
    HINTS ${WINDOWS_SDK_PATH}/Lib/${WINDOWS_SDK_VERSION}/um/x64
)
find_library(DXCOMPILER_LIBRARY
    NAMES dxcompiler
    HINTS ${WINDOWS_SDK_PATH}/Lib/${WINDOWS_SDK_VERSION}/um/x64
)
find_file(DXCOMPILER_DLL
    NAMES dxcompiler.dll
    HINTS ${WINDOWS_SDK_PATH}/bin/${WINDOWS_SDK_VERSION}/x64
)
find_file(DXIL_DLL
    NAMES dxil.dll
    HINTS ${WINDOWS_SDK_PATH}/bin/${WINDOWS_SDK_VERSION}/x64
)

get_filename_component(D3D12_LIBRARY_DIR ${D3D12_LIBRARY} DIRECTORY)

set(D3D12_LIBRARIES ${D3D12_LIBRARY} ${DXGI_LIBRARY})
set(D3D12_COMPILER_LIBRARIES ${D3DCOMPILER_LIBRARY} ${DXGUID_LIBRARY} ${DXCOMPILER_LIBRARY})
set(D3D12_INCLUDE_DIRS ${D3D12_INCLUDE_DIR} ${DXGI_INCLUDE_DIR})

set(DXCOMPILER_DLLS ${DXCOMPILER_DLL} ${DXIL_DLL})

include(FindPackageHandleStandardArgs)
# handle the QUIETLY and REQUIRED arguments and set D3D12_FOUND to TRUE
# if all listed variables are TRUE
find_package_handle_standard_args(D3D12
                                  DEFAULT_MSG
                                  D3D12_INCLUDE_DIRS
                                  D3D12_LIBRARIES
)

if(D3D12_FOUND)
    add_library(D3D12::D3D12 UNKNOWN IMPORTED)
    set_target_properties(D3D12::D3D12 PROPERTIES
        IMPORTED_LOCATION ${D3D12_LIBRARY}
        IMPORTED_LINK_INTERFACE_LIBRARIES "${D3D12_LIBRARIES}"
        INTERFACE_INCLUDE_DIRECTORIES "${D3D12_INCLUDE_DIRS}"
    )
    add_library(D3D12::compiler UNKNOWN IMPORTED)
    set_target_properties(D3D12::compiler PROPERTIES
        IMPORTED_LOCATION ${D3DCOMPILER_LIBRARY}
        IMPORTED_LINK_INTERFACE_LIBRARIES "${D3D12_COMPILER_LIBRARIES}"
    )
endif()

mark_as_advanced(
  D3D12_INCLUDE_DIRS
  D3D12_LIBRARIES
  D3D12_INCLUDE_DIR
  D3D12_LIBRARY
  D3D12_LIBRARY_DIR
  DXGI_INCLUDE_DIR
  DXGI_LIBRARY
  DXCOMPILER_DLLS
  D3D12_COMPILER_LIBRARIES
  WINDOWS_SDK_PATH
  WINDOWS_SDK_VERSION
)
