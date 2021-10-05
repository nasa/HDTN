#include <boost/test/unit_test.hpp>
#include <iostream>
#include "CustodyTimers.h"
#include <boost/thread.hpp>

    
BOOST_AUTO_TEST_CASE(CustodyTimersTestCase)
{
    static const cbhe_eid_t EID1(5, 5);
    static const cbhe_eid_t EID2(10, 5);
    static const cbhe_eid_t EID3(15, 5);
    static const std::vector<cbhe_eid_t> ALL_EIDS_AVAILABLE_VEC = { EID1, EID2, EID3 };
    static const std::vector<cbhe_eid_t> JUST_EID1_AVAILABLE_VEC = { EID1 };
    static const std::vector<cbhe_eid_t> JUST_EID2_AVAILABLE_VEC = { EID2 };

    //never expire
    {
        CustodyTimers ct(boost::posix_time::seconds(10000));
        const boost::posix_time::ptime nowPtime = boost::posix_time::microsec_clock::universal_time();
        BOOST_REQUIRE_EQUAL(ct.GetNumCustodyTransferTimers(), 0);
        BOOST_REQUIRE_EQUAL(ct.GetNumCustodyTransferTimers(EID1), 0);
        BOOST_REQUIRE_EQUAL(ct.GetNumCustodyTransferTimers(EID2), 0);
        for (uint64_t cid = 1; cid <= 10; ++cid) {
            BOOST_REQUIRE(ct.StartCustodyTransferTimer(EID1, cid));
            BOOST_REQUIRE_EQUAL(ct.GetNumCustodyTransferTimers(), cid);
            BOOST_REQUIRE_EQUAL(ct.GetNumCustodyTransferTimers(EID1), cid);
            BOOST_REQUIRE_EQUAL(ct.GetNumCustodyTransferTimers(EID2), 0);
            BOOST_REQUIRE_EQUAL(ct.GetNumCustodyTransferTimers(EID3), 0);
        }
        for (uint64_t cid = 1; cid <= 20; ++cid) {
            uint64_t returnedCid;
            BOOST_REQUIRE(!ct.PollOneAndPopExpiredCustodyTimer(returnedCid, ALL_EIDS_AVAILABLE_VEC, nowPtime));
        }
        for (uint64_t cid = 1; cid <= 10; ++cid) {
            BOOST_REQUIRE(ct.CancelCustodyTransferTimer(EID1, cid));
            BOOST_REQUIRE(!ct.CancelCustodyTransferTimer(EID1, cid));
            BOOST_REQUIRE_EQUAL(ct.GetNumCustodyTransferTimers(EID1), 10 - cid);
            BOOST_REQUIRE_EQUAL(ct.GetNumCustodyTransferTimers(), 10 - cid);
        }

        //multiple EIDs
        for (uint64_t cid = 1; cid <= 10; ++cid) {
            BOOST_REQUIRE(ct.StartCustodyTransferTimer(EID1, cid));
            BOOST_REQUIRE(ct.StartCustodyTransferTimer(EID2, cid + 100));
            BOOST_REQUIRE(ct.StartCustodyTransferTimer(EID3, cid + 200));
            BOOST_REQUIRE_EQUAL(ct.GetNumCustodyTransferTimers(), cid * 3);
            BOOST_REQUIRE_EQUAL(ct.GetNumCustodyTransferTimers(EID1), cid);
            BOOST_REQUIRE_EQUAL(ct.GetNumCustodyTransferTimers(EID2), cid);
            BOOST_REQUIRE_EQUAL(ct.GetNumCustodyTransferTimers(EID3), cid);
            BOOST_REQUIRE(!ct.StartCustodyTransferTimer(EID3, cid + 200)); //fail already added
        }
    }

    //always expire
    {
        CustodyTimers ct(boost::posix_time::seconds(0));
        boost::this_thread::sleep(boost::posix_time::milliseconds(1)); //expired now
        BOOST_REQUIRE_EQUAL(ct.GetNumCustodyTransferTimers(), 0);
        BOOST_REQUIRE_EQUAL(ct.GetNumCustodyTransferTimers(EID1), 0);
        BOOST_REQUIRE_EQUAL(ct.GetNumCustodyTransferTimers(EID2), 0);
        for (uint64_t cid = 1; cid <= 10; ++cid) {
            BOOST_REQUIRE(ct.StartCustodyTransferTimer(EID1, cid));
            BOOST_REQUIRE_EQUAL(ct.GetNumCustodyTransferTimers(), cid);
            BOOST_REQUIRE_EQUAL(ct.GetNumCustodyTransferTimers(EID1), cid);
            BOOST_REQUIRE_EQUAL(ct.GetNumCustodyTransferTimers(EID2), 0);
            BOOST_REQUIRE_EQUAL(ct.GetNumCustodyTransferTimers(EID3), 0);
        }
        for (uint64_t cid = 1; cid <= 10; ++cid) {
            uint64_t returnedCid;
            BOOST_REQUIRE(ct.PollOneAndPopExpiredCustodyTimer(returnedCid, ALL_EIDS_AVAILABLE_VEC, boost::posix_time::microsec_clock::universal_time()));
            BOOST_REQUIRE_EQUAL(returnedCid, cid); //fifo order
            BOOST_REQUIRE_EQUAL(ct.GetNumCustodyTransferTimers(EID1), 10 - cid);
            BOOST_REQUIRE_EQUAL(ct.GetNumCustodyTransferTimers(), 10 - cid);
        }

        //multiple EIDs
        for (uint64_t cid = 1; cid <= 10; ++cid) {
            BOOST_REQUIRE(ct.StartCustodyTransferTimer(EID1, cid));
            BOOST_REQUIRE(ct.StartCustodyTransferTimer(EID2, cid + 100));
            BOOST_REQUIRE(ct.StartCustodyTransferTimer(EID3, cid + 200));
        }
        for (uint64_t cid = 1; cid <= 10; ++cid) { //remove just eid 2
            uint64_t returnedCid;
            BOOST_REQUIRE(ct.PollOneAndPopExpiredCustodyTimer(returnedCid, JUST_EID2_AVAILABLE_VEC, boost::posix_time::microsec_clock::universal_time()));
            BOOST_REQUIRE_EQUAL(returnedCid, cid + 100); //fifo order
            BOOST_REQUIRE_EQUAL(ct.GetNumCustodyTransferTimers(EID1), 10);
            BOOST_REQUIRE_EQUAL(ct.GetNumCustodyTransferTimers(EID2), 10 - cid);
            BOOST_REQUIRE_EQUAL(ct.GetNumCustodyTransferTimers(EID3), 10);
        }
        for (uint64_t cid = 1; cid <= 10; ++cid) { //remove just eid 1
            uint64_t returnedCid;
            BOOST_REQUIRE(ct.PollOneAndPopExpiredCustodyTimer(returnedCid, JUST_EID1_AVAILABLE_VEC, boost::posix_time::microsec_clock::universal_time()));
            BOOST_REQUIRE_EQUAL(returnedCid, cid); //fifo order
            BOOST_REQUIRE_EQUAL(ct.GetNumCustodyTransferTimers(EID1), 10 - cid);
            BOOST_REQUIRE_EQUAL(ct.GetNumCustodyTransferTimers(EID2), 0);
            BOOST_REQUIRE_EQUAL(ct.GetNumCustodyTransferTimers(EID3), 10);
        }
        for (uint64_t cid = 1; cid <= 10; ++cid) { //remove just eid 3
            uint64_t returnedCid;
            BOOST_REQUIRE(ct.PollOneAndPopExpiredCustodyTimer(returnedCid, ALL_EIDS_AVAILABLE_VEC, boost::posix_time::microsec_clock::universal_time()));
            BOOST_REQUIRE_EQUAL(returnedCid, cid + 200); //fifo order
            BOOST_REQUIRE_EQUAL(ct.GetNumCustodyTransferTimers(EID1), 0);
            BOOST_REQUIRE_EQUAL(ct.GetNumCustodyTransferTimers(EID2), 0);
            BOOST_REQUIRE_EQUAL(ct.GetNumCustodyTransferTimers(EID3), 10 - cid);
        }
    }


}
