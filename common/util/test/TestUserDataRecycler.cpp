/**
 * @file TestUserDataRecycler.cpp
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
#include <list>
#include <forward_list>
#include "UserDataRecycler.h"


BOOST_AUTO_TEST_CASE(UserDataRecyclerTestCase)
{
    UserDataRecyclerVecUint8 udr(5);
    BOOST_REQUIRE_EQUAL(udr.GetListSize(), 0);
    BOOST_REQUIRE_EQUAL(udr.GetListCapacity(), 5);
    { //try get data from empty recycler fail
        std::vector<uint8_t> udReturned;

        udr.GetRecycledOrCreateNewUserData(udReturned);
        BOOST_REQUIRE_EQUAL(udReturned.size(), 0);
        BOOST_REQUIRE_EQUAL(udReturned.capacity(), 0);
        //still unchanged
        BOOST_REQUIRE_EQUAL(udr.GetListSize(), 0);
        BOOST_REQUIRE_EQUAL(udr.GetListCapacity(), 5);
    }
    { //try add empty data and fail
        std::vector<uint8_t> ud, udReturned;
        BOOST_REQUIRE_EQUAL(ud.size(), 0);
        BOOST_REQUIRE_EQUAL(ud.capacity(), 0);
        BOOST_REQUIRE(!udr.ReturnUserData(std::move(ud))); //fail
        //no change
        BOOST_REQUIRE_EQUAL(udr.GetListSize(), 0);
        BOOST_REQUIRE_EQUAL(udr.GetListCapacity(), 5);

        udr.GetRecycledOrCreateNewUserData(udReturned);
        BOOST_REQUIRE_EQUAL(udReturned.size(), 0);
        BOOST_REQUIRE_EQUAL(udReturned.capacity(), 0);
        //still unchanged
        BOOST_REQUIRE_EQUAL(udr.GetListSize(), 0);
        BOOST_REQUIRE_EQUAL(udr.GetListCapacity(), 5);
    }
    { //add empty data but has reserved space and succeed
        std::vector<uint8_t> ud, udReturned;
        ud.reserve(100);
        BOOST_REQUIRE_EQUAL(ud.size(), 0);
        BOOST_REQUIRE_NE(ud.capacity(), 0);
        BOOST_REQUIRE_GE(ud.capacity(), 100);
        BOOST_REQUIRE(udr.ReturnUserData(std::move(ud))); //success
        //changed
        BOOST_REQUIRE_EQUAL(udr.GetListSize(), 1);
        BOOST_REQUIRE_EQUAL(udr.GetListCapacity(), 5);

        udr.GetRecycledOrCreateNewUserData(udReturned);
        BOOST_REQUIRE_EQUAL(udReturned.size(), 0);
        BOOST_REQUIRE_NE(udReturned.capacity(), 0);
        BOOST_REQUIRE_GE(udReturned.capacity(), 100);
        //decrease
        BOOST_REQUIRE_EQUAL(udr.GetListSize(), 0);
        BOOST_REQUIRE_EQUAL(udr.GetListCapacity(), 5);
    }
    { //add data with size() and succeed
        std::vector<uint8_t> ud, udReturned;
        ud.resize(100);
        BOOST_REQUIRE_EQUAL(ud.size(), 100);
        BOOST_REQUIRE_NE(ud.capacity(), 0);
        BOOST_REQUIRE_GE(ud.capacity(), 100);
        BOOST_REQUIRE(udr.ReturnUserData(std::move(ud))); //success
        //changed
        BOOST_REQUIRE_EQUAL(udr.GetListSize(), 1);
        BOOST_REQUIRE_EQUAL(udr.GetListCapacity(), 5);

        udr.GetRecycledOrCreateNewUserData(udReturned);
        BOOST_REQUIRE_EQUAL(udReturned.size(), 100);
        BOOST_REQUIRE_NE(udReturned.capacity(), 0);
        BOOST_REQUIRE_GE(udReturned.capacity(), 100);
        //decrease
        BOOST_REQUIRE_EQUAL(udr.GetListSize(), 0);
        BOOST_REQUIRE_EQUAL(udr.GetListCapacity(), 5);
    }
    for (unsigned int i = 0; i < 5; ++i) {
        //add data with size() and succeed
        std::vector<uint8_t> ud;
        ud.resize(100 + i);
        BOOST_REQUIRE_EQUAL(ud.size(), 100 + i);
        BOOST_REQUIRE_NE(ud.capacity(), 0);
        BOOST_REQUIRE_GE(ud.capacity(), 100 + i);
        BOOST_REQUIRE(udr.ReturnUserData(std::move(ud))); //success
        //changed
        BOOST_REQUIRE_EQUAL(udr.GetListSize(), i + 1);
        BOOST_REQUIRE_EQUAL(udr.GetListCapacity(), 5);
    }
    for (unsigned int i = 5; i < 10; ++i) {
        //add data with size() and fail because list full
        std::vector<uint8_t> ud;
        ud.resize(100 + i);
        BOOST_REQUIRE_EQUAL(ud.size(), 100 + i);
        BOOST_REQUIRE_NE(ud.capacity(), 0);
        BOOST_REQUIRE_GE(ud.capacity(), 100 + i);
        BOOST_REQUIRE(!udr.ReturnUserData(std::move(ud))); //fail
        //unchanged
        BOOST_REQUIRE_EQUAL(udr.GetListSize(), 5);
        BOOST_REQUIRE_EQUAL(udr.GetListCapacity(), 5);
    }
    for (int i = 4; i >= 0; --i) { //underlying forward_list is FILO
        //get data with size() and succeed
        std::vector<uint8_t> udReturned;
        udr.GetRecycledOrCreateNewUserData(udReturned);
        BOOST_REQUIRE_EQUAL(udReturned.size(), 100 + i);
        BOOST_REQUIRE_NE(udReturned.capacity(), 0);
        BOOST_REQUIRE_GE(udReturned.capacity(), 100 + i);
        //decrease
        BOOST_REQUIRE_EQUAL(udr.GetListSize(), i);
        BOOST_REQUIRE_EQUAL(udr.GetListCapacity(), 5);
    }
    //verify empty
    BOOST_REQUIRE_EQUAL(udr.GetListSize(), 0);
    BOOST_REQUIRE_EQUAL(udr.GetListCapacity(), 5);
}

