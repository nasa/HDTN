#include <boost/test/unit_test.hpp>
#include "LtpFragmentMap.h"
#include <boost/bind.hpp>

BOOST_AUTO_TEST_CASE(LtpFragmentMapTestCase)
{
    typedef LtpFragmentMap::data_fragment_t df;
    typedef Ltp::report_segment_t rs;
    typedef Ltp::reception_claim_t rc;
    BOOST_REQUIRE(df::SimulateSetKeyFind(df(0, 0), df(1, 1))); //abuts so found
    BOOST_REQUIRE(df::SimulateSetKeyFind(df(0, 1), df(2, 3))); //abuts so found
    BOOST_REQUIRE(df::SimulateSetKeyFind(df(1, 2), df(3, 4))); //abuts so found
    BOOST_REQUIRE(df::SimulateSetKeyFind(df(0, 0), df(0, 0))); //identical so found
    BOOST_REQUIRE(df::SimulateSetKeyFind(df(0, 1), df(0, 1))); //identical so found
    BOOST_REQUIRE(df::SimulateSetKeyFind(df(200, 300), df(200, 300))); //identical so found
    BOOST_REQUIRE(df::SimulateSetKeyFind(df(0, 500), df(100, 200))); //overlap so found
    BOOST_REQUIRE(df::SimulateSetKeyFind(df(0, 500), df(400, 600))); //overlap so found

    BOOST_REQUIRE(!df::SimulateSetKeyFind(df(0, 0), df(2, 2))); //no overlap no abut so notfound
    BOOST_REQUIRE(!df::SimulateSetKeyFind(df(100, 200), df(202, 300))); //no overlap no abut so notfound
    BOOST_REQUIRE(!df::SimulateSetKeyFind(df(1, 1), df(3, 3))); //no overlap no abut so notfound
    BOOST_REQUIRE(!df::SimulateSetKeyFind(df(1, 1), df(3, 4))); //no overlap no abut so notfound
    BOOST_REQUIRE(!df::SimulateSetKeyFind(df(0, 1), df(3, 4))); //no overlap no abut so notfound
    BOOST_REQUIRE(!df::SimulateSetKeyFind(df(1, 2), df(4, 5))); //no overlap no abut so notfound

    //sanity check of set equality operators
    BOOST_REQUIRE(std::set<df>({ df(100,200), df(300,400) }) == std::set<df>({ df(100,200), df(300,400) }));
    BOOST_REQUIRE(std::set<df>({ df(100,200), df(300,400) }) != std::set<df>({ df(100,200), df(301,400) }));

    {
        std::set<df> fragmentSet;
        rs reportSegment;
        LtpFragmentMap::InsertFragment(fragmentSet, df(100, 200));
        BOOST_REQUIRE(fragmentSet == std::set<df>({ df(100, 200) }));
        {
            BOOST_REQUIRE(LtpFragmentMap::PopulateReportSegment(fragmentSet, reportSegment));
            BOOST_REQUIRE_EQUAL(reportSegment, rs(0, 0, 201, 100, std::vector<rc>({rc(0,101)})));
        }
        LtpFragmentMap::InsertFragment(fragmentSet, df(300, 400));
        BOOST_REQUIRE(fragmentSet == std::set<df>({ df(100, 200), df(300, 400) }));
        {
            BOOST_REQUIRE(LtpFragmentMap::PopulateReportSegment(fragmentSet, reportSegment));
            BOOST_REQUIRE_EQUAL(reportSegment, rs(0, 0, 401, 100, std::vector<rc>({ rc(0,101), rc(200,101) })));
        }
        LtpFragmentMap::InsertFragment(fragmentSet, df(99, 200));
        BOOST_REQUIRE(fragmentSet == std::set<df>({ df(99, 200), df(300, 400) }));
        {
            BOOST_REQUIRE(LtpFragmentMap::PopulateReportSegment(fragmentSet, reportSegment));
            BOOST_REQUIRE_EQUAL(reportSegment, rs(0, 0, 401, 99, std::vector<rc>({ rc(0,102), rc(201,101) })));
        }
        LtpFragmentMap::InsertFragment(fragmentSet, df(99, 201));
        BOOST_REQUIRE(fragmentSet == std::set<df>({ df(99, 201), df(300, 400) }));
        LtpFragmentMap::InsertFragment(fragmentSet, df(98, 202));
        BOOST_REQUIRE(fragmentSet == std::set<df>({ df(98, 202), df(300, 400) }));
        LtpFragmentMap::InsertFragment(fragmentSet, df(100, 200));
        BOOST_REQUIRE(fragmentSet == std::set<df>({ df(98, 202), df(300, 400) }));
        LtpFragmentMap::InsertFragment(fragmentSet, df(299, 401));
        BOOST_REQUIRE(fragmentSet == std::set<df>({ df(98, 202), df(299, 401) }));
        LtpFragmentMap::InsertFragment(fragmentSet, df(250, 260));
        BOOST_REQUIRE(fragmentSet == std::set<df>({ df(98, 202), df(250, 260), df(299, 401) }));
        LtpFragmentMap::InsertFragment(fragmentSet, df(50, 450));
        BOOST_REQUIRE(fragmentSet == std::set<df>({ df(50, 450) }));
        LtpFragmentMap::InsertFragment(fragmentSet, df(500, 600));
        BOOST_REQUIRE(fragmentSet == std::set<df>({ df(50, 450), df(500, 600) }));
        LtpFragmentMap::InsertFragment(fragmentSet, df(451, 499));
        BOOST_REQUIRE(fragmentSet == std::set<df>({ df(50, 600) }));
    }

    {
        //FROM RFC:
        //If on the other hand, the scope of a report segment has lower bound
        //1000 and upper bound 6000, and the report contains two data reception
        //claims, one with offset 0 and length 2000 and the other with offset
        //3000 and length 500, then the report signifies successful reception
        //only of bytes 1000 - 2999 and 4000 - 4499 of the block.From this we can
        //infer that bytes 3000 - 3999 and 4500 - 5999 of the block need to be
        //retransmitted, but we cannot infer anything about reception of the
        //first 1000 bytes or of any subsequent data beginning at block offset
        //6000.
        std::set<df> fragmentSet;
        rs reportSegment;
        LtpFragmentMap::InsertFragment(fragmentSet, df(1000, 2999));
        LtpFragmentMap::InsertFragment(fragmentSet, df(4000, 4499));
        BOOST_REQUIRE(LtpFragmentMap::PopulateReportSegment(fragmentSet, reportSegment));
        reportSegment.upperBound = 6000; //increase upper bound
        BOOST_REQUIRE_EQUAL(reportSegment, rs(0, 0, 6000, 1000, std::vector<rc>({ rc(0,2000), rc(3000,500) })));
        std::set<df> fragmentSet2;
        LtpFragmentMap::AddReportSegmentToFragmentSet(fragmentSet2, reportSegment);
        BOOST_REQUIRE(fragmentSet == fragmentSet2);
        std::set<df> fragmentsNeedingResent;
        LtpFragmentMap::AddReportSegmentToFragmentSetNeedingResent(fragmentsNeedingResent, reportSegment);
        //LtpFragmentMap::PrintFragmentSet(fragmentsNeedingResent);
        BOOST_REQUIRE(fragmentsNeedingResent == std::set<df>({ df(3000,3999), df(4500,5999) }));
        //LtpFragmentMap::PrintFragmentSet(std::set<df>({ df(3000,3999), df(4500,5999) }));
    }
    {
        rs reportSegment(0, 0, 6000, 0, std::vector<rc>({ rc(0,2000), rc(3000,500) }));
        std::set<df> fragmentsNeedingResent;
        LtpFragmentMap::AddReportSegmentToFragmentSetNeedingResent(fragmentsNeedingResent, reportSegment);
        BOOST_REQUIRE(fragmentsNeedingResent == std::set<df>({ df(2000,2999), df(3500,5999) }));
    }
    {
        rs reportSegment(0, 0, 6000, 0, std::vector<rc>({ rc(1,2000), rc(3000,500) }));
        std::set<df> fragmentsNeedingResent;
        LtpFragmentMap::AddReportSegmentToFragmentSetNeedingResent(fragmentsNeedingResent, reportSegment);
        //LtpFragmentMap::PrintFragmentSet(fragmentsNeedingResent);
        BOOST_REQUIRE(fragmentsNeedingResent == std::set<df>({ df(0,0), df(2001,2999), df(3500,5999) }));
    }
    {
        rs reportSegment(0, 0, 3500, 0, std::vector<rc>({ rc(1,2000), rc(3000,500) }));
        std::set<df> fragmentsNeedingResent;
        LtpFragmentMap::AddReportSegmentToFragmentSetNeedingResent(fragmentsNeedingResent, reportSegment);
        //LtpFragmentMap::PrintFragmentSet(fragmentsNeedingResent);
        BOOST_REQUIRE(fragmentsNeedingResent == std::set<df>({ df(0,0), df(2001,2999) }));
    }
}
