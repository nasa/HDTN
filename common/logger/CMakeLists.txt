cmake_minimum_required(VERSION 3.0)
project(log_lib)
set(CMAKE_CXX_STANDARD 11)
find_package(Boost REQUIRED COMPONENTS system log log_setup)

file(MAKE_DIRECTORY ../../logs)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
add_library(log_lib SHARED
      	src/Logger.cpp
)

target_include_directories(log_lib PUBLIC include ${Boost_INCLUDE_DIRS})

target_link_libraries(log_lib
    ${Boost_LIBRARIES}
    Boost::log
    Boost::log_setup
)
