add_executable(bpgen-async
	bpgen/src/BpGenAsync.cpp
	bpgen/src/BpGenAsyncMain.cpp
	bpgen/src/BpGenAsyncRunner.cpp
)
install(TARGETS bpgen-async DESTINATION ${CMAKE_INSTALL_BINDIR})

target_include_directories(bpgen-async
	PUBLIC 
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/bpgen/include>
)
target_link_libraries(bpgen-async PUBLIC outduct_manager_lib induct_manager_lib bpcodec hdtn_util bp_app_patterns_lib Boost::program_options)

add_executable(bping
	bping/src/BPing.cpp
	bping/src/BPingMain.cpp
	bping/src/BPingRunner.cpp
)
install(TARGETS bping DESTINATION ${CMAKE_INSTALL_BINDIR})
target_include_directories(bping
	PUBLIC 
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/bping/include>
)
target_link_libraries(bping PUBLIC outduct_manager_lib induct_manager_lib bpcodec hdtn_util bp_app_patterns_lib Boost::program_options)


add_executable(bpsink-async
	bpsink/BpSinkAsync.cpp
	bpsink/BpSinkAsyncMain.cpp
	bpsink/BpSinkAsyncRunner.cpp
)
install(TARGETS bpsink-async DESTINATION ${CMAKE_INSTALL_BINDIR})
target_include_directories(bpsink-async
	PUBLIC 
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/bpsink/include>
)
target_link_libraries(bpsink-async
	PUBLIC
		induct_manager_lib
		outduct_manager_lib
		bpcodec
		hdtn_util
		bp_app_patterns_lib
		Boost::program_options
		log_lib
)

add_executable(bpsendfile
	bpsendfile/src/BpSendFile.cpp
	bpsendfile/src/BpSendFileMain.cpp
	bpsendfile/src/BpSendFileRunner.cpp
)
install(TARGETS bpsendfile DESTINATION ${CMAKE_INSTALL_BINDIR})
target_include_directories(bpsendfile
	PUBLIC 
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/bpsendfile/include>
)
target_link_libraries(bpsendfile PUBLIC outduct_manager_lib induct_manager_lib bpcodec hdtn_util bp_app_patterns_lib Boost::program_options)

add_executable(bpreceivefile
	bpreceivefile/src/BpReceiveFile.cpp
	bpreceivefile/src/BpReceiveFileMain.cpp
	bpreceivefile/src/BpReceiveFileRunner.cpp
)
install(TARGETS bpreceivefile DESTINATION ${CMAKE_INSTALL_BINDIR})
target_include_directories(bpreceivefile 
	PUBLIC 
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/bpreceivefile/include>
)
target_link_libraries(bpreceivefile PUBLIC outduct_manager_lib induct_manager_lib bpcodec hdtn_util bp_app_patterns_lib Boost::program_options)

add_executable(bpsendpacket
    bpsendpacket/src/BpSendPacket.cpp
    bpsendpacket/src/BpSendPacketMain.cpp
    bpsendpacket/src/BpSendPacketRunner.cpp
)
install(TARGETS bpsendpacket DESTINATION ${CMAKE_INSTALL_BINDIR})
target_include_directories(bpsendpacket
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/bpsendpacket/include>
)
target_link_libraries(bpsendpacket PUBLIC outduct_manager_lib induct_manager_lib bpcodec hdtn_util bp_app_patterns_lib Boost::program_options)

add_executable(bpreceivepacket
    bpreceivepacket/src/BpReceivePacket.cpp
    bpreceivepacket/src/BpReceivePacketMain.cpp
    bpreceivepacket/src/BpReceivePacketRunner.cpp
)
install(TARGETS bpreceivepacket DESTINATION ${CMAKE_INSTALL_BINDIR})
target_include_directories(bpreceivepacket
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/bpreceivepacket/include>
)
target_link_libraries(bpreceivepacket PUBLIC outduct_manager_lib induct_manager_lib bpcodec hdtn_util bp_app_patterns_lib Boost::program_options)
