/**
 * @file TestPaddedVectorUint8.cpp
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
#include <boost/thread.hpp>
#include <iostream>
#include <string>
#include <inttypes.h>
#include <vector>
#include "PaddedVectorUint8.h"

static void VerifyVector(const padded_vector_uint8_unit_test_t & v) {
    static const std::vector<std::string> testStringsVec = { "padding_start", "before_data", "after_reserved", "padding_end" };
    //std::cout << "capacity " << v.capacity() << "\n";
    const uint8_t * paddingStart = v.data() - PaddedMallocatorConstants::PADDING_ELEMENTS_BEFORE;
    const uint8_t * dataStart = v.data();
    const uint8_t * rightAfterReservedSpace = v.data() + v.capacity();
    const uint8_t * paddingEnd = v.data() + (PaddedMallocatorConstants::PADDING_ELEMENTS_AFTER + v.capacity());
    std::string s0(((char*)paddingStart), testStringsVec[0].size());//start of padding
    std::string s1(((char*)dataStart) - testStringsVec[1].size(), testStringsVec[1].size());//right before data
    std::string s2(((char*)rightAfterReservedSpace), testStringsVec[2].size());//right after reserved size
    std::string s3(((char*)paddingEnd) - testStringsVec[3].size(), testStringsVec[3].size());//right before data
    BOOST_REQUIRE_EQUAL(s0, testStringsVec[0]);
    BOOST_REQUIRE_EQUAL(s1, testStringsVec[1]);
    BOOST_REQUIRE_EQUAL(s2, testStringsVec[2]);
    BOOST_REQUIRE_EQUAL(s3, testStringsVec[3]);
}
BOOST_AUTO_TEST_CASE(PaddedVectorUint8TestCase)
{

    {
        padded_vector_uint8_unit_test_t v(1);
        v[0] = 1;
        BOOST_REQUIRE_EQUAL(v[0], 1);
        VerifyVector(v);
        v.resize(8);
        VerifyVector(v);
        v.push_back(5);
        BOOST_REQUIRE_EQUAL(v[0], 1);
        BOOST_REQUIRE_EQUAL(v[8], 5);
        VerifyVector(v);
    }

    

}

