/**
 * @file TestLtpRandomNumberGenerator.cpp
 * @author  Brian Tomko <brian.j.tomko@nasa.gov>
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

#include <boost/test/unit_test.hpp>
#include "LtpRandomNumberGenerator.h"
#include <boost/bind/bind.hpp>
#include <boost/timer/timer.hpp>

BOOST_AUTO_TEST_CASE(LtpRandomNumberGeneratorTestCase)
{
    {
        LtpRandomNumberGenerator rng;
        rng.SetEngineIndex(131);
        for (uint64_t i = 1; i <= 65535; ++i) {
            uint64_t randomNumber = rng.GetRandomSession64();
            BOOST_REQUIRE(randomNumber > 0);
            BOOST_REQUIRE_EQUAL(randomNumber & 0xffffu, i);
            BOOST_REQUIRE_EQUAL((randomNumber >> 55) & 1U, 0); //bit 55 set to 0 to leave room for incrementing without rolling into the engineIndex
            BOOST_REQUIRE_EQUAL(static_cast<unsigned int>(LtpRandomNumberGenerator::GetEngineIndexFromRandomSessionNumber(randomNumber)), 131);
        }
        for (uint64_t i = 1; i <= 10; ++i) { //incremental part should have rolled around at this point
            uint64_t randomNumber = rng.GetRandomSession64();
            BOOST_REQUIRE(randomNumber > 0);
            BOOST_REQUIRE_EQUAL(randomNumber & 0xffffu, i);
            BOOST_REQUIRE_EQUAL((randomNumber >> 55) & 1U, 0); //bit 55 set to 0 to leave room for incrementing without rolling into the engineIndex
            BOOST_REQUIRE_EQUAL(static_cast<unsigned int>(LtpRandomNumberGenerator::GetEngineIndexFromRandomSessionNumber(randomNumber)), 131);
        }
    }

    //test with additional randomness
    {
        LtpRandomNumberGenerator rng;
        rng.SetEngineIndex(131);
        boost::random_device rd;
        for (uint64_t i = 1; i <= 65535; ++i) {
            uint64_t randomNumber = rng.GetRandomSession64(rd);
            BOOST_REQUIRE(randomNumber > 0);
            BOOST_REQUIRE_EQUAL(randomNumber & 0xffffu, i);
            BOOST_REQUIRE_EQUAL((randomNumber >> 55) & 1U, 0); //bit 55 set to 0 to leave room for incrementing without rolling into the engineIndex
            BOOST_REQUIRE_EQUAL(static_cast<unsigned int>(LtpRandomNumberGenerator::GetEngineIndexFromRandomSessionNumber(randomNumber)), 131);
        }
        for (uint64_t i = 1; i <= 10; ++i) { //incremental part should have rolled around at this point
            uint64_t randomNumber = rng.GetRandomSession64(rd);
            BOOST_REQUIRE(randomNumber > 0);
            BOOST_REQUIRE_EQUAL(randomNumber & 0xffffu, i);
            BOOST_REQUIRE_EQUAL((randomNumber >> 55) & 1U, 0); //bit 55 set to 0 to leave room for incrementing without rolling into the engineIndex
            BOOST_REQUIRE_EQUAL(static_cast<unsigned int>(LtpRandomNumberGenerator::GetEngineIndexFromRandomSessionNumber(randomNumber)), 131);
        }
    }



    {
        LtpRandomNumberGenerator rng;
        for (uint64_t i = 1; i <= 65535; ++i) {
            uint64_t randomNumber = rng.GetRandomSerialNumber64();
            BOOST_REQUIRE(randomNumber > 0);
            BOOST_REQUIRE_EQUAL(randomNumber & 0xffffu, 1);
            BOOST_REQUIRE_EQUAL(randomNumber >> 63, 0);
        }
        for (uint64_t i = 1; i <= 10; ++i) { //incremental part should have rolled around at this point
            uint64_t randomNumber = rng.GetRandomSerialNumber64();
            BOOST_REQUIRE(randomNumber > 0);
            BOOST_REQUIRE_EQUAL(randomNumber & 0xffffu, 1);
            BOOST_REQUIRE_EQUAL(randomNumber >> 63, 0);
        }
    }

    //test with additional randomness
    {
        LtpRandomNumberGenerator rng;
        boost::random_device rd;
        for (uint64_t i = 1; i <= 65535; ++i) {
            uint64_t randomNumber = rng.GetRandomSerialNumber64(rd);
            BOOST_REQUIRE(randomNumber > 0);
            BOOST_REQUIRE_EQUAL(randomNumber & 0xffffu, 1);
            BOOST_REQUIRE_EQUAL(randomNumber >> 63, 0);
        }
        for (uint64_t i = 1; i <= 10; ++i) { //incremental part should have rolled around at this point
            uint64_t randomNumber = rng.GetRandomSerialNumber64(rd);
            BOOST_REQUIRE(randomNumber > 0);
            BOOST_REQUIRE_EQUAL(randomNumber & 0xffffu, 1);
            BOOST_REQUIRE_EQUAL(randomNumber >> 63, 0);
        }
    }



    {
        LtpRandomNumberGenerator rng;
        rng.SetEngineIndex(131);
        for (uint64_t i = 1; i <= 65535; ++i) {
            uint32_t randomNumber = rng.GetRandomSession32();
            BOOST_REQUIRE(randomNumber > 0);
            BOOST_REQUIRE_EQUAL(randomNumber & 0xffffu, i);
            BOOST_REQUIRE_EQUAL((randomNumber >> 23) & 1U, 0); //bit 23 set to 0 to leave room for incrementing without rolling into the engineIndex
            BOOST_REQUIRE_EQUAL(static_cast<unsigned int>(LtpRandomNumberGenerator::GetEngineIndexFromRandomSessionNumber(randomNumber)), 131);
        }
        for (uint64_t i = 1; i <= 10; ++i) { //incremental part should have rolled around at this point
            uint32_t randomNumber = rng.GetRandomSession32();
            BOOST_REQUIRE(randomNumber > 0);
            BOOST_REQUIRE_EQUAL(randomNumber & 0xffffu, i);
            BOOST_REQUIRE_EQUAL((randomNumber >> 23) & 1U, 0); //bit 23 set to 0 to leave room for incrementing without rolling into the engineIndex
            BOOST_REQUIRE_EQUAL(static_cast<unsigned int>(LtpRandomNumberGenerator::GetEngineIndexFromRandomSessionNumber(randomNumber)), 131);
        }
    }

    //test with additional randomness
    {
        LtpRandomNumberGenerator rng;
        rng.SetEngineIndex(131);
        boost::random_device rd;
        for (uint64_t i = 1; i <= 65535; ++i) {
            uint32_t randomNumber = rng.GetRandomSession32(rd);
            BOOST_REQUIRE(randomNumber > 0);
            BOOST_REQUIRE_EQUAL(randomNumber & 0xffffu, i);
            BOOST_REQUIRE_EQUAL((randomNumber >> 23) & 1U, 0); //bit 23 set to 0 to leave room for incrementing without rolling into the engineIndex
            BOOST_REQUIRE_EQUAL(static_cast<unsigned int>(LtpRandomNumberGenerator::GetEngineIndexFromRandomSessionNumber(randomNumber)), 131);
        }
        for (uint64_t i = 1; i <= 10; ++i) { //incremental part should have rolled around at this point
            uint32_t randomNumber = rng.GetRandomSession32(rd);
            BOOST_REQUIRE(randomNumber > 0);
            BOOST_REQUIRE_EQUAL(randomNumber & 0xffffu, i);
            BOOST_REQUIRE_EQUAL((randomNumber >> 23) & 1U, 0); //bit 23 set to 0 to leave room for incrementing without rolling into the engineIndex
            BOOST_REQUIRE_EQUAL(static_cast<unsigned int>(LtpRandomNumberGenerator::GetEngineIndexFromRandomSessionNumber(randomNumber)), 131);
        }
    }

    {
        LtpRandomNumberGenerator rng;
        for (uint64_t i = 1; i <= 65535; ++i) {
            uint32_t randomNumber = rng.GetRandomSerialNumber32();
            BOOST_REQUIRE(randomNumber > 0);
            BOOST_REQUIRE_EQUAL(randomNumber & 0xffffu, 1);
            BOOST_REQUIRE_EQUAL(randomNumber >> 31, 0);
        }
        for (uint64_t i = 1; i <= 10; ++i) { //incremental part should have rolled around at this point
            uint32_t randomNumber = rng.GetRandomSerialNumber32();
            BOOST_REQUIRE(randomNumber > 0);
            BOOST_REQUIRE_EQUAL(randomNumber & 0xffffu, 1);
            BOOST_REQUIRE_EQUAL(randomNumber >> 31, 0);
        }
    }

    //test with additional randomness
    {
        LtpRandomNumberGenerator rng;
        boost::random_device rd;
        for (uint64_t i = 1; i <= 65535; ++i) {
            uint32_t randomNumber = rng.GetRandomSerialNumber32(rd);
            BOOST_REQUIRE(randomNumber > 0);
            BOOST_REQUIRE_EQUAL(randomNumber & 0xffffu, 1);
            BOOST_REQUIRE_EQUAL(randomNumber >> 31, 0);
        }
        for (uint64_t i = 1; i <= 10; ++i) { //incremental part should have rolled around at this point
            uint32_t randomNumber = rng.GetRandomSerialNumber32(rd);
            BOOST_REQUIRE(randomNumber > 0);
            BOOST_REQUIRE_EQUAL(randomNumber & 0xffffu, 1);
            BOOST_REQUIRE_EQUAL(randomNumber >> 31, 0);
        }
    }

    //test ping
    {
        
        {
            LtpRandomNumberGenerator rng;
            rng.SetEngineIndex(129);
            uint32_t randomNumber = rng.GetPingSession32();
            BOOST_REQUIRE(rng.IsPingSession(randomNumber, true));
            BOOST_REQUIRE(!rng.IsPingSession(randomNumber, false));
            const uint64_t engineIndex = rng.GetEngineIndexFromRandomSessionNumber(randomNumber);
            BOOST_REQUIRE_EQUAL(engineIndex, 129);
        }
        {
            LtpRandomNumberGenerator rng;
            rng.SetEngineIndex(133);
            uint64_t randomNumber = rng.GetPingSession64();
            BOOST_REQUIRE(rng.IsPingSession(randomNumber, false));
            const uint64_t engineIndex = rng.GetEngineIndexFromRandomSessionNumber(randomNumber);
            BOOST_REQUIRE_EQUAL(engineIndex, 133);
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
