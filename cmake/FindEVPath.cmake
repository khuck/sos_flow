# Find EVPath
# ~~~~~~~~~~~~
# Copyright (c) 2017, Kevin Huck <khuck at cs.uoregon.edu>
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.
#
# CMake module to search for EVPath library
#
# If it's found it sets EVPath_FOUND to TRUE
# and following variables are set:
#    EVPath_INCLUDE_DIR
#    EVPath_LIBRARIES

# First, look in only one variable, EVPath_DIR.  This script will accept any of:
# EVPATH_DIR, EVPATH_ROOT, EVPath_DIR, EVPath_ROOT, or environment variables
# using the same set of names.


if ("${EVPath_DIR} " STREQUAL " ")
    IF (NOT EVPath_FIND_QUIETLY)
        message("EVPath_DIR not set, trying alternatives...")
    ENDIF (NOT EVPath_FIND_QUIETLY)

    # All upper case options
    if (DEFINED EVPATH_DIR)
        set(EVPath_DIR ${EVPATH_DIR})
    endif (DEFINED EVPATH_DIR)
    if (DEFINED EVPATH_ROOT)
        set(EVPath_DIR ${EVPATH_ROOT})
    endif (DEFINED EVPATH_ROOT)
    if (DEFINED ENV{EVPATH_DIR})
        set(EVPath_DIR $ENV{EVPATH_DIR})
    endif (DEFINED ENV{EVPATH_DIR})
    if (DEFINED ENV{EVPATH_ROOT})
        set(EVPath_DIR $ENV{EVPATH_ROOT})
    endif (DEFINED ENV{EVPATH_ROOT})
    if (DEFINED ENV{FLEXPATH_DIR})
        set(EVPath_DIR $ENV{FLEXPATH_DIR})
    endif (DEFINED ENV{FLEXPATH_DIR})

    # Mixed case options
    if (DEFINED EVPath_ROOT)
        set(EVPath_DIR ${EVPath_ROOT})
    endif (DEFINED EVPath_ROOT)
    if (DEFINED ENV{EVPath_DIR})
        set(EVPath_DIR $ENV{EVPath_DIR})
    endif (DEFINED ENV{EVPath_DIR})
    if (DEFINED ENV{EVPath_ROOT})
        set(EVPath_DIR $ENV{EVPath_ROOT})
    endif (DEFINED ENV{EVPath_ROOT})
endif ("${EVPath_DIR} " STREQUAL " ")

IF (NOT EVPath_FIND_QUIETLY)
MESSAGE(STATUS "EVPath_DIR set to: '${EVPath_DIR}'")
ENDIF (NOT EVPath_FIND_QUIETLY)


# First, see if the evpath_config program is in our path.  
# If so, use it.

if(NOT EVPath_FIND_QUIETLY)
    message("FindEVPath: looking for evpath_config")
endif()
find_program(EVPath_CONFIG NAMES evpath_config 
             PATHS 
             "${EVPath_DIR}/bin"
             NO_DEFAULT_PATH)

if(EVPath_CONFIG)
    IF (NOT EVPath_FIND_QUIETLY)
        message("FindEVPath: run ${EVPath_CONFIG}")
    ENDIF (NOT EVPath_FIND_QUIETLY)
    execute_process(COMMAND ${EVPath_CONFIG} "-s"
        OUTPUT_VARIABLE evpath_config_out
        RESULT_VARIABLE evpath_config_ret
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    IF (NOT EVPath_FIND_QUIETLY)
        message("FindEVPath: return value = ${evpath_config_ret}")
        message("FindEVPath: output = ${evpath_config_out}")
    ENDIF (NOT EVPath_FIND_QUIETLY)
    if(evpath_config_ret EQUAL 0)
        string(REPLACE " " ";" evpath_config_list ${evpath_config_out})
        IF (NOT EVPath_FIND_QUIETLY)
            message("FindEVPath: list = ${evpath_config_list}")
        ENDIF (NOT EVPath_FIND_QUIETLY)
        set(evpath_libs)
        set(evpath_lib_hints)
        set(evpath_lib_flags)
        foreach(OPT IN LISTS evpath_config_list)
            if(OPT MATCHES "^-L(.*)")
                list(APPEND evpath_lib_hints "${CMAKE_MATCH_1}")
            elseif(OPT MATCHES "^-l(.*)")
                list(APPEND evpath_libs "${CMAKE_MATCH_1}")
            else()
                message("${OPT}")
                list(APPEND evpath_libs "${OPT}")
            endif()
        endforeach()
        set(HAVE_EVPath 1)
    endif()
    IF (NOT EVPath_FIND_QUIETLY)
        message("FindEVPath: hints = ${evpath_lib_hints}")
        message("FindEVPath: libs = ${evpath_libs}")
        message("FindEVPath: flags = ${evpath_lib_flags}")
    ENDIF (NOT EVPath_FIND_QUIETLY)
    set(EVPath_LIBRARIES)
    foreach(lib IN LISTS evpath_libs)
        find_library(evpath_${lib}_LIBRARY NAME ${lib} HINTS ${evpath_lib_hints})
        if(evpath_${lib}_LIBRARY)
            list(APPEND EVPath_LIBRARIES ${evpath_${lib}_LIBRARY})
        else()
            list(APPEND EVPath_LIBRARIES ${lib})
        endif()
    endforeach()
    set(HAVE_EVPath 1)
    set(EVPath_LIBRARIES "${EVPath_LIBRARIES}" CACHE STRING "")
    FIND_PATH(EVPath_INCLUDE_DIR evpath.h "${EVPath_DIR}/include" NO_DEFAULT_PATH)

else(EVPath_CONFIG)

    find_package(PkgConfig REQUIRED)

    message("FindEVPath: evpath_config not available.")
    message("FindEVPath: pkg_search_module for libenet...")
    pkg_search_module(EVPath REQUIRED libenet QUIET)
    # could be needed on some platforms
    message("FindEVPath: pkg_search_module for libfabric...")
    pkg_search_module(FABRIC libfabric QUIET)

    if(NOT EVPath_FOUND)
        message("FindEVPath: searching for libraries...")
        # FIND_PATH and FIND_LIBRARY normally search standard locations
        # before the specified paths. To search non-standard paths first,
        # FIND_* is invoked first with specified paths and NO_DEFAULT_PATH
        # and then again with no specified paths to search the default
        # locations. When an earlier FIND_* succeeds, subsequent FIND_*s
        # searching for the same item do nothing. 

        FIND_PATH(EVPath_INCLUDE_DIR evpath.h 
            "${EVPath_DIR}/include" NO_DEFAULT_PATH)
        FIND_PATH(EVPath_INCLUDE_DIR evpath.h)

        FIND_LIBRARY(EVPath_LIBRARIES NAMES libenet.a PATHS
            "${EVPath_DIR}/lib" NO_DEFAULT_PATH)
        FIND_LIBRARY(EVPath_LIBRARIES NAMES enet)
    endif()
endif()

IF (EVPath_INCLUDE_DIR AND EVPath_LIBRARIES)
   SET(EVPath_FOUND TRUE)
ENDIF (EVPath_INCLUDE_DIR AND EVPath_LIBRARIES)

IF (EVPath_FOUND)

   IF (NOT EVPath_FIND_QUIETLY)
      MESSAGE(STATUS "Found EVPath: ${EVPath_LIBRARIES}")
   ENDIF (NOT EVPath_FIND_QUIETLY)

ELSE (EVPath_FOUND)

   IF (EVPath_FIND_REQUIRED)
      MESSAGE(FATAL_ERROR "Could not find EVPath")
   ENDIF (EVPath_FIND_REQUIRED)

ENDIF (EVPath_FOUND)
