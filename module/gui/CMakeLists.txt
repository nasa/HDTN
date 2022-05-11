add_library(gui_lib
    WebsocketServer.cpp   
)
GENERATE_EXPORT_HEADER(gui_lib)
get_target_property(target_type gui_lib TYPE)
if (target_type STREQUAL SHARED_LIBRARY)
	set_property(TARGET gui_lib PROPERTY CXX_VISIBILITY_PRESET hidden)
	set_property(TARGET gui_lib PROPERTY VISIBILITY_INLINES_HIDDEN ON)
endif()
set(MY_PUBLIC_HEADERS
    WebsocketServer.h
	${CMAKE_CURRENT_BINARY_DIR}/hdtn_gui_export.h
)
set_target_properties(gui_lib PROPERTIES PUBLIC_HEADER "${MY_PUBLIC_HEADERS}") # this needs to be a list, so putting in quotes makes it a ; separated list
target_link_libraries(gui_lib
	PUBLIC
		hdtn_util
		log_lib
		bpcodec
		${civetweb_LIB}
		${civetwebcpp_LIB}
        ${CMAKE_DL_LIBS}
)
target_include_directories(gui_lib
	PUBLIC
		$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}> #project does not have a /include directory
		$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}> # for GENERATE_EXPORT_HEADER
		$<BUILD_INTERFACE:${civetweb_INCLUDE}>
)
install(TARGETS gui_lib
	EXPORT gui_lib-targets
	DESTINATION "${CMAKE_INSTALL_LIBDIR}"
	PUBLIC_HEADER DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)

add_hdtn_package_export(gui_lib HdtnGuiLib) #exported target will have the name HDTN::HdtnGuiLib and not gui_lib.  Also requires install to EXPORT gui_lib-targets

add_executable(web_interface
    main.cpp
)

install(TARGETS web_interface DESTINATION ${CMAKE_INSTALL_BINDIR})
target_link_libraries(web_interface gui_lib)

