//Implementation of https://tools.ietf.org/html/rfc6256
// Using Self-Delimiting Numeric Values in Protocols
#include "CborUint.h"
#include <iostream>
#include <boost/multiprecision/cpp_int.hpp>
#include <boost/multiprecision/detail/bitscan.hpp>
#include <boost/endian/conversion.hpp>
#ifdef USE_X86_HARDWARE_ACCELERATION
#include <immintrin.h>
#endif

#define CBOR_UINT8_TYPE   (24)
#define CBOR_UINT16_TYPE  (25)
#define CBOR_UINT32_TYPE  (26)
#define CBOR_UINT64_TYPE  (27)


//return output size
unsigned int CborEncodeU64(uint8_t * outputEncoded, const uint64_t valToEncodeU64, const uint64_t bufferSize) {
#ifdef USE_X86_HARDWARE_ACCELERATION
    if (bufferSize >= 9) {
        return CborEncodeU64FastBufSize9(outputEncoded, valToEncodeU64);
    }
    else {
        return CborEncodeU64Fast(outputEncoded, valToEncodeU64, bufferSize);
    }
#else
    return CborEncodeU64Classic(outputEncoded, valToEncodeU64, bufferSize);
#endif // USE_X86_HARDWARE_ACCELERATION
}

//return output size
unsigned int CborEncodeU64BufSize9(uint8_t * const outputEncoded, const uint64_t valToEncodeU64) {
#ifdef USE_X86_HARDWARE_ACCELERATION
    return CborEncodeU64FastBufSize9(outputEncoded, valToEncodeU64);
#else
    return CborEncodeU64ClassicBufSize9(outputEncoded, valToEncodeU64);
#endif // USE_X86_HARDWARE_ACCELERATION
}

//return output size
unsigned int CborGetEncodingSizeU64(const uint64_t valToEncodeU64) {
#ifdef USE_X86_HARDWARE_ACCELERATION
    return CborGetEncodingSizeU64Fast(valToEncodeU64);
#else
    return CborGetEncodingSizeU64Classic(valToEncodeU64);
#endif // USE_X86_HARDWARE_ACCELERATION
}

//return decoded value (0 if failure), also set parameter numBytes taken to decode
uint64_t CborDecodeU64(const uint8_t * inputEncoded, uint8_t * numBytes, const uint64_t bufferSize) {
#ifdef USE_X86_HARDWARE_ACCELERATION
    if (bufferSize >= 9) {
        return CborDecodeU64FastBufSize9(inputEncoded, numBytes);
    }
    else {
        return CborDecodeU64Fast(inputEncoded, numBytes, bufferSize);
    }
#else
    return CborDecodeU64Classic(inputEncoded, numBytes, bufferSize);
#endif // USE_X86_HARDWARE_ACCELERATION
}

//return decoded value (0 if failure), also set parameter numBytes taken to decode
uint64_t CborDecodeU64BufSize9(const uint8_t * inputEncoded, uint8_t * numBytes) {
#ifdef USE_X86_HARDWARE_ACCELERATION
    return CborDecodeU64FastBufSize9(inputEncoded, numBytes);
#else
    return CborDecodeU64ClassicBufSize9(inputEncoded, numBytes);
#endif // USE_X86_HARDWARE_ACCELERATION
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

//return output size
unsigned int CborGetEncodingSizeU64Classic(const uint64_t valToEncodeU64) {
    if (valToEncodeU64 < CBOR_UINT8_TYPE) {
        return 1;
    }
    else if (valToEncodeU64 <= UINT8_MAX) {
        return 2;
    }
    else if (valToEncodeU64 <= UINT16_MAX) {
        return 3;
    }
    else if (valToEncodeU64 <= UINT32_MAX) {
        return 5;
    }
    else if (valToEncodeU64 <= UINT64_MAX) {
        return 9;
    }
    else {
        return 0;
    }
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

#ifdef USE_X86_HARDWARE_ACCELERATION

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

//return output size
unsigned int CborGetEncodingSizeU64Fast(const uint64_t valToEncodeU64) {
    //critical that the compiler optimizes this instruction using cmovne instead of imul (which is what the casts to uint8_t and bool are for)
    //bitscan seems to have undefined behavior on a value of zero
    //msb will be 0 if value to encode < 24
    const unsigned int msb = (static_cast<bool>(valToEncodeU64 >= CBOR_UINT8_TYPE)) * (static_cast<uint8_t>(boost::multiprecision::detail::find_msb<uint64_t>(valToEncodeU64)));
    const uint32_t requiredEncodingSizePlusTypePlusShifts = msbToRequiredEncodingSizePlusTypePlusShifts[msb];
    const uint8_t encodingSize = (uint8_t)requiredEncodingSizePlusTypePlusShifts;

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
#if 1
    static const uint8_t RIGHT_SHIFTS[CBOR_UINT64_TYPE + 1] = {
        56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, //type = 0..23
        56, 48, 32, 0 //type = 24..27
    };
    static const uint8_t NUM_BYTES[CBOR_UINT64_TYPE + 1] = {
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, //type = 0..23
        2, 3, 5, 9 //type = 24..27
    };

    const uint8_t type = inputEncoded[0];
    if (type > CBOR_UINT64_TYPE) { //branch prediction should never enter here
        *numBytes = 0;
        return 0;
    }
    *numBytes = NUM_BYTES[type];
    const __m128i enc = _mm_castpd_si128(_mm_load_sd((double const*)(&inputEncoded[static_cast<uint8_t>(type >= CBOR_UINT8_TYPE)]))); //Load a double-precision (64-bit) floating-point element from memory into the lower of dst, and zero the upper element. mem_addr does not need to be aligned on any particular boundary.
    const uint64_t result64Be = _mm_cvtsi128_si64(enc); //SSE2 Copy the lower 64-bit integer in a to dst.
    //const uint64_t result64Be = *(reinterpret_cast<const uint64_t*>(&inputEncoded[static_cast<uint8_t>(type >= CBOR_UINT8_TYPE)]));
    return (static_cast<uint64_t>(boost::endian::big_to_native(result64Be))) >> RIGHT_SHIFTS[type];
#else
    static constexpr uint16_t A = (1 << 8) | 56; //type = 0..23
    static constexpr uint16_t B = (2 << 8) | 56; //type = 24
    static constexpr uint16_t C = (3 << 8) | 48; //type = 25
    static constexpr uint16_t D = (5 << 8) | 32; //type = 26
    static constexpr uint16_t E = (9 << 8) | 0; //type = 27
    static const uint16_t NUM_BYTES_PLUS_RIGHT_SHIFTS[CBOR_UINT64_TYPE + 1] = {
        A, A, A, A, A, A, A, A, A, A, A, A, A, A, A, A, A, A, A, A, A, A, A, A, //type = 0..23
        B, C, D, E //type = 24..27
    };

    const uint8_t type = inputEncoded[0];
    if (type > CBOR_UINT64_TYPE) { //branch prediction should never enter here
        *numBytes = 0;
        return 0;
    }
    const uint16_t rightShiftsPlusNumBytes = NUM_BYTES_PLUS_RIGHT_SHIFTS[type];
    *numBytes = rightShiftsPlusNumBytes >> 8;
    const __m128i enc = _mm_castpd_si128(_mm_load_sd((double const*)(&inputEncoded[static_cast<uint8_t>(type >= CBOR_UINT8_TYPE)]))); //Load a double-precision (64-bit) floating-point element from memory into the lower of dst, and zero the upper element. mem_addr does not need to be aligned on any particular boundary.
    const uint64_t result64Be = _mm_cvtsi128_si64(enc); //SSE2 Copy the lower 64-bit integer in a to dst.
    return (static_cast<uint64_t>(boost::endian::big_to_native(result64Be))) >> (static_cast<uint8_t>(rightShiftsPlusNumBytes));
#endif
}


#endif //#ifdef USE_X86_HARDWARE_ACCELERATION

//return output size
unsigned int CborGetNumBytesRequiredToEncode(const uint64_t valToEncodeU64) {

    //critical that the compiler optimizes this instruction using cmovne instead of imul (which is what the casts to uint8_t and bool are for)
    //bitscan seems to have undefined behavior on a value of zero
    //msb will be 0 if value to encode < 24
    const unsigned int msb = (static_cast<bool>(valToEncodeU64 < CBOR_UINT8_TYPE)) * (static_cast<uint8_t>(boost::multiprecision::detail::find_msb<uint64_t>(valToEncodeU64)));

    return msbToRequiredEncodingSize[msb];
}

uint64_t CborTwoUint64ArraySerialize(uint8_t * serialization, const uint64_t element1, const uint64_t element2) {
    uint8_t * const serializationBase = serialization;
    *serialization++ = (4U << 5) | 2; //major type 4, additional information 2
    serialization += CborEncodeU64BufSize9(serialization, element1);
    serialization += CborEncodeU64BufSize9(serialization, element2);
    return serialization - serializationBase;
}
uint64_t CborTwoUint64ArraySerialize(uint8_t * serialization, const uint64_t element1, const uint64_t element2, uint64_t bufferSize) {
    uint8_t * const serializationBase = serialization;
    uint64_t thisSerializationSize;
    if (bufferSize == 0) {
        return 0;
    }
    --bufferSize;
    *serialization++ = (4U << 5) | 2; //major type 4, additional information 2

    thisSerializationSize = CborEncodeU64(serialization, element1, bufferSize); //if zero returned on failure will only malform the bundle but not cause memory overflow
    serialization += thisSerializationSize;
    bufferSize -= thisSerializationSize;

    thisSerializationSize = CborEncodeU64(serialization, element2, bufferSize);
    serialization += thisSerializationSize;
    //bufferSize -= thisSerializationSize; //not needed

    return serialization - serializationBase;
}
uint64_t CborTwoUint64ArraySerializationSize(const uint64_t element1, const uint64_t element2) {
    uint64_t serializationSize = 1; //cbor first byte major type 4, additional information 2
    serializationSize += CborGetEncodingSizeU64(element1);
    serializationSize += CborGetEncodingSizeU64(element2);
    return serializationSize;
}
bool CborTwoUint64ArrayDeserialize(const uint8_t * serialization, uint8_t * numBytesTakenToDecode, uint64_t bufferSize, uint64_t & element1, uint64_t & element2) {
    uint8_t cborUintSize;
    const uint8_t * const serializationBase = serialization;

    if (bufferSize == 0) {
        return false;
    }
    --bufferSize;
    const uint8_t initialCborByte = *serialization++;
    if ((initialCborByte != ((4U << 5) | 2U)) && //major type 4, additional information 2 (array of length 2)
        (initialCborByte != ((4U << 5) | 31U))) { //major type 4, additional information 31 (Indefinite-Length Array)
        return false;
    }

    element1 = CborDecodeU64(serialization, &cborUintSize, bufferSize);
    if (cborUintSize == 0) {
        return false; //failure
    }
    serialization += cborUintSize;
    bufferSize -= cborUintSize;

    element2 = CborDecodeU64(serialization, &cborUintSize, bufferSize);
    if (cborUintSize == 0) {
        return false; //failure
    }
    serialization += cborUintSize;

    //An implementation of the Bundle Protocol MAY accept a sequence of
    //bytes that does not conform to the Bundle Protocol specification
    //(e.g., one that represents data elements in fixed-length arrays
    //rather than indefinite-length arrays) and transform it into
    //conformant BP structure before processing it.
    if (initialCborByte == ((4U << 5) | 31U)) { //major type 4, additional information 31 (Indefinite-Length Array)
        bufferSize -= cborUintSize; //from element 2 above
        if (bufferSize == 0) {
            return false;
        }
        const uint8_t breakStopCode = *serialization++;
        if (breakStopCode != 0xff) {
            return false;
        }
    }

    *numBytesTakenToDecode = static_cast<uint8_t>(serialization - serializationBase);
    return true;
}


uint64_t CborArbitrarySizeUint64ArraySerialize(uint8_t * serialization, const std::vector<uint64_t> & elements) {
    uint8_t * const serializationBase = serialization;

    uint8_t * const arrayHeaderStartPtr = serialization;
    serialization += CborEncodeU64BufSize9(serialization, elements.size());
    *arrayHeaderStartPtr |= (4U << 5); //change from major type 0 (unsigned integer) to major type 4 (array)

    for (std::size_t i = 0; i < elements.size(); ++i) {
        serialization += CborEncodeU64BufSize9(serialization, elements[i]);
    }
    return serialization - serializationBase;
}
uint64_t CborArbitrarySizeUint64ArraySerialize(uint8_t * serialization, const std::vector<uint64_t> & elements, uint64_t bufferSize) {
    uint8_t * const serializationBase = serialization;
    uint64_t thisSerializationSize;
    uint8_t * const arrayHeaderStartPtr = serialization;
    thisSerializationSize = CborEncodeU64(serialization, elements.size(), bufferSize); //if zero returned on failure will only malform the bundle but not cause memory overflow
    serialization += thisSerializationSize;
    bufferSize -= thisSerializationSize;
    *arrayHeaderStartPtr |= (4U << 5); //change from major type 0 (unsigned integer) to major type 4 (array)

    for (std::size_t i = 0; i < elements.size(); ++i) {
        thisSerializationSize = CborEncodeU64(serialization, elements[i], bufferSize); //if zero returned on failure will only malform the bundle but not cause memory overflow
        serialization += thisSerializationSize;
        bufferSize -= thisSerializationSize;
    }
    return serialization - serializationBase;
}
uint64_t CborArbitrarySizeUint64ArraySerializationSize(const std::vector<uint64_t> & elements) {
    uint64_t serializationSize = CborGetEncodingSizeU64(elements.size());
    for (std::size_t i = 0; i < elements.size(); ++i) {
        serializationSize += CborGetEncodingSizeU64(elements[i]);
    }
    return serializationSize;
}
bool CborArbitrarySizeUint64ArrayDeserialize(uint8_t * serialization, uint64_t & numBytesTakenToDecode, uint64_t bufferSize, std::vector<uint64_t> & elements, const uint64_t maxElements) {
    uint8_t cborUintSizeDecoded;
    const uint8_t * const serializationBase = serialization;

    if (bufferSize == 0) {
        return false;
    }
    const uint8_t initialCborByte = *serialization; //buffer size verified above
    if (initialCborByte == ((4U << 5) | 31U)) { //major type 4, additional information 31 (Indefinite-Length Array)
        //An implementation of the Bundle Protocol MAY accept a sequence of
        //bytes that does not conform to the Bundle Protocol specification
        //(e.g., one that represents data elements in fixed-length arrays
        //rather than indefinite-length arrays) and transform it into
        //conformant BP structure before processing it.
        ++serialization;
        --bufferSize;
        elements.resize(0);
        elements.reserve(maxElements);
        for (std::size_t i = 0; i < maxElements; ++i) {
            elements.push_back(CborDecodeU64(serialization, &cborUintSizeDecoded, bufferSize));
            if (cborUintSizeDecoded == 0) {
                return false; //failure
            }
            serialization += cborUintSizeDecoded;
            bufferSize -= cborUintSizeDecoded;
        }
        if (bufferSize == 0) {
            return false;
        }
        const uint8_t breakStopCode = *serialization++;
        if (breakStopCode != 0xff) {
            return false;
        }
    }
    else {
        uint8_t * const arrayHeaderStartPtr = serialization; //buffer size verified above
        const uint8_t cborMajorTypeArray = (*arrayHeaderStartPtr) >> 5;
        if (cborMajorTypeArray != 4) {
            return false; //failure
        }
        *arrayHeaderStartPtr &= 0x1f; //temporarily zero out major type to 0 to make it unsigned integer
        const uint64_t numElements = CborDecodeU64(arrayHeaderStartPtr, &cborUintSizeDecoded, bufferSize);
        *arrayHeaderStartPtr |= (4U << 5); // restore to major type to 4 (change from major type 0 (unsigned integer) to major type 4 (array))
        if (cborUintSizeDecoded == 0) {
            return false; //failure
        }
        if (numElements > maxElements) {
            return false; //failure
        }
        serialization += cborUintSizeDecoded;
        bufferSize -= cborUintSizeDecoded;

        elements.resize(numElements);
        for (std::size_t i = 0; i < numElements; ++i) {
            elements[i] = CborDecodeU64(serialization, &cborUintSizeDecoded, bufferSize);
            if (cborUintSizeDecoded == 0) {
                return false; //failure
            }
            serialization += cborUintSizeDecoded;
            bufferSize -= cborUintSizeDecoded;
        }
    }

    numBytesTakenToDecode = (serialization - serializationBase);
    return true;
}
