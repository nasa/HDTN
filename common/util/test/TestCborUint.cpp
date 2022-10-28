/**
 * @file TestCborUint.cpp
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
#include <iostream>
#include <string>
#include <inttypes.h>
#include <vector>
#include "CborUint.h"
#include <boost/timer/timer.hpp>
#include <algorithm>
#include <random>
#include <boost/multiprecision/cpp_int.hpp>
#include <boost/multiprecision/detail/bitscan.hpp>
#include <boost/endian/conversion.hpp>
#ifdef USE_CBOR_FAST
#include <immintrin.h>
#endif

/*
    //https://datatracker.ietf.org/doc/html/rfc8949#appendix-A
   +==============================+====================================+
   |Diagnostic                    | Encoded                            |
   +==============================+====================================+
   |0                             | 0x00                               |
   +------------------------------+------------------------------------+
   |1                             | 0x01                               |
   +------------------------------+------------------------------------+
   |10                            | 0x0a                               |
   +------------------------------+------------------------------------+
   |23                            | 0x17                               |
   +------------------------------+------------------------------------+
   |24                            | 0x1818                             |
   +------------------------------+------------------------------------+
   |25                            | 0x1819                             |
   +------------------------------+------------------------------------+
   |100                           | 0x1864                             |
   +------------------------------+------------------------------------+
   |1000                          | 0x1903e8                           |
   +------------------------------+------------------------------------+
   |1000000                       | 0x1a000f4240                       |
   +------------------------------+------------------------------------+
   |1000000000000                 | 0x1b000000e8d4a51000               |
   +------------------------------+------------------------------------+
   |18446744073709551615          | 0x1bffffffffffffffff               |
   +------------------------------+------------------------------------+
   |18446744073709551616          | 0xc249010000000000000000           |
   +------------------------------+------------------------------------+

*/
//pair = value, expectedEncoding
typedef std::pair<uint64_t, std::vector<uint8_t> > pairVE;
static const std::vector<pairVE> testValuesPlusExpectedEncodings = {
    pairVE(0, {0x00}),
    pairVE(1, {0x01}),
    pairVE(10, {0x0a}),
    pairVE(23, {0x17}),
    pairVE(24, {0x18, 0x18}),
    pairVE(25, {0x18, 0x19}),
    pairVE(100, {0x18, 0x64}),
    pairVE(1000, {0x19, 0x03, 0xe8}),
    pairVE(1000000, {0x1a, 0x00, 0x0f, 0x42, 0x40}),
    pairVE(1000000000000, {0x1b, 0x00, 0x00, 0x00, 0xe8, 0xd4, 0xa5, 0x10, 0x00}),
    pairVE(18446744073709551615U, {0x1b, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff})
    //pairVE(18446744073709551616, {0xc2, 0x49, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}), //too large
};

//pair = value, encodeSize
typedef std::pair<uint64_t, unsigned int> pairVS;
static const std::vector<pairVS> testValuesPlusEncodedSizes = {
    pairVS(0, 1),
    pairVS(1, 1),
    pairVS(2, 1),
    pairVS(3, 1),
    pairVS(4, 1),
    pairVS(5, 1),
    pairVS(6, 1),
    pairVS(7, 1),
    pairVS(8, 1),
    pairVS(9, 1),
    pairVS(10, 1),
    pairVS(11, 1),
    pairVS(12, 1),
    pairVS(13, 1),
    pairVS(14, 1),
    pairVS(15, 1),
    pairVS(16, 1),
    pairVS(17, 1),
    pairVS(18, 1),
    pairVS(19, 1),
    pairVS(20, 1),
    pairVS(21, 1),
    pairVS(22, 1),
    pairVS(23, 1),

    pairVS(24, 2),
    pairVS(25, 2),
    pairVS(26, 2),
    pairVS(27, 2),
    pairVS(28, 2),
    pairVS(29, 2),
    pairVS(30, 2),
    
    
    // (1 << 8) = 256
    pairVS(256 - 4, 2),
    pairVS(256 - 3, 2),
    pairVS(256 - 2, 2),
    pairVS(256 - 1, 2),
    pairVS(256 + 0, 3),
    pairVS(256 + 1, 3),
    pairVS(256 + 2, 3),
    pairVS(256 + 3, 3),
    pairVS(256 + 4, 3),

    // (1 << 16) = 65536
    pairVS(65536 - 4, 3),
    pairVS(65536 - 3, 3),
    pairVS(65536 - 2, 3),
    pairVS(65536 - 1, 3),
    pairVS(65536 + 0, 5),
    pairVS(65536 + 1, 5),
    pairVS(65536 + 2, 5),
    pairVS(65536 + 3, 5),
    pairVS(65536 + 4, 5),

    // (1 << 32) = 4294967296
    pairVS(4294967296 - 4, 5),
    pairVS(4294967296 - 3, 5),
    pairVS(4294967296 - 2, 5),
    pairVS(4294967296 - 1, 5),
    pairVS(4294967296 + 0, 9),
    pairVS(4294967296 + 1, 9),
    pairVS(4294967296 + 2, 9),
    pairVS(4294967296 + 3, 9),
    pairVS(4294967296 + 4, 9),

    // (1 << 64) - 1
    pairVS(UINT64_MAX - 4, 9),
    pairVS(UINT64_MAX - 3, 9),
    pairVS(UINT64_MAX - 2, 9),
    pairVS(UINT64_MAX - 1, 9),
    pairVS(UINT64_MAX , 9),

};

BOOST_AUTO_TEST_CASE(CborUint64BitAppendixATestCase)
{
    std::vector<uint8_t> encodedClassic(9);
    std::vector<uint8_t> encodedFast(9);
    for (std::size_t i = 0; i < testValuesPlusExpectedEncodings.size(); ++i) {
        const pairVE & p = testValuesPlusExpectedEncodings[i];
        const uint64_t valueToEncode = p.first;
        const std::vector<uint8_t> & expectedEncoding = p.second;
        
        //encode classic
        encodedClassic.resize(9);
        encodedClassic.assign(encodedClassic.size(), 0);
        unsigned int encodedSizeClassic = CborEncodeU64Classic(&encodedClassic[0], valueToEncode, 9);
        BOOST_REQUIRE_EQUAL(encodedSizeClassic, expectedEncoding.size());
        BOOST_REQUIRE_EQUAL(encodedSizeClassic, CborGetEncodingSizeU64Classic(valueToEncode));
        encodedClassic.resize(encodedSizeClassic);
        BOOST_REQUIRE(encodedClassic == expectedEncoding);

        //encode classic buf size 9
        encodedClassic.resize(9);
        encodedClassic.assign(encodedClassic.size(), 0);
        encodedSizeClassic = CborEncodeU64ClassicBufSize9(&encodedClassic[0], valueToEncode);
        BOOST_REQUIRE_EQUAL(encodedSizeClassic, expectedEncoding.size());
        encodedClassic.resize(encodedSizeClassic);
        BOOST_REQUIRE(encodedClassic == expectedEncoding);

        //decode classic
        uint8_t numBytesTakenToDecode;
        uint64_t decodedValueClassic = CborDecodeU64Classic(expectedEncoding.data(), &numBytesTakenToDecode, 9);
        BOOST_REQUIRE_EQUAL((unsigned int)numBytesTakenToDecode, expectedEncoding.size());
        BOOST_REQUIRE_EQUAL(decodedValueClassic, valueToEncode);

        //decode classic buf size 9
        decodedValueClassic = CborDecodeU64ClassicBufSize9(expectedEncoding.data(), &numBytesTakenToDecode);
        BOOST_REQUIRE_EQUAL((unsigned int)numBytesTakenToDecode, expectedEncoding.size());
        BOOST_REQUIRE_EQUAL(decodedValueClassic, valueToEncode);

#ifdef USE_CBOR_FAST

        //encode fast
        encodedFast.resize(9);
        encodedFast.assign(encodedFast.size(), 0);
        unsigned int encodedSizeFast = CborEncodeU64Fast(&encodedFast[0], valueToEncode, 9);
        BOOST_REQUIRE_EQUAL(encodedSizeFast, expectedEncoding.size());
        BOOST_REQUIRE_EQUAL(encodedSizeFast, CborGetEncodingSizeU64Fast(valueToEncode));
        encodedFast.resize(encodedSizeFast);
        BOOST_REQUIRE(encodedFast == expectedEncoding);

        //encode fast buf size 9
        encodedFast.resize(9);
        encodedFast.assign(encodedFast.size(), 0);
        encodedSizeFast = CborEncodeU64FastBufSize9(&encodedFast[0], valueToEncode);
        BOOST_REQUIRE_EQUAL(encodedSizeFast, expectedEncoding.size());
        encodedFast.resize(encodedSizeFast);
        BOOST_REQUIRE(encodedFast == expectedEncoding);

        //decode fast
        uint64_t decodedValueFast = CborDecodeU64Fast(expectedEncoding.data(), &numBytesTakenToDecode, 9);
        BOOST_REQUIRE_EQUAL((unsigned int)numBytesTakenToDecode, expectedEncoding.size());
        BOOST_REQUIRE_EQUAL(decodedValueFast, valueToEncode);

        //decode fast buf size 9
        decodedValueFast = CborDecodeU64FastBufSize9(expectedEncoding.data(), &numBytesTakenToDecode);
        BOOST_REQUIRE_EQUAL((unsigned int)numBytesTakenToDecode, expectedEncoding.size());
        BOOST_REQUIRE_EQUAL(decodedValueFast, valueToEncode);

#endif //#ifdef USE_CBOR_FAST

    }

}

#ifdef USE_CBOR_FAST
BOOST_AUTO_TEST_CASE(CborUint64BitNoOverwriteTestCase)
{
    uint64_t s2;
    memcpy(&s2, "bbbbbbbb", 8);
    uint32_t s3;
    memcpy(&s3, "cccc", 4);
    for (unsigned int offset = 0; offset <= 9; ++offset) {
        //sanity check to make sure _mm_stream_si64 won't overwrite data past 8 bytes regardless of alignment
        std::vector<uint64_t> alignedData(4);
        memcpy(alignedData.data(), "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", 32);

        char *s = (char*)alignedData.data();
        char *sOffset = &s[offset];
        _mm_stream_si64((long long int *)sOffset, s2);
        
        BOOST_REQUIRE_EQUAL(std::string(sOffset, sOffset + 16), std::string("bbbbbbbbaaaaaaaa"));

        _mm_stream_si32((int32_t *)sOffset, s3);
        BOOST_REQUIRE_EQUAL(std::string(sOffset, sOffset + 16), std::string("ccccbbbbaaaaaaaa"));
    }
}
#endif

BOOST_AUTO_TEST_CASE(CborUint64BitEdgeCasesTestCase)
{
    std::vector<uint8_t> encodedClassic(9);
    std::vector<uint8_t> encodedFast(9);
    for (std::size_t i = 0; i < testValuesPlusEncodedSizes.size(); ++i) {
        const pairVS & p = testValuesPlusEncodedSizes[i];
        const uint64_t valueToEncode = p.first;
        const unsigned int expectedEncodingSize = p.second;

        //fail encoding if buffer too small (must return encoding size 0)
        BOOST_REQUIRE_EQUAL(CborEncodeU64Classic(&encodedClassic[0], valueToEncode, expectedEncodingSize - 1), 0);
        //encode classic buf size 9
        encodedClassic.assign(encodedClassic.size(), 0);
        unsigned int encodedSizeClassic = CborEncodeU64ClassicBufSize9(&encodedClassic[0], valueToEncode);
        BOOST_REQUIRE_EQUAL(encodedSizeClassic, expectedEncodingSize);
        //encode classic
        encodedClassic.assign(encodedClassic.size(), 0);
        encodedSizeClassic = CborEncodeU64Classic(&encodedClassic[0], valueToEncode, expectedEncodingSize);
        BOOST_REQUIRE_EQUAL(encodedSizeClassic, expectedEncodingSize);
        
        

        //decode classic
        uint8_t numBytesTakenToDecode;
        uint64_t decodedValueClassic = CborDecodeU64Classic(encodedClassic.data(), &numBytesTakenToDecode, expectedEncodingSize);
        BOOST_REQUIRE_EQUAL((unsigned int)numBytesTakenToDecode, expectedEncodingSize);
        BOOST_REQUIRE_EQUAL(decodedValueClassic, valueToEncode);
        //decode classic buf size 9
        decodedValueClassic = CborDecodeU64ClassicBufSize9(encodedClassic.data(), &numBytesTakenToDecode);
        BOOST_REQUIRE_EQUAL((unsigned int)numBytesTakenToDecode, expectedEncodingSize);
        BOOST_REQUIRE_EQUAL(decodedValueClassic, valueToEncode);
        //fail decoding if buffer too small (numBytesTakenToDecode must be 0)
        CborDecodeU64Classic(encodedClassic.data(), &numBytesTakenToDecode, expectedEncodingSize - 1);
        BOOST_REQUIRE_EQUAL((unsigned int)numBytesTakenToDecode, 0);

#ifdef USE_CBOR_FAST

        
        //fail encoding if buffer too small (must return encoding size 0)
        BOOST_REQUIRE_EQUAL(CborEncodeU64Fast(&encodedFast[0], valueToEncode, expectedEncodingSize - 1), 0);
        //encode fast
        encodedFast.assign(encodedFast.size(), 0);
        unsigned int encodedSizeFast = CborEncodeU64Fast(&encodedFast[0], valueToEncode, expectedEncodingSize);
        BOOST_REQUIRE_EQUAL(encodedSizeFast, expectedEncodingSize);
        //encode fast buf size 9
        encodedFast.assign(encodedFast.size(), 0);
        encodedSizeFast = CborEncodeU64FastBufSize9(&encodedFast[0], valueToEncode);
        BOOST_REQUIRE_EQUAL(encodedSizeFast, expectedEncodingSize);
        

        //decode fast
        uint64_t decodedValueFast = CborDecodeU64Fast(encodedFast.data(), &numBytesTakenToDecode, expectedEncodingSize);
        BOOST_REQUIRE_EQUAL((unsigned int)numBytesTakenToDecode, expectedEncodingSize);
        BOOST_REQUIRE_EQUAL(decodedValueFast, valueToEncode);
        //decode fast buf size 9
        decodedValueFast = CborDecodeU64FastBufSize9(encodedFast.data(), &numBytesTakenToDecode);
        BOOST_REQUIRE_EQUAL((unsigned int)numBytesTakenToDecode, expectedEncodingSize);
        BOOST_REQUIRE_EQUAL(decodedValueFast, valueToEncode);
        //fail decoding if buffer too small (numBytesTakenToDecode must be 0)
        CborDecodeU64Fast(encodedFast.data(), &numBytesTakenToDecode, expectedEncodingSize - 1);
        BOOST_REQUIRE_EQUAL((unsigned int)numBytesTakenToDecode, 0);

#endif //#ifdef USE_CBOR_FAST

    }

}


BOOST_AUTO_TEST_CASE(CborUint64BitSpeedTestCase, *boost::unit_test::disabled())
{
#define SPEED_TEST_LARGE_ENCODINGS 1
#if SPEED_TEST_LARGE_ENCODINGS
    std::vector<pairVS> testValuesPlusEncodedSizes2(testValuesPlusEncodedSizes.cbegin() + 45, testValuesPlusEncodedSizes.cend());
    for (uint64_t i = 5; i < 70; ++i) {
        testValuesPlusEncodedSizes2.push_back(pairVS(UINT64_MAX - i, 9));
    }
    
#else
    const std::vector<pairVS> testValuesPlusEncodedSizes2(testValuesPlusEncodedSizes.cbegin() + 21, testValuesPlusEncodedSizes.cend()); //create an even mix of various encoding sizes
#endif
    
    // shuffle to prevent branch prediction
    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(testValuesPlusEncodedSizes2.begin(), testValuesPlusEncodedSizes2.end(), g);
    // https://stackoverflow.com/questions/45013977/random-shuffle-is-not-a-member-of-std-error
    //random_shuffle was deprecated in C++14 and completely removed in C++17.
    //std::random_shuffle(testValuesPlusEncodedSizes2.begin(), testValuesPlusEncodedSizes2.end());

    unsigned int totalExpectedEncodingSize = 0;
    std::vector<uint64_t> allExpectedDecodedValues;
    allExpectedDecodedValues.reserve(testValuesPlusEncodedSizes2.size());
    for (std::size_t i = 0; i < testValuesPlusEncodedSizes2.size(); ++i) {
        allExpectedDecodedValues.push_back(testValuesPlusEncodedSizes2[i].first);
        totalExpectedEncodingSize += testValuesPlusEncodedSizes2[i].second;
    }

    std::vector<uint8_t> allEncodedDataClassic(testValuesPlusEncodedSizes2.size() * 9);
    std::vector<uint8_t> allEncodedDataFast(testValuesPlusEncodedSizes2.size() * 9);
    std::size_t totalBytesEncodedClassic = 0;
    std::size_t totalBytesEncodedClassicBufSize9 = 0;
    std::size_t totalBytesEncodedFast = 0;
    std::size_t totalBytesEncodedFastBufSize9 = 0;
    std::cout << "starting speed test\n";
    std::cout << "testValuesPlusEncodedSizes2 size: " << testValuesPlusEncodedSizes2.size() << std::endl;

    static const std::size_t LOOP_COUNT = 5000000;
    //ENCODE ARRAY OF VALS (CLASSIC)
    {
        std::cout << "encode classic\n";
        boost::timer::auto_cpu_timer t;
        for (std::size_t loopI = 0; loopI < LOOP_COUNT; ++loopI) {
            uint8_t * allEncodedDataPtr = allEncodedDataClassic.data();
            for (std::size_t i = 0; i < testValuesPlusEncodedSizes2.size(); ++i) {
                const uint64_t valueToEncode = testValuesPlusEncodedSizes2[i].first;
                //encode
                const unsigned int encodedSize = CborEncodeU64Classic(allEncodedDataPtr, valueToEncode, 9);
                allEncodedDataPtr += encodedSize;
                totalBytesEncodedClassic += encodedSize;
            }
        }
        //std::cout << "totalBytesEncoded " << totalBytesEncoded << std::endl;
        BOOST_REQUIRE_EQUAL(totalBytesEncodedClassic, totalExpectedEncodingSize * LOOP_COUNT);
    }

    //ENCODE ARRAY OF VALS (CLASSIC buf size 9)
    {
        std::cout << "encode classic buf size 9\n";
        boost::timer::auto_cpu_timer t;
        for (std::size_t loopI = 0; loopI < LOOP_COUNT; ++loopI) {
            uint8_t * allEncodedDataPtr = allEncodedDataClassic.data();
            for (std::size_t i = 0; i < testValuesPlusEncodedSizes2.size(); ++i) {
                const uint64_t valueToEncode = testValuesPlusEncodedSizes2[i].first;
                //encode
                const unsigned int encodedSize = CborEncodeU64ClassicBufSize9(allEncodedDataPtr, valueToEncode);
                allEncodedDataPtr += encodedSize;
                totalBytesEncodedClassicBufSize9 += encodedSize;
            }
        }
        //std::cout << "totalBytesEncoded " << totalBytesEncoded << std::endl;
        BOOST_REQUIRE_EQUAL(totalBytesEncodedClassicBufSize9, totalExpectedEncodingSize * LOOP_COUNT);
    }

#ifdef USE_CBOR_FAST
    //ENCODE ARRAY OF VALS (FAST)
    {
        std::cout << "encode fast\n";
        boost::timer::auto_cpu_timer t;
        for (std::size_t loopI = 0; loopI < LOOP_COUNT; ++loopI) {
            uint8_t * allEncodedDataPtr = allEncodedDataFast.data();
            for (std::size_t i = 0; i < testValuesPlusEncodedSizes2.size(); ++i) {
                const uint64_t valueToEncode = testValuesPlusEncodedSizes2[i].first;
                //encode
                const unsigned int encodedSize = CborEncodeU64Fast(allEncodedDataPtr, valueToEncode, 9);
                allEncodedDataPtr += encodedSize;
                totalBytesEncodedFast += encodedSize;
            }
        }
        //std::cout << "totalBytesEncoded " << totalBytesEncoded << std::endl;
        BOOST_REQUIRE_EQUAL(totalBytesEncodedFast, totalExpectedEncodingSize * LOOP_COUNT);
    }

    //ENCODE ARRAY OF VALS (FAST buf size 9)
    {
        std::cout << "encode fast buf size 9\n";
        boost::timer::auto_cpu_timer t;
        for (std::size_t loopI = 0; loopI < LOOP_COUNT; ++loopI) {
            uint8_t * allEncodedDataPtr = allEncodedDataFast.data();
            for (std::size_t i = 0; i < testValuesPlusEncodedSizes2.size(); ++i) {
                const uint64_t valueToEncode = testValuesPlusEncodedSizes2[i].first;
                //encode
                const unsigned int encodedSize = CborEncodeU64FastBufSize9(allEncodedDataPtr, valueToEncode);
                allEncodedDataPtr += encodedSize;
                totalBytesEncodedFastBufSize9 += encodedSize;
            }
        }
        //std::cout << "totalBytesEncoded " << totalBytesEncoded << std::endl;
        BOOST_REQUIRE_EQUAL(totalBytesEncodedFastBufSize9, totalExpectedEncodingSize * LOOP_COUNT);
    }
#endif

    //DECODE ARRAY OF VALS (CLASSIC)
    {
        std::cout << "decode classic\n";
        std::size_t totalBytesDecoded = 0;
        std::vector<uint64_t> allDecodedVals(testValuesPlusEncodedSizes2.size());
        std::size_t j;
        boost::timer::auto_cpu_timer t;
        for (std::size_t loopI = 0; loopI < LOOP_COUNT; ++loopI) {
            const uint8_t * allEncodedDataPtr = allEncodedDataClassic.data();
            j = 0;
            uint64_t * allDecodedDataPtr = allDecodedVals.data();
            while (j < totalExpectedEncodingSize) {
                //return decoded value (0 if failure), also set parameter numBytes taken to decode
                uint8_t numBytesTakenToDecode;
                const uint64_t decodedVal = CborDecodeU64Classic(allEncodedDataPtr, &numBytesTakenToDecode, 9);
                *allDecodedDataPtr++ = decodedVal;
                allEncodedDataPtr += numBytesTakenToDecode;
                totalBytesDecoded += numBytesTakenToDecode;
                j += numBytesTakenToDecode;
            }
        }
        BOOST_REQUIRE_EQUAL(j, totalExpectedEncodingSize);
        BOOST_REQUIRE_EQUAL(totalBytesDecoded, totalExpectedEncodingSize * LOOP_COUNT);
        BOOST_REQUIRE(allDecodedVals == allExpectedDecodedValues);
    }

    //DECODE ARRAY OF VALS (CLASSIC buf size 9)
    {
        std::cout << "decode classic buf size 9\n";
        std::size_t totalBytesDecoded = 0;
        std::vector<uint64_t> allDecodedVals(testValuesPlusEncodedSizes2.size());
        std::size_t j;
        boost::timer::auto_cpu_timer t;
        for (std::size_t loopI = 0; loopI < LOOP_COUNT; ++loopI) {
            const uint8_t * allEncodedDataPtr = allEncodedDataClassic.data();
            j = 0;
            uint64_t * allDecodedDataPtr = allDecodedVals.data();
            while (j < totalExpectedEncodingSize) {
                //return decoded value (0 if failure), also set parameter numBytes taken to decode
                uint8_t numBytesTakenToDecode;
                const uint64_t decodedVal = CborDecodeU64ClassicBufSize9(allEncodedDataPtr, &numBytesTakenToDecode);
                *allDecodedDataPtr++ = decodedVal;
                allEncodedDataPtr += numBytesTakenToDecode;
                totalBytesDecoded += numBytesTakenToDecode;
                j += numBytesTakenToDecode;
            }
        }
        BOOST_REQUIRE_EQUAL(j, totalExpectedEncodingSize);
        BOOST_REQUIRE_EQUAL(totalBytesDecoded, totalExpectedEncodingSize * LOOP_COUNT);
        BOOST_REQUIRE(allDecodedVals == allExpectedDecodedValues);
    }

#ifdef USE_CBOR_FAST
    //DECODE ARRAY OF VALS (FAST)
    {
        std::cout << "decode fast\n";
        std::size_t totalBytesDecoded = 0;
        std::vector<uint64_t> allDecodedVals(testValuesPlusEncodedSizes2.size());
        std::size_t j;
        boost::timer::auto_cpu_timer t;
        for (std::size_t loopI = 0; loopI < LOOP_COUNT; ++loopI) {
            const uint8_t * allEncodedDataPtr = allEncodedDataClassic.data();
            j = 0;
            uint64_t * allDecodedDataPtr = allDecodedVals.data();
            while (j < totalExpectedEncodingSize) {
                //return decoded value (0 if failure), also set parameter numBytes taken to decode
                uint8_t numBytesTakenToDecode;
                const uint64_t decodedVal = CborDecodeU64Fast(allEncodedDataPtr, &numBytesTakenToDecode, 9);
                *allDecodedDataPtr++ = decodedVal;
                allEncodedDataPtr += numBytesTakenToDecode;
                totalBytesDecoded += numBytesTakenToDecode;
                j += numBytesTakenToDecode;
            }
        }
        BOOST_REQUIRE_EQUAL(j, totalExpectedEncodingSize);
        BOOST_REQUIRE_EQUAL(totalBytesDecoded, totalExpectedEncodingSize * LOOP_COUNT);
        BOOST_REQUIRE(allDecodedVals == allExpectedDecodedValues);
    }

    //DECODE ARRAY OF VALS (FAST buf size 9)
    {
        std::cout << "decode fast buf size 9\n";
        std::size_t totalBytesDecoded = 0;
        std::vector<uint64_t> allDecodedVals(testValuesPlusEncodedSizes2.size());
        std::size_t j;
        boost::timer::auto_cpu_timer t;
        for (std::size_t loopI = 0; loopI < LOOP_COUNT; ++loopI) {
            const uint8_t * allEncodedDataPtr = allEncodedDataClassic.data();
            j = 0;
            uint64_t * allDecodedDataPtr = allDecodedVals.data();
            while (j < totalExpectedEncodingSize) {
                //return decoded value (0 if failure), also set parameter numBytes taken to decode
                uint8_t numBytesTakenToDecode;
                const uint64_t decodedVal = CborDecodeU64FastBufSize9(allEncodedDataPtr, &numBytesTakenToDecode);
                *allDecodedDataPtr++ = decodedVal;
                allEncodedDataPtr += numBytesTakenToDecode;
                totalBytesDecoded += numBytesTakenToDecode;
                j += numBytesTakenToDecode;
            }
        }
        BOOST_REQUIRE_EQUAL(j, totalExpectedEncodingSize);
        BOOST_REQUIRE_EQUAL(totalBytesDecoded, totalExpectedEncodingSize * LOOP_COUNT);
        BOOST_REQUIRE(allDecodedVals == allExpectedDecodedValues);
    }
#endif
    
}

