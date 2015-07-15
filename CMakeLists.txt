cmake_minimum_required(VERSION 2.8)
project(UNCOREPlugin)

#set(CMAKE_C_COMPILER clang)
#plugin source files
set(PLUGIN_SOURCE uncore_plugin.c uncore_base.c)

#Internal variables of X86_ENERGY for portability
set(LIBX86HEADER x86_energy.h)
set(LIBMSRHEADER msr.h)
set(LIBMSR libmsr)
set(LIBX86STAT libx86_energy_static.a)
set(LIBX86DYN libx86_energy.so)


#additional c flags
set(CMAKE_C_FLAGS "-D_GNU_SOURCE -std=c11")

#debugging c flags
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -O0 -Wstrict-prototypes -Wall -Wundef -Wno-long-long -Wsign-compare -Wcomment -pedantic -finline-functions -fno-strict-aliasing")
set(CMAKE_C_FLAGS_RELEASE "-Os")


# Find X86_ENERGY
include(common/FindX86Energy.cmake)
if(X86_ENERGY_FOUND)
    include_directories(${X86_ADAPT_INC_DIR})
    link_directories(${X86_ADAPT_LIB_DIR})
#    target_link_libraries(x86_energy ${LIBX86E_NAME} -L${X86_ENERGY_LIB_DIR}) 
else()
	message(FATAL_ERROR "Could not find libx86_energy library.")
endif()

# Find LIBMSR
include(common/FindMSR.cmake)

if(MSR_FOUND)
	include_directories(${MSR_INC_DIR})
    link_directories(${MSR_LIB_DIR})
    message(STATUS "Using ${MSR_LIB_DIR}")
else()
	message(FATAL_ERROR "Could not find libmsr library.")
endif()

include(common/FindVampirTrace.cmake)

if (VT_FOUND)
    include_directories(${VT_INCLUDE_DIRS})
else ()
	message(FATAL_ERROR "Could NOT find VampirTrace (missing VT_INC)")
endif()

# Find LIBPFM
option(PFM_DIR "Path to libpfm" "")
unset(PFM_LIB_DIR CACHE)
set(PFM_LIB libpfm.so)
find_path(PFM_LIB_DIR libpfm.so HINTS ${PFM_DIR}/ ${PFM_DIR}/src/libpfm4/lib ${PFM_DIR}/lib $ENV{LD_LIBRARY_PATH} $ENV{PAPI_LIB} ENV LD_LIBRARY_PATH DOC "Path to libpfm")

if ( PFM_LIB_DIR )
    link_directories(${PFM_LIB_DIR})
	include_directories(${PFM_LIB_DIR}/../include)
	message(STATUS "Found libpfm header at ${PFM_LIB_DIR}/../include")
else()
	message(FATAL "Could NOT find libpfm header (Missing PFM_DIR)")
endif()

# put everyting together

include_directories(${PFM_INC_DIR} $ENV{VT_INC} $ENV{PAPI_INC})
add_library(${PROJECT_NAME} SHARED ${PLUGIN_SOURCE})
target_link_libraries(${PROJECT_NAME} ${LIBMSR_NAME} ${LIBX86E_NAME} pfm pthread m)
if(${X86_ENERGY_BUILD})
    add_dependencies(${PROJECT_NAME} ${LIBDYN})
endif(${X86_ENERGY_BUILD})

message(STATUS "INCLUDE_DIRECTORIES=${INCLUDE_DIRECTORIES}")
