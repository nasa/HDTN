# HDTN CLI library
add_library(hdtn_cli_lib
	src/HdtnCliRunner.cpp
)
GENERATE_EXPORT_HEADER(hdtn_cli_lib)
get_target_property(target_type hdtn_cli_lib TYPE)
if (target_type STREQUAL SHARED_LIBRARY)
	set_property(TARGET hdtn_cli_lib PROPERTY CXX_VISIBILITY_PRESET hidden)
	set_property(TARGET hdtn_cli_lib PROPERTY VISIBILITY_INLINES_HIDDEN ON)
endif()
set(MY_PUBLIC_HEADERS
    include/HdtnCliRunner.h
	${CMAKE_CURRENT_BINARY_DIR}/hdtn_cli_lib_export.h
)
set_target_properties(hdtn_cli_lib PROPERTIES PUBLIC_HEADER "${MY_PUBLIC_HEADERS}")
target_include_directories(hdtn_cli_lib
	PUBLIC
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
	    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}> # for GENERATE_EXPORT_HEADER
)
target_link_libraries(hdtn_cli_lib PUBLIC hdtn_util log_lib telemetry_definitions config_lib Boost::program_options)
install(TARGETS hdtn_cli_lib
	EXPORT hdtn_cli_lib-targets
	DESTINATION "${CMAKE_INSTALL_LIBDIR}"
	PUBLIC_HEADER DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)
add_hdtn_package_export(hdtn_cli_lib HDTNCliLib)

# HDTN CLI executable
add_executable(hdtn-cli
	src/HdtnCliMain.cpp
)
install(TARGETS hdtn-cli DESTINATION ${CMAKE_INSTALL_BINDIR})
target_link_libraries(hdtn-cli hdtn_cli_lib)