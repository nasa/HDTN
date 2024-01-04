#include <boost/test/unit_test.hpp>
#include <boost/log/core.hpp>
#include <boost/filesystem.hpp>
#include <boost/regex.hpp>
#include <boost/thread.hpp>

#include "TelemetryLogger.h"
#include "StatsLogger.h"

   
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

static const std::string timestamp_regex = "\\d+";
static const std::string header_regex = "^timestamp\\(ms\\),value\n";

#ifdef DO_STATS_LOGGING
BOOST_AUTO_TEST_CASE(TelemetryLoggerLogTelemetryTestCase) 
{
    hdtn::StatsLogger::Reset();
    if (boost::filesystem::exists("stats/all_sampled_stats")) {
        boost::filesystem::remove_all("stats/all_sampled_stats");
    }

    TelemetryLogger logger;
    AllInductTelemetry_t inductTelem;
    inductTelem.m_bundleByteCountEgress = 100000;
    inductTelem.m_bundleByteCountStorage = 250000;

    StorageTelemetry_t storageTelem;
    storageTelem.m_usedSpaceBytes = 50;
    storageTelem.m_freeSpaceBytes = 50;
    storageTelem.m_numBundleBytesOnDisk = 40;
    storageTelem.m_totalBundlesErasedFromStorageNoCustodyTransfer = 10;
    storageTelem.m_totalBundlesErasedFromStorageWithCustodyTransfer = 20;
    storageTelem.m_totalBundlesRewrittenToStorageFromFailedEgressSend = 35;
    storageTelem.m_totalBundleBytesSentToEgressFromStorageForwardCutThrough = 19;
    storageTelem.m_totalBundleBytesSentToEgressFromStorageReadFromDisk = 21;

    AllOutductTelemetry_t outductTelem;
    outductTelem.m_totalBundleBytesSuccessfullySent = 13;
    outductTelem.m_totalBundleBytesGivenToOutducts = 180000;

    logger.LogTelemetry(inductTelem, outductTelem, storageTelem);

    // Ensure stats are flushed
    boost::log::core::get()->flush();

    BOOST_TEST(boost::filesystem::exists("stats/all_sampled_stats"));
    std::string fileName = findFirstEntry("stats/all_sampled_stats");
    BOOST_TEST(boost::regex_match(
        file_contents_to_str(fileName),
        boost::regex(
            "^timestamp\\(ms\\),ingress_data_rate_mbps,ingress_total_bytes_sent,ingress_bytes_sent_egress,ingress_bytes_sent_storage,storage_used_space_bytes,storage_free_space_bytes,storage_bundle_bytes_on_disk,storage_bundles_erased,storage_bundles_rewritten_from_failed_egress_send,storage_bytes_sent_to_egress_cutthrough,storage_bytes_sent_to_egress_from_disk,egress_data_rate_mbps,egress_total_bytes_sent_success,egress_total_bytes_attempted\n" +
            timestamp_regex + ",0.00,350000,100000,250000,50,50,40,30,35,19,21,0.00,13,180000\n"
        )
    ));
}
#endif

BOOST_AUTO_TEST_CASE(TelemetryLoggerCalculateMbpsRateTestCase)
{
    boost::posix_time::ptime nowTime = boost::posix_time::microsec_clock::universal_time();
    boost::posix_time::time_duration oneSec = boost::posix_time::time_duration(0, 0, 1, 0);
    boost::posix_time::ptime lastTime = nowTime - oneSec;

    double currentBytes = 1000000;
    double prevBytes = 0;
    double mbpsVal = TelemetryLogger::CalculateMbpsRate(currentBytes, prevBytes, nowTime, lastTime);
    BOOST_REQUIRE_EQUAL(8, mbpsVal);

    currentBytes = 3000000;
    prevBytes = 1000000;
    mbpsVal = TelemetryLogger::CalculateMbpsRate(currentBytes, prevBytes, nowTime, lastTime);
    BOOST_REQUIRE_EQUAL(16, mbpsVal);

    lastTime = nowTime - oneSec - oneSec;
    mbpsVal = TelemetryLogger::CalculateMbpsRate(currentBytes, prevBytes, nowTime, lastTime);
    BOOST_REQUIRE_EQUAL(8, mbpsVal);
}
