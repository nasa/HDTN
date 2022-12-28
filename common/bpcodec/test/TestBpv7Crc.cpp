/**
 * @file TestBpv7Crc.cpp
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
#include "codec/Bpv7Crc.h"
#include <iostream>
#include <string>
#include <inttypes.h>
#include <vector>

BOOST_AUTO_TEST_CASE(Bpv7CrcTestCase)
{
    static const std::vector<std::string> MESSAGE_STRINGS = { "TheQuickBrownFoxJumpsOverTheLazyDog.", "Short", "Length08" };
    //verified with https://crccalc.com/ (no spaces allowed on this site).. also verified with http://www.sunshine2k.de/coding/javascript/crc/crc_js.html
    static const std::vector<uint32_t> EXPECTED_CRC32C_VEC = { 0xAE76DF21 , 0x7B6BE32C, 0x73C00CF6 };
    static const std::vector<uint16_t> EXPECTED_CRC16_X25_VEC = { 0x2870 , 0x62B8, 0x46F5 };
    std::vector<uint8_t> cborSerialization(10); //need to be 3 bytes for crc16 or 5 bytes for crc32

    for (std::size_t messageIndex = 0; messageIndex < MESSAGE_STRINGS.size(); ++messageIndex) {
        const std::string & messageStr = MESSAGE_STRINGS[messageIndex];
        std::vector<uint8_t> changedAlignmentMessageVec(messageStr.size() + 20);
        const uint32_t expectedCrc32C = EXPECTED_CRC32C_VEC[messageIndex];
        const uint16_t expectedCrc16_X25 = EXPECTED_CRC16_X25_VEC[messageIndex];
        for (std::size_t i = 0; i < 9; ++i) {
            uint8_t * dataStart = &changedAlignmentMessageVec[i];
            memcpy(dataStart, messageStr.data(), messageStr.length());
            //std::cout << (char)dataStart[0] << " " << (((std::uintptr_t)dataStart) & 7) << std::endl;
            uint16_t crc16Software = Bpv7Crc::Crc16_X25_Unaligned(dataStart, messageStr.length());
            //printf("%x %x\n", crc16Software, expectedCrc16_X25);
            BOOST_REQUIRE_EQUAL(expectedCrc16_X25, crc16Software);

            //cbor serialize then deserialize crc16
            if (i == 0) { //only need to do this once
                BOOST_REQUIRE_EQUAL(Bpv7Crc::SerializeCrc16ForBpv7(&cborSerialization[0], expectedCrc16_X25), 3);
                uint16_t deserializedCrc16;
                uint8_t numBytesTakenToDecode;
                BOOST_REQUIRE(Bpv7Crc::DeserializeCrc16ForBpv7(cborSerialization.data(), &numBytesTakenToDecode, deserializedCrc16));
                BOOST_REQUIRE_EQUAL(numBytesTakenToDecode, 3);
                BOOST_REQUIRE_EQUAL(deserializedCrc16, expectedCrc16_X25);
            }

            uint32_t crc32Software = Bpv7Crc::Crc32C_Unaligned_Software(dataStart, messageStr.length());
            BOOST_REQUIRE_EQUAL(expectedCrc32C, crc32Software);
#ifdef USE_CRC32C_FAST
            uint32_t crc32Hardware = Bpv7Crc::Crc32C_Unaligned_Hardware(dataStart, messageStr.length());
            BOOST_REQUIRE_EQUAL(expectedCrc32C, crc32Hardware);
            //printf("%x %x %x\n", crc32Hardware, crc32Software, expectedCrc32C);
#endif
            //cbor serialize then deserialize crc32
            if (i == 0) { //only need to do this once
                BOOST_REQUIRE_EQUAL(Bpv7Crc::SerializeCrc32ForBpv7(&cborSerialization[0], expectedCrc32C), 5);
                uint32_t deserializedCrc32;
                uint8_t numBytesTakenToDecode;
                BOOST_REQUIRE(Bpv7Crc::DeserializeCrc32ForBpv7(cborSerialization.data(), &numBytesTakenToDecode, deserializedCrc32));
                BOOST_REQUIRE_EQUAL(numBytesTakenToDecode, 5);
                BOOST_REQUIRE_EQUAL(deserializedCrc32, expectedCrc32C);
            }
        }
    }
}

