cmake_minimum_required(VERSION 3.0)
project(log_lib)
set(CMAKE_CXX_STANDARD 11)

file(MAKE_DIRECTORY ../../logs)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
add_library(log_lib
      	src/Logger.cpp
)
GENERATE_EXPORT_HEADER(log_lib)
set_property(TARGET log_lib PROPERTY CXX_VISIBILITY_PRESET hidden)
set_property(TARGET log_lib PROPERTY VISIBILITY_INLINES_HIDDEN ON)
install(TARGETS log_lib DESTINATION lib)
install(FILES
	include/Logger.h
	${PROJECT_BINARY_DIR}/log_lib_export.h
	DESTINATION include
)
target_include_directories(log_lib PUBLIC include ${Boost_INCLUDE_DIRS} ${PROJECT_BINARY_DIR})

target_link_libraries(log_lib
    ${Boost_LIBRARIES}
)
