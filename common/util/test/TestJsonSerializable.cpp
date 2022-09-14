/**
 * @file TestJsonSerializable.cpp
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
#include "JsonSerializable.h"



BOOST_AUTO_TEST_CASE(JsonSerializableTestCase)
{
    const char* jsonText =
    "{"
        "\"mybool1\":true,"
        "\"mybool2\":false,"
        "\"mystr\":\"test\","
        "\"myint\":-3,"
        "\"myuint\":10"
    "}\n";

    boost::property_tree::ptree pt = JsonSerializable::GetPropertyTreeFromCharArray((char*)jsonText, strlen(jsonText));
    BOOST_REQUIRE_EQUAL(pt.get<bool>("mybool1", false), true);
    BOOST_REQUIRE_EQUAL(pt.get<bool>("mybool2", true), false);
    BOOST_REQUIRE_EQUAL(pt.get<std::string>("mystr", ""), "test");
    BOOST_REQUIRE_EQUAL(pt.get<int>("myint", 100), -3);
    BOOST_REQUIRE_EQUAL(pt.get<unsigned int>("myuint", 100), 10);
}
