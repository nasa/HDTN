cmake_minimum_required(VERSION 3.0)
project(bpcodec)
#set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_CXX_STANDARD 11)



add_library(bpcodec
        src/codec/Bpv7CbhePrimaryBlock.cpp
		src/codec/Bpv7CanonicalBlock.cpp
		src/codec/Bpv7ExtensionBlocks.cpp
		src/codec/Bpv7BpSecExtensionBlocks.cpp
		src/codec/Bpv7AdministrativeRecords.cpp
        src/codec/Bpv6CbhePrimaryBlock.cpp
		src/codec/Bpv6CanonicalBlock.cpp
		src/codec/Cbhe.cpp
		src/codec/Bpv6AdministrativeRecords.cpp
		src/codec/Bpv6ExtensionBlocks.cpp
		src/codec/Bpv6MetadataExtensionBlock.cpp
		src/codec/CustodyIdAllocator.cpp
		src/codec/CustodyTransferManager.cpp
		src/codec/BundleViewV6.cpp
		src/codec/BundleViewV7.cpp
		src/codec/Bpv7Crc.cpp
)
GENERATE_EXPORT_HEADER(bpcodec)
target_link_libraries(bpcodec hdtn_util)
target_include_directories(bpcodec PUBLIC include ${PROJECT_BINARY_DIR})
install(TARGETS bpcodec DESTINATION lib)
install(FILES
	include/codec/bpv6.h
	include/codec/bpv7.h
	include/codec/Bpv7Crc.h
	include/codec/BundleViewV6.h
    include/codec/BundleViewV7.h
	include/codec/Cbhe.h
	include/codec/Cose.h
	include/codec/CustodyIdAllocator.h
	include/codec/CustodyTransferManager.h
	include/codec/PrimaryBlock.h
	${PROJECT_BINARY_DIR}/bpcodec_export.h
	DESTINATION include/codec
)
install(FILES
	../include/message.hpp
	../include/stats.hpp
	DESTINATION include
)


add_library(bp_app_patterns_lib
        src/app_patterns/BpSourcePattern.cpp
		src/app_patterns/BpSinkPattern.cpp
)
GENERATE_EXPORT_HEADER(bp_app_patterns_lib)
target_link_libraries(bp_app_patterns_lib bpcodec hdtn_util outduct_manager_lib induct_manager_lib ${pthread_LIB} ${Boost_LIBRARIES})
target_include_directories(bp_app_patterns_lib PUBLIC include ${Boost_INCLUDE_DIRS} ${PROJECT_BINARY_DIR})
install(TARGETS bp_app_patterns_lib DESTINATION lib)
install(FILES
	include/app_patterns/BpSinkPattern.h
	include/app_patterns/BpSourcePattern.h
	${PROJECT_BINARY_DIR}/bp_app_patterns_lib_export.h
	DESTINATION include/app_patterns
)
add_subdirectory(apps)

