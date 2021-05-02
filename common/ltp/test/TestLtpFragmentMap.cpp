#include <boost/test/unit_test.hpp>
#include "LtpFragmentMap.h"
#include <boost/bind.hpp>

BOOST_AUTO_TEST_CASE(LtpFragmentMapTestCase)
{
    typedef LtpFragmentMap::data_fragment_t df;
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
        LtpFragmentMap::InsertFragment(fragmentSet, df(100, 200));
        BOOST_REQUIRE(fragmentSet == std::set<df>({ df(100, 200) }));
        LtpFragmentMap::InsertFragment(fragmentSet, df(300, 400));
        BOOST_REQUIRE(fragmentSet == std::set<df>({ df(100, 200), df(300, 400) }));
        LtpFragmentMap::InsertFragment(fragmentSet, df(99, 200));
        BOOST_REQUIRE(fragmentSet == std::set<df>({ df(99, 200), df(300, 400) }));
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
}
