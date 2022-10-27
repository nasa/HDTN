file(MAKE_DIRECTORY ../../stats)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
add_library(stats_lib
      	src/StatsWriter.cpp
)
GENERATE_EXPORT_HEADER(stats_lib)
get_target_property(target_type stats_lib TYPE)
if (target_type STREQUAL SHARED_LIBRARY)
	set_property(TARGET stats_lib PROPERTY CXX_VISIBILITY_PRESET hidden)
	set_property(TARGET stats_lib PROPERTY VISIBILITY_INLINES_HIDDEN ON)
endif()
set(MY_PUBLIC_HEADERS
    include/StatsWriter.h
	${CMAKE_CURRENT_BINARY_DIR}/stats_lib_export.h
)
set_target_properties(stats_lib PROPERTIES PUBLIC_HEADER "${MY_PUBLIC_HEADERS}") # this needs to be a list, so putting in quotes makes it a ; separated list
target_link_libraries(stats_lib
	PUBLIC
		Boost::boost #boost headers
		Boost::log
)
target_include_directories(stats_lib
	PUBLIC
		$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
		$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}> # for GENERATE_EXPORT_HEADER
)
install(TARGETS stats_lib
	EXPORT stats_lib-targets
	DESTINATION "${CMAKE_INSTALL_LIBDIR}"
	PUBLIC_HEADER DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)
add_hdtn_package_export(stats_lib StatsWriterLib) #exported target will have the name HDTN::StatsWriterLib and not stats_lib.  Also requires install to EXPORT stats_lib-targets
