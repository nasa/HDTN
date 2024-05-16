# Gstreamer Dependencies
find_package(PkgConfig REQUIRED)
pkg_search_module(gstreamer REQUIRED IMPORTED_TARGET gstreamer-1.0>=1.4)
pkg_search_module(gstreamer-sdp REQUIRED IMPORTED_TARGET gstreamer-sdp-1.0>=1.4)
pkg_search_module(gstreamer-app REQUIRED IMPORTED_TARGET gstreamer-app-1.0>=1.4)
pkg_search_module(gstreamer-video REQUIRED IMPORTED_TARGET gstreamer-video-1.0>=1.4)

find_package(ZLIB) #for compression
find_library(AVCODEC_LIBRARY avcodec)

add_library(streaming_lib
	BpInduct/src/GStreamerAppSinkInduct.cpp
	BpInduct/src/GStreamerShmInduct.cpp
	BpOutduct/src/GStreamerAppSrcOutduct.cpp
	src/DtnFrameQueue.cpp
	src/DtnRtp.cpp
)
GENERATE_EXPORT_HEADER(streaming_lib)
get_target_property(target_type streaming_lib TYPE)
if (target_type STREQUAL SHARED_LIBRARY)
	set_property(TARGET streaming_lib PROPERTY CXX_VISIBILITY_PRESET hidden)
	set_property(TARGET streaming_lib PROPERTY VISIBILITY_INLINES_HIDDEN ON)
endif()
set(MY_PUBLIC_HEADERS
    include/AsyncListener.h
	include/DtnFrameQueue.h
	include/DtnRtp.h
	include/DtnRtpFrame.h
	include/DtnUtil.h
	BpInduct/include/GStreamerAppSinkInduct.h
	BpInduct/include/GStreamerShmInduct.h
	BpOutduct/include/GStreamerAppSrcOutduct.h
	${CMAKE_CURRENT_BINARY_DIR}/streaming_lib_export.h
)
set_target_properties(streaming_lib PROPERTIES PUBLIC_HEADER "${MY_PUBLIC_HEADERS}") # this needs to be a list, so putting in quotes makes it a ; separated list
target_link_libraries(streaming_lib
	PUBLIC
		hdtn_util
		log_lib
		udp_lib

		PkgConfig::gstreamer
		PkgConfig::gstreamer-sdp
		PkgConfig::gstreamer-app
		PkgConfig::gstreamer-video
)
target_include_directories(streaming_lib
	PUBLIC
		$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/BpInduct/include>
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/BpOutduct/include>
		$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}> # for GENERATE_EXPORT_HEADER
)
install(TARGETS streaming_lib
	EXPORT streaming_lib-targets
	DESTINATION "${CMAKE_INSTALL_LIBDIR}"
	PUBLIC_HEADER DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)
add_hdtn_package_export(streaming_lib StreamingLib) #exported target will have the name HDTN::StreamingLib and not streaming_lib.  Also requires install to EXPORT streaming_lib-targets




# BPSendStream
add_executable(bpsend_stream
	BpSendStream/src/BpSendStream.cpp
	BpSendStream/src/BpSendStreamMain.cpp
	BpSendStream/src/BpSendStreamRunner.cpp
)
install(TARGETS bpsend_stream DESTINATION ${CMAKE_INSTALL_BINDIR})
target_include_directories(bpsend_stream
	PUBLIC 
		BpSendStream/include 
)
target_link_libraries(bpsend_stream
	streaming_lib
	bp_app_patterns_lib
	Boost::program_options
) 

# BPReceiveStream
add_executable(bprecv_stream
	BpReceiveStream/src/BpReceiveStreamMain.cpp
	BpReceiveStream/src/BpReceiveStreamRunner.cpp
	BpReceiveStream/src/BpReceiveStream.cpp
)
install(TARGETS bprecv_stream DESTINATION ${CMAKE_INSTALL_BINDIR})
target_include_directories(bprecv_stream
	PUBLIC 
		BpReceiveStream/include 
)
target_link_libraries(bprecv_stream
	streaming_lib
	bp_app_patterns_lib
	Boost::program_options
) 
