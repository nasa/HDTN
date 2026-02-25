/**
 * @file TestUtf8Paths.cpp
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
#include "Utf8Paths.h"
#include <boost/filesystem/fstream.hpp>


BOOST_AUTO_TEST_CASE(Utf8PathsTestCase)
{
    { //UTF-8 (Hebrew characters)
        //is \xd7\xa9\xd7\x9c\xd7\x95\xd7\x9d
        std::string shalomUtf8Str({ '\xd7', '\xa9', '\xd7', '\x9c', '\xd7', '\x95', '\xd7', '\x9d', '.', 't', 'x', 't' });
        //for (int i = 0; i < shalomUtf8Str.size(); ++i) {
        //    printf("%x\n", (unsigned char)shalomUtf8Str[i]);
        //}
        boost::filesystem::path shalomPath(Utf8Paths::Utf8StringToPath(shalomUtf8Str));
        //std::cout << "sp " << shalomUtf8Str << "\n";
        BOOST_REQUIRE_EQUAL(shalomUtf8Str.size(), 12);
        if 
#if (__cplusplus >= 201703L)
            constexpr
#endif
            (sizeof(boost::filesystem::path::value_type) == 2) { //windows wchar_t
            BOOST_REQUIRE_EQUAL(shalomPath.size(), 8);
        }
        else { //linux char
            BOOST_REQUIRE_EQUAL(shalomPath.size(), 12);
        }
        BOOST_REQUIRE(!Utf8Paths::IsAscii(shalomUtf8Str));
        //boost::filesystem::ofstream ofs(shalomPath);
        std::string shalomUtf8StrDecoded(Utf8Paths::PathToUtf8String(shalomPath));
        BOOST_REQUIRE(shalomUtf8Str == shalomUtf8StrDecoded);
    }
    { //ascii character path
        std::string helloUtf8Str("hello.txt");
        boost::filesystem::path helloPath(Utf8Paths::Utf8StringToPath(helloUtf8Str));
        BOOST_REQUIRE_EQUAL(helloUtf8Str.size(), 9);
        BOOST_REQUIRE_EQUAL(helloPath.size(), 9);
        BOOST_REQUIRE(Utf8Paths::IsAscii(helloUtf8Str));
        std::string helloUtf8StrDecoded(Utf8Paths::PathToUtf8String(helloPath));
        BOOST_REQUIRE(helloUtf8Str == helloUtf8StrDecoded);
    }
}

