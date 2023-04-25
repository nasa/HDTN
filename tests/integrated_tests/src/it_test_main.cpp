/**
 * @file it_test_main.cpp
 *
 * @copyright Copyright © 2021 United States Government as represented by
 * the National Aeronautics and Space Administration.
 * No copyright is claimed in the United States under Title 17, U.S.Code.
 * All Other Rights Reserved.
 *
 * @section LICENSE
 * Released under the NASA Open Source Agreement (NOSA)
 * See LICENSE.md in the source root directory for more information.
 *
 * @section DESCRIPTION
 *
 * This file launches all HDTN integrated tests (using Boost Test) into a process.
 * The unit test framework will provide its own main() function.
 */


#define BOOST_TEST_MODULE HtdnTestsModule

//note: BOOST_TEST_DYN_LINK may be set as global compile definition by CMake script

//#define BOOST_TEST_NO_MAIN 1


#include <boost/test/unit_test.hpp>


