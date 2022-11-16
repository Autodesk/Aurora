# Find the NRI include path
if (DEFINED NRI_ROOT)
    # Prioritize the NRI installed at ${NRI_ROOT}
    find_path(NRI_INCLUDE_DIR      # Set variable NRI_INCLUDE_DIR
              NRI.h                # Find a path with NRI.h
              NO_DEFAULT_PATH
              PATHS "${NRI_ROOT}/Include"
              DOC "path to NVIDIA NRI SDK header files"
    )
    find_library(NRI_LIBRARY       # Set variable NRI_LIBRARY
                 NRI               # Find library path with libNRI.so, NRI.dll or NRI.lib
                 NO_DEFAULT_PATH
                 PATHS "${NRI_ROOT}/Lib"
                 PATH_SUFFIXES Release RelWithDebInfo Debug
                 DOC "path to NVIDIA NRI SDK library files"
    )
endif()
# Once the privous find_path succeeds the result variable will be set and stored in the cache
# so that no call will search again.
find_path(NRI_INCLUDE_DIR      # Set variable NRI_INCLUDE_DIR
          NRI.h                # Find a path with NRI.h
          DOC "path to NVIDIA NRI SDK header files"
)
find_library(NRI_LIBRARY       # Set variable NRI_INCLUDE_DIR
             NRI               # Find library path with libNRI.so, NRI.dll or NRI.lib
             DOC "path to NVIDIA NRI SDK library files"
)

set(NRI_LIBRARIES ${NRI_LIBRARY})
set(NRI_INCLUDE_DIRS ${NRI_INCLUDE_DIR})

get_filename_component(NRI_LIBRARY_DIR ${NRI_LIBRARY} DIRECTORY)
if(WIN32)
    set(NRI_LIBRARY_DLL ${NRI_LIBRARY_DIR}/NRI.dll)
endif()

include(FindPackageHandleStandardArgs)
# handle the QUIETLY and REQUIRED arguments and set NRI_FOUND to TRUE
# if all listed variables are TRUE
find_package_handle_standard_args(NRI
                                  DEFAULT_MSG
                                  NRI_INCLUDE_DIRS
                                  NRI_LIBRARIES
)

if(NRI_FOUND)
    add_library(NRI::NRI UNKNOWN IMPORTED)
 	set_target_properties(NRI::NRI PROPERTIES
		IMPORTED_LOCATION ${NRI_LIBRARY}
		IMPORTED_LINK_INTERFACE_LIBRARIES "${NRI_LIBRARIES}"
        INTERFACE_INCLUDE_DIRECTORIES "${NRI_INCLUDE_DIRS}"
    )
endif()

mark_as_advanced(
    NRI_INCLUDE_DIRS
    NRI_INCLUDE_DIR
    NRI_LIBRARIES
    NRI_LIBRARY
    NRI_LIBRARY_DIR
    NRI_LIBRARY_DLL
)
