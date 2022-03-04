cmake_minimum_required(VERSION 3.0)
project(hdtn_gui)
set(CMAKE_CXX_STANDARD 11)


add_library(hdtn_gui
    WebsocketServer.cpp   
)
GENERATE_EXPORT_HEADER(hdtn_gui)
get_target_property(target_type hdtn_gui TYPE)
if (target_type STREQUAL SHARED_LIBRARY)
	set_property(TARGET hdtn_gui PROPERTY CXX_VISIBILITY_PRESET hidden)
	set_property(TARGET hdtn_gui PROPERTY VISIBILITY_INLINES_HIDDEN ON)
endif()
install(TARGETS hdtn_gui DESTINATION lib)
install(FILES
	WebsocketServer.h
	${PROJECT_BINARY_DIR}/hdtn_gui_export.h
	DESTINATION include
)
target_include_directories(hdtn_gui PUBLIC . include ${Boost_INCLUDE_DIRS} ${civetweb_INCLUDE} ${PROJECT_BINARY_DIR})
target_link_libraries(hdtn_gui ${pthread_LIB} ${Boost_LIBRARIES} ${civetweb_LIB} ${civetwebcpp_LIB})

