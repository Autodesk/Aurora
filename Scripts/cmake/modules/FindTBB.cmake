# Prevent re-defining the package target
if(TARGET TBB::TBB OR TARGET TBB::tbb)
  return()
endif()

find_path(TBB_INCLUDE_DIRS # Set variable TBB_INCLUDE_DIRS
          tbb/tbb.h        # Find a path with tbb.h
          HINTS ${TBB_ROOT}
          PATH_SUFFIXES include
          REQUIRED
)
cmake_path(GET TBB_INCLUDE_DIRS PARENT_PATH TBB_INSTALL_PREFIX)


##################################
# Find TBB components
##################################

set(TBB_COMPOMPONENTS tbb tbbmalloc tbbmalloc_proxy)

add_library(TBB::TBB INTERFACE IMPORTED)
set_target_properties(TBB::TBB PROPERTIES
  INTERFACE_INCLUDE_DIRECTORIES  ${TBB_INCLUDE_DIRS}
)

foreach(_comp ${TBB_COMPOMPONENTS})
  add_library(TBB::${_comp} SHARED IMPORTED)

  set_target_properties(TBB::${_comp} PROPERTIES
    INTERFACE_COMPILE_DEFINITIONS "$<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>:TBB_USE_DEBUG=1>"
    INTERFACE_INCLUDE_DIRECTORIES "${TBB_INCLUDE_DIR}"
  )

  find_library(TBB_${_comp}_LIBRARY_RELEASE ${_comp})
  if(TBB_${_comp}_LIBRARY_RELEASE)
    list(APPEND TBB_LIBRARIES_RELEASE "${TBB_${_comp}_LIBRARY_RELEASE}")
    set_property(TARGET TBB::${_comp} APPEND PROPERTY
      IMPORTED_CONFIGURATIONS RELEASE
    )
    if(WIN32)
      set_target_properties(TBB::${_comp} PROPERTIES
        IMPORTED_IMPLIB_RELEASE "${TBB_${_comp}_LIBRARY_RELEASE}"
      )
      find_file(TBB_${_comp}_DLL_RELEASE ${_comp}.dll REQUIRED
        PATHS "${TBB_INSTALL_PREFIX}/bin" "${TBB_INSTALL_PREFIX}/lib"
      )
      set_target_properties(TBB::${_comp} PROPERTIES
        IMPORTED_LOCATION_RELEASE "${TBB_${_comp}_DLL_RELEASE}"
      )
    elseif(APPLE)
      find_file(TBB_${_comp}_LIBRARY_RELEASE ${_comp}.dylib REQUIRED
        PATHS "${TBB_INSTALL_PREFIX}/lib"
      )
      set_target_properties(TBB::${_comp} PROPERTIES
        IMPORTED_LOCATION_RELEASE "${TBB_${_comp}_LIBRARY_RELEASE}"
      )
    else() # linux
      cmake_path(GET TBB_${_comp}_LIBRARY_RELEASE FILENAME TBB_${_comp}_SO)
      set_target_properties(TBB::${_comp} PROPERTIES
        IMPORTED_LOCATION_RELEASE "${TBB_${_comp}_LIBRARY_RELEASE}"
        IMPORTED_SONAME_RELEASE "${TBB_${_comp}_SO}"
      )
      unset(TBB_${_comp}_SO)
    endif()
  endif()

  find_library(TBB_${_comp}_LIBRARY_DEBUG ${_comp}_debug)
  if(TBB_${_comp}_LIBRARY_DEBUG)
    list(APPEND TBB_LIBRARIES_DEBUG "${TBB_${_comp}_LIBRARY_DEBUG}")
    set_property(TARGET TBB::${_comp} APPEND PROPERTY
      IMPORTED_CONFIGURATIONS DEBUG
    )
    if(WIN32)
      set_target_properties(TBB::${_comp} PROPERTIES
        IMPORTED_IMPLIB_DEBUG "${TBB_${_comp}_LIBRARY_DEBUG}"
      )
      find_file(TBB_${_comp}_DLL_DEBUG ${_comp}_debug.dll REQUIRED
        PATHS "${TBB_INSTALL_PREFIX}/bin" "${TBB_INSTALL_PREFIX}/lib"
      )
      set_target_properties(TBB::${_comp} PROPERTIES
        IMPORTED_LOCATION_DEBUG "${TBB_${_comp}_DLL_DEBUG}"
      )
    elseif(APPLE)
      find_file(TBB_${_comp}_LIBRARY_DEBUG ${_comp}_debug.dylib REQUIRED
        PATHS "${TBB_INSTALL_PREFIX}/lib"
      )
      set_target_properties(TBB::${_comp} PROPERTIES
        IMPORTED_LOCATION_DEBUG "${TBB_${_comp}_LIBRARY_DEBUG}"
      )
    else() # linux
      cmake_path(GET TBB_${_comp}_LIBRARY_DEBUG FILENAME TBB_${_comp}_SO)
      set_target_properties(TBB::${_comp} PROPERTIES
        IMPORTED_LOCATION_DEBUG "${TBB_${_comp}_LIBRARY_DEBUG}"
        IMPORTED_SONAME_DEBUG "${TBB_${_comp}_SO}"
      )
      unset(TBB_${_comp}_SO)
    endif()
  endif()

  set_property(TARGET TBB::TBB APPEND PROPERTY
    INTERFACE_LINK_LIBRARIES TBB::${_comp}
  )
endforeach()

if (TBB_LIBRARIES_RELEASE)
  set(TBB_LIBRARIES ${TBB_LIBRARIES_RELEASE})
elseif (TBB_LIBRARIES_DEBUG)
  set(TBB_LIBRARIES ${TBB_LIBRARIES_DEBUG})
endif()

set(TBB_LIBRARY_DIR "${TBB_INSTALL_PREFIX}/lib")

include(FindPackageHandleStandardArgs)
# handle the QUIETLY and REQUIRED arguments and set TBB_FOUND to TRUE
# if all listed variables are TRUE
find_package_handle_standard_args(TBB
                                  DEFAULT_MSG
                                  TBB_INCLUDE_DIRS
                                  TBB_LIBRARIES
)

mark_as_advanced(
    TBB_INCLUDE_DIRS
    TBB_LIBRARY_DIR
    TBB_LIBRARIES
    TBB_LIBRARIES_RELEASE
    TBB_LIBRARIES_DEBUG
)
