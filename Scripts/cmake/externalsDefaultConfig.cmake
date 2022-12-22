
if(NOT DEFINED EXTERNALS_ROOT)
    set(EXTERNALS_ROOT "${CMAKE_SOURCE_DIR}/_externals")
else()
    if(NOT IS_DIRECTORY "${EXTERNALS_ROOT}")
        message(FATAL_ERROR "EXTERNALS_ROOT does not exist: ${EXTERNALS_ROOT}")
    endif()
endif()

set(AURORA_DEPENDENCIES "")

if(NOT DEFINED ZLIB_ROOT)
    set(ZLIB_ROOT "${EXTERNALS_ROOT}/zlib")
endif()
list(APPEND AURORA_DEPENDENCIES "${ZLIB_ROOT}")
# find_package_verbose(ZLIB)

if(NOT DEFINED JPEG_ROOT)
    set(JPEG_ROOT "${EXTERNALS_ROOT}/libjpeg")
endif()
list(APPEND AURORA_DEPENDENCIES "${JPEG_ROOT}")
# find_package_verbose(JPEG)

if(NOT DEFINED TIFF_ROOT)
    set(TIFF_ROOT "${EXTERNALS_ROOT}/libtiff")
endif()
list(APPEND AURORA_DEPENDENCIES "${TIFF_ROOT}")
# find_package_verbose(TIFF)

if(NOT DEFINED PNG_ROOT)
    set(PNG_ROOT "${EXTERNALS_ROOT}/libpng")
endif()
list(APPEND AURORA_DEPENDENCIES "${PNG_ROOT}")
# find_package_verbose(PNG)

if(NOT DEFINED glm_ROOT)
    set(glm_ROOT "${EXTERNALS_ROOT}/glm")
endif()
list(APPEND AURORA_DEPENDENCIES "${glm_ROOT}")
# find_package_verbose(glm)

if(NOT DEFINED stb_ROOT)
    set(stb_ROOT "${EXTERNALS_ROOT}/stb")
endif()
list(APPEND AURORA_DEPENDENCIES "${stb_ROOT}")
# find_package_verbose(stb)

if(NOT DEFINED TinyGLTF_ROOT)
    set(TinyGLTF_ROOT "${EXTERNALS_ROOT}/tinygltf")
endif()
list(APPEND AURORA_DEPENDENCIES "${TinyGLTF_ROOT}")
# find_package_verbose(TinyGLTF)

if(NOT DEFINED tinyobjloader_ROOT)
    set(tinyobjloader_ROOT "${EXTERNALS_ROOT}/tinyobjloader")
endif()
list(APPEND AURORA_DEPENDENCIES "${tinyobjloader_ROOT}")
# find_package_verbose(tinyobjloader)

if(NOT DEFINED Boost_ROOT)
    set(Boost_ROOT "${EXTERNALS_ROOT}/boost")
endif()
list(APPEND AURORA_DEPENDENCIES "${Boost_ROOT}")
# find_package_verbose(Boost)

if(NOT DEFINED TBB_ROOT)
    set(TBB_ROOT "${EXTERNALS_ROOT}/tbb")
endif()
list(APPEND AURORA_DEPENDENCIES "${TBB_ROOT}")
# find_package_verbose(TBB)

if(NOT DEFINED OpenEXR_ROOT)
    set(OpenEXR_ROOT "${EXTERNALS_ROOT}/OpenEXR")
endif()
list(APPEND AURORA_DEPENDENCIES "${OpenEXR_ROOT}")
# find_package_verbose(OpenEXR)

if(NOT DEFINED OpenImageIO_ROOT)
    set(OpenImageIO_ROOT "${EXTERNALS_ROOT}/OpenImageIO")
endif()
list(APPEND AURORA_DEPENDENCIES "${OpenImageIO_ROOT}")
# find_package_verbose(OpenImageIO)

if(NOT DEFINED MaterialX_ROOT)
    set(MaterialX_ROOT "${EXTERNALS_ROOT}/MaterialX")
endif()
list(APPEND AURORA_DEPENDENCIES "${MaterialX_ROOT}")
# find_package_verbose(MaterialX)

if(NOT DEFINED OpenSubdiv_ROOT)
    set(OpenSubdiv_ROOT "${EXTERNALS_ROOT}/OpenSubdiv")
endif()
list(APPEND AURORA_DEPENDENCIES "${OpenSubdiv_ROOT}")
# find_package_verbose(OpenSubdiv)

if(NOT DEFINED pxr_ROOT)
    set(pxr_ROOT "${EXTERNALS_ROOT}/USD")
endif()
list(APPEND AURORA_DEPENDENCIES "${pxr_ROOT}")
# find_package_verbose(pxr)

if(NOT DEFINED Slang_ROOT)
    set(Slang_ROOT "${EXTERNALS_ROOT}/Slang")
endif()
list(APPEND AURORA_DEPENDENCIES "${Slang_ROOT}")
# find_package_verbose(Slang)

if(NOT DEFINED GLEW_ROOT)
    set(GLEW_ROOT "${EXTERNALS_ROOT}/glew")
endif()
list(APPEND AURORA_DEPENDENCIES "${GLEW_ROOT}")
# find_package_verbose(GLEW)

if(NOT DEFINED glfw3_ROOT)
    set(glfw3_ROOT "${EXTERNALS_ROOT}/GLFW")
endif()
list(APPEND AURORA_DEPENDENCIES "${glfw3_ROOT}")
# find_package_verbose(glfw3)

if(NOT DEFINED cxxopts_ROOT)
    set(cxxopts_ROOT "${EXTERNALS_ROOT}/cxxopts")
endif()
list(APPEND AURORA_DEPENDENCIES "${cxxopts_ROOT}")
# find_package_verbose(cxxopts)

if(NOT DEFINED GTest_ROOT)
    set(GTest_ROOT "${EXTERNALS_ROOT}/gtest")
endif()
list(APPEND AURORA_DEPENDENCIES "${GTest_ROOT}")
# find_package_verbose(GTest)

# If you want to use you own build of certain external library, simply set <pkg>_ROOT
# to guide find_package() to locate your own build. External libraries required directly
# and indirectly by Aurora are:
#     pxr
#     MaterialX
#     NRI
#     NRD
#     OpenImageIO
#     glm
#     Slang
#     stb
#     PNG
#     ZLIB
#     TinyGLTF
#     tinyobjloader
#     glew
#     glfw3
#     cxxopts
#     GTest

# To debug finding the external libraries, uncomment the package you want to debug.
# find_package_verbose(D3D12)
# find_package_verbose(pxr)
# find_package_verbose(MaterialX)
# find_package_verbose(NRI)
# find_package_verbose(NRD)
# find_package_verbose(OpenImageIO)
# find_package_verbose(Slang)
# find_package_verbose(glm)
# find_package_verbose(stb)
# find_package_verbose(PNG)
# find_package_verbose(ZLIB)
# find_package_verbose(TinyGLTF)
# find_package_verbose(tinyobjloader)
# find_package_verbose(GLEW)
# find_package_verbose(glfw3)
# find_package_verbose(cxxopts)
# find_package_verbose(GTest)
