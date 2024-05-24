/**
 * @file TestBpv7Crc.cpp
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
#include <boost/thread.hpp>
#include "codec/Bpv7Crc.h"
#include <iostream>
#include <string>
#include <inttypes.h>
#include <vector>
#include <boost/timer/timer.hpp>

BOOST_AUTO_TEST_CASE(Bpv7CrcTestCase)
{
    static const std::vector<std::string> MESSAGE_STRINGS = { "TheQuickBrownFoxJumpsOverTheLazyDog.", "Short", "Length08" };
    //verified with https://crccalc.com/ (no spaces allowed on this site).. also verified with http://www.sunshine2k.de/coding/javascript/crc/crc_js.html
    static const std::vector<uint32_t> EXPECTED_CRC32C_VEC = { 0xAE76DF21 , 0x7B6BE32C, 0x73C00CF6 };
    static const std::vector<uint16_t> EXPECTED_CRC16_X25_VEC = { 0x2870 , 0x62B8, 0x46F5 };
    std::vector<uint8_t> cborSerialization(10); //need to be 3 bytes for crc16 or 5 bytes for crc32
    Crc32c_InOrderChunks crc32cTxInOrderChunks;
    Crc32c_ReceiveOutOfOrderChunks crc32cRxOutOfOrderChunks;

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
            crc32cTxInOrderChunks.Reset();
            crc32cTxInOrderChunks.AddUnalignedBytes(dataStart, messageStr.length());
            BOOST_REQUIRE_EQUAL(expectedCrc32C, crc32cTxInOrderChunks.FinalizeAndGet());
            crc32cTxInOrderChunks.Reset();
            crc32cTxInOrderChunks.AddUnalignedBytes(dataStart, 1);
            crc32cTxInOrderChunks.AddUnalignedBytes(dataStart + 1, messageStr.length() - 1);
            BOOST_REQUIRE_EQUAL(expectedCrc32C, crc32cTxInOrderChunks.FinalizeAndGet());

            crc32cRxOutOfOrderChunks.Reset();
            crc32cRxOutOfOrderChunks.AddUnalignedBytes(dataStart + 0, 0, messageStr.length());
            BOOST_REQUIRE_EQUAL(expectedCrc32C, crc32cRxOutOfOrderChunks.FinalizeAndGet());
            crc32cRxOutOfOrderChunks.Reset();
            crc32cRxOutOfOrderChunks.AddUnalignedBytes(dataStart + 0, 0, 1);
            crc32cRxOutOfOrderChunks.AddUnalignedBytes(dataStart + 3, 3, messageStr.length() - 3); //3 because 1 + 2
            crc32cRxOutOfOrderChunks.AddUnalignedBytes(dataStart + 1, 1, 2);
            BOOST_REQUIRE_EQUAL(expectedCrc32C, crc32cRxOutOfOrderChunks.FinalizeAndGet());

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

    
    crc32cTxInOrderChunks.Reset();
    crc32cTxInOrderChunks.AddUnalignedBytes((uint8_t*)MESSAGE_STRINGS[0].data(), MESSAGE_STRINGS[0].length());
    crc32cTxInOrderChunks.AddUnalignedBytes((uint8_t*)MESSAGE_STRINGS[1].data(), MESSAGE_STRINGS[1].length());

    crc32cRxOutOfOrderChunks.Reset();
    BOOST_REQUIRE(crc32cRxOutOfOrderChunks.AddUnalignedBytes((uint8_t*)MESSAGE_STRINGS[0].data(), 0, MESSAGE_STRINGS[0].length()));
    BOOST_REQUIRE(crc32cRxOutOfOrderChunks.AddUnalignedBytes((uint8_t*)MESSAGE_STRINGS[1].data(), MESSAGE_STRINGS[0].length(), MESSAGE_STRINGS[1].length()));
    BOOST_REQUIRE_EQUAL(crc32cRxOutOfOrderChunks.FinalizeAndGet(), crc32cTxInOrderChunks.FinalizeAndGet());

    crc32cRxOutOfOrderChunks.Reset();
    BOOST_REQUIRE(crc32cRxOutOfOrderChunks.AddUnalignedBytes((uint8_t*)MESSAGE_STRINGS[1].data(), MESSAGE_STRINGS[0].length(), MESSAGE_STRINGS[1].length()));
    BOOST_REQUIRE(crc32cRxOutOfOrderChunks.AddUnalignedBytes((uint8_t*)MESSAGE_STRINGS[0].data(), 0, MESSAGE_STRINGS[0].length()));
    BOOST_REQUIRE_EQUAL(crc32cRxOutOfOrderChunks.FinalizeAndGet(), crc32cTxInOrderChunks.FinalizeAndGet());

}

BOOST_AUTO_TEST_CASE(Bpv7ChunkCrcTestCase)
{
    Crc32c_InOrderChunks crc32cTxInOrderChunks;
    Crc32c_ReceiveOutOfOrderChunks crc32cRxOutOfOrderChunks;

    std::vector<uint32_t> dataChunk(100000);
    for (std::size_t i = 0; i < dataChunk.size(); ++i) {
        dataChunk[i] = (uint32_t)(i + 10);
    }
    static constexpr unsigned int numChunks = 10;
    static constexpr unsigned int chunkIndexToChange = 5;
    std::size_t offset = 0;
    for (unsigned int c = 0; c < numChunks; ++c) {
        dataChunk[chunkIndexToChange] = UINT32_MAX - c;
        const std::size_t sizeChunk = dataChunk.size() - c;
        crc32cTxInOrderChunks.AddUnalignedBytes((uint8_t*)dataChunk.data(), sizeChunk);
        crc32cRxOutOfOrderChunks.AddUnalignedBytes((uint8_t*)dataChunk.data(), offset, sizeChunk);
        offset += sizeChunk;
    }
    const uint32_t expectedCrc = crc32cTxInOrderChunks.FinalizeAndGet();
    const uint32_t rxCrc = crc32cRxOutOfOrderChunks.FinalizeAndGet();
    BOOST_REQUIRE_EQUAL(expectedCrc, rxCrc);
#if 0 //for benchmarking
    std::cout << "start timer\n";
    {
        boost::timer::auto_cpu_timer t;
        for (unsigned int iterations = 0; iterations < 100000; ++iterations) {
            const uint32_t rxCrc2 = crc32cRxOutOfOrderChunks.FinalizeAndGet();
            if (rxCrc2 != expectedCrc) {
                BOOST_REQUIRE(false);
            }
        }
    }
    //hardware: 9.281264s wall
    //boost crc: 224.312788s wall
    //zlib crc combine lut: 0.175751s wall, 0.171875s user + 0.000000s system = 0.171875s CPU (97.8%)
    //zlib crc combine preinit: 25.170631s wall, 17.531250s user + 0.015625s system = 17.546875s CPU (69.7%)
    //zlib crc combine original algorithm: 28.184864s wall, 12.296875s user + 0.000000s system = 12.296875s CPU (43.6%)
#endif
}
