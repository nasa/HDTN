function(add_hdtn_package_export hdtn_library_name hdtn_export_name)
	add_library("HDTN::${hdtn_library_name}" ALIAS ${hdtn_library_name})
	#This is required so that the exported target has the name ${hdtn_export_name} and not ${hdtn_library_name}
	set_target_properties(${hdtn_library_name} PROPERTIES EXPORT_NAME ${hdtn_export_name})

	set(INSTALL_CONFIGDIR "${CMAKE_INSTALL_LIBDIR}/cmake/${hdtn_export_name}")

	#Export the targets to a script
	install(EXPORT "${hdtn_library_name}-targets"
		FILE
			"${hdtn_export_name}Targets.cmake"
		NAMESPACE
			HDTN::
		DESTINATION
			${INSTALL_CONFIGDIR}
	)

	#Create a ConfigVersion.cmake file
	include(CMakePackageConfigHelpers)
	write_basic_package_version_file(
		"${CMAKE_CURRENT_BINARY_DIR}/${hdtn_export_name}ConfigVersion.cmake"
		VERSION ${HDTN_VERSION}
		COMPATIBILITY AnyNewerVersion
	)

	configure_package_config_file("${CMAKE_CURRENT_LIST_DIR}/${hdtn_export_name}Config.cmake.in"
		"${CMAKE_CURRENT_BINARY_DIR}/${hdtn_export_name}Config.cmake"
		INSTALL_DESTINATION ${INSTALL_CONFIGDIR}
	)

	#Install the config, configversion
	install(FILES
		"${CMAKE_CURRENT_BINARY_DIR}/${hdtn_export_name}Config.cmake"
		"${CMAKE_CURRENT_BINARY_DIR}/${hdtn_export_name}ConfigVersion.cmake"
		DESTINATION ${INSTALL_CONFIGDIR}
	)

endfunction()
