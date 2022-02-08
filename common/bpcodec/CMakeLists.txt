cmake_minimum_required(VERSION 3.0)
project(bpcodec)
#set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_CXX_STANDARD 11)



add_library(bpcodec STATIC
        src/codec/Bpv7CbhePrimaryBlock.cpp
		src/codec/Bpv7CanonicalBlock.cpp
		src/codec/Bpv7ExtensionBlocks.cpp
		src/codec/Bpv7BpSecExtensionBlocks.cpp
		src/codec/Bpv7AdministrativeRecords.cpp
        src/codec/bpv6.cpp
		src/codec/Cbhe.cpp
        src/codec/bpv6-ext-block.cpp
		src/codec/Bpv6AdministrativeRecords.cpp
		src/codec/AggregateCustodySignal.cpp
		src/codec/CustodyTransferEnhancementBlock.cpp
		src/codec/CustodyIdAllocator.cpp
		src/codec/CustodyTransferManager.cpp
		src/codec/BundleViewV6.cpp
		src/codec/BundleViewV7.cpp
		src/codec/Bpv7Crc.cpp
)

target_link_libraries(bpcodec hdtn_util)
target_include_directories(bpcodec PUBLIC include)

add_library(bp_app_patterns_lib STATIC
        src/app_patterns/BpSourcePattern.cpp
		src/app_patterns/BpSinkPattern.cpp
)
target_link_libraries(bp_app_patterns_lib bpcodec hdtn_util outduct_manager_lib induct_manager_lib ${pthread_LIB} ${Boost_LIBRARIES})
target_include_directories(bp_app_patterns_lib PUBLIC include ${Boost_INCLUDE_DIRS})

add_subdirectory(apps)

