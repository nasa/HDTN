cmake_minimum_required(VERSION 3.0)
project(ltp_lib)
set(CMAKE_CXX_STANDARD 11)



add_library(ltp_lib STATIC
    src/Ltp.cpp
	src/LtpFragmentMap.cpp
	src/LtpRandomNumberGenerator.cpp
	src/LtpSessionReceiver.cpp
	src/LtpSessionSender.cpp
	src/LtpEngine.cpp
	src/LtpTimerManager.cpp
	src/LtpUdpEngine.cpp
	#src/LtpBundleSource.cpp
)

target_link_libraries(ltp_lib hdtn_util ${Boost_LIBRARIES} ${pthread_LIB})
target_include_directories(ltp_lib PUBLIC include ${Boost_INCLUDE_DIRS})


