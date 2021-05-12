#include <boost/test/unit_test.hpp>
#include <iostream>
#include <string>
#include <inttypes.h>
#include <vector>
#include "Sdnv.h"
#include <boost/timer/timer.hpp>

BOOST_AUTO_TEST_CASE(Sdnv32BitTestCase)
{
    std::vector<uint8_t> encoded(10);
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
        const unsigned int outputSizeBytes = SdnvEncodeU32Classic(encoded.data(), val);
        const unsigned int outputSizeBytesFast = SdnvEncodeU64Fast(encodedFast.data(), val);
        BOOST_REQUIRE_EQUAL(outputSizeBytes, outputSizeBytesFast);
        BOOST_REQUIRE(encoded == encodedFast);

        //decode val
        uint8_t numBytesDecoded = 0;
        uint8_t numBytesDecodedFast = 0;
        const uint32_t valDecoded = SdnvDecodeU32Classic(encoded.data(), &numBytesDecoded);
        const uint64_t valDecodedFast = SdnvDecodeU64Fast(encoded.data(), &numBytesDecodedFast);
        BOOST_REQUIRE_EQUAL(outputSizeBytes, numBytesDecoded);
        BOOST_REQUIRE_EQUAL(outputSizeBytes, numBytesDecodedFast);
        BOOST_REQUIRE_EQUAL(val, valDecoded);
        BOOST_REQUIRE_EQUAL(val, valDecodedFast);
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
    std::vector<uint8_t> allEncodedDataFast(testVals.size() * 5 + sizeof(uint64_t));
    std::size_t totalBytesEncoded = 0;
    std::size_t totalBytesEncodedFast = 0;
    uint8_t * allEncodedDataPtr = allEncodedData.data();
    uint8_t * allEncodedDataFastPtr = allEncodedDataFast.data();
    for (std::size_t i = 0; i < testVals.size(); ++i) {
        const uint32_t val = testVals[i];
        //encode
        const unsigned int outputSizeBytes = SdnvEncodeU32Classic(allEncodedDataPtr, val);
        allEncodedDataPtr += outputSizeBytes;
        totalBytesEncoded += outputSizeBytes;
        //encode fast
        const unsigned int outputSizeBytesFast = SdnvEncodeU64Fast(allEncodedDataFastPtr, val);
        allEncodedDataFastPtr += outputSizeBytesFast;
        totalBytesEncodedFast += outputSizeBytesFast;
    }
    BOOST_REQUIRE_EQUAL(totalBytesEncoded, 136);
    BOOST_REQUIRE_EQUAL(totalBytesEncoded, totalBytesEncodedFast);
    allEncodedData.resize(totalBytesEncoded);
    allEncodedDataFast.resize(totalBytesEncodedFast + sizeof(uint64_t));



    //DECODE ARRAY OF VALS
    allEncodedDataPtr = allEncodedData.data();
    allEncodedDataFastPtr = allEncodedDataFast.data();
    std::vector<uint32_t> allDecodedVals;
    std::vector<uint32_t> allDecodedValsFast;
    std::size_t j = 0;
    while (j < totalBytesEncoded) {
        //return decoded value (0 if failure), also set parameter numBytes taken to decode
        uint8_t numBytesTakenToDecode;
        const uint32_t decodedVal = SdnvDecodeU32Classic(allEncodedDataPtr, &numBytesTakenToDecode);
        //std::cout << decodedVal << " " << (int)numBytesTakenToDecode << "\n";
        BOOST_REQUIRE_NE(numBytesTakenToDecode, 0);
        allDecodedVals.push_back(decodedVal);
        allEncodedDataPtr += numBytesTakenToDecode;

        uint8_t numBytesTakenToDecodeFast;
        const uint64_t decodedValFast = SdnvDecodeU64Fast(allEncodedDataFastPtr, &numBytesTakenToDecodeFast);
        //std::cout << decodedVal << " " << (int)numBytesTakenToDecode << "\n";
        BOOST_REQUIRE_NE(numBytesTakenToDecodeFast, 0);
        allDecodedValsFast.push_back(static_cast<uint32_t>(decodedValFast));
        allEncodedDataFastPtr += numBytesTakenToDecodeFast;

        BOOST_REQUIRE_EQUAL(numBytesTakenToDecodeFast, numBytesTakenToDecode);

        j += numBytesTakenToDecode;
    }
    BOOST_REQUIRE_EQUAL(j, totalBytesEncoded);
    BOOST_REQUIRE(allDecodedVals == testVals);
    BOOST_REQUIRE(allDecodedValsFast == testVals);
    allEncodedDataFast.resize(totalBytesEncodedFast);
    BOOST_REQUIRE(allEncodedData == allEncodedDataFast);
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

BOOST_AUTO_TEST_CASE(Sdnv64BitTestCase)
{
    std::vector<uint8_t> encoded(10);
    std::vector<uint8_t> encodedFast(10);
    encoded.assign(encoded.size(), 0);
    encodedFast.assign(encodedFast.size(), 0);
    uint16_t coverageMask = 0;
    


    for (std::size_t i = 0; i < testVals.size(); ++i) {
        const uint64_t val = testVals[i];
        //std::cout << "val=" << val << std::endl;
        encoded.assign(encoded.size(), 0);
        encodedFast.assign(encodedFast.size(), 0);
        //encode val
        const unsigned int outputSizeBytes = SdnvEncodeU64Classic(encoded.data(), val);
        const unsigned int outputSizeBytesFast = SdnvEncodeU64Fast(encodedFast.data(), val);
        BOOST_REQUIRE_EQUAL(outputSizeBytes, outputSizeBytesFast);
        BOOST_REQUIRE(encoded == encodedFast);

        //decode val
        uint8_t numBytesDecoded = 0;
        uint8_t numBytesDecodedFast = 0;
        const uint64_t valDecoded = SdnvDecodeU64Classic(encoded.data(), &numBytesDecoded);
        const uint64_t valDecodedFast = SdnvDecodeU64Fast(encoded.data(), &numBytesDecodedFast);
        BOOST_REQUIRE_EQUAL(outputSizeBytes, numBytesDecoded);
        BOOST_REQUIRE_EQUAL(outputSizeBytes, numBytesDecodedFast);
        BOOST_REQUIRE_EQUAL(val, valDecoded);
        BOOST_REQUIRE_EQUAL(val, valDecodedFast);
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
    std::vector<uint8_t> allEncodedDataFast(testVals.size() * 10 + sizeof(uint64_t));
    std::size_t totalBytesEncoded = 0;
    std::size_t totalBytesEncodedFast = 0;
    uint8_t * allEncodedDataPtr = allEncodedData.data();
    uint8_t * allEncodedDataFastPtr = allEncodedDataFast.data();
    for (std::size_t i = 0; i < testVals.size(); ++i) {
        const uint64_t val = testVals[i];
        //encode
        const unsigned int outputSizeBytes = SdnvEncodeU64Classic(allEncodedDataPtr, val);
        allEncodedDataPtr += outputSizeBytes;
        totalBytesEncoded += outputSizeBytes;
        //encode fast
        const unsigned int outputSizeBytesFast = SdnvEncodeU64Fast(allEncodedDataFastPtr, val);
        allEncodedDataFastPtr += outputSizeBytesFast;
        totalBytesEncodedFast += outputSizeBytesFast;
    }
    //std::cout << "totalBytesEncoded " << totalBytesEncoded << std::endl;
    BOOST_REQUIRE_EQUAL(totalBytesEncoded, 541);
    BOOST_REQUIRE_EQUAL(totalBytesEncoded, totalBytesEncodedFast);
    allEncodedData.resize(totalBytesEncoded);
    allEncodedDataFast.resize(totalBytesEncodedFast + sizeof(uint64_t));



    //DECODE ARRAY OF VALS
    allEncodedDataPtr = allEncodedData.data();
    allEncodedDataFastPtr = allEncodedDataFast.data();
    std::vector<uint64_t> allDecodedVals;
    std::vector<uint64_t> allDecodedValsFast;
    std::size_t j = 0;
    while (j < totalBytesEncoded) {
        //return decoded value (0 if failure), also set parameter numBytes taken to decode
        uint8_t numBytesTakenToDecode;
        const uint64_t decodedVal = SdnvDecodeU64Classic(allEncodedDataPtr, &numBytesTakenToDecode);
        //std::cout << decodedVal << " " << (int)numBytesTakenToDecode << "\n";
        BOOST_REQUIRE_NE(numBytesTakenToDecode, 0);
        allDecodedVals.push_back(decodedVal);
        allEncodedDataPtr += numBytesTakenToDecode;

        uint8_t numBytesTakenToDecodeFast;
        const uint64_t decodedValFast = SdnvDecodeU64Fast(allEncodedDataFastPtr, &numBytesTakenToDecodeFast);
        //std::cout << decodedVal << " " << (int)numBytesTakenToDecode << "\n";
        BOOST_REQUIRE_NE(numBytesTakenToDecodeFast, 0);
        allDecodedValsFast.push_back(decodedValFast);
        allEncodedDataFastPtr += numBytesTakenToDecodeFast;

        BOOST_REQUIRE_EQUAL(numBytesTakenToDecodeFast, numBytesTakenToDecode);

        j += numBytesTakenToDecode;
    }
    BOOST_REQUIRE_EQUAL(j, totalBytesEncoded);
    BOOST_REQUIRE(allDecodedVals == testVals);
    BOOST_REQUIRE(allDecodedValsFast == testVals);
    allEncodedDataFast.resize(totalBytesEncodedFast);
    BOOST_REQUIRE(allEncodedData == allEncodedDataFast);


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
    std::vector<uint8_t> allEncodedDataFast(testVals2.size() * 10 + sizeof(uint64_t));
    std::size_t totalBytesEncoded = 0;
    std::size_t totalBytesEncodedFast = 0;
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
                const unsigned int outputSizeBytes = SdnvEncodeU64Classic(allEncodedDataPtr, val);
                allEncodedDataPtr += outputSizeBytes;
                totalBytesEncoded += outputSizeBytes;
            }
        }
        //std::cout << "totalBytesEncoded " << totalBytesEncoded << std::endl;
        BOOST_REQUIRE_EQUAL(totalBytesEncoded, expectedTotalBytesEncoded);
    }

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
                const unsigned int outputSizeBytesFast = SdnvEncodeU64Fast(allEncodedDataFastPtr, val);
                allEncodedDataFastPtr += outputSizeBytesFast;
                totalBytesEncodedFast += outputSizeBytesFast;
            }
        }
        //std::cout << "totalBytesEncoded " << totalBytesEncoded << std::endl;
        BOOST_REQUIRE_EQUAL(totalBytesEncodedFast, expectedTotalBytesEncoded);
    }


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
                const uint64_t decodedVal = SdnvDecodeU64Classic(allEncodedDataPtr, &numBytesTakenToDecode);
                *allDecodedDataPtr++ = decodedVal;
                allEncodedDataPtr += numBytesTakenToDecode;

                j += numBytesTakenToDecode;
            }
        }
        BOOST_REQUIRE_EQUAL(j, totalBytesEncoded);
        BOOST_REQUIRE(allDecodedVals == testVals2);
    }

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
                const uint64_t decodedValFast = SdnvDecodeU64Fast(allEncodedDataFastPtr, &numBytesTakenToDecodeFast);
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
}

