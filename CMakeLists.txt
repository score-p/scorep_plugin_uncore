cmake_minimum_required(VERSION 2.8)
project(upe_plugin)

option(BACKEND_SCOREP "Build plugin using scorep(ON) or vampirtrace(OFF)" ON)
option(X86_ADAPT "Using x86 adapt instead of perf for instrumentating the performance counters" OFF)

set(SCOREP_FOUND false)

set(PFM_INC "" CACHE PATH "pfm include directory")
set(PLUGIN_SOURCE uncore_perf_plugin.c)
set(PLUGIN_LINK_LIBS pthread m)


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

if(X86_ADAPT)
    set(LIBX86A_NAME "libx86_adapt.so")
    include(common/FindX86Adapt.cmake)
    add_definitions(-DX86_ADAPT)
    if(X86_ADAPT_FOUND)
        include_directories(${X86_ADAPT_INC_DIR})
        add_definitions("-DX86_ADAPT")
        set(PLUGIN_SOURCE ${PLUGIN_SOURCE} x86a_wrapper.c)
        set(PLUGIN_LINK_LIBS ${PLUGIN_LINK_LIBS} x86_adapt)
    else()
        message(SEND_ERROR "Could not find x86 adapt")
    endif()
endif()

include(common/FindPAPI.cmake)
find_path(PFM_INC_DIR "perfmon/pfmlib.h" HINTS ${PFM_INC} ${PFM_INC}/include
    ${PAPI_INC_DIR} ${PAPI_INC}/libpfm4/include)
if(PFM_INC_DIR)
    include_directories(${PFM_INC_DIR})
else()
    message(SEND_ERROR "Could not find lib pfm header. \
    use -DPFM_INC=<path to pfm src directory> \
    e.g. -DPFM_INC=~/papi/src/libpfm4 or use -DPAPI_INC=~/papi/src")
endif()


#additional c flags
set(CMAKE_C_FLAGS "-D_GNU_SOURCE -std=c11 -O2")
#debugging c flags
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -DHAVE_DEBUG -O0 -g3 -Wstrict-prototypes -Wall -Wundef -Wno-long-long -Wsign-compare -Wcomment -pedantic -finline-functions -fno-strict-aliasing")
#release c flags
set(CMAKE_C_FLAGS_RELEASE "-Os")

add_library(${PROJECT_NAME} SHARED ${PLUGIN_SOURCE})
target_link_libraries(${PROJECT_NAME} ${PLUGIN_LINK_LIBS})

install(TARGETS ${PROJECT_NAME} LIBRARY DESTINATION lib)
