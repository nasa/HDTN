/**
 * @file TestSlip.cpp
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
#include "slip.h"
#include <iostream>
#include <string>
#include <inttypes.h>
#include <vector>

BOOST_AUTO_TEST_CASE(SlipNoSpecialCharactersTestCase)
{
    const std::vector<uint8_t> ipPacket = { 2,3,4,5,6,7,8,9,10,11,20,21,22,23,6 };
    std::vector<uint8_t> slipEncodedIpPacket(ipPacket.size() * 3);
    

    BOOST_REQUIRE(ipPacket != slipEncodedIpPacket);

    unsigned int sizeEncoded = SlipEncode(ipPacket.data(), slipEncodedIpPacket.data(), (unsigned int)ipPacket.size());
    BOOST_REQUIRE_EQUAL(sizeEncoded, (unsigned int)ipPacket.size() + 2); // +2 for END characters at beginning and end 
    
    slipEncodedIpPacket.resize(sizeEncoded);

    //Test slip encode per character function
    {
        std::vector<uint8_t> slipEncodedPerCharacterIpPacket;
        BOOST_REQUIRE(slipEncodedIpPacket != slipEncodedPerCharacterIpPacket);
        slipEncodedPerCharacterIpPacket.push_back(SLIP_END);
        for (std::size_t i = 0; i < ipPacket.size(); ++i) {
            uint8_t buf[2];
            const unsigned int bytesEncodedThisChar = SlipEncodeChar(ipPacket[i], buf);
            for (unsigned int j = 0; j < bytesEncodedThisChar; ++j) {
                slipEncodedPerCharacterIpPacket.push_back(buf[j]);
            }
        }
        slipEncodedPerCharacterIpPacket.push_back(SLIP_END);
        BOOST_REQUIRE(slipEncodedIpPacket == slipEncodedPerCharacterIpPacket);
    }

    //Test that the encoded is the same as the original ip packet because there are no special characters
    {
        std::vector<uint8_t> slipEncodedIpPacketWithoutEnds(slipEncodedIpPacket.begin()+1, slipEncodedIpPacket.begin() + 1 + ipPacket.size());
        BOOST_REQUIRE(ipPacket == slipEncodedIpPacketWithoutEnds);
    }
    
    //Test slip decode per character function
    {
        std::vector<uint8_t> slipDecodedIpPacket(ipPacket.size() * 3);
        BOOST_REQUIRE(ipPacket != slipDecodedIpPacket);
        SlipDecodeState_t slipDecodeState;
        SlipDecodeInit(&slipDecodeState);
        unsigned int decodedIpPacketLength = 0;
        for (std::size_t i = 1; i < 1000; ++i) {
            BOOST_REQUIRE_LT(i, slipEncodedIpPacket.size());
            uint8_t outChar;
            unsigned int retVal = SlipDecodeChar(&slipDecodeState, slipEncodedIpPacket[i], &outChar);
            if (retVal == 1) { //new outChar
                slipDecodedIpPacket[decodedIpPacketLength++] = outChar;
            }            
            else if (retVal == 2) { //complete (no new outchar)
                BOOST_REQUIRE_EQUAL(decodedIpPacketLength, (unsigned int)ipPacket.size());
                slipDecodedIpPacket.resize(decodedIpPacketLength);
                break;
            }
            
        }
        
        BOOST_REQUIRE(ipPacket == slipDecodedIpPacket);
    }
}



BOOST_AUTO_TEST_CASE(SlipTwoSpecialCharactersInMiddleTestCase)
{
    const std::vector<uint8_t> ipPacket = { 2,3,4,5,6, SLIP_END,  8,9,10,11,20, SLIP_ESC, 22,23,4 };
    const std::vector<uint8_t> expectedSlipEncodedPacket = { SLIP_END, 2,3,4,5,6, SLIP_ESC, SLIP_ESC_END, 8,9,10,11,20, SLIP_ESC, SLIP_ESC_ESC, 22,23,4, SLIP_END };
    std::vector<uint8_t> slipEncodedIpPacket(ipPacket.size() * 10);


    BOOST_REQUIRE(ipPacket != slipEncodedIpPacket);

    unsigned int sizeEncoded = SlipEncode(ipPacket.data(), slipEncodedIpPacket.data(), (unsigned int)ipPacket.size());
    BOOST_REQUIRE(expectedSlipEncodedPacket != slipEncodedIpPacket); //not equal until resize
    BOOST_REQUIRE_EQUAL(sizeEncoded, (unsigned int)expectedSlipEncodedPacket.size());
    slipEncodedIpPacket.resize(sizeEncoded);
    BOOST_REQUIRE(expectedSlipEncodedPacket == slipEncodedIpPacket);

    //Test slip encode per character function
    {
        std::vector<uint8_t> slipEncodedPerCharacterIpPacket;
        BOOST_REQUIRE(expectedSlipEncodedPacket != slipEncodedPerCharacterIpPacket);
        slipEncodedPerCharacterIpPacket.push_back(SLIP_END);
        for (std::size_t i = 0; i < ipPacket.size(); ++i) {
            uint8_t buf[2];
            const unsigned int bytesEncodedThisChar = SlipEncodeChar(ipPacket[i], buf);
            for (unsigned int j = 0; j < bytesEncodedThisChar; ++j) {
                slipEncodedPerCharacterIpPacket.push_back(buf[j]);
            }
        }
        slipEncodedPerCharacterIpPacket.push_back(SLIP_END);
        BOOST_REQUIRE(expectedSlipEncodedPacket == slipEncodedPerCharacterIpPacket);
    }

    //Test slip decode per character function
    {
        std::vector<uint8_t> slipDecodedIpPacket(ipPacket.size() * 3);
        BOOST_REQUIRE(ipPacket != slipDecodedIpPacket);
        SlipDecodeState_t slipDecodeState;
        SlipDecodeInit(&slipDecodeState);
        unsigned int decodedIpPacketLength = 0;
        for (std::size_t i = 1; i < 1000; ++i) {
            BOOST_REQUIRE_LT(i, slipEncodedIpPacket.size());
            uint8_t outChar;
            unsigned int retVal = SlipDecodeChar(&slipDecodeState, slipEncodedIpPacket[i], &outChar);
            if (retVal == 1) { //new outChar
                slipDecodedIpPacket[decodedIpPacketLength++] = outChar;
            }
            else if (retVal == 2) { //complete (no new outchar)
                BOOST_REQUIRE_EQUAL(decodedIpPacketLength, (unsigned int)ipPacket.size());
                slipDecodedIpPacket.resize(decodedIpPacketLength);
                break;
            }

        }

        BOOST_REQUIRE(ipPacket == slipDecodedIpPacket);
    }
}



BOOST_AUTO_TEST_CASE(SlipSpecialCharactersInMiddleAndEndsTestCase)
{
    const std::vector<uint8_t> ipPacket = { SLIP_END, 2,3,4,5,6, SLIP_END, SLIP_ESC, 8,9,10,11,20, 22,23,4, SLIP_END };
    const std::vector<uint8_t> expectedSlipEncodedPacket = { SLIP_END,   SLIP_ESC, SLIP_ESC_END, 2,3,4,5,6, SLIP_ESC, SLIP_ESC_END, SLIP_ESC, SLIP_ESC_ESC, 8,9,10,11,20, 22,23,4, SLIP_ESC, SLIP_ESC_END,  SLIP_END };
    std::vector<uint8_t> slipEncodedIpPacket(ipPacket.size() * 10);


    BOOST_REQUIRE(ipPacket != slipEncodedIpPacket);

    unsigned int sizeEncoded = SlipEncode(ipPacket.data(), slipEncodedIpPacket.data(), (unsigned int)ipPacket.size());
    BOOST_REQUIRE(expectedSlipEncodedPacket != slipEncodedIpPacket); //not equal until resize
    BOOST_REQUIRE_EQUAL(sizeEncoded, (unsigned int)expectedSlipEncodedPacket.size());
    slipEncodedIpPacket.resize(sizeEncoded);
    BOOST_REQUIRE(expectedSlipEncodedPacket == slipEncodedIpPacket);

    //Test slip encode per character function
    {
        std::vector<uint8_t> slipEncodedPerCharacterIpPacket;
        BOOST_REQUIRE(expectedSlipEncodedPacket != slipEncodedPerCharacterIpPacket);
        slipEncodedPerCharacterIpPacket.push_back(SLIP_END);
        for (std::size_t i = 0; i < ipPacket.size(); ++i) {
            uint8_t buf[2];
            const unsigned int bytesEncodedThisChar = SlipEncodeChar(ipPacket[i], buf);
            for (unsigned int j = 0; j < bytesEncodedThisChar; ++j) {
                slipEncodedPerCharacterIpPacket.push_back(buf[j]);
            }
        }
        slipEncodedPerCharacterIpPacket.push_back(SLIP_END);
        BOOST_REQUIRE(expectedSlipEncodedPacket == slipEncodedPerCharacterIpPacket);
    }

    //Test slip decode per character function
    {
        std::vector<uint8_t> slipDecodedIpPacket(ipPacket.size() * 3);
        BOOST_REQUIRE(ipPacket != slipDecodedIpPacket);
        SlipDecodeState_t slipDecodeState;
        SlipDecodeInit(&slipDecodeState);
        unsigned int decodedIpPacketLength = 0;
        for (std::size_t i = 1; i < 1000; ++i) {
            BOOST_REQUIRE_LT(i, slipEncodedIpPacket.size());
            uint8_t outChar;
            unsigned int retVal = SlipDecodeChar(&slipDecodeState, slipEncodedIpPacket[i], &outChar);
            if (retVal == 1) { //new outChar
                slipDecodedIpPacket[decodedIpPacketLength++] = outChar;
            }
            else if (retVal == 2) { //complete (no new outchar)
                BOOST_REQUIRE_EQUAL(decodedIpPacketLength, (unsigned int)ipPacket.size());
                slipDecodedIpPacket.resize(decodedIpPacketLength);
                break;
            }

        }

        BOOST_REQUIRE(ipPacket == slipDecodedIpPacket);
    }
}
