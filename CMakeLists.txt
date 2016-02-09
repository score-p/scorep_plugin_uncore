cmake_minimum_required(VERSION 2.8)
project(upe_plugin)

option(BACKEND_SCOREP "Build plugin using scorep(ON) or vampirtrace(OFF)" ON)

set(VT_INC "" CACHE PATH "Path of the vampirtrace headers")
set(PAPI_INC "" CACHE PATH "Path of the papi headers")
set(SCOREP_FOUND false)

set(PLUGIN_SOURCE uncore_perf_plugin.c)


if(BACKEND_SCOREP)
    include(common/FindScorep.cmake)
    if(SCOREP_FOUND)
        include_directories(${SCOREP_INCLUDE_DIRS})
        add_definitions("-DBACKEND_SCOREP")
    else()
        message("Score-P was not found, falling back to VampirTrace!")
    endif()
endif()

if(NOT SCOREP_FOUND OR NOT BACKEND_SCOREP)
    include(common/FindVampirTrace.cmake)
    if(VT_FOUND)
        include_directories(${VT_INCLUDE_DIRS})
        add_definitions("-DBACKEND_VTRACE")
    else()
        message(SEND_ERROR "Found neither Score-P nor VampirTrace backend!")
    endif()
endif()


#Find PAPI inlcude directory
unset(__PAPI_INC CACHE)
find_path(__PAPI_INC NAMES "papi.h" HINTS ${PAPI_INC} $ENV{PAPI_INC})
if(NOT IS_ABSOLUTE ${__PAPI_INC})
    message(FATAL_ERROR "Could not find PAPI headers. Specify the path with -DPAPI_INC=<path to PAPI>.")
endif(NOT IS_ABSOLUTE ${__PAPI_INC})
message(STATUS "Found PAPI headers in ${__PAPI_INC}.")


#additional c flags
set(CMAKE_C_FLAGS "-D_GNU_SOURCE -std=c11 -O2 -g")
#debugging c flags
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -DHAVE_DEBUG -O0 -Wstrict-prototypes -Wall -Wundef -Wno-long-long -Wsign-compare -Wcomment -pedantic -finline-functions -fno-strict-aliasing")
#release c flags
set(CMAKE_C_FLAGS_RELEASE "-Os")

include_directories( ${__PAPI_INC})
add_library(${PROJECT_NAME} SHARED ${PLUGIN_SOURCE})
target_link_libraries(${PROJECT_NAME}  pthread m)

install(TARGETS ${PROJECT_NAME} LIBRARY DESTINATION lib)
