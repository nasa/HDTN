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

#include <boost/regex.hpp>
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

const std::string date_regex = "\\[ \\d{4}-\\d{2}-\\d{2} \\d{2}:\\d{2}:\\d{2}]";
const std::string anything_regex = "((.|\n)*)";

BOOST_AUTO_TEST_CASE(LoggerToStringTestCase)
{
    // Process
    BOOST_REQUIRE_EQUAL(hdtn::Logger::toString(hdtn::Logger::Process::bpgen), "bpgen");
    BOOST_REQUIRE_EQUAL(hdtn::Logger::toString(hdtn::Logger::Process::bping), "bping");
    BOOST_REQUIRE_EQUAL(hdtn::Logger::toString(hdtn::Logger::Process::bpreceivefile), "bpreceivefile");
    BOOST_REQUIRE_EQUAL(hdtn::Logger::toString(hdtn::Logger::Process::bpsendfile), "bpsendfile");
    BOOST_REQUIRE_EQUAL(hdtn::Logger::toString(hdtn::Logger::Process::bpsink), "bpsink");
    BOOST_REQUIRE_EQUAL(hdtn::Logger::toString(hdtn::Logger::Process::ltpfiletransfer), "ltpfiletransfer");
    BOOST_REQUIRE_EQUAL(hdtn::Logger::toString(hdtn::Logger::Process::egress), "egress");
    BOOST_REQUIRE_EQUAL(hdtn::Logger::toString(hdtn::Logger::Process::gui), "gui");
    BOOST_REQUIRE_EQUAL(hdtn::Logger::toString(hdtn::Logger::Process::unittest), "unittest");
    BOOST_REQUIRE_EQUAL(hdtn::Logger::toString(hdtn::Logger::Process::ingress), "ingress");
    BOOST_REQUIRE_EQUAL(hdtn::Logger::toString(hdtn::Logger::Process::router), "router");
    BOOST_REQUIRE_EQUAL(hdtn::Logger::toString(hdtn::Logger::Process::scheduler), "scheduler");
    BOOST_REQUIRE_EQUAL(hdtn::Logger::toString(hdtn::Logger::Process::storage), "storage");
    BOOST_REQUIRE_EQUAL(hdtn::Logger::toString(hdtn::Logger::Process::releasemessagesender), "releasemessagesender");
    BOOST_REQUIRE_EQUAL(hdtn::Logger::toString(hdtn::Logger::Process::storagespeedtest), "storagespeedtest");
    BOOST_REQUIRE_EQUAL(hdtn::Logger::toString(hdtn::Logger::Process::udpdelaysim), "udpdelaysim");
    BOOST_REQUIRE_EQUAL(hdtn::Logger::toString(hdtn::Logger::Process::none), "");

    // Subprocess
    BOOST_REQUIRE_EQUAL(hdtn::Logger::toString(hdtn::Logger::SubProcess::egress), "egress");
    BOOST_REQUIRE_EQUAL(hdtn::Logger::toString(hdtn::Logger::SubProcess::ingress), "ingress");
    BOOST_REQUIRE_EQUAL(hdtn::Logger::toString(hdtn::Logger::SubProcess::router), "router");
    BOOST_REQUIRE_EQUAL(hdtn::Logger::toString(hdtn::Logger::SubProcess::scheduler), "scheduler");
    BOOST_REQUIRE_EQUAL(hdtn::Logger::toString(hdtn::Logger::SubProcess::storage), "storage");
    BOOST_REQUIRE_EQUAL(hdtn::Logger::toString(hdtn::Logger::SubProcess::gui), "gui");
    BOOST_REQUIRE_EQUAL(hdtn::Logger::toString(hdtn::Logger::SubProcess::none), "");
}

BOOST_AUTO_TEST_CASE(LoggerFromStringTestCase)
{
    BOOST_REQUIRE(hdtn::Logger::fromString("egress") == hdtn::Logger::SubProcess::egress);
    BOOST_REQUIRE(hdtn::Logger::fromString("ingress") == hdtn::Logger::SubProcess::ingress);
    BOOST_REQUIRE(hdtn::Logger::fromString("router") == hdtn::Logger::SubProcess::router);
    BOOST_REQUIRE(hdtn::Logger::fromString("scheduler") == hdtn::Logger::SubProcess::scheduler);
    BOOST_REQUIRE(hdtn::Logger::fromString("storage") == hdtn::Logger::SubProcess::storage);
    BOOST_REQUIRE(hdtn::Logger::fromString("foobar") == hdtn::Logger::SubProcess::none);
}

#ifdef LOG_TO_CONSOLE
BOOST_AUTO_TEST_CASE(LoggerStdoutTestCase)
{
    // First, swap out the cout buffer with the boost test stream
    // so we can capture output
    OutputTester output_tester;
    output_tester.redirect_cout_cerr();

    // Do logging with sub-processes
    _LOG_INTERNAL(hdtn::Logger::SubProcess::egress, trace) << "Egress foo bar";
    _LOG_INTERNAL(hdtn::Logger::SubProcess::ingress, debug) << "Ingress foo bar";
    _LOG_INTERNAL(hdtn::Logger::SubProcess::router, info) << "Router foo bar";
    _LOG_INTERNAL(hdtn::Logger::SubProcess::scheduler, warning) << "Scheduler foo bar";

    // Put buffers back
    output_tester.reset_cout_cerr();

    // Assert results
    BOOST_REQUIRE_EQUAL(output_tester.cout_test_stream.str(),
        std::string("[ egress         ][ trace]: Egress foo bar\n") +
        std::string("[ ingress        ][ debug]: Ingress foo bar\n") +
        std::string("[ router         ][ info]: Router foo bar\n") +
        std::string("[ scheduler      ][ warning]: Scheduler foo bar\n")
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
    _LOG_INTERNAL(hdtn::Logger::SubProcess::egress, error) << "Egress foo bar!";
    _LOG_INTERNAL(hdtn::Logger::SubProcess::ingress, fatal) << "Ingress foo bar!";

    // Put cout back
    output_tester.reset_cout_cerr();

    // Assert results
    BOOST_REQUIRE_EQUAL(output_tester.cerr_test_stream.str(),
        std::string("[ egress         ][ error]: Egress foo bar!\n") +
        std::string("[ ingress        ][ fatal]: Ingress foo bar!\n")
    );
}
#endif

#ifdef LOG_TO_CONSOLE
BOOST_AUTO_TEST_CASE(LoggerMatchingSubprocessAndProcessTestCase)
{
    // If the process and sub-process match, only the process attribute should
    // be logged
    OutputTester output_tester;
    output_tester.redirect_cout_cerr();

    _LOG_INTERNAL(hdtn::Logger::SubProcess::unittest, info) << "Unittest foo bar";

    output_tester.reset_cout_cerr();
    BOOST_REQUIRE_EQUAL(output_tester.cout_test_stream.str(),
        std::string("[ unittest       ][ info]: Unittest foo bar\n")
    );
}
#endif

#ifdef LOG_TO_PROCESS_FILE
BOOST_AUTO_TEST_CASE(LoggerProcessFileTestCase)
{
    // First, swap out the cerr + cout buffers with the boost test stream
    // so we can capture output
    OutputTester output_tester;
    output_tester.redirect_cout_cerr();

    _LOG_INTERNAL(hdtn::Logger::SubProcess::egress, info) << "Egress file test case";
    _LOG_INTERNAL(hdtn::Logger::SubProcess::ingress, error) << "Ingress file test case";

    // Put cerr + cout back
    output_tester.reset_cout_cerr();

    // Assert results
    BOOST_TEST(boost::filesystem::exists("logs/"));
    BOOST_TEST(boost::filesystem::exists("logs/unittest_00000.log"));
    BOOST_TEST(boost::regex_match(
        file_contents_to_str("logs/unittest_00000.log"),
        boost::regex(anything_regex + "\\[ egress]" + date_regex + "\\[ info]: Egress file test case\n\\[ ingress]" + date_regex + "\\[ error]: Ingress file test case\n$"))
    );
}
#endif

#ifdef LOG_TO_SUBPROCESS_FILES
BOOST_AUTO_TEST_CASE(LoggerSubProcessFilesTestCase) {
    // First, swap out the cerr + cout buffers with the boost test stream
    // so we can capture output
    OutputTester output_tester;
    output_tester.redirect_cout_cerr();

    _LOG_INTERNAL(hdtn::Logger::SubProcess::storage, info) << "Storage file test case";
    _LOG_INTERNAL(hdtn::Logger::SubProcess::egress, error) << "Egress file test case";

    output_tester.reset_cout_cerr();

    BOOST_TEST(boost::filesystem::exists("logs/"));
    BOOST_TEST(boost::filesystem::exists("logs/storage_00000.log"));
    BOOST_TEST(boost::regex_match(
        file_contents_to_str("logs/storage_00000.log"),
        boost::regex(anything_regex + date_regex + "\\[ info]: Storage file test case\n$"))
    );
    BOOST_TEST(boost::filesystem::exists("logs/egress_00000.log"));
    BOOST_TEST(boost::regex_match(
        file_contents_to_str("logs/egress_00000.log"),
        boost::regex(anything_regex + date_regex + "\\[ error]: Egress file test case\n$"))
    );
}
#endif

#ifdef LOG_TO_ERROR_FILE
BOOST_AUTO_TEST_CASE(LoggerErrorFileTestCase) {
    OutputTester output_tester;
    output_tester.redirect_cout_cerr();

    _LOG_INTERNAL(hdtn::Logger::SubProcess::ingress, error) << "Error file test case";

    output_tester.reset_cout_cerr();

    BOOST_TEST(boost::filesystem::exists("logs/"));
    BOOST_TEST(boost::filesystem::exists("logs/error_00000.log"));
    BOOST_TEST(boost::regex_match(
        file_contents_to_str("logs/error_00000.log"),
        boost::regex(anything_regex + "\\[ unittest]\\[ ingress]" + date_regex + "\\[.*LoggerTests.cpp:\\d{3}]: Error file test case\n$"))
    );
}
#endif
