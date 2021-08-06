cmake_minimum_required(VERSION 3.0)
project(config_lib)
set(CMAKE_CXX_STANDARD 11)



add_library(config_lib STATIC
    src/InductsConfig.cpp
	src/OutductsConfig.cpp
    src/StorageConfig.cpp
	src/HdtnConfig.cpp
)

target_link_libraries(config_lib hdtn_util log_lib ${Boost_LIBRARIES})
target_include_directories(config_lib PUBLIC include ${Boost_INCLUDE_DIRS})

