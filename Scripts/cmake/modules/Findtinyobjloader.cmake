# Find the tinyobjloader include path
if (DEFINED tinyobjloader_ROOT)
    # Prioritize the tinyobjloader installed at ${tinyobjloader_ROOT}
    find_path(tinyobjloader_INCLUDE_DIR      # Set variable tinyobjloader_INCLUDE_DIR
              tiny_obj_loader.h              # Find a path with tiny_obj_loader.h
              NO_DEFAULT_PATH
              PATHS "${tinyobjloader_ROOT}/Include"
              DOC "path to tinyobjloader header files"
    )
    find_library(tinyobjloader_LIBRARY       # Set variable tinyobjloader_LIBRARY
                 tinyobjloader               # Find library path with libtinyobjloader.so, tinyobjloader.dll or tinyobjloader.lib
                 NO_DEFAULT_PATH
                 PATHS "${tinyobjloader_ROOT}/Lib"
                 PATH_SUFFIXES Release RelWithDebInfo Debug
                 DOC "path to tinyobjloader library file"
    )
endif()
# Once the privous find_path succeeds the result variable will be set and stored in the cache
# so that no call will search again.
find_path(tinyobjloader_INCLUDE_DIR      # Set variable tinyobjloader_INCLUDE_DIR
          tiny_obj_loader.h              # Find a path with tiny_obj_loader.h
          DOC "path to tinyobjloader header files"
)
find_library(tinyobjloader_LIBRARY       # Set variable tinyobjloader_INCLUDE_DIR
             tinyobjloader               # Find library path with libtinyobjloader.so, tinyobjloader.dll or tinyobjloader.lib
             DOC "path to tinyobjloader library file"
)

set(tinyobjloader_LIBRARIES ${tinyobjloader_LIBRARY})
set(tinyobjloader_INCLUDE_DIRS ${tinyobjloader_INCLUDE_DIR})

include(FindPackageHandleStandardArgs)
# handle the QUIETLY and REQUIRED arguments and set tinyobjloader_FOUND to TRUE
# if all listed variables are TRUE
find_package_handle_standard_args(tinyobjloader
                                  DEFAULT_MSG
                                  tinyobjloader_INCLUDE_DIRS
                                  tinyobjloader_LIBRARIES
)

if(tinyobjloader_FOUND)
    add_library(tinyobjloader::tinyobjloader UNKNOWN IMPORTED)
 	set_target_properties(tinyobjloader::tinyobjloader PROPERTIES
		IMPORTED_LOCATION "${tinyobjloader_LIBRARY}"
		IMPORTED_LINK_INTERFACE_LIBRARIES "${tinyobjloader_LIBRARIES}"
        INTERFACE_INCLUDE_DIRECTORIES "${tinyobjloader_INCLUDE_DIRS}"
    )
endif()

mark_as_advanced(
    tinyobjloader_INCLUDE_DIRS
    tinyobjloader_INCLUDE_DIR
    tinyobjloader_LIBRARIES
    tinyobjloader_LIBRARY
)
