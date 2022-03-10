add_executable(integrated-tests
	src/it_test_main.cpp
        src/integrated_tests.cpp
        ../../common/bpcodec/apps/bpgen/src/BpGenAsync.cpp
        ../../common/bpcodec/apps/bpgen/src/BpGenAsyncRunner.cpp
        ../../common/bpcodec/apps/bpsink/BpSinkAsyncRunner.cpp
        ../../common/bpcodec/apps/bpsink/BpSinkAsync.cpp
        ../../module/storage/src/StorageRunner.cpp
        ../../module/storage/src/test/ReleaseSender.cpp
        ../../module/scheduler/src/scheduler.cpp
)
install(TARGETS integrated-tests DESTINATION ${CMAKE_INSTALL_BINDIR})
target_include_directories(integrated-tests PUBLIC include
    ../../module/storage/src/test
    ../../module/storage/include
    ../../module/scheduler/include
    ../../common/include
    ../../common/bpcodec/include
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
	egress_async_lib
	ingress_async_lib
	induct_manager_lib
	outduct_manager_lib
	bpcodec
	bp_app_patterns_lib
	Boost::unit_test_framework
)

add_executable(cgr-test
    src/cgr_tests.cpp
    ../../module/scheduler/src/cgrServer.cpp
)
install(TARGETS cgr-test DESTINATION ${CMAKE_INSTALL_BINDIR})
target_include_directories(cgr-test
	PUBLIC
		../../module/scheduler/include
		../../common/include
)

target_link_libraries(cgr-test
    bpcodec
    hdtn_util
    config_lib
	Boost::unit_test_framework
)
