add_library(bpsec 
	    src/BPSecManager.cpp
		src/InitializationVectors.cpp
)

target_compile_options(bpsec PRIVATE ${NON_WINDOWS_HARDWARE_ACCELERATION_FLAGS})
GENERATE_EXPORT_HEADER(bpsec)
get_target_property(target_type bpsec TYPE)
if (target_type STREQUAL SHARED_LIBRARY)
	set_property(TARGET bpsec PROPERTY CXX_VISIBILITY_PRESET hidden)
	set_property(TARGET bpsec PROPERTY VISIBILITY_INLINES_HIDDEN ON)
endif()
target_link_libraries(bpsec
	PUBLIC
		hdtn_util
		bpcodec
		log_lib
)

target_include_directories(bpsec 
	PUBLIC 
		$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
		$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}> # for GENERATE_EXPORT_HEADER
)	

set(MY_PUBLIC_HEADERS
	include/BPSecManager.h
	include/InitializationVectors.h
	${CMAKE_CURRENT_BINARY_DIR}/bpsec_export.h
)

set_target_properties(bpsec PROPERTIES PUBLIC_HEADER "${MY_PUBLIC_HEADERS}") # this needs to be a list, so putting in quotes makes it a ; separated list
install(TARGETS bpsec
	EXPORT bpsec-targets
	DESTINATION "${CMAKE_INSTALL_LIBDIR}"
	PUBLIC_HEADER DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)

add_hdtn_package_export(bpsec Bpsec) #exported target will have the name HDTN::Bpcodec and not bpcodec.  Also requires install to EXPORT bpcodec-targets
install(FILES
	DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

#Enable Address Sanitizer (Asan) for GCC/Clang
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address")
#set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address")

#add_subdirectory(policies)

