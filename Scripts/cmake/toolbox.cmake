include(CMakePrintHelpers)

# Debug helper to print the variables and imported targets of a package.
function(find_package_verbose PKG_NAME)
  get_directory_property(_varsBefore VARIABLES)
  get_directory_property(_targetsBefore IMPORTED_TARGETS)

  find_package(${PKG_NAME} ${ARGN})

  get_directory_property(_vars VARIABLES)
  list(REMOVE_ITEM _vars _varsBefore ${_varsBefore} _targetsBefore )
  message(STATUS "${PKG_NAME}:")
  message(STATUS "  VARIABLES:")
  foreach(_var IN LISTS _vars)
      message(STATUS "    ${_var} = ${${_var}}")
  endforeach()

  get_directory_property(_targets IMPORTED_TARGETS)
  list(REMOVE_ITEM _targets _targetsBefore ${_targetsBefore})
  message(STATUS "  IMPORTED_TARGETS:")
  foreach(_target IN LISTS _targets)
      message(STATUS "    ${_target}")
  endforeach()
endfunction()

function(strip_path PATHLIST FILELIST)
    set(_files "")
    foreach(_path ${PATHLIST})
        cmake_path(GET _path FILENAME _file)
        list(APPEND _files ${_file})
    endforeach()
    set(${FILELIST} ${_files} PARENT_SCOPE)
endfunction()

find_package(Python3 REQUIRED) # Required to minify shaders
# Macro to setup custom command and custom build tool that minifies all of them if any has changed.
# This will remove unneeded characters and pack the shaders into a single C++ header file.
macro(minify_shaders header shader_folder shaders)
    # Set the tool override for the shaders to "NONE", this will mean VS does not attempt to compile them (though they will appear in project.)
    foreach(shader ${shaders})
        set_property(SOURCE ${shader} PROPERTY VS_TOOL_OVERRIDE "NONE")
    endforeach()

    # Add a custom command to create minified shader.
    add_custom_command(
        OUTPUT ${header}
        COMMAND ${Python3_EXECUTABLE} ${SCRIPTS_DIR}/minifyShadersFolder.py ${shader_folder} ${header} ${Slang_COMPILER}
        COMMENT "Minifying path tracing shaders to ${header}"
        DEPENDS ${shaders}
    )
endmacro()
