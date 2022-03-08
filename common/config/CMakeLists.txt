cmake_minimum_required(VERSION 3.0)
project(config_lib)
set(CMAKE_CXX_STANDARD 11)



add_library(config_lib
    src/InductsConfig.cpp
	src/OutductsConfig.cpp
    src/StorageConfig.cpp
	src/HdtnConfig.cpp
)
GENERATE_EXPORT_HEADER(config_lib)
get_target_property(target_type config_lib TYPE)
if (target_type STREQUAL SHARED_LIBRARY)
	set_property(TARGET config_lib PROPERTY CXX_VISIBILITY_PRESET hidden)
	set_property(TARGET config_lib PROPERTY VISIBILITY_INLINES_HIDDEN ON)
endif()
set(MY_PUBLIC_HEADERS
    include/HdtnConfig.h
	include/InductsConfig.h
	include/OutductsConfig.h
	include/StorageConfig.h
	${PROJECT_BINARY_DIR}/config_lib_export.h
)
set_target_properties(config_lib PROPERTIES PUBLIC_HEADER "${MY_PUBLIC_HEADERS}") # this needs to be a list, so putting in quotes makes it a ; separated list
install(TARGETS config_lib
	DESTINATION "${CMAKE_INSTALL_LIBDIR}"
	PUBLIC_HEADER DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)
target_link_libraries(config_lib hdtn_util log_lib ${Boost_LIBRARIES})
target_include_directories(config_lib PUBLIC include ${Boost_INCLUDE_DIRS} ${PROJECT_BINARY_DIR})


