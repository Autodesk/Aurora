# Prevent re-defining the package target
if(TARGET NRD::NRD)
  return()
endif()

find_path(NRD_INCLUDE_DIR      # Set variable NRD_INCLUDE_DIR
          NRD.h                # Find a path with NRD.h
          PATH_SUFFIXES "Include"
)
find_path(NRD_INTEGRATION_INCLUDE_DIR # Set variable NRD_INTEGRATION_INCLUDE_DIR
          NRDIntegration.h            # Find a path with NRDIntegration.h
          PATH_SUFFIXES "Integration"
          REQUIRED
          DOC "path to NVIDIA Real-time Denoisers SDK integration header files"
)
find_path(NRD_SHADERS_INCLUDE_DIR
          NRD.hlsli
          PATH_SUFFIXES "Shaders/Include"
          REQUIRED
          DOC "path to NVIDIA Real-time Denoisers SDK shader header files"
)

cmake_path(GET NRD_INCLUDE_DIR PARENT_PATH NRD_INSTALL_PREFIX)
cmake_path(GET NRD_SHADERS_INCLUDE_DIR PARENT_PATH NRD_SHADERS_DIR)
set(NRD_SHADERS_SOURCE_DIR ${NRD_SHADERS_DIR}/Source)
set(NRD_SHADERS_RESOURCES_DIR ${NRD_SHADERS_DIR}/Resources)
set(NRD_INCLUDE_DIRS ${NRD_INCLUDE_DIR} ${NRD_INTEGRATION_INCLUDE_DIR})

add_library(NRD::NRD SHARED IMPORTED)

find_library(NRD_LIBRARY_RELEASE # Set variable NRD_LIBRARY_RELEASE
             NRD                 # Find library path with libNRD.so, NRD.dll, or NRD.lib
)
if(NRD_LIBRARY_RELEASE)
  set_property(TARGET NRD::NRD APPEND PROPERTY
    IMPORTED_CONFIGURATIONS RELEASE
  )
  set_target_properties(NRD::NRD PROPERTIES
    IMPORTED_IMPLIB_RELEASE "${NRD_LIBRARY_RELEASE}"
  )
  if(WIN32)
    find_file(NRD_DLL_RELEASE NRD.dll REQUIRED
      PATHS "${NRD_INSTALL_PREFIX}/lib" "${NRD_INSTALL_PREFIX}/bin"
    )
    set_target_properties(NRD::NRD PROPERTIES
      IMPORTED_LOCATION_RELEASE "${NRD_DLL_RELEASE}"
    )
  endif()
endif()

find_library(NRD_LIBRARY_DEBUG # Set variable NRD_LIBRARY_DEBUG
             NRDd              # Find library path with libNRDd.so, NRDd.dll, or NRDd.lib
)
if(NRD_LIBRARY_DEBUG)
  set_property(TARGET NRD::NRD APPEND PROPERTY
    IMPORTED_CONFIGURATIONS DEBUG
  )
  set_target_properties(NRD::NRD PROPERTIES
    IMPORTED_IMPLIB_DEBUG "${NRD_LIBRARY_DEBUG}"
  )
  if(WIN32)
    find_file(NRD_DLL_DEBUG NRDd.dll REQUIRED
      PATHS "${NRD_INSTALL_PREFIX}/lib" "${NRD_INSTALL_PREFIX}/bin"
    )
    set_target_properties(NRD::NRD PROPERTIES
      IMPORTED_LOCATION_DEBUG "${NRD_DLL_DEBUG}"
    )
  endif()
endif()

if (NRD_LIBRARY_RELEASE)
  set(NRD_LIBRARIES ${NRD_LIBRARY_RELEASE})
elseif (NRD_LIBRARY_DEBUG)
  set(NRD_LIBRARIES ${NRD_LIBRARY_DEBUG})
endif()

get_filename_component(NRD_LIBRARY_DIR ${NRD_LIBRARIES} DIRECTORY)
if(WIN32)
  file(GLOB NRD_LIBRARY_DLLS ${NRD_LIBRARY_DIR}/*.dll)
endif()

include(FindPackageHandleStandardArgs)
# handle the QUIETLY and REQUIRED arguments and set NRD_FOUND to TRUE
# if all listed variables are TRUE
find_package_handle_standard_args(NRD
                                  DEFAULT_MSG
                                  NRD_INCLUDE_DIRS
                                  NRD_INTEGRATION_INCLUDE_DIR
                                  NRD_LIBRARIES
)

mark_as_advanced(
    NRD_INCLUDE_DIRS
    NRD_INCLUDE_DIR
    NRD_INTEGRATION_INCLUDE_DIR
    NRD_LIBRARIES
    NRD_LIBRARY_RELEASE
    NRD_LIBRARY_DEBUG
    NRD_LIBRARY_DIR
    NRD_LIBRARY_DLLS
    NRD_SHADERS_DIR
    NRD_SHADERS_INCLUDE_DIR
    NRD_SHADERS_SOURCE_DIR
    NRD_SHADERS_RESOURCES_DIR
)
