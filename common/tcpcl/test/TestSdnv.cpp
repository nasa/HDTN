#include <boost/test/unit_test.hpp>
#include "sdnv.h"
#include <iostream>
#include <string>
#include <inttypes.h>
#include <vector>

BOOST_AUTO_TEST_CASE(SdnvTestCase)
{
	uint8_t encoded[5];
	uint8_t coverageMask = 0;
	const std::vector<uint32_t> testVals = {
		0,
		1,
		2,

		127 - 2,
		127 - 1,
		127,
		127 + 1,
		127 + 2,

		16383 - 2,
		16383 - 1,
		16383,
		16383 + 1,
		16383 + 2,

		2097151 - 2,
		2097151 - 1,
		2097151,
		2097151 + 1,
		2097151 + 2,

		268435455 - 2,
		268435455 - 1,
		268435455,
		268435455 + 1,
		268435455 + 2,

		UINT32_MAX - 2,
		UINT32_MAX - 1,
		UINT32_MAX
	};

	
	for(std::size_t i = 0; i < testVals.size(); ++i) {
		const uint32_t val = testVals[i];
		//return output size
		const unsigned int outputSizeBytes = SdnvEncodeU32(encoded, val);
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
	for (std::size_t i = 0; i < testVals.size(); ++i) {
		const uint32_t val = testVals[i];
		//return output size
		const unsigned int outputSizeBytes = SdnvEncodeU32(allEncodedDataPtr, val);
		allEncodedDataPtr += outputSizeBytes;
		totalBytesEncoded += outputSizeBytes;
	}
	BOOST_REQUIRE_EQUAL(totalBytesEncoded, 76);
	allEncodedData.resize(totalBytesEncoded);

	

	//DECODE ARRAY OF VALS
	allEncodedDataPtr = allEncodedData.data();
	std::vector<uint32_t> allDecodedVals;
	std::size_t j = 0;
	while (j < totalBytesEncoded) {
		//return decoded value (0 if failure), also set parameter numBytes taken to decode
		uint8_t numBytesTakenToDecode;
		const uint32_t decodedVal = SdnvDecodeU32(allEncodedDataPtr, &numBytesTakenToDecode);
		//std::cout << decodedVal << " " << (int)numBytesTakenToDecode << "\n";
		BOOST_REQUIRE_NE(numBytesTakenToDecode, 0);
		allDecodedVals.push_back(decodedVal);
		j += numBytesTakenToDecode;
		allEncodedDataPtr += numBytesTakenToDecode;
	}
	BOOST_REQUIRE_EQUAL(j, totalBytesEncoded);
	BOOST_REQUIRE(allDecodedVals == testVals);
}

