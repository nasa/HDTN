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
	${CMAKE_CURRENT_BINARY_DIR}/stcp_lib_export.h
)
set_target_properties(stcp_lib PROPERTIES PUBLIC_HEADER "${MY_PUBLIC_HEADERS}") # this needs to be a list, so putting in quotes makes it a ; separated list
install(TARGETS stcp_lib
	DESTINATION "${CMAKE_INSTALL_LIBDIR}"
	PUBLIC_HEADER DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)
target_link_libraries(stcp_lib
	PUBLIC
		hdtn_util
)
target_include_directories(stcp_lib
	PUBLIC
		$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
		$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}> # for GENERATE_EXPORT_HEADER
)
