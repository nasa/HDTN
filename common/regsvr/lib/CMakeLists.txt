cmake_minimum_required(VERSION 3.0)
project(hdtn_reg)
#set(CMAKE_BUILD_TYPE Debug)
#set(CMAKE_CONFIGURATION_TYPES "Debug;Release;MinSizeRel;RelWithDebInfo" CACHE STRING "" FORCE)
set(CMAKE_CXX_STANDARD 11)
find_package(Boost 1.67.0 REQUIRED system filesystem timer thread regex filesystem program_options)

# define _WIN32_WINNT to use Windows 10 for boost
if(WIN32)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
    add_compile_definitions(_WIN32_WINNT=0x0A00)
endif()

message("CMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}")
message("CMAKE_CONFIGURATION_TYPES=${CMAKE_CONFIGURATION_TYPES}")


add_library(hdtn_reg STATIC
    src/reg.cpp
    ../../util/src/JsonSerializable.cpp
)

if (WIN32)
	#add_compile_definitions(ZMQ_STATIC)
	set(libzmq_INCLUDE "C:/libzmq_msvc_x64/include" CACHE PATH "Folder to Libzmq headers")
	set(libzmq_LIB "C:/libzmq_msvc_x64/lib/libzmq-v141-mt-4_3_4.lib" CACHE FILEPATH "Libzmq library")
	target_link_libraries(hdtn_reg ${libzmq_LIB})
	target_include_directories(hdtn_reg PUBLIC ${libzmq_INCLUDE})
else()
	target_link_libraries(hdtn_reg pthread zmq)
endif()

target_link_libraries(hdtn_reg ${Boost_LIBRARIES})
target_include_directories(hdtn_reg PUBLIC include ../../util/include ${Boost_INCLUDE_DIRS})


add_executable(hdtn_regtest
    src/reg.cpp
    src/main.cpp
    ../../util/src/JsonSerializable.cpp
)

if (WIN32)
	target_link_libraries(hdtn_regtest ${libzmq_LIB})
	target_include_directories(hdtn_regtest PUBLIC ${libzmq_INCLUDE})
else()
        target_link_libraries(hdtn_regtest pthread zmq)
endif()

target_link_libraries(hdtn_regtest ${Boost_LIBRARIES})
target_include_directories(hdtn_regtest PUBLIC include ../../util/include ${Boost_INCLUDE_DIRS})

