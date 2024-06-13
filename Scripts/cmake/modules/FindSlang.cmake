# Prevent re-defining the package target
if(TARGET Slang::Slang)
  return()
endif()

if(WIN32)
    set(Slang_BUILD_CONFIG "windows-x64")
elseif(LINUX)
    set(Slang_BUILD_CONFIG "linux-x64")
elseif(APPLE)
    set(Slang_BUILD_CONFIG "macosx-aarch64")
endif()

# Find the Slang include path
if (DEFINED Slang_ROOT)
    # Prioritize the Slang installed at ${Slang_ROOT}
    find_path(Slang_INCLUDE_DIR      # Set variable Slang_INCLUDE_DIR
              slang.h                # Find a path with Slang.h
              NO_DEFAULT_PATH
              PATHS "${Slang_ROOT}"
              DOC "path to Slang header files"
    )
    find_library(Slang_LIBRARY       # Set variable Slang_LIBRARY
                 slang               # Find library path with libslang.so, slang.dll, or slang.lib
                 NO_DEFAULT_PATH
                 PATHS "${Slang_ROOT}/bin/${Slang_BUILD_CONFIG}"
                 PATH_SUFFIXES release debug
                 DOC "path to slang library files"
    )
    find_program(Slang_COMPILER       # Set variable Slang_COMPILER
                 slangc               # Find executable path with slangc
                 NO_DEFAULT_PATH
                 PATHS "${Slang_ROOT}/bin/${Slang_BUILD_CONFIG}"
                 PATH_SUFFIXES release debug
                 DOC "path to slangc compiler executable"
    )
endif()
# Once the prioritized find_path succeeds the result variable will be set and stored in the cache
# so that no call will search again.
find_path(Slang_INCLUDE_DIR      # Set variable Slang_INCLUDE_DIR
          slang.h                # Find a path with Slang.h
          DOC "path to Slang header files"
)
find_library(Slang_LIBRARY       # Set variable Slang_LIBRARY
             slang               # Find library path with libslang.so, slang.dll, or slang.lib
             DOC "path to slang library files"
)
find_program(Slang_COMPILER       # Set variable Slang_LIBRARY
             slangc               # Find library path with libslang.so, slang.dll, or slang.lib
             DOC "path to slangc compiler executable"
)
 
set(Slang_LIBRARIES ${Slang_LIBRARY})
set(Slang_INCLUDE_DIRS ${Slang_INCLUDE_DIR})

get_filename_component(Slang_LIBRARY_DIR ${Slang_LIBRARY} DIRECTORY)
if(WIN32)
    file(GLOB Slang_LIBRARY_DLL ${Slang_LIBRARY_DIR}/*.dll)
endif()

include(FindPackageHandleStandardArgs)
# Handle the QUIETLY and REQUIRED arguments and set Slang_FOUND to TRUE
# if all listed variables are TRUE
find_package_handle_standard_args(Slang
                                  DEFAULT_MSG
                                  Slang_INCLUDE_DIRS
                                  Slang_LIBRARIES
)

if(Slang_FOUND)
    add_library(Slang::Slang UNKNOWN IMPORTED)
 	set_target_properties(Slang::Slang PROPERTIES
		IMPORTED_LOCATION ${Slang_LIBRARY}
		IMPORTED_LINK_INTERFACE_LIBRARIES "${Slang_LIBRARIES}"
        INTERFACE_INCLUDE_DIRECTORIES "${Slang_INCLUDE_DIRS}"
    )
endif()

mark_as_advanced(
    Slang_INCLUDE_DIRS
    Slang_INCLUDE_DIR
    Slang_LIBRARIES
    Slang_LIBRARY
    Slang_LIBRARY_DIR
    Slang_LIBRARY_DLL
)
