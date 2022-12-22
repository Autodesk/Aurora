# Prevent re-defining the package target
if(TARGET stb::stb)
  return()
endif()

# Find the stb include path
if (DEFINED stb_ROOT)
    # Prioritize the stb installed at ${stb_ROOT}
    find_path(stb_INCLUDE_DIR      # Set variable stb_INCLUDE_DIR
              stb_image.h          # Find a path with stb_image.h
              NO_DEFAULT_PATH
              PATHS "${stb_ROOT}"
    )
endif()
# Once the privous find_path succeeds the result variable will be set and stored in the cache
# so that no call will search again.
find_path(stb_INCLUDE_DIR      # Set variable stb_INCLUDE_DIR
          stb_image.h          # Find a path with stb_image.h
)

set(stb_INCLUDE_DIRS ${stb_INCLUDE_DIR})

include(FindPackageHandleStandardArgs)
# handle the QUIETLY and REQUIRED arguments and set NRI_FOUND to TRUE
# if all listed variables are TRUE
find_package_handle_standard_args(stb
                                  DEFAULT_MSG
                                  stb_INCLUDE_DIRS
)

if(stb_FOUND)
    add_library(stb::stb INTERFACE IMPORTED)
    set_target_properties(stb::stb PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES ${stb_INCLUDE_DIRS}
    )
endif()

mark_as_advanced(
    stb_INCLUDE_DIRS
    stb_INCLUDE_DIR
)
