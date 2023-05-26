# @file boost_test_link_type.cmake
# @author  Brian Tomko <brian.j.tomko@nasa.gov>
#
# @copyright Copyright © 2021 United States Government as represented by
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
# This script detects if BOOST_TEST_DYN_LINK (or BOOST_ALL_DYN_LINK) is needed for compiling the unit-tests.
# If it's needed, add BOOST_ALL_DYN_LINK as a global compile definition for both the unit-tests and the logger.
# This script should be more reliable than trying to determine based on whether the user set Boost_USE_STATIC_LIBS.

unset(COMPILER_DOES_NOT_NEED_BOOST_ALL_DYN_LINK CACHE) #remove cache variable so it will always rerun this check
unset(COMPILER_NEEDS_BOOST_ALL_DYN_LINK CACHE) #remove cache variable so it will always rerun this check
SET(CMAKE_REQUIRED_LIBRARIES Boost::unit_test_framework)
check_cxx_source_compiles("
	#define BOOST_TEST_MODULE HtdnUnitTestsModule
	#include <boost/test/unit_test.hpp>
	BOOST_AUTO_TEST_CASE(LinkTestCase) {
		BOOST_REQUIRE(true);
	}" COMPILER_DOES_NOT_NEED_BOOST_ALL_DYN_LINK)



#add compile definitions for x86 hardware acceleration
if(COMPILER_DOES_NOT_NEED_BOOST_ALL_DYN_LINK)
	message("Compiler does not need BOOST_ALL_DYN_LINK")
	unset(COMPILER_DOES_NOT_NEED_BOOST_ALL_DYN_LINK CACHE) #remove cache variable so it will always rerun this check
else()
	unset(COMPILER_DOES_NOT_NEED_BOOST_ALL_DYN_LINK CACHE) #remove cache variable so it will always rerun this check
	message("Compiler might need BOOST_ALL_DYN_LINK.. testing")
	check_cxx_source_compiles("
		#define BOOST_TEST_MODULE HtdnUnitTestsModule
		#define BOOST_ALL_DYN_LINK
		#include <boost/test/unit_test.hpp>
		BOOST_AUTO_TEST_CASE(LinkTestCase) {
			BOOST_REQUIRE(true);
		}" COMPILER_NEEDS_BOOST_ALL_DYN_LINK)
	if(COMPILER_NEEDS_BOOST_ALL_DYN_LINK)
		message("Compiler needs BOOST_ALL_DYN_LINK.. adding compile definition BOOST_ALL_DYN_LINK")
		add_compile_definitions(BOOST_ALL_DYN_LINK)
		unset(COMPILER_NEEDS_BOOST_ALL_DYN_LINK CACHE) #remove cache variable so it will always rerun this check
	else()
		unset(COMPILER_NEEDS_BOOST_ALL_DYN_LINK CACHE) #remove cache variable so it will always rerun this check
		message(FATAL_ERROR "Cannot compile boost test program.")
	endif()
endif()
