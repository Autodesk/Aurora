# Prevent re-defining the package target
if(TARGET NRI::NRI)
  return()
endif()

find_path(NRI_INCLUDE_DIR      # Set variable NRI_INCLUDE_DIR
          NRI.h                # Find a path with NRI.h
          PATH_SUFFIXES "Include"
)

cmake_path(GET NRI_INCLUDE_DIR PARENT_PATH NRI_INSTALL_PREFIX)
set(NRI_INCLUDE_DIRS ${NRI_INCLUDE_DIR})

add_library(NRI::NRI SHARED IMPORTED)

find_library(NRI_LIBRARY_RELEASE  # Set variable NRI_LIBRARY_RELEASE
             NRI                  # Find library path with libNRI.so, NRI.dll or NRI.lib
)
if(NRI_LIBRARY_RELEASE)
  set_property(TARGET NRI::NRI APPEND PROPERTY
    IMPORTED_CONFIGURATIONS RELEASE
  )
  set_target_properties(NRI::NRI PROPERTIES
    IMPORTED_IMPLIB_RELEASE "${NRI_LIBRARY_RELEASE}"
  )
  if(WIN32)
    find_file(NRI_DLL_RELEASE NRI.dll REQUIRED
      PATHS "${NRI_INSTALL_PREFIX}/lib" "${NRI_INSTALL_PREFIX}/bin"
    )
    set_target_properties(NRI::NRI PROPERTIES
      IMPORTED_LOCATION_RELEASE "${NRI_DLL_RELEASE}"
    )
  endif()
endif()

find_library(NRI_LIBRARY_DEBUG  # Set variable NRI_LIBRARY_DEBUG
             NRId               # Find library path with libNRId.so, NRId.dll or NRId.lib
)
if(NRI_LIBRARY_DEBUG)
  set_property(TARGET NRI::NRI APPEND PROPERTY
    IMPORTED_CONFIGURATIONS DEBUG
  )
  set_target_properties(NRI::NRI PROPERTIES
    IMPORTED_IMPLIB_DEBUG "${NRI_LIBRARY_DEBUG}"
  )
  if(WIN32)
    find_file(NRI_DLL_DEBUG NRId.dll REQUIRED
      PATHS "${NRI_INSTALL_PREFIX}/lib" "${NRI_INSTALL_PREFIX}/bin"
    )
    set_target_properties(NRI::NRI PROPERTIES
      IMPORTED_LOCATION_DEBUG "${NRI_DLL_DEBUG}"
    )
  endif()
endif()

if (NRI_LIBRARY_RELEASE)
  set(NRI_LIBRARIES ${NRI_LIBRARY_RELEASE})
elseif (NRI_LIBRARY_DEBUG)
  set(NRI_LIBRARIES ${NRI_LIBRARY_DEBUG})
endif()


get_filename_component(NRI_LIBRARY_DIR ${NRI_LIBRARIES} DIRECTORY)
if(WIN32)
  file(GLOB NRI_LIBRARY_DLLS ${NRI_LIBRARY_DIR}/*.dll)
endif()

include(FindPackageHandleStandardArgs)
# handle the QUIETLY and REQUIRED arguments and set NRI_FOUND to TRUE
# if all listed variables are TRUE
find_package_handle_standard_args(NRI
                                  DEFAULT_MSG
                                  NRI_INCLUDE_DIRS
                                  NRI_LIBRARIES
)

mark_as_advanced(
    NRI_INCLUDE_DIRS
    NRI_INCLUDE_DIR
    NRI_LIBRARIES
    NRI_LIBRARY_RELEASE
    NRI_LIBRARY_DEBUG
    NRI_LIBRARY_DIR
    NRI_LIBRARY_DLLS
)
