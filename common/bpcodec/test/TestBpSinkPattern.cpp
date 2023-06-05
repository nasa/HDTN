/**
 * @file TestBpSinkPattern.cpp
 *
 * @copyright Copyright © 2021 United States Government as represented by
 * the National Aeronautics and Space Administration.
 * No copyright is claimed in the United States under Title 17, U.S.Code.
 * All Other Rights Reserved.
 *
 * @section LICENSE
 * Released under the NASA Open Source Agreement (NOSA)
 * See LICENSE.md in the source root directory for more information.
 */

#include <boost/log/core.hpp>
#include <boost/test/unit_test.hpp>
#include <boost/regex.hpp>

#include "app_patterns/BpSinkPattern.h"
#include "codec/bpv6.h"
#include "codec/bpv7.h"
#include "StatsLogger.h"

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

class BpSinkPatternMockChild : public BpSinkPattern
{
    public:
        void LogStats(PrimaryBlock& primaryBlock, bool isBpVersion6);
    private:
        bool ProcessPayload(const uint8_t * data, const uint64_t size) override;
};

bool BpSinkPatternMockChild::ProcessPayload(const uint8_t * data, const uint64_t size) {
    return true;
}

void BpSinkPatternMockChild::LogStats(PrimaryBlock& primaryBlock, bool isBpVersion6) {
    BpSinkPattern::LogStats(primaryBlock, isBpVersion6);
}

BOOST_AUTO_TEST_CASE(BpSinkPatterLogStatsV6TestCase)
{
    boost::filesystem::remove_all("stats/bundle_stats");
    hdtn::StatsLogger::Reset();

    Bpv6CbhePrimaryBlock primaryBlock;
    primaryBlock.SetZero();

    BpSinkPatternMockChild pattern;
    pattern.LogStats(primaryBlock, true);

    // Before asserting, ensure all stats are flushed to disk
    boost::log::core::get()->flush();

    BOOST_TEST(boost::filesystem::exists("stats/"));
    BOOST_TEST(boost::filesystem::exists("stats/bundle_stats"));
    std::string fileName = findFirstEntry("stats/bundle_stats");
    BOOST_TEST(boost::filesystem::exists(fileName));
    BOOST_TEST(boost::regex_match(
        file_contents_to_str(fileName),
        boost::regex(
            "^timestamp\\(ms\\),priority,expiration_ms,destination_node_id,destination_service_id,source_node_id,source_service_id,bundle_source_to_sink_latency_s,lifetime_seconds,creation_seconds_since_2000\n" +
            timestamp_regex + ",0,0,0,0,0,0,\\d+,0,0\n"
        )
    ));
}

BOOST_AUTO_TEST_CASE(BpSinkPatterLogStatsV7TestCase)
{
    boost::filesystem::remove_all("stats/bundle_stats");
    hdtn::StatsLogger::Reset();

    Bpv7CbhePrimaryBlock primaryBlock;
    primaryBlock.SetZero();

    BpSinkPatternMockChild pattern;
    pattern.LogStats(primaryBlock, false);

    // Before asserting, ensure all stats are flushed to disk
    boost::log::core::get()->flush();

    BOOST_TEST(boost::filesystem::exists("stats/"));
    BOOST_TEST(boost::filesystem::exists("stats/bundle_stats"));
    std::string fileName = findFirstEntry("stats/bundle_stats");
    BOOST_TEST(boost::filesystem::exists(fileName));
    BOOST_TEST(boost::regex_match(
        file_contents_to_str(fileName),
        boost::regex(
            "^timestamp\\(ms\\),priority,expiration_ms,destination_node_id,destination_service_id,source_node_id,source_service_id,bundle_source_to_sink_latency_ms,lifetime_ms,creation_ms_since_2000\n" +
            timestamp_regex + ",2,0,0,0,0,0,\\d+,0,0\n"
        )
    ));
}
