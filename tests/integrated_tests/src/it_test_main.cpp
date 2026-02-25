/**
 * @file it_test_main.cpp
 *
 * @copyright Copyright (c) 2026 United States Government as represented by
 * the National Aeronautics and Space Administration.
 * No copyright is claimed in the United States under Title 17, U.S.Code.
 * All Other Rights Reserved.
 *
 * @section LICENSE
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
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


