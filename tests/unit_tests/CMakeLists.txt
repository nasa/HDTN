add_executable(unit-tests
    src/test_main.cpp
    ../../common/tcpcl/test/TestTcpcl.cpp
	../../common/tcpcl/test/TestTcpclV4.cpp
	../../common/ltp/test/TestLtp.cpp
	../../common/ltp/test/TestLtpFragmentSet.cpp
	../../common/ltp/test/TestLtpSessionRecreationPreventer.cpp
	../../common/ltp/test/TestLtpRandomNumberGenerator.cpp
	../../common/ltp/test/TestLtpEngine.cpp
	../../common/ltp/test/TestLtpUdpEngine.cpp
	../../common/ltp/test/TestLtpTimerManager.cpp
    ../../common/util/test/TestSdnv.cpp
	../../common/util/test/TestCborUint.cpp
	../../common/util/test/TestCircularIndexBuffer.cpp
	#../../common/util/test/TestRateManagerAsync.cpp
	../../common/util/test/TestTimestampUtil.cpp
	../../common/util/test/TestUri.cpp
	../../common/util/test/TestEnumAsFlagsMacro.cpp
	../../common/util/test/TestPaddedVectorUint8.cpp
	../../common/util/test/TestCpuFlagDetection.cpp
	../../common/util/test/TestTokenRateLimiter.cpp
	../../common/util/test/TestUdpBatchSender.cpp
	../../common/util/test/TestJsonSerializable.cpp
	../../common/util/test/TestDirectoryScanner.cpp
	../../common/util/test/dir_monitor/test_async.cpp
	../../common/util/test/dir_monitor/test_sync.cpp
	#../../common/util/test/test_running.cpp
	../../common/bpcodec/test/TestAggregateCustodySignal.cpp
	../../common/bpcodec/test/TestCustodyTransfer.cpp
	../../common/bpcodec/test/TestCustodyIdAllocator.cpp
	../../common/bpcodec/test/TestBundleViewV6.cpp
	../../common/bpcodec/test/TestBundleViewV7.cpp
	../../common/bpcodec/test/TestBpsecDefaultSecurityContexts.cpp
	../../common/bpcodec/test/TestBpv7Crc.cpp
	../../common/config/test/TestInductsConfig.cpp
	../../common/config/test/TestOutductsConfig.cpp
	../../common/config/test/TestStorageConfig.cpp
	../../common/config/test/TestHdtnConfig.cpp
	../../common/cgr/test/TestDijkstra.cpp
	../../common/logger/unit_tests/LoggerTests.cpp
	../../common/stats_writer/unit_tests/StatsWriterTests.cpp
	#../../common/cgr/test/TestYen.cpp
    ../../module/storage/unit_tests/MemoryManagerTreeArrayTests.cpp
    ../../module/storage/unit_tests/BundleStorageManagerMtTests.cpp
	../../module/storage/unit_tests/TestBundleStorageCatalog.cpp
	../../module/storage/unit_tests/TestBundleUuidToUint64HashMap.cpp
	../../module/storage/unit_tests/TestCustodyTimers.cpp
    #../../module/storage/unit_tests/BundleStorageManagerMtAsFifoTests.cpp
)
install(TARGETS unit-tests DESTINATION ${CMAKE_INSTALL_BINDIR})

target_link_libraries(unit-tests
	hdtn_util
	tcpcl_lib
	stcp_lib
	ltp_lib
	storage_lib
	config_lib
	cgr_lib
	ingress_async_lib
	bpcodec
	udp_delay_sim_lib
	log_lib
	stats_lib
	Boost::unit_test_framework
	Boost::timer
)


set_property(
	SOURCE ../../common/logger/unit_tests/LoggerTests.cpp
	PROPERTY COMPILE_DEFINITIONS
		$<$<BOOL:${LOG_TO_CONSOLE}>:LOG_TO_CONSOLE> 
		$<$<BOOL:${LOG_TO_PROCESS_FILE}>:LOG_TO_PROCESS_FILE>
		$<$<BOOL:${LOG_TO_SUBPROCESS_FILES}>:LOG_TO_SUBPROCESS_FILES>
		$<$<BOOL:${LOG_TO_ERROR_FILE}>:LOG_TO_ERROR_FILE>
)

