#include <boost/test/unit_test.hpp>
#include <boost/thread.hpp>
#include "Uri.h"
#include <iostream>
#include <string>
#include <inttypes.h>
#include <vector>

BOOST_AUTO_TEST_CASE(IpnUriTestCase)
{
    //test GetIpnUriString()
    {
        BOOST_REQUIRE_EQUAL(Uri::GetIpnUriString(1, 1), std::string("ipn:1.1"));
        BOOST_REQUIRE_EQUAL(Uri::GetIpnUriString(0, 0), std::string("ipn:0.0"));
        BOOST_REQUIRE_EQUAL(Uri::GetIpnUriString(1, 0), std::string("ipn:1.0"));
        BOOST_REQUIRE_EQUAL(Uri::GetIpnUriString(UINT64_MAX, UINT64_MAX), std::string("ipn:18446744073709551615.18446744073709551615"));
    }

    //test ParseIpnUriString()
    {
        uint64_t eidNodeNumber = 0;
        uint64_t eidServiceNumber = 0;

        BOOST_REQUIRE(Uri::ParseIpnUriString(std::string("ipn:18446744073709551615.18446744073709551615"), eidNodeNumber, eidServiceNumber));
        BOOST_REQUIRE_EQUAL(eidNodeNumber, UINT64_MAX);
        BOOST_REQUIRE_EQUAL(eidServiceNumber, UINT64_MAX);

        BOOST_REQUIRE(Uri::ParseIpnUriString(std::string("ipn:18446744073709551614.18446744073709551613"), eidNodeNumber, eidServiceNumber));
        BOOST_REQUIRE_EQUAL(eidNodeNumber, UINT64_MAX - 1);
        BOOST_REQUIRE_EQUAL(eidServiceNumber, UINT64_MAX - 2);

        BOOST_REQUIRE(Uri::ParseIpnUriString(std::string("ipn:1.0"), eidNodeNumber, eidServiceNumber));
        BOOST_REQUIRE_EQUAL(eidNodeNumber, 1);
        BOOST_REQUIRE_EQUAL(eidServiceNumber, 0);

        BOOST_REQUIRE(Uri::ParseIpnUriString(std::string("ipn:0.1"), eidNodeNumber, eidServiceNumber));
        BOOST_REQUIRE_EQUAL(eidNodeNumber, 0);
        BOOST_REQUIRE_EQUAL(eidServiceNumber, 1);

        BOOST_REQUIRE(!Uri::ParseIpnUriString(std::string("iipn:1.0"), eidNodeNumber, eidServiceNumber));
        BOOST_REQUIRE(!Uri::ParseIpnUriString(std::string("ipn::1.0"), eidNodeNumber, eidServiceNumber));
        BOOST_REQUIRE(!Uri::ParseIpnUriString(std::string("ipn:.1.0"), eidNodeNumber, eidServiceNumber));
        BOOST_REQUIRE(!Uri::ParseIpnUriString(std::string("ipn:1..0"), eidNodeNumber, eidServiceNumber));
        BOOST_REQUIRE(!Uri::ParseIpnUriString(std::string("ipn:1:0"), eidNodeNumber, eidServiceNumber));
        BOOST_REQUIRE(!Uri::ParseIpnUriString(std::string("ipn:.0"), eidNodeNumber, eidServiceNumber));
        BOOST_REQUIRE(!Uri::ParseIpnUriString(std::string("ipn:1."), eidNodeNumber, eidServiceNumber));
    }

}

