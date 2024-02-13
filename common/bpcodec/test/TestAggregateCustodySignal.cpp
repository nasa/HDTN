/**
 * @file TestAggregateCustodySignal.cpp
 * @author  Brian Tomko <brian.j.tomko@nasa.gov>
 *
 * @copyright Copyright (c) 2021 United States Government as represented by
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
#include "codec/bpv6.h"
#include <iostream>
#include <string>
#include <inttypes.h>
#include <vector>

BOOST_AUTO_TEST_CASE(Bpv6AggregateCustodySignalTestCase)
{
    
    //AggregateCustodySignal
    //example on page 8 of https://cwe.ccsds.org/sis/docs/SIS-DTN/Meeting%20Materials/2011/Fall%20(Colorado)/jenkins-sisdtn-aggregate-custody-signals.pdf
    //encodes 1-2,4 (bundle 3 lost)
    {
        Bpv6AdministrativeRecordContentAggregateCustodySignal acs;
        acs.AddContiguousCustodyIdsToFill(1, 2);
        acs.AddCustodyIdToFill(4);
        std::vector<uint8_t> serializationVec(100);
        uint64_t serializationLength = acs.SerializeFills(&serializationVec[0], serializationVec.size()); //serializationVec does not need to be padded, but it will run faster if it is .. recommend 16 to 32-bytes
        BOOST_REQUIRE_EQUAL(serializationLength, 4); //two pairs of 1-byte-min-sized-sdnvs
        BOOST_REQUIRE_EQUAL(acs.GetFillSerializedSize(), 4); //two pairs of 1-byte-min-sized-sdnvs
        std::vector<uint8_t> expectedSerialization = { 1,2,2,1 }; //example is incorrect in that it has {1,2,1,1}
        
        serializationVec.resize(serializationLength);
        BOOST_REQUIRE(serializationVec == expectedSerialization);
        serializationVec.resize(100);
        
        //serialize deserialize whole acs admin record
        const uint64_t expectedSerializationLength = acs.GetSerializationSize();
        serializationLength = acs.SerializeBpv6(&serializationVec[0], serializationVec.size()); //serializationVec does not need to be padded, but it will run faster if it is .. recommend 16 to 32-bytes
        BOOST_REQUIRE_EQUAL(serializationLength, expectedSerializationLength);
        BOOST_REQUIRE_EQUAL(serializationLength, 5); //one-byte-m_statusFlagsPlus7bitReasonCode plus two pairs of 1-byte-min-sized-sdnvs
        Bpv6AdministrativeRecordContentAggregateCustodySignal acs2;
        uint64_t numBytesTakenToDecode;
        BOOST_REQUIRE(acs2.DeserializeBpv6(serializationVec.data(), numBytesTakenToDecode, serializationLength)); //serializationVec does not need to be padded, but it will run faster if it is .. recommend 16 to 32-bytes
        BOOST_REQUIRE_EQUAL(serializationLength, numBytesTakenToDecode);
        BOOST_REQUIRE(acs == acs2);
        
        //misc
        BOOST_REQUIRE(!(acs != acs2));
        Bpv6AdministrativeRecordContentAggregateCustodySignal acsCopy = acs;
        Bpv6AdministrativeRecordContentAggregateCustodySignal acsCopy2(acs);
        Bpv6AdministrativeRecordContentAggregateCustodySignal acs2Moved = std::move(acs2);
        BOOST_REQUIRE(acs != acs2); //acs2 moved
        BOOST_REQUIRE(acs == acs2Moved);
        BOOST_REQUIRE(acs == acsCopy);
        BOOST_REQUIRE(acs == acsCopy2);
        Bpv6AdministrativeRecordContentAggregateCustodySignal acs2Moved2(std::move(acs2Moved));
        BOOST_REQUIRE(acs != acs2Moved); //acs2 moved
        BOOST_REQUIRE(acs == acs2Moved2);
        
    }

    //encodes 0-255,512-782
    {
        Bpv6AdministrativeRecordContentAggregateCustodySignal acs;
        acs.AddContiguousCustodyIdsToFill(0, 255);
        acs.AddContiguousCustodyIdsToFill(512, 782);
        std::vector<uint8_t> serializationVec(100);
        
        uint64_t serializationLength;

        //serialize deserialize whole acs admin record
        serializationLength = acs.SerializeBpv6(&serializationVec[0], serializationVec.size()); //serializationVec does not need to be padded, but it will run faster if it is .. recommend 16 to 32-bytes
        //BOOST_REQUIRE_EQUAL(serializationLength, 6); //two bytes plus two pairs of 1-byte-min-sized-sdnvs
        Bpv6AdministrativeRecordContentAggregateCustodySignal acs2;
        uint64_t numBytesTakenToDecode;
        BOOST_REQUIRE(acs2.DeserializeBpv6(serializationVec.data(), numBytesTakenToDecode, serializationLength)); //serializationVec does not need to be padded, but it will run faster if it is .. recommend 16 to 32-bytes
        BOOST_REQUIRE_EQUAL(serializationLength, numBytesTakenToDecode);
        BOOST_REQUIRE(acs == acs2);
        BOOST_REQUIRE_EQUAL(acs.m_custodyIdFills.size(), 2);
        BOOST_REQUIRE_EQUAL(acs.m_custodyIdFills.begin()->beginIndex, 0);
        BOOST_REQUIRE_EQUAL(acs.m_custodyIdFills.begin()->endIndex, 255);
        BOOST_REQUIRE_EQUAL(boost::next(acs.m_custodyIdFills.begin())->beginIndex, 512);
        BOOST_REQUIRE_EQUAL(boost::next(acs.m_custodyIdFills.begin())->endIndex, 782);
    }



}

