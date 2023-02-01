/**
 * @file TestUtf8Paths.cpp
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
        if (sizeof(boost::filesystem::path::value_type) == 2) { //windows wchar_t
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

