add_executable(integrated-tests
	src/it_test_main.cpp
        src/integrated_tests.cpp
        src/bp_sink_pattern.cpp
        ../../common/bpcodec/apps/bpgen/src/BpGenAsync.cpp
        ../../common/bpcodec/apps/bpgen/src/BpGenAsyncRunner.cpp
        ../../common/bpcodec/apps/bpsink/BpSinkAsyncRunner.cpp
        ../../common/bpcodec/apps/bpsink/BpSinkAsync.cpp
	../../common/bpcodec/apps/bpsendfile/src/BpSendFile.cpp
        ../../common/bpcodec/apps/bpsendfile/src/BpSendFileRunner.cpp
        ../../common/bpcodec/apps/bpreceivefile/src/BpReceiveFileRunner.cpp
        ../../common/bpcodec/apps/bpreceivefile/src/BpReceiveFile.cpp
)
install(TARGETS integrated-tests DESTINATION ${CMAKE_INSTALL_BINDIR})
target_include_directories(integrated-tests PUBLIC
	include
    ../../common/include
    ../../common/bpcodec/apps/bpgen/include
    ../../common/bpcodec/apps/bpsink/include
    ../../common/bpcodec/apps/bpsendfile/include
    ../../common/bpcodec/apps/bpreceivefile/include
)

target_link_libraries(integrated-tests 
	storage_lib
	hdtn_util
	tcpcl_lib
	stcp_lib
	udp_lib
	ltp_lib
	hdtn_one_process_lib
	egress_async_lib
	ingress_async_lib
	router_lib
	induct_manager_lib
	outduct_manager_lib
	bpcodec
	bp_app_patterns_lib
	Boost::unit_test_framework
)

#integrated_tests.cpp is causing issues on gnu compilers with the following warning
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
	set_property(
		SOURCE src/integrated_tests.cpp
		PROPERTY COMPILE_OPTIONS
			"-Wno-use-after-free"
	)
endif()
