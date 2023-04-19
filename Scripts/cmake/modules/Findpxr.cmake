# Prevent re-defining the package target
if(TARGET pxr::usd)
  return()
endif()

find_library(USD_usdviewq_LIBRARY_RELEASE usd_usdviewq)
find_library(USD_usdviewq_LIBRARY_DEBUG usd_usdviewqd)
if(USD_usdviewq_LIBRARY_RELEASE OR USD_usdviewq_LIBRARY_DEBUG)
  # USD requires Python for usdview. If usdview librray exists, Aurora would
  # need to link to Boost python component and python librray as well.
  set(USD_COMPOMPONENTS_USDVIEW "True")

  set(Boost_USE_STATIC_LIBS OFF)
  set(Boost_NO_BOOST_CMAKE ON)

  # Iterate through the potential python3 versions to locate the Boost python component.
  set(PYTHON_MINOR_VERSIONS 12 11 10 9 8 7)
  foreach(_ver ${PYTHON_MINOR_VERSIONS})
    set(PYTHON_COMPONENT python3${_ver})
    find_package(Boost REQUIRED OPTIONAL_COMPONENTS ${PYTHON_COMPONENT})

    if(Boost_${PYTHON_COMPONENT}_FOUND)
      #  With the found Boost python component, we can now locate the python library to link to.
      add_library(Boost::python ALIAS Boost::${PYTHON_COMPONENT})
      find_package(Python3 3.${_ver} EXACT COMPONENTS Development)

      set(PYTHON_INCLUDE_DIRS "${Python3_INCLUDE_DIRS}")
      set(PYTHON_LIBRARIES ${Python3_LIBRARIES})

      break()
    endif()
  endforeach()
else()
  set(USD_COMPOMPONENTS_USDVIEW "False")
endif()

if(NOT Boost_FOUND)
    find_package(Boost REQUIRED)
endif()
if(NOT TBB_FOUND)
    find_package(TBB REQUIRED)
endif()
if(NOT OPENSUBDIV_FOUND)
    set(OPENSUBDIV_USE_GPU TRUE)
    find_package(OpenSubdiv REQUIRED)
endif()
if(NOT Vulkan_shaderc_combined_FOUND)
  # Vulkan is required by hgiVulkan
  find_package(Vulkan COMPONENTS shaderc_combined) # requires cmake 3.24
endif()
if(NOT OpenGL_FOUND)
  find_package(OpenGL REQUIRED)
endif()

find_path(PXR_INCLUDE_DIR # Set variable PXR_INCLUDE_DIR
          pxr/pxr.h       # Find a path with pxr.h
          REQUIRED
)
set(PXR_INCLUDE_DIRS "${PXR_INCLUDE_DIR}")
cmake_path(GET PXR_INCLUDE_DIR PARENT_PATH PXR_INSTALL_PREFIX)
set(PXR_LIBRARY_DIR "${PXR_INSTALL_PREFIX}/lib")
set(PXR_LIBRARY_DIRS "${PXR_LIBRARY_DIR}")

# Configure all USD targets
set(USD_COMPOMPONENTS arch tf gf js trace work plug vt ar kind sdf ndr sdr pcp usd usdGeom usdVol usdMedia usdShade usdLux usdProc usdRender usdHydra usdRi usdSkel usdUI usdUtils usdPhysics garch hf hio cameraUtil pxOsd glf hgi hgiGL hgiInterop hd hdGp hdsi hdSt hdx usdImaging usdImagingGL usdProcImaging usdRiImaging usdSkelImaging usdVolImaging usdAppUtils)

if(Vulkan_shaderc_combined_FOUND)
  list(APPEND USD_COMPOMPONENTS "hgiVulkan")
endif()

if(USD_COMPOMPONENTS_USDVIEW)
  list(APPEND USD_COMPOMPONENTS "usdviewq")
endif()

foreach(_comp ${USD_COMPOMPONENTS})
  add_library(${_comp} SHARED IMPORTED)
  set_target_properties(${_comp} PROPERTIES
    INTERFACE_COMPILE_DEFINITIONS "PXR_PYTHON_ENABLED=1"
  )

  find_library(USD_${_comp}_LIBRARY_RELEASE usd_${_comp})
  if(USD_${_comp}_LIBRARY_RELEASE)
    list(APPEND USD_LIBRARIES_RELEASE "${USD_${_comp}_LIBRARY_RELEASE}")
    set_property(TARGET ${_comp} APPEND PROPERTY
      IMPORTED_CONFIGURATIONS RELEASE
    )
    if(WIN32)
      set_target_properties(${_comp} PROPERTIES
        IMPORTED_IMPLIB_RELEASE "${USD_${_comp}_LIBRARY_RELEASE}"
      )
      cmake_path(GET USD_${_comp}_LIBRARY_RELEASE PARENT_PATH USD_${_comp}_LIBRARY_DIR_RELEASE)
      find_file(USD_${_comp}_DLL_RELEASE usd_${_comp}.dll REQUIRED
        PATHS "${USD_${_comp}_LIBRARY_DIR_RELEASE}"
      )
      set_target_properties(arch PROPERTIES
        IMPORTED_LOCATION_RELEASE "${USD_${_comp}_DLL_RELEASE}"
      )
      unset(USD_${_comp}_LIBRARY_DIR_RELEASE)
      unset(USD_${_comp}_DLL_RELEASE)
    else() # linux
      cmake_path(GET USD_${_comp}_LIBRARY_RELEASE FILENAME USD_${_comp}_SO)
      set_target_properties(${_comp} PROPERTIES
        IMPORTED_LOCATION_RELEASE "${USD_${_comp}_LIBRARY_RELEASE}"
        IMPORTED_SONAME_RELEASE "${USD_${_comp}_SO}"
      )
      unset(USD_${_comp}_SO)
    endif()
  endif()

  find_library(USD_${_comp}_LIBRARY_DEBUG usd_${_comp}d)
  if(USD_${_comp}_LIBRARY_DEBUG)
    list(APPEND USD_LIBRARIES_DEBUG "${USD_${_comp}_LIBRARY_DEBUG}")
    set_property(TARGET ${_comp} APPEND PROPERTY
      IMPORTED_CONFIGURATIONS DEBUG
    )
    if(WIN32)
      set_target_properties(${_comp} PROPERTIES
        IMPORTED_IMPLIB_DEBUG "${USD_${_comp}_LIBRARY_DEBUG}"
      )
      cmake_path(GET USD_${_comp}_LIBRARY_DEBUG PARENT_PATH USD_${_comp}_LIBRARY_DIR_DEBUG)
      find_file(USD_${_comp}_DLL_DEBUG usd_${_comp}d.dll REQUIRED
        PATHS "${USD_${_comp}_LIBRARY_DIR_DEBUG}"
      )
      set_target_properties(arch PROPERTIES
        IMPORTED_LOCATION_DEBUG "${USD_${_comp}_DLL_DEBUG}"
      )
      unset(USD_${_comp}_LIBRARY_DIR_DEBUG)
      unset(USD_${_comp}_DLL_DEBUG)
    else() # linux
      cmake_path(GET USD_${_comp}_LIBRARY_DEBUG FILENAME USD_${_comp}_SO)
      set_target_properties(${_comp} PROPERTIES
        IMPORTED_LOCATION_DEBUG "${USD_${_comp}_LIBRARY_DEBUG}"
        IMPORTED_SONAME_DEBUG "${USD_${_comp}_SO}"
      )
      unset(USD_${_comp}_SO)
    endif()
  endif()

  if (USD_${_comp}_LIBRARY_RELEASE OR USD_${_comp}_LIBRARY_DEBUG)
    add_library(pxr::${_comp} ALIAS ${_comp})
  endif()
endforeach()

if (USD_LIBRARIES_RELEASE)
  set(PXR_LIBRARIES ${USD_LIBRARIES_RELEASE})
elseif (USD_LIBRARIES_DEBUG)
  set(PXR_LIBRARIES ${USD_LIBRARIES_DEBUG})
endif()


if(WIN32)
  set_target_properties(arch PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${PXR_INCLUDE_DIRS}"
    INTERFACE_LINK_LIBRARIES "Ws2_32.lib;Dbghelp.lib"
  )
  set_target_properties(tf PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${PXR_INCLUDE_DIRS};$<$<TARGET_EXISTS:Boost::python>:${PYTHON_INCLUDE_DIRS}>;${Boost_INCLUDE_DIRS};${TBB_INCLUDE_DIRS}"
    INTERFACE_LINK_LIBRARIES "arch;Shlwapi.lib;TBB::tbb;$<$<TARGET_EXISTS:Boost::python>:${PYTHON_LIBRARIES};Boost::python>"
    INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "$<$<TARGET_EXISTS:Boost::python>:${PYTHON_INCLUDE_DIRS}>;${Boost_INCLUDE_DIRS};${TBB_INCLUDE_DIRS}"
  )
else()
  set_target_properties(arch PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${PXR_INCLUDE_DIRS}"
    INTERFACE_LINK_LIBRARIES "dl;m"
  )
  set_target_properties(tf PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${PXR_INCLUDE_DIRS};$<$<TARGET_EXISTS:Boost::python>:${PYTHON_INCLUDE_DIRS}>;${Boost_INCLUDE_DIRS};${TBB_INCLUDE_DIRS}"
    INTERFACE_LINK_LIBRARIES "arch;TBB::tbb;$<$<TARGET_EXISTS:Boost::python>:${PYTHON_LIBRARIES};Boost::python>"
    INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "$<$<TARGET_EXISTS:Boost::python>:${PYTHON_INCLUDE_DIRS}>;${Boost_INCLUDE_DIRS};${TBB_INCLUDE_DIRS}"
)
endif()
set_target_properties(gf PROPERTIES
  INTERFACE_INCLUDE_DIRECTORIES "${PXR_INCLUDE_DIRS}"
  INTERFACE_LINK_LIBRARIES "arch;tf"
)
set_target_properties(js PROPERTIES
  INTERFACE_INCLUDE_DIRECTORIES "${PXR_INCLUDE_DIRS};${Boost_INCLUDE_DIRS}"
  INTERFACE_LINK_LIBRARIES "tf"
  INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${Boost_INCLUDE_DIRS}"
)
set_target_properties(trace PROPERTIES
  INTERFACE_INCLUDE_DIRECTORIES "${PXR_INCLUDE_DIRS};${Boost_INCLUDE_DIRS};${TBB_INCLUDE_DIRS}"
  INTERFACE_LINK_LIBRARIES "arch;js;tf;TBB::tbb;$<$<TARGET_EXISTS:Boost::python>:Boost::python>"
  INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${Boost_INCLUDE_DIRS};${TBB_INCLUDE_DIRS}"
)
set_target_properties(work PROPERTIES
  INTERFACE_INCLUDE_DIRECTORIES "${PXR_INCLUDE_DIRS};${TBB_INCLUDE_DIRS}"
  INTERFACE_LINK_LIBRARIES "tf;trace;TBB::tbb"
  INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${TBB_INCLUDE_DIRS}"
)
set_target_properties(plug PROPERTIES
  INTERFACE_INCLUDE_DIRECTORIES "${PXR_INCLUDE_DIRS};${Boost_INCLUDE_DIRS};${TBB_INCLUDE_DIRS}"
  INTERFACE_LINK_LIBRARIES "arch;tf;js;trace;work;TBB::tbb;$<$<TARGET_EXISTS:Boost::python>:Boost::python>"
  INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${Boost_INCLUDE_DIRS};${TBB_INCLUDE_DIRS}"
)
set_target_properties(vt PROPERTIES
  INTERFACE_INCLUDE_DIRECTORIES "${PXR_INCLUDE_DIRS};${Boost_INCLUDE_DIRS};${TBB_INCLUDE_DIRS}"
  INTERFACE_LINK_LIBRARIES "arch;tf;gf;trace;TBB::tbb;$<$<TARGET_EXISTS:Boost::python>:Boost::python>"
  INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${Boost_INCLUDE_DIRS};${TBB_INCLUDE_DIRS}"
)
set_target_properties(ar PROPERTIES
  INTERFACE_INCLUDE_DIRECTORIES "${PXR_INCLUDE_DIRS};${Boost_INCLUDE_DIRS}"
  INTERFACE_LINK_LIBRARIES "arch;js;tf;plug;vt;$<$<TARGET_EXISTS:Boost::python>:Boost::python>"
  INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${Boost_INCLUDE_DIRS}"
)
set_target_properties(kind PROPERTIES
  INTERFACE_INCLUDE_DIRECTORIES "${PXR_INCLUDE_DIRS}"
  INTERFACE_LINK_LIBRARIES "tf;plug"
)
set_target_properties(sdf PROPERTIES
  INTERFACE_INCLUDE_DIRECTORIES "${PXR_INCLUDE_DIRS};${Boost_INCLUDE_DIRS}"
  INTERFACE_LINK_LIBRARIES "arch;tf;gf;trace;vt;work;ar;$<$<TARGET_EXISTS:Boost::python>:Boost::python>"
  INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${Boost_INCLUDE_DIRS}"
)
set_target_properties(ndr PROPERTIES
  INTERFACE_INCLUDE_DIRECTORIES "${PXR_INCLUDE_DIRS};${Boost_INCLUDE_DIRS}"
  INTERFACE_LINK_LIBRARIES "tf;plug;vt;work;ar;sdf;$<$<TARGET_EXISTS:Boost::python>:Boost::python>"
  INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${Boost_INCLUDE_DIRS}"
)
set_target_properties(sdr PROPERTIES
  INTERFACE_INCLUDE_DIRECTORIES "${PXR_INCLUDE_DIRS};${Boost_INCLUDE_DIRS}"
  INTERFACE_LINK_LIBRARIES "tf;vt;ar;ndr;sdf;$<$<TARGET_EXISTS:Boost::python>:Boost::python>"
  INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${Boost_INCLUDE_DIRS}"
)
set_target_properties(pcp PROPERTIES
  INTERFACE_INCLUDE_DIRECTORIES "${PXR_INCLUDE_DIRS};${Boost_INCLUDE_DIRS};${TBB_INCLUDE_DIRS}"
  INTERFACE_LINK_LIBRARIES "tf;trace;vt;sdf;work;ar;$<$<TARGET_EXISTS:Boost::python>:Boost::python>;TBB::tbb"
  INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${Boost_INCLUDE_DIRS};${TBB_INCLUDE_DIRS}"
)
set_target_properties(usd PROPERTIES
  INTERFACE_INCLUDE_DIRECTORIES "${PXR_INCLUDE_DIRS};${Boost_INCLUDE_DIRS};${TBB_INCLUDE_DIRS}"
  INTERFACE_LINK_LIBRARIES "arch;kind;pcp;sdf;ar;plug;tf;trace;vt;work;$<$<TARGET_EXISTS:Boost::python>:Boost::python>;TBB::tbb"
  INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${Boost_INCLUDE_DIRS};${TBB_INCLUDE_DIRS}"
)
set_target_properties(usdGeom PROPERTIES
  INTERFACE_INCLUDE_DIRECTORIES "${PXR_INCLUDE_DIRS};${Boost_INCLUDE_DIRS};${TBB_INCLUDE_DIRS}"
  INTERFACE_LINK_LIBRARIES "js;tf;plug;vt;sdf;trace;usd;work;$<$<TARGET_EXISTS:Boost::python>:Boost::python>;TBB::tbb"
  INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${Boost_INCLUDE_DIRS};${TBB_INCLUDE_DIRS}"
)
set_target_properties(usdVol PROPERTIES
  INTERFACE_INCLUDE_DIRECTORIES "${PXR_INCLUDE_DIRS}"
  INTERFACE_LINK_LIBRARIES "tf;usd;usdGeom"
)
set_target_properties(usdMedia PROPERTIES
  INTERFACE_INCLUDE_DIRECTORIES "${PXR_INCLUDE_DIRS}"
  INTERFACE_LINK_LIBRARIES "tf;vt;sdf;usd;usdGeom"
)
set_target_properties(usdShade PROPERTIES
  INTERFACE_INCLUDE_DIRECTORIES "${PXR_INCLUDE_DIRS}"
  INTERFACE_LINK_LIBRARIES "tf;vt;js;sdf;ndr;sdr;usd;usdGeom"
)
set_target_properties(usdLux PROPERTIES
  INTERFACE_INCLUDE_DIRECTORIES "${PXR_INCLUDE_DIRS}"
  INTERFACE_LINK_LIBRARIES "tf;vt;ndr;sdf;usd;usdGeom;usdShade"
)
set_target_properties(usdProc PROPERTIES
  INTERFACE_INCLUDE_DIRECTORIES "${PXR_INCLUDE_DIRS}"
  INTERFACE_LINK_LIBRARIES "tf;usd;usdGeom"
)
set_target_properties(usdRender PROPERTIES
  INTERFACE_INCLUDE_DIRECTORIES "${PXR_INCLUDE_DIRS}"
  INTERFACE_LINK_LIBRARIES "gf;tf;usd;usdGeom"
)
set_target_properties(usdHydra PROPERTIES
  INTERFACE_INCLUDE_DIRECTORIES "${PXR_INCLUDE_DIRS}"
  INTERFACE_LINK_LIBRARIES "tf;usd;usdShade"
)
set_target_properties(usdRi PROPERTIES
  INTERFACE_INCLUDE_DIRECTORIES "${PXR_INCLUDE_DIRS};${Boost_INCLUDE_DIRS}"
  INTERFACE_LINK_LIBRARIES "tf;vt;sdf;usd;usdShade;usdGeom;$<$<TARGET_EXISTS:Boost::python>:Boost::python>"
  INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${Boost_INCLUDE_DIRS}"
)
set_target_properties(usdSkel PROPERTIES
  INTERFACE_INCLUDE_DIRECTORIES "${PXR_INCLUDE_DIRS};${Boost_INCLUDE_DIRS};${TBB_INCLUDE_DIRS}"
  INTERFACE_LINK_LIBRARIES "arch;gf;tf;trace;vt;work;sdf;usd;usdGeom;$<$<TARGET_EXISTS:Boost::python>:Boost::python>;TBB::tbb"
  INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${Boost_INCLUDE_DIRS};${TBB_INCLUDE_DIRS}"
)
set_target_properties(usdUI PROPERTIES
  INTERFACE_INCLUDE_DIRECTORIES "${PXR_INCLUDE_DIRS}"
  INTERFACE_LINK_LIBRARIES "tf;vt;sdf;usd"
)
set_target_properties(usdUtils PROPERTIES
  INTERFACE_INCLUDE_DIRECTORIES "${PXR_INCLUDE_DIRS};${Boost_INCLUDE_DIRS}"
  INTERFACE_LINK_LIBRARIES "arch;tf;gf;sdf;usd;usdGeom;$<$<TARGET_EXISTS:Boost::python>:Boost::python>"
  INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${Boost_INCLUDE_DIRS}"
)
set_target_properties(usdPhysics PROPERTIES
  INTERFACE_INCLUDE_DIRECTORIES "${PXR_INCLUDE_DIRS};${Boost_INCLUDE_DIRS};${TBB_INCLUDE_DIRS}"
  INTERFACE_LINK_LIBRARIES "tf;plug;vt;sdf;trace;usd;usdGeom;usdShade;work;$<$<TARGET_EXISTS:Boost::python>:Boost::python>;TBB::tbb"
  INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${Boost_INCLUDE_DIRS};${TBB_INCLUDE_DIRS}"
)
set_target_properties(garch PROPERTIES
  INTERFACE_INCLUDE_DIRECTORIES "${PXR_INCLUDE_DIRS};${Boost_INCLUDE_DIRS}"
  INTERFACE_LINK_LIBRARIES "arch;tf;OpenGL::GL"
  INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${Boost_INCLUDE_DIRS}"
)
set_target_properties(hf PROPERTIES
  INTERFACE_INCLUDE_DIRECTORIES "${PXR_INCLUDE_DIRS}"
  INTERFACE_LINK_LIBRARIES "plug;tf;trace"
)
set_target_properties(hio PROPERTIES
  INTERFACE_INCLUDE_DIRECTORIES "${PXR_INCLUDE_DIRS};${Boost_INCLUDE_DIRS}"
  INTERFACE_LINK_LIBRARIES "arch;js;plug;tf;vt;trace;ar;hf"
  INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${Boost_INCLUDE_DIRS}"
)
set_target_properties(cameraUtil PROPERTIES
  INTERFACE_INCLUDE_DIRECTORIES "${PXR_INCLUDE_DIRS}"
  INTERFACE_LINK_LIBRARIES "tf;gf"
)
set_target_properties(pxOsd PROPERTIES
  INTERFACE_INCLUDE_DIRECTORIES "${PXR_INCLUDE_DIRS};${OPENSUBDIV_INCLUDE_DIRS}"
  INTERFACE_LINK_LIBRARIES "tf;gf;vt;OpenSubdiv::OpenSubdiv;$<$<TARGET_EXISTS:Boost::python>:Boost::python>"
  INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${OPENSUBDIV_INCLUDE_DIRS}"
)
set_target_properties(glf PROPERTIES
  INTERFACE_INCLUDE_DIRECTORIES "${PXR_INCLUDE_DIRS};${Boost_INCLUDE_DIRS}"
  INTERFACE_LINK_LIBRARIES "ar;arch;garch;gf;hf;hio;plug;tf;trace;sdf;$<$<TARGET_EXISTS:Boost::python>:Boost::python>;OpenGL::GL"
  INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${Boost_INCLUDE_DIRS}"
)
set_target_properties(hgi PROPERTIES
  INTERFACE_INCLUDE_DIRECTORIES "${PXR_INCLUDE_DIRS}"
  INTERFACE_LINK_LIBRARIES "gf;plug;tf"
)
set_target_properties(hgiGL PROPERTIES
  INTERFACE_INCLUDE_DIRECTORIES "${PXR_INCLUDE_DIRS}"
  INTERFACE_LINK_LIBRARIES "arch;garch;hgi;tf;trace"
)
set_target_properties(hd PROPERTIES
  INTERFACE_INCLUDE_DIRECTORIES "${PXR_INCLUDE_DIRS};${TBB_INCLUDE_DIRS}"
  INTERFACE_LINK_LIBRARIES "plug;tf;trace;vt;work;sdf;cameraUtil;hf;pxOsd;TBB::tbb"
  INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${TBB_INCLUDE_DIRS}"
)
set_target_properties(hdGp PROPERTIES
  INTERFACE_INCLUDE_DIRECTORIES "${PXR_INCLUDE_DIRS};${TBB_INCLUDE_DIRS}"
  INTERFACE_LINK_LIBRARIES "hd;hf;TBB::tbb"
  INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${TBB_INCLUDE_DIRS}"
)
set_target_properties(hdsi PROPERTIES
  INTERFACE_INCLUDE_DIRECTORIES "${PXR_INCLUDE_DIRS}"
  INTERFACE_LINK_LIBRARIES "plug;tf;trace;vt;work;sdf;cameraUtil;hf;hd;pxOsd"
)
set_target_properties(hdSt PROPERTIES
  INTERFACE_INCLUDE_DIRECTORIES "${PXR_INCLUDE_DIRS};${OPENSUBDIV_INCLUDE_DIRS}"
  INTERFACE_LINK_LIBRARIES "hio;garch;glf;hd;hdsi;hgiGL;hgiInterop;sdr;tf;trace;OpenSubdiv::OpenSubdiv"
  INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${OPENSUBDIV_INCLUDE_DIRS}"
)
set_target_properties(hdx PROPERTIES
  INTERFACE_INCLUDE_DIRECTORIES "${PXR_INCLUDE_DIRS}"
  INTERFACE_LINK_LIBRARIES "plug;tf;vt;gf;work;garch;glf;pxOsd;hd;hdSt;hgi;hgiInterop;cameraUtil;sdf"
)
set_target_properties(usdImaging PROPERTIES
  INTERFACE_INCLUDE_DIRECTORIES "${PXR_INCLUDE_DIRS};${TBB_INCLUDE_DIRS}"
  INTERFACE_LINK_LIBRARIES "gf;tf;plug;trace;vt;work;hd;pxOsd;sdf;usd;usdGeom;usdLux;usdRender;usdShade;usdVol;ar;TBB::tbb"
  INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${TBB_INCLUDE_DIRS}"
)
set_target_properties(usdImagingGL PROPERTIES
  INTERFACE_INCLUDE_DIRECTORIES "${PXR_INCLUDE_DIRS};$<$<TARGET_EXISTS:Boost::python>:${PYTHON_INCLUDE_DIRS}>;${TBB_INCLUDE_DIRS}"
  INTERFACE_LINK_LIBRARIES "gf;tf;plug;trace;vt;work;hio;garch;glf;hd;hdx;pxOsd;sdf;sdr;usd;usdGeom;usdHydra;usdShade;usdImaging;ar;$<$<TARGET_EXISTS:Boost::python>:Boost::python>;TBB::tbb"
  INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "$<$<TARGET_EXISTS:Boost::python>:${PYTHON_INCLUDE_DIRS}>;${TBB_INCLUDE_DIRS}"
)
set_target_properties(usdProcImaging PROPERTIES
  INTERFACE_INCLUDE_DIRECTORIES "${PXR_INCLUDE_DIRS}"
  INTERFACE_LINK_LIBRARIES "usdImaging;usdProc"
)
set_target_properties(usdRiImaging PROPERTIES
  INTERFACE_INCLUDE_DIRECTORIES "${PXR_INCLUDE_DIRS};${TBB_INCLUDE_DIRS}"
  INTERFACE_LINK_LIBRARIES "gf;tf;plug;trace;vt;work;hd;pxOsd;sdf;usd;usdGeom;usdLux;usdShade;usdImaging;usdVol;ar;TBB::tbb"
  INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${TBB_INCLUDE_DIRS}"
)
set_target_properties(usdSkelImaging PROPERTIES
  INTERFACE_INCLUDE_DIRECTORIES "${PXR_INCLUDE_DIRS}"
  INTERFACE_LINK_LIBRARIES "hio;hd;usdImaging;usdSkel"
)
set_target_properties(usdVolImaging PROPERTIES
  INTERFACE_INCLUDE_DIRECTORIES "${PXR_INCLUDE_DIRS}"
  INTERFACE_LINK_LIBRARIES "usdImaging"
)
set_target_properties(usdAppUtils PROPERTIES
  INTERFACE_INCLUDE_DIRECTORIES "${PXR_INCLUDE_DIRS};${Boost_INCLUDE_DIRS}"
  INTERFACE_LINK_LIBRARIES "garch;gf;hio;sdf;tf;usd;usdGeom;usdImagingGL;$<$<TARGET_EXISTS:Boost::python>:Boost::python>"
  INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${Boost_INCLUDE_DIRS}"
)

if(USD_COMPOMPONENTS_USDVIEW)
  set_target_properties(usdviewq PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${PXR_INCLUDE_DIRS};${Boost_INCLUDE_DIRS}"
    INTERFACE_LINK_LIBRARIES "tf;usd;usdGeom;$<$<TARGET_EXISTS:Boost::python>:Boost::python>"
    INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${Boost_INCLUDE_DIRS}"
  )
endif()

if(USD_hgiVulkan_LIBRARY_RELEASE OR USD_hgiVulkan_LIBRARY_DEBUG)
  set_target_properties(hgiVulkan PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${PXR_INCLUDE_DIRS}"
    INTERFACE_LINK_LIBRARIES "arch;hgi;tf;trace;Vulkan::Vulkan;Vulkan::shaderc_combined"
  )
endif()
set_target_properties(hgiInterop PROPERTIES
  INTERFACE_COMPILE_DEFINITIONS "PXR_PYTHON_ENABLED=1"
  INTERFACE_INCLUDE_DIRECTORIES "${PXR_INCLUDE_DIRS}"
)
if (USD_hgiVulkan_LIBRARY_RELEASE OR USD_hgiVulkan_LIBRARY_DEBUG)
  set_target_properties(hgiInterop PROPERTIES
    INTERFACE_LINK_LIBRARIES "gf;tf;hgi;vt;garch;hgiVulkan"
  )
else()
  set_target_properties(hgiInterop PROPERTIES
    INTERFACE_LINK_LIBRARIES "gf;tf;hgi;vt;garch"
  )
endif()


include(FindPackageHandleStandardArgs)
# handle the QUIETLY and REQUIRED arguments and set pxr_FOUND to TRUE
# if all listed variables are TRUE
find_package_handle_standard_args(pxr
                                  DEFAULT_MSG
                                  PXR_INCLUDE_DIRS
                                  PXR_LIBRARIES
)

mark_as_advanced(
    PXR_INCLUDE_DIRS
    PXR_INCLUDE_DIR
    PXR_LIBRARIES
    PXR_LIBRARY_DIRS
    PXR_LIBRARY_DIR
    USD_LIBRARIES_RELEASE
    USD_LIBRARIES_DEBUG
)