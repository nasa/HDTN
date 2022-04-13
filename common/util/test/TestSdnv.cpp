/**
 * @file TestSdnv.cpp
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
#include "Sdnv.h"
#include <boost/timer/timer.hpp>
#ifdef USE_SDNV_FAST
//for __m128i
#include <immintrin.h>
#endif

BOOST_AUTO_TEST_CASE(Sdnv32BitTestCase)
{
    //before we start, it's important to make sure the vector clear and resize(0) do not change capacity (important for sdnv hardware decode operation)
    {
        std::vector<uint8_t> sdnvTempVec;
        BOOST_REQUIRE_EQUAL(sdnvTempVec.size(), 0);
        BOOST_REQUIRE_EQUAL(sdnvTempVec.capacity(), 0);
        sdnvTempVec.reserve(32);
        BOOST_REQUIRE_EQUAL(sdnvTempVec.size(), 0);
        BOOST_REQUIRE_EQUAL(sdnvTempVec.capacity(), 32);
        for (std::size_t i = 0; i < 10; ++i) {
            sdnvTempVec.push_back(static_cast<uint8_t>(i));
        }
        BOOST_REQUIRE_EQUAL(sdnvTempVec.size(), 10);
        BOOST_REQUIRE_EQUAL(sdnvTempVec.capacity(), 32);
        sdnvTempVec.clear();
        BOOST_REQUIRE_EQUAL(sdnvTempVec.size(), 0);
        BOOST_REQUIRE_EQUAL(sdnvTempVec.capacity(), 32);

        for (std::size_t i = 0; i < 10; ++i) {
            sdnvTempVec.push_back(static_cast<uint8_t>(i));
        }
        BOOST_REQUIRE_EQUAL(sdnvTempVec.size(), 10);
        BOOST_REQUIRE_EQUAL(sdnvTempVec.capacity(), 32);
        sdnvTempVec.resize(0);
        BOOST_REQUIRE_EQUAL(sdnvTempVec.size(), 0);
        BOOST_REQUIRE_EQUAL(sdnvTempVec.capacity(), 32);
    }

    std::vector<uint8_t> encoded(10);
    std::vector<uint8_t> encoded2(10);
    std::vector<uint8_t> encodedFast(10);
    encoded.assign(encoded.size(), 0);
    encodedFast.assign(encodedFast.size(), 0);
    uint8_t coverageMask = 0;
    const std::vector<uint32_t> testVals = {
        0,
        1,
        2,
        3,
        4,

        127 - 4,
        127 - 3,
        127 - 2,
        127 - 1,
        127,
        127 + 1,
        127 + 2,
        127 + 3,
        127 + 4,

        16383 - 4,
        16383 - 3,
        16383 - 2,
        16383 - 1,
        16383,
        16383 + 1,
        16383 + 2,
        16383 + 3,
        16383 + 4,

        2097151 - 4,
        2097151 - 3,
        2097151 - 2,
        2097151 - 1,
        2097151,
        2097151 + 1,
        2097151 + 2,
        2097151 + 3,
        2097151 + 4,

        268435455 - 4,
        268435455 - 3,
        268435455 - 2,
        268435455 - 1,
        268435455,
        268435455 + 1,
        268435455 + 2,
        268435455 + 3,
        268435455 + 4,

        UINT32_MAX - 4,
        UINT32_MAX - 3,
        UINT32_MAX - 2,
        UINT32_MAX - 1,
        UINT32_MAX
    };


    for (std::size_t i = 0; i < testVals.size(); ++i) {
        const uint32_t val = testVals[i];
        //std::cout << "val=" << val << std::endl;
        encoded.assign(encoded.size(), 0);
        encodedFast.assign(encodedFast.size(), 0);
        //encode val
        const unsigned int outputSizeBytes = SdnvEncodeU32ClassicBufSize5(encoded.data(), val);
#ifdef USE_SDNV_FAST
        const unsigned int outputSizeBytesFast = SdnvEncodeU32FastBufSize8(encodedFast.data(), val);
        BOOST_REQUIRE_EQUAL(outputSizeBytes, outputSizeBytesFast);
        BOOST_REQUIRE(encoded == encodedFast);
#endif
        {
            //if buffersize is at least outputSizeBytes it will succeed
            encoded2.assign(encoded.size(), 0);
            unsigned int outputSizeBytes2 = SdnvEncodeU32(encoded2.data(), val, outputSizeBytes);
            BOOST_REQUIRE(encoded == encoded2);
            BOOST_REQUIRE_EQUAL(outputSizeBytes2, outputSizeBytes);
            //repeat with explicit classic call
            encoded2.assign(encoded.size(), 0);
            outputSizeBytes2 = SdnvEncodeU32Classic(encoded2.data(), val, outputSizeBytes);
            BOOST_REQUIRE(encoded == encoded2);
            BOOST_REQUIRE_EQUAL(outputSizeBytes2, outputSizeBytes);
        }
        {
            //if buffersize is less than outputSizeBytes it will succeed
            encoded2.assign(encoded.size(), 0);
            unsigned int outputSizeBytes2 = SdnvEncodeU32(encoded2.data(), val, outputSizeBytes - 1);
            BOOST_REQUIRE_EQUAL(outputSizeBytes2, 0);
            //repeat with explicit classic call
            encoded2.assign(encoded.size(), 0);
            outputSizeBytes2 = SdnvEncodeU32Classic(encoded2.data(), val, outputSizeBytes - 1);
            BOOST_REQUIRE_EQUAL(outputSizeBytes2, 0);
        }

        //decode val classic
        uint8_t numBytesDecoded = 0;
        const uint32_t valDecoded = SdnvDecodeU32Classic(encoded.data(), &numBytesDecoded, encoded.size());
        BOOST_REQUIRE_EQUAL(outputSizeBytes, numBytesDecoded);
        BOOST_REQUIRE_EQUAL(val, valDecoded);
#ifdef USE_SDNV_FAST
        //decode val fast
        uint8_t numBytesDecodedFast = 0;
        const uint32_t valDecodedFast = SdnvDecodeU32FastBufSize8(encoded.data(), &numBytesDecodedFast);
        BOOST_REQUIRE_EQUAL(outputSizeBytes, numBytesDecodedFast);
        BOOST_REQUIRE_EQUAL(val, valDecodedFast);
#endif
        {
            //if the buffer size was outputSizeBytes it will succeed
            uint8_t numBytesDecodedWillSucceed = 0;
            const uint32_t valDecodedWillSucceed = SdnvDecodeU32(encoded.data(), &numBytesDecodedWillSucceed, outputSizeBytes); //will call classic routine
            BOOST_REQUIRE_EQUAL(outputSizeBytes, numBytesDecodedWillSucceed);
            BOOST_REQUIRE_EQUAL(val, valDecodedWillSucceed);
        }
        {
            //if the buffer size was 1 size too small it will fail
            uint8_t numBytesDecodedWillFail = UINT8_MAX;
            const uint32_t valDecodedWillFail = SdnvDecodeU32(encoded.data(), &numBytesDecodedWillFail, outputSizeBytes - 1); //will call classic routine
            BOOST_REQUIRE_NE(outputSizeBytes, numBytesDecodedWillFail);
            BOOST_REQUIRE_EQUAL(numBytesDecodedWillFail, 0);
            BOOST_REQUIRE_EQUAL(valDecodedWillFail, 0);
        }
        if (val <= 127) {
            BOOST_REQUIRE_EQUAL(outputSizeBytes, 1);
            BOOST_REQUIRE_EQUAL(val, encoded[0]); //should be equal to itself
            coverageMask |= (1 << 0);
        }
        else if (val <= 16383) {
            BOOST_REQUIRE_EQUAL(outputSizeBytes, 2);
            coverageMask |= (1 << 1);
        }
        else if (val <= 2097151) {
            BOOST_REQUIRE_EQUAL(outputSizeBytes, 3);
            coverageMask |= (1 << 2);
        }
        else if (val <= 268435455) {
            BOOST_REQUIRE_EQUAL(outputSizeBytes, 4);
            coverageMask |= (1 << 3);
        }
        else {
            BOOST_REQUIRE_EQUAL(outputSizeBytes, 5);
            coverageMask |= (1 << 4);
        }


    }
    BOOST_REQUIRE_EQUAL(coverageMask, 0x1f);

    //ENCODE ARRAY OF VALS
    std::vector<uint8_t> allEncodedData(testVals.size() * 5);
    std::size_t totalBytesEncoded = 0;
    uint8_t * allEncodedDataPtr = allEncodedData.data();
#ifdef USE_SDNV_FAST
    std::vector<uint8_t> allEncodedDataFast(testVals.size() * 5 + sizeof(uint64_t));
    std::size_t totalBytesEncodedFast = 0;
    uint8_t * allEncodedDataFastPtr = allEncodedDataFast.data();
#endif
    for (std::size_t i = 0; i < testVals.size(); ++i) {
        const uint32_t val = testVals[i];
        //encode
        const unsigned int outputSizeBytes = SdnvEncodeU32ClassicBufSize5(allEncodedDataPtr, val);
        allEncodedDataPtr += outputSizeBytes;
        totalBytesEncoded += outputSizeBytes;
#ifdef USE_SDNV_FAST
        //encode fast
        const unsigned int outputSizeBytesFast = SdnvEncodeU32FastBufSize8(allEncodedDataFastPtr, val);
        allEncodedDataFastPtr += outputSizeBytesFast;
        totalBytesEncodedFast += outputSizeBytesFast;
#endif
    }
    BOOST_REQUIRE_EQUAL(totalBytesEncoded, 136);
    allEncodedData.resize(totalBytesEncoded);
#ifdef USE_SDNV_FAST
    BOOST_REQUIRE_EQUAL(totalBytesEncoded, totalBytesEncodedFast);
    allEncodedDataFast.resize(totalBytesEncodedFast + sizeof(uint64_t));
#endif


    //DECODE ARRAY OF VALS
    allEncodedDataPtr = allEncodedData.data();
    std::vector<uint32_t> allDecodedVals;
#ifdef USE_SDNV_FAST
    allEncodedDataFastPtr = allEncodedDataFast.data();
    std::vector<uint32_t> allDecodedValsFast;
#endif
    std::size_t j = 0;
    while (j < totalBytesEncoded) {
        //return decoded value (0 if failure), also set parameter numBytes taken to decode
        uint8_t numBytesTakenToDecode;
        const uint32_t decodedVal = SdnvDecodeU32Classic(allEncodedDataPtr, &numBytesTakenToDecode, 8); //will always be at least 8 byte buffer
        //std::cout << decodedVal << " " << (int)numBytesTakenToDecode << "\n";
        BOOST_REQUIRE_NE(numBytesTakenToDecode, 0);
        allDecodedVals.push_back(decodedVal);
        allEncodedDataPtr += numBytesTakenToDecode;
#ifdef USE_SDNV_FAST
        uint8_t numBytesTakenToDecodeFast;
        const uint32_t decodedValFast = SdnvDecodeU32FastBufSize8(allEncodedDataFastPtr, &numBytesTakenToDecodeFast);
        //std::cout << decodedVal << " " << (int)numBytesTakenToDecode << "\n";
        BOOST_REQUIRE_NE(numBytesTakenToDecodeFast, 0);
        allDecodedValsFast.push_back(decodedValFast);
        allEncodedDataFastPtr += numBytesTakenToDecodeFast;

        BOOST_REQUIRE_EQUAL(numBytesTakenToDecodeFast, numBytesTakenToDecode);
#endif

        j += numBytesTakenToDecode;
    }
    BOOST_REQUIRE_EQUAL(j, totalBytesEncoded);
    BOOST_REQUIRE(allDecodedVals == testVals);
#ifdef USE_SDNV_FAST
    BOOST_REQUIRE(allDecodedValsFast == testVals);
    allEncodedDataFast.resize(totalBytesEncodedFast);
    BOOST_REQUIRE(allEncodedData == allEncodedDataFast);
#endif
}

static const std::vector<uint64_t> testVals = {
    0,
    1,
    2,
    3,
    4,

    // (1 << 7) - 1
    127 - 4,
    127 - 3,
    127 - 2,
    127 - 1,
    127,
    127 + 1,
    127 + 2,
    127 + 3,
    127 + 4,

    // (1 << 14) - 1
    16383 - 4,
    16383 - 3,
    16383 - 2,
    16383 - 1,
    16383,
    16383 + 1,
    16383 + 2,
    16383 + 3,
    16383 + 4,

    // (1 << 21) - 1
    2097151 - 4,
    2097151 - 3,
    2097151 - 2,
    2097151 - 1,
    2097151,
    2097151 + 1,
    2097151 + 2,
    2097151 + 3,
    2097151 + 4,

    // (1 << 28) - 1
    268435455 - 4,
    268435455 - 3,
    268435455 - 2,
    268435455 - 1,
    268435455,
    268435455 + 1,
    268435455 + 2,
    268435455 + 3,
    268435455 + 4,

    static_cast<uint64_t>(UINT32_MAX) - 4,
    static_cast<uint64_t>(UINT32_MAX) - 3,
    static_cast<uint64_t>(UINT32_MAX) - 2,
    static_cast<uint64_t>(UINT32_MAX) - 1,
    static_cast<uint64_t>(UINT32_MAX),
    static_cast<uint64_t>(UINT32_MAX) + 1,
    static_cast<uint64_t>(UINT32_MAX) + 2,
    static_cast<uint64_t>(UINT32_MAX) + 3,
    static_cast<uint64_t>(UINT32_MAX) + 4,

    // (1 << 35) - 1
    34359738367 - 4,
    34359738367 - 3,
    34359738367 - 2,
    34359738367 - 1,
    34359738367,
    34359738367 + 1,
    34359738367 + 2,
    34359738367 + 3,
    34359738367 + 4,

    // (1 << 42) - 1
    4398046511103 - 4,
    4398046511103 - 3,
    4398046511103 - 2,
    4398046511103 - 1,
    4398046511103,
    4398046511103 + 1,
    4398046511103 + 2,
    4398046511103 + 3,
    4398046511103 + 4,

    // (1 << 49) - 1
    562949953421311 - 4,
    562949953421311 - 3,
    562949953421311 - 2,
    562949953421311 - 1,
    562949953421311,
    562949953421311 + 1,
    562949953421311 + 2,
    562949953421311 + 3,
    562949953421311 + 4,

    // (1 << 56) - 1
    72057594037927935 - 4,
    72057594037927935 - 3,
    72057594037927935 - 2,
    72057594037927935 - 1,
    72057594037927935,
    72057594037927935 + 1,
    72057594037927935 + 2,
    72057594037927935 + 3,
    72057594037927935 + 4,

    // (1 << 63) - 1
    9223372036854775807U - 4,
    9223372036854775807U - 3,
    9223372036854775807U - 2,
    9223372036854775807U - 1,
    9223372036854775807U,
    9223372036854775807U + 1,
    9223372036854775807U + 2,
    9223372036854775807U + 3,
    9223372036854775807U + 4,

    UINT64_MAX - 4,
    UINT64_MAX - 3,
    UINT64_MAX - 2,
    UINT64_MAX - 1,
    UINT64_MAX
};

BOOST_AUTO_TEST_CASE(Sdnv32BitErrorDecodeTestCase)
{
    std::vector<uint8_t> encoded(2 * sizeof(uint64_t)); //sizeof(__m128i)

    uint8_t numBytesTakenToDecode = UINT8_MAX;
    uint32_t decodedVal;
    encoded.assign(encoded.size(), 0xff); //never ending sdnv
    decodedVal = SdnvDecodeU32Classic(encoded.data(), &numBytesTakenToDecode, encoded.size());
    BOOST_REQUIRE_EQUAL(numBytesTakenToDecode, 0); //expect invalid (encoded sdnv > 5 bytes)
#ifdef USE_SDNV_FAST
    numBytesTakenToDecode = UINT8_MAX;
    decodedVal = SdnvDecodeU32FastBufSize8(encoded.data(), &numBytesTakenToDecode);
    BOOST_REQUIRE_EQUAL(numBytesTakenToDecode, 0); //expect invalid (encoded sdnv > 5 bytes)
#endif
    encoded.assign(encoded.size(), 0);
    const unsigned int outputSizeBytes = SdnvEncodeU32ClassicBufSize5(encoded.data(), UINT32_MAX);
    BOOST_REQUIRE_EQUAL(outputSizeBytes, 5);
    BOOST_REQUIRE_EQUAL(encoded[0], 0x8f); //f -> bits 29,30,31,32
    BOOST_REQUIRE_EQUAL(encoded[3], 0xff);
    BOOST_REQUIRE_EQUAL(encoded[4], 0x7f); //least significant byte with sdnv stopping msb set to 0

    for (uint8_t byteMakesAbove32Bit = 0x90; byteMakesAbove32Bit < 0xff; ++byteMakesAbove32Bit) { //0x90 makes 33 bit sdnv
        encoded[0] = byteMakesAbove32Bit;
        //for (std::size_t i = 0; i < encoded.size(); ++i) {
        //    printf("%02x ", encoded[i]);
        //}
        //std::cout << std::endl;

        numBytesTakenToDecode = UINT8_MAX;
        decodedVal = SdnvDecodeU32Classic(encoded.data(), &numBytesTakenToDecode, encoded.size());
        BOOST_REQUIRE_EQUAL(numBytesTakenToDecode, 0); //expect invalid (decoded value would be > UINT32_MAX)
#ifdef USE_SDNV_FAST
        numBytesTakenToDecode = UINT8_MAX;
        decodedVal = SdnvDecodeU32FastBufSize8(encoded.data(), &numBytesTakenToDecode);
        BOOST_REQUIRE_EQUAL(numBytesTakenToDecode, 0); //expect invalid (decoded value would be > UINT32_MAX)
#endif
    }
}

BOOST_AUTO_TEST_CASE(Sdnv64BitErrorDecodeTestCase)
{
    std::vector<uint8_t> encoded(2 * sizeof(uint64_t)); //sizeof(__m128i)
    encoded.assign(encoded.size(), 0xff); //never ending sdnv

    uint8_t numBytesTakenToDecode = UINT8_MAX;
    uint64_t decodedVal;
    decodedVal = SdnvDecodeU64Classic(encoded.data(), &numBytesTakenToDecode, encoded.size());
    BOOST_REQUIRE_EQUAL(numBytesTakenToDecode, 0); //expect invalid (encoded sdnv > 10 bytes)
#ifdef USE_SDNV_FAST
    numBytesTakenToDecode = UINT8_MAX;
    decodedVal = SdnvDecodeU64FastBufSize16(encoded.data(), &numBytesTakenToDecode);
    BOOST_REQUIRE_EQUAL(numBytesTakenToDecode, 0); //expect invalid (encoded sdnv > 10 bytes)
#endif
    encoded.assign(encoded.size(), 0);
    const unsigned int outputSizeBytes = SdnvEncodeU64ClassicBufSize10(encoded.data(), UINT64_MAX);
    BOOST_REQUIRE_EQUAL(outputSizeBytes, 10);
    BOOST_REQUIRE_EQUAL(encoded[0], 0x81); //1 -> 64th bit
    BOOST_REQUIRE_EQUAL(encoded[8], 0xff);
    BOOST_REQUIRE_EQUAL(encoded[9], 0x7f); //least significant byte with sdnv stopping msb set to 0

    for (uint8_t byteMakesAbove64Bit = 0x82; byteMakesAbove64Bit < 0xff; ++byteMakesAbove64Bit) { //0x82 makes 65 bit sdnv
        encoded[0] = byteMakesAbove64Bit;
        //for (std::size_t i = 0; i < encoded.size(); ++i) {
        //    printf("%02x ", encoded[i]);
        //}
        //std::cout << std::endl;

        numBytesTakenToDecode = UINT8_MAX;
        decodedVal = SdnvDecodeU64Classic(encoded.data(), &numBytesTakenToDecode, encoded.size());
        BOOST_REQUIRE_EQUAL(numBytesTakenToDecode, 0); //expect invalid (decoded value would be > UINT64_MAX)
#ifdef USE_SDNV_FAST
        numBytesTakenToDecode = UINT8_MAX;
        decodedVal = SdnvDecodeU64FastBufSize16(encoded.data(), &numBytesTakenToDecode);
        BOOST_REQUIRE_EQUAL(numBytesTakenToDecode, 0); //expect invalid (decoded value would be > UINT64_MAX)
#endif
    }
}

BOOST_AUTO_TEST_CASE(Sdnv64BitTestCase)
{
    std::vector<uint8_t> encoded(16);
    encoded.assign(encoded.size(), 0);
    std::vector<uint8_t> encoded2(16);
    uint16_t coverageMask = 0;
#ifdef USE_SDNV_FAST
    std::vector<uint8_t> encodedFast(16);
    encodedFast.assign(encodedFast.size(), 0);
#endif


    for (std::size_t i = 0; i < testVals.size(); ++i) {
        const uint64_t val = testVals[i];
        //std::cout << "val=" << val << std::endl;
        encoded.assign(encoded.size(), 0);

        //encode val
        const unsigned int outputSizeBytes = SdnvEncodeU64ClassicBufSize10(encoded.data(), val);
#ifdef USE_SDNV_FAST
        const unsigned int outputSizeBytesFast = SdnvEncodeU64FastBufSize10(encodedFast.data(), val);
        BOOST_REQUIRE_EQUAL(outputSizeBytes, outputSizeBytesFast);
        BOOST_REQUIRE(encoded == encodedFast);
        encodedFast.assign(encodedFast.size(), 0);
#endif
        {
            //if buffersize is at least outputSizeBytes it will succeed
            encoded2.assign(encoded.size(), 0);
            unsigned int outputSizeBytes2 = SdnvEncodeU64(encoded2.data(), val, outputSizeBytes);
            BOOST_REQUIRE(encoded == encoded2);
            BOOST_REQUIRE_EQUAL(outputSizeBytes2, outputSizeBytes);
            //repeat with explicit classic call
            encoded2.assign(encoded.size(), 0);
            outputSizeBytes2 = SdnvEncodeU64Classic(encoded2.data(), val, outputSizeBytes);
            BOOST_REQUIRE(encoded == encoded2);
            BOOST_REQUIRE_EQUAL(outputSizeBytes2, outputSizeBytes);
        }
        {
            //if buffersize is less than outputSizeBytes it will succeed
            encoded2.assign(encoded.size(), 0);
            unsigned int outputSizeBytes2 = SdnvEncodeU64(encoded2.data(), val, outputSizeBytes - 1);
            BOOST_REQUIRE_EQUAL(outputSizeBytes2, 0);
            //repeat with explicit classic call
            encoded2.assign(encoded.size(), 0);
            outputSizeBytes2 = SdnvEncodeU64Classic(encoded2.data(), val, outputSizeBytes - 1);
            BOOST_REQUIRE_EQUAL(outputSizeBytes2, 0);
        }

        //decode val
        uint8_t numBytesDecoded = 0;
        const uint64_t valDecoded = SdnvDecodeU64Classic(encoded.data(), &numBytesDecoded, encoded.size());
        BOOST_REQUIRE_EQUAL(outputSizeBytes, numBytesDecoded);
        BOOST_REQUIRE_EQUAL(val, valDecoded);
#ifdef USE_SDNV_FAST
        uint8_t numBytesDecodedFast = 0;
        const uint64_t valDecodedFast = SdnvDecodeU64FastBufSize16(encoded.data(), &numBytesDecodedFast);
        BOOST_REQUIRE_EQUAL(outputSizeBytes, numBytesDecodedFast);
        BOOST_REQUIRE_EQUAL(val, valDecodedFast);
#endif
        {
            //if the buffer size was outputSizeBytes it will succeed
            uint8_t numBytesDecodedWillSucceed = 0;
            const uint64_t valDecodedWillSucceed = SdnvDecodeU64(encoded.data(), &numBytesDecodedWillSucceed, outputSizeBytes); //will call classic routine
            BOOST_REQUIRE_EQUAL(outputSizeBytes, numBytesDecodedWillSucceed);
            BOOST_REQUIRE_EQUAL(val, valDecodedWillSucceed);
        }
        {
            //if the buffer size was 1 size too small it will fail
            uint8_t numBytesDecodedWillFail = UINT8_MAX;
            const uint64_t valDecodedWillFail = SdnvDecodeU64(encoded.data(), &numBytesDecodedWillFail, outputSizeBytes - 1); //will call classic routine
            BOOST_REQUIRE_NE(outputSizeBytes, numBytesDecodedWillFail);
            BOOST_REQUIRE_EQUAL(numBytesDecodedWillFail, 0);
            BOOST_REQUIRE_EQUAL(valDecodedWillFail, 0);
        }
        if (val <= 127) {
            BOOST_REQUIRE_EQUAL(outputSizeBytes, 1);
            BOOST_REQUIRE_EQUAL(val, encoded[0]); //should be equal to itself
            coverageMask |= (1 << 0);
        }
        else if (val <= 16383) {
            BOOST_REQUIRE_EQUAL(outputSizeBytes, 2);
            coverageMask |= (1 << 1);
        }
        else if (val <= 2097151) {
            BOOST_REQUIRE_EQUAL(outputSizeBytes, 3);
            coverageMask |= (1 << 2);
        }
        else if (val <= 268435455) {
            BOOST_REQUIRE_EQUAL(outputSizeBytes, 4);
            coverageMask |= (1 << 3);
        }
        else if (val <= 34359738367) {
            BOOST_REQUIRE_EQUAL(outputSizeBytes, 5);
            coverageMask |= (1 << 4);
        }
        else if (val <= 4398046511103) {
            BOOST_REQUIRE_EQUAL(outputSizeBytes, 6);
            coverageMask |= (1 << 5);
        }
        else if (val <= 562949953421311) {
            BOOST_REQUIRE_EQUAL(outputSizeBytes, 7);
            coverageMask |= (1 << 6);
        }
        else if (val <= 72057594037927935) {
            BOOST_REQUIRE_EQUAL(outputSizeBytes, 8);
            coverageMask |= (1 << 7);
        }
        else if (val <= 9223372036854775807U) {
            BOOST_REQUIRE_EQUAL(outputSizeBytes, 9);
            coverageMask |= (1 << 8);
        }
        else {
            BOOST_REQUIRE_EQUAL(outputSizeBytes, 10);
            coverageMask |= (1 << 9);
        }


    }
    BOOST_REQUIRE_EQUAL(coverageMask, 0x3ff);

    //ENCODE ARRAY OF VALS
    std::vector<uint8_t> allEncodedData(testVals.size() * 10);
    std::size_t totalBytesEncoded = 0;
    uint8_t * allEncodedDataPtr = allEncodedData.data();
#ifdef USE_SDNV_FAST
    std::vector<uint8_t> allEncodedDataFast(testVals.size() * 10 + (2 * sizeof(uint64_t)));
    std::size_t totalBytesEncodedFast = 0;
    uint8_t * allEncodedDataFastPtr = allEncodedDataFast.data();
#endif
    for (std::size_t i = 0; i < testVals.size(); ++i) {
        const uint64_t val = testVals[i];
        //encode
        const unsigned int outputSizeBytes = SdnvEncodeU64ClassicBufSize10(allEncodedDataPtr, val);
        allEncodedDataPtr += outputSizeBytes;
        totalBytesEncoded += outputSizeBytes;
#ifdef USE_SDNV_FAST
        //encode fast
        const unsigned int outputSizeBytesFast = SdnvEncodeU64FastBufSize10(allEncodedDataFastPtr, val);
        allEncodedDataFastPtr += outputSizeBytesFast;
        totalBytesEncodedFast += outputSizeBytesFast;
#endif
    }
    //std::cout << "totalBytesEncoded " << totalBytesEncoded << std::endl;
    BOOST_REQUIRE_EQUAL(totalBytesEncoded, 541);
    allEncodedData.resize(totalBytesEncoded);
#ifdef USE_SDNV_FAST
    BOOST_REQUIRE_EQUAL(totalBytesEncoded, totalBytesEncodedFast);
    allEncodedDataFast.resize(totalBytesEncodedFast + sizeof(uint64_t));
#endif


    //DECODE ARRAY OF VALS
    allEncodedDataPtr = allEncodedData.data();
    std::vector<uint64_t> allDecodedVals;
#ifdef USE_SDNV_FAST
    allEncodedDataFastPtr = allEncodedDataFast.data();
    std::vector<uint64_t> allDecodedValsFast;
#endif
    std::size_t j = 0;
    while (j < totalBytesEncoded) {
        //return decoded value (0 if failure), also set parameter numBytes taken to decode
        uint8_t numBytesTakenToDecode;
        const uint64_t decodedVal = SdnvDecodeU64Classic(allEncodedDataPtr, &numBytesTakenToDecode, 10); //will always be at least 10 byte buffer remaining
        //std::cout << decodedVal << " " << (int)numBytesTakenToDecode << "\n";
        BOOST_REQUIRE_NE(numBytesTakenToDecode, 0);
        allDecodedVals.push_back(decodedVal);
        allEncodedDataPtr += numBytesTakenToDecode;

#ifdef USE_SDNV_FAST
        uint8_t numBytesTakenToDecodeFast;
        const uint64_t decodedValFast = SdnvDecodeU64FastBufSize16(allEncodedDataFastPtr, &numBytesTakenToDecodeFast);
        //std::cout << decodedVal << " " << (int)numBytesTakenToDecode << "\n";
        BOOST_REQUIRE_NE(numBytesTakenToDecodeFast, 0);
        allDecodedValsFast.push_back(decodedValFast);
        allEncodedDataFastPtr += numBytesTakenToDecodeFast;

        BOOST_REQUIRE_EQUAL(numBytesTakenToDecodeFast, numBytesTakenToDecode);
#endif
        j += numBytesTakenToDecode;
    }
    BOOST_REQUIRE_EQUAL(j, totalBytesEncoded);
    BOOST_REQUIRE(allDecodedVals == testVals);
#ifdef USE_SDNV_FAST
    BOOST_REQUIRE(allDecodedValsFast == testVals);
    allEncodedDataFast.resize(totalBytesEncodedFast);
    BOOST_REQUIRE(allEncodedData == allEncodedDataFast);
#endif

#ifdef USE_SDNV_FAST
    //DECODE UP TO 16-BYTES AT A TIME ARRAY OF VALS
    {
        allEncodedDataPtr = allEncodedData.data();
        std::vector<uint64_t> allDecodedValsFastMultiple(testVals.size());
        uint64_t * allDecodedDataFastMultiplePtr = allDecodedValsFastMultiple.data();
        j = 0;
        unsigned int decodedRemaining = (unsigned int)testVals.size();
        while (j < totalBytesEncoded) {
            //return decoded value (0 if failure), also set parameter numBytes taken to decode
            uint8_t totalBytesDecoded;
            unsigned int numValsDecodedThisIteration = SdnvDecodeMultipleU64Fast(allEncodedDataPtr, &totalBytesDecoded, allDecodedDataFastMultiplePtr, decodedRemaining);
            decodedRemaining -= numValsDecodedThisIteration;
            allDecodedDataFastMultiplePtr += numValsDecodedThisIteration;
            //std::cout << decodedVal << " " << (int)numBytesTakenToDecode << "\n";
            BOOST_REQUIRE_NE(totalBytesDecoded, 0);
            allEncodedDataPtr += totalBytesDecoded;

            j += totalBytesDecoded;
        }
        BOOST_REQUIRE_EQUAL(j, totalBytesEncoded);
        BOOST_REQUIRE(allDecodedValsFastMultiple == testVals);
    }

# ifdef SDNV_SUPPORT_AVX2_FUNCTIONS //must also support USE_SDNV_FAST
    //DECODE UP TO 32-BYTES AT A TIME ARRAY OF VALS
    {
        allEncodedDataPtr = allEncodedData.data();
        std::vector<uint64_t> allDecodedValsFastMultiple32(testVals.size());
        uint64_t * allDecodedDataFastMultiple32Ptr = allDecodedValsFastMultiple32.data();
        j = 0;
        unsigned int decodedRemaining = (unsigned int)testVals.size();
        while (j < totalBytesEncoded) {
            //return decoded value (0 if failure), also set parameter numBytes taken to decode
            uint8_t totalBytesDecoded;
            unsigned int numValsDecodedThisIteration = SdnvDecodeMultiple256BitU64Fast(allEncodedDataPtr, &totalBytesDecoded, allDecodedDataFastMultiple32Ptr, decodedRemaining);
            decodedRemaining -= numValsDecodedThisIteration;
            allDecodedDataFastMultiple32Ptr += numValsDecodedThisIteration;
            //std::cout << decodedVal << " " << (int)numBytesTakenToDecode << "\n";
            BOOST_REQUIRE_NE(totalBytesDecoded, 0);
            allEncodedDataPtr += totalBytesDecoded;

            j += totalBytesDecoded;
        }
        BOOST_REQUIRE_EQUAL(j, totalBytesEncoded);
        BOOST_REQUIRE(allDecodedValsFastMultiple32 == testVals);
    }

    //DECODE UP TO 32-BYTES AT A TIME ARRAY OF VALS using SdnvDecodeArrayU64Fast
    {
        std::vector<uint64_t> allDecodedValsFastMultiple32(testVals.size());
        allDecodedValsFastMultiple32.assign(allDecodedValsFastMultiple32.size(), 0); //make sure the SdnvDecodeArrayU64Fast function actually writes data
        uint64_t numBytesTakenToDecode;
        const unsigned int numValuesActuallyDecoded = SdnvDecodeArrayU64Fast(
            allEncodedData.data(),
            numBytesTakenToDecode,
            allDecodedValsFastMultiple32.data(),
            static_cast<unsigned int>(allDecodedValsFastMultiple32.size()),
            allEncodedData.size()
        );
        BOOST_REQUIRE_EQUAL(allDecodedValsFastMultiple32.size(), numValuesActuallyDecoded);
        
        BOOST_REQUIRE_EQUAL(numBytesTakenToDecode, totalBytesEncoded);
        BOOST_REQUIRE(allDecodedValsFastMultiple32 == testVals);
    }

    //DECODE UP TO 32-BYTES AT A TIME ARRAY OF VALS using SdnvDecodeArrayU64Fast (same as above but force a partial decode)
    {
        std::vector<uint64_t> allDecodedValsFastMultiple32(testVals.size() * 2); //double expected number to emulate a partial decode
        allDecodedValsFastMultiple32.assign(allDecodedValsFastMultiple32.size(), 0); //make sure the SdnvDecodeArrayU64Fast function actually writes data
        uint64_t numBytesTakenToDecode;
        const unsigned int numValuesActuallyDecoded = SdnvDecodeArrayU64Fast(
            allEncodedData.data(),
            numBytesTakenToDecode,
            allDecodedValsFastMultiple32.data(),
            static_cast<unsigned int>(allDecodedValsFastMultiple32.size()), 
            allEncodedData.size()
        );
        BOOST_REQUIRE_EQUAL(allDecodedValsFastMultiple32.size(), numValuesActuallyDecoded * 2); //actually decoded half of expected

        BOOST_REQUIRE_EQUAL(numBytesTakenToDecode, totalBytesEncoded);
        allDecodedValsFastMultiple32.resize(numValuesActuallyDecoded); //resize before comparision
        BOOST_REQUIRE(allDecodedValsFastMultiple32 == testVals);
    }

    //DECODE UP TO 32-BYTES AT A TIME ARRAY OF VALS using SdnvDecodeArrayU64Fast and 1 byte sdnvs
    for(uint8_t val=0; val < 100; ++val) { //all these vals are equivalent as encoded and decoded since they are <= 127
        std::vector<uint64_t> allDecodedValsFastMultiple32(val + 1);
        allDecodedValsFastMultiple32.reserve(allDecodedValsFastMultiple32.size() * 3); //triple capacity for partial decode test scheme below
        std::vector<uint64_t> allExpectedDecodedValsFastMultiple32(val + 1);
        std::vector<uint8_t> allEncoded1ByteVals(100);
        for (std::size_t i = 0; i < allDecodedValsFastMultiple32.size(); ++i) {
            allEncoded1ByteVals[i] = static_cast<uint8_t>(i);
            allExpectedDecodedValsFastMultiple32[i] = i;
        }
        
        for (unsigned int scheme = 0; scheme < 3; ++scheme) {
            allDecodedValsFastMultiple32.assign(allDecodedValsFastMultiple32.size(), UINT8_MAX); //make sure the SdnvDecodeArrayU64Fast function actually writes data
            uint64_t encodedBufferSize;
            unsigned int numSdnvsToDecode;
            if (scheme == 0) {
                encodedBufferSize = allDecodedValsFastMultiple32.size(); //min size encoded buffer size
                numSdnvsToDecode = static_cast<unsigned int>(allDecodedValsFastMultiple32.size());
            }
            else if (scheme == 1) {
                encodedBufferSize = 100; //larger encoded buffer size to force avx operations
                numSdnvsToDecode = static_cast<unsigned int>(allDecodedValsFastMultiple32.size());
            }
            else {
                encodedBufferSize = allDecodedValsFastMultiple32.size(); //min size encoded buffer size
                numSdnvsToDecode = static_cast<unsigned int>(allDecodedValsFastMultiple32.size() * 3); //parital decode scheme (try decode more sdnvs than available)      
            }
                
            uint64_t numBytesTakenToDecode;
            const unsigned int numValuesActuallyDecoded = SdnvDecodeArrayU64Fast(
                allEncoded1ByteVals.data(),
                numBytesTakenToDecode,
                allDecodedValsFastMultiple32.data(),
                numSdnvsToDecode,
                encodedBufferSize
            );
            BOOST_REQUIRE_EQUAL(allDecodedValsFastMultiple32.size(), numValuesActuallyDecoded);
            if (scheme != 2) {
                BOOST_REQUIRE_EQUAL(numValuesActuallyDecoded, numSdnvsToDecode);
            }
            else {
                BOOST_REQUIRE_EQUAL(numValuesActuallyDecoded * 3, numSdnvsToDecode);
            }

            BOOST_REQUIRE_EQUAL(numBytesTakenToDecode, allDecodedValsFastMultiple32.size());
            BOOST_REQUIRE(allDecodedValsFastMultiple32 == allExpectedDecodedValsFastMultiple32);
        }
    }
# endif //#ifdef SDNV_SUPPORT_AVX2_FUNCTIONS
#endif //#ifdef USE_SDNV_FAST

    //DECODE ARRAY OF VALS using SdnvDecodeArrayU64Classic (same as above but switch out SdnvDecodeArrayU64Fast for SdnvDecodeArrayU64Classic)
    {
        std::vector<uint64_t> allDecodedValsFastMultiple32(testVals.size());
        allDecodedValsFastMultiple32.assign(allDecodedValsFastMultiple32.size(), 0); //make sure the SdnvDecodeArrayU64Fast function actually writes data
        uint64_t numBytesTakenToDecode;
        const unsigned int numValuesActuallyDecoded = SdnvDecodeArrayU64Classic(
            allEncodedData.data(),
            numBytesTakenToDecode,
            allDecodedValsFastMultiple32.data(),
            static_cast<unsigned int>(allDecodedValsFastMultiple32.size()),
            allEncodedData.size()
        );
        BOOST_REQUIRE_EQUAL(allDecodedValsFastMultiple32.size(), numValuesActuallyDecoded);

        BOOST_REQUIRE_EQUAL(numBytesTakenToDecode, totalBytesEncoded);
        BOOST_REQUIRE(allDecodedValsFastMultiple32 == testVals);
    }

    //DECODE ARRAY OF VALS using SdnvDecodeArrayU64Classic (same as above but force a partial decode)
    {
        std::vector<uint64_t> allDecodedValsFastMultiple32(testVals.size() * 2); //double expected number to emulate a partial decode
        allDecodedValsFastMultiple32.assign(allDecodedValsFastMultiple32.size(), 0); //make sure the SdnvDecodeArrayU64Fast function actually writes data
        uint64_t numBytesTakenToDecode;
        const unsigned int numValuesActuallyDecoded = SdnvDecodeArrayU64Classic(
            allEncodedData.data(),
            numBytesTakenToDecode,
            allDecodedValsFastMultiple32.data(),
            static_cast<unsigned int>(allDecodedValsFastMultiple32.size()),
            allEncodedData.size()
        );
        BOOST_REQUIRE_EQUAL(allDecodedValsFastMultiple32.size(), numValuesActuallyDecoded * 2); //actually decoded half of expected

        BOOST_REQUIRE_EQUAL(numBytesTakenToDecode, totalBytesEncoded);
        allDecodedValsFastMultiple32.resize(numValuesActuallyDecoded); //resize before comparision
        BOOST_REQUIRE(allDecodedValsFastMultiple32 == testVals);
    }

    //DECODE ARRAY OF VALS using SdnvDecodeArrayU64Classic and 1 byte sdnvs
    for (uint8_t val = 0; val < 100; ++val) { //all these vals are equivalent as encoded and decoded since they are <= 127
        std::vector<uint64_t> allDecodedValsFastMultiple32(val + 1);
        allDecodedValsFastMultiple32.reserve(allDecodedValsFastMultiple32.size() * 3); //triple capacity for partial decode test scheme below
        std::vector<uint64_t> allExpectedDecodedValsFastMultiple32(val + 1);
        std::vector<uint8_t> allEncoded1ByteVals(100);
        for (std::size_t i = 0; i < allDecodedValsFastMultiple32.size(); ++i) {
            allEncoded1ByteVals[i] = static_cast<uint8_t>(i);
            allExpectedDecodedValsFastMultiple32[i] = i;
        }

        for (unsigned int scheme = 0; scheme < 3; ++scheme) {
            allDecodedValsFastMultiple32.assign(allDecodedValsFastMultiple32.size(), UINT8_MAX); //make sure the SdnvDecodeArrayU64Fast function actually writes data
            uint64_t encodedBufferSize;
            unsigned int numSdnvsToDecode;
            if (scheme == 0) {
                encodedBufferSize = allDecodedValsFastMultiple32.size(); //min size encoded buffer size
                numSdnvsToDecode = static_cast<unsigned int>(allDecodedValsFastMultiple32.size());
            }
            else if (scheme == 1) {
                encodedBufferSize = 100; //larger encoded buffer size to force avx operations
                numSdnvsToDecode = static_cast<unsigned int>(allDecodedValsFastMultiple32.size());
            }
            else {
                encodedBufferSize = allDecodedValsFastMultiple32.size(); //min size encoded buffer size
                numSdnvsToDecode = static_cast<unsigned int>(allDecodedValsFastMultiple32.size() * 3); //parital decode scheme (try decode more sdnvs than available)      
            }

            uint64_t numBytesTakenToDecode;
            const unsigned int numValuesActuallyDecoded = SdnvDecodeArrayU64Classic(
                allEncoded1ByteVals.data(),
                numBytesTakenToDecode,
                allDecodedValsFastMultiple32.data(),
                numSdnvsToDecode,
                encodedBufferSize
            );
            BOOST_REQUIRE_EQUAL(allDecodedValsFastMultiple32.size(), numValuesActuallyDecoded);
            if (scheme != 2) {
                BOOST_REQUIRE_EQUAL(numValuesActuallyDecoded, numSdnvsToDecode);
            }
            else {
                BOOST_REQUIRE_EQUAL(numValuesActuallyDecoded * 3, numSdnvsToDecode);
            }

            BOOST_REQUIRE_EQUAL(numBytesTakenToDecode, allDecodedValsFastMultiple32.size());
            BOOST_REQUIRE(allDecodedValsFastMultiple32 == allExpectedDecodedValsFastMultiple32);
        }
    }
}


BOOST_AUTO_TEST_CASE(Sdnv64BitSpeedTestCase, *boost::unit_test::disabled())
{
#define SPEED_TEST_LARGE_SDNVS 1
#if SPEED_TEST_LARGE_SDNVS
    static const unsigned int expectedTotalBytesEncoded = 541;
    const std::vector<uint64_t> testVals2(testVals.cbegin(), testVals.cend()); //allow the big sdnvs with 9 and 10 byte encoding lengths
#else
    const std::vector<uint64_t> testVals2(testVals.cbegin(), testVals.cend() - 20);
    static const unsigned int expectedTotalBytesEncoded = 354;
#endif

    std::vector<uint8_t> allEncodedData(testVals2.size() * 10);
    std::size_t totalBytesEncoded = 0;
#ifdef USE_SDNV_FAST
    std::vector<uint8_t> allEncodedDataFast(testVals2.size() * 10 + sizeof(uint64_t));
    std::size_t totalBytesEncodedFast = 0;
#endif
    std::cout << "starting speed test\n";
    std::cout << "testvals2 size: " << testVals2.size() << std::endl;

    static const std::size_t LOOP_COUNT = 5000000;
    //ENCODE ARRAY OF VALS (CLASSIC)
    {
        std::cout << "encode classic\n";
        boost::timer::auto_cpu_timer t;
        for (std::size_t loopI = 0; loopI < LOOP_COUNT; ++loopI) {
            totalBytesEncoded = 0;
            uint8_t * allEncodedDataPtr = allEncodedData.data();
            for (std::size_t i = 0; i < testVals2.size(); ++i) {
                const uint64_t val = testVals2[i];
                //encode
                const unsigned int outputSizeBytes = SdnvEncodeU64ClassicBufSize10(allEncodedDataPtr, val);
                allEncodedDataPtr += outputSizeBytes;
                totalBytesEncoded += outputSizeBytes;
            }
        }
        //std::cout << "totalBytesEncoded " << totalBytesEncoded << std::endl;
        BOOST_REQUIRE_EQUAL(totalBytesEncoded, expectedTotalBytesEncoded);
    }

#ifdef USE_SDNV_FAST
    //ENCODE ARRAY OF VALS (FAST)
    {
        std::cout << "encode fast\n";
        boost::timer::auto_cpu_timer t;
        for (std::size_t loopI = 0; loopI < LOOP_COUNT; ++loopI) {
            totalBytesEncodedFast = 0;
            uint8_t * allEncodedDataFastPtr = allEncodedDataFast.data();
            for (std::size_t i = 0; i < testVals2.size(); ++i) {
                const uint64_t val = testVals2[i];
                //encode fast
                const unsigned int outputSizeBytesFast = SdnvEncodeU64FastBufSize10(allEncodedDataFastPtr, val);
                allEncodedDataFastPtr += outputSizeBytesFast;
                totalBytesEncodedFast += outputSizeBytesFast;
            }
        }
        //std::cout << "totalBytesEncoded " << totalBytesEncoded << std::endl;
        BOOST_REQUIRE_EQUAL(totalBytesEncodedFast, expectedTotalBytesEncoded);
    }
#endif

    //DECODE ARRAY OF VALS (CLASSIC)
    {
        std::cout << "decode classic\n";
        std::vector<uint64_t> allDecodedVals(testVals2.size());
        std::size_t j;
        boost::timer::auto_cpu_timer t;
        for (std::size_t loopI = 0; loopI < LOOP_COUNT; ++loopI) {
            uint8_t * allEncodedDataPtr = allEncodedData.data();
            j = 0;
            uint64_t * allDecodedDataPtr = allDecodedVals.data();
            while (j < totalBytesEncoded) {
                //return decoded value (0 if failure), also set parameter numBytes taken to decode
                uint8_t numBytesTakenToDecode;
                const uint64_t decodedVal = SdnvDecodeU64Classic(allEncodedDataPtr, &numBytesTakenToDecode, 10); //will always be at least 10 byte buffer
                *allDecodedDataPtr++ = decodedVal;
                allEncodedDataPtr += numBytesTakenToDecode;

                j += numBytesTakenToDecode;
            }
        }
        BOOST_REQUIRE_EQUAL(j, totalBytesEncoded);
        BOOST_REQUIRE(allDecodedVals == testVals2);
    }

#ifdef USE_SDNV_FAST
    //DECODE ARRAY OF VALS (FAST)
    {
        std::cout << "decode fast\n";
        std::vector<uint64_t> allDecodedValsFast(testVals2.size());
        std::size_t j;
        boost::timer::auto_cpu_timer t;
        for (std::size_t loopI = 0; loopI < LOOP_COUNT; ++loopI) {
            uint8_t * allEncodedDataFastPtr = allEncodedDataFast.data();
            j = 0;
            uint64_t * allDecodedDataFastPtr = allDecodedValsFast.data();
            while (j < totalBytesEncoded) {

                uint8_t numBytesTakenToDecodeFast;
                const uint64_t decodedValFast = SdnvDecodeU64FastBufSize16(allEncodedDataFastPtr, &numBytesTakenToDecodeFast);
                //std::cout << decodedVal << " " << (int)numBytesTakenToDecode << "\n";
                *allDecodedDataFastPtr++ = decodedValFast;
                allEncodedDataFastPtr += numBytesTakenToDecodeFast;

                j += numBytesTakenToDecodeFast;
            }
        }
        BOOST_REQUIRE_EQUAL(j, totalBytesEncoded);
        BOOST_REQUIRE(allDecodedValsFast == testVals2);
    }

    //DECODE UP TO 16-BYTES AT A TIME ARRAY OF VALS
    {
        std::cout << "decode fast 16 byte\n";
        std::size_t j;
        std::vector<uint64_t> allDecodedValsFastMultiple(testVals2.size());
        boost::timer::auto_cpu_timer t;
        for (std::size_t loopI = 0; loopI < LOOP_COUNT; ++loopI) {
            uint8_t * allEncodedDataPtr = allEncodedData.data();
            uint64_t * allDecodedDataFastMultiplePtr = allDecodedValsFastMultiple.data();
            j = 0;
            unsigned int decodedRemaining = (unsigned int)testVals2.size();
            while (j < totalBytesEncoded) {
                //return decoded value (0 if failure), also set parameter numBytes taken to decode
                uint8_t totalBytesDecoded;
                unsigned int numValsDecodedThisIteration = SdnvDecodeMultipleU64Fast(allEncodedDataPtr, &totalBytesDecoded, allDecodedDataFastMultiplePtr, decodedRemaining);
                decodedRemaining -= numValsDecodedThisIteration;
                allDecodedDataFastMultiplePtr += numValsDecodedThisIteration;
                allEncodedDataPtr += totalBytesDecoded;

                j += totalBytesDecoded;
            }
        }
        BOOST_REQUIRE_EQUAL(j, totalBytesEncoded);
        BOOST_REQUIRE(allDecodedValsFastMultiple == testVals2);
    }

# ifdef SDNV_SUPPORT_AVX2_FUNCTIONS //must also support USE_SDNV_FAST
    //DECODE UP TO 32-BYTES AT A TIME ARRAY OF VALS
    {
        std::cout << "decode fast 32 byte\n";
        std::size_t j;
        std::vector<uint64_t> allDecodedValsFastMultiple32(testVals2.size());
        boost::timer::auto_cpu_timer t;
        for (std::size_t loopI = 0; loopI < LOOP_COUNT; ++loopI) {
            uint8_t * allEncodedDataPtr = allEncodedData.data();
            uint64_t * allDecodedDataFastMultiple32Ptr = allDecodedValsFastMultiple32.data();
            j = 0;
            unsigned int decodedRemaining = (unsigned int)testVals2.size();
            while (j < totalBytesEncoded) {
                //return decoded value (0 if failure), also set parameter numBytes taken to decode
                uint8_t totalBytesDecoded;
                unsigned int numValsDecodedThisIteration = SdnvDecodeMultiple256BitU64Fast(allEncodedDataPtr, &totalBytesDecoded, allDecodedDataFastMultiple32Ptr, decodedRemaining);
                decodedRemaining -= numValsDecodedThisIteration;
                allDecodedDataFastMultiple32Ptr += numValsDecodedThisIteration;
                allEncodedDataPtr += totalBytesDecoded;

                j += totalBytesDecoded;
            }
        }
        BOOST_REQUIRE_EQUAL(j, totalBytesEncoded);
        BOOST_REQUIRE(allDecodedValsFastMultiple32 == testVals2);
    }
# endif //#ifdef SDNV_SUPPORT_AVX2_FUNCTIONS
#endif //#ifdef USE_SDNV_FAST
}

