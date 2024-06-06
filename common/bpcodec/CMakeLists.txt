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
		src/codec/ZlibCrc32cCombine.cpp
		src/codec/Bpv6Fragment.cpp
		src/codec/Bpv6FragmentManager.cpp
)
target_compile_options(bpcodec PRIVATE ${NON_WINDOWS_HARDWARE_ACCELERATION_FLAGS})
GENERATE_EXPORT_HEADER(bpcodec)
get_target_property(target_type bpcodec TYPE)
if (target_type STREQUAL SHARED_LIBRARY)
	set_property(TARGET bpcodec PROPERTY CXX_VISIBILITY_PRESET hidden)
	set_property(TARGET bpcodec PROPERTY VISIBILITY_INLINES_HIDDEN ON)
endif()
target_link_libraries(bpcodec
	PUBLIC
		hdtn_util
		log_lib
		stats_lib
)
target_include_directories(bpcodec
	PUBLIC
		$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
		$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}> # for GENERATE_EXPORT_HEADER
)

set(MY_PUBLIC_HEADERS
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
	include/codec/Bpv6Fragment.h
	include/codec/Bpv6FragmentManager.h
	${CMAKE_CURRENT_BINARY_DIR}/bpcodec_export.h
)
set_target_properties(bpcodec PROPERTIES PUBLIC_HEADER "${MY_PUBLIC_HEADERS}") # this needs to be a list, so putting in quotes makes it a ; separated list
install(TARGETS bpcodec
	EXPORT bpcodec-targets
	DESTINATION "${CMAKE_INSTALL_LIBDIR}"
	PUBLIC_HEADER DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/codec"
)
add_hdtn_package_export(bpcodec Bpcodec) #exported target will have the name HDTN::Bpcodec and not bpcodec.  Also requires install to EXPORT bpcodec-targets
install(FILES
	../include/message.hpp
	../include/stats.hpp
	../include/HdtnVersion.hpp
	DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
add_library(bp_app_patterns_lib
        src/app_patterns/BpSourcePattern.cpp
		src/app_patterns/BpSinkPattern.cpp
)
GENERATE_EXPORT_HEADER(bp_app_patterns_lib)
get_target_property(target_type bp_app_patterns_lib TYPE)
if (target_type STREQUAL SHARED_LIBRARY)
	set_property(TARGET bp_app_patterns_lib PROPERTY CXX_VISIBILITY_PRESET hidden)
	set_property(TARGET bp_app_patterns_lib PROPERTY VISIBILITY_INLINES_HIDDEN ON)
endif()
target_link_libraries(bp_app_patterns_lib
	PUBLIC
		bpcodec
		hdtn_util
		outduct_manager_lib
		induct_manager_lib
	PRIVATE
		$<$<BOOL:${ENABLE_BPSEC}>:bpsec_lib>
)
target_include_directories(bp_app_patterns_lib
	PUBLIC
		$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
		$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}> # for GENERATE_EXPORT_HEADER
)
set(MY_PUBLIC_HEADERS
    include/app_patterns/BpSinkPattern.h
	include/app_patterns/BpSourcePattern.h
	${CMAKE_CURRENT_BINARY_DIR}/bp_app_patterns_lib_export.h
)
set_target_properties(bp_app_patterns_lib PROPERTIES PUBLIC_HEADER "${MY_PUBLIC_HEADERS}") # this needs to be a list, so putting in quotes makes it a ; separated list
install(TARGETS bp_app_patterns_lib
	EXPORT bp_app_patterns_lib-targets
	DESTINATION "${CMAKE_INSTALL_LIBDIR}"
	PUBLIC_HEADER DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/app_patterns"
)

set_property(
	SOURCE src/app_patterns/BpSourcePattern.cpp
	PROPERTY COMPILE_DEFINITIONS
		$<$<BOOL:${ENABLE_BPV7_PRIORITY}>:ENABLE_BPV7_PRIORITY>
)

add_hdtn_package_export(bp_app_patterns_lib BpAppPatternsLib) #exported target will have the name HDTN::BpAppPatternsLib and not bp_app_patterns_lib.  Also requires install to EXPORT bp_app_patterns_lib-targets

add_subdirectory(apps)
