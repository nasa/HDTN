cmake_minimum_required(VERSION 3.12)  # CMake version check

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

project(dtn_rtp_streaming)          
set(CMAKE_CXX_STANDARD 11)   
set(CMAKE_VERBOSE_MAKEFILE FALSE)
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread -Wall -Wformat -std=c++2a  -g -O3 ")  #-Werror
#set(CMAKE_BUILD_TYPE RelWithDebInfo)

# Third-party Dependencies
set(MIN_BOOST_VERSION 1.66.0)
find_package(Boost ${MIN_BOOST_VERSION} REQUIRED system date_time filesystem timer thread regex filesystem unit_test_framework program_options iostreams random log log_setup)

if(WIN32)
	# define _WIN32_WINNT to use Windows 10 for boost
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
    add_compile_definitions(_WIN32_WINNT=0x0A00)
	list(APPEND COMPILE_DEFINITIONS_TO_EXPORT _CRT_SECURE_NO_WARNINGS)
	list(APPEND COMPILE_DEFINITIONS_TO_EXPORT _WIN32_WINNT=0x0A00)

	#remove any existing level 3 warnings
	string(REGEX REPLACE "/W[01234X]" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
	string(REGEX REPLACE "/W[01234X]" "" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
	#compile with warning level 4
	add_compile_options(/W4)
	#Treats all compiler warnings as errors
	add_compile_options(/WX)
	#Treat linker warnings as errors
	add_link_options(/WX)
else()
	#enable all warnings
	add_compile_options(-Wall)
	#enable some extra warning flags that are not enabled by -Wall
	add_compile_options(-Wextra)
	#Issue all the warnings demanded by strict ISO C and ISO C++
	add_compile_options(-Wpedantic)
	#Needed to disable warning for false positive in boost: -Wno-use-after-free only on integrated_tests.cpp
endif()

find_package(ZLIB) #for compression
find_library(AVCODEC_LIBRARY avcodec)

# HDTN Dependencies
SET(CMAKE_PREFIX_PATH $ENV{HDTN_INSTALL_ROOT} CACHE PATH "Path for find_package to import installed hdtn libraries")
find_package(HDTNUtil REQUIRED)
find_package(StatsLoggerLib REQUIRED)
find_package(Bpcodec REQUIRED)
find_package(TelemetryDefinitions REQUIRED)
find_package(LtpLib REQUIRED)
find_package(SlipOverUartLib REQUIRED)
find_package(OutductManagerLib REQUIRED)
find_package(BpAppPatternsLib REQUIRED)
find_package(EgressLib REQUIRED)
find_package(HDTNConfigLib REQUIRED)
find_package(HDTNCgrLib REQUIRED)
find_package(InductManagerLib REQUIRED)
find_package(IngressLib REQUIRED)
find_package(LoggerLib REQUIRED)
find_package(StcpLib REQUIRED)
find_package(StorageLib REQUIRED)
find_package(TcpclLib REQUIRED)
find_package(UdpLib REQUIRED)
find_package(UdpDelaySimLib REQUIRED)

# Gstreamer Dependencies
find_package(PkgConfig REQUIRED)
pkg_search_module(gstreamer REQUIRED IMPORTED_TARGET gstreamer-1.0>=1.4)
pkg_search_module(gstreamer-sdp REQUIRED IMPORTED_TARGET gstreamer-sdp-1.0>=1.4)
pkg_search_module(gstreamer-app REQUIRED IMPORTED_TARGET gstreamer-app-1.0>=1.4)
pkg_search_module(gstreamer-video REQUIRED IMPORTED_TARGET gstreamer-video-1.0>=1.4)


set(DTN_SOURCES 
src/DtnFrameQueue.cpp
src/DtnRtp.cpp)

set(REQUIRED_LIBS 
HDTN::HDTNUtil
HDTN::TelemetryDefinitions
HDTN::LtpLib
HDTN::BpAppPatternsLib
HDTN::Bpcodec
HDTN::EgressLib
HDTN::HDTNConfigLib
HDTN::HDTNCgrLib
HDTN::InductManagerLib
HDTN::IngressLib
HDTN::LoggerLib
HDTN::OutductManagerLib
HDTN::StcpLib
HDTN::StorageLib
HDTN::TcpclLib
HDTN::UdpLib
HDTN::UdpDelaySimLib

Boost::unit_test_framework
Boost::timer
Boost::program_options 
Boost::thread
Boost::log

PkgConfig::gstreamer
PkgConfig::gstreamer-sdp
PkgConfig::gstreamer-app
PkgConfig::gstreamer-video
)

# BPSendStream
add_executable(bpsend_stream
	BpSendStream/src/BpSendStream.cpp
	BpSendStream/src/BpSendStreamMain.cpp
	BpSendStream/src/BpSendStreamRunner.cpp
	BpInduct/src/GStreamerAppSinkInduct.cpp
	BpInduct/src/GStreamerShmInduct.cpp
	BpOutduct/src/GStreamerAppSrcOutduct.cpp
	
	${DTN_SOURCES}
)
target_include_directories(bpsend_stream PUBLIC 
		BpSendStream/include 
		include/
		BpInduct/include
		BpOutduct/include
)
target_link_libraries(bpsend_stream ${REQUIRED_LIBS}) 

# BPReceiveStream
add_executable(bprecv_stream
	BpReceiveStream/src/BpReceiveStreamMain.cpp
	BpReceiveStream/src/BpReceiveStreamRunner.cpp
	BpReceiveStream/src/BpReceiveStream.cpp
	BpOutduct/src/GStreamerAppSrcOutduct.cpp
	${DTN_SOURCES}
)
target_include_directories(bprecv_stream PUBLIC 
		BpReceiveStream/include 
		include/
		BpOutduct/include
)
target_link_libraries(bprecv_stream  ${REQUIRED_LIBS}) 
