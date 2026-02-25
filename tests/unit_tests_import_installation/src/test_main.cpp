/**
 * @file test_main.cpp
 * @author  Brian Tomko <brian.j.tomko@nasa.gov>
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
 * This file launches all HDTN unit tests (using Boost Test) into a process.
 * These unit tests are being compiled from a CMake installed version of HDTN
 * rather than from within the HDTN project itself.
 */


#define BOOST_TEST_MODULE HtdnUnitTestsModule

//todo: better cmake solution to detect if we are using boost static or shared libs... assume for now
//      that shared libs will be used on linux and static libs will be used on windows.
#ifndef _WIN32
#define BOOST_TEST_DYN_LINK
#endif

#include <boost/test/unit_test.hpp>
#include <boost/test/results_reporter.hpp>
#include <boost/test/unit_test_parameters.hpp>

// Global Test Fixture. Used to setup report options for all unit tests.
class BoostUnitTestsFixture {
public:
    BoostUnitTestsFixture();
    ~BoostUnitTestsFixture();
};

BoostUnitTestsFixture::BoostUnitTestsFixture() {
    boost::unit_test::results_reporter::set_level(boost::unit_test::report_level::DETAILED_REPORT);
    boost::unit_test::unit_test_log.set_threshold_level( boost::unit_test::log_messages );
}

BoostUnitTestsFixture::~BoostUnitTestsFixture() {
}

BOOST_GLOBAL_FIXTURE(BoostUnitTestsFixture);




