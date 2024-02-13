/**
 * @file TestEnumAsFlagsMacro.cpp
 * @author  Brian Tomko <brian.j.tomko@nasa.gov>
 *
 * @copyright Copyright (c) 2021 United States Government as represented by
 * the National Aeronautics and Space Administration.
 * No copyright is claimed in the United States under Title 17, U.S.Code.
 * All Other Rights Reserved.
 *
 * @section LICENSE
 * Released under the NASA Open Source Agreement (NOSA)
 * See LICENSE.md in the source root directory for more information.
 */

#include <boost/test/unit_test.hpp>
#include "EnumAsFlagsMacro.h"

enum class TestFlags : uint64_t {
    none = 0,
    flag0 = 1 << 0,
    flag1 = 1 << 1,
    flag2 = 1 << 2,
    flag3 = 1 << 3,
    flag4 = 1 << 4,
    flag5 = 1 << 5,
    flag6 = 1 << 6,
    flag7 = 1 << 7,
    flag8 = 1 << 8,
    flag9 = 1 << 9,
    flag10 = 1 << 10
};
MAKE_ENUM_SUPPORT_FLAG_OPERATORS(TestFlags);
MAKE_ENUM_SUPPORT_OSTREAM_OPERATOR(TestFlags);

BOOST_AUTO_TEST_CASE(EnumAsFlagsMacroTestCase)
{
    BOOST_REQUIRE_EQUAL(sizeof(std::underlying_type<TestFlags>::type), 8);
        
    TestFlags f = TestFlags::none;
    BOOST_REQUIRE_EQUAL(f, TestFlags::none);
    BOOST_REQUIRE_NE(f, TestFlags::flag0);
    BOOST_REQUIRE_EQUAL(f | TestFlags::flag0, TestFlags::flag0);
    f |= TestFlags::flag10;
    BOOST_REQUIRE_NE(f, TestFlags::none);
    BOOST_REQUIRE_EQUAL(f, TestFlags::flag10);
    BOOST_REQUIRE_EQUAL(f | TestFlags::flag0, TestFlags::flag10 | TestFlags::flag0);
    f |= TestFlags::flag0;
    f |= TestFlags::flag1;
    f |= TestFlags::flag2;
    BOOST_REQUIRE_EQUAL(f, TestFlags::flag0 | TestFlags::flag1 | TestFlags::flag2 | TestFlags::flag10);
    f &= TestFlags::flag1 | TestFlags::flag2 | TestFlags::flag3;
    BOOST_REQUIRE_EQUAL(f, TestFlags::flag1 | TestFlags::flag2);
    f = f | TestFlags::flag0;
    f = f | TestFlags::flag10;
    BOOST_REQUIRE_EQUAL(f, TestFlags::flag0 | TestFlags::flag1 | TestFlags::flag2 | TestFlags::flag10);
    f &= ~TestFlags::flag10;
    BOOST_REQUIRE_EQUAL(f, TestFlags::flag0 | TestFlags::flag1 | TestFlags::flag2);
    f ^= TestFlags::flag10;
    BOOST_REQUIRE_EQUAL(f, TestFlags::flag0 | TestFlags::flag1 | TestFlags::flag2 | TestFlags::flag10);
    f ^= TestFlags::flag10;
    BOOST_REQUIRE_EQUAL(f, TestFlags::flag0 | TestFlags::flag1 | TestFlags::flag2);
    BOOST_REQUIRE_EQUAL(TestFlags::flag0 & TestFlags::flag0, TestFlags::flag0);
    BOOST_REQUIRE_EQUAL(TestFlags::flag0 & TestFlags::flag1, TestFlags::none);
    BOOST_REQUIRE_EQUAL(TestFlags::flag0 ^ TestFlags::flag0, TestFlags::none);
    BOOST_REQUIRE_EQUAL(TestFlags::flag0 ^ TestFlags::flag0 ^ TestFlags::flag0, TestFlags::flag0);
    BOOST_REQUIRE_EQUAL(TestFlags::flag0 ^ TestFlags::flag1, TestFlags::flag0 | TestFlags::flag1);
}

