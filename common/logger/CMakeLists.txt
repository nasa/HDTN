cmake_minimum_required(VERSION 3.0)
project(log_lib)
set(CMAKE_CXX_STANDARD 11)

file(MAKE_DIRECTORY ../../logs)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
add_library(log_lib
      	src/Logger.cpp
)
GENERATE_EXPORT_HEADER(log_lib)
get_target_property(target_type log_lib TYPE)
if (target_type STREQUAL SHARED_LIBRARY)
	set_property(TARGET log_lib PROPERTY CXX_VISIBILITY_PRESET hidden)
	set_property(TARGET log_lib PROPERTY VISIBILITY_INLINES_HIDDEN ON)
endif()
set(MY_PUBLIC_HEADERS
    include/Logger.h
	${PROJECT_BINARY_DIR}/log_lib_export.h
)
set_target_properties(log_lib PROPERTIES PUBLIC_HEADER "${MY_PUBLIC_HEADERS}") # this needs to be a list, so putting in quotes makes it a ; separated list
install(TARGETS log_lib
	DESTINATION "${CMAKE_INSTALL_LIBDIR}"
	PUBLIC_HEADER DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)
target_include_directories(log_lib PUBLIC include ${Boost_INCLUDE_DIRS} ${PROJECT_BINARY_DIR})
target_link_libraries(log_lib
    ${Boost_LIBRARIES}
)
