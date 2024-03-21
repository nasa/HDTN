# @file civetweb_builder.cmake
# @author  Brian Tomko <brian.j.tomko@nasa.gov>
#
# @copyright Copyright (c) 2021 United States Government as represented by
# the National Aeronautics and Space Administration.
# No copyright is claimed in the United States under Title 17, U.S.Code.
# All Other Rights Reserved.
#
# @section LICENSE
# Released under the NASA Open Source Agreement (NOSA)
# See LICENSE.md in the source root directory for more information.
#
# @section DESCRIPTION
#
# This script downloads and builds the civetweb library from source.


SET(CIVETWEB_TAG "v1.16")
OPTION(WEB_INTERFACE_USE_CIVETWEB "Use lightweight CivetWeb ${CIVETWEB_TAG} (statically built) instead of Boost Beast for the GUI backend webserver" OFF)
SET(CIVETWEB_ZIP_FILE "${CIVETWEB_TAG}.zip")
SET(CIVETWEB_PREDOWNLOADED_ZIP_FILE "" CACHE FILEPATH "Option to provide path to CivetWeb github source .zip file (must be ${CIVETWEB_TAG}) instead of having CMake automatically download it.")

if(WEB_INTERFACE_USE_CIVETWEB)
	if(NOT USE_WEB_INTERFACE)
		message(FATAL_ERROR "WEB_INTERFACE_USE_CIVETWEB was set to ON, but USE_WEB_INTERFACE was set to OFF instead of ON")
	endif()
	find_package(Git)
	if(Git_FOUND)
	  message("Git found: ${GIT_EXECUTABLE}")
	endif()
	include(ExternalProject)
	set(CIVETWEB_URL_TYPE "URL")
	set(CIVETWEB_URL_MD5_TYPE "URL_MD5")
	set(CIVETWEB_URL_MD5_VALUE "106ca921e4c9e3d3d76e0bcd937ef50c")
	set(CIVETWEB_GIT_TAG_TYPE "")
	set(CIVETWEB_GIT_TAG_VALUE "")
	if(CIVETWEB_PREDOWNLOADED_ZIP_FILE MATCHES ".zip$")
		SET(CIVETWEB_DOWNLOAD_LINK "${CIVETWEB_PREDOWNLOADED_ZIP_FILE}")
		message("won't download CivetWeb, using local source archive file: ${CIVETWEB_DOWNLOAD_LINK}")
	elseif(Git_FOUND)
		set(CIVETWEB_URL_MD5_TYPE "")
		set(CIVETWEB_URL_MD5_VALUE "")
		set(CIVETWEB_GIT_TAG_TYPE "GIT_TAG")
		set(CIVETWEB_GIT_TAG_VALUE "${CIVETWEB_TAG}")
		set(CIVETWEB_URL_TYPE "GIT_REPOSITORY")
		SET(CIVETWEB_DOWNLOAD_LINK "https://github.com/civetweb/civetweb.git")
		message("will use git to download CivetWeb from: ${CIVETWEB_DOWNLOAD_LINK}")
	else()
		SET(CIVETWEB_DOWNLOAD_LINK "https://github.com/civetweb/civetweb/archive/refs/tags/${CIVETWEB_ZIP_FILE}")
		message("will download CivetWeb from: ${CIVETWEB_DOWNLOAD_LINK}")
	endif()
	SET(CIVETWEB_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/third_party_civetweb")
	SET(CIVETWEB_INSTALL_DIRECTORY "${CIVETWEB_WORKING_DIRECTORY}/install")
	SET(CIVETWEB_INSTALL_LIB_DIRECTORY "${CIVETWEB_INSTALL_DIRECTORY}/lib")
	SET(CIVETWEB_INSTALL_INCLUDE_DIRECTORY "${CIVETWEB_INSTALL_DIRECTORY}/include")

	if(${CMAKE_VERSION} VERSION_GREATER_EQUAL "3.24")
		set(DOWNLOAD_EXTRACT_TIMESTAMP_PARAM DOWNLOAD_EXTRACT_TIMESTAMP)
		set(DOWNLOAD_EXTRACT_TIMESTAMP_VAL TRUE)
	else()
		set(DOWNLOAD_EXTRACT_TIMESTAMP_PARAM "")
		set(DOWNLOAD_EXTRACT_TIMESTAMP_VAL "")
	endif()
	#SET(CMAKE_CXX_FLAGS_CIVETWEB ${CMAKE_CXX_FLAGS})
	#SET(CMAKE_C_FLAGS_CIVETWEB ${CMAKE_C_FLAGS})
	#string(REGEX REPLACE "-Werror" "" CMAKE_CXX_FLAGS_CIVETWEB "${CMAKE_CXX_FLAGS_CIVETWEB}")
	#string(REGEX REPLACE "-Werror" "" CMAKE_C_FLAGS_CIVETWEB "${CMAKE_C_FLAGS_CIVETWEB}")
	#message("CMAKE_CXX_FLAGS_CIVETWEB: ${CMAKE_CXX_FLAGS_CIVETWEB}")
	#message("CMAKE_C_FLAGS_CIVETWEB: ${CMAKE_C_FLAGS_CIVETWEB}")

	ExternalProject_Add(civetweb-custom-target
		${CIVETWEB_URL_TYPE} ${CIVETWEB_DOWNLOAD_LINK}
		${CIVETWEB_URL_MD5_TYPE} ${CIVETWEB_URL_MD5_VALUE}
		${CIVETWEB_GIT_TAG_TYPE} ${CIVETWEB_GIT_TAG_VALUE}
		PREFIX ${CIVETWEB_WORKING_DIRECTORY}
		#$<$<VERSION_GREATER_EQUAL:$<CMAKE_VERSION>,3.24>:DOWNLOAD_EXTRACT_TIMESTAMP TRUE>
		${DOWNLOAD_EXTRACT_TIMESTAMP_PARAM} ${DOWNLOAD_EXTRACT_TIMESTAMP_VAL}
		CMAKE_ARGS 
			-DCMAKE_BUILD_TYPE=Release
			#-DCMAKE_CXX_FLAGS=${CMAKE_CXX_FLAGS_CIVETWEB}
			#-DCMAKE_C_FLAGS=${CMAKE_C_FLAGS_CIVETWEB}
			-DCMAKE_INSTALL_PREFIX:PATH=${CIVETWEB_INSTALL_DIRECTORY}
			-DBUILD_TESTING:BOOL=OFF
			-DCIVETWEB_BUILD_TESTING:BOOL=OFF
			-DCIVETWEB_ENABLE_ASAN:BOOL=OFF
			-DCIVETWEB_ENABLE_CXX:BOOL=ON
			#-DCIVETWEB_ENABLE_DEBUG_TOOLS:BOOL=OFF
			-DCIVETWEB_ENABLE_SERVER_EXECUTABLE:BOOL=OFF
			-DCIVETWEB_INSTALL_EXECUTABLE:BOOL=OFF
			-DCIVETWEB_ENABLE_WEBSOCKETS:BOOL=ON
			-DCIVETWEB_ENABLE_SSL:BOOL=OFF
	)
	link_directories(${CIVETWEB_INSTALL_LIB_DIRECTORY})
	if(WIN32)
		SET(CIVETWEB_LIB "civetweb.lib")
		SET(CIVETWEB_CPP_LIB "civetweb-cpp.lib")
	else()
		SET(CIVETWEB_LIB "civetweb")
		SET(CIVETWEB_CPP_LIB "civetweb-cpp")
	endif()
endif()
