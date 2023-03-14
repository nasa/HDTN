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
        rng.SetEngineIndex(5);
        BOOST_REQUIRE_EQUAL(rng.GetInternalRngRef().GetReseedAdditionalEntropyCount(), 0);
        BOOST_REQUIRE_EQUAL(rng.GetInternalRngRef().GetReseedPrngCount(), 0);
        for (uint64_t i = 1; i <= 65535; ++i) {
            uint64_t randomNumber = rng.GetRandomSession64();
            BOOST_REQUIRE(randomNumber > 0);
            BOOST_REQUIRE_EQUAL(randomNumber & 0xffffffu, i); //24-bits of incremental part
            BOOST_REQUIRE_EQUAL(rng.GetInternalBirthdayParadoxRef(), i + 1); //points to next value
            BOOST_REQUIRE_EQUAL((randomNumber >> 60) & 1U, 0); //bit 60 set to 0 to leave room for incrementing without rolling into the engineIndex
            BOOST_REQUIRE_EQUAL(static_cast<unsigned int>(LtpRandomNumberGenerator::GetEngineIndexFromRandomSessionNumber(randomNumber)), 5);
        }
        BOOST_REQUIRE_EQUAL(rng.GetInternalRngRef().GetReseedAdditionalEntropyCount(), 256);
        BOOST_REQUIRE_EQUAL(rng.GetInternalRngRef().GetReseedPrngCount(), 255);
        rng.GetInternalBirthdayParadoxRef() = 16777215 - 10; //advance
        for (uint64_t i = 16777215 - 10; i <= 16777215; ++i) { //incremental part should have rolled around at this point
            uint64_t randomNumber = rng.GetRandomSession64();
            BOOST_REQUIRE(randomNumber > 0);
            BOOST_REQUIRE_EQUAL(randomNumber & 0xffffffu, i); //24-bits of incremental part
            BOOST_REQUIRE_EQUAL(rng.GetInternalBirthdayParadoxRef(), (i == 16777215) ? 1 : (i + 1)); //points to next value
            BOOST_REQUIRE_EQUAL((randomNumber >> 60) & 1U, 0); //bit 60 set to 0 to leave room for incrementing without rolling into the engineIndex
            BOOST_REQUIRE_EQUAL(static_cast<unsigned int>(LtpRandomNumberGenerator::GetEngineIndexFromRandomSessionNumber(randomNumber)), 5);
        }
        for (uint64_t i = 1; i <= 10; ++i) { //incremental part should have rolled around at this point
            uint64_t randomNumber = rng.GetRandomSession64();
            BOOST_REQUIRE(randomNumber > 0);
            BOOST_REQUIRE_EQUAL(randomNumber & 0xffffffu, i); //24-bits of incremental part
            BOOST_REQUIRE_EQUAL(rng.GetInternalBirthdayParadoxRef(), i + 1); //points to next value
            BOOST_REQUIRE_EQUAL((randomNumber >> 60) & 1U, 0); //bit 60 set to 0 to leave room for incrementing without rolling into the engineIndex
            BOOST_REQUIRE_EQUAL(static_cast<unsigned int>(LtpRandomNumberGenerator::GetEngineIndexFromRandomSessionNumber(randomNumber)), 5);
        }
        BOOST_REQUIRE_EQUAL(rng.GetInternalRngRef().GetReseedAdditionalEntropyCount(), 256);
        BOOST_REQUIRE_EQUAL(rng.GetInternalRngRef().GetReseedPrngCount(), 256);
    }

    
    {
        LtpRandomNumberGenerator rng;
        BOOST_REQUIRE_EQUAL(rng.GetInternalRngRef().GetReseedAdditionalEntropyCount(), 0);
        BOOST_REQUIRE_EQUAL(rng.GetInternalRngRef().GetReseedPrngCount(), 0);
        for (uint64_t i = 1; i <= 65535; ++i) {
            uint64_t randomNumber = rng.GetRandomSerialNumber64();
            BOOST_REQUIRE(randomNumber > 0);
            BOOST_REQUIRE_EQUAL(randomNumber & 0xffffu, 1);
            BOOST_REQUIRE_EQUAL(randomNumber >> 63, 0);
        }
        BOOST_REQUIRE_EQUAL(rng.GetInternalRngRef().GetReseedAdditionalEntropyCount(), 256);
        BOOST_REQUIRE_EQUAL(rng.GetInternalRngRef().GetReseedPrngCount(), 255);
        for (uint64_t i = 1; i <= 10; ++i) { //incremental part should have rolled around at this point
            uint64_t randomNumber = rng.GetRandomSerialNumber64();
            BOOST_REQUIRE(randomNumber > 0);
            BOOST_REQUIRE_EQUAL(randomNumber & 0xffffu, 1);
            BOOST_REQUIRE_EQUAL(randomNumber >> 63, 0);
        }
        BOOST_REQUIRE_EQUAL(rng.GetInternalRngRef().GetReseedAdditionalEntropyCount(), 256);
        BOOST_REQUIRE_EQUAL(rng.GetInternalRngRef().GetReseedPrngCount(), 256);
    }



    {
        LtpRandomNumberGenerator rng;
        rng.SetEngineIndex(7);
        BOOST_REQUIRE_EQUAL(rng.GetInternalRngRef().GetReseedAdditionalEntropyCount(), 0);
        BOOST_REQUIRE_EQUAL(rng.GetInternalRngRef().GetReseedPrngCount(), 0);
        for (uint64_t i = 1; i <= 65535; ++i) {
            uint32_t randomNumber = rng.GetRandomSession32();
            BOOST_REQUIRE(randomNumber > 0);
            BOOST_REQUIRE_EQUAL(randomNumber & 0x1fffffu, i); //21-bits of incremental part
            BOOST_REQUIRE_EQUAL(rng.GetInternalBirthdayParadoxRef(), i + 1); //points to next value
            BOOST_REQUIRE_EQUAL((randomNumber >> 28) & 1U, 0); //bit 28 set to 0 to leave room for incrementing without rolling into the engineIndex
            BOOST_REQUIRE_EQUAL(static_cast<unsigned int>(LtpRandomNumberGenerator::GetEngineIndexFromRandomSessionNumber(randomNumber)), 7);
        }
        BOOST_REQUIRE_EQUAL(rng.GetInternalRngRef().GetReseedAdditionalEntropyCount(), 256);
        BOOST_REQUIRE_EQUAL(rng.GetInternalRngRef().GetReseedPrngCount(), 255);
        rng.GetInternalBirthdayParadoxRef() = 2097151 - 10; //advance
        for (uint64_t i = 2097151 - 10; i <= 2097151; ++i) { //incremental part should have rolled around at this point
            uint32_t randomNumber = rng.GetRandomSession32();
            BOOST_REQUIRE(randomNumber > 0);
            BOOST_REQUIRE_EQUAL(randomNumber & 0x1fffffu, i); //21-bits of incremental part
            BOOST_REQUIRE_EQUAL(rng.GetInternalBirthdayParadoxRef(), (i == 2097151) ? 1 : (i + 1)); //points to next value
            BOOST_REQUIRE_EQUAL((randomNumber >> 28) & 1U, 0); //bit 28 set to 0 to leave room for incrementing without rolling into the engineIndex
            BOOST_REQUIRE_EQUAL(static_cast<unsigned int>(LtpRandomNumberGenerator::GetEngineIndexFromRandomSessionNumber(randomNumber)), 7);
        }
        for (uint64_t i = 1; i <= 10; ++i) { //incremental part should have rolled around at this point
            uint32_t randomNumber = rng.GetRandomSession32();
            BOOST_REQUIRE(randomNumber > 0);
            BOOST_REQUIRE_EQUAL(randomNumber & 0x1fffffu, i); //21-bits of incremental part
            BOOST_REQUIRE_EQUAL(rng.GetInternalBirthdayParadoxRef(), i + 1); //points to next value
            BOOST_REQUIRE_EQUAL((randomNumber >> 28) & 1U, 0); //bit 28 set to 0 to leave room for incrementing without rolling into the engineIndex
            BOOST_REQUIRE_EQUAL(static_cast<unsigned int>(LtpRandomNumberGenerator::GetEngineIndexFromRandomSessionNumber(randomNumber)), 7);
        }
        BOOST_REQUIRE_EQUAL(rng.GetInternalRngRef().GetReseedAdditionalEntropyCount(), 256);
        BOOST_REQUIRE_EQUAL(rng.GetInternalRngRef().GetReseedPrngCount(), 256);
    }


    {
        LtpRandomNumberGenerator rng;
        BOOST_REQUIRE_EQUAL(rng.GetInternalRngRef().GetReseedAdditionalEntropyCount(), 0);
        BOOST_REQUIRE_EQUAL(rng.GetInternalRngRef().GetReseedPrngCount(), 0);
        for (uint64_t i = 1; i <= 65535; ++i) {
            uint32_t randomNumber = rng.GetRandomSerialNumber32();
            BOOST_REQUIRE(randomNumber > 0);
            BOOST_REQUIRE_EQUAL(randomNumber & 0xffffu, 1);
            BOOST_REQUIRE_EQUAL(randomNumber >> 31, 0);
        }
        BOOST_REQUIRE_EQUAL(rng.GetInternalRngRef().GetReseedAdditionalEntropyCount(), 256);
        BOOST_REQUIRE_EQUAL(rng.GetInternalRngRef().GetReseedPrngCount(), 255);
        for (uint64_t i = 1; i <= 10; ++i) { //incremental part should have rolled around at this point
            uint32_t randomNumber = rng.GetRandomSerialNumber32();
            BOOST_REQUIRE(randomNumber > 0);
            BOOST_REQUIRE_EQUAL(randomNumber & 0xffffu, 1);
            BOOST_REQUIRE_EQUAL(randomNumber >> 31, 0);
        }
        BOOST_REQUIRE_EQUAL(rng.GetInternalRngRef().GetReseedAdditionalEntropyCount(), 256);
        BOOST_REQUIRE_EQUAL(rng.GetInternalRngRef().GetReseedPrngCount(), 256);
    }


    //test ping
    {
        for (uint8_t i = 1; i <= 7; ++i) {
            LtpRandomNumberGenerator rng;
            rng.SetEngineIndex(i);
            uint32_t randomNumber = rng.GetPingSession32();
            BOOST_REQUIRE(rng.IsPingSession(randomNumber, true));
            BOOST_REQUIRE(!rng.IsPingSession(randomNumber, false));
            const uint64_t engineIndex = rng.GetEngineIndexFromRandomSessionNumber(randomNumber);
            BOOST_REQUIRE_EQUAL(engineIndex, i);
        }
        for (uint8_t i = 1; i <= 7; ++i) {
            LtpRandomNumberGenerator rng;
            rng.SetEngineIndex(i);
            uint64_t randomNumber = rng.GetPingSession64();
            BOOST_REQUIRE(rng.IsPingSession(randomNumber, false));
            const uint64_t engineIndex = rng.GetEngineIndexFromRandomSessionNumber(randomNumber);
            BOOST_REQUIRE_EQUAL(engineIndex, i);
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
