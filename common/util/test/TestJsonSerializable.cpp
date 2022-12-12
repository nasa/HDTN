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
    static const char* jsonText =
    "{"
        "\"mybool1\":true,"
        "\"mybool2\":false,"
        "\"mystr\":\"test\","
        "\"myint\":-3,\"myuint\"  :  10,"
        "\"myurl\":    \"https://www.nasa.gov/\""
    "}\n";

    static const std::string jsonTextStr(jsonText);

    {
        boost::property_tree::ptree pt;
        BOOST_REQUIRE(JsonSerializable::GetPropertyTreeFromJsonCharArray((char*)jsonText, strlen(jsonText), pt));
        BOOST_REQUIRE_EQUAL(pt.get<bool>("mybool1", false), true);
        BOOST_REQUIRE_EQUAL(pt.get<bool>("mybool2", true), false);
        BOOST_REQUIRE_EQUAL(pt.get<std::string>("mystr", ""), "test");
        BOOST_REQUIRE_EQUAL(pt.get<int>("myint", 100), -3);
        BOOST_REQUIRE_EQUAL(pt.get<unsigned int>("myuint", 100), 10);
        BOOST_REQUIRE_EQUAL(pt.get<std::string>("myurl", ""), "https://www.nasa.gov/");
    }

    //test GetAllJsonKeys regex, a precondition to finding duplicates
    {
        std::set<std::string> jsonKeys; //support out of order
        JsonSerializable::GetAllJsonKeys(jsonTextStr, jsonKeys);
        BOOST_REQUIRE(jsonKeys == std::set<std::string>({ "mybool1", "mybool2", "mystr", "myint", "myuint", "myurl" }));
        //test without having to load entire file
        std::set<std::string> jsonKeys2;
        std::istringstream iss(jsonTextStr);
        JsonSerializable::GetAllJsonKeysLineByLine(iss, jsonKeys2);
        BOOST_REQUIRE(jsonKeys == jsonKeys2);
    }
}
