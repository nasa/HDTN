add_library(slip_over_uart_lib
    src/slip.c
	src/UartInterface.cpp
)
#target_compile_options(slip_over_uart_lib PRIVATE ${NON_WINDOWS_HARDWARE_ACCELERATION_FLAGS})
GENERATE_EXPORT_HEADER(slip_over_uart_lib)
get_target_property(target_type slip_over_uart_lib TYPE)
if (target_type STREQUAL SHARED_LIBRARY)
	set_property(TARGET slip_over_uart_lib PROPERTY CXX_VISIBILITY_PRESET hidden)
	set_property(TARGET slip_over_uart_lib PROPERTY VISIBILITY_INLINES_HIDDEN ON)
endif()
set(MY_PUBLIC_HEADERS
    include/slip.h
	include/UartInterface.h
	${CMAKE_CURRENT_BINARY_DIR}/slip_over_uart_lib_export.h
)
set_target_properties(slip_over_uart_lib PROPERTIES PUBLIC_HEADER "${MY_PUBLIC_HEADERS}") # this needs to be a list, so putting in quotes makes it a ; separated list
target_link_libraries(slip_over_uart_lib
	PUBLIC
		hdtn_util
		telemetry_definitions
		log_lib
)
target_include_directories(slip_over_uart_lib
	PUBLIC
		$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
		$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}> # for GENERATE_EXPORT_HEADER
)
install(TARGETS slip_over_uart_lib
	EXPORT slip_over_uart_lib-targets
	DESTINATION "${CMAKE_INSTALL_LIBDIR}"
	PUBLIC_HEADER DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)
add_hdtn_package_export(slip_over_uart_lib SlipOverUartLib) #exported target will have the name HDTN::SlipOverUartLib and not slip_over_uart_lib.  Also requires install to EXPORT slip_over_uart_lib-targets
