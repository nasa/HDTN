#include <boost/test/unit_test.hpp>
#include <boost/timer/timer.hpp>

#include "DeadlineTimer.h"

BOOST_AUTO_TEST_CASE(DeadlineTimerTestCase)
{
    DeadlineTimer deadlineTimer(100);

    // The deadline timer should sleep for the
    // specified amount of time
    boost::timer::cpu_timer cpuTimer;
    bool success = deadlineTimer.Sleep();
    cpuTimer.stop();
    BOOST_REQUIRE_EQUAL(true, success);
    BOOST_REQUIRE_GE(cpuTimer.elapsed().wall, 100000000 /*100 ms*/);

    // The deadline timer should not block if the sleep
    // duration has already passed
    usleep(100000 /*100 ms*/);
    cpuTimer.start();
    deadlineTimer.Sleep();
    cpuTimer.stop();
    BOOST_REQUIRE_LE(cpuTimer.elapsed().wall, 10000000 /*10 ms*/);
}
