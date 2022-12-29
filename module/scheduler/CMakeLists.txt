add_library(scheduler_lib
	src/scheduler.cpp
	src/SchedulerRunner.cpp
)
GENERATE_EXPORT_HEADER(scheduler_lib)
get_target_property(target_type scheduler_lib TYPE)
if (target_type STREQUAL SHARED_LIBRARY)
	set_property(TARGET scheduler_lib PROPERTY CXX_VISIBILITY_PRESET hidden)
	set_property(TARGET scheduler_lib PROPERTY VISIBILITY_INLINES_HIDDEN ON)
endif()
set(MY_PUBLIC_HEADERS
    include/scheduler.h
	include/SchedulerRunner.h
	${CMAKE_CURRENT_BINARY_DIR}/scheduler_lib_export.h
)
set_target_properties(scheduler_lib PROPERTIES PUBLIC_HEADER "${MY_PUBLIC_HEADERS}") # this needs to be a list, so putting in quotes makes it a ; separated list
target_link_libraries(scheduler_lib
	PUBLIC
		bpcodec
		hdtn_util
		config_lib
		Boost::program_options
		log_lib
		telemetry_definitions
)
target_include_directories(scheduler_lib
	PUBLIC
		$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
		$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}> # for GENERATE_EXPORT_HEADER
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../common/include> # for message.hpp
)
install(TARGETS scheduler_lib
	EXPORT scheduler_lib-targets
	DESTINATION "${CMAKE_INSTALL_LIBDIR}"
	PUBLIC_HEADER DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)
add_hdtn_package_export(scheduler_lib SchedulerLib) #exported target will have the name HDTN::SchedulerLib and not scheduler_lib.  Also requires install to EXPORT scheduler_lib-targets




add_executable(hdtn-scheduler
    src/main.cpp
)

install(TARGETS hdtn-scheduler DESTINATION ${CMAKE_INSTALL_BINDIR})
target_link_libraries(hdtn-scheduler scheduler_lib)
