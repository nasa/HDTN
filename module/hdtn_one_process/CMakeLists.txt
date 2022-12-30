add_executable(hdtn-one-process 
    src/HdtnOneProcessRunner.cpp
	src/HdtnOneProcessMain.cpp
)

set_property(
	SOURCE src/HdtnOneProcessRunner.cpp
	PROPERTY COMPILE_DEFINITIONS
		$<$<BOOL:${RUN_TELEMETRY}>:RUN_TELEMETRY>
)

install(TARGETS hdtn-one-process DESTINATION ${CMAKE_INSTALL_BINDIR})
target_link_libraries(hdtn-one-process
	PUBLIC
		bpcodec
		hdtn_util
		log_lib
		ingress_async_lib
		storage_lib
		egress_async_lib
		$<TARGET_NAME_IF_EXISTS:telem_lib>
		scheduler_lib
		router_lib
		Boost::program_options
)
target_include_directories(hdtn-one-process
	PUBLIC
		$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
		$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}> # for GENERATE_EXPORT_HEADER
)
