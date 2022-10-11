cmake_minimum_required(VERSION 3.12) #using generator $<TARGET_NAME_IF_EXISTS:tgt> which is new in version 3.12.

#
# Suggestion from https://cmake.org/pipermail/cmake/2008-September/023808.html
# If the user specifies -DCMAKE_BUILD_TYPE on the command line, take their definition
# and dump it in the cache along with proper documentation, otherwise set CMAKE_BUILD_TYPE
# to Release prior to calling PROJECT()
#
if(DEFINED CMAKE_BUILD_TYPE)
   SET(CMAKE_BUILD_TYPE ${CMAKE_BUILD_TYPE} CACHE STRING "Choose the type of build, options are: None(CMAKE_CXX_FLAGS or CMAKE_C_FLAGS used) Debug Release RelWithDebInfo MinSizeRel.")
else()
   SET(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build, options are: None(CMAKE_CXX_FLAGS or CMAKE_C_FLAGS used) Debug Release RelWithDebInfo MinSizeRel.")
endif()
string( TOLOWER "${CMAKE_BUILD_TYPE}" build_type_lower )
message("CMAKE_BUILD_TYPE = ${CMAKE_BUILD_TYPE}")
if( build_type_lower STREQUAL "debug" )
	message(WARNING "You are building in Debug mode!  HDTN can run order(s) of magnitude slower!")
endif()

include(GenerateExportHeader)
include(CheckIncludeFile)
include(CheckCXXSourceCompiles)
include(CheckCXXSourceRuns)

option(BUILD_SHARED_LIBS "Build shared libraries" Off)

set(HDTN_VERSION "1.0")
project(HDTN VERSION ${HDTN_VERSION})
set(CMAKE_CXX_STANDARD 11)

SET(CMAKE_INSTALL_LIBDIR "lib" CACHE PATH "HDTN shared and static libraries will be installed to CMAKE_INSTALL_PREFIX/CMAKE_INSTALL_LIBDIR/")
SET(CMAKE_INSTALL_INCLUDEDIR "include/HDTN" CACHE PATH "HDTN public headers will be installed to CMAKE_INSTALL_PREFIX/CMAKE_INSTALL_INCLUDEDIR/")
SET(CMAKE_INSTALL_BINDIR "bin" CACHE PATH "HDTN binaries will be installed to CMAKE_INSTALL_PREFIX/CMAKE_INSTALL_BINDIR/")

set(MIN_BOOST_VERSION 1.67.0)
find_package(Boost ${MIN_BOOST_VERSION} REQUIRED system date_time filesystem timer thread regex filesystem program_options unit_test_framework iostreams random log log_setup)
# define _WIN32_WINNT to use Windows 10 for boost
if(WIN32)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
    add_compile_definitions(_WIN32_WINNT=0x0A00)
	list(APPEND COMPILE_DEFINITIONS_TO_EXPORT _CRT_SECURE_NO_WARNINGS)
	list(APPEND COMPILE_DEFINITIONS_TO_EXPORT _WIN32_WINNT=0x0A00)
endif()

#logging link type
if(NOT Boost_USE_STATIC_LIBS)
	#add_compile_definitions(BOOST_LOG_DYN_LINK)
	add_compile_definitions(BOOST_ALL_DYN_LINK) 
endif()

#Log level definitions. These match the Boost trivial log levels.
SET(LOG_LEVEL_TRACE 0)
SET(LOG_LEVEL_DEBUG 1)
SET(LOG_LEVEL_INFO 2)
SET(LOG_LEVEL_WARNING 3)
SET(LOG_LEVEL_ERROR 4)
SET(LOG_LEVEL_FATAL 5)
SET(LOG_LEVEL_NONE 6)

#Set log level. This line can be updated to change the logging level.
SET(LOG_LEVEL ${LOG_LEVEL_INFO})

#Add logging compile definitions and validate
add_compile_definitions(LOG_LEVEL_TRACE=LOG_LEVEL_TRACE)
add_compile_definitions(LOG_LEVEL_DEBUG=LOG_LEVEL_DEBUG)
add_compile_definitions(LOG_LEVEL_INFO=LOG_LEVEL_INFO)
add_compile_definitions(LOG_LEVEL_WARNING=LOG_LEVEL_WARNING)
add_compile_definitions(LOG_LEVEL_ERROR=LOG_LEVEL_ERROR)
add_compile_definitions(LOG_LEVEL_FATAL=LOG_LEVEL_FATAL)
add_compile_definitions(LOG_LEVEL_NONE=LOG_LEVEL_NONE)
add_compile_definitions(LOG_LEVEL=LOG_LEVEL)
if (LOG_LEVEL GREATER LOG_LEVEL_INFO)
	message(WARNING "You are building with a log level of ${LOG_LEVEL}. You may miss important log messages.")
elseif (LOG_LEVEL LESS LOG_LEVEL_INFO)
	message(WARNING "You are building with a log level of ${LOG_LEVEL}. This contains extra logging that may be too verbose for production.")
endif()

#Bundle Storage Manager options
SET(STORAGE_SEGMENT_ID_SIZE_BITS "32" CACHE STRING "Set word size of storage's segment_id_t to either 32-bit (default recommended) or 64-bit (high memory only systems for greater than 17TB storage).")
if (STORAGE_SEGMENT_ID_SIZE_BITS STREQUAL "32")
	message("Defining storage segment_id_t to uint32_t")
	add_compile_definitions(STORAGE_SEGMENT_ID_SIZE_BITS=32)
elseif (STORAGE_SEGMENT_ID_SIZE_BITS STREQUAL "64")
	message("Defining storage segment_id_t to uint64_t")
	add_compile_definitions(STORAGE_SEGMENT_ID_SIZE_BITS=64)
else()
	message(FATAL_ERROR "STORAGE_SEGMENT_ID_SIZE_BITS must be set to 32 or 64 in CMakeCache.txt")
endif()
SET(STORAGE_SEGMENT_SIZE_MULTIPLE_OF_4KB "1" CACHE STRING "Set storage segment size multiple (default 1 (1*4096 bytes) recommended for 4KB block sizes).")
if (STORAGE_SEGMENT_SIZE_MULTIPLE_OF_4KB MATCHES "^[1-9][0-9]*$")
	math(EXPR SEGMENT_SIZE "${STORAGE_SEGMENT_SIZE_MULTIPLE_OF_4KB} * 4096")
	message("Defining storage segment size to ${SEGMENT_SIZE} bytes")
	add_compile_definitions("STORAGE_SEGMENT_SIZE_MULTIPLE_OF_4KB=${STORAGE_SEGMENT_SIZE_MULTIPLE_OF_4KB}")
else()
	message(FATAL_ERROR "STORAGE_SEGMENT_SIZE_MULTIPLE_OF_4KB must be set to an integer of at least 1 in CMakeCache.txt")
endif()

OPTION(USE_X86_HARDWARE_ACCELERATION "Use CPU SSE/SSE2/BMI1/BMI2 instructions" ON)
OPTION(LTP_RNG_USE_RDSEED "Use CPU RDSEED instruction as a source of randomness for LTP Random Number Generator" ON)
include(${CMAKE_SOURCE_DIR}/hardware_acceleration.cmake) # Determine if hardware acceleration can be used


if (WIN32)
	#add_compile_definitions(ZMQ_STATIC)
	set(libzmq_INCLUDE "C:/libzmq_msvc_x64/include" CACHE PATH "Folder to Libzmq headers")
	set(libzmq_LIB "C:/libzmq_msvc_x64/lib/libzmq-v141-mt-4_3_4.lib" CACHE FILEPATH "Libzmq library")
else()
	set(libzmq_INCLUDE "")
	set(libzmq_LIB zmq)
endif()

OPTION(USE_WEB_INTERFACE "Use CivetWeb HDTN Web Interface" OFF)
if(USE_WEB_INTERFACE)
	add_compile_definitions(USE_WEB_INTERFACE)
	if (WIN32)
		set(civetweb_INCLUDE "C:/civetweb_msvc2017/include" CACHE PATH "Folder to CivetServer.h and civetweb.h")
		set(civetweb_LIB "C:/civetweb_msvc2017/lib_release/civetweb.lib" CACHE FILEPATH "Civetweb library")
		set(civetwebcpp_LIB "C:/civetweb_msvc2017/lib_release/civetweb-cpp.lib" CACHE FILEPATH "Civetweb CPP library")
	else()
		set(civetweb_INCLUDE "${CMAKE_CURRENT_SOURCE_DIR}/external/include" CACHE PATH "Folder to CivetServer.h and civetweb.h")
		set(civetweb_LIB "${CMAKE_CURRENT_SOURCE_DIR}/external/lib/libcivetweb.a" CACHE FILEPATH "Civetweb library")
		set(civetwebcpp_LIB "" CACHE FILEPATH "Civetweb CPP library")
	endif()
endif()

OPTION(ENABLE_OPENSSL_SUPPORT "TcpclV4 can use OpenSSL" ON)
if(ENABLE_OPENSSL_SUPPORT)
    add_compile_definitions(OPENSSL_SUPPORT_ENABLED)
	list(APPEND COMPILE_DEFINITIONS_TO_EXPORT OPENSSL_SUPPORT_ENABLED) #used by TcpAsyncSender.h and various TcpclV4 includes
	OPTION(OPENSSL_USE_STATIC_LIBS Off)
    if (WIN32)
		set(OPENSSL_ROOT_DIR "C:/openssl-1.1.1e_msvc2017" CACHE PATH "Folder to the root directory of an OpenSSL installation")
	endif()
	find_package(OpenSSL REQUIRED)
endif()

include(${CMAKE_SOURCE_DIR}/add_hdtn_package_export.cmake)

add_subdirectory(common/bpcodec)
add_subdirectory(common/telemetry)
add_subdirectory(common/logger)
add_subdirectory(common/util)
add_subdirectory(common/config)
add_subdirectory(common/cgr)
add_subdirectory(common/induct_manager)
add_subdirectory(common/outduct_manager)
add_subdirectory(common/tcpcl)
add_subdirectory(common/ltp)
add_subdirectory(common/stcp)
add_subdirectory(common/udp)
if(USE_WEB_INTERFACE)
	add_subdirectory(module/gui)
endif()
add_subdirectory(module/storage)
add_subdirectory(module/ingress)
add_subdirectory(module/egress)
add_subdirectory(module/scheduler)
add_subdirectory(module/router)
add_subdirectory(module/hdtn_one_process)
add_subdirectory(module/udp_delay_sim)
add_subdirectory(tests/unit_tests)
add_subdirectory(tests/integrated_tests)

