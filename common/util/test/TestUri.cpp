#include <boost/test/unit_test.hpp>
#include <boost/thread.hpp>
#include "Uri.h"
#include <iostream>
#include <string>
#include <inttypes.h>
#include <vector>
#include <boost/lexical_cast.hpp>

void GenerateBitscanLut() { //generate lookup table stuff for Uri::GetStringLengthOfUint
    for (unsigned int i = 0; i <= 63; ++i) {
        const uint64_t bitScanIndexMinVal = (static_cast<uint64_t>(1)) << i;
        const uint64_t bitScanIndexMinValStrLen = boost::lexical_cast<std::string>(bitScanIndexMinVal).length();
        //blsmsk - Set all the lower bits of dst up to and including the lowest set bit in unsigned integer a.
        //dst := (a - 1) XOR a
        const uint64_t bitScanIndexMaxVal = (bitScanIndexMinVal - 1) ^ bitScanIndexMinVal;
        const uint64_t bitScanIndexMaxValStrLen = boost::lexical_cast<std::string>(bitScanIndexMaxVal).length();
        
        if(true) { //for a base 2 edge case test of Uri::GetStringLengthOfUint
            std::cout << bitScanIndexMinVal << "ui64, //1 << " << i << "\n";
            std::cout << bitScanIndexMaxVal << "ui64, //_blsmsk_u64(1 << " << i << ")\n";
        }
        if (false) { //make sure the difference is 0 or 1
            std::cout << "bitScanIndexMaxValStrLen - bitScanIndexMinValStrLen = " << (bitScanIndexMaxValStrLen - bitScanIndexMinValStrLen) << "\n";
        }
        if (false) { //generate lookup table for Uri::GetStringLengthOfUint
            if (i == 0) {
                std::cout << "static const uint8_t bitScanIndexMinValStrLengths[64] = {\n";
            }
            std::cout << "    " << bitScanIndexMinValStrLen << ", //1 << " << i << "\n";
            if (i == 63) {
                std::cout << "};\n";
            }
        }
    }
}

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
        BOOST_REQUIRE(!Uri::ParseIpnUriString(std::string("ipn:."), eidNodeNumber, eidServiceNumber));
        BOOST_REQUIRE(!Uri::ParseIpnUriString(std::string("ipn:"), eidNodeNumber, eidServiceNumber));
        BOOST_REQUIRE(!Uri::ParseIpnUriString(std::string("ipn:1"), eidNodeNumber, eidServiceNumber));
        BOOST_REQUIRE(!Uri::ParseIpnUriString(std::string("ipn:10"), eidNodeNumber, eidServiceNumber));
        BOOST_REQUIRE(!Uri::ParseIpnUriString(std::string("ipn:1844674407370955161555.1844674407370955161555"), eidNodeNumber, eidServiceNumber));
        BOOST_REQUIRE(!Uri::ParseIpnUriString(std::string("ipn:1.1844674407370955161555"), eidNodeNumber, eidServiceNumber));
        BOOST_REQUIRE(!Uri::ParseIpnUriString(std::string("ipn:1844674407370955161555.1"), eidNodeNumber, eidServiceNumber));
    }

}

BOOST_AUTO_TEST_CASE(UintToStringLengthTestCase)
{
    static const std::vector<uint64_t> testVals1 = {
        1ui64,
        10ui64,
        100ui64,
        1000ui64,
        10000ui64,
        100000ui64,
        1000000ui64,
        10000000ui64,
        100000000ui64,
        1000000000ui64,
        10000000000ui64,
        100000000000ui64,
        1000000000000ui64,
        10000000000000ui64,
        100000000000000ui64,
        1000000000000000ui64,
        10000000000000000ui64,
        100000000000000000ui64,
        1000000000000000000ui64,
        10000000000000000000ui64
    };
    for (std::size_t i = 0; i < testVals1.size(); ++i) { //test base 10 edge cases first
        for (int j = -1; j <= 1; ++j) {
            const uint64_t valToTest = testVals1[i] + j;
            //std::cout << valToTest << "\n";
            const uint64_t computedLength = Uri::GetStringLengthOfUint(valToTest);
            const uint64_t actualLength = boost::lexical_cast<std::string>(valToTest).length();
            BOOST_REQUIRE_EQUAL(computedLength, actualLength);
        }
    }
    for (unsigned int i = 0; i <= 63; ++i) { //test base 2 edge cases next
        const uint64_t bitScanIndexMinVal = (static_cast<uint64_t>(1)) << i;
        const uint64_t bitScanIndexMinValStrLenComputed = Uri::GetStringLengthOfUint(bitScanIndexMinVal);
        const uint64_t bitScanIndexMinValStrLenActual = boost::lexical_cast<std::string>(bitScanIndexMinVal).length();
        BOOST_REQUIRE_EQUAL(bitScanIndexMinValStrLenComputed, bitScanIndexMinValStrLenActual);
        //blsmsk - Set all the lower bits of dst up to and including the lowest set bit in unsigned integer a.
        //dst := (a - 1) XOR a
        const uint64_t bitScanIndexMaxVal = (bitScanIndexMinVal - 1) ^ bitScanIndexMinVal;
        const uint64_t bitScanIndexMaxValStrLenComputed = Uri::GetStringLengthOfUint(bitScanIndexMaxVal);
        const uint64_t bitScanIndexMaxValStrLenActual = boost::lexical_cast<std::string>(bitScanIndexMaxVal).length();
        BOOST_REQUIRE_EQUAL(bitScanIndexMaxValStrLenComputed, bitScanIndexMaxValStrLenActual);
        //std::cout << bitScanIndexMinVal << "ui64, //1 << " << i << "\n";
        //std::cout << bitScanIndexMaxVal << "ui64, //_blsmsk_u64(1 << " << i << ")\n";
        if (i > 0) {
            BOOST_REQUIRE_GT(bitScanIndexMaxVal, bitScanIndexMinVal);
        }
        else {
            BOOST_REQUIRE_EQUAL(bitScanIndexMaxVal, bitScanIndexMinVal);
        }
    }
    /*
    //the following test passes overnight: 2305032892 assertions out of 2305032892 passed (2^32+2305032892=6600000188)
    for (uint64_t i = 0; i < 6600000000ui64; ++i) {
        const uint64_t valToTest = i;
        const uint64_t computedLength = Uri::GetStringLengthOfUint(valToTest);
        const uint64_t actualLength = boost::lexical_cast<std::string>(valToTest).length();
        BOOST_REQUIRE_EQUAL(computedLength, actualLength);
    }
    */
    //GenerateBitscanLut();
}
