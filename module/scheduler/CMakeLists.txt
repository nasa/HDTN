add_executable(hdtn-scheduler
    src/scheduler.cpp
    src/main.cpp
)

install(TARGETS hdtn-scheduler DESTINATION ${CMAKE_INSTALL_BINDIR})
target_link_libraries(hdtn-scheduler
	bpcodec
	hdtn_util
	config_lib
	Boost::program_options
)

target_include_directories(hdtn-scheduler
	PUBLIC
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../common/include> # for message.hpp
)

