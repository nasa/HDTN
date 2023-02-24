#include <boost/test/unit_test.hpp>
#include <boost/log/core.hpp>
#include <boost/filesystem.hpp>
#include <boost/regex.hpp>
#include <boost/thread.hpp>

#include "TelemetryLogger.h"

   
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
BOOST_AUTO_TEST_CASE(TelemetryLoggerLogTelemetryIngressTestCase) 
{
    boost::filesystem::remove_all("stats/ingress_data_rate_mbps");
    boost::filesystem::remove_all("stats/ingress_data_volume_bytes");

    TelemetryLogger logger;
    IngressTelemetry_t telem;
    telem.totalDataBytes = 1000;
    Telemetry_t* t = &telem;
    logger.LogTelemetry(t);

    boost::this_thread::sleep(boost::posix_time::milliseconds(200));
    telem.totalDataBytes = 4000;
    logger.LogTelemetry(t);

    // Ensure stats are flushed
    boost::log::core::get()->flush();

    BOOST_TEST(boost::filesystem::exists("stats/ingress_data_volume_bytes"));
    std::string fileName = findFirstEntry("stats/ingress_data_volume_bytes");
    BOOST_TEST(boost::regex_match(
        file_contents_to_str(fileName),
        boost::regex(
            header_regex +
            timestamp_regex + ",1000\n" +
            timestamp_regex + ",4000\n$"
        )
    ));
    BOOST_TEST(boost::filesystem::exists("stats/ingress_data_rate_mbps"));
    fileName = findFirstEntry("stats/ingress_data_rate_mbps");
    BOOST_TEST(boost::regex_match(
        file_contents_to_str(fileName),
        boost::regex(
            header_regex +
            timestamp_regex + ",0.\\d+\n$"
        )
    ));
}
#endif

#ifdef DO_STATS_LOGGING
BOOST_AUTO_TEST_CASE(TelemetryLoggerLogTelemetryEgressTestCase) 
{
    boost::filesystem::remove_all("stats/egress_data_rate_mbps");
    boost::filesystem::remove_all("stats/egress_data_volume_bytes");

    TelemetryLogger logger;
    EgressTelemetry_t telem;
    telem.totalDataBytes = 1000;
    Telemetry_t* t = &telem;
    logger.LogTelemetry(t);

    boost::this_thread::sleep(boost::posix_time::milliseconds(200));
    telem.totalDataBytes = 4000;
    logger.LogTelemetry(t);

    // Ensure stats are flushed
    boost::log::core::get()->flush();

    BOOST_TEST(boost::filesystem::exists("stats/egress_data_volume_bytes"));
    std::string fileName = findFirstEntry("stats/egress_data_volume_bytes");
    BOOST_TEST(boost::regex_match(
        file_contents_to_str(fileName),
        boost::regex(
            header_regex +
            timestamp_regex + ",1000\n" +
            timestamp_regex + ",4000\n$"
        )
    ));
    BOOST_TEST(boost::filesystem::exists("stats/egress_data_rate_mbps"));
    fileName = findFirstEntry("stats/egress_data_rate_mbps");
    BOOST_TEST(boost::regex_match(
        file_contents_to_str(fileName),
        boost::regex(
            header_regex +
            timestamp_regex + ",0.1\\d+\n$"
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
