add_library(masker_lib
    src/Masker.cpp
	src/RedundantMasker.cpp
	src/ShiftingMasker.cpp
)
GENERATE_EXPORT_HEADER(masker_lib)
get_target_property(target_type masker_lib TYPE)
if (target_type STREQUAL SHARED_LIBRARY)
	set_property(TARGET masker_lib PROPERTY CXX_VISIBILITY_PRESET hidden)
	set_property(TARGET masker_lib PROPERTY VISIBILITY_INLINES_HIDDEN ON)
endif()
set(MY_PUBLIC_HEADERS
    include/Masker.h
	include/RedundantMasker.h
	include/ShiftingMasker.h
	${CMAKE_CURRENT_BINARY_DIR}/masker_lib_export.h
)
set_target_properties(masker_lib PROPERTIES PUBLIC_HEADER "${MY_PUBLIC_HEADERS}") # this needs to be a list, so putting in quotes makes it a ; separated list
target_link_libraries(masker_lib
	PUBLIC
		bpcodec
)
target_include_directories(masker_lib
	PUBLIC
		$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
		$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}> # for GENERATE_EXPORT_HEADER
)
install(TARGETS masker_lib
	EXPORT masker_lib-targets
	DESTINATION "${CMAKE_INSTALL_LIBDIR}"
	PUBLIC_HEADER DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)
add_hdtn_package_export(masker_lib HDTNMaskerLib) #exported target will have the name HDTN::HDTNMaskerLib and not masker_lib. Also requires install to EXPORT masker_lib-targets
