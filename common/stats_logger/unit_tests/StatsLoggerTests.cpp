/**
 * @file StatsLoggerTests.cpp
 * @author  Ethan Schweinsberg <ethan.e.schweinsberg@nasa.gov>
 *
 * @copyright Copyright (c) 2021 United States Government as represented by
 * the National Aeronautics and Space Administration.
 * No copyright is claimed in the United States under Title 17, U.S.Code.
 * All Other Rights Reserved.
 *
 * @section LICENSE
 * Released under the NASA Open Source Agreement (NOSA)
 * See LICENSE.md in the source root directory for more information.
 */

#include "StatsLogger.h"
#include <fstream>
#include <boost/version.hpp>
#if (BOOST_VERSION >= 107200)
#include <boost/filesystem/directory.hpp>
#endif
#include <boost/filesystem/operations.hpp>
#include <boost/test/unit_test.hpp>
#include <boost/regex.hpp>


#ifdef DO_STATS_LOGGING
static const std::string timestamp_regex = "\\d+";

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


BOOST_AUTO_TEST_CASE(StatsLoggerLogMetrics)
{
    hdtn::StatsLogger::Reset();
    // Start with a clean stats directory
    if (boost::filesystem::exists("stats/foo")) {
        boost::filesystem::remove_all("stats/foo");
    }

    std::vector<hdtn::StatsLogger::metric_t> metrics;
    metrics.push_back(hdtn::StatsLogger::metric_t("foo", double(1)));
    metrics.push_back(hdtn::StatsLogger::metric_t("bar", 19.50));
    metrics.push_back(hdtn::StatsLogger::metric_t("foobar", uint64_t(2000)));

    hdtn::StatsLogger::Log("foo", metrics);

    // Before asserting, ensure all stats are flushed to disk
    boost::log::core::get()->flush();

    BOOST_TEST(boost::filesystem::exists("stats/"));
    BOOST_TEST(boost::filesystem::exists("stats/foo"));
    std::string fileName = findFirstEntry("stats/foo");
    BOOST_TEST(boost::filesystem::exists(fileName));
    BOOST_TEST(boost::regex_match(
        file_contents_to_str(fileName),
        boost::regex(
            "^timestamp\\(ms\\),foo,bar,foobar\n" +
            timestamp_regex + ",1.00,19.50,2000\n"
        )
    ));


    hdtn::StatsLogger::Log("bar", metrics);
    hdtn::StatsLogger::Log("bar", metrics);

    // Before asserting, ensure all stats are flushed to disk
    boost::log::core::get()->flush();

    BOOST_TEST(boost::filesystem::exists("stats/"));
    BOOST_TEST(boost::filesystem::exists("stats/bar"));
    fileName = findFirstEntry("stats/bar");
    BOOST_TEST(boost::filesystem::exists(fileName));
    BOOST_TEST(boost::regex_match(
        file_contents_to_str(fileName),
        boost::regex(
            "^timestamp\\(ms\\),foo,bar,foobar\n" +
            timestamp_regex + ",1.00,19.50,2000\n" +
            timestamp_regex + ",1.00,19.50,2000\n"
        )
    ));
}
#endif