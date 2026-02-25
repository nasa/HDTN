/**
 * @file RouterTests.cpp
 * @author Ethan Schweinsberg <ethan.e.schweinsberg@nasa.gov>
 *
 * @copyright Copyright (c) 2026 United States Government as represented by
 * the National Aeronautics and Space Administration.
 * No copyright is claimed in the United States under Title 17, U.S.Code.
 * All Other Rights Reserved.
 *
 * @section LICENSE
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "JsonSerializable.h"
#include "router.h"

#include <boost/test/unit_test.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/foreach.hpp>

BOOST_AUTO_TEST_CASE(RouterGetRateBpsTestCase)
{
    // It's compatible with the deprecated rate field
    std::string message = "[{\"rate\": 20}]";
    boost::property_tree::ptree pt;
    bool success = JsonSerializable::GetPropertyTreeFromJsonCharArray(&(message[0]), message.size(), pt); //message.data() is const in C++11
    BOOST_REQUIRE_EQUAL(success, true);

    BOOST_FOREACH(const boost::property_tree::ptree::value_type & eventPt, pt) {
        uint64_t rate = Router::GetRateBpsFromPtree(eventPt);
        BOOST_REQUIRE_EQUAL(rate, 20000000);
    }

    // It's compatible with the new rateBps field
    message = "[{\"rateBitsPerSec\": 20000000}]";
    success = JsonSerializable::GetPropertyTreeFromJsonCharArray(&(message[0]), message.size(), pt); //message.data() is const in C++11
    BOOST_REQUIRE_EQUAL(success, true);

    BOOST_FOREACH(const boost::property_tree::ptree::value_type & eventPt, pt) {
        uint64_t rate = Router::GetRateBpsFromPtree(eventPt);
        BOOST_REQUIRE_EQUAL(rate, 20000000);
    }

    // It prefers the new rateBps field
    message = "[{\"rateBitsPerSec\": 20000000, \"rate\": 40}]";
    success = JsonSerializable::GetPropertyTreeFromJsonCharArray(&(message[0]), message.size(), pt); //message.data() is const in C++11
    BOOST_REQUIRE_EQUAL(success, true);

    BOOST_FOREACH(const boost::property_tree::ptree::value_type & eventPt, pt) {
        uint64_t rate = Router::GetRateBpsFromPtree(eventPt);
        BOOST_REQUIRE_EQUAL(rate, 20000000);
    }
}
