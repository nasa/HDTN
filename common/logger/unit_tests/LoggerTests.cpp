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

#include <regex>
#include <boost/filesystem.hpp>
#include <boost/test/unit_test.hpp>
#include <boost/test/tools/output_test_stream.hpp>
#include "Logger.h"

/**
 * Reads a file's contents into a string and returns it
 */
std::string file_contents_to_str(std::string path) {
    std::ifstream in(path);
    std::stringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

const std::string date_regex = "\\[\\d{4}-\\d{2}-\\d{2} \\d{2}:\\d{2}:\\d{2}]";
const std::string anything_regex = "((.|\n)*)";

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

BOOST_AUTO_TEST_CASE(LoggerLogInternalStdoutTestCase)
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

BOOST_AUTO_TEST_CASE(LoggerLogInternalStderrTestCase)
{
    // First, swap out the cerr buffer with the boost test stream
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

BOOST_AUTO_TEST_CASE(LoggerLogInternalFilesTestCase)
{
    // First, swap out the cerr + cout buffers with the boost test stream
    // so we can capture output
    std::streambuf *cerr_backup = std::cerr.rdbuf();
    std::streambuf *cout_backup = std::cout.rdbuf();
    boost::test_tools::output_test_stream cerr_test_out;
    boost::test_tools::output_test_stream cout_test_out;
    std::cerr.rdbuf(cerr_test_out.rdbuf());
    std::cout.rdbuf(cout_test_out.rdbuf()); 

    _LOG_INTERNAL(hdtn::Logger::Module::egress, info) << "Egress files test case";
    _LOG_INTERNAL(hdtn::Logger::Module::ingress, error) << "Ingress files test case";

    // Put cerr + cout back
    std::cerr.rdbuf(cerr_backup);
    std::cout.rdbuf(cout_backup);

    // Assert results
    BOOST_TEST(boost::filesystem::exists("logs/"));
    BOOST_TEST(boost::filesystem::exists("logs/egress_00000.log"));
    BOOST_TEST(std::regex_match(
        file_contents_to_str("logs/egress_00000.log"),
        std::regex(anything_regex + date_regex + "\\[info]: Egress files test case\n$"))
    );
    BOOST_TEST(boost::filesystem::exists("logs/ingress_00000.log"));
    BOOST_TEST(std::regex_match(
        file_contents_to_str("logs/ingress_00000.log"),
        std::regex(anything_regex + date_regex + "\\[error]: Ingress files test case\n$"))
    );
    BOOST_TEST(boost::filesystem::exists("logs/error_00000.log"));
    BOOST_TEST(std::regex_match(
        file_contents_to_str("logs/error_00000.log"),
        std::regex(anything_regex + "\\[ingress]" + date_regex + "\\[.*LoggerTests.cpp:\\d{3}]: Ingress files test case\n$"))
    );
    BOOST_TEST(boost::filesystem::exists("logs/hdtn_00000.log"));
    BOOST_TEST(std::regex_match(
        file_contents_to_str("logs/hdtn_00000.log"),
        std::regex(anything_regex + "\\[egress]" + date_regex + "\\[info]: Egress files test case\n\\[ingress]" + date_regex + "\\[error]: Ingress files test case\n$"))
    );
}
