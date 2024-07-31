if(TinyGLTF_ROOT AND NOT TinyGLTF_DIR)
    set(TinyGLTF_DIR "${TinyGLTF_ROOT}/lib/cmake")
endif()
find_package(TinyGLTF CONFIG ${ARGN})
