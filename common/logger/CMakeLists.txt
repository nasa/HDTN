file(MAKE_DIRECTORY ../../logs)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
add_library(log_lib
      	src/Logger.cpp
)

#Log output options
set_property(
	SOURCE src/Logger.cpp
	PROPERTY COMPILE_DEFINITIONS
		$<$<BOOL:${LOG_TO_CONSOLE}>:LOG_TO_CONSOLE> 
		$<$<BOOL:${LOG_TO_PROCESS_FILE}>:LOG_TO_PROCESS_FILE>
		$<$<BOOL:${LOG_TO_SUBPROCESS_FILES}>:LOG_TO_SUBPROCESS_FILES>
		$<$<BOOL:${LOG_TO_ERROR_FILE}>:LOG_TO_ERROR_FILE>
)


GENERATE_EXPORT_HEADER(log_lib)
get_target_property(target_type log_lib TYPE)
if (target_type STREQUAL SHARED_LIBRARY)
	set_property(TARGET log_lib PROPERTY CXX_VISIBILITY_PRESET hidden)
	set_property(TARGET log_lib PROPERTY VISIBILITY_INLINES_HIDDEN ON)
endif()
set(MY_PUBLIC_HEADERS
    include/Logger.h
	${CMAKE_CURRENT_BINARY_DIR}/log_lib_export.h
)
set_target_properties(log_lib PROPERTIES PUBLIC_HEADER "${MY_PUBLIC_HEADERS}") # this needs to be a list, so putting in quotes makes it a ; separated list
target_link_libraries(log_lib
	PUBLIC
		Boost::boost #boost headers
		Boost::log
)
target_include_directories(log_lib
	PUBLIC
		$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
		$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}> # for GENERATE_EXPORT_HEADER
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include> # for HdtnVersion.hpp
)
install(TARGETS log_lib
	EXPORT log_lib-targets
	DESTINATION "${CMAKE_INSTALL_LIBDIR}"
	PUBLIC_HEADER DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)
add_hdtn_package_export(log_lib LoggerLib) #exported target will have the name HDTN::LoggerLib and not log_lib.  Also requires install to EXPORT log_lib-targets
