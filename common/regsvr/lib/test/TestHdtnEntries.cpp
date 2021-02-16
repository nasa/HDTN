#include <boost/test/unit_test.hpp>
#include "reg.hpp"
#include <boost/make_shared.hpp>

BOOST_AUTO_TEST_CASE(HdtnEntriesTestCase)
{
    hdtn::HdtnEntries_ptr entries1 = boost::make_shared< hdtn::HdtnEntries>();
    entries1->AddEntry("myprot1", "myaddr1", "mytype1", 11, "mymode1");
    entries1->AddEntry("myprot2", "myaddr2", "mytype2", 12, "mymode2");


    hdtn::HdtnEntries_ptr entries1_copy = boost::make_shared< hdtn::HdtnEntries>();
    entries1_copy->AddEntry("myprot1", "myaddr1", "mytype1", 11, "mymode1");
    entries1_copy->AddEntry("myprot2", "myaddr2", "mytype2", 12, "mymode2");

    hdtn::HdtnEntries_ptr entries2 = boost::make_shared< hdtn::HdtnEntries>();
    entries2->AddEntry("myprot3", "myaddr1", "mytype1", 11, "mymode1");
    entries2->AddEntry("myprot4", "myaddr2", "mytype2", 12, "mymode2");

	//1, 2, 3 all combinations
    BOOST_REQUIRE(*entries1 == *entries1_copy);
    BOOST_REQUIRE(!(*entries1 == *entries2));

    std::string entries1Json = entries1->ToJson();
    //std::cout << entries1Json << "\n";
    hdtn::HdtnEntries_ptr entries1_fromJson = hdtn::HdtnEntries::CreateFromJson(entries1Json);
    BOOST_REQUIRE(entries1_fromJson); //not null
    BOOST_REQUIRE(*entries1 == *entries1_fromJson);
    BOOST_REQUIRE(entries1Json == entries1_fromJson->ToJson());

    BOOST_REQUIRE_EQUAL(entries1_fromJson->m_hdtnEntryList.front().protocol, "myprot1");
    BOOST_REQUIRE_EQUAL(entries1_fromJson->m_hdtnEntryList.front().address, "myaddr1");
    BOOST_REQUIRE_EQUAL(entries1_fromJson->m_hdtnEntryList.front().type, "mytype1");
    BOOST_REQUIRE_EQUAL(entries1_fromJson->m_hdtnEntryList.front().port, 11);
    BOOST_REQUIRE_EQUAL(entries1_fromJson->m_hdtnEntryList.front().mode, "mymode1");

    BOOST_REQUIRE_EQUAL(entries1_fromJson->m_hdtnEntryList.back().protocol, "myprot2");
    BOOST_REQUIRE_EQUAL(entries1_fromJson->m_hdtnEntryList.back().address, "myaddr2");
    BOOST_REQUIRE_EQUAL(entries1_fromJson->m_hdtnEntryList.back().type, "mytype2");
    BOOST_REQUIRE_EQUAL(entries1_fromJson->m_hdtnEntryList.back().port, 12);
    BOOST_REQUIRE_EQUAL(entries1_fromJson->m_hdtnEntryList.back().mode, "mymode2");
	
}

