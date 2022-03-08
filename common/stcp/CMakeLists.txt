cmake_minimum_required(VERSION 3.0)
project(stcp_lib)
set(CMAKE_CXX_STANDARD 11)



add_library(stcp_lib
	src/StcpBundleSink.cpp
	src/StcpBundleSource.cpp
)
GENERATE_EXPORT_HEADER(stcp_lib)
get_target_property(target_type stcp_lib TYPE)
if (target_type STREQUAL SHARED_LIBRARY)
	set_property(TARGET stcp_lib PROPERTY CXX_VISIBILITY_PRESET hidden)
	set_property(TARGET stcp_lib PROPERTY VISIBILITY_INLINES_HIDDEN ON)
endif()
set(MY_PUBLIC_HEADERS
    include/StcpBundleSink.h
	include/StcpBundleSource.h
	${PROJECT_BINARY_DIR}/stcp_lib_export.h
)
set_target_properties(stcp_lib PROPERTIES PUBLIC_HEADER "${MY_PUBLIC_HEADERS}") # this needs to be a list, so putting in quotes makes it a ; separated list
install(TARGETS stcp_lib
	DESTINATION "${CMAKE_INSTALL_LIBDIR}"
	PUBLIC_HEADER DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)
target_link_libraries(stcp_lib hdtn_util ${Boost_LIBRARIES} ${pthread_LIB})
target_include_directories(stcp_lib PUBLIC include ${Boost_INCLUDE_DIRS} ${PROJECT_BINARY_DIR})


