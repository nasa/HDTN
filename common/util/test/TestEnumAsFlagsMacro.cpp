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

