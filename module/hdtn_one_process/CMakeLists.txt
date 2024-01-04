add_executable(hdtn-one-process 
     src/HdtnOneProcessMain.cpp
)
install(TARGETS hdtn-one-process DESTINATION ${CMAKE_INSTALL_BINDIR})
target_link_libraries(hdtn-one-process hdtn_one_process_lib)


add_library(hdtn_one_process_lib
     src/HdtnOneProcessRunner.cpp
)
GENERATE_EXPORT_HEADER(hdtn_one_process_lib)
get_target_property(target_type hdtn_one_process_lib TYPE)
if (target_type STREQUAL SHARED_LIBRARY)
	set_property(TARGET hdtn_one_process_lib PROPERTY CXX_VISIBILITY_PRESET hidden)
	set_property(TARGET hdtn_one_process_lib PROPERTY VISIBILITY_INLINES_HIDDEN ON)
endif()
set(MY_PUBLIC_HEADERS
    include/HdtnOneProcessRunner.h
    ${CMAKE_CURRENT_BINARY_DIR}/hdtn_one_process_lib_export.h
)

set_target_properties(hdtn_one_process_lib PROPERTIES PUBLIC_HEADER "${MY_PUBLIC_HEADERS}") # this needs to be a list, so putting in quotes makes it a ; separated list
target_link_libraries(hdtn_one_process_lib
	PUBLIC
		hdtn_util
		log_lib
		induct_manager_lib
		ingress_async_lib
		storage_lib
		egress_async_lib
		$<TARGET_NAME_IF_EXISTS:telem_lib>
		router_lib
		bpcodec
		Boost::program_options
)
target_include_directories(hdtn_one_process_lib
	PUBLIC
		$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
		$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}> # for GENERATE_EXPORT_HEADER
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../common/include> # for message.hpp
)
install(TARGETS hdtn_one_process_lib
	EXPORT hdtn_one_process_lib-targets
	DESTINATION "${CMAKE_INSTALL_LIBDIR}"
	PUBLIC_HEADER DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)
add_hdtn_package_export(hdtn_one_process_lib HdtnOneProcessLib) #exported target will have the name HDTN::HdtnOneProcessLib and not hdtn_one_process_lib.  Also requires install to EXPORT hdtn_one_process_lib-targets



set_property(
	SOURCE src/HdtnOneProcessRunner.cpp
	PROPERTY COMPILE_DEFINITIONS
		$<$<BOOL:${RUN_TELEMETRY}>:RUN_TELEMETRY>
)
