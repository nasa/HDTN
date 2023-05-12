/**
 * @file TestInitializationVectors.cpp
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
#include "InitializationVectors.h"
#include <boost/thread.hpp>


BOOST_AUTO_TEST_CASE(InitializationVector12ByteTestCase)
{
    InitializationVector12Byte iv;
    BOOST_REQUIRE_NE(iv.m_timePart, 0);
    BOOST_REQUIRE_EQUAL(iv.m_counterPart, 0);
    {
        boost::this_thread::sleep(boost::posix_time::milliseconds(100));
        InitializationVector12Byte iv2;
        BOOST_REQUIRE_GT(iv2.m_timePart, iv.m_timePart);
        const uint64_t deltaMs = iv2.m_timePart - iv.m_timePart;
        BOOST_REQUIRE_GT(deltaMs, 50);
        BOOST_REQUIRE_LT(deltaMs, 500);
    }

    const std::vector<uint8_t> noBitsSetVec(12, 0);
    const std::vector<uint8_t> allBitsSetVec(12, 0xff);
    std::vector<uint8_t> serialized(allBitsSetVec);
    BOOST_REQUIRE(serialized == allBitsSetVec);
    BOOST_REQUIRE(noBitsSetVec != allBitsSetVec);

    //rollover of counter part into time part
    iv.m_timePart = 1; //1 millisecond
    iv.m_counterPart = UINT32_MAX - 2;
    iv.Serialize(serialized.data());
    BOOST_REQUIRE(serialized != allBitsSetVec);
    iv.Increment();
    BOOST_REQUIRE_EQUAL(iv.m_timePart, 1);
    BOOST_REQUIRE_EQUAL(iv.m_counterPart, UINT32_MAX - 1);
    iv.Increment();
    BOOST_REQUIRE_EQUAL(iv.m_timePart, 1);
    BOOST_REQUIRE_EQUAL(iv.m_counterPart, UINT32_MAX);
    iv.Increment();
    BOOST_REQUIRE_EQUAL(iv.m_timePart, 2); //2 milliseconds
    BOOST_REQUIRE_EQUAL(iv.m_counterPart, 0);
    iv.Increment();
    BOOST_REQUIRE_EQUAL(iv.m_timePart, 2);
    BOOST_REQUIRE_EQUAL(iv.m_counterPart, 1);

    //unrealistic rollover of time part
    iv.m_timePart = UINT64_MAX;
    iv.m_counterPart = UINT32_MAX - 2;
    iv.Increment();
    BOOST_REQUIRE_EQUAL(iv.m_timePart, UINT64_MAX);
    BOOST_REQUIRE_EQUAL(iv.m_counterPart, UINT32_MAX - 1);
    iv.Serialize(serialized.data());
    BOOST_REQUIRE(serialized != allBitsSetVec);
    iv.Increment();
    BOOST_REQUIRE_EQUAL(iv.m_timePart, UINT64_MAX);
    BOOST_REQUIRE_EQUAL(iv.m_counterPart, UINT32_MAX);
    iv.Serialize(serialized.data());
    BOOST_REQUIRE(serialized == allBitsSetVec); //now all bits set
    iv.Increment();
    BOOST_REQUIRE_EQUAL(iv.m_timePart, 0);
    BOOST_REQUIRE_EQUAL(iv.m_counterPart, 0);
    iv.Serialize(serialized.data());
    BOOST_REQUIRE(serialized == noBitsSetVec);
    iv.Increment();
    BOOST_REQUIRE_EQUAL(iv.m_timePart, 0);
    BOOST_REQUIRE_EQUAL(iv.m_counterPart, 1);
}

BOOST_AUTO_TEST_CASE(InitializationVector16ByteTestCase)
{
    InitializationVector16Byte iv;
    BOOST_REQUIRE_NE(iv.m_timePart, 0);
    BOOST_REQUIRE_EQUAL(iv.m_counterPart, 0);
    {
        boost::this_thread::sleep(boost::posix_time::milliseconds(100));
        InitializationVector12Byte iv2;
        BOOST_REQUIRE_GT(iv2.m_timePart, iv.m_timePart);
        const uint64_t deltaMs = iv2.m_timePart - iv.m_timePart;
        BOOST_REQUIRE_GT(deltaMs, 50);
        BOOST_REQUIRE_LT(deltaMs, 500);
    }

    const std::vector<uint8_t> noBitsSetVec(16, 0);
    const std::vector<uint8_t> allBitsSetVec(16, 0xff);
    std::vector<uint8_t> serialized(allBitsSetVec);
    BOOST_REQUIRE(serialized == allBitsSetVec);
    BOOST_REQUIRE(noBitsSetVec != allBitsSetVec);

    //rollover of counter part into time part
    iv.m_timePart = 1; //1 millisecond
    iv.m_counterPart = UINT64_MAX - 2;
    iv.Serialize(serialized.data());
    BOOST_REQUIRE(serialized != allBitsSetVec);
    iv.Increment();
    BOOST_REQUIRE_EQUAL(iv.m_timePart, 1);
    BOOST_REQUIRE_EQUAL(iv.m_counterPart, UINT64_MAX - 1);
    iv.Increment();
    BOOST_REQUIRE_EQUAL(iv.m_timePart, 1);
    BOOST_REQUIRE_EQUAL(iv.m_counterPart, UINT64_MAX);
    iv.Increment();
    BOOST_REQUIRE_EQUAL(iv.m_timePart, 2); //2 milliseconds
    BOOST_REQUIRE_EQUAL(iv.m_counterPart, 0);
    iv.Increment();
    BOOST_REQUIRE_EQUAL(iv.m_timePart, 2);
    BOOST_REQUIRE_EQUAL(iv.m_counterPart, 1);

    //unrealistic rollover of time part
    iv.m_timePart = UINT64_MAX;
    iv.m_counterPart = UINT64_MAX - 2;
    iv.Increment();
    BOOST_REQUIRE_EQUAL(iv.m_timePart, UINT64_MAX);
    BOOST_REQUIRE_EQUAL(iv.m_counterPart, UINT64_MAX - 1);
    iv.Serialize(serialized.data());
    BOOST_REQUIRE(serialized != allBitsSetVec);
    iv.Increment();
    BOOST_REQUIRE_EQUAL(iv.m_timePart, UINT64_MAX);
    BOOST_REQUIRE_EQUAL(iv.m_counterPart, UINT64_MAX);
    iv.Serialize(serialized.data());
    BOOST_REQUIRE(serialized == allBitsSetVec); //now all bits set
    iv.Increment();
    BOOST_REQUIRE_EQUAL(iv.m_timePart, 0);
    BOOST_REQUIRE_EQUAL(iv.m_counterPart, 0);
    iv.Serialize(serialized.data());
    BOOST_REQUIRE(serialized == noBitsSetVec);
    iv.Increment();
    BOOST_REQUIRE_EQUAL(iv.m_timePart, 0);
    BOOST_REQUIRE_EQUAL(iv.m_counterPart, 1);
}
