/**
 * @file LoggerTests.cpp
 * @author  Ethan Schweinsberg <ethan.e.schweinsberg@nasa.gov>
 *
 * @copyright Copyright � 2021 United States Government as represented by
 * the National Aeronautics and Space Administration.
 * No copyright is claimed in the United States under Title 17, U.S.Code.
 * All Other Rights Reserved.
 *
 * @section LICENSE
 * Released under the NASA Open Source Agreement (NOSA)
 * See LICENSE.md in the source root directory for more information.
 */

#include <boost/test/unit_test.hpp>
#include <boost/test/tools/output_test_stream.hpp>
#include "Logger.h"

BOOST_AUTO_TEST_CASE(LoggerToStringTestCase)
{
    BOOST_REQUIRE_EQUAL(hdtn::Logger::toString(hdtn::Logger::Module::egress), "egress");
    BOOST_REQUIRE_EQUAL(hdtn::Logger::toString(hdtn::Logger::Module::ingress), "ingress");
    BOOST_REQUIRE_EQUAL(hdtn::Logger::toString(hdtn::Logger::Module::router), "router");
    BOOST_REQUIRE_EQUAL(hdtn::Logger::toString(hdtn::Logger::Module::scheduler), "scheduler");
    BOOST_REQUIRE_EQUAL(hdtn::Logger::toString(hdtn::Logger::Module::storage), "storage");
    BOOST_REQUIRE_EQUAL(hdtn::Logger::toString(hdtn::Logger::Module(20)), "");
}

BOOST_AUTO_TEST_CASE(LoggerFromStringTestCase)
{
    BOOST_REQUIRE_EQUAL(hdtn::Logger::fromString("egress"), hdtn::Logger::Module::egress);
    BOOST_REQUIRE_EQUAL(hdtn::Logger::fromString("ingress"), hdtn::Logger::Module::ingress);
    BOOST_REQUIRE_EQUAL(hdtn::Logger::fromString("router"), hdtn::Logger::Module::router);
    BOOST_REQUIRE_EQUAL(hdtn::Logger::fromString("scheduler"), hdtn::Logger::Module::scheduler);
    BOOST_REQUIRE_EQUAL(hdtn::Logger::fromString("storage"), hdtn::Logger::Module::storage);
    BOOST_REQUIRE_EQUAL(hdtn::Logger::fromString(""), hdtn::Logger::Module(-1));
}

BOOST_AUTO_TEST_CASE(LoggerEnsureInitializedTestCase)
{
    // Logger should only get initialized once
    BOOST_REQUIRE_EQUAL(hdtn::Logger::ensureInitialized(), true);
    BOOST_REQUIRE_EQUAL(hdtn::Logger::ensureInitialized(), false);
    BOOST_REQUIRE_EQUAL(hdtn::Logger::ensureInitialized(), false);
}

BOOST_AUTO_TEST_CASE(LoggerLogInternalTestCaseStdout)
{
    // First, swap out the cout buffer with the boost test stream
    // so we can capture output
    std::streambuf *backup = std::cout.rdbuf();
    boost::test_tools::output_test_stream test_out;
    std::cout.rdbuf(test_out.rdbuf());

    // Do logging
    _LOG_INTERNAL(hdtn::Logger::Module::egress, trace) << "Egress foo bar";
    _LOG_INTERNAL(hdtn::Logger::Module::ingress, debug) << "Ingress foo bar";
    _LOG_INTERNAL(hdtn::Logger::Module::router, info) << "Router foo bar";
    _LOG_INTERNAL(hdtn::Logger::Module::scheduler, warning) << "Scheduler foo bar";

    // Put cout back
    std::cout.rdbuf(backup);

    // Assert results
    BOOST_TEST(test_out.is_equal(
        std::string("[egress][trace]: Egress foo bar\n") +
        std::string("[ingress][debug]: Ingress foo bar\n") +
        std::string("[router][info]: Router foo bar\n") +
        std::string("[scheduler][warning]: Scheduler foo bar\n")
    ));
}

BOOST_AUTO_TEST_CASE(LoggerLogInternalTestCaseStderr)
{
    // First, swap out the cout buffer with the boost test stream
    // so we can capture output
    std::streambuf *backup = std::cerr.rdbuf();
    boost::test_tools::output_test_stream test_out;
    std::cerr.rdbuf(test_out.rdbuf());

    // Do logging
    _LOG_INTERNAL(hdtn::Logger::Module::egress, error) << "Egress foo bar!";
    _LOG_INTERNAL(hdtn::Logger::Module::ingress, fatal) << "Ingress foo bar!";

    // Put cout back
    std::cerr.rdbuf(backup);

    // Assert results
    BOOST_TEST(test_out.is_equal(
        std::string("[egress][error]: Egress foo bar!\n") +
        std::string("[ingress][fatal]: Ingress foo bar!\n")
    ));
}