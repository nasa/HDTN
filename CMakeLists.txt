cmake_minimum_required(VERSION 3.12)  # CMake version check

project(dtn_rtp_streaming)          
set(CMAKE_CXX_STANDARD 11)   
set(CMAKE_VERBOSE_MAKEFILE FALSE)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread -Wall -Wformat -std=c++2a  -g -O3 ")  #-Werror
set(CMAKE_BUILD_TYPE RelWithDebInfo)

# Third-party Dependencies
set(MIN_BOOST_VERSION 1.66.0)
find_package(Boost ${MIN_BOOST_VERSION} REQUIRED system date_time filesystem timer thread regex filesystem unit_test_framework program_options iostreams random log log_setup)
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
find_package(Boost "1.67.0" REQUIRED timer unit_test_framework)

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
