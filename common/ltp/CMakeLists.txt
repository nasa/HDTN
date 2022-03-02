cmake_minimum_required(VERSION 3.0)
project(ltp_lib)
set(CMAKE_CXX_STANDARD 11)



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
	src/LtpClientServiceDataToSend.cpp
)
GENERATE_EXPORT_HEADER(ltp_lib)
set_property(TARGET ltp_lib PROPERTY CXX_VISIBILITY_PRESET hidden)
set_property(TARGET ltp_lib PROPERTY VISIBILITY_INLINES_HIDDEN ON)
install(TARGETS ltp_lib DESTINATION lib)
install(FILES
	include/Ltp.h
	include/LtpBundleSink.h
	include/LtpBundleSource.h
	include/LtpClientServiceDataToSend.h
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
	${PROJECT_BINARY_DIR}/ltp_lib_export.h
	DESTINATION include
)
target_link_libraries(ltp_lib hdtn_util ${Boost_LIBRARIES} ${pthread_LIB})
target_include_directories(ltp_lib PUBLIC include ${Boost_INCLUDE_DIRS} ${PROJECT_BINARY_DIR})

add_executable(ltp-file-transfer
    apps/ltp_file_transfer/src/LtpFileTransferMain.cpp
	apps/ltp_file_transfer/src/LtpFileTransferRunner.cpp
)
install(TARGETS ltp-file-transfer DESTINATION bin)
target_include_directories(ltp-file-transfer PUBLIC 
	apps/ltp_file_transfer/include
	${Boost_INCLUDE_DIRS}
)
target_link_libraries(ltp-file-transfer PUBLIC ltp_lib hdtn_util ${pthread_LIB} ${Boost_LIBRARIES})
