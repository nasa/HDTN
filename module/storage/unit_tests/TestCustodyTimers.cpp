/**
 * @file TestCustodyTimers.cpp
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
        uint64_t returnedCid;
        for (uint64_t cid = 1; cid <= 20; ++cid) {
            BOOST_REQUIRE(!ct.PollOneAndPopExpiredCustodyTimer(returnedCid, ALL_EIDS_AVAILABLE_VEC, nowPtime));
        }
        uint64_t countPops = 0;
        while (ct.PollOneAndPopAnyExpiredCustodyTimer(returnedCid, nowPtime)) {
            ++countPops;
        }
        BOOST_REQUIRE_EQUAL(countPops, 0);
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
        boost::this_thread::sleep(boost::posix_time::milliseconds(1)); //expired now (called after StartCustodyTransferTimer)
        uint64_t returnedCid;
        for (uint64_t cid = 1; cid <= 10; ++cid) {
            BOOST_REQUIRE(ct.PollOneAndPopExpiredCustodyTimer(returnedCid, ALL_EIDS_AVAILABLE_VEC, boost::posix_time::microsec_clock::universal_time()));
            BOOST_REQUIRE_EQUAL(returnedCid, cid); //fifo order
            BOOST_REQUIRE_EQUAL(ct.GetNumCustodyTransferTimers(EID1), 10 - cid);
            BOOST_REQUIRE_EQUAL(ct.GetNumCustodyTransferTimers(), 10 - cid);
        }

        //repeat above for PollOneAndPopAnyExpiredCustodyTimer
        for (uint64_t cid = 1; cid <= 10; ++cid) {
            BOOST_REQUIRE(ct.StartCustodyTransferTimer(EID1, cid));
            BOOST_REQUIRE_EQUAL(ct.GetNumCustodyTransferTimers(), cid);
            BOOST_REQUIRE_EQUAL(ct.GetNumCustodyTransferTimers(EID1), cid);
            BOOST_REQUIRE_EQUAL(ct.GetNumCustodyTransferTimers(EID2), 0);
            BOOST_REQUIRE_EQUAL(ct.GetNumCustodyTransferTimers(EID3), 0);
        }
        uint64_t countPops = 0;
        returnedCid = 0;
        boost::this_thread::sleep(boost::posix_time::milliseconds(1)); //expired now (called after StartCustodyTransferTimer)
        while (ct.PollOneAndPopAnyExpiredCustodyTimer(returnedCid, boost::posix_time::microsec_clock::universal_time())) {
            ++countPops;
        }
        BOOST_REQUIRE_EQUAL(countPops, 10);
        BOOST_REQUIRE_GE(returnedCid, 1);
        BOOST_REQUIRE_LE(returnedCid, 10);

        //multiple EIDs
        for (uint64_t cid = 1; cid <= 10; ++cid) {
            BOOST_REQUIRE(ct.StartCustodyTransferTimer(EID1, cid));
            BOOST_REQUIRE(ct.StartCustodyTransferTimer(EID2, cid + 100));
            BOOST_REQUIRE(ct.StartCustodyTransferTimer(EID3, cid + 200));
        }
        boost::this_thread::sleep(boost::posix_time::milliseconds(1)); //expired now (called after StartCustodyTransferTimer)
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

        //repeat multiple EIDs above for PollOneAndPopAnyExpiredCustodyTimer
        for (uint64_t cid = 1; cid <= 10; ++cid) {
            BOOST_REQUIRE(ct.StartCustodyTransferTimer(EID1, cid));
            BOOST_REQUIRE(ct.StartCustodyTransferTimer(EID2, cid + 100));
            BOOST_REQUIRE(ct.StartCustodyTransferTimer(EID3, cid + 200));
        }
        countPops = 0;
        returnedCid = 0;
        boost::this_thread::sleep(boost::posix_time::milliseconds(1)); //expired now (called after StartCustodyTransferTimer)
        while (ct.PollOneAndPopAnyExpiredCustodyTimer(returnedCid, boost::posix_time::microsec_clock::universal_time())) {
            ++countPops;
        }
        BOOST_REQUIRE_EQUAL(countPops, 30);
        BOOST_REQUIRE_GE(returnedCid, 1);
        BOOST_REQUIRE_LE(returnedCid, 10 + 200);
    }


}
