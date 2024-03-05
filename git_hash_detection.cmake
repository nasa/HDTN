# @file git_hash_detection.cmake
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
# This script detects the git hash of HDTN (storing in CMake variable HDTN_COMMIT_SHA) by:
# 1) if building from a cloned git repo (with a .git directory in the source root), get the hash using git
# 2) if building from zip, tar[.bz2|.gz], use the sha hash within the archive file comment header..
#    e.g. cmake -DHDTN_ARCHIVE_FILE=/path/to/hdtn.zip ..

#CMAKE regex doesn't support the {n} format
string(REPEAT "[0-9a-f]" 40 REGEX_SHA1_MATCH)
message("$ENV{path}")
if(WIN32)
	set(where_cmd "where")
else()
	set(where_cmd "which")
endif()
set(list_of_bin_utilities git cat zcat bzcat unzip 7z)
foreach(current_utility IN LISTS list_of_bin_utilities)
	#message("testing ${current_utility}")
	execute_process(
		COMMAND "${where_cmd}" "${current_utility}" #where and which write to stdout on success
		OUTPUT_VARIABLE HAS_${current_utility}
		ERROR_VARIABLE JUNK_TO_IGNORE)
	if(HAS_${current_utility})
		message("${current_utility} detected in path")
	else()
		message("${current_utility} NOT detected in path")
	endif()
endforeach()

function(GitGetTarCommitId firstCommand)
	if(NOT HAS_git)
		message(FATAL_ERROR "git required but not found in path.")
	elseif(NOT HAS_${firstCommand})
		message(FATAL_ERROR "${firstCommand} required but not found in path.")
	endif()
	#Commands are executed concurrently as a pipeline,
	#with the standard output of each process
	# piped to the standard input of the next
	execute_process(
		COMMAND "${firstCommand}" "${HDTN_ARCHIVE_FILE}"
		COMMAND git get-tar-commit-id
		OUTPUT_VARIABLE GIT_GET_TAR_COMMIT_ID_OUTPUT
		OUTPUT_STRIP_TRAILING_WHITESPACE)
	if(NOT GIT_GET_TAR_COMMIT_ID_OUTPUT)
		message(FATAL_ERROR "Unable to run: ${firstCommand} ${HDTN_ARCHIVE_FILE} | git get-tar-commit-id")
	endif()
	string(REGEX MATCH "^${REGEX_SHA1_MATCH}$" HDTN_COMMIT_SHA_MATCH "${GIT_GET_TAR_COMMIT_ID_OUTPUT}")
	if(NOT HDTN_COMMIT_SHA_MATCH)
		message(FATAL_ERROR "Command [${firstCommand} ${HDTN_ARCHIVE_FILE} | git get-tar-commit-id] produced an invalid sha1 hash: ${GIT_GET_TAR_COMMIT_ID_OUTPUT}")
	endif()
	set(HDTN_COMMIT_SHA "${HDTN_COMMIT_SHA_MATCH}" PARENT_SCOPE)
endfunction(GitGetTarCommitId)

if(EXISTS "${CMAKE_SOURCE_DIR}/.git")
	message("This is a git repository.")
	execute_process(COMMAND git rev-parse HEAD
		WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        OUTPUT_VARIABLE GIT_REVPARSE_OUTPUT
		ERROR_VARIABLE GIT_REVPARSE_OUTPUT_ERROR
        OUTPUT_STRIP_TRAILING_WHITESPACE)

	if(GIT_REVPARSE_OUTPUT_ERROR)
		message(FATAL_ERROR "Command git rev-parse HEAD produced this error: ${GIT_REVPARSE_OUTPUT_ERROR}")
	endif()
	if(NOT GIT_REVPARSE_OUTPUT)
		message(FATAL_ERROR "Cannot execute git rev-parse HEAD!")
	endif()
	string(REGEX MATCH "^${REGEX_SHA1_MATCH}$" HDTN_COMMIT_SHA "${GIT_REVPARSE_OUTPUT}")
	if(NOT HDTN_COMMIT_SHA)
		message(FATAL_ERROR "Command git rev-parse HEAD produced an invalid sha1 hash: ${GIT_REVPARSE_OUTPUT}")
	endif()
elseif(HDTN_ARCHIVE_FILE)
	message("This is NOT a git repository but cmake variable HDTN_ARCHIVE_FILE was set.. detecting git sha from archive.")
	if(NOT EXISTS "${HDTN_ARCHIVE_FILE}")
		message(FATAL_ERROR "HDTN_ARCHIVE_FILE was set but points to non-existant file: ${HDTN_ARCHIVE_FILE}")
	endif()
	
	string(TOLOWER "${HDTN_ARCHIVE_FILE}" HDTN_ARCHIVE_FILE_LOWERCASE )
	if(HDTN_ARCHIVE_FILE_LOWERCASE MATCHES "\\.tar\\.gz$")
		message("tar.gz file detected")
		GitGetTarCommitId(zcat)
	elseif(HDTN_ARCHIVE_FILE_LOWERCASE MATCHES "\\.tar\\.bz2$")
		message("tar.bz2 file detected")
		GitGetTarCommitId(bzcat)
	elseif(HDTN_ARCHIVE_FILE_LOWERCASE MATCHES "\\.tar$")
		message("tar file detected")
		GitGetTarCommitId(cat)
	elseif(HDTN_ARCHIVE_FILE_LOWERCASE MATCHES "\\.zip$")
		message("zip file detected")
		if(HAS_unzip)
			execute_process(
				COMMAND "unzip" "-zq" "${HDTN_ARCHIVE_FILE}"
				OUTPUT_VARIABLE GIT_GET_UNZIP_COMMENT_OUTPUT
				OUTPUT_STRIP_TRAILING_WHITESPACE)
			if(NOT GIT_GET_UNZIP_COMMENT_OUTPUT)
				message(FATAL_ERROR "Unable to run: unzip -zq ${HDTN_ARCHIVE_FILE}")
			endif()
			string(REGEX MATCH "^${REGEX_SHA1_MATCH}$" HDTN_COMMIT_SHA "${GIT_GET_UNZIP_COMMENT_OUTPUT}")
			if(NOT HDTN_COMMIT_SHA)
				message(FATAL_ERROR "Command [unzip -zq ${HDTN_ARCHIVE_FILE}] produced an invalid sha1 hash: ${GIT_GET_UNZIP_COMMENT_OUTPUT}")
			endif()
		elseif(HAS_7z)
			execute_process(
				COMMAND "7z" "t" "-slt" "${HDTN_ARCHIVE_FILE}"
				OUTPUT_VARIABLE GIT_GET_UNZIP_COMMENT_OUTPUT
				OUTPUT_STRIP_TRAILING_WHITESPACE)
			if(NOT GIT_GET_UNZIP_COMMENT_OUTPUT)
				message(FATAL_ERROR "Unable to run: 7z t -slt ${HDTN_ARCHIVE_FILE}")
			endif()
			string(TOLOWER "${GIT_GET_UNZIP_COMMENT_OUTPUT}" GIT_GET_UNZIP_COMMENT_OUTPUT_LOWERCASE )
			#message(FATAL_ERROR "${GIT_GET_UNZIP_COMMENT_OUTPUT_LOWERCASE}")
			string(REGEX MATCH "comment[ ]?=[ ]?(${REGEX_SHA1_MATCH})" HDTN_COMMIT_SHA_LIST "${GIT_GET_UNZIP_COMMENT_OUTPUT_LOWERCASE}")
			if(HDTN_COMMIT_SHA_LIST AND CMAKE_MATCH_1)
				set(HDTN_COMMIT_SHA "${CMAKE_MATCH_1}")
			else()
				message(FATAL_ERROR "Command [7z t -slt ${HDTN_ARCHIVE_FILE}] produced an invalid sha1 hash: ${GIT_GET_UNZIP_COMMENT_OUTPUT}")
			endif()
		else()
			message(FATAL_ERROR "unzip or 7z required but not found in path.")
		endif()
	else()
		message(FATAL_ERROR "HDTN_ARCHIVE_FILE ${HDTN_ARCHIVE_FILE} must have an extension of [zip|tar|tar.gz|tar.bz2]")
	endif()
else()
	message(WARNING "HDTN will not be compiled with the current git commit sha1 hash printed to the logger.  If you downloaded HDTN as an archive without cloning, please rerun cmake with -DHDTN_ARCHIVE_FILE=/path/to/downloaded_hdtn_archive.[zip|tar|tar.gz|tar.bz2]")
endif()

if(HDTN_COMMIT_SHA)
	message("HDTN Git SHA-1 hash detected as: ${HDTN_COMMIT_SHA}")
endif()
