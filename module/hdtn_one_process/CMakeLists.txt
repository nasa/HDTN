add_library(hdtnOneProcess_lib
     src/HdtnOneProcessRunner.cpp
)
GENERATE_EXPORT_HEADER(hdtnOneProcess_lib)
get_target_property(target_type hdtnOneProcess_lib TYPE)
if (target_type STREQUAL SHARED_LIBRARY)
	set_property(TARGET hdtnOneProcess_lib PROPERTY CXX_VISIBILITY_PRESET hidden)
	set_property(TARGET hdtnOneProcess_lib PROPERTY VISIBILITY_INLINES_HIDDEN ON)
endif()
set(MY_PUBLIC_HEADERS
    include/HdtnOneProcessRunner.h
    ${CMAKE_CURRENT_BINARY_DIR}/hdtnOneProcess_lib_export.h
)
set_target_properties(hdtnOneProcess_lib PROPERTIES PUBLIC_HEADER "${MY_PUBLIC_HEADERS}") # this needs to be a list, so putting in quotes makes it a ; separated list
target_link_libraries(hdtnOneProcess_lib
	PUBLIC
		hdtn_util
		log_lib
		induct_manager_lib
                ingress_async_lib
		storage_lib
		egress_async_lib
		$<TARGET_NAME_IF_EXISTS:telem_lib>
		scheduler_lib
		router_lib
		bpcodec
		Boost::program_options
)
target_include_directories(hdtnOneProcess_lib
	PUBLIC
		$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
		$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}> # for GENERATE_EXPORT_HEADER
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../common/include> # for message.hpp
)
install(TARGETS hdtnOneProcess_lib
	EXPORT hdtnOneProcess_lib-targets
	DESTINATION "${CMAKE_INSTALL_LIBDIR}"
	PUBLIC_HEADER DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)
add_hdtn_package_export(hdtnOneProcess_lib HdtnOneProcessLib) #exported target will have the name HDTN::IngressLib and not ingress_async_lib.  Also requires install to EXPORT ingress_async_lib-targets

add_executable(hdtn-one-process 
     src/HdtnOneProcessMain.cpp
)

set_property(
	SOURCE src/HdtnOneProcessRunner.cpp
	PROPERTY COMPILE_DEFINITIONS
		$<$<BOOL:${RUN_TELEMETRY}>:RUN_TELEMETRY>
)

install(TARGETS hdtn-one-process DESTINATION ${CMAKE_INSTALL_BINDIR})
target_link_libraries(hdtn-one-process hdtnOneProcess_lib)
target_include_directories(hdtn-one-process
	PUBLIC
		$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
		$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}> # for GENERATE_EXPORT_HEADER
)
