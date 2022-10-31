/**
 * @file StatsLoggerTests.cpp
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

#include <fstream>
#include <regex>
#include <boost/filesystem.hpp>
#include <boost/test/unit_test.hpp>
#include "StatsLogger.h"

static const std::string timestamp_regex = "\\d{13}";
static const std::string anything_regex = "((.|\n)*)";

/**
 * Reads a file's contents into a string and returns it
 */
static std::string file_contents_to_str(std::string path) {
    std::ifstream in(path);
    std::stringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

#ifdef DO_STATS_LOGGING
BOOST_AUTO_TEST_CASE(StatsLoggerLogMetrics)
{
    LOG_STAT("foo") << 1;
    LOG_STAT("foo") << "x";
    LOG_STAT("bar") << 19.7;
    LOG_STAT("foo") << 30.5 << ",y";
    LOG_STAT("foobar") << 2000;

    BOOST_TEST(boost::filesystem::exists("stats/"));
    BOOST_TEST(boost::filesystem::exists("stats/foo.csv"));
    BOOST_TEST(std::regex_match(
        file_contents_to_str("stats/foo.csv"),
        std::regex(
            anything_regex + "foo," + timestamp_regex + ",1\n" +
            "foo," + timestamp_regex + ",x\n" +
            "foo," + timestamp_regex + ",30.5,y\n$"
        ))
    );

    BOOST_TEST(boost::filesystem::exists("stats/bar.csv"));
    BOOST_TEST(std::regex_match(
        file_contents_to_str("stats/bar.csv"),
        std::regex(
            anything_regex + "bar," + timestamp_regex + ",19.7\n$"
        ))
    );

    BOOST_TEST(boost::filesystem::exists("stats/foobar.csv"));
    BOOST_TEST(std::regex_match(
        file_contents_to_str("stats/foobar.csv"),
        std::regex(
            anything_regex + "foobar," + timestamp_regex + ",2000\n$"
        ))
    );
}
#endif