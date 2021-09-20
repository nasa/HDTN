#include <boost/test/unit_test.hpp>
#include "codec/CustodyIdAllocator.h"
#include <iostream>
#include <vector>


BOOST_AUTO_TEST_CASE(CustodyIdAllocatorTestCase)
{
    
    //test GetNextCustodyIdForNextHopCtebToSend() with 1 src node
    {
        CustodyIdAllocator cia;
        for (unsigned int i = 0; i < 1000; ++i) {
            BOOST_REQUIRE_EQUAL(cia.GetNextCustodyIdForNextHopCtebToSend(cbhe_eid_t(1,2)), i);
        }
    }

    //test GetNextCustodyIdForNextHopCtebToSend() with 2 src nodes
    {
        CustodyIdAllocator cia;
        BOOST_REQUIRE_EQUAL(cia.GetNextCustodyIdForNextHopCtebToSend(cbhe_eid_t(1, 2)), 0);
        BOOST_REQUIRE_EQUAL(cia.GetNextCustodyIdForNextHopCtebToSend(cbhe_eid_t(2, 2)), 256);
        for (unsigned int i = 1; i <= 255; ++i) {
            BOOST_REQUIRE_EQUAL(cia.GetNextCustodyIdForNextHopCtebToSend(cbhe_eid_t(1, 2)), i);
        }
        for (unsigned int i = 512; i <= 767; ++i) {
            BOOST_REQUIRE_EQUAL(cia.GetNextCustodyIdForNextHopCtebToSend(cbhe_eid_t(1, 2)), i);
        }
        for (unsigned int i = 257; i <= 511; ++i) {
            BOOST_REQUIRE_EQUAL(cia.GetNextCustodyIdForNextHopCtebToSend(cbhe_eid_t(2, 2)), i);
        }
        BOOST_REQUIRE_EQUAL(cia.GetNextCustodyIdForNextHopCtebToSend(cbhe_eid_t(2, 2)), 1024); //767 allocated 768-1023 to node 1,2
        BOOST_REQUIRE_EQUAL(cia.GetNextCustodyIdForNextHopCtebToSend(cbhe_eid_t(2, 2)), 1025);
        BOOST_REQUIRE_EQUAL(cia.GetNextCustodyIdForNextHopCtebToSend(cbhe_eid_t(1, 2)), 768);
        BOOST_REQUIRE_EQUAL(cia.GetNextCustodyIdForNextHopCtebToSend(cbhe_eid_t(1, 2)), 769);
    }

    //test FreeCustodyId
    {
        CustodyIdAllocator cia;
        cia.InitializeAddUsedCustodyId(2);
        cia.InitializeAddUsedCustodyId(4);
        BOOST_REQUIRE_EQUAL(cia.GetNextCustodyIdForNextHopCtebToSend(cbhe_eid_t(1, 1)), 256);
        BOOST_REQUIRE_EQUAL(cia.FreeCustodyId(2), 0); //0 multipliers freed yet
        BOOST_REQUIRE_EQUAL(cia.GetNextCustodyIdForNextHopCtebToSend(cbhe_eid_t(2, 2)), 512);
        BOOST_REQUIRE_EQUAL(cia.FreeCustodyId(4), 1); //1 multiplier freed
        BOOST_REQUIRE_EQUAL(cia.FreeCustodyId(4), 0); //0 multipliers freed (already removed above)
        BOOST_REQUIRE_EQUAL(cia.GetNextCustodyIdForNextHopCtebToSend(cbhe_eid_t(3, 3)), 768);
        BOOST_REQUIRE_EQUAL(cia.GetNextCustodyIdForNextHopCtebToSend(cbhe_eid_t(4, 4)), 0);

        //cia.PrintUsedCustodyIds();
        //cia.PrintUsedCustodyIdMultipliers();
        BOOST_REQUIRE_EQUAL(cia.FreeCustodyId(768), 0); //0 multipliers freed
        for (unsigned int i = 769; i <= 1023; ++i) {
            BOOST_REQUIRE_EQUAL(cia.GetNextCustodyIdForNextHopCtebToSend(cbhe_eid_t(3, 3)), i);
            BOOST_REQUIRE_EQUAL(cia.FreeCustodyId(i), (i == 1023) ? 1 : 0); //1 multiplier freed at i==1023
        }
        //cia.PrintUsedCustodyIds();
        //cia.PrintUsedCustodyIdMultipliers();
        for (unsigned int i = 1024; i <= (1024+(2*256-1)); ++i) {
            BOOST_REQUIRE_EQUAL(cia.GetNextCustodyIdForNextHopCtebToSend(cbhe_eid_t(3, 3)), i);
        }
        //cia.PrintUsedCustodyIds();
        //cia.PrintUsedCustodyIdMultipliers();
        for (unsigned int i = 768; i <= 1023; ++i) {
            BOOST_REQUIRE_EQUAL(cia.GetNextCustodyIdForNextHopCtebToSend(cbhe_eid_t(3, 3)), i);
        }

    }

    //test FreeCustodyIdRange
    {
        {
            CustodyIdAllocator cia;
            cia.InitializeAddUsedCustodyId(2);
            cia.InitializeAddUsedCustodyId(4);
            BOOST_REQUIRE_EQUAL(cia.FreeCustodyIdRange(2, 4), 1); //1 multiplier freed
        }
        {
            CustodyIdAllocator cia;
            cia.InitializeAddUsedCustodyId(2);
            cia.InitializeAddUsedCustodyId(4);
            BOOST_REQUIRE_EQUAL(cia.FreeCustodyIdRange(3, 4), 0); //no multipliers freed
        }
        {
            CustodyIdAllocator cia;
            cia.InitializeAddUsedCustodyId(2);
            cia.InitializeAddUsedCustodyId(1000);
            BOOST_REQUIRE_EQUAL(cia.FreeCustodyIdRange(0, 1000), 2); //2 multipliers freed
        }
        {
            CustodyIdAllocator cia;
            cia.InitializeAddUsedCustodyId(2);
            cia.InitializeAddUsedCustodyId(1000);
            BOOST_REQUIRE_EQUAL(cia.FreeCustodyIdRange(0, 999), 1); //1 multiplier freed
        }
        {
            CustodyIdAllocator cia;
            cia.InitializeAddUsedCustodyId(2);
            cia.InitializeAddUsedCustodyId(1000);
            BOOST_REQUIRE_EQUAL(cia.FreeCustodyIdRange(3, 999), 0); //0 multipliers freed
        }
    }
}

