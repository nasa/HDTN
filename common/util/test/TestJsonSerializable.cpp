/**
 * @file TestJsonSerializable.cpp
 * @author  Brian Tomko <brian.j.tomko@nasa.gov>
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

#include <boost/test/unit_test.hpp>
#include "JsonSerializable.h"



BOOST_AUTO_TEST_CASE(JsonSerializableTestCase)
{
    //UTF-8 (Hebrew characters): shalom is \xd7\xa9\xd7\x9c\xd7\x95\xd7\x9d
    #define UTF_8_SAMPLE_STR "\xd7\xa9\xd7\x9c\xd7\x95\xd7\x9d.txt"
    #define UTF_8_SAMPLE_KEY "\xd7\xa9\xd7\x9c\xd7\x95\xd7\x9d_key"
    static const char* const jsonText =
    "{"
        "\"mybool1\":true,"
        "\"mybool2\":false,"
        "\"mystr\":\"test\","
        "\"myutf8str\":\"" UTF_8_SAMPLE_STR "\","
        "\"" UTF_8_SAMPLE_KEY "\":\"nonUtfStr\","
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
        BOOST_REQUIRE_EQUAL(pt.get<std::string>("myutf8str", ""), UTF_8_SAMPLE_STR);
        BOOST_REQUIRE_EQUAL(pt.get<std::string>(UTF_8_SAMPLE_KEY, ""), "nonUtfStr");
        BOOST_REQUIRE_EQUAL(pt.get<int>("myint", 100), -3);
        BOOST_REQUIRE_EQUAL(pt.get<unsigned int>("myuint", 100), 10);
        BOOST_REQUIRE_EQUAL(pt.get<std::string>("myurl", ""), "https://www.nasa.gov/");
    }

    //test GetAllJsonKeys regex, a precondition to finding duplicates
    {
        std::set<std::string> jsonKeys; //support out of order
        JsonSerializable::GetAllJsonKeys(jsonTextStr, jsonKeys);
        BOOST_REQUIRE(jsonKeys == std::set<std::string>({ "mybool1", "mybool2", "mystr", "myutf8str", UTF_8_SAMPLE_KEY, "myint", "myuint", "myurl" }));
        //test without having to load entire file
        std::set<std::string> jsonKeys2;
        std::istringstream iss(jsonTextStr);
        JsonSerializable::GetAllJsonKeysLineByLine(iss, jsonKeys2);
        BOOST_REQUIRE(jsonKeys == jsonKeys2);
    }
}
