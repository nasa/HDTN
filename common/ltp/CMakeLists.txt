cmake_minimum_required(VERSION 3.0)
project(ltp_lib)
set(CMAKE_CXX_STANDARD 11)



add_library(ltp_lib STATIC
    src/Ltp.cpp
	src/LtpFragmentSet.cpp
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

target_link_libraries(ltp_lib hdtn_util ${Boost_LIBRARIES} ${pthread_LIB})
target_include_directories(ltp_lib PUBLIC include ${Boost_INCLUDE_DIRS})

add_executable(ltp-file-transfer
    apps/ltp_file_transfer/src/LtpFileTransferMain.cpp
	apps/ltp_file_transfer/src/LtpFileTransferRunner.cpp
)

target_include_directories(ltp-file-transfer PUBLIC 
	apps/ltp_file_transfer/include
	${Boost_INCLUDE_DIRS}
	${libzmq_INCLUDE}
)
target_link_libraries(ltp-file-transfer PUBLIC ltp_lib bpcodec hdtn_util ${pthread_LIB} ${Boost_LIBRARIES} ${libzmq_LIB})
