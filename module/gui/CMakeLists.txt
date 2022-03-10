add_library(hdtn_gui
    WebsocketServer.cpp   
)
GENERATE_EXPORT_HEADER(hdtn_gui)
get_target_property(target_type hdtn_gui TYPE)
if (target_type STREQUAL SHARED_LIBRARY)
	set_property(TARGET hdtn_gui PROPERTY CXX_VISIBILITY_PRESET hidden)
	set_property(TARGET hdtn_gui PROPERTY VISIBILITY_INLINES_HIDDEN ON)
endif()
set(MY_PUBLIC_HEADERS
    WebsocketServer.h
	${CMAKE_CURRENT_BINARY_DIR}/hdtn_gui_export.h
)
set_target_properties(hdtn_gui PROPERTIES PUBLIC_HEADER "${MY_PUBLIC_HEADERS}") # this needs to be a list, so putting in quotes makes it a ; separated list
target_link_libraries(hdtn_gui
	PUBLIC
		hdtn_util
		log_lib
		bpcodec
		${civetweb_LIB}
		${civetwebcpp_LIB}
)
target_include_directories(hdtn_gui
	PUBLIC
		$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}> #project does not have a /include directory
		$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}> # for GENERATE_EXPORT_HEADER
		$<BUILD_INTERFACE:${civetweb_INCLUDE}>
)
install(TARGETS hdtn_gui
	EXPORT hdtn_gui-targets
	DESTINATION "${CMAKE_INSTALL_LIBDIR}"
	PUBLIC_HEADER DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)
add_hdtn_package_export(hdtn_gui HdtnGuiLib) #exported target will have the name HDTN::HdtnGuiLib and not hdtn_gui.  Also requires install to EXPORT hdtn_gui-targets
