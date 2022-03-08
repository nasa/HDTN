cmake_minimum_required(VERSION 3.0)
project(udp_lib)
set(CMAKE_CXX_STANDARD 11)



add_library(udp_lib
	src/UdpBundleSink.cpp
	src/UdpBundleSource.cpp
)
GENERATE_EXPORT_HEADER(udp_lib)
get_target_property(target_type udp_lib TYPE)
if (target_type STREQUAL SHARED_LIBRARY)
	set_property(TARGET udp_lib PROPERTY CXX_VISIBILITY_PRESET hidden)
	set_property(TARGET udp_lib PROPERTY VISIBILITY_INLINES_HIDDEN ON)
endif()
set(MY_PUBLIC_HEADERS
    include/UdpBundleSink.h
	include/UdpBundleSource.h
	${PROJECT_BINARY_DIR}/udp_lib_export.h
)
set_target_properties(udp_lib PROPERTIES PUBLIC_HEADER "${MY_PUBLIC_HEADERS}") # this needs to be a list, so putting in quotes makes it a ; separated list
install(TARGETS udp_lib
	DESTINATION "${CMAKE_INSTALL_LIBDIR}"
	PUBLIC_HEADER DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)
target_link_libraries(udp_lib hdtn_util ${Boost_LIBRARIES} ${pthread_LIB})
target_include_directories(udp_lib PUBLIC include ${Boost_INCLUDE_DIRS} ${PROJECT_BINARY_DIR})


