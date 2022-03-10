add_executable(hdtn-router 
    src/router.cpp
    src/main.cpp
    src/cgrServer.cpp
)
install(TARGETS hdtn-router DESTINATION ${CMAKE_INSTALL_BINDIR})
target_link_libraries(hdtn-router
	bpcodec
	hdtn_util
	config_lib
	Boost::program_options
)

target_include_directories(hdtn-router
	PUBLIC
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../common/include> # for message.hpp
)


