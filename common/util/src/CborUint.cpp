//Implementation of https://tools.ietf.org/html/rfc6256
// Using Self-Delimiting Numeric Values in Protocols
#include "CborUint.h"
#include <iostream>
#include <boost/multiprecision/cpp_int.hpp>
#include <boost/multiprecision/detail/bitscan.hpp>
#include <boost/endian/conversion.hpp>
#include <immintrin.h>


#define CBOR_UINT8_TYPE   (24)
#define CBOR_UINT16_TYPE  (25)
#define CBOR_UINT32_TYPE  (26)
#define CBOR_UINT64_TYPE  (27)


//return output size
unsigned int CborEncodeU64(uint8_t * outputEncoded, const uint64_t valToEncodeU64, const uint64_t bufferSize) {
#ifdef SDNV_USE_HARDWARE_ACCELERATION
    return CborEncodeU64Fast(outputEncoded, valToEncodeU64, bufferSize);
#else
    return CborEncodeU64Classic(outputEncoded, valToEncodeU64, bufferSize);
#endif // SDNV_USE_HARDWARE_ACCELERATION
}

//return output size
unsigned int CborEncodeU64BufSize9(uint8_t * const outputEncoded, const uint64_t valToEncodeU64) {
#ifdef SDNV_USE_HARDWARE_ACCELERATION
    return CborEncodeU64FastBufSize9(outputEncoded, valToEncodeU64);
#else
    return CborEncodeU64ClassicBufSize9(outputEncoded, valToEncodeU64);
#endif // SDNV_USE_HARDWARE_ACCELERATION
}

//return decoded value (0 if failure), also set parameter numBytes taken to decode
uint64_t CborDecodeU64(const uint8_t * inputEncoded, uint8_t * numBytes, const uint64_t bufferSize) {
#ifdef SDNV_USE_HARDWARE_ACCELERATION
    return CborDecodeU64Fast(inputEncoded, numBytes, bufferSize);
#else
    return CborDecodeU64Classic(inputEncoded, numBytes, bufferSize);
#endif // SDNV_USE_HARDWARE_ACCELERATION
}

//return decoded value (0 if failure), also set parameter numBytes taken to decode
uint64_t CborDecodeU64BufSize9(const uint8_t * inputEncoded, uint8_t * numBytes) {
#ifdef SDNV_USE_HARDWARE_ACCELERATION
    return CborDecodeU64FastBufSize9(inputEncoded, numBytes);
#else
    return CborDecodeU64ClassicBufSize9(inputEncoded, numBytes);
#endif // SDNV_USE_HARDWARE_ACCELERATION
}



//return output size
unsigned int CborEncodeU64Classic(uint8_t * const outputEncoded, const uint64_t valToEncodeU64, const uint64_t bufferSize) {
    if (valToEncodeU64 < CBOR_UINT8_TYPE) {
        if (bufferSize < 1) {
            return 0;
        }
        outputEncoded[0] = (uint8_t)valToEncodeU64;
        return 1;
    }
    else if (valToEncodeU64 <= UINT8_MAX) {
        if (bufferSize < 2) {
            return 0;
        }
        outputEncoded[0] = CBOR_UINT8_TYPE;
        outputEncoded[1] = (uint8_t)valToEncodeU64;
        return 2;
    }
    else if (valToEncodeU64 <= UINT16_MAX) {
        if (bufferSize < 3) {
            return 0;
        }
        outputEncoded[0] = CBOR_UINT16_TYPE;
        const uint16_t be16 = boost::endian::native_to_big((uint16_t)valToEncodeU64);
        const uint8_t * const be16As8Ptr = (uint8_t*)&be16;
        outputEncoded[1] = be16As8Ptr[0];
        outputEncoded[2] = be16As8Ptr[1];
        return 3;
    }
    else if (valToEncodeU64 <= UINT32_MAX) {
        if (bufferSize < 5) {
            return 0;
        }
        outputEncoded[0] = CBOR_UINT32_TYPE;
        const uint32_t be32 = boost::endian::native_to_big((uint32_t)valToEncodeU64);
        const uint8_t * const be32As8Ptr = (uint8_t*)&be32;
        outputEncoded[1] = be32As8Ptr[0];
        outputEncoded[2] = be32As8Ptr[1];
        outputEncoded[3] = be32As8Ptr[2];
        outputEncoded[4] = be32As8Ptr[3];
        return 5;
    }
    else if (valToEncodeU64 <= UINT64_MAX) {
        if (bufferSize < 9) {
            return 0;
        }
        outputEncoded[0] = CBOR_UINT64_TYPE;
        const uint64_t be64 = boost::endian::native_to_big(valToEncodeU64);
#if 0
        * reinterpret_cast<uint64_t*>(&outputEncoded[1]) = be64;
#else
        const uint8_t * const be64As8Ptr = (uint8_t*)&be64;
        outputEncoded[1] = be64As8Ptr[0];
        outputEncoded[2] = be64As8Ptr[1];
        outputEncoded[3] = be64As8Ptr[2];
        outputEncoded[4] = be64As8Ptr[3];
        outputEncoded[5] = be64As8Ptr[4];
        outputEncoded[6] = be64As8Ptr[5];
        outputEncoded[7] = be64As8Ptr[6];
        outputEncoded[8] = be64As8Ptr[7];
#endif
        return 9;
    }
    return 0;
}

//return output size
unsigned int CborEncodeU64ClassicBufSize9(uint8_t * const outputEncoded, const uint64_t valToEncodeU64) {
    if (valToEncodeU64 < CBOR_UINT8_TYPE) {
        outputEncoded[0] = (uint8_t)valToEncodeU64;
        return 1;
    }
    else if (valToEncodeU64 <= UINT8_MAX) {
        outputEncoded[0] = CBOR_UINT8_TYPE;
        outputEncoded[1] = (uint8_t)valToEncodeU64;
        return 2;
    }
    else if (valToEncodeU64 <= UINT16_MAX) {
        outputEncoded[0] = CBOR_UINT16_TYPE;
        const uint16_t be16 = boost::endian::native_to_big((uint16_t)valToEncodeU64);
        const uint8_t * const be16As8Ptr = (uint8_t*)&be16;
        outputEncoded[1] = be16As8Ptr[0];
        outputEncoded[2] = be16As8Ptr[1];
        return 3;
    }
    else if (valToEncodeU64 <= UINT32_MAX) {
        outputEncoded[0] = CBOR_UINT32_TYPE;
        const uint32_t be32 = boost::endian::native_to_big((uint32_t)valToEncodeU64);
        const uint8_t * const be32As8Ptr = (uint8_t*)&be32;
        outputEncoded[1] = be32As8Ptr[0];
        outputEncoded[2] = be32As8Ptr[1];
        outputEncoded[3] = be32As8Ptr[2];
        outputEncoded[4] = be32As8Ptr[3];
        return 5;
    }
    else if (valToEncodeU64 <= UINT64_MAX) {
        outputEncoded[0] = CBOR_UINT64_TYPE;
        const uint64_t be64 = boost::endian::native_to_big(valToEncodeU64);
#if 0
        * reinterpret_cast<uint64_t*>(&outputEncoded[1]) = be64;
#else
        const uint8_t * const be64As8Ptr = (uint8_t*)&be64;
        outputEncoded[1] = be64As8Ptr[0];
        outputEncoded[2] = be64As8Ptr[1];
        outputEncoded[3] = be64As8Ptr[2];
        outputEncoded[4] = be64As8Ptr[3];
        outputEncoded[5] = be64As8Ptr[4];
        outputEncoded[6] = be64As8Ptr[5];
        outputEncoded[7] = be64As8Ptr[6];
        outputEncoded[8] = be64As8Ptr[7];
#endif
        return 9;
    }
    return 0;
}

//return decoded value (return invalid number that must be ignored on failure)
//  also sets parameter numBytes taken to decode (set to 0 on failure)
uint64_t CborDecodeU64Classic(const uint8_t * const inputEncoded, uint8_t * numBytes, const uint64_t bufferSize) {
    *numBytes = 0; //initialize to invalid
    if (bufferSize == 0) {
        return 0;
    }
    uint64_t result = 0;
    const uint8_t type = inputEncoded[0];
    if (type < CBOR_UINT8_TYPE) {
        result = type;
        *numBytes = 1;
    }
    else if (type == CBOR_UINT8_TYPE) {
        if (bufferSize >= 2) {
            result = inputEncoded[1];
            *numBytes = 2;
        }
    }
    else if (type == CBOR_UINT16_TYPE) {
        if (bufferSize >= 3) {
            uint16_t result16Be;
            uint8_t * const result16BeAs8Ptr = (uint8_t*)&result16Be;
            result16BeAs8Ptr[0] = inputEncoded[1];
            result16BeAs8Ptr[1] = inputEncoded[2];
            result = boost::endian::big_to_native(result16Be);
            *numBytes = 3;
        }
    }
    else if (type == CBOR_UINT32_TYPE) {
        if (bufferSize >= 5) {
            uint32_t result32Be;
            uint8_t * const result32BeAs8Ptr = (uint8_t*)&result32Be;
            result32BeAs8Ptr[0] = inputEncoded[1];
            result32BeAs8Ptr[1] = inputEncoded[2];
            result32BeAs8Ptr[2] = inputEncoded[3];
            result32BeAs8Ptr[3] = inputEncoded[4];
            result = boost::endian::big_to_native(result32Be);
            *numBytes = 5;
        }
    }
    else if (type == CBOR_UINT64_TYPE) {
        if (bufferSize >= 9) {
            uint64_t result64Be;
            uint8_t * const result64BeAs8Ptr = (uint8_t*)&result64Be;
            result64BeAs8Ptr[0] = inputEncoded[1];
            result64BeAs8Ptr[1] = inputEncoded[2];
            result64BeAs8Ptr[2] = inputEncoded[3];
            result64BeAs8Ptr[3] = inputEncoded[4];
            result64BeAs8Ptr[4] = inputEncoded[5];
            result64BeAs8Ptr[5] = inputEncoded[6];
            result64BeAs8Ptr[6] = inputEncoded[7];
            result64BeAs8Ptr[7] = inputEncoded[8];
            result = boost::endian::big_to_native(result64Be);
            *numBytes = 9;
        }
    }

    return result;
}

//return decoded value (return invalid number that must be ignored on failure)
//  also sets parameter numBytes taken to decode (set to 0 on failure)
uint64_t CborDecodeU64ClassicBufSize9(const uint8_t * const inputEncoded, uint8_t * numBytes) {
    *numBytes = 0; //initialize to invalid
    uint64_t result = 0;
    const uint8_t type = inputEncoded[0];
    if (type < CBOR_UINT8_TYPE) {
        result = type;
        *numBytes = 1;
    }
    else if (type == CBOR_UINT8_TYPE) {
        result = inputEncoded[1];
        *numBytes = 2;
    }
    else if (type == CBOR_UINT16_TYPE) {
        uint16_t result16Be;
        uint8_t * const result16BeAs8Ptr = (uint8_t*)&result16Be;
        result16BeAs8Ptr[0] = inputEncoded[1];
        result16BeAs8Ptr[1] = inputEncoded[2];
        result = boost::endian::big_to_native(result16Be);
        *numBytes = 3;
    }
    else if (type == CBOR_UINT32_TYPE) {
        uint32_t result32Be;
        uint8_t * const result32BeAs8Ptr = (uint8_t*)&result32Be;
        result32BeAs8Ptr[0] = inputEncoded[1];
        result32BeAs8Ptr[1] = inputEncoded[2];
        result32BeAs8Ptr[2] = inputEncoded[3];
        result32BeAs8Ptr[3] = inputEncoded[4];
        result = boost::endian::big_to_native(result32Be);
        *numBytes = 5;
    }
    else if (type == CBOR_UINT64_TYPE) {
        uint64_t result64Be;
        uint8_t * const result64BeAs8Ptr = (uint8_t*)&result64Be;
        result64BeAs8Ptr[0] = inputEncoded[1];
        result64BeAs8Ptr[1] = inputEncoded[2];
        result64BeAs8Ptr[2] = inputEncoded[3];
        result64BeAs8Ptr[3] = inputEncoded[4];
        result64BeAs8Ptr[4] = inputEncoded[5];
        result64BeAs8Ptr[5] = inputEncoded[6];
        result64BeAs8Ptr[6] = inputEncoded[7];
        result64BeAs8Ptr[7] = inputEncoded[8];
        result = boost::endian::big_to_native(result64Be);
        *numBytes = 9;
    }

    return result;
}

#ifdef SDNV_USE_HARDWARE_ACCELERATION


static const uint8_t msbToRequiredEncodingSize[64] = {
    1,1,1,1,2,2,2,2,
    3,3,3,3,3,3,3,3,
    5,5,5,5,5,5,5,5,
    5,5,5,5,5,5,5,5,
    9,9,9,9,9,9,9,9,
    9,9,9,9,9,9,9,9,
    9,9,9,9,9,9,9,9,
    9,9,9,9,9,9,9,9
};
static const uint8_t encodingSizeToType[10] = {
    0, //size 0 = invalid
    0, //size 1 = the value itself
    CBOR_UINT8_TYPE, //size 2
    CBOR_UINT16_TYPE, //size 3
    0, //size 4 = invalid
    CBOR_UINT32_TYPE, //size 5
    0, //size 6 = invalid
    0, //size 7 = invalid
    0, //size 8 = invalid
    CBOR_UINT64_TYPE, //size 9
};
static const uint8_t encodingSizeToShifts[10] = {
    0, //size 0 = invalid
    0, //size 1 = the value itself
    56, //size 2
    48, //size 3
    0, //size 4 = invalid
    32, //size 5
    0, //size 6 = invalid
    0, //size 7 = invalid
    0, //size 8 = invalid
    0, //size 9
};
//encoding sizes with extra info to allow one lookup in a table
#define ES2 ((CBOR_UINT8_TYPE << 8) | (56U << 16))
#define ES3 ((CBOR_UINT16_TYPE << 8) | (48U << 16))
#define ES5 ((CBOR_UINT32_TYPE << 8) | (32U << 16))
#define ES9 ((CBOR_UINT64_TYPE << 8) | (0U << 16))
static const uint32_t msbToRequiredEncodingSizePlusTypePlusShifts[64] = {
    1,1,1,1,(2 | ES2),(2 | ES2),(2 | ES2),(2 | ES2),
    (3 | ES3),(3 | ES3),(3 | ES3),(3 | ES3),(3 | ES3),(3 | ES3),(3 | ES3),(3 | ES3),
    (5 | ES5),(5 | ES5),(5 | ES5),(5 | ES5),(5 | ES5),(5 | ES5),(5 | ES5),(5 | ES5),
    (5 | ES5),(5 | ES5),(5 | ES5),(5 | ES5),(5 | ES5),(5 | ES5),(5 | ES5),(5 | ES5),
    (9 | ES9),(9 | ES9),(9 | ES9),(9 | ES9),(9 | ES9),(9 | ES9),(9 | ES9),(9 | ES9),
    (9 | ES9),(9 | ES9),(9 | ES9),(9 | ES9),(9 | ES9),(9 | ES9),(9 | ES9),(9 | ES9),
    (9 | ES9),(9 | ES9),(9 | ES9),(9 | ES9),(9 | ES9),(9 | ES9),(9 | ES9),(9 | ES9),
    (9 | ES9),(9 | ES9),(9 | ES9),(9 | ES9),(9 | ES9),(9 | ES9),(9 | ES9),(9 | ES9)
};

//return output size
unsigned int CborEncodeU64Fast(uint8_t * const outputEncoded, const uint64_t valToEncodeU64, const uint64_t bufferSize) {

    //critical that the compiler optimizes this instruction using cmovne instead of imul (which is what the casts to uint8_t and bool are for)
    //bitscan seems to have undefined behavior on a value of zero
    //msb will be 0 if value to encode < 24
    const unsigned int msb = (static_cast<bool>(valToEncodeU64 >= CBOR_UINT8_TYPE)) * (static_cast<uint8_t>(boost::multiprecision::detail::find_msb<uint64_t>(valToEncodeU64)));

    const unsigned int encodingSize = msbToRequiredEncodingSize[msb];
    if (bufferSize >= 9) {
#if 0
        //const uint32_t typeAsUint32Le = boost::endian::native_to_little((uint32_t)encodingSizeToType[encodingSize]); //byte in first location
        //const uint8_t type = encodingSizeToType[encodingSize]; //byte in first location
        if (encodingSize == 1) {
            _mm_stream_si32((int32_t *)(&outputEncoded[0]), boost::endian::native_to_little((uint32_t)valToEncodeU64)); //outputEncoded[0] = (uint8_t)valToEncodeU64;
        }
        else if (encodingSize == 2) {
            uint32_t valToWrite = (uint8_t)valToEncodeU64;
            valToWrite <<= 8;
            valToWrite |= encodingSizeToType[encodingSize];
            _mm_stream_si32((int32_t *)(&outputEncoded[0]), boost::endian::native_to_little(valToWrite));
            //outputEncoded[0] = encodingSizeToType[encodingSize];
            //outputEncoded[1] = (uint8_t)valToEncodeU64;
        }
        else if (encodingSize == 3) {

            uint32_t valToWrite = boost::endian::native_to_big((uint16_t)valToEncodeU64);
            valToWrite <<= 8;
            valToWrite |= encodingSizeToType[encodingSize];
            _mm_stream_si32((int32_t *)(&outputEncoded[0]), boost::endian::native_to_little(valToWrite));
            //outputEncoded[0] = encodingSizeToType[encodingSize];
            //outputEncoded[1] = (uint8_t)(valToEncodeU64 >> 8); //big endian most significant byte first
            //outputEncoded[2] = (uint8_t)valToEncodeU64; //big endian least significant byte last
        }
        else if (encodingSize == 5) {
            uint64_t valToWrite = boost::endian::native_to_big((uint32_t)valToEncodeU64);
            valToWrite <<= 8;
            valToWrite |= encodingSizeToType[encodingSize];
            _mm_stream_si64((long long int *)(&outputEncoded[0]), boost::endian::native_to_little(valToWrite));
            //outputEncoded[0] = encodingSizeToType[encodingSize];
            //_mm_stream_si32((int32_t *)(&outputEncoded[1]), boost::endian::native_to_big((uint32_t)valToEncodeU64));
        }
        else if (encodingSize == 9) {
            _mm_stream_si32((int32_t *)(&outputEncoded[0]), boost::endian::native_to_little((uint32_t)encodingSizeToType[encodingSize])); //outputEncoded[0] = encodingSizeToType[encodingSize];
            _mm_stream_si64((long long int *)(&outputEncoded[1]), boost::endian::native_to_big(valToEncodeU64));
        }
#else
        /*
        if ((bufferSize >= 9) && (encodingSize > 1)) {
            outputEncoded[0] = encodingSizeToType[encodingSize];
            _mm_stream_si64((long long int *)(&outputEncoded[1]), boost::endian::native_to_big(valToEncodeU64 << encodingSizeToShifts[encodingSize]));
            return encodingSize;
        }*/
        const uint8_t firstByte = encodingSizeToType[encodingSize] | ((static_cast<bool>(encodingSize == 1)) * ((uint8_t)valToEncodeU64)); //byte in first location
        _mm_stream_si32((int32_t *)outputEncoded, boost::endian::native_to_little((uint32_t)firstByte)); //outputEncoded[0] = (uint8_t)valToEncodeU64;
        //_mm_stream_si64((long long int *)outputEncoded, boost::endian::native_to_little((uint64_t)firstByte)); //outputEncoded[0] = (uint8_t)valToEncodeU64;
        _mm_stream_si64((long long int *)(&outputEncoded[1]), boost::endian::native_to_big(valToEncodeU64 << encodingSizeToShifts[encodingSize]));
        /*return encodingSize;
        if (encodingSize == 1) {
            _mm_stream_si32((int32_t *)(&outputEncoded[0]), boost::endian::native_to_little((uint32_t)valToEncodeU64)); //outputEncoded[0] = (uint8_t)valToEncodeU64;
        }
        if ((encodingSize > 1)) {
            outputEncoded[0] = encodingSizeToType[encodingSize];
            const uint64_t src = boost::endian::native_to_big(valToEncodeU64 << encodingSizeToShifts[encodingSize]);
            memcpy(&outputEncoded[1], &src, encodingSize - 1);
            return encodingSize;
        }*/
#endif
    }
    else if (bufferSize < encodingSize) {
        return 0;
    }
    else {
#if 0

#endif
        outputEncoded[0] = encodingSizeToType[encodingSize];
        if (encodingSize == 1) {
            outputEncoded[0] = (uint8_t)valToEncodeU64;
        }
        else if (encodingSize == 2) {
            outputEncoded[1] = (uint8_t)valToEncodeU64;
        }
        else if (encodingSize == 3) {
            outputEncoded[1] = (uint8_t)(valToEncodeU64 >> 8); //big endian most significant byte first
            outputEncoded[2] = (uint8_t)valToEncodeU64; //big endian least significant byte last
        }
        else if (encodingSize == 5) {
            _mm_stream_si32((int32_t *)(&outputEncoded[1]), boost::endian::native_to_big((uint32_t)valToEncodeU64));
        }
        else if (encodingSize == 9) {
            _mm_stream_si64((long long int *)(&outputEncoded[1]), boost::endian::native_to_big(valToEncodeU64));
        }
    }
    return encodingSize;
}

//return output size
unsigned int CborEncodeU64FastBufSize9(uint8_t * const outputEncoded, const uint64_t valToEncodeU64) {
    //critical that the compiler optimizes this instruction using cmovne instead of imul (which is what the casts to uint8_t and bool are for)
    //bitscan seems to have undefined behavior on a value of zero
    //msb will be 0 if value to encode < 24
    const unsigned int msb = (static_cast<bool>(valToEncodeU64 >= CBOR_UINT8_TYPE)) * (static_cast<uint8_t>(boost::multiprecision::detail::find_msb<uint64_t>(valToEncodeU64)));
    const uint32_t requiredEncodingSizePlusTypePlusShifts = msbToRequiredEncodingSizePlusTypePlusShifts[msb];
    const uint8_t encodingSize = (uint8_t)requiredEncodingSizePlusTypePlusShifts;
    const uint8_t type = (uint8_t)(requiredEncodingSizePlusTypePlusShifts >> 8);
    const uint8_t shift = (uint8_t)(requiredEncodingSizePlusTypePlusShifts >> 16);
    const uint8_t firstByte = type | ((static_cast<bool>(encodingSize == 1)) * ((uint8_t)valToEncodeU64)); //byte in first location
#if 0
    outputEncoded[0] = firstByte;
    *(reinterpret_cast<uint64_t*>(&outputEncoded[1])) = boost::endian::native_to_big(valToEncodeU64 << shift);
#else
#if 1
    _mm_stream_si32((int32_t *)outputEncoded, boost::endian::native_to_little((uint32_t)firstByte)); //outputEncoded[0] = (uint8_t)valToEncodeU64;
#else
    _mm_stream_si64((long long int *)outputEncoded, boost::endian::native_to_little((uint64_t)firstByte)); //outputEncoded[0] = (uint8_t)valToEncodeU64;
#endif
    _mm_stream_si64((long long int *)(&outputEncoded[1]), boost::endian::native_to_big(valToEncodeU64 << shift));
#endif
    return encodingSize;
}

//return decoded value (return invalid number that must be ignored on failure)
//  also sets parameter numBytes taken to decode (set to 0 on failure)
uint64_t CborDecodeU64Fast(const uint8_t * const inputEncoded, uint8_t * numBytes, const uint64_t bufferSize) {
    *numBytes = 0; //initialize to invalid
    if (bufferSize == 0) {
        return 0;
    }
    uint64_t result = 0;
    const uint8_t type = inputEncoded[0];
    if (type < CBOR_UINT8_TYPE) {
        result = type;
        *numBytes = 1;
    }
    else if (type == CBOR_UINT8_TYPE) {
        if (bufferSize >= 2) {
            result = inputEncoded[1];
            *numBytes = 2;
        }
    }
    else if (type == CBOR_UINT16_TYPE) {
        if (bufferSize >= 3) {
            result = ((static_cast<uint16_t>(inputEncoded[1])) << 8) | inputEncoded[2];
            *numBytes = 3;
        }
    }
    else if (type == CBOR_UINT32_TYPE) {
        if (bufferSize >= 5) {
            //const __m128i enc = _mm_loadu_si32((__m128i const*)(&inputEncoded[1])); //SSE2 Load unaligned 32-bit integer from memory into the first element of dst.
#if 1
            const __m128i enc = _mm_castps_si128(_mm_load_ss((float const*)(&inputEncoded[1]))); //Load a single-precision (32-bit) floating-point element from memory into the lower of dst, and zero the upper 3 elements. mem_addr does not need to be aligned on any particular boundary.
            const uint32_t result32Be = _mm_cvtsi128_si32(enc); //SSE2 Copy the lower 32-bit integer in a to dst.
            result = boost::endian::big_to_native(result32Be);
#else
            result = static_cast<uint32_t>(_loadbe_i32(&inputEncoded[1]));
#endif

            *numBytes = 5;
        }
    }
    else if (type == CBOR_UINT64_TYPE) {
        if (bufferSize >= 9) {
            //const __m128i enc = _mm_loadl_epi64((__m128i const*)(&inputEncoded[1])); //_mm_loadu_si64(data); //SSE Load unaligned 64-bit integer from memory into the first element of dst.
#if 1
            const __m128i enc = _mm_castpd_si128(_mm_load_sd((double const*)(&inputEncoded[1]))); //Load a double-precision (64-bit) floating-point element from memory into the lower of dst, and zero the upper element. mem_addr does not need to be aligned on any particular boundary.
            const uint64_t result64Be = _mm_cvtsi128_si64(enc); //SSE2 Copy the lower 64-bit integer in a to dst.
            result = boost::endian::big_to_native(result64Be);
#else
            result = static_cast<uint64_t>(_loadbe_i64(&inputEncoded[1]));
#endif
            *numBytes = 9;
        }
    }

    return result;
}

//return decoded value (return invalid number that must be ignored on failure)
//  also sets parameter numBytes taken to decode (set to 0 on failure)
uint64_t CborDecodeU64FastBufSize9(const uint8_t * const inputEncoded, uint8_t * numBytes) {
    *numBytes = 0; //initialize to invalid
    uint64_t result = 0;
    const uint8_t type = inputEncoded[0];
    
    if (type < CBOR_UINT8_TYPE) {
        result = type;
        *numBytes = 1;
    }
    else if (type == CBOR_UINT8_TYPE) {
        result = inputEncoded[1];
        *numBytes = 2;
    }
    else if (type == CBOR_UINT16_TYPE) {
#if 1
        result = ((static_cast<uint16_t>(inputEncoded[1])) << 8) | inputEncoded[2];
#else
        result = static_cast<uint16_t>(_loadbe_i16(&inputEncoded[1]));
#endif
        *numBytes = 3;
    }
    else if (type == CBOR_UINT32_TYPE) {//_mm_load_ss
        //const __m128i enc = _mm_loadu_si32((__m128i const*)(&inputEncoded[1])); //SSE2 Load unaligned 32-bit integer from memory into the first element of dst.
#if 1
        const __m128i enc = _mm_castps_si128(_mm_load_ss((float const*)(&inputEncoded[1]))); //Load a single-precision (32-bit) floating-point element from memory into the lower of dst, and zero the upper 3 elements. mem_addr does not need to be aligned on any particular boundary.
        const uint32_t result32Be = _mm_cvtsi128_si32(enc); //SSE2 Copy the lower 32-bit integer in a to dst.
        result = boost::endian::big_to_native(result32Be);
#else
        result = static_cast<uint32_t>(_loadbe_i32(&inputEncoded[1]));
#endif
        *numBytes = 5;
    }
    else if (type == CBOR_UINT64_TYPE) {
        //const __m128i enc = _mm_loadl_epi64((__m128i const*)(&inputEncoded[1])); //_mm_loadu_si64(data); //SSE Load unaligned 64-bit integer from memory into the first element of dst.
#if 1
        const __m128i enc = _mm_castpd_si128(_mm_load_sd((double const*)(&inputEncoded[1]))); //Load a double-precision (64-bit) floating-point element from memory into the lower of dst, and zero the upper element. mem_addr does not need to be aligned on any particular boundary.
        const uint64_t result64Be = _mm_cvtsi128_si64(enc); //SSE2 Copy the lower 64-bit integer in a to dst.
        result = boost::endian::big_to_native(result64Be);
#else
        result = static_cast<uint64_t>(_loadbe_i64(&inputEncoded[1]));
#endif
        *numBytes = 9;
    }

    return result;
}


#endif //#ifdef SDNV_USE_HARDWARE_ACCELERATION

//return output size
unsigned int CborGetNumBytesRequiredToEncode(const uint64_t valToEncodeU64) {

    //critical that the compiler optimizes this instruction using cmovne instead of imul (which is what the casts to uint8_t and bool are for)
    //bitscan seems to have undefined behavior on a value of zero
    //msb will be 0 if value to encode < 24
    const unsigned int msb = (static_cast<bool>(valToEncodeU64 < CBOR_UINT8_TYPE)) * (static_cast<uint8_t>(boost::multiprecision::detail::find_msb<uint64_t>(valToEncodeU64)));

    return msbToRequiredEncodingSize[msb];
}
