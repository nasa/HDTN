#include <boost/test/unit_test.hpp>
#include <boost/thread.hpp>
#include "codec/AggregateCustodySignal.h"
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
        AggregateCustodySignal acs;
        acs.AddContiguousCustodyIdsToFill(1, 2);
        acs.AddCustodyIdToFill(4);
        std::vector<uint8_t> serializationVec(100);
        uint64_t serializationLength = acs.SerializeFills(&serializationVec[0]);
        BOOST_REQUIRE_EQUAL(serializationLength, 4); //two pairs of 1-byte-min-sized-sdnvs
        std::vector<uint8_t> expectedSerialization = { 1,2,2,1 }; //example is incorrect in that it has {1,2,1,1}
        
        serializationVec.resize(serializationLength);
        BOOST_REQUIRE(serializationVec == expectedSerialization);
        serializationVec.resize(100);
        
        //serialize deserialize whole acs admin record
        serializationLength = acs.Serialize(&serializationVec[0]); //serializationVec must be padded, recommend 16 to 32-bytes
        BOOST_REQUIRE_EQUAL(serializationLength, 6); //two bytes plus two pairs of 1-byte-min-sized-sdnvs
        AggregateCustodySignal acs2;
        BOOST_REQUIRE(acs2.Deserialize(serializationVec.data(), serializationLength)); //serializationVec must be padded, recommend 16 to 32-bytes
        BOOST_REQUIRE(acs == acs2);
        
        //misc
        BOOST_REQUIRE(!(acs != acs2));
        AggregateCustodySignal acsCopy = acs;
        AggregateCustodySignal acsCopy2(acs);
        AggregateCustodySignal acs2Moved = std::move(acs2);
        BOOST_REQUIRE(acs != acs2); //acs2 moved
        BOOST_REQUIRE(acs == acs2Moved);
        BOOST_REQUIRE(acs == acsCopy);
        BOOST_REQUIRE(acs == acsCopy2);
        AggregateCustodySignal acs2Moved2(std::move(acs2Moved));
        BOOST_REQUIRE(acs != acs2Moved); //acs2 moved
        BOOST_REQUIRE(acs == acs2Moved2);
        
    }

    //encodes 0-255,512-782
    {
        AggregateCustodySignal acs;
        acs.AddContiguousCustodyIdsToFill(0, 255);
        acs.AddContiguousCustodyIdsToFill(512, 782);
        std::vector<uint8_t> serializationVec(100);
        
        uint64_t serializationLength;

        //serialize deserialize whole acs admin record
        serializationLength = acs.Serialize(&serializationVec[0]); //serializationVec must be padded, recommend 16 to 32-bytes
        //BOOST_REQUIRE_EQUAL(serializationLength, 6); //two bytes plus two pairs of 1-byte-min-sized-sdnvs
        AggregateCustodySignal acs2;
        BOOST_REQUIRE(acs2.Deserialize(serializationVec.data(), serializationLength)); //serializationVec must be padded, recommend 16 to 32-bytes
        BOOST_REQUIRE(acs == acs2);
        BOOST_REQUIRE_EQUAL(acs.m_custodyIdFills.size(), 2);
        BOOST_REQUIRE_EQUAL(acs.m_custodyIdFills.begin()->beginIndex, 0);
        BOOST_REQUIRE_EQUAL(acs.m_custodyIdFills.begin()->endIndex, 255);
        BOOST_REQUIRE_EQUAL(boost::next(acs.m_custodyIdFills.begin())->beginIndex, 512);
        BOOST_REQUIRE_EQUAL(boost::next(acs.m_custodyIdFills.begin())->endIndex, 782);
    }



}

