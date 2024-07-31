# Prevent re-defining the package target
if(TARGET glm::glm)
  return()
endif()

# Find the stb include path
if (DEFINED glm_ROOT)
    find_path(GLM_INCLUDE_DIR       # Set variable GLM_INCLUDE_DIR
              glm/glm.hpp           # Find a path with glm.hpp
              NO_DEFAULT_PATH
              PATHS "${glm_ROOT}")
endif()
find_path(GLM_INCLUDE_DIR glm/glm.hpp)

set(GLM_INCLUDE_DIRS ${GLM_INCLUDE_DIR})

# Handles the REQUIRED, QUIET and version-related arguments of find_package().
# It also sets the <PackageName>_FOUND variable.
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(glm DEFAULT_MSG GLM_INCLUDE_DIRS)

if(glm_FOUND)
    add_library(glm::glm INTERFACE IMPORTED)
    set_target_properties(glm::glm PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES ${GLM_INCLUDE_DIRS})
endif()

mark_as_advanced(glm_ROOT)
