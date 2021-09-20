cmake_minimum_required(VERSION 3.0)
project(bpcodec_perf)
set(CMAKE_CXX_STANDARD 11)

if(NOT WIN32)
	add_executable(encode-benchmark
		encode-benchmark/encoding.cpp
	)

	target_include_directories(encode-benchmark PUBLIC ../include)
	target_link_libraries(encode-benchmark PUBLIC bpcodec)

	add_executable(bundle-benchmark
		bundle-benchmark/bundle.cpp
	)

	target_include_directories(bundle-benchmark PUBLIC ../include)
	target_link_libraries(bundle-benchmark PUBLIC bpcodec)

	add_executable(bundle-decoder
		bundle-decoder/decode.cpp
	)

	target_include_directories(bundle-decoder PUBLIC ../include)
	target_link_libraries(bundle-decoder PUBLIC bpcodec)

	add_executable(bpgen
		bpgen/bpgen.cpp
	) 

target_include_directories(bpgen PUBLIC ../include)
target_link_libraries(bpgen PUBLIC bpcodec)
endif()

add_executable(bpgen-async
               bpgen/src/BpGenAsync.cpp
    	       bpgen/src/BpGenAsyncMain.cpp
               bpgen/src/BpGenAsyncRunner.cpp
)

target_include_directories(bpgen-async PUBLIC 
	../include
	bpgen/include
	${Boost_INCLUDE_DIRS}
)
target_link_libraries(bpgen-async PUBLIC outduct_manager_lib induct_manager_lib bpcodec hdtn_util bp_app_patterns_lib ${pthread_LIB} ${Boost_LIBRARIES})

if (NOT WIN32)	
	add_executable(bpsink
	               bpsink/bpsink.cpp
	)

	target_include_directories(bpsink PUBLIC ../include)
	target_link_libraries(bpsink PUBLIC bpcodec)
endif()

add_executable(bpsink-async
               bpsink/BpSinkAsync.cpp
               bpsink/BpSinkAsyncMain.cpp
	       bpsink/BpSinkAsyncRunner.cpp
)

target_include_directories(bpsink-async PUBLIC
	../include 
	bpsink/include
	${Boost_INCLUDE_DIRS}
)
target_link_libraries(bpsink-async PUBLIC induct_manager_lib outduct_manager_lib bpcodec hdtn_util bp_app_patterns_lib ${pthread_LIB} ${Boost_LIBRARIES})

if (NOT WIN32)	
	add_executable(bpreflect
		       bpreflect/bpreflect.cpp
	)


	target_include_directories(bpreflect PUBLIC ../include)
	target_link_libraries(bpreflect PUBLIC bpcodec)
endif()

add_executable(bpsendfile
               bpsendfile/src/BpSendFile.cpp
    	       bpsendfile/src/BpSendFileMain.cpp
               bpsendfile/src/BpSendFileRunner.cpp
)

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

target_include_directories(bpreceivefile PUBLIC 
	../include
	bpreceivefile/include
	${Boost_INCLUDE_DIRS}
)
target_link_libraries(bpreceivefile PUBLIC outduct_manager_lib induct_manager_lib bpcodec hdtn_util bp_app_patterns_lib ${pthread_LIB} ${Boost_LIBRARIES})
