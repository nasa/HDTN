#include <boost/test/unit_test.hpp>
#include "TokenRateLimiter.h"

BOOST_AUTO_TEST_CASE(TokenRateLimiterLowRate)
{
    TokenRateLimiter limiter;
    limiter.SetRate(
        50, // 20ms per token
        boost::posix_time::seconds(1),
        boost::posix_time::milliseconds(100)
    );

    // deplete the tokens
    BOOST_REQUIRE_EQUAL(limiter.GetRemainingTokens(), 5);
    BOOST_REQUIRE(limiter.HasFullBucketOfTokens());
    BOOST_REQUIRE(limiter.TakeTokens(3));
    BOOST_REQUIRE_EQUAL(limiter.GetRemainingTokens(), 2);
    BOOST_REQUIRE(!limiter.HasFullBucketOfTokens());
    BOOST_REQUIRE(!limiter.TakeTokens(3));
    BOOST_REQUIRE_EQUAL(limiter.GetRemainingTokens(), 2);
    BOOST_REQUIRE(limiter.TakeTokens(2));
    BOOST_REQUIRE_EQUAL(limiter.GetRemainingTokens(), 0);

    // add fractional times until fully available
    limiter.AddTime(boost::posix_time::milliseconds(10));
    BOOST_REQUIRE_EQUAL(limiter.GetRemainingTokens(), 0);
    limiter.AddTime(boost::posix_time::milliseconds(8));
    BOOST_REQUIRE_EQUAL(limiter.GetRemainingTokens(), 0);
    limiter.AddTime(boost::posix_time::milliseconds(2));
    BOOST_REQUIRE_EQUAL(limiter.GetRemainingTokens(), 1);
    // next unit time
    limiter.AddTime(boost::posix_time::milliseconds(20));
    BOOST_REQUIRE_EQUAL(limiter.GetRemainingTokens(), 2);

    // burst limit
    limiter.AddTime(boost::posix_time::seconds(2));
    BOOST_REQUIRE_EQUAL(limiter.GetRemainingTokens(), 5);
    BOOST_REQUIRE(limiter.HasFullBucketOfTokens());
}

BOOST_AUTO_TEST_CASE(TokenRateLimiterHighRate)
{
    static constexpr uint64_t u64_1e9 = 1000000000;
    static constexpr uint64_t u64_1e8 = 100000000;
    static constexpr uint64_t u64_5e7 = 50000000;
    static constexpr uint64_t u64_3e7 = 30000000;
    static constexpr uint64_t u64_2e7 = 20000000;
    TokenRateLimiter limiter;
    limiter.SetRate(
        u64_1e9, // 1ns per token
        boost::posix_time::seconds(1),
        boost::posix_time::milliseconds(100)
    );

    BOOST_REQUIRE(limiter.HasFullBucketOfTokens());
    // deplete the tokens
    BOOST_REQUIRE_EQUAL(limiter.GetRemainingTokens(), u64_1e8);
    BOOST_REQUIRE(limiter.TakeTokens(u64_5e7));
    BOOST_REQUIRE_EQUAL(limiter.GetRemainingTokens(), u64_5e7);
    BOOST_REQUIRE(!limiter.HasFullBucketOfTokens());
    BOOST_REQUIRE(limiter.TakeTokens(u64_3e7));
    BOOST_REQUIRE_EQUAL(limiter.GetRemainingTokens(), u64_2e7);
    BOOST_REQUIRE(limiter.TakeTokens(u64_2e7));
    BOOST_REQUIRE_EQUAL(limiter.GetRemainingTokens(), 0);

    limiter.AddTime(boost::posix_time::microseconds(1));
    BOOST_REQUIRE_EQUAL(limiter.GetRemainingTokens(), 1000);

    // burst limit
    limiter.AddTime(boost::posix_time::seconds(2));
    BOOST_REQUIRE_EQUAL(limiter.GetRemainingTokens(), u64_1e8);
    BOOST_REQUIRE(limiter.HasFullBucketOfTokens());
}
