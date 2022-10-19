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

/**
 * OutputTester is used for redirecting cout and cerr
 * into a test buffer so the data can be used in testing.
 */
class OutputTester {
public:
    void redirect_cout_cerr() {
        cerr_backup = std::cerr.rdbuf();
        cout_backup = std::cout.rdbuf();
        std::cerr.rdbuf(cerr_test_stream.rdbuf());
        std::cout.rdbuf(cout_test_stream.rdbuf());
    }

    void reset_cout_cerr() {
        std::cout.rdbuf(cout_backup);
        std::cerr.rdbuf(cerr_backup);
    }

    std::stringstream cerr_test_stream;
    std::stringstream cout_test_stream;

private:
    std::streambuf *cerr_backup;
    std::streambuf *cout_backup;
};

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
    BOOST_REQUIRE(hdtn::Logger::fromString("egress") == hdtn::Logger::Module::egress);
    BOOST_REQUIRE(hdtn::Logger::fromString("ingress") == hdtn::Logger::Module::ingress);
    BOOST_REQUIRE(hdtn::Logger::fromString("router") == hdtn::Logger::Module::router);
    BOOST_REQUIRE(hdtn::Logger::fromString("scheduler") == hdtn::Logger::Module::scheduler);
    BOOST_REQUIRE(hdtn::Logger::fromString("storage") == hdtn::Logger::Module::storage);
    BOOST_REQUIRE(hdtn::Logger::fromString("foobar") == hdtn::Logger::Module(-1));
}

#ifdef LOG_TO_CONSOLE
BOOST_AUTO_TEST_CASE(LoggerStdoutTestCase)
{
    // First, swap out the cout buffer with the boost test stream
    // so we can capture output
    OutputTester output_tester;
    output_tester.redirect_cout_cerr();

    // Do logging
    _LOG_INTERNAL(hdtn::Logger::Module::egress, trace) << "Egress foo bar";
    _LOG_INTERNAL(hdtn::Logger::Module::ingress, debug) << "Ingress foo bar";
    _LOG_INTERNAL(hdtn::Logger::Module::router, info) << "Router foo bar";
    _LOG_INTERNAL(hdtn::Logger::Module::scheduler, warning) << "Scheduler foo bar";

    // Put buffers back
    output_tester.reset_cout_cerr();

    // Assert results
    BOOST_REQUIRE_EQUAL(output_tester.cout_test_stream.str(),
        std::string("[egress][trace]: Egress foo bar\n") +
        std::string("[ingress][debug]: Ingress foo bar\n") +
        std::string("[router][info]: Router foo bar\n") +
        std::string("[scheduler][warning]: Scheduler foo bar\n")
    );
}
#endif

#ifdef LOG_TO_CONSOLE
BOOST_AUTO_TEST_CASE(LoggerStderrTestCase)
{
    // First, swap out the cerr buffer with the boost test stream
    // so we can capture output
    OutputTester output_tester;
    output_tester.redirect_cout_cerr();

    // Do logging
    _LOG_INTERNAL(hdtn::Logger::Module::egress, error) << "Egress foo bar!";
    _LOG_INTERNAL(hdtn::Logger::Module::ingress, fatal) << "Ingress foo bar!";

    // Put cout back
    output_tester.reset_cout_cerr();

    // Assert results
    BOOST_REQUIRE_EQUAL(output_tester.cerr_test_stream.str(),
        std::string("[egress][error]: Egress foo bar!\n") +
        std::string("[ingress][fatal]: Ingress foo bar!\n")
    );
}
#endif

#ifdef LOG_TO_SINGLE_FILE
BOOST_AUTO_TEST_CASE(LoggerHdtnFileTestCase)
{
    // First, swap out the cerr + cout buffers with the boost test stream
    // so we can capture output
    OutputTester output_tester;
    output_tester.redirect_cout_cerr();

    _LOG_INTERNAL(hdtn::Logger::Module::egress, info) << "Egress file test case";
    _LOG_INTERNAL(hdtn::Logger::Module::ingress, error) << "Ingress file test case";

    // Put cerr + cout back
    output_tester.reset_cout_cerr();

    // Assert results
    BOOST_TEST(boost::filesystem::exists("logs/"));
    BOOST_TEST(boost::filesystem::exists("logs/hdtn_00000.log"));
    BOOST_TEST(std::regex_match(
        file_contents_to_str("logs/hdtn_00000.log"),
        std::regex(anything_regex + "\\[egress]" + date_regex + "\\[info]: Egress file test case\n\\[ingress]" + date_regex + "\\[error]: Ingress file test case\n$"))
    );
}
#endif

#ifdef LOG_TO_MODULE_FILES
BOOST_AUTO_TEST_CASE(LoggerModuleFilesTestCase) {
    // First, swap out the cerr + cout buffers with the boost test stream
    // so we can capture output
    OutputTester output_tester;
    output_tester.redirect_cout_cerr();

    _LOG_INTERNAL(hdtn::Logger::Module::storage, info) << "Storage file test case";
    _LOG_INTERNAL(hdtn::Logger::Module::egress, error) << "Egress file test case";

    output_tester.reset_cout_cerr();

    BOOST_TEST(boost::filesystem::exists("logs/"));
    BOOST_TEST(boost::filesystem::exists("logs/storage_00000.log"));
    BOOST_TEST(std::regex_match(
        file_contents_to_str("logs/storage_00000.log"),
        std::regex(anything_regex + date_regex + "\\[info]: Storage file test case\n$"))
    );
    BOOST_TEST(boost::filesystem::exists("logs/egress_00000.log"));
    BOOST_TEST(std::regex_match(
        file_contents_to_str("logs/egress_00000.log"),
        std::regex(anything_regex + date_regex + "\\[error]: Egress file test case\n$"))
    );
}
#endif

#ifdef LOG_TO_ERROR_FILE
BOOST_AUTO_TEST_CASE(LoggerErrorFileTestCase) {
    OutputTester output_tester;
    output_tester.redirect_cout_cerr();

    _LOG_INTERNAL(hdtn::Logger::Module::ingress, error) << "Error file test case";

    output_tester.reset_cout_cerr();

    BOOST_TEST(boost::filesystem::exists("logs/"));
    BOOST_TEST(boost::filesystem::exists("logs/error_00000.log"));
    BOOST_TEST(std::regex_match(
        file_contents_to_str("logs/error_00000.log"),
        std::regex(anything_regex + "\\[ingress]" + date_regex + "\\[.*LoggerTests.cpp:\\d{3}]: Error file test case\n$"))
    );
}
#endif
