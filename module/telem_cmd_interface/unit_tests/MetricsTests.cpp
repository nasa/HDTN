#include <boost/test/unit_test.hpp>
#include <boost/thread.hpp>

#include "Metrics.h"

BOOST_AUTO_TEST_CASE(MetricsStructInitTestCase)
{
    // All struct fields should be initialized to 0
    Metrics::metrics_t metrics;
    char *bytes = (char*)&metrics;
    int sum = 0;
    for (int i=0; i < sizeof(Metrics::metrics_t); ++i) {
        sum += bytes[i];
    }
    BOOST_REQUIRE_EQUAL(0, sum);
}

BOOST_AUTO_TEST_CASE(MetricsClearTestCase)
{
    // Fake telem
    StorageTelemetry_t telem;
    telem.totalBundlesErasedFromStorage = 10;
    telem.totalBundlesSentToEgressFromStorage = 20;

    Metrics metrics;
    metrics.ProcessStorageTelem(telem);
    metrics.Clear();

    Metrics::metrics_t metricsVals = metrics.Get();
    char *bytes = (char*)&metricsVals;
    int sum = 0;
    for (int i=0; i < sizeof(Metrics::metrics_t); ++i) {
        sum += bytes[i];
    }
    BOOST_REQUIRE_EQUAL(0, sum);
}

BOOST_AUTO_TEST_CASE(MetricsProcessIngressTelemTestCase)
{
    IngressTelemetry_t telem;
    telem.totalDataBytes = 1000;
    telem.bundleCountEgress = 5;
    telem.bundleCountStorage = 10;

    Metrics metrics;
    metrics.ProcessIngressTelem(telem);
    Metrics::metrics_t result = metrics.Get();
    BOOST_REQUIRE_EQUAL(5, result.bundleCountSentToEgress);
    BOOST_REQUIRE_EQUAL(10, result.bundleCountSentToStorage);
    BOOST_REQUIRE_EQUAL(0, result.ingressAverageRateMbps);
    BOOST_REQUIRE_EQUAL(0, result.ingressCurrentRateMbps);

    telem.totalDataBytes = 4000;
    telem.bundleCountEgress = 10;
    telem.bundleCountStorage = 20;
    boost::this_thread::sleep(boost::posix_time::milliseconds(200));
    metrics.ProcessIngressTelem(telem);
    result = metrics.Get();
    BOOST_REQUIRE_EQUAL(10, result.bundleCountSentToEgress);
    BOOST_REQUIRE_EQUAL(20, result.bundleCountSentToStorage);
    BOOST_REQUIRE_CLOSE(.16, result.ingressAverageRateMbps, 10);
    BOOST_REQUIRE_CLOSE(.12, result.ingressCurrentRateMbps, 10);
}

BOOST_AUTO_TEST_CASE(MetricsProcessEgressTelemTestCase)
{
    EgressTelemetry_t telem;
    telem.totalDataBytes = 1000;
    telem.egressBundleCount = 5;
    telem.egressMessageCount = 10;

    Metrics metrics;
    metrics.ProcessEgressTelem(telem);
    Metrics::metrics_t result = metrics.Get();
    BOOST_REQUIRE_EQUAL(5, result.egressBundleCount);
    BOOST_REQUIRE_EQUAL(10, result.egressMessageCount);
    BOOST_REQUIRE_EQUAL(0, result.egressAverageRateMbps);
    BOOST_REQUIRE_EQUAL(0, result.egressCurrentRateMbps);

    telem.totalDataBytes = 4000;
    telem.egressBundleCount = 10;
    telem.egressMessageCount = 20;
    boost::this_thread::sleep(boost::posix_time::milliseconds(200));
    metrics.ProcessEgressTelem(telem);
    result = metrics.Get();
    BOOST_REQUIRE_EQUAL(10, result.egressBundleCount);
    BOOST_REQUIRE_EQUAL(20, result.egressMessageCount);
    BOOST_REQUIRE_CLOSE(.16, result.egressAverageRateMbps, 10);
    BOOST_REQUIRE_CLOSE(.12, result.egressCurrentRateMbps, 10);
}

BOOST_AUTO_TEST_CASE(MetricsProcessStorageTelemTestCase)
{
    StorageTelemetry_t telem;
    telem.totalBundlesErasedFromStorage = 11;
    telem.totalBundlesSentToEgressFromStorage = 12;

    Metrics metrics;
    metrics.ProcessStorageTelem(telem);
    Metrics::metrics_t result = metrics.Get();
    BOOST_REQUIRE_EQUAL(11, result.totalBundlesErasedFromStorage);
    BOOST_REQUIRE_EQUAL(12, result.totalBunglesSentFromEgressToStorage);
}

BOOST_AUTO_TEST_CASE(MetricsCalculateMbpsRateTestCase)
{
    boost::posix_time::ptime nowTime = boost::posix_time::microsec_clock::universal_time();
    boost::posix_time::time_duration oneSec = boost::posix_time::time_duration(0, 0, 1, 0);
    boost::posix_time::ptime lastTime = nowTime - oneSec;

    double currentBytes = 1000000;
    double prevBytes = 0;
    double mbpsVal = Metrics::CalculateMbpsRate(currentBytes, prevBytes, nowTime, lastTime);
    BOOST_REQUIRE_EQUAL(8, mbpsVal);

    currentBytes = 3000000;
    prevBytes = 1000000;
    mbpsVal = Metrics::CalculateMbpsRate(currentBytes, prevBytes, nowTime, lastTime);
    BOOST_REQUIRE_EQUAL(16, mbpsVal);

    lastTime = nowTime - oneSec - oneSec;
    mbpsVal = Metrics::CalculateMbpsRate(currentBytes, prevBytes, nowTime, lastTime);
    BOOST_REQUIRE_EQUAL(8, mbpsVal);
}
