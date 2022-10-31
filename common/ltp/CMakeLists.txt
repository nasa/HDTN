add_library(ltp_lib
    src/Ltp.cpp
	src/LtpFragmentSet.cpp
	src/LtpSessionRecreationPreventer.cpp
	src/LtpRandomNumberGenerator.cpp
	src/LtpSessionReceiver.cpp
	src/LtpSessionSender.cpp
	src/LtpEngine.cpp
	src/LtpTimerManager.cpp
	src/LtpUdpEngine.cpp
	src/LtpUdpEngineManager.cpp
	src/LtpBundleSink.cpp
	src/LtpBundleSource.cpp
)

set_property(
	SOURCE src/LtpRandomNumberGenerator.cpp
	PROPERTY COMPILE_DEFINITIONS
		$<$<BOOL:${HAS_LTP_RNG_USE_RDSEED}>:LTP_RNG_USE_RDSEED> 
)
set_property(
	SOURCE src/LtpRandomNumberGenerator.cpp
	PROPERTY COMPILE_OPTIONS
		$<$<BOOL:${HAS_LTP_RNG_USE_RDSEED}>:${NON_WINDOWS_RDSEED_COMPILE_FLAG}> 
)

target_compile_options(ltp_lib PRIVATE ${NON_WINDOWS_HARDWARE_ACCELERATION_FLAGS})

#Github issue 26: Allow same-engine transfers
#There are currently three places in LtpEngine.cpp that guard against
#sessionId.sessionOriginatorEngineId == M_THIS_ENGINE_ID and early-return from processing segments.
#There is no prohibition against sending segments to your own engine,
#and for certain purposes some transfers are made to the local BPA.
#
#If there is no technical reason to prohibit these, it would be helpful to remove these guards.
#I did a quick experiment of just removing the three checks and things seem to go through properly.
OPTION(LTP_ENGINE_ALLOW_SAME_ENGINE_TRANSFERS "Allow sending segments to your own LTP engine." Off)
#from https://cmake.org/pipermail/cmake/2016-January/062570.html
set_property(
	SOURCE src/LtpEngine.cpp
	PROPERTY COMPILE_DEFINITIONS
		$<$<BOOL:${LTP_ENGINE_ALLOW_SAME_ENGINE_TRANSFERS}>:LTP_ENGINE_ALLOW_SAME_ENGINE_TRANSFERS> 
)

GENERATE_EXPORT_HEADER(ltp_lib)
get_target_property(target_type ltp_lib TYPE)
if (target_type STREQUAL SHARED_LIBRARY)
	set_property(TARGET ltp_lib PROPERTY CXX_VISIBILITY_PRESET hidden)
	set_property(TARGET ltp_lib PROPERTY VISIBILITY_INLINES_HIDDEN ON)
endif()
set(MY_PUBLIC_HEADERS
    include/Ltp.h
	include/LtpBundleSink.h
	include/LtpBundleSource.h
	include/LtpEngine.h
	include/LtpFragmentSet.h
	include/LtpNoticesToClientService.h
	include/LtpRandomNumberGenerator.h
	include/LtpSessionReceiver.h
	include/LtpSessionRecreationPreventer.h
	include/LtpSessionSender.h
	include/LtpTimerManager.h
	include/LtpUdpEngine.h
	include/LtpUdpEngineManager.h
	${CMAKE_CURRENT_BINARY_DIR}/ltp_lib_export.h
)
set_target_properties(ltp_lib PROPERTIES PUBLIC_HEADER "${MY_PUBLIC_HEADERS}") # this needs to be a list, so putting in quotes makes it a ; separated list
target_link_libraries(ltp_lib
	PUBLIC
		hdtn_util
		telemetry_definitions
		Boost::random
		log_lib
)
target_include_directories(ltp_lib
	PUBLIC
		$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
		$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}> # for GENERATE_EXPORT_HEADER
)
install(TARGETS ltp_lib
	EXPORT ltp_lib-targets
	DESTINATION "${CMAKE_INSTALL_LIBDIR}"
	PUBLIC_HEADER DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)
add_hdtn_package_export(ltp_lib LtpLib) #exported target will have the name HDTN::LtpLib and not ltp_lib.  Also requires install to EXPORT ltp_lib-targets

add_executable(ltp-file-transfer
    apps/ltp_file_transfer/src/LtpFileTransferMain.cpp
	apps/ltp_file_transfer/src/LtpFileTransferRunner.cpp
)
install(TARGETS ltp-file-transfer DESTINATION ${CMAKE_INSTALL_BINDIR})
target_include_directories(ltp-file-transfer
	PUBLIC 
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/apps/ltp_file_transfer/include>
)
target_link_libraries(ltp-file-transfer PUBLIC ltp_lib Boost::program_options)
