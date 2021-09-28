#include <boost/test/unit_test.hpp>
#include "LtpSessionRecreationPreventer.h"

BOOST_AUTO_TEST_CASE(LtpSessionRecreationPreventerTestCase)
{
    {
        const uint64_t maxSessions = 1000;
        LtpSessionRecreationPreventer srp(maxSessions);
        for (uint64_t i = 0; i < maxSessions; ++i) {
            BOOST_REQUIRE(!srp.ContainsSession(i));
            BOOST_REQUIRE(srp.AddSession(i));
            BOOST_REQUIRE(srp.ContainsSession(i));
            BOOST_REQUIRE(!srp.AddSession(i));
        }
        for (uint64_t i = 0; i < maxSessions; ++i) {
            BOOST_REQUIRE(srp.ContainsSession(i));
            BOOST_REQUIRE(!srp.AddSession(i));
            BOOST_REQUIRE(srp.ContainsSession(i));
            BOOST_REQUIRE(!srp.AddSession(i));
        }
        for (uint64_t i = 0; i < maxSessions; ++i) {
            const uint64_t newSession = i + maxSessions;
            BOOST_REQUIRE(srp.ContainsSession(i));
            BOOST_REQUIRE(!srp.ContainsSession(newSession));
            BOOST_REQUIRE(srp.AddSession(newSession));
            BOOST_REQUIRE(!srp.ContainsSession(i));
            BOOST_REQUIRE(srp.ContainsSession(newSession));
        }
        for (uint64_t i = 0; i < maxSessions; ++i) {
            BOOST_REQUIRE(!srp.ContainsSession(i));
            BOOST_REQUIRE(srp.AddSession(i));
            BOOST_REQUIRE(srp.ContainsSession(i));
            BOOST_REQUIRE(!srp.AddSession(i));
        }
        
    }

   
}
