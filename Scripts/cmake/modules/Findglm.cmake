if(glm_ROOT AND NOT glm_DIR)
    set(glm_DIR "${glm_ROOT}/cmake/glm")
endif()
find_package(glm CONFIG ${ARGN})
