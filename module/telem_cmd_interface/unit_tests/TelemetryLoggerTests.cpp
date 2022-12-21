#include <boost/test/unit_test.hpp>
#include <boost/log/core.hpp>
#include <boost/filesystem.hpp>

#include "TelemetryLogger.h"
#include "Metrics.h"

#ifdef DO_STATS_LOGGING
BOOST_AUTO_TEST_CASE(TelemetryLoggerLogMetricsTestCase) 
{
    boost::filesystem::remove_all("stats/egress_data_rate_mbps");
    boost::filesystem::remove_all("stats/egress_data_volume_bytes");
    boost::filesystem::remove_all("stats/ingress_data_rate_mbps");
    boost::filesystem::remove_all("stats/ingress_data_volume_bytes");

    TelemetryLogger logger;
    Metrics::metrics_t metrics;
    logger.LogMetrics(metrics);

    // Ensure stats are flushed
    boost::log::core::get()->flush();

    BOOST_TEST(boost::filesystem::exists("stats/egress_data_rate_mbps"));
    BOOST_TEST(boost::filesystem::exists("stats/egress_data_volume_bytes"));
    BOOST_TEST(boost::filesystem::exists("stats/ingress_data_volume_bytes"));
    BOOST_TEST(boost::filesystem::exists("stats/ingress_data_rate_mbps"));
}
#endif