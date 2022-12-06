add_executable(hdtn-storage
        src/StorageRunner.cpp
        src/main.cpp
)
install(TARGETS hdtn-storage DESTINATION ${CMAKE_INSTALL_BINDIR})
target_link_libraries(hdtn-storage storage_lib)


add_library(storage_lib
        src/MemoryManagerTreeArray.cpp
        src/BundleStorageManagerMT.cpp
		src/BundleStorageManagerAsio.cpp
		src/BundleStorageManagerBase.cpp
		src/HashMap16BitFixedSize.cpp
		src/BundleStorageCatalog.cpp
		src/CustodyTimers.cpp
		src/CatalogEntry.cpp
        src/ZmqStorageInterface.cpp
)
target_compile_options(storage_lib PRIVATE ${NON_WINDOWS_HARDWARE_ACCELERATION_FLAGS})
GENERATE_EXPORT_HEADER(storage_lib)
get_target_property(target_type storage_lib TYPE)
if (target_type STREQUAL SHARED_LIBRARY)
	set_property(TARGET storage_lib PROPERTY CXX_VISIBILITY_PRESET hidden)
	set_property(TARGET storage_lib PROPERTY VISIBILITY_INLINES_HIDDEN ON)
endif()
set(MY_PUBLIC_HEADERS
    include/BundleStorageCatalog.h
	include/BundleStorageConfig.h
	include/BundleStorageManagerAsio.h
	include/BundleStorageManagerBase.h
	include/BundleStorageManagerMT.h
	include/CatalogEntry.h
	include/CustodyTimers.h
	include/HashMap16BitFixedSize.h
	include/MemoryManagerTreeArray.h
	include/StorageRunner.h
	include/ZmqStorageInterface.h
	${CMAKE_CURRENT_BINARY_DIR}/storage_lib_export.h
)
set_target_properties(storage_lib PROPERTIES PUBLIC_HEADER "${MY_PUBLIC_HEADERS}") # this needs to be a list, so putting in quotes makes it a ; separated list
target_link_libraries(storage_lib
	PUBLIC
		hdtn_util
		log_lib
		config_lib
		bpcodec
		telemetry_definitions
		Boost::program_options
)
target_include_directories(storage_lib
	PUBLIC
		$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
		$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}> # for GENERATE_EXPORT_HEADER
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../common/include> # for message.hpp
)
install(TARGETS storage_lib
	EXPORT storage_lib-targets
	DESTINATION "${CMAKE_INSTALL_LIBDIR}"
	PUBLIC_HEADER DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)
add_hdtn_package_export(storage_lib StorageLib) #exported target will have the name HDTN::StorageLib and not storage_lib.  Also requires install to EXPORT storage_lib-targets


add_executable(storage-speedtest
        src/test/StorageSpeedTestMain.cpp
)
install(TARGETS storage-speedtest DESTINATION ${CMAKE_INSTALL_BINDIR})
target_link_libraries(storage-speedtest storage_lib Boost::timer)
