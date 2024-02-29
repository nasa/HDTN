cmake_minimum_required(VERSION 3.12) #using generator $<TARGET_NAME_IF_EXISTS:tgt> which is new in version 3.12.

#
# Suggestion from https://cmake.org/pipermail/cmake/2008-September/023808.html
# If the user specifies -DCMAKE_BUILD_TYPE on the command line, take their definition
# and dump it in the cache along with proper documentation, otherwise set CMAKE_BUILD_TYPE
# to Release prior to calling PROJECT()
#
if(DEFINED CMAKE_BUILD_TYPE)
   SET(CMAKE_BUILD_TYPE ${CMAKE_BUILD_TYPE} CACHE STRING "Choose the type of build, options are: None(CMAKE_CXX_FLAGS or CMAKE_C_FLAGS used) Debug Release RelWithDebInfo MinSizeRel.")
else()
   SET(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build, options are: None(CMAKE_CXX_FLAGS or CMAKE_C_FLAGS used) Debug Release RelWithDebInfo MinSizeRel.")
endif()
string( TOLOWER "${CMAKE_BUILD_TYPE}" build_type_lower )
message("CMAKE_BUILD_TYPE = ${CMAKE_BUILD_TYPE}")
if( build_type_lower STREQUAL "debug" )
	message(WARNING "You are building in Debug mode!  HDTN can run order(s) of magnitude slower!")
endif()


project(UNIT_TESTS_INSTALLATION VERSION "1.0")
set(CMAKE_CXX_STANDARD 11)

#set find_package search
SET(CMAKE_PREFIX_PATH $ENV{HDTN_INSTALL_ROOT} CACHE PATH "Path for find_package to import installed hdtn libraries")

#import all the libraries from HDTN (just to make sure they import.. unit tests won't need all of them)



find_package(HDTNUtil REQUIRED)
if(ENABLE_BPSEC)
	find_package(BpsecLib REQUIRED)
endif()
find_package(TelemetryDefinitions REQUIRED)
find_package(LtpLib REQUIRED)
find_package(BpAppPatternsLib REQUIRED)
find_package(Bpcodec REQUIRED)
find_package(EgressLib REQUIRED)
find_package(HDTNConfigLib REQUIRED)
find_package(HDTNCgrLib REQUIRED)
find_package(HDTNTelemLib REQUIRED)
find_package(InductManagerLib REQUIRED)
find_package(IngressLib REQUIRED)
find_package(LoggerLib REQUIRED)
find_package(OutductManagerLib REQUIRED)
find_package(SlipOverUartLib REQUIRED)
find_package(StcpLib REQUIRED)
find_package(StorageLib REQUIRED)
find_package(TcpclLib REQUIRED)
find_package(UdpLib REQUIRED)
find_package(UdpDelaySimLib REQUIRED)
find_package(HDTNCliLib REQUIRED)
find_package(RouterLib REQUIRED)
find_package(Boost "1.67.0" REQUIRED timer unit_test_framework)
# define _WIN32_WINNT to use Windows 10 for boost
if(WIN32)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
    add_compile_definitions(_WIN32_WINNT=0x0A00)
endif()
if(NON_ARM_COMPILATION)
	message("enabled: NON_ARM_COMPILATION: set to ${NON_ARM_COMPILATION}")
else()
	message("disabled: NON_ARM_COMPILATION: set to ${NON_ARM_COMPILATION}")
endif()

if(ENABLE_BPSEC)
	message("enabled: ENABLE_BPSEC: set to ${ENABLE_BPSEC}")
else()
	message("disabled: ENABLE_BPSEC: set to ${ENABLE_BPSEC}")
endif()

if(RUN_TELEMETRY)
	message("enabled: RUN_TELEMETRY: set to ${RUN_TELEMETRY}")
else()
	message("disabled: RUN_TELEMETRY: set to ${RUN_TELEMETRY}")
endif()

if(ENABLE_MASKING)
	message("enabled: ENABLE_MASKING: set to ${ENABLE_MASKING}")
else()
	message("disabled: ENABLE_MASKING: set to ${ENABLE_MASKING}")
endif()


add_executable(unit-tests-from-install
    src/test_main.cpp
    ../../common/tcpcl/test/TestTcpcl.cpp
	../../common/tcpcl/test/TestTcpclV4.cpp
	../../common/slip_over_uart/test/TestSlip.cpp
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
	../../common/util/test/TestCcsdsEncap.cpp
	../../common/util/test/TestUtf8Paths.cpp
	../../common/util/test/TestEnumAsFlagsMacro.cpp
	../../common/util/test/TestPaddedVectorUint8.cpp
	../../common/util/test/TestFreeListAllocator.cpp
	$<$<BOOL:${NON_ARM_COMPILATION}>:../../common/util/test/TestCpuFlagDetection.cpp>
	../../common/util/test/TestTokenRateLimiter.cpp
	../../common/util/test/TestUdpBatchSender.cpp
	../../common/util/test/TestJsonSerializable.cpp
	../../common/util/test/TestDirectoryScanner.cpp
	../../common/util/test/TestMemoryInFiles.cpp
	../../common/util/test/TestForwardListQueue.cpp
	../../common/util/test/TestUserDataRecycler.cpp
	../../common/util/test/TestDeadlineTimer.cpp
	../../common/util/test/dir_monitor/test_async.cpp
	../../common/util/test/dir_monitor/test_sync.cpp
	#../../common/util/test/test_running.cpp
	../../common/telemetry_definitions/test/TestTelemetryDefinitions.cpp
	../../common/telemetry_definitions/test/TestTelemetryServer.cpp
	../../common/bpcodec/test/TestAggregateCustodySignal.cpp
	../../common/bpcodec/test/TestCustodyTransfer.cpp
	../../common/bpcodec/test/TestCustodyIdAllocator.cpp
	../../common/bpcodec/test/TestBundleViewV6.cpp
	../../common/bpcodec/test/TestBundleViewV7.cpp
	../../common/bpcodec/test/TestBpv6Fragmentation.cpp
	../../common/bpcodec/test/TestBpsecDefaultSecurityContexts.cpp
	$<$<BOOL:${ENABLE_BPSEC}>:../../common/bpsec/test/TestBpsecDefault.cpp>
	$<$<BOOL:${ENABLE_BPSEC}>:../../common/bpsec/test/TestInitializationVectors.cpp>
	$<$<BOOL:${ENABLE_BPSEC}>:../../common/bpsec/test/TestBpSecPolicyManager.cpp>
	../../common/bpcodec/test/TestBpv7Crc.cpp
	../../common/bpcodec/test/TestBpSinkPattern.cpp
	../../common/config/test/TestInductsConfig.cpp
	../../common/config/test/TestOutductsConfig.cpp
	../../common/config/test/TestStorageConfig.cpp
	../../common/config/test/TestHdtnConfig.cpp
	../../common/config/test/TestHdtnDistributedConfig.cpp
	$<$<BOOL:${ENABLE_BPSEC}>:../../common/config/test/TestBpSecConfig.cpp>
	../../common/cgr/test/TestDijkstra.cpp
	../../common/logger/unit_tests/LoggerTests.cpp
	../../common/stats_logger/unit_tests/StatsLoggerTests.cpp
	#../../common/cgr/test/TestYen.cpp
    ../../module/storage/unit_tests/MemoryManagerTreeArrayTests.cpp
    ../../module/storage/unit_tests/BundleStorageManagerMtTests.cpp
	../../module/storage/unit_tests/TestBundleStorageCatalog.cpp
	../../module/storage/unit_tests/TestBundleUuidToUint64HashMap.cpp
	../../module/storage/unit_tests/TestCustodyTimers.cpp
    #../../module/storage/unit_tests/BundleStorageManagerMtAsFifoTests.cpp
	$<$<BOOL:${RUN_TELEMETRY}>:../../module/telem_cmd_interface/unit_tests/TelemetryRunnerTests.cpp>
	$<$<BOOL:${RUN_TELEMETRY}>:../../module/telem_cmd_interface/unit_tests/TelemetryConnectionTests.cpp>
	$<$<BOOL:${RUN_TELEMETRY}>:../../module/telem_cmd_interface/unit_tests/TelemetryConnectionPollerTests.cpp>
	$<$<BOOL:${RUN_TELEMETRY}>:../../module/telem_cmd_interface/unit_tests/TelemetryLoggerTests.cpp>
	$<$<BOOL:${RUN_TELEMETRY}>:../../module/telem_cmd_interface/unit_tests/TelemetryRunnerProgramOptionsTests.cpp>
    ../../module/router/unit_tests/RouterTests.cpp
	../../module/cli/test/TestHdtnCliRunner.cpp
)

target_link_libraries(unit-tests-from-install
	HDTN::HDTNUtil
	HDTN::TelemetryDefinitions
	HDTN::LtpLib
	HDTN::BpAppPatternsLib
	HDTN::Bpcodec
	$<$<BOOL:${ENABLE_BPSEC}>:HDTN::BpsecLib>
	HDTN::EgressLib
	HDTN::HDTNConfigLib
	HDTN::HDTNCgrLib
	HDTN::InductManagerLib
	HDTN::IngressLib
	HDTN::LoggerLib
	HDTN::StcpLib
	HDTN::StorageLib
	HDTN::TcpclLib
	HDTN::UdpLib
	HDTN::SlipOverUartLib
	HDTN::UdpDelaySimLib
	HDTN::HdtnTelemLib
	HDTN::OutductManagerLib
	HDTN::HDTNCliLib
	HDTN::RouterLib
	Boost::unit_test_framework
	Boost::timer
)
