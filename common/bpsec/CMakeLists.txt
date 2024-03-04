add_library(bpsec_lib 
	    src/BpSecBundleProcessor.cpp
		src/BpSecPolicyManager.cpp
		src/InitializationVectors.cpp
)

target_compile_options(bpsec_lib PRIVATE ${NON_WINDOWS_HARDWARE_ACCELERATION_FLAGS})
GENERATE_EXPORT_HEADER(bpsec_lib)
get_target_property(target_type bpsec_lib TYPE)
if (target_type STREQUAL SHARED_LIBRARY)
	set_property(TARGET bpsec_lib PROPERTY CXX_VISIBILITY_PRESET hidden)
	set_property(TARGET bpsec_lib PROPERTY VISIBILITY_INLINES_HIDDEN ON)
endif()
target_link_libraries(bpsec_lib
	PUBLIC
		hdtn_util
		bpcodec
		config_lib
		log_lib
)

target_include_directories(bpsec_lib 
	PUBLIC 
		$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
		$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}> # for GENERATE_EXPORT_HEADER
)	

set(MY_PUBLIC_HEADERS
	include/BpSecBundleProcessor.h
	include/BpSecPolicyManager.h
	include/InitializationVectors.h
	${CMAKE_CURRENT_BINARY_DIR}/bpsec_lib_export.h
)

set_target_properties(bpsec_lib PROPERTIES PUBLIC_HEADER "${MY_PUBLIC_HEADERS}") # this needs to be a list, so putting in quotes makes it a ; separated list
install(TARGETS bpsec_lib
	EXPORT bpsec_lib-targets
	DESTINATION "${CMAKE_INSTALL_LIBDIR}"
	PUBLIC_HEADER DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)

add_hdtn_package_export(bpsec_lib BpsecLib) #exported target will have the name HDTN::BpsecLib and not bpsec_lib.  Also requires install to EXPORT bpsec_lib-targets

