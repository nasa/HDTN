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
#include <boost/filesystem.hpp>
#include <boost/test/unit_test.hpp>
#include <boost/regex.hpp>
#include "StatsLogger.h"

static const std::string timestamp_regex = "\\d+";
static const std::string header_regex = "^timestamp\\(ms\\),value\n";

/**
 * Reads a file's contents into a string and returns it
 */
static std::string file_contents_to_str(std::string path) {
    std::ifstream in(path);
    std::stringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

/**
 * Finds the first entry in a directory and returns its path 
 */
static std::string findFirstEntry(std::string inputDir) {
    for (boost::filesystem::directory_entry& entry : boost::filesystem::directory_iterator(inputDir)) {
        return entry.path().string();
    }
    return "";
}

#ifdef DO_STATS_LOGGING
BOOST_AUTO_TEST_CASE(StatsLoggerLogMetrics)
{
    // Start with a clean stats directory
    boost::filesystem::remove_all("stats");

    LOG_STAT("foo") << 1;
    LOG_STAT("foo") << "x";
    LOG_STAT("bar") << 19.7;
    LOG_STAT("foo") << 30.5 << ",y";
    LOG_STAT("foobar") << 2000;

    // Before asserting, ensure all stats are flushed to disk
    boost::log::core::get()->flush();

    BOOST_TEST(boost::filesystem::exists("stats/"));
    BOOST_TEST(boost::filesystem::exists("stats/foo"));
    std::string fileName = findFirstEntry("stats/foo");
    BOOST_TEST(boost::filesystem::exists(fileName));
    BOOST_TEST(boost::regex_match(
        file_contents_to_str(fileName),
        boost::regex(
            header_regex +
            timestamp_regex + ",1\n" +
            timestamp_regex + ",x\n" +
            timestamp_regex + ",30.5,y\n$"
        ))
    );

    BOOST_TEST(boost::filesystem::exists("stats/bar"));
    fileName = findFirstEntry("stats/bar");
    BOOST_TEST(boost::filesystem::exists(fileName));
    BOOST_TEST(boost::regex_match(
        file_contents_to_str(fileName),
        boost::regex(
            header_regex +
            timestamp_regex + ",19.7\n$"
        ))
    );

    BOOST_TEST(boost::filesystem::exists("stats/foobar"));
    fileName = findFirstEntry("stats/foobar")    ;
    BOOST_TEST(boost::filesystem::exists(fileName));
    BOOST_TEST(boost::regex_match(
        file_contents_to_str(fileName),
        boost::regex(
            header_regex +
            timestamp_regex + ",2000\n$"
        ))
    );
}
#endif