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
target_link_libraries(bpgen-async PUBLIC tcpcl_lib stcp_lib udp_lib bpcodec hdtn_util ${pthread_LIB} ${Boost_LIBRARIES})

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
target_link_libraries(bpsink-async PUBLIC tcpcl_lib stcp_lib udp_lib bpcodec hdtn_util ${pthread_LIB} ${Boost_LIBRARIES})

if (NOT WIN32)	
	add_executable(bpreflect
		bpreflect/bpreflect.cpp
	)


	target_include_directories(bpreflect PUBLIC ../include)
	target_link_libraries(bpreflect PUBLIC bpcodec)
endif()
