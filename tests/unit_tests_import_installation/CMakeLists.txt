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
find_package(TelemetryDefinitions REQUIRED)
find_package(LtpLib REQUIRED)
find_package(BpAppPatternsLib REQUIRED)
find_package(Bpcodec REQUIRED)
find_package(EgressLib REQUIRED)
find_package(HDTNConfigLib REQUIRED)
find_package(HDTNCgrLib REQUIRED)
find_package(InductManagerLib REQUIRED)
find_package(IngressLib REQUIRED)
find_package(LoggerLib REQUIRED)
find_package(OutductManagerLib REQUIRED)
find_package(StcpLib REQUIRED)
find_package(StorageLib REQUIRED)
find_package(TcpclLib REQUIRED)
find_package(UdpLib REQUIRED)
find_package(UdpDelaySimLib REQUIRED)
find_package(Boost "1.67.0" REQUIRED timer unit_test_framework)
# define _WIN32_WINNT to use Windows 10 for boost
if(WIN32)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
    add_compile_definitions(_WIN32_WINNT=0x0A00)
endif()

add_executable(unit-tests-from-install
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
	../../common/util/test/TestMemoryInFiles.cpp
	../../common/util/test/TestForwardListQueue.cpp
	../../common/telemetry_definitions/test/TestTelemetryDefinitions.cpp
	#note: Logger unit tests not added due to specific COMPILE_DEFINITIONS used
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
	#../../common/cgr/test/TestYen.cpp
    ../../module/storage/unit_tests/MemoryManagerTreeArrayTests.cpp
    ../../module/storage/unit_tests/BundleStorageManagerMtTests.cpp
	../../module/storage/unit_tests/TestBundleStorageCatalog.cpp
	../../module/storage/unit_tests/TestBundleUuidToUint64HashMap.cpp
	../../module/storage/unit_tests/TestCustodyTimers.cpp
    #../../module/storage/unit_tests/BundleStorageManagerMtAsFifoTests.cpp
	../../module/telem_cmd_interface/unit_tests/WebsocketServerTests.cpp
	../../module/telem_cmd_interface/unit_tests/TelemetryRunnerTests.cpp
	../../module/telem_cmd_interface/unit_tests/TelemetryConnectionTests.cpp
)

target_link_libraries(unit-tests-from-install
	HDTN::HDTNUtil
	HDTN::TelemetryDefinitions
	HDTN::LtpLib
	HDTN::BpAppPatternsLib
	HDTN::Bpcodec
	HDTN::EgressLib
	HDTN::HDTNConfigLib
	HDTN::HDTNCgrLib
	HDTN::InductManagerLib
	HDTN::IngressLib
	HDTN::LoggerLib
	HDTN::OutductManagerLib
	HDTN::StcpLib
	HDTN::StorageLib
	HDTN::TcpclLib
	HDTN::UdpLib
	HDTN::UdpDelaySimLib
	HDTN::HdtnTelemLib
	Boost::unit_test_framework
	Boost::timer
)
