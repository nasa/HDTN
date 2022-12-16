#include <boost/test/unit_test.hpp>

#include "Metrics.h"

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
