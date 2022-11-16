# Find the NRD include path
if (DEFINED NRD_ROOT)
    # Prioritize the NVIDIA Real-time Denoisers installed at ${NRD_ROOT}
    find_path(NRD_INCLUDE_DIR      # Set variable NRD_INCLUDE_DIR
              NRD.h                # Find a path with NRD.h
              NO_DEFAULT_PATH
              PATHS "${NRD_ROOT}/Include"
              DOC "path to NVIDIA Real-time Denoisers SDK header files"
    )
    find_path(NRD_INTEGRATION_INCLUDE_DIR # Set variable NRD_INTEGRATION_INCLUDE_DIR
              NRDIntegration.h            # Find a path with NRDIntegration.h
              NO_DEFAULT_PATH
              PATHS "${NRD_ROOT}/Integration"
              DOC "path to NVIDIA Real-time Denoisers SDK header files"
    )
    find_library(NRD_LIBRARY       # Set variable NRD_LIBRARY
                 NRD               # Find library path with libNRD.so, NRD.dll, or NRD.lib
                 NO_DEFAULT_PATH
                 PATHS "${NRD_ROOT}/Lib"
                 PATH_SUFFIXES Release RelWithDebInfo Debug
                 DOC "path to NVIDIA Real-time Denoisers SDK library files"
    )
    find_path(NRD_SHADERS_INCLUDE_DIR
              NRD.hlsli
              NO_DEFAULT_PATH
              PATHS "${NRD_ROOT}/Shaders/Include"
              DOC "path to NVIDIA Real-time Denoisers SDK shader header files"
    )
endif()
# Once the prioritized find_path succeeds the result variable will be set and stored in the cache
# so that no call will search again.
find_path(NRD_INCLUDE_DIR      # Set variable NRD_INCLUDE_DIR
          NRD.h                # Find a path with NRD.h
          DOC "path to NVIDIA Real-time Denoisers SDK header files"
)
find_path(NRD_INTEGRATION_INCLUDE_DIR # Set variable NRD_INTEGRATION_INCLUDE_DIR
          NRDIntegration.h            # Find a path with NRDIntegration.h
          DOC "path to NVIDIA Real-time Denoisers SDK header files"
)
find_library(NRD_LIBRARY       # Set variable NRD_INCLUDE_DIR
             NRD               # Find library path with libNRD.so, NRD.dll, or NRD.lib
             DOC "path to NVIDIA Real-time Denoisers SDK library files"
)
find_path(NRD_SHADERS_INCLUDE_DIR
          NRD.hlsli
          DOC "path to NVIDIA Real-time Denoisers SDK shader header files"
)

if(NRD_SHADERS_INCLUDE_DIR)
    cmake_path(GET NRD_SHADERS_INCLUDE_DIR PARENT_PATH NRD_SHADERS_DIR)
    set(NRD_SHADERS_SOURCE_DIR ${NRD_SHADERS_DIR}/Source)
    set(NRD_SHADERS_RESOURCES_DIR ${NRD_SHADERS_DIR}/Resources)
endif()

set(NRD_LIBRARIES ${NRD_LIBRARY})
set(NRD_INCLUDE_DIRS ${NRD_INCLUDE_DIR} ${NRD_INTEGRATION_INCLUDE_DIR})

get_filename_component(NRD_LIBRARY_DIR ${NRD_LIBRARY} DIRECTORY)
if(WIN32)
    set(NRD_LIBRARY_DLL ${NRD_LIBRARY_DIR}/NRD.dll)
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

if(NRD_FOUND)
    add_library(NRD::NRD UNKNOWN IMPORTED)
 	set_target_properties(NRD::NRD PROPERTIES
		IMPORTED_LOCATION ${NRD_LIBRARY}
		IMPORTED_LINK_INTERFACE_LIBRARIES "${NRD_LIBRARIES}"
        INTERFACE_INCLUDE_DIRECTORIES "${NRD_INCLUDE_DIRS}"
    )
endif()

mark_as_advanced(
    NRD_INCLUDE_DIRS
    NRD_INCLUDE_DIR
    NRD_INTEGRATION_INCLUDE_DIR
    NRD_LIBRARIES
    NRD_LIBRARY
    NRD_LIBRARY_DIR
    NRD_LIBRARY_DLL
    NRD_SHADERS_DIR
    NRD_SHADERS_INCLUDE_DIR
    NRD_SHADERS_SOURCE_DIR
    NRD_SHADERS_RESOURCES_DIR
)
