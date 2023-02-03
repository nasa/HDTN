/**
 * @file SchedulerTests.cpp
 * @author Ethan Schweinsberg <ethan.e.schweinsberg@nasa.gov>
 *
 * @copyright Copyright © 2023 United States Government as represented by
 * the National Aeronautics and Space Administration.
 * No copyright is claimed in the United States under Title 17, U.S.Code.
 * All Other Rights Reserved.
 *
 * @section LICENSE
 * Released under the NASA Open Source Agreement (NOSA)
 * See LICENSE.md in the source root directory for more information.
 */

#include "JsonSerializable.h"
#include "scheduler.h"

#include <boost/test/unit_test.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/foreach.hpp>

BOOST_AUTO_TEST_CASE(SchedulerGetRateBpsTestCase)
{
    // It's compatible with the deprecated rate field
    std::string message = "[{\"rate\": 20}]";
    boost::property_tree::ptree pt;
    bool success = JsonSerializable::GetPropertyTreeFromJsonCharArray(message.data(), message.size(), pt);
    BOOST_REQUIRE_EQUAL(success, true);

    BOOST_FOREACH(const boost::property_tree::ptree::value_type & eventPt, pt) {
        uint64_t rate = Scheduler::GetRateBpsFromPtree(eventPt);
        BOOST_REQUIRE_EQUAL(rate, 20000000);
    }

    // It's compatible with the new rateBps field
    message = "[{\"rateBps\": 20000000}]";
    success = JsonSerializable::GetPropertyTreeFromJsonCharArray(message.data(), message.size(), pt);
    BOOST_REQUIRE_EQUAL(success, true);

    BOOST_FOREACH(const boost::property_tree::ptree::value_type & eventPt, pt) {
        uint64_t rate = Scheduler::GetRateBpsFromPtree(eventPt);
        BOOST_REQUIRE_EQUAL(rate, 20000000);
    }

    // It prefers the new rateBps field
    message = "[{\"rateBps\": 20000000, \"rate\": 40}]";
    success = JsonSerializable::GetPropertyTreeFromJsonCharArray(message.data(), message.size(), pt);
    BOOST_REQUIRE_EQUAL(success, true);

    BOOST_FOREACH(const boost::property_tree::ptree::value_type & eventPt, pt) {
        uint64_t rate = Scheduler::GetRateBpsFromPtree(eventPt);
        BOOST_REQUIRE_EQUAL(rate, 20000000);
    }
}
