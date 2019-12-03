find_path (Cereal_INCLUDE_DIR NAMES cereal/cereal.hpp)
mark_as_advanced (Cereal_INCLUDE_DIR)

if (Cereal_INCLUDE_DIR)
    find_file (Cereal_VERSION_FILE version.hpp PATHS ${Cereal_INCLUDE_DIR}/cereal/)

    if (NOT Cereal_VERSION_FILE)
	    message (STATUS "Could not find cereal version header! probably pre version 1.3.0")

    else ()
        file (STRINGS ${Cereal_INCLUDE_DIR}/cereal/version.hpp _ver_line
              REGEX "^#define CEREAL_VERSION_[A-Z]+  *[0-9]+"
              )
        string (REGEX MATCH "MAJOR  *[0-9]+" Cereal_VERSION_MAJOR "${_ver_line}[0]")
        string (REGEX MATCH "[0-9]+"         Cereal_VERSION_MAJOR ${Cereal_VERSION_MAJOR})
        string (REGEX MATCH "MINOR  *[0-9]+" Cereal_VERSION_MINOR "${_ver_line}[1]")
        string (REGEX MATCH "[0-9]+"         Cereal_VERSION_MINOR ${Cereal_VERSION_MINOR})
        string (REGEX MATCH "PATCH  *[0-9]+" Cereal_VERSION_PATCH "${_ver_line}[2]")
        string (REGEX MATCH "[0-9]+"         Cereal_VERSION_PATCH ${Cereal_VERSION_PATCH})
        set (Cereal_VERSION ${Cereal_VERSION_MAJOR}.${Cereal_VERSION_MINOR}.${Cereal_VERSION_PATCH})
        unset (_ver_line)
    endif ()
endif ()

include (${CMAKE_CURRENT_LIST_DIR}/FindPackageHandleStandardArgs.cmake)
find_package_handle_standard_args (Cereal
    REQUIRED_VARS Cereal_INCLUDE_DIR
    VERSION_VAR Cereal_VERSION
    )

if (Cereal_FOUND)
    set (Cereal_INCLUDE_DIRS ${Cereal_INCLUDE_DIR})
endif ()
