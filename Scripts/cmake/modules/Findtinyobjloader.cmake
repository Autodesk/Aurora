if(TARGET tinyobjloader::tinyobjloader)
    return()
endif()

find_path(tinyobjloader_INCLUDE_DIR # Set variable tinyobjloader_INCLUDE_DIR
          tiny_obj_loader.h         # Find a path with tiny_obj_loader.h
          REQUIRED
)
set(tinyobjloader_INCLUDE_DIRS ${tinyobjloader_INCLUDE_DIR})

add_library(tinyobjloader::tinyobjloader UNKNOWN IMPORTED)
set_target_properties(tinyobjloader::tinyobjloader PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${tinyobjloader_INCLUDE_DIRS}"
)

find_library(tinyobjloader_LIBRARY_RELEASE tinyobjloader)
if(tinyobjloader_LIBRARY_RELEASE)
    set_property(TARGET tinyobjloader::tinyobjloader APPEND PROPERTY
        IMPORTED_CONFIGURATIONS RELEASE
    )
    set_target_properties(tinyobjloader::tinyobjloader PROPERTIES
        IMPORTED_LOCATION_RELEASE "${tinyobjloader_LIBRARY_RELEASE}"
    )
endif()

find_library(tinyobjloader_LIBRARY_DEBUG tinyobjloaderd)
if(tinyobjloader_LIBRARY_DEBUG)
    set_property(TARGET tinyobjloader::tinyobjloader APPEND PROPERTY
        IMPORTED_CONFIGURATIONS DEBUG
    )
    set_target_properties(tinyobjloader::tinyobjloader PROPERTIES
        IMPORTED_LOCATION_DEBUG "${tinyobjloader_LIBRARY_DEBUG}"
    )
endif()

if (tinyobjloader_LIBRARY_RELEASE)
    set(tinyobjloader_LIBRARIES ${tinyobjloader_LIBRARY_RELEASE})
elseif (tinyobjloader_LIBRARY_DEBUG)
    set(tinyobjloader_LIBRARIES ${tinyobjloader_LIBRARY_DEBUG})
endif()

include(FindPackageHandleStandardArgs)
# handle the QUIETLY and REQUIRED arguments and set tinyobjloader_FOUND to TRUE
# if all listed variables are TRUE
find_package_handle_standard_args(tinyobjloader
                                  DEFAULT_MSG
                                  tinyobjloader_INCLUDE_DIRS
                                  tinyobjloader_LIBRARIES
)

mark_as_advanced(
    tinyobjloader_INCLUDE_DIRS
    tinyobjloader_INCLUDE_DIR
    tinyobjloader_LIBRARIES
    tinyobjloader_LIBRARY_RELEASE
    tinyobjloader_LIBRARY_DEBUG
)
