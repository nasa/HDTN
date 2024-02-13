/**
 * @file TestTokenRateLimiter.cpp
 * @author  Brian Sipos
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
    BOOST_REQUIRE_EQUAL(limiter.GetRemainingTokens(), 5); //50 * 100ms
    BOOST_REQUIRE(limiter.CanTakeTokens());
    BOOST_REQUIRE(limiter.HasFullBucketOfTokens());
    BOOST_REQUIRE(limiter.TakeTokens(3));
    BOOST_REQUIRE_EQUAL(limiter.GetRemainingTokens(), 2);
    BOOST_REQUIRE(limiter.CanTakeTokens());
    BOOST_REQUIRE(!limiter.HasFullBucketOfTokens());
    BOOST_REQUIRE(limiter.TakeTokens(3));
    BOOST_REQUIRE_EQUAL(limiter.GetRemainingTokens(), -1);
    BOOST_REQUIRE(!limiter.CanTakeTokens());
    BOOST_REQUIRE(!limiter.TakeTokens(2));
    BOOST_REQUIRE_EQUAL(limiter.GetRemainingTokens(), -1);

    // add fractional times until fully available
    limiter.AddTime(boost::posix_time::milliseconds(10));
    BOOST_REQUIRE(!limiter.CanTakeTokens());
    BOOST_REQUIRE_EQUAL(limiter.GetRemainingTokens(), 0);
    limiter.AddTime(boost::posix_time::milliseconds(8));
    BOOST_REQUIRE(!limiter.CanTakeTokens());
    BOOST_REQUIRE_EQUAL(limiter.GetRemainingTokens(), 0);
    limiter.AddTime(boost::posix_time::milliseconds(2));
    BOOST_REQUIRE(limiter.CanTakeTokens()); //finally remain is 0
    BOOST_REQUIRE_EQUAL(limiter.GetRemainingTokens(), 0);
    // next unit time
    limiter.AddTime(boost::posix_time::milliseconds(20));
    BOOST_REQUIRE_EQUAL(limiter.GetRemainingTokens(), 1);
    BOOST_REQUIRE(limiter.CanTakeTokens());

    // burst limit
    limiter.AddTime(boost::posix_time::seconds(2));
    BOOST_REQUIRE_EQUAL(limiter.GetRemainingTokens(), 5);
    BOOST_REQUIRE(limiter.HasFullBucketOfTokens());
    BOOST_REQUIRE(limiter.CanTakeTokens());
}

BOOST_AUTO_TEST_CASE(TokenRateLimiterHighRate)
{
    static constexpr int64_t i64_1e9 = 1000000000;
    static constexpr int64_t i64_1e8 = 100000000;
    static constexpr int64_t i64_5e7 = 50000000;
    static constexpr int64_t i64_3e7 = 30000000;
    static constexpr int64_t i64_2e7 = 20000000;
    TokenRateLimiter limiter;
    limiter.SetRate(
        i64_1e9, // 1ns per token
        boost::posix_time::seconds(1),
        boost::posix_time::milliseconds(100)
    );

    BOOST_REQUIRE(limiter.HasFullBucketOfTokens());
    // deplete the tokens
    BOOST_REQUIRE_EQUAL(limiter.GetRemainingTokens(), i64_1e8); //i64_1e9 * 100ms
    BOOST_REQUIRE(limiter.TakeTokens(i64_5e7));
    BOOST_REQUIRE_EQUAL(limiter.GetRemainingTokens(), i64_5e7);
    BOOST_REQUIRE(!limiter.HasFullBucketOfTokens());
    BOOST_REQUIRE(limiter.TakeTokens(i64_3e7));
    BOOST_REQUIRE_EQUAL(limiter.GetRemainingTokens(), i64_2e7);
    BOOST_REQUIRE(limiter.TakeTokens(i64_2e7));
    BOOST_REQUIRE_EQUAL(limiter.GetRemainingTokens(), 0);

    limiter.AddTime(boost::posix_time::microseconds(1));
    BOOST_REQUIRE_EQUAL(limiter.GetRemainingTokens(), 1000);

    // burst limit
    limiter.AddTime(boost::posix_time::seconds(2));
    BOOST_REQUIRE_EQUAL(limiter.GetRemainingTokens(), i64_1e8);
    BOOST_REQUIRE(limiter.HasFullBucketOfTokens());
}
