#include <boost/test/unit_test.hpp>
#include "LtpRandomNumberGenerator.h"
#include <boost/bind.hpp>
#include <boost/timer/timer.hpp>

BOOST_AUTO_TEST_CASE(LtpRandomNumberGeneratorTestCase)
{
    {
        LtpRandomNumberGenerator rng;
        for (uint64_t i = 1; i <= 65535; ++i) {
            uint64_t randomNumber = rng.GetRandom();
            BOOST_REQUIRE(randomNumber > 0);
            BOOST_REQUIRE_EQUAL(randomNumber & 0xffffu, i);
            BOOST_REQUIRE_EQUAL(randomNumber >> 63, 0);
        }
        for (uint64_t i = 1; i <= 10; ++i) { //incremental part should have rolled around at this point
            uint64_t randomNumber = rng.GetRandom();
            BOOST_REQUIRE(randomNumber > 0);
            BOOST_REQUIRE_EQUAL(randomNumber & 0xffffu, i);
            BOOST_REQUIRE_EQUAL(randomNumber >> 63, 0);
        }
    }

    //test with additional randomness
    {
        LtpRandomNumberGenerator rng;
        boost::random_device rd;
        for (uint64_t i = 1; i <= 65535; ++i) {
            uint64_t randomNumber = rng.GetRandom(rd);
            BOOST_REQUIRE(randomNumber > 0);
            BOOST_REQUIRE_EQUAL(randomNumber & 0xffffu, i);
            BOOST_REQUIRE_EQUAL(randomNumber >> 63, 0);
        }
        for (uint64_t i = 1; i <= 10; ++i) { //incremental part should have rolled around at this point
            uint64_t randomNumber = rng.GetRandom(rd);
            BOOST_REQUIRE(randomNumber > 0);
            BOOST_REQUIRE_EQUAL(randomNumber & 0xffffu, i);
            BOOST_REQUIRE_EQUAL(randomNumber >> 63, 0);
        }
    }
    /*
    {
        volatile uint64_t randomNumber;
        std::cout << "speed random\n";
        boost::timer::auto_cpu_timer t;
        for (unsigned int i = 0; i < 1000000; ++i) {
            randomNumber = rng.GetRandom();
        }
        BOOST_REQUIRE(randomNumber > 0);
    }
    */
}
