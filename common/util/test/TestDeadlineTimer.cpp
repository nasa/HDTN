#include <boost/test/unit_test.hpp>
#include <boost/timer/timer.hpp>
#include <boost/thread.hpp>

#include "DeadlineTimer.h"

BOOST_AUTO_TEST_CASE(DeadlineTimerTestCase)
{
    // The deadline timer should sleep for the
    // specified amount of time
    boost::timer::cpu_timer cpuTimer;
    DeadlineTimer deadlineTimer(100);
    bool success = deadlineTimer.SleepUntilNextInterval();
    cpuTimer.stop();
    BOOST_REQUIRE_EQUAL(true, success);
    BOOST_REQUIRE_GE(cpuTimer.elapsed().wall, 100000000 /*100 ms*/);

    // The deadline timer should not block if the sleep
    // duration has already passed
    boost::this_thread::sleep(boost::posix_time::milliseconds(100));
    cpuTimer.start();
    deadlineTimer.SleepUntilNextInterval();
    cpuTimer.stop();
    BOOST_REQUIRE_LE(cpuTimer.elapsed().wall, 10000000 /*10 ms*/);
}
