cmake_minimum_required(VERSION 3.0)
project(bpcodec_perf)
set(CMAKE_CXX_STANDARD 11)


add_executable(bpgen-async
               bpgen/src/BpGenAsync.cpp
    	       bpgen/src/BpGenAsyncMain.cpp
               bpgen/src/BpGenAsyncRunner.cpp
)
install(TARGETS bpgen-async DESTINATION ${CMAKE_INSTALL_BINDIR})

target_include_directories(bpgen-async PUBLIC 
	../include
	bpgen/include
	${Boost_INCLUDE_DIRS}
)
target_link_libraries(bpgen-async PUBLIC outduct_manager_lib induct_manager_lib bpcodec hdtn_util bp_app_patterns_lib ${pthread_LIB} ${Boost_LIBRARIES})

add_executable(bping
               bping/src/BPing.cpp
    	       bping/src/BPingMain.cpp
               bping/src/BPingRunner.cpp
)
install(TARGETS bping DESTINATION ${CMAKE_INSTALL_BINDIR})
target_include_directories(bping PUBLIC 
	../include
	bping/include
	${Boost_INCLUDE_DIRS}
)
target_link_libraries(bping PUBLIC outduct_manager_lib induct_manager_lib bpcodec hdtn_util bp_app_patterns_lib ${pthread_LIB} ${Boost_LIBRARIES})


add_executable(bpsink-async
               bpsink/BpSinkAsync.cpp
               bpsink/BpSinkAsyncMain.cpp
	       bpsink/BpSinkAsyncRunner.cpp
)
install(TARGETS bpsink-async DESTINATION ${CMAKE_INSTALL_BINDIR})
target_include_directories(bpsink-async PUBLIC
	../include 
	bpsink/include
	${Boost_INCLUDE_DIRS}
)
target_link_libraries(bpsink-async PUBLIC induct_manager_lib outduct_manager_lib bpcodec hdtn_util bp_app_patterns_lib ${pthread_LIB} ${Boost_LIBRARIES})

add_executable(bpsendfile
               bpsendfile/src/BpSendFile.cpp
    	       bpsendfile/src/BpSendFileMain.cpp
               bpsendfile/src/BpSendFileRunner.cpp
)
install(TARGETS bpsendfile DESTINATION ${CMAKE_INSTALL_BINDIR})
target_include_directories(bpsendfile PUBLIC 
	../include
	bpsendfile/include
	${Boost_INCLUDE_DIRS}
)
target_link_libraries(bpsendfile PUBLIC outduct_manager_lib induct_manager_lib bpcodec hdtn_util bp_app_patterns_lib ${pthread_LIB} ${Boost_LIBRARIES})

add_executable(bpreceivefile
               bpreceivefile/src/BpReceiveFile.cpp
    	       bpreceivefile/src/BpReceiveFileMain.cpp
               bpreceivefile/src/BpReceiveFileRunner.cpp
)
install(TARGETS bpreceivefile DESTINATION ${CMAKE_INSTALL_BINDIR})
target_include_directories(bpreceivefile PUBLIC 
	../include
	bpreceivefile/include
	${Boost_INCLUDE_DIRS}
)
target_link_libraries(bpreceivefile PUBLIC outduct_manager_lib induct_manager_lib bpcodec hdtn_util bp_app_patterns_lib ${pthread_LIB} ${Boost_LIBRARIES})
