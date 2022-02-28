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
install(TARGETS config_lib DESTINATION lib)
install(FILES
	include/HdtnConfig.h
	include/InductsConfig.h
	include/OutductsConfig.h
	include/StorageConfig.h
	${PROJECT_BINARY_DIR}/config_lib_export.h
	DESTINATION include
)
target_link_libraries(config_lib hdtn_util log_lib ${Boost_LIBRARIES})
target_include_directories(config_lib PUBLIC include ${Boost_INCLUDE_DIRS} ${PROJECT_BINARY_DIR})


