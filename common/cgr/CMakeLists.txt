add_library(cgr_lib
    src/libcgr.cpp
)
GENERATE_EXPORT_HEADER(cgr_lib)
get_target_property(target_type cgr_lib TYPE)
if (target_type STREQUAL SHARED_LIBRARY)
	set_property(TARGET cgr_lib PROPERTY CXX_VISIBILITY_PRESET hidden)
	set_property(TARGET cgr_lib PROPERTY VISIBILITY_INLINES_HIDDEN ON)
endif()
set(MY_PUBLIC_HEADERS
    include/libcgr.h
	${CMAKE_CURRENT_BINARY_DIR}/cgr_lib_export.h
)
set_target_properties(cgr_lib PROPERTIES PUBLIC_HEADER "${MY_PUBLIC_HEADERS}") # this needs to be a list, so putting in quotes makes it a ; separated list
target_link_libraries(cgr_lib
	PUBLIC
		hdtn_util
		log_lib
)
target_include_directories(cgr_lib
	PUBLIC
		$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
		$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}> # for GENERATE_EXPORT_HEADER
)
install(TARGETS cgr_lib
	EXPORT cgr_lib-targets
	DESTINATION "${CMAKE_INSTALL_LIBDIR}"
	PUBLIC_HEADER DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)
add_hdtn_package_export(cgr_lib HDTNCgrLib) #exported target will have the name HDTN::HDTNCgrLib and not cgr_lib. Also requires install to EXPORT cgr_lib-targets
