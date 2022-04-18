/**
 * @file TestUri.cpp
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
            std::cout << bitScanIndexMinVal << "u, //1 << " << i << "\n";
            std::cout << bitScanIndexMaxVal << "u, //_blsmsk_u64(1 << " << i << ")\n";
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
        BOOST_REQUIRE_EQUAL(Uri::GetIpnUriString(1, UINT64_MAX), std::string("ipn:1.18446744073709551615"));
    }

    //test WriteIpnUriCstring and GetIpnUriCstringLengthRequiredIncludingNullTerminator
    {
        char buffer[70];
        BOOST_REQUIRE_EQUAL(Uri::WriteIpnUriCstring(1, 1, buffer, 8), 8); //8 includes null char
        BOOST_REQUIRE_EQUAL(std::string(buffer), std::string("ipn:1.1"));

        BOOST_REQUIRE_EQUAL(Uri::WriteIpnUriCstring(1, 1, buffer, 7), 0); //7 does not include null char

        BOOST_REQUIRE_EQUAL(Uri::WriteIpnUriCstring(1, 1, buffer, 6), 0); //6 way too short

        BOOST_REQUIRE_EQUAL(Uri::WriteIpnUriCstring(1, 1, buffer, 70), 8); //70 goes beyond string length
        BOOST_REQUIRE_EQUAL(std::string(buffer), std::string("ipn:1.1"));
        BOOST_REQUIRE_EQUAL(Uri::GetIpnUriCstringLengthRequiredIncludingNullTerminator(1, 1), 8);

        BOOST_REQUIRE_EQUAL(Uri::WriteIpnUriCstring(0, 0, buffer, 70), 8);
        BOOST_REQUIRE_EQUAL(std::string(buffer), std::string("ipn:0.0"));
        BOOST_REQUIRE_EQUAL(Uri::GetIpnUriCstringLengthRequiredIncludingNullTerminator(0, 0), 8);

        BOOST_REQUIRE_EQUAL(Uri::WriteIpnUriCstring(1, 0, buffer, 70), 8);
        BOOST_REQUIRE_EQUAL(std::string(buffer), std::string("ipn:1.0"));
        BOOST_REQUIRE_EQUAL(Uri::GetIpnUriCstringLengthRequiredIncludingNullTerminator(1, 0), 8);

        static const std::string largestIpn("ipn:18446744073709551615.18446744073709551615");
        BOOST_REQUIRE_EQUAL(Uri::WriteIpnUriCstring(UINT64_MAX, UINT64_MAX, buffer, 70), largestIpn.length() + 1); //+1 to include the null char
        BOOST_REQUIRE_EQUAL(std::string(buffer), largestIpn);
        BOOST_REQUIRE_EQUAL(Uri::GetIpnUriCstringLengthRequiredIncludingNullTerminator(UINT64_MAX, UINT64_MAX), largestIpn.length() + 1); //+1 to include the null char

        static const std::string largestIpnSvc("ipn:1.18446744073709551615");
        BOOST_REQUIRE_EQUAL(Uri::WriteIpnUriCstring(1, UINT64_MAX, buffer, 70), largestIpnSvc.length() + 1); //+1 to include the null char
        BOOST_REQUIRE_EQUAL(std::string(buffer), largestIpnSvc);
        BOOST_REQUIRE_EQUAL(Uri::GetIpnUriCstringLengthRequiredIncludingNullTerminator(1, UINT64_MAX), largestIpnSvc.length() + 1); //+1 to include the null char
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
        BOOST_REQUIRE(!Uri::ParseIpnUriString(std::string(""), eidNodeNumber, eidServiceNumber));
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

    //test bool Uri::ParseIpnUriCstring(const char * data, uint64_t bufferSize, uint64_t & bytesDecodedIncludingNullChar, uint64_t & eidNodeNumber, uint64_t & eidServiceNumber)()
    {
        uint64_t eidNodeNumber = 0;
        uint64_t eidServiceNumber = 0;
        uint64_t bytesDecodedIncludingNullChar;
        {
            const std::string ipnStr("ipn:18446744073709551615.18446744073709551615");
            BOOST_REQUIRE(Uri::ParseIpnUriCstring(ipnStr.c_str(), 70, bytesDecodedIncludingNullChar, eidNodeNumber, eidServiceNumber)); //70 sufficiently large buffer
            BOOST_REQUIRE_EQUAL(eidNodeNumber, UINT64_MAX);
            BOOST_REQUIRE_EQUAL(eidServiceNumber, UINT64_MAX);
            BOOST_REQUIRE_EQUAL(bytesDecodedIncludingNullChar, ipnStr.length() + 1); //+1 to include the null char

            BOOST_REQUIRE(!Uri::ParseIpnUriCstring(ipnStr.c_str(), 5, bytesDecodedIncludingNullChar, eidNodeNumber, eidServiceNumber)); //5 way too small of buffer
            BOOST_REQUIRE(!Uri::ParseIpnUriCstring(ipnStr.c_str(), 0, bytesDecodedIncludingNullChar, eidNodeNumber, eidServiceNumber)); //0 way too small of buffer
            BOOST_REQUIRE(!Uri::ParseIpnUriCstring(ipnStr.c_str(), ipnStr.length(), bytesDecodedIncludingNullChar, eidNodeNumber, eidServiceNumber)); //buffer has no room for null char
            BOOST_REQUIRE(Uri::ParseIpnUriCstring(ipnStr.c_str(), ipnStr.length() + 1, bytesDecodedIncludingNullChar, eidNodeNumber, eidServiceNumber)); //perfect length buffer
        }

        {
            const std::string ipnStr("ipn:18446744073709551614.18446744073709551613");
            BOOST_REQUIRE(Uri::ParseIpnUriCstring(ipnStr.c_str(), 70, bytesDecodedIncludingNullChar, eidNodeNumber, eidServiceNumber)); //70 sufficiently large buffer
            BOOST_REQUIRE_EQUAL(eidNodeNumber, UINT64_MAX - 1);
            BOOST_REQUIRE_EQUAL(eidServiceNumber, UINT64_MAX - 2);
            BOOST_REQUIRE_EQUAL(bytesDecodedIncludingNullChar, ipnStr.length() + 1); //+1 to include the null char
        }

        {
            const std::string ipnStr("ipn:1.0");
            BOOST_REQUIRE(Uri::ParseIpnUriCstring(ipnStr.c_str(), 70, bytesDecodedIncludingNullChar, eidNodeNumber, eidServiceNumber)); //70 sufficiently large buffer
            BOOST_REQUIRE_EQUAL(eidNodeNumber, 1);
            BOOST_REQUIRE_EQUAL(eidServiceNumber, 0);
            BOOST_REQUIRE_EQUAL(bytesDecodedIncludingNullChar, ipnStr.length() + 1); //+1 to include the null char
        }

        {
            const std::string ipnStr("ipn:0.1");
            BOOST_REQUIRE(Uri::ParseIpnUriCstring(ipnStr.c_str(), 70, bytesDecodedIncludingNullChar, eidNodeNumber, eidServiceNumber)); //70 sufficiently large buffer
            BOOST_REQUIRE_EQUAL(eidNodeNumber, 0);
            BOOST_REQUIRE_EQUAL(eidServiceNumber, 1);
            BOOST_REQUIRE_EQUAL(bytesDecodedIncludingNullChar, ipnStr.length() + 1); //+1 to include the null char
        }

        //all should fail by invalid ipns, 70 sufficiently large buffer
        BOOST_REQUIRE(!Uri::ParseIpnUriCstring("iipn:1.0", 70, bytesDecodedIncludingNullChar, eidNodeNumber, eidServiceNumber)); 
        BOOST_REQUIRE(!Uri::ParseIpnUriCstring("", 70, bytesDecodedIncludingNullChar, eidNodeNumber, eidServiceNumber));
        BOOST_REQUIRE(!Uri::ParseIpnUriCstring("ipn::1.0", 70, bytesDecodedIncludingNullChar, eidNodeNumber, eidServiceNumber));
        BOOST_REQUIRE(!Uri::ParseIpnUriCstring("ipn:.1.0", 70, bytesDecodedIncludingNullChar, eidNodeNumber, eidServiceNumber));
        BOOST_REQUIRE(!Uri::ParseIpnUriCstring("ipn:1..0", 70, bytesDecodedIncludingNullChar, eidNodeNumber, eidServiceNumber));
        BOOST_REQUIRE(!Uri::ParseIpnUriCstring("ipn:1:0", 70, bytesDecodedIncludingNullChar, eidNodeNumber, eidServiceNumber));
        BOOST_REQUIRE(!Uri::ParseIpnUriCstring("ipn:.0", 70, bytesDecodedIncludingNullChar, eidNodeNumber, eidServiceNumber));
        BOOST_REQUIRE(!Uri::ParseIpnUriCstring("ipn:1.", 70, bytesDecodedIncludingNullChar, eidNodeNumber, eidServiceNumber));
        BOOST_REQUIRE(!Uri::ParseIpnUriCstring("ipn:.", 70, bytesDecodedIncludingNullChar, eidNodeNumber, eidServiceNumber));
        BOOST_REQUIRE(!Uri::ParseIpnUriCstring("ipn:", 70, bytesDecodedIncludingNullChar, eidNodeNumber, eidServiceNumber));
        BOOST_REQUIRE(!Uri::ParseIpnUriCstring("ipn:1", 70, bytesDecodedIncludingNullChar, eidNodeNumber, eidServiceNumber));
        BOOST_REQUIRE(!Uri::ParseIpnUriCstring("ipn:10", 70, bytesDecodedIncludingNullChar, eidNodeNumber, eidServiceNumber));
        BOOST_REQUIRE(!Uri::ParseIpnUriCstring("ipn:1844674407370955161555.1844674407370955161555", 70, bytesDecodedIncludingNullChar, eidNodeNumber, eidServiceNumber));
        BOOST_REQUIRE(!Uri::ParseIpnUriCstring("ipn:1.1844674407370955161555", 70, bytesDecodedIncludingNullChar, eidNodeNumber, eidServiceNumber));
        BOOST_REQUIRE(!Uri::ParseIpnUriCstring("ipn:1844674407370955161555.1", 70, bytesDecodedIncludingNullChar, eidNodeNumber, eidServiceNumber));
    }

}

BOOST_AUTO_TEST_CASE(UintToStringLengthTestCase)
{
    static const std::vector<uint64_t> testVals1 = {
        1u,
        10u,
        100u,
        1000u,
        10000u,
        100000u,
        1000000u,
        10000000u,
        100000000u,
        1000000000u,
        10000000000u,
        100000000000u,
        1000000000000u,
        10000000000000u,
        100000000000000u,
        1000000000000000u,
        10000000000000000u,
        100000000000000000u,
        1000000000000000000u,
        10000000000000000000u
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
        //std::cout << bitScanIndexMinVal << "u, //1 << " << i << "\n";
        //std::cout << bitScanIndexMaxVal << "u, //_blsmsk_u64(1 << " << i << ")\n";
        if (i > 0) {
            BOOST_REQUIRE_GT(bitScanIndexMaxVal, bitScanIndexMinVal);
        }
        else {
            BOOST_REQUIRE_EQUAL(bitScanIndexMaxVal, bitScanIndexMinVal);
        }
    }
    /*
    //the following test passes overnight: 2305032892 assertions out of 2305032892 passed (2^32+2305032892=6600000188)
    for (uint64_t i = 0; i < 6600000000u; ++i) {
        const uint64_t valToTest = i;
        const uint64_t computedLength = Uri::GetStringLengthOfUint(valToTest);
        const uint64_t actualLength = boost::lexical_cast<std::string>(valToTest).length();
        BOOST_REQUIRE_EQUAL(computedLength, actualLength);
    }
    */
    //GenerateBitscanLut();
}
