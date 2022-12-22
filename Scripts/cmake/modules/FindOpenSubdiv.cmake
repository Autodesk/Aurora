# Prevent re-defining the package target
if(TARGET OpenSubdiv::OpenSubdiv)
  return()
endif()

find_path(OPENSUBDIV_INCLUDE_DIRS # Set variable OPENSUBDIV_INCLUDE_DIRS
          opensubdiv/osd/mesh.h   # Find a path with mesh.h
          REQUIRED
)

##################################
# Find OpenSubdiv components
##################################

set(OPENSUBDIV_COMPOMPONENTS osdCPU osdGPU)

add_library(OpenSubdiv::OpenSubdiv INTERFACE IMPORTED)
set_target_properties(OpenSubdiv::OpenSubdiv PROPERTIES
  INTERFACE_INCLUDE_DIRECTORIES  ${OPENSUBDIV_INCLUDE_DIRS}
)
foreach(_comp ${OPENSUBDIV_COMPOMPONENTS})
  add_library(OpenSubdiv::${_comp} SHARED IMPORTED)

  find_library(OPENSUBDIV_${_comp}_LIBRARY_RELEASE ${_comp})
  if(OPENSUBDIV_${_comp}_LIBRARY_RELEASE)
    list(APPEND OPENSUBDIV_LIBRARIES_RELEASE "${OPENSUBDIV_${_comp}_LIBRARY_RELEASE}")
    set_property(TARGET OpenSubdiv::${_comp} APPEND PROPERTY
      IMPORTED_CONFIGURATIONS RELEASE
    )
    set_target_properties(OpenSubdiv::${_comp} PROPERTIES
      IMPORTED_IMPLIB_RELEASE "${OPENSUBDIV_${_comp}_LIBRARY_RELEASE}"
      IMPORTED_LOCATION_RELEASE "${OPENSUBDIV_${_comp}_LIBRARY_RELEASE}"
    )
  endif()

  find_library(OPENSUBDIV_${_comp}_LIBRARY_DEBUG ${_comp}d)
  if(OPENSUBDIV_${_comp}_LIBRARY_DEBUG)
    list(APPEND OPENSUBDIV_LIBRARIES_DEBUG "${OPENSUBDIV_${_comp}_LIBRARY_DEBUG}")
    set_property(TARGET OpenSubdiv::${_comp} APPEND PROPERTY
      IMPORTED_CONFIGURATIONS DEBUG
    )
    set_target_properties(OpenSubdiv::${_comp} PROPERTIES
      IMPORTED_IMPLIB_DEBUG "${OPENSUBDIV_${_comp}_LIBRARY_DEBUG}"
      IMPORTED_LOCATION_DEBUG "${OPENSUBDIV_${_comp}_LIBRARY_DEBUG}"
    )
  endif()

  set_property(TARGET OpenSubdiv::OpenSubdiv APPEND PROPERTY
    INTERFACE_LINK_LIBRARIES OpenSubdiv::${_comp}
  )
endforeach()

if (OPENSUBDIV_LIBRARIES_RELEASE)
  set(OPENSUBDIV_LIBRARIES ${OPENSUBDIV_LIBRARIES_RELEASE})
elseif (OPENSUBDIV_LIBRARIES_DEBUG)
  set(OPENSUBDIV_LIBRARIES ${OPENSUBDIV_LIBRARIES_DEBUG})
endif()

include(FindPackageHandleStandardArgs)
# handle the QUIETLY and REQUIRED arguments and set OpenSubdiv_FOUND to TRUE
# if all listed variables are TRUE
find_package_handle_standard_args(OpenSubdiv
                                  DEFAULT_MSG
                                  OPENSUBDIV_INCLUDE_DIRS
                                  OPENSUBDIV_LIBRARIES
)

mark_as_advanced(
    OPENSUBDIV_INCLUDE_DIRS
    OPENSUBDIV_LIBRARIES
    OPENSUBDIV_LIBRARIES_RELEASE
    OPENSUBDIV_LIBRARIES_DEBUG
)
