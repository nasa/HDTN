add_executable(integrated-tests
	src/it_test_main.cpp
        src/integrated_tests.cpp
        ../../common/bpcodec/apps/bpgen/src/BpGenAsync.cpp
        ../../common/bpcodec/apps/bpgen/src/BpGenAsyncRunner.cpp
        ../../common/bpcodec/apps/bpsink/BpSinkAsyncRunner.cpp
        ../../common/bpcodec/apps/bpsink/BpSinkAsync.cpp
)
install(TARGETS integrated-tests DESTINATION ${CMAKE_INSTALL_BINDIR})
target_include_directories(integrated-tests PUBLIC
	include
    ../../common/include
    ../../common/bpcodec/apps/bpgen/include
    ../../common/bpcodec/apps/bpsink/include
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
	scheduler_lib
	induct_manager_lib
	outduct_manager_lib
	bpcodec
	bp_app_patterns_lib
	Boost::unit_test_framework
)
