#include <boost/test/unit_test.hpp>
#include "LtpFragmentSet.h"
#include <boost/bind/bind.hpp>

BOOST_AUTO_TEST_CASE(LtpFragmentSetTestCase)
{
    typedef LtpFragmentSet::data_fragment_t df;
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
        LtpFragmentSet::InsertFragment(fragmentSet, df(100, 200));
        BOOST_REQUIRE(fragmentSet == std::set<df>({ df(100, 200) }));
        {
            BOOST_REQUIRE(LtpFragmentSet::PopulateReportSegment(fragmentSet, reportSegment));
            BOOST_REQUIRE_EQUAL(reportSegment, rs(0, 0, 201, 100, std::vector<rc>({rc(0,101)})));
        }
        LtpFragmentSet::InsertFragment(fragmentSet, df(300, 400));
        BOOST_REQUIRE(fragmentSet == std::set<df>({ df(100, 200), df(300, 400) }));
        {
            BOOST_REQUIRE(LtpFragmentSet::PopulateReportSegment(fragmentSet, reportSegment));
            BOOST_REQUIRE_EQUAL(reportSegment, rs(0, 0, 401, 100, std::vector<rc>({ rc(0,101), rc(200,101) })));
        }
        LtpFragmentSet::InsertFragment(fragmentSet, df(99, 200));
        BOOST_REQUIRE(fragmentSet == std::set<df>({ df(99, 200), df(300, 400) }));
        {
            BOOST_REQUIRE(LtpFragmentSet::PopulateReportSegment(fragmentSet, reportSegment));
            BOOST_REQUIRE_EQUAL(reportSegment, rs(0, 0, 401, 99, std::vector<rc>({ rc(0,102), rc(201,101) })));
        }
        LtpFragmentSet::InsertFragment(fragmentSet, df(99, 201));
        BOOST_REQUIRE(fragmentSet == std::set<df>({ df(99, 201), df(300, 400) }));
        LtpFragmentSet::InsertFragment(fragmentSet, df(98, 202));
        BOOST_REQUIRE(fragmentSet == std::set<df>({ df(98, 202), df(300, 400) }));
        LtpFragmentSet::InsertFragment(fragmentSet, df(100, 200));
        BOOST_REQUIRE(fragmentSet == std::set<df>({ df(98, 202), df(300, 400) }));
        LtpFragmentSet::InsertFragment(fragmentSet, df(299, 401));
        BOOST_REQUIRE(fragmentSet == std::set<df>({ df(98, 202), df(299, 401) }));
        LtpFragmentSet::InsertFragment(fragmentSet, df(250, 260));
        BOOST_REQUIRE(fragmentSet == std::set<df>({ df(98, 202), df(250, 260), df(299, 401) }));
        LtpFragmentSet::InsertFragment(fragmentSet, df(50, 450));
        BOOST_REQUIRE(fragmentSet == std::set<df>({ df(50, 450) }));
        LtpFragmentSet::InsertFragment(fragmentSet, df(500, 600));
        BOOST_REQUIRE(fragmentSet == std::set<df>({ df(50, 450), df(500, 600) }));
        LtpFragmentSet::InsertFragment(fragmentSet, df(451, 499));
        BOOST_REQUIRE(fragmentSet == std::set<df>({ df(50, 600) }));
    }

    //test removing fragments (not used in ltp)
    {
        std::set<df> fragmentSet;
        FragmentSet::InsertFragment(fragmentSet, df(0, 0));
        BOOST_REQUIRE(fragmentSet == std::set<df>({ df(0, 0)}));
        FragmentSet::RemoveFragment(fragmentSet, df(0, 0));
        BOOST_REQUIRE(fragmentSet == std::set<df>({ }));

        FragmentSet::InsertFragment(fragmentSet, df(0, 100));
        BOOST_REQUIRE(fragmentSet == std::set<df>({ df(0, 100) }));
        FragmentSet::RemoveFragment(fragmentSet, df(0, 100));
        BOOST_REQUIRE(fragmentSet == std::set<df>({ }));

        FragmentSet::InsertFragment(fragmentSet, df(0, 100));
        BOOST_REQUIRE(fragmentSet == std::set<df>({ df(0, 100) }));
        FragmentSet::RemoveFragment(fragmentSet, df(100, 100));
        BOOST_REQUIRE(fragmentSet == std::set<df>({ df(0, 99) }));
        FragmentSet::RemoveFragment(fragmentSet, df(0, 0));
        BOOST_REQUIRE(fragmentSet == std::set<df>({ df(1, 99) }));
        FragmentSet::RemoveFragment(fragmentSet, df(50, 50)); //split
        BOOST_REQUIRE(fragmentSet == std::set<df>({ df(1, 49), df(51, 99) }));
        FragmentSet::RemoveFragment(fragmentSet, df(0, 3)); //rm left
        BOOST_REQUIRE(fragmentSet == std::set<df>({ df(4, 49), df(51, 99) }));
        FragmentSet::RemoveFragment(fragmentSet, df(90, 1000)); //rm right
        BOOST_REQUIRE(fragmentSet == std::set<df>({ df(4, 49), df(51, 89) }));
        FragmentSet::RemoveFragment(fragmentSet, df(45, 55)); //span across
        BOOST_REQUIRE(fragmentSet == std::set<df>({ df(4, 44), df(56, 89) }));
        FragmentSet::RemoveFragment(fragmentSet, df(10, 12)); //split left
        BOOST_REQUIRE(fragmentSet == std::set<df>({ df(4, 9), df(13, 44), df(56, 89) }));
        FragmentSet::RemoveFragment(fragmentSet, df(60, 70)); //split right
        BOOST_REQUIRE(fragmentSet == std::set<df>({ df(4, 9), df(13, 44), df(56, 59), df(71, 89) }));
        FragmentSet::RemoveFragment(fragmentSet, df(0, 1000)); //delete all
        BOOST_REQUIRE(fragmentSet == std::set<df>({ }));

        FragmentSet::InsertFragment(fragmentSet, df(60, 70));
        BOOST_REQUIRE(fragmentSet == std::set<df>({ df(60, 70) }));
        FragmentSet::RemoveFragment(fragmentSet, df(0, 70));
        BOOST_REQUIRE(fragmentSet == std::set<df>({ }));

        FragmentSet::InsertFragment(fragmentSet, df(60, 70));
        BOOST_REQUIRE(fragmentSet == std::set<df>({ df(60, 70) }));
        FragmentSet::RemoveFragment(fragmentSet, df(60, 1000));
        BOOST_REQUIRE(fragmentSet == std::set<df>({ }));

        FragmentSet::InsertFragment(fragmentSet, df(60, 70));
        BOOST_REQUIRE(fragmentSet == std::set<df>({ df(60, 70) }));
        FragmentSet::RemoveFragment(fragmentSet, df(0, 69));
        BOOST_REQUIRE(fragmentSet == std::set<df>({ df(70, 70) }));
        FragmentSet::InsertFragment(fragmentSet, df(60, 70));
        BOOST_REQUIRE(fragmentSet == std::set<df>({ df(60, 70) }));
        FragmentSet::RemoveFragment(fragmentSet, df(61, 1000));
        BOOST_REQUIRE(fragmentSet == std::set<df>({ df(60, 60) }));
        FragmentSet::RemoveFragment(fragmentSet, df(60, 60));
        BOOST_REQUIRE(fragmentSet == std::set<df>({ }));
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
        LtpFragmentSet::InsertFragment(fragmentSet, df(1000, 2999));
        LtpFragmentSet::InsertFragment(fragmentSet, df(4000, 4499));
        BOOST_REQUIRE(LtpFragmentSet::PopulateReportSegment(fragmentSet, reportSegment));
        reportSegment.upperBound = 6000; //increase upper bound
        BOOST_REQUIRE_EQUAL(reportSegment, rs(0, 0, 6000, 1000, std::vector<rc>({ rc(0,2000), rc(3000,500) })));
        std::set<df> fragmentSet2;
        LtpFragmentSet::AddReportSegmentToFragmentSet(fragmentSet2, reportSegment);
        BOOST_REQUIRE(fragmentSet == fragmentSet2);
        std::set<df> fragmentsNeedingResent;
        LtpFragmentSet::AddReportSegmentToFragmentSetNeedingResent(fragmentsNeedingResent, reportSegment);
        //LtpFragmentSet::PrintFragmentSet(fragmentsNeedingResent);
        BOOST_REQUIRE(fragmentsNeedingResent == std::set<df>({ df(3000,3999), df(4500,5999) }));
        //LtpFragmentSet::PrintFragmentSet(std::set<df>({ df(3000,3999), df(4500,5999) }));
    }
    {
        rs reportSegment(0, 0, 6000, 0, std::vector<rc>({ rc(0,2000), rc(3000,500) }));
        std::set<df> fragmentsNeedingResent;
        LtpFragmentSet::AddReportSegmentToFragmentSetNeedingResent(fragmentsNeedingResent, reportSegment);
        BOOST_REQUIRE(fragmentsNeedingResent == std::set<df>({ df(2000,2999), df(3500,5999) }));
    }
    {
        rs reportSegment(0, 0, 6000, 0, std::vector<rc>({ rc(1,2000), rc(3000,500) }));
        std::set<df> fragmentsNeedingResent;
        LtpFragmentSet::AddReportSegmentToFragmentSetNeedingResent(fragmentsNeedingResent, reportSegment);
        //LtpFragmentSet::PrintFragmentSet(fragmentsNeedingResent);
        BOOST_REQUIRE(fragmentsNeedingResent == std::set<df>({ df(0,0), df(2001,2999), df(3500,5999) }));
    }
    {
        rs reportSegment(0, 0, 3500, 0, std::vector<rc>({ rc(1,2000), rc(3000,500) }));
        std::set<df> fragmentsNeedingResent;
        LtpFragmentSet::AddReportSegmentToFragmentSetNeedingResent(fragmentsNeedingResent, reportSegment);
        //LtpFragmentSet::PrintFragmentSet(fragmentsNeedingResent);
        BOOST_REQUIRE(fragmentsNeedingResent == std::set<df>({ df(0,0), df(2001,2999) }));
    }
    {
        //added to fix bug:
        //rs: upper bound : 20, lower bound : 15
        //    claims :
        //    offset : 1, length : 4
        //
        //acked segments : (0, 14) (16, 19)
        //
        //    need resent : nothing, but should be (15,15)
        rs reportSegment(0, 0, 20, 15, std::vector<rc>({ rc(1,4) }));
        std::set<df> fragmentsNeedingResent;
        LtpFragmentSet::AddReportSegmentToFragmentSetNeedingResent(fragmentsNeedingResent, reportSegment);
        BOOST_REQUIRE(fragmentsNeedingResent == std::set<df>({ df(15,15)}));
    }

    //REPORT SEGMENTS WITH CUSTOM LOWER AND UPPER BOUNDS

    {
        rs reportSegment;
        BOOST_REQUIRE(LtpFragmentSet::PopulateReportSegment(std::set<df>({ df(1000,2999), df(4000,4499) }) , reportSegment));
        reportSegment.upperBound = 6000; //increase upper bound
        BOOST_REQUIRE_EQUAL(reportSegment, rs(0, 0, 6000, 1000, std::vector<rc>({ rc(0,2000), rc(3000,500) })));
    }
    //same as above
    {
        rs reportSegment;
        BOOST_REQUIRE(LtpFragmentSet::PopulateReportSegment(std::set<df>({ df(1000,2999), df(4000,4499) }), reportSegment, 1000));
        reportSegment.upperBound = 6000; //increase upper bound
        BOOST_REQUIRE_EQUAL(reportSegment, rs(0, 0, 6000, 1000, std::vector<rc>({ rc(0,2000), rc(3000,500) })));

        //SOME UPPER BOUND TESTS BELOW
        BOOST_REQUIRE(!LtpFragmentSet::PopulateReportSegment(std::set<df>({ df(1000,2999), df(4000,4499) }), reportSegment, 1000, 1000)); //can't have UB = LB
        BOOST_REQUIRE(!LtpFragmentSet::PopulateReportSegment(std::set<df>({ df(1000,2999), df(4000,4499) }), reportSegment, 1000, 999)); //can't have UB < LB

        BOOST_REQUIRE(LtpFragmentSet::PopulateReportSegment(std::set<df>({ df(1000,2999), df(4000,4499) }), reportSegment, 1000, 1001));
        BOOST_REQUIRE_EQUAL(reportSegment, rs(0, 0, 1001, 1000, std::vector<rc>({ rc(0,1)})));

        BOOST_REQUIRE(LtpFragmentSet::PopulateReportSegment(std::set<df>({ df(1000,2999), df(4000,4499) }), reportSegment, 1000, 1002));
        BOOST_REQUIRE_EQUAL(reportSegment, rs(0, 0, 1002, 1000, std::vector<rc>({ rc(0,2) })));

        BOOST_REQUIRE(LtpFragmentSet::PopulateReportSegment(std::set<df>({ df(1000,2999), df(4000,4499) }), reportSegment, 1000, 3500));
        BOOST_REQUIRE_EQUAL(reportSegment, rs(0, 0, 3500, 1000, std::vector<rc>({ rc(0,2000)})));

        BOOST_REQUIRE(LtpFragmentSet::PopulateReportSegment(std::set<df>({ df(1000,2999), df(4000,4499) }), reportSegment, 1000, 4400));
        BOOST_REQUIRE_EQUAL(reportSegment, rs(0, 0, 4400, 1000, std::vector<rc>({ rc(0,2000), rc(3000, 400) })));

        BOOST_REQUIRE(LtpFragmentSet::PopulateReportSegment(std::set<df>({ df(1000,2999), df(4000,4499) }), reportSegment, 1000, 6000));
        BOOST_REQUIRE_EQUAL(reportSegment, rs(0, 0, 6000, 1000, std::vector<rc>({ rc(0,2000), rc(3000, 500) })));
    }

    {
        rs reportSegment;
        BOOST_REQUIRE(LtpFragmentSet::PopulateReportSegment(std::set<df>({ df(1000,2999), df(4000,4499) }), reportSegment, 0));
        reportSegment.upperBound = 6000; //increase upper bound
        BOOST_REQUIRE_EQUAL(reportSegment, rs(0, 0, 6000, 0, std::vector<rc>({ rc(1000,2000), rc(4000,500) })));
    }

    {
        rs reportSegment;
        BOOST_REQUIRE(LtpFragmentSet::PopulateReportSegment(std::set<df>({ df(1000,2999), df(4000,4499) }), reportSegment, 1));
        reportSegment.upperBound = 6000; //increase upper bound
        BOOST_REQUIRE_EQUAL(reportSegment, rs(0, 0, 6000, 1, std::vector<rc>({ rc(999,2000), rc(3999,500) })));
    }

    {
        rs reportSegment;
        BOOST_REQUIRE(LtpFragmentSet::PopulateReportSegment(std::set<df>({ df(1000,2999), df(4000,4499) }), reportSegment, 1001));
        reportSegment.upperBound = 6000; //increase upper bound
        BOOST_REQUIRE_EQUAL(reportSegment, rs(0, 0, 6000, 1001, std::vector<rc>({ rc(0,1999), rc(2999,500) })));
    }

    {
        rs reportSegment;
        BOOST_REQUIRE(LtpFragmentSet::PopulateReportSegment(std::set<df>({ df(1000,2999), df(4000,4499) }), reportSegment, 2999));
        reportSegment.upperBound = 6000; //increase upper bound
        BOOST_REQUIRE_EQUAL(reportSegment, rs(0, 0, 6000, 2999, std::vector<rc>({ rc(0,1), rc(1001,500) })));
    }

    {
        rs reportSegment;
        BOOST_REQUIRE(LtpFragmentSet::PopulateReportSegment(std::set<df>({ df(1000,2999), df(4000,4499) }), reportSegment, 3000));
        reportSegment.upperBound = 6000; //increase upper bound
        BOOST_REQUIRE_EQUAL(reportSegment, rs(0, 0, 6000, 3000, std::vector<rc>({ rc(1000,500) })));
    }

    {
        rs reportSegment;
        BOOST_REQUIRE(LtpFragmentSet::PopulateReportSegment(std::set<df>({ df(1000,2999), df(4000,4499) }), reportSegment, 3001));
        reportSegment.upperBound = 6000; //increase upper bound
        BOOST_REQUIRE_EQUAL(reportSegment, rs(0, 0, 6000, 3001, std::vector<rc>({ rc(999,500) })));
    }

    //TEST ContainsFragmentEntirely
    {
        std::set<df> fragmentSet;
        LtpFragmentSet::InsertFragment(fragmentSet, df(100, 200));
        BOOST_REQUIRE(fragmentSet == std::set<df>({ df(100, 200) }));
        //contains
        BOOST_REQUIRE(LtpFragmentSet::ContainsFragmentEntirely(fragmentSet, df(100, 200)));
        BOOST_REQUIRE(LtpFragmentSet::ContainsFragmentEntirely(fragmentSet, df(101, 199)));
        //does not contain
        BOOST_REQUIRE(!LtpFragmentSet::ContainsFragmentEntirely(fragmentSet, df(10, 20)));
        BOOST_REQUIRE(!LtpFragmentSet::ContainsFragmentEntirely(fragmentSet, df(100, 201)));
        BOOST_REQUIRE(!LtpFragmentSet::ContainsFragmentEntirely(fragmentSet, df(100, 202)));
        BOOST_REQUIRE(!LtpFragmentSet::ContainsFragmentEntirely(fragmentSet, df(99, 200)));
        BOOST_REQUIRE(!LtpFragmentSet::ContainsFragmentEntirely(fragmentSet, df(98, 200)));
        BOOST_REQUIRE(!LtpFragmentSet::ContainsFragmentEntirely(fragmentSet, df(98, 150)));
        BOOST_REQUIRE(!LtpFragmentSet::ContainsFragmentEntirely(fragmentSet, df(150, 250)));

        fragmentSet.clear();
        LtpFragmentSet::InsertFragment(fragmentSet, df(0, 200));
        BOOST_REQUIRE(fragmentSet == std::set<df>({ df(0, 200) }));
        //contains
        BOOST_REQUIRE(LtpFragmentSet::ContainsFragmentEntirely(fragmentSet, df(0, 0)));
        BOOST_REQUIRE(LtpFragmentSet::ContainsFragmentEntirely(fragmentSet, df(100, 200)));
        BOOST_REQUIRE(LtpFragmentSet::ContainsFragmentEntirely(fragmentSet, df(200, 200)));
        BOOST_REQUIRE(LtpFragmentSet::ContainsFragmentEntirely(fragmentSet, df(101, 199)));
        //does not contain
        BOOST_REQUIRE(!LtpFragmentSet::ContainsFragmentEntirely(fragmentSet, df(199, 201)));
        BOOST_REQUIRE(!LtpFragmentSet::ContainsFragmentEntirely(fragmentSet, df(200, 201)));
        BOOST_REQUIRE(!LtpFragmentSet::ContainsFragmentEntirely(fragmentSet, df(201, 201)));

        fragmentSet.clear();
        LtpFragmentSet::InsertFragment(fragmentSet, df(100, 200));
        LtpFragmentSet::InsertFragment(fragmentSet, df(300, 400));
        BOOST_REQUIRE(fragmentSet == std::set<df>({ df(100, 200), df(300, 400) }));
        //contains
        BOOST_REQUIRE(LtpFragmentSet::ContainsFragmentEntirely(fragmentSet, df(100, 100)));
        BOOST_REQUIRE(LtpFragmentSet::ContainsFragmentEntirely(fragmentSet, df(100, 200)));
        BOOST_REQUIRE(LtpFragmentSet::ContainsFragmentEntirely(fragmentSet, df(101, 200)));
        BOOST_REQUIRE(LtpFragmentSet::ContainsFragmentEntirely(fragmentSet, df(100, 199)));
        BOOST_REQUIRE(LtpFragmentSet::ContainsFragmentEntirely(fragmentSet, df(101, 199)));
        BOOST_REQUIRE(LtpFragmentSet::ContainsFragmentEntirely(fragmentSet, df(200, 200)));
        BOOST_REQUIRE(LtpFragmentSet::ContainsFragmentEntirely(fragmentSet, df(300, 300)));
        BOOST_REQUIRE(LtpFragmentSet::ContainsFragmentEntirely(fragmentSet, df(300, 400)));
        BOOST_REQUIRE(LtpFragmentSet::ContainsFragmentEntirely(fragmentSet, df(301, 400)));
        BOOST_REQUIRE(LtpFragmentSet::ContainsFragmentEntirely(fragmentSet, df(300, 399)));
        BOOST_REQUIRE(LtpFragmentSet::ContainsFragmentEntirely(fragmentSet, df(400, 400)));
        BOOST_REQUIRE(LtpFragmentSet::ContainsFragmentEntirely(fragmentSet, df(301, 399)));
        //does not contain
        BOOST_REQUIRE(!LtpFragmentSet::ContainsFragmentEntirely(fragmentSet, df(0, 0)));
        BOOST_REQUIRE(!LtpFragmentSet::ContainsFragmentEntirely(fragmentSet, df(0, 99)));
        BOOST_REQUIRE(!LtpFragmentSet::ContainsFragmentEntirely(fragmentSet, df(0, 100)));
        BOOST_REQUIRE(!LtpFragmentSet::ContainsFragmentEntirely(fragmentSet, df(0, 101)));
        BOOST_REQUIRE(!LtpFragmentSet::ContainsFragmentEntirely(fragmentSet, df(0, 1000)));
        BOOST_REQUIRE(!LtpFragmentSet::ContainsFragmentEntirely(fragmentSet, df(201, 299)));
        BOOST_REQUIRE(!LtpFragmentSet::ContainsFragmentEntirely(fragmentSet, df(200, 300)));
        BOOST_REQUIRE(!LtpFragmentSet::ContainsFragmentEntirely(fragmentSet, df(201, 300)));
        BOOST_REQUIRE(!LtpFragmentSet::ContainsFragmentEntirely(fragmentSet, df(200, 299)));
        BOOST_REQUIRE(!LtpFragmentSet::ContainsFragmentEntirely(fragmentSet, df(401, 401)));
        BOOST_REQUIRE(!LtpFragmentSet::ContainsFragmentEntirely(fragmentSet, df(400, 401)));
        BOOST_REQUIRE(!LtpFragmentSet::ContainsFragmentEntirely(fragmentSet, df(300, 1000)));
        BOOST_REQUIRE(!LtpFragmentSet::ContainsFragmentEntirely(fragmentSet, df(299, 300)));
        BOOST_REQUIRE(!LtpFragmentSet::ContainsFragmentEntirely(fragmentSet, df(299, 400)));
        BOOST_REQUIRE(!LtpFragmentSet::ContainsFragmentEntirely(fragmentSet, df(299, 401)));
    }

    //TEST DoesNotContainFragmentEntirely (not used in ltp)
    {
        std::set<df> fragmentSet;
        LtpFragmentSet::InsertFragment(fragmentSet, df(100, 200));
        BOOST_REQUIRE(fragmentSet == std::set<df>({ df(100, 200) }));
        //overlap
        BOOST_REQUIRE(!LtpFragmentSet::DoesNotContainFragmentEntirely(fragmentSet, df(100, 200)));
        BOOST_REQUIRE(!LtpFragmentSet::DoesNotContainFragmentEntirely(fragmentSet, df(101, 199)));
        BOOST_REQUIRE(!LtpFragmentSet::DoesNotContainFragmentEntirely(fragmentSet, df(10, 100)));
        BOOST_REQUIRE(!LtpFragmentSet::DoesNotContainFragmentEntirely(fragmentSet, df(100, 100)));
        BOOST_REQUIRE(!LtpFragmentSet::DoesNotContainFragmentEntirely(fragmentSet, df(200, 200)));
        BOOST_REQUIRE(!LtpFragmentSet::DoesNotContainFragmentEntirely(fragmentSet, df(200, 300)));
        BOOST_REQUIRE(!LtpFragmentSet::DoesNotContainFragmentEntirely(fragmentSet, df(100, 201)));
        BOOST_REQUIRE(!LtpFragmentSet::DoesNotContainFragmentEntirely(fragmentSet, df(100, 202)));
        BOOST_REQUIRE(!LtpFragmentSet::DoesNotContainFragmentEntirely(fragmentSet, df(99, 200)));
        BOOST_REQUIRE(!LtpFragmentSet::DoesNotContainFragmentEntirely(fragmentSet, df(99, 100)));
        BOOST_REQUIRE(!LtpFragmentSet::DoesNotContainFragmentEntirely(fragmentSet, df(98, 200)));
        BOOST_REQUIRE(!LtpFragmentSet::DoesNotContainFragmentEntirely(fragmentSet, df(98, 150)));
        BOOST_REQUIRE(!LtpFragmentSet::DoesNotContainFragmentEntirely(fragmentSet, df(150, 250)));
        BOOST_REQUIRE(!LtpFragmentSet::DoesNotContainFragmentEntirely(fragmentSet, df(0, 1000)));
        //not contained (may abut)
        BOOST_REQUIRE(LtpFragmentSet::DoesNotContainFragmentEntirely(fragmentSet, df(10, 20)));        
        BOOST_REQUIRE(LtpFragmentSet::DoesNotContainFragmentEntirely(fragmentSet, df(10, 99)));
        BOOST_REQUIRE(LtpFragmentSet::DoesNotContainFragmentEntirely(fragmentSet, df(99, 99)));        
        BOOST_REQUIRE(LtpFragmentSet::DoesNotContainFragmentEntirely(fragmentSet, df(201, 201)));
        BOOST_REQUIRE(LtpFragmentSet::DoesNotContainFragmentEntirely(fragmentSet, df(201, 300)));

        fragmentSet.clear();
        LtpFragmentSet::InsertFragment(fragmentSet, df(0, 200));
        BOOST_REQUIRE(fragmentSet == std::set<df>({ df(0, 200) }));
        //overlap
        BOOST_REQUIRE(!LtpFragmentSet::DoesNotContainFragmentEntirely(fragmentSet, df(0, 199)));
        BOOST_REQUIRE(!LtpFragmentSet::DoesNotContainFragmentEntirely(fragmentSet, df(0, 200)));
        BOOST_REQUIRE(!LtpFragmentSet::DoesNotContainFragmentEntirely(fragmentSet, df(0, 201)));
        BOOST_REQUIRE(!LtpFragmentSet::DoesNotContainFragmentEntirely(fragmentSet, df(0, 0)));
        BOOST_REQUIRE(!LtpFragmentSet::DoesNotContainFragmentEntirely(fragmentSet, df(0, 1)));
        BOOST_REQUIRE(!LtpFragmentSet::DoesNotContainFragmentEntirely(fragmentSet, df(1, 199)));
        BOOST_REQUIRE(!LtpFragmentSet::DoesNotContainFragmentEntirely(fragmentSet, df(1, 200)));
        BOOST_REQUIRE(!LtpFragmentSet::DoesNotContainFragmentEntirely(fragmentSet, df(1, 201)));
        BOOST_REQUIRE(!LtpFragmentSet::DoesNotContainFragmentEntirely(fragmentSet, df(1, 1)));
        BOOST_REQUIRE(!LtpFragmentSet::DoesNotContainFragmentEntirely(fragmentSet, df(1, 2)));

        fragmentSet.clear();
        LtpFragmentSet::InsertFragment(fragmentSet, df(100, 200));
        LtpFragmentSet::InsertFragment(fragmentSet, df(300, 400));
        BOOST_REQUIRE(fragmentSet == std::set<df>({ df(100, 200), df(300, 400) }));
        //overlap
        BOOST_REQUIRE(!LtpFragmentSet::DoesNotContainFragmentEntirely(fragmentSet, df(0, 100)));
        BOOST_REQUIRE(!LtpFragmentSet::DoesNotContainFragmentEntirely(fragmentSet, df(0, 101)));
        BOOST_REQUIRE(!LtpFragmentSet::DoesNotContainFragmentEntirely(fragmentSet, df(0, 1000)));
        BOOST_REQUIRE(!LtpFragmentSet::DoesNotContainFragmentEntirely(fragmentSet, df(200, 300)));
        BOOST_REQUIRE(!LtpFragmentSet::DoesNotContainFragmentEntirely(fragmentSet, df(200, 299)));
        BOOST_REQUIRE(!LtpFragmentSet::DoesNotContainFragmentEntirely(fragmentSet, df(200, 200)));
        BOOST_REQUIRE(!LtpFragmentSet::DoesNotContainFragmentEntirely(fragmentSet, df(201, 300)));
        BOOST_REQUIRE(!LtpFragmentSet::DoesNotContainFragmentEntirely(fragmentSet, df(201, 301)));
        BOOST_REQUIRE(!LtpFragmentSet::DoesNotContainFragmentEntirely(fragmentSet, df(201, 1000)));
        BOOST_REQUIRE(!LtpFragmentSet::DoesNotContainFragmentEntirely(fragmentSet, df(400, 400)));
        BOOST_REQUIRE(!LtpFragmentSet::DoesNotContainFragmentEntirely(fragmentSet, df(400, 1000)));
        //not contained (may abut)
        BOOST_REQUIRE(LtpFragmentSet::DoesNotContainFragmentEntirely(fragmentSet, df(0, 0)));
        BOOST_REQUIRE(LtpFragmentSet::DoesNotContainFragmentEntirely(fragmentSet, df(0, 1)));
        BOOST_REQUIRE(LtpFragmentSet::DoesNotContainFragmentEntirely(fragmentSet, df(0, 99)));
        BOOST_REQUIRE(LtpFragmentSet::DoesNotContainFragmentEntirely(fragmentSet, df(201, 201)));
        BOOST_REQUIRE(LtpFragmentSet::DoesNotContainFragmentEntirely(fragmentSet, df(201, 299)));
        BOOST_REQUIRE(LtpFragmentSet::DoesNotContainFragmentEntirely(fragmentSet, df(299, 299)));
        BOOST_REQUIRE(LtpFragmentSet::DoesNotContainFragmentEntirely(fragmentSet, df(401, 401)));
        BOOST_REQUIRE(LtpFragmentSet::DoesNotContainFragmentEntirely(fragmentSet, df(401, 1000)));

        LtpFragmentSet::InsertFragment(fragmentSet, df(500, 600));
        BOOST_REQUIRE(fragmentSet == std::set<df>({ df(100, 200), df(300, 400), df(500, 600) }));
        //overlap 
        BOOST_REQUIRE(!LtpFragmentSet::DoesNotContainFragmentEntirely(fragmentSet, df(0, 100)));
        BOOST_REQUIRE(!LtpFragmentSet::DoesNotContainFragmentEntirely(fragmentSet, df(0, 101)));
        BOOST_REQUIRE(!LtpFragmentSet::DoesNotContainFragmentEntirely(fragmentSet, df(0, 1000)));
        BOOST_REQUIRE(!LtpFragmentSet::DoesNotContainFragmentEntirely(fragmentSet, df(200, 300)));
        BOOST_REQUIRE(!LtpFragmentSet::DoesNotContainFragmentEntirely(fragmentSet, df(200, 299)));
        BOOST_REQUIRE(!LtpFragmentSet::DoesNotContainFragmentEntirely(fragmentSet, df(200, 200)));
        BOOST_REQUIRE(!LtpFragmentSet::DoesNotContainFragmentEntirely(fragmentSet, df(201, 300)));
        BOOST_REQUIRE(!LtpFragmentSet::DoesNotContainFragmentEntirely(fragmentSet, df(201, 301)));
        BOOST_REQUIRE(!LtpFragmentSet::DoesNotContainFragmentEntirely(fragmentSet, df(201, 1000)));
        BOOST_REQUIRE(!LtpFragmentSet::DoesNotContainFragmentEntirely(fragmentSet, df(400, 400)));
        BOOST_REQUIRE(!LtpFragmentSet::DoesNotContainFragmentEntirely(fragmentSet, df(400, 1000)));
        BOOST_REQUIRE(!LtpFragmentSet::DoesNotContainFragmentEntirely(fragmentSet, df(400, 401)));
        BOOST_REQUIRE(!LtpFragmentSet::DoesNotContainFragmentEntirely(fragmentSet, df(401, 1000)));
        BOOST_REQUIRE(!LtpFragmentSet::DoesNotContainFragmentEntirely(fragmentSet, df(499, 500)));
        BOOST_REQUIRE(!LtpFragmentSet::DoesNotContainFragmentEntirely(fragmentSet, df(600, 601)));
        BOOST_REQUIRE(!LtpFragmentSet::DoesNotContainFragmentEntirely(fragmentSet, df(600, 600)));
        BOOST_REQUIRE(!LtpFragmentSet::DoesNotContainFragmentEntirely(fragmentSet, df(500, 500)));
        //not contained (may abut)
        BOOST_REQUIRE(LtpFragmentSet::DoesNotContainFragmentEntirely(fragmentSet, df(0, 0)));
        BOOST_REQUIRE(LtpFragmentSet::DoesNotContainFragmentEntirely(fragmentSet, df(0, 1)));
        BOOST_REQUIRE(LtpFragmentSet::DoesNotContainFragmentEntirely(fragmentSet, df(0, 99)));
        BOOST_REQUIRE(LtpFragmentSet::DoesNotContainFragmentEntirely(fragmentSet, df(201, 201)));
        BOOST_REQUIRE(LtpFragmentSet::DoesNotContainFragmentEntirely(fragmentSet, df(201, 299)));
        BOOST_REQUIRE(LtpFragmentSet::DoesNotContainFragmentEntirely(fragmentSet, df(299, 299)));
        BOOST_REQUIRE(LtpFragmentSet::DoesNotContainFragmentEntirely(fragmentSet, df(401, 401)));        
        BOOST_REQUIRE(LtpFragmentSet::DoesNotContainFragmentEntirely(fragmentSet, df(499, 499)));        
        BOOST_REQUIRE(LtpFragmentSet::DoesNotContainFragmentEntirely(fragmentSet, df(601, 601)));
        
    }


    //LARGE REPORT SEGMENTS NEEDING SPLIT UP
    {
        rs tooLargeReportSegment;
        const std::set<df> originalReceivedFragments({ df(10,19), df(30,39), df(50,59), df(65,69), df(75,89), df(100,109), df(120,129), df(140,149), df(160,169), df(180,189) });
        BOOST_REQUIRE(LtpFragmentSet::PopulateReportSegment(originalReceivedFragments, tooLargeReportSegment, 5));
        tooLargeReportSegment.upperBound = 6000; //increase upper bound
        BOOST_REQUIRE_EQUAL(tooLargeReportSegment, rs(0, 0, 6000, 5, std::vector<rc>({ rc(5,10), rc(25,10), rc(45,10), rc(60,5), rc(70,15), rc(95,10), rc(115,10), rc(135,10), rc(155,10), rc(175,10) })));

        //split size 1
        {
            std::vector<rs> reportSegmentsVec;
            BOOST_REQUIRE(LtpFragmentSet::SplitReportSegment(tooLargeReportSegment, reportSegmentsVec, 1));
            BOOST_REQUIRE_EQUAL(reportSegmentsVec.size(), 10);
            //for (std::size_t i = 0; i < reportSegmentsVec.size(); ++i) {
            //    std::cout << "rs: " << reportSegmentsVec[i] << std::endl;
            //}
            const std::vector<rs> expectedRsVec = {
                rs(0, 0, 20, 5, std::vector<rc>({ rc(5,10)})),
                rs(0, 0, 40, 20, std::vector<rc>({ rc(10,10)})),
                rs(0, 0, 60, 40, std::vector<rc>({ rc(10,10)})),
                rs(0, 0, 70, 60, std::vector<rc>({ rc(5,5)})),
                rs(0, 0, 90, 70, std::vector<rc>({ rc(5,15)})),
                rs(0, 0, 110, 90, std::vector<rc>({ rc(10,10)})),
                rs(0, 0, 130, 110, std::vector<rc>({ rc(10,10)})),
                rs(0, 0, 150, 130, std::vector<rc>({ rc(10,10)})),
                rs(0, 0, 170, 150, std::vector<rc>({ rc(10,10)})),
                rs(0, 0, 6000, 170, std::vector<rc>({ rc(10,10)}))
            };
            BOOST_REQUIRE(expectedRsVec == reportSegmentsVec);
            std::set<df> fragmentSet;
            for (std::size_t i = 0; i < reportSegmentsVec.size(); ++i) {
                LtpFragmentSet::AddReportSegmentToFragmentSet(fragmentSet, reportSegmentsVec[i]);
            }
            BOOST_REQUIRE(originalReceivedFragments == fragmentSet);
        }

        //split size 2
        {
            std::vector<rs> reportSegmentsVec;
            BOOST_REQUIRE(LtpFragmentSet::SplitReportSegment(tooLargeReportSegment, reportSegmentsVec, 2));
            BOOST_REQUIRE_EQUAL(reportSegmentsVec.size(), 5); //ceil(10/2)
            //for (std::size_t i = 0; i < reportSegmentsVec.size(); ++i) {
            //    std::cout << "rs: " << reportSegmentsVec[i] << std::endl;
            //}
            const std::vector<rs> expectedRsVec = {
                rs(0, 0, 40, 5, std::vector<rc>({ rc(5,10), rc(25,10)})),
                rs(0, 0, 70, 40, std::vector<rc>({ rc(10,10), rc(25,5)})),
                rs(0, 0, 110, 70, std::vector<rc>({ rc(5,15), rc(30,10)})),
                rs(0, 0, 150, 110, std::vector<rc>({ rc(10,10), rc(30,10)})),
                rs(0, 0, 6000, 150, std::vector<rc>({ rc(10,10), rc(30,10)}))
            };
            BOOST_REQUIRE(expectedRsVec == reportSegmentsVec);
            std::set<df> fragmentSet;
            for (std::size_t i = 0; i < reportSegmentsVec.size(); ++i) {
                LtpFragmentSet::AddReportSegmentToFragmentSet(fragmentSet, reportSegmentsVec[i]);
            }
            BOOST_REQUIRE(originalReceivedFragments == fragmentSet);
        }

        //split size 3
        {
            std::vector<rs> reportSegmentsVec;
            BOOST_REQUIRE(LtpFragmentSet::SplitReportSegment(tooLargeReportSegment, reportSegmentsVec, 3));
            BOOST_REQUIRE_EQUAL(reportSegmentsVec.size(), 4); //ceil(10/3)
            //for (std::size_t i = 0; i < reportSegmentsVec.size(); ++i) {
            //    std::cout << "rs: " << reportSegmentsVec[i] << std::endl;
            //}
            const std::vector<rs> expectedRsVec = {
                rs(0, 0, 60, 5, std::vector<rc>({ rc(5,10), rc(25,10), rc(45,10)})),
                rs(0, 0, 110, 60, std::vector<rc>({ rc(5,5), rc(15,15), rc(40,10)})),
                rs(0, 0, 170, 110, std::vector<rc>({ rc(10,10), rc(30,10), rc(50,10)})),
                rs(0, 0, 6000, 170, std::vector<rc>({ rc(10,10)}))
            };
            BOOST_REQUIRE(expectedRsVec == reportSegmentsVec);
            std::set<df> fragmentSet;
            for (std::size_t i = 0; i < reportSegmentsVec.size(); ++i) {
                LtpFragmentSet::AddReportSegmentToFragmentSet(fragmentSet, reportSegmentsVec[i]);
            }
            BOOST_REQUIRE(originalReceivedFragments == fragmentSet);
        }

        //split size 4
        {
            std::vector<rs> reportSegmentsVec;
            BOOST_REQUIRE(LtpFragmentSet::SplitReportSegment(tooLargeReportSegment, reportSegmentsVec, 4));
            BOOST_REQUIRE_EQUAL(reportSegmentsVec.size(), 3); //ceil(10/4)
            //for (std::size_t i = 0; i < reportSegmentsVec.size(); ++i) {
            //    std::cout << "rs: " << reportSegmentsVec[i] << std::endl;
            //}
            const std::vector<rs> expectedRsVec = {
                rs(0, 0, 70, 5, std::vector<rc>({ rc(5,10), rc(25,10), rc(45,10), rc(60,5)})),
                rs(0, 0, 150, 70, std::vector<rc>({ rc(5,15), rc(30,10), rc(50,10), rc(70,10)})),
                rs(0, 0, 6000, 150, std::vector<rc>({ rc(10,10), rc(30,10)}))
            };
            BOOST_REQUIRE(expectedRsVec == reportSegmentsVec);
            std::set<df> fragmentSet;
            for (std::size_t i = 0; i < reportSegmentsVec.size(); ++i) {
                LtpFragmentSet::AddReportSegmentToFragmentSet(fragmentSet, reportSegmentsVec[i]);
            }
            BOOST_REQUIRE(originalReceivedFragments == fragmentSet);
        }

        //split size 5
        {
            std::vector<rs> reportSegmentsVec;
            BOOST_REQUIRE(LtpFragmentSet::SplitReportSegment(tooLargeReportSegment, reportSegmentsVec, 5));
            BOOST_REQUIRE_EQUAL(reportSegmentsVec.size(), 2); //ceil(10/5)
            //for (std::size_t i = 0; i < reportSegmentsVec.size(); ++i) {
            //    std::cout << "rs: " << reportSegmentsVec[i] << std::endl;
            //}
            const std::vector<rs> expectedRsVec = {
                rs(0, 0, 90, 5, std::vector<rc>({ rc(5,10), rc(25,10), rc(45,10), rc(60,5), rc(70,15) })),
                rs(0, 0, 6000, 90, std::vector<rc>({ rc(10,10), rc(30,10), rc(50,10), rc(70,10), rc(90,10)}))
            };
            BOOST_REQUIRE(expectedRsVec == reportSegmentsVec);
            std::set<df> fragmentSet;
            for (std::size_t i = 0; i < reportSegmentsVec.size(); ++i) {
                LtpFragmentSet::AddReportSegmentToFragmentSet(fragmentSet, reportSegmentsVec[i]);
            }
            BOOST_REQUIRE(originalReceivedFragments == fragmentSet);
        }

        //split size 6
        {
            std::vector<rs> reportSegmentsVec;
            BOOST_REQUIRE(LtpFragmentSet::SplitReportSegment(tooLargeReportSegment, reportSegmentsVec, 6));
            BOOST_REQUIRE_EQUAL(reportSegmentsVec.size(), 2); //ceil(10/6)
            //for (std::size_t i = 0; i < reportSegmentsVec.size(); ++i) {
            //    std::cout << "rs: " << reportSegmentsVec[i] << std::endl;
            //}
            const std::vector<rs> expectedRsVec = {
                rs(0, 0, 110, 5, std::vector<rc>({ rc(5,10), rc(25,10), rc(45,10), rc(60,5), rc(70,15), rc(95,10) })),
                rs(0, 0, 6000, 110, std::vector<rc>({ rc(10,10), rc(30,10), rc(50,10), rc(70,10)}))
            };            
            BOOST_REQUIRE(expectedRsVec == reportSegmentsVec);
            std::set<df> fragmentSet;
            for (std::size_t i = 0; i < reportSegmentsVec.size(); ++i) {
                LtpFragmentSet::AddReportSegmentToFragmentSet(fragmentSet, reportSegmentsVec[i]);
            }
            BOOST_REQUIRE(originalReceivedFragments == fragmentSet);
        }

        //split size 10
        {
            std::vector<rs> reportSegmentsVec;
            BOOST_REQUIRE(LtpFragmentSet::SplitReportSegment(tooLargeReportSegment, reportSegmentsVec, 10));
            BOOST_REQUIRE_EQUAL(reportSegmentsVec.size(), 1); //ceil(10/10)
            //for (std::size_t i = 0; i < reportSegmentsVec.size(); ++i) {
            //    std::cout << "rs: " << reportSegmentsVec[i] << std::endl;
            //}
            const std::vector<rs> expectedRsVec = {
                rs(0, 0, 6000, 5, std::vector<rc>({ rc(5,10), rc(25,10), rc(45,10), rc(60,5), rc(70,15), rc(95,10), rc(115,10), rc(135,10), rc(155,10), rc(175,10)} ))
            };
            BOOST_REQUIRE(expectedRsVec == reportSegmentsVec);
            std::set<df> fragmentSet;
            for (std::size_t i = 0; i < reportSegmentsVec.size(); ++i) {
                LtpFragmentSet::AddReportSegmentToFragmentSet(fragmentSet, reportSegmentsVec[i]);
            }
            BOOST_REQUIRE(originalReceivedFragments == fragmentSet);
        }
    }
}
