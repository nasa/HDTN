//Implementation of https://tools.ietf.org/html/rfc6256
// Using Self-Delimiting Numeric Values in Protocols
#include "Sdnv.h"
#include <iostream>
#include <boost/multiprecision/cpp_int.hpp>
#include <boost/multiprecision/detail/bitscan.hpp>
#include <boost/endian/conversion.hpp>
#include <immintrin.h>

#define ONE_U32 (static_cast<uint32_t>(1U))
#define ONE_U64 (static_cast<uint64_t>(1U))

#define SDNV32_MAX_1_BYTE  ((ONE_U32 <<  7U) - 1U)
#define SDNV32_MAX_2_BYTE  ((ONE_U32 << 14U) - 1U)
#define SDNV32_MAX_3_BYTE  ((ONE_U32 << 21U) - 1U)
#define SDNV32_MAX_4_BYTE  ((ONE_U32 << 28U) - 1U)

#define SDNV64_MAX_1_BYTE  ((ONE_U64 <<  7U) - 1U)
#define SDNV64_MAX_2_BYTE  ((ONE_U64 << 14U) - 1U)
#define SDNV64_MAX_3_BYTE  ((ONE_U64 << 21U) - 1U)
#define SDNV64_MAX_4_BYTE  ((ONE_U64 << 28U) - 1U)
#define SDNV64_MAX_5_BYTE  ((ONE_U64 << 35U) - 1U)
#define SDNV64_MAX_6_BYTE  ((ONE_U64 << 42U) - 1U)
#define SDNV64_MAX_7_BYTE  ((ONE_U64 << 49U) - 1U)
#define SDNV64_MAX_8_BYTE  ((ONE_U64 << 56U) - 1U)
#define SDNV64_MAX_9_BYTE  ((ONE_U64 << 63U) - 1U)



//return output size
unsigned int SdnvEncodeU32(uint8_t * outputEncoded, const uint32_t valToEncodeU32) {
#ifdef SDNV_USE_HARDWARE_ACCELERATION
    return SdnvEncodeU32Fast(outputEncoded, valToEncodeU32);
#else
    return SdnvEncodeU32Classic(outputEncoded, valToEncodeU32);
#endif // SDNV_USE_HARDWARE_ACCELERATION

}

//return output size
unsigned int SdnvEncodeU64(uint8_t * outputEncoded, const uint64_t valToEncodeU64) {
#ifdef SDNV_USE_HARDWARE_ACCELERATION
    return SdnvEncodeU64Fast(outputEncoded, valToEncodeU64);
#else
    return SdnvEncodeU64Classic(outputEncoded, valToEncodeU64);
#endif // SDNV_USE_HARDWARE_ACCELERATION
}

//return decoded value (0 if failure), also set parameter numBytes taken to decode
uint32_t SdnvDecodeU32(const uint8_t * inputEncoded, uint8_t * numBytes) {
#ifdef SDNV_USE_HARDWARE_ACCELERATION
    return SdnvDecodeU32Fast(inputEncoded, numBytes);
#else
    return SdnvDecodeU32Classic(inputEncoded, numBytes);
#endif // SDNV_USE_HARDWARE_ACCELERATION
}

//return decoded value (0 if failure), also set parameter numBytes taken to decode
uint64_t SdnvDecodeU64(const uint8_t * inputEncoded, uint8_t * numBytes) {
#ifdef SDNV_USE_HARDWARE_ACCELERATION
    return SdnvDecodeU64Fast(inputEncoded, numBytes);
#else
    return SdnvDecodeU64Classic(inputEncoded, numBytes);
#endif // SDNV_USE_HARDWARE_ACCELERATION
}


//return output size
unsigned int SdnvEncodeU32Classic(uint8_t * outputEncoded, const uint32_t valToEncodeU32) {
    if (valToEncodeU32 <= SDNV32_MAX_1_BYTE) {
        outputEncoded[0] = (uint8_t)(valToEncodeU32 & 0x7f);
        return 1;
    }
    else if (valToEncodeU32 <= SDNV32_MAX_2_BYTE) {
        outputEncoded[1] = (uint8_t)(valToEncodeU32 & 0x7f);
        outputEncoded[0] = (uint8_t)(((valToEncodeU32 >> 7) & 0x7f) | 0x80);
        return 2;
    }
    else if (valToEncodeU32 <= SDNV32_MAX_3_BYTE) {
        outputEncoded[2] = (uint8_t)(valToEncodeU32 & 0x7f);
        outputEncoded[1] = (uint8_t)(((valToEncodeU32 >> 7) & 0x7f) | 0x80);
        outputEncoded[0] = (uint8_t)(((valToEncodeU32 >> 14) & 0x7f) | 0x80);
        return 3;
    }
    else if (valToEncodeU32 <= SDNV32_MAX_4_BYTE) {
        outputEncoded[3] = (uint8_t)(valToEncodeU32 & 0x7f);
        outputEncoded[2] = (uint8_t)(((valToEncodeU32 >> 7) & 0x7f) | 0x80);
        outputEncoded[1] = (uint8_t)(((valToEncodeU32 >> 14) & 0x7f) | 0x80);
        outputEncoded[0] = (uint8_t)(((valToEncodeU32 >> 21) & 0x7f) | 0x80);
        return 4;
    }
    else { //if(valToEncodeU32 <= SDNV_MAX_5_BYTE)
        outputEncoded[4] = (uint8_t)(valToEncodeU32 & 0x7f);
        outputEncoded[3] = (uint8_t)(((valToEncodeU32 >> 7) & 0x7f) | 0x80);
        outputEncoded[2] = (uint8_t)(((valToEncodeU32 >> 14) & 0x7f) | 0x80);
        outputEncoded[1] = (uint8_t)(((valToEncodeU32 >> 21) & 0x7f) | 0x80);
        outputEncoded[0] = (uint8_t)(((valToEncodeU32 >> 28) & 0x7f) | 0x80);
        return 5;
    }
}

//return output size
unsigned int SdnvEncodeU64Classic(uint8_t * outputEncoded, const uint64_t valToEncodeU64) {
    if (valToEncodeU64 <= SDNV64_MAX_1_BYTE) {
        outputEncoded[0] = (uint8_t)(valToEncodeU64 & 0x7f);
        return 1;
    }
    else if (valToEncodeU64 <= SDNV64_MAX_2_BYTE) {
        outputEncoded[1] = (uint8_t)(valToEncodeU64 & 0x7f);
        outputEncoded[0] = (uint8_t)(((valToEncodeU64 >> 7) & 0x7f) | 0x80);
        return 2;
    }
    else if (valToEncodeU64 <= SDNV64_MAX_3_BYTE) {
        outputEncoded[2] = (uint8_t)(valToEncodeU64 & 0x7f);
        outputEncoded[1] = (uint8_t)(((valToEncodeU64 >> 7) & 0x7f) | 0x80);
        outputEncoded[0] = (uint8_t)(((valToEncodeU64 >> 14) & 0x7f) | 0x80);
        return 3;
    }
    else if (valToEncodeU64 <= SDNV64_MAX_4_BYTE) {
        outputEncoded[3] = (uint8_t)(valToEncodeU64 & 0x7f);
        outputEncoded[2] = (uint8_t)(((valToEncodeU64 >> 7) & 0x7f) | 0x80);
        outputEncoded[1] = (uint8_t)(((valToEncodeU64 >> 14) & 0x7f) | 0x80);
        outputEncoded[0] = (uint8_t)(((valToEncodeU64 >> 21) & 0x7f) | 0x80);
        return 4;
    }
    else if (valToEncodeU64 <= SDNV64_MAX_5_BYTE) {
        outputEncoded[4] = (uint8_t)(valToEncodeU64 & 0x7f);
        outputEncoded[3] = (uint8_t)(((valToEncodeU64 >> 7) & 0x7f) | 0x80);
        outputEncoded[2] = (uint8_t)(((valToEncodeU64 >> 14) & 0x7f) | 0x80);
        outputEncoded[1] = (uint8_t)(((valToEncodeU64 >> 21) & 0x7f) | 0x80);
        outputEncoded[0] = (uint8_t)(((valToEncodeU64 >> 28) & 0x7f) | 0x80);
        return 5;
    }
    else if (valToEncodeU64 <= SDNV64_MAX_6_BYTE) {
        outputEncoded[5] = (uint8_t)(valToEncodeU64 & 0x7f);
        outputEncoded[4] = (uint8_t)(((valToEncodeU64 >> 7) & 0x7f) | 0x80);
        outputEncoded[3] = (uint8_t)(((valToEncodeU64 >> 14) & 0x7f) | 0x80);
        outputEncoded[2] = (uint8_t)(((valToEncodeU64 >> 21) & 0x7f) | 0x80);
        outputEncoded[1] = (uint8_t)(((valToEncodeU64 >> 28) & 0x7f) | 0x80);
        outputEncoded[0] = (uint8_t)(((valToEncodeU64 >> 35) & 0x7f) | 0x80);
        return 6;
    }
    else if (valToEncodeU64 <= SDNV64_MAX_7_BYTE) {
        outputEncoded[6] = (uint8_t)(valToEncodeU64 & 0x7f);
        outputEncoded[5] = (uint8_t)(((valToEncodeU64 >> 7) & 0x7f) | 0x80);
        outputEncoded[4] = (uint8_t)(((valToEncodeU64 >> 14) & 0x7f) | 0x80);
        outputEncoded[3] = (uint8_t)(((valToEncodeU64 >> 21) & 0x7f) | 0x80);
        outputEncoded[2] = (uint8_t)(((valToEncodeU64 >> 28) & 0x7f) | 0x80);
        outputEncoded[1] = (uint8_t)(((valToEncodeU64 >> 35) & 0x7f) | 0x80);
        outputEncoded[0] = (uint8_t)(((valToEncodeU64 >> 42) & 0x7f) | 0x80);
        return 7;
    }
    else if (valToEncodeU64 <= SDNV64_MAX_8_BYTE) {
        outputEncoded[7] = (uint8_t)(valToEncodeU64 & 0x7f);
        outputEncoded[6] = (uint8_t)(((valToEncodeU64 >> 7) & 0x7f) | 0x80);
        outputEncoded[5] = (uint8_t)(((valToEncodeU64 >> 14) & 0x7f) | 0x80);
        outputEncoded[4] = (uint8_t)(((valToEncodeU64 >> 21) & 0x7f) | 0x80);
        outputEncoded[3] = (uint8_t)(((valToEncodeU64 >> 28) & 0x7f) | 0x80);
        outputEncoded[2] = (uint8_t)(((valToEncodeU64 >> 35) & 0x7f) | 0x80);
        outputEncoded[1] = (uint8_t)(((valToEncodeU64 >> 42) & 0x7f) | 0x80);
        outputEncoded[0] = (uint8_t)(((valToEncodeU64 >> 49) & 0x7f) | 0x80);
        return 8;
    }
    else if (valToEncodeU64 <= SDNV64_MAX_9_BYTE) {
        outputEncoded[8] = (uint8_t)(valToEncodeU64 & 0x7f);
        outputEncoded[7] = (uint8_t)(((valToEncodeU64 >> 7) & 0x7f) | 0x80);
        outputEncoded[6] = (uint8_t)(((valToEncodeU64 >> 14) & 0x7f) | 0x80);
        outputEncoded[5] = (uint8_t)(((valToEncodeU64 >> 21) & 0x7f) | 0x80);
        outputEncoded[4] = (uint8_t)(((valToEncodeU64 >> 28) & 0x7f) | 0x80);
        outputEncoded[3] = (uint8_t)(((valToEncodeU64 >> 35) & 0x7f) | 0x80);
        outputEncoded[2] = (uint8_t)(((valToEncodeU64 >> 42) & 0x7f) | 0x80);
        outputEncoded[1] = (uint8_t)(((valToEncodeU64 >> 49) & 0x7f) | 0x80);
        outputEncoded[0] = (uint8_t)(((valToEncodeU64 >> 56) & 0x7f) | 0x80);
        return 9;
    }
    else { //10 byte
        outputEncoded[9] = (uint8_t)(valToEncodeU64 & 0x7f);
        outputEncoded[8] = (uint8_t)(((valToEncodeU64 >> 7) & 0x7f) | 0x80);
        outputEncoded[7] = (uint8_t)(((valToEncodeU64 >> 14) & 0x7f) | 0x80);
        outputEncoded[6] = (uint8_t)(((valToEncodeU64 >> 21) & 0x7f) | 0x80);
        outputEncoded[5] = (uint8_t)(((valToEncodeU64 >> 28) & 0x7f) | 0x80);
        outputEncoded[4] = (uint8_t)(((valToEncodeU64 >> 35) & 0x7f) | 0x80);
        outputEncoded[3] = (uint8_t)(((valToEncodeU64 >> 42) & 0x7f) | 0x80);
        outputEncoded[2] = (uint8_t)(((valToEncodeU64 >> 49) & 0x7f) | 0x80);
        outputEncoded[1] = (uint8_t)(((valToEncodeU64 >> 56) & 0x7f) | 0x80);
        outputEncoded[0] = (uint8_t)(((valToEncodeU64 >> 63) & 0x7f) | 0x80);
        return 10;
    }
}

//return decoded value (return invalid number that must be ignored on failure)
//  also sets parameter numBytes taken to decode (set to 0 on failure)
uint32_t SdnvDecodeU32Classic(const uint8_t * inputEncoded, uint8_t * numBytes) {
    //(Initial Step) Set the result to 0.  Set an index to the first
    //  byte of the encoded SDNV.
    //(Recursion Step) Shift the result left 7 bits.  Add the low-order
    //   7 bits of the value at the index to the result.  If the high-order
    //   bit under the pointer is a 1, advance the index by one byte within
    //   the encoded SDNV and repeat the Recursion Step, otherwise return
    //   the current value of the result.

    const uint8_t firstByte = *inputEncoded;
    uint32_t result = 0;
    for (uint8_t byteCount = 1; byteCount <= 5; ++byteCount) {
        result <<= 7;
        const uint8_t currentByte = *inputEncoded++;
        result += (currentByte & 0x7f);
        if ((currentByte & 0x80) == 0) { //if msbit is a 0 then stop
            *numBytes = ((byteCount == 5) && (firstByte > 0x8f)) ? 0 : byteCount; //set 0 if invalid, decoded value would be > UINT32_MAX
            return result;
        }
    }
    *numBytes = 0;
    return 0;
}

//return decoded value (return invalid number that must be ignored on failure)
//  also sets parameter numBytes taken to decode (set to 0 on failure)
uint64_t SdnvDecodeU64Classic(const uint8_t * inputEncoded, uint8_t * numBytes) {
    //(Initial Step) Set the result to 0.  Set an index to the first
    //  byte of the encoded SDNV.
    //(Recursion Step) Shift the result left 7 bits.  Add the low-order
    //   7 bits of the value at the index to the result.  If the high-order
    //   bit under the pointer is a 1, advance the index by one byte within
    //   the encoded SDNV and repeat the Recursion Step, otherwise return
    //   the current value of the result.
    const uint8_t firstByte = *inputEncoded;
    uint64_t result = 0;
    for (uint8_t byteCount = 1; byteCount <= 10; ++byteCount) {
        result <<= 7;
        const uint8_t currentByte = *inputEncoded++;
        result += (currentByte & 0x7f);
        if ((currentByte & 0x80) == 0) { //if msbit is a 0 then stop
            *numBytes = ((byteCount == 10) && (firstByte > 0x81)) ? 0 : byteCount; //set 0 if invalid, decoded value would be > UINT64_MAX
            return result;
        }
    }
    *numBytes = 0;
    return 0;
}

#ifdef SDNV_USE_HARDWARE_ACCELERATION
static const uint64_t masksPdepPext1[17] = { //index 0 based for mask0x80Index //17 because mask index can be maximum 16
    0x7f00000000000000,
    0x7f7f000000000000,
    0x7f7f7f0000000000,
    0x7f7f7f7f00000000,
    0x7f7f7f7f7f000000,
    0x7f7f7f7f7f7f0000,
    0x7f7f7f7f7f7f7f00,
    0x7f7f7f7f7f7f7f7f,
    0x7f7f7f7f7f7f7f7f,
    0x7f7f7f7f7f7f7f7f,
    0,0,0,0,0,0,0 //don't cares for invalid masks above 10 bytes encoded
};
static const uint64_t masksPdepPext2[17] = { //index 0 based for mask0x80Index (for big 9 and 10 byte sdnv)
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0x7f00000000000000,
    0x7f7f000000000000,
    0,0,0,0,0,0,0 //don't cares for invalid masks above 10 bytes encoded
};

static const uint8_t decodedShifts[17] = { //index 0 based for maskIndex
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        7,//9 byte
        14,//10 byte
        0,0,0,0,0,0,0 //don't cares for invalid masks above 10 bytes encoded
};

static const uint64_t masks0x80[10] = {
        0x00,
        0x8000000000000000,
        0x8080000000000000,
        0x8080800000000000,
        0x8080808000000000,
        0x8080808080000000,
        0x8080808080800000,
        0x8080808080808000,
        0x8080808080808000,
        0x8080808080808000,
};
static const uint64_t masks0x80_2[10] = {
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0x8000000000000000,
    0x8080000000000000,
};

static const uint8_t mask0x80Indices[70] = {
    0,0,0,0,0,0,0,
    1,1,1,1,1,1,1,
    2,2,2,2,2,2,2,
    3,3,3,3,3,3,3,
    4,4,4,4,4,4,4,
    5,5,5,5,5,5,5,
    6,6,6,6,6,6,6,
    7,7,7,7,7,7,7,
    8,8,8,8,8,8,8,
    9,9,9,9,9,9,9
};
static const uint8_t memoryOffsets[10] = {
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    1, //idx 8
    2, //idx 9
};



#ifdef _MSC_VER
__declspec(align(16))
#endif
static const uint8_t SHUFFLE_SHR_128I[17*sizeof(__m128i)]
#ifndef _MSC_VER
__attribute__((aligned(16)))
#endif
 =
{
    //little endian reverse order (LSB first)
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, //shr 0
    0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x80, //shr 1
    0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x80, 0x80, //shr 2
    0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x80, 0x80, 0x80, //shr 3
    0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x80, 0x80, 0x80, 0x80, //shr 4
    0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x80, 0x80, 0x80, 0x80, 0x80, //shr 5
    0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, //shr 6
    0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, //shr 7
    0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, //shr 8
    0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, //shr 9
    0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, //shr 10
    0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, //shr 11
    0x0c, 0x0d, 0x0e, 0x0f, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, //shr 12
    0x0d, 0x0e, 0x0f, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, //shr 13
    0x0e, 0x0f, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, //shr 14
    0x0f, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, //shr 15
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, //shr 16
};


//the commented out version below attempting to bypass lookup tables with shifts turned out to be significantly slower
/*

//return output size
unsigned int SdnvEncodeU64Fast(uint8_t * outputEncoded, const uint64_t valToEncodeU64) {
    
    

    //if (valToEncodeU64 > SDNV64_MAX_8_BYTE) {
        //std::cout << "notice: SdnvEncodeU64Fast encountered a value that would result in an encoded SDNV greater than 8 bytes.. using classic version" << std::endl;
    //    return SdnvEncodeU64Classic(outputEncoded, valToEncodeU64);
    //}
    const unsigned int msb = (valToEncodeU64 != 0) * boost::multiprecision::detail::find_msb<uint64_t>(valToEncodeU64);
    const unsigned int mask0x80Index = (msb / 7);
    //const unsigned int sdnvSizeBytes = mask0x80Index + 1;
    //std::cout << "encode fast: msb: " << msb << " mask0x80Index: " << mask0x80Index << std::endl;
    //const uint64_t maskPdep1 = (static_cast<uint64_t>(0x7f7f7f7f7f7f7f7fU)) & ((static_cast<int64_t>(0xff00000000000000)) >> (mask0x80Index * 8)); //masksPdep1[mask0x80Index]
    const unsigned int mask0x80IndexIsLessThanOrEqualTo7 = (mask0x80Index <= 7);
    const unsigned int maskShift1 = (7 - mask0x80Index) * 8 * mask0x80IndexIsLessThanOrEqualTo7;
    const uint64_t maskPdep1 = (static_cast<uint64_t>(0x7f7f7f7f7f7f7f7fU)) << maskShift1; //masksPdep1[mask0x80Index]
    const uint64_t mask0x80 = (static_cast<uint64_t>(0x8080808080808000U)) << maskShift1; //masks0x80[mask0x80Index]
    //std::cout << "maskPdep1: " << std::hex << maskPdep1 << std::dec << std::endl;
    const uint64_t encoded64 = _pdep_u64(valToEncodeU64, maskPdep1) | mask0x80;
    //std::cout << "encoded64: " << std::hex << encoded64  << std::dec << std::endl;
    const uint64_t encoded64ForMemcpy = boost::endian::big_to_native(encoded64);
    if (mask0x80IndexIsLessThanOrEqualTo7) {
        _mm_stream_si64((long long int *)(outputEncoded), encoded64ForMemcpy);
    }
    else {
        //for the large 9 and 10 byte sdnv
        const uint64_t encoded64BigSdnv = _pdep_u64(valToEncodeU64 >> 56, masksPdep2[mask0x80Index]) | masks0x80_2[mask0x80Index];
        //std::cout << "encoded64BigSdnv: " << std::hex << encoded64BigSdnv  << std::dec << std::endl;
        const uint64_t encoded64BigSdnvForMemcpy = boost::endian::big_to_native(encoded64BigSdnv);
#if 0
        const __m128i forStore = _mm_set_epi64x(0, encoded64ForMemcpy); //put encoded64ForMemcpy in element 0
        _mm_storeu_si64(outputEncoded, forStore);//SSE Store 64-bit integer from the first element of a into memory. mem_addr does not need to be aligned on any particular boundary.
#else
        //outputEncoded[0] = static_cast<uint8_t>(encoded64BigSdnvForMemcpy);
        //outputEncoded[1] = static_cast<uint8_t>(encoded64BigSdnvForMemcpy >> 8);
        _mm_stream_si64((long long int *)(outputEncoded), encoded64BigSdnvForMemcpy);
        //if (valToEncodeU64 > SDNV64_MAX_8_BYTE) { for (unsigned int i = 0; i < 10; ++i) { std::cout << std::hex << (int)outputEncoded[i] << " "; } std::cout << std::endl; }
        const unsigned int memoryOffset = (mask0x80Index - 7) * (mask0x80Index > 7); //memoryOffsets[mask0x80Index]
        _mm_stream_si64((long long int *)(outputEncoded + memoryOffset), encoded64ForMemcpy);
        //if (valToEncodeU64 > SDNV64_MAX_8_BYTE) { for (unsigned int i = 0; i < 10; ++i) { std::cout << std::hex << (int)outputEncoded[i] << " "; } std::cout << std::endl; }

        //uint8_t numBytes;  std::cout << "valToEncodeU64: " << valToEncodeU64 << "  deoded: " << SdnvDecodeU64Classic(outputEncoded, &numBytes) << std::endl;
        //std::cout << "numbytes: " << (int) numBytes << std::endl;
    }
#endif
    return mask0x80Index + 1; //sdnvSizeBytes;
}
*/

//return output size
unsigned int SdnvEncodeU32Fast(uint8_t * outputEncoded, const uint32_t valToEncodeU32) {
    //critical that the compiler optimizes this instruction using cmovne instead of imul (which is what the casts to uint8_t and bool are for)
    //bitscan seems to have undefined behavior on a value of zero
    const unsigned int msb = (static_cast<bool>(valToEncodeU32 != 0)) * (static_cast<uint8_t>(boost::multiprecision::detail::find_msb<uint32_t>(valToEncodeU32)));

    const unsigned int mask0x80Index = mask0x80Indices[msb]; //(msb / 7);
    //std::cout << "encode fast: msb: " << msb << std::endl;
    const uint64_t encoded64 = _pdep_u64(valToEncodeU32, masksPdepPext1[mask0x80Index]) | masks0x80[mask0x80Index];
    //std::cout << "encoded64: " << std::hex << encoded64  << std::dec << std::endl;
    const uint64_t encoded64ForMemcpy = boost::endian::big_to_native(encoded64);

    _mm_stream_si64((long long int *)(outputEncoded), encoded64ForMemcpy);
    
    return mask0x80Index + 1; //sdnvSizeBytes = mask0x80Index + 1;
}

//return output size
unsigned int SdnvEncodeU64Fast(uint8_t * outputEncoded, const uint64_t valToEncodeU64) {

    //critical that the compiler optimizes this instruction using cmovne instead of imul (which is what the casts to uint8_t and bool are for)
    //bitscan seems to have undefined behavior on a value of zero
    const unsigned int msb = (static_cast<bool>(valToEncodeU64 != 0)) * (static_cast<uint8_t>(boost::multiprecision::detail::find_msb<uint64_t>(valToEncodeU64)));

    const unsigned int mask0x80Index = mask0x80Indices[msb]; //(msb / 7);
    //std::cout << "encode fast: msb: " << msb << std::endl;
    const uint64_t encoded64 = _pdep_u64(valToEncodeU64, masksPdepPext1[mask0x80Index]) | masks0x80[mask0x80Index];
    //std::cout << "encoded64: " << std::hex << encoded64  << std::dec << std::endl;
    const uint64_t encoded64ForMemcpy = boost::endian::big_to_native(encoded64);
#define ENCODE_USE_BRANCHING 0
#if ENCODE_USE_BRANCHING
    if (mask0x80Index <= 7) {
        _mm_stream_si64((long long int *)(outputEncoded), encoded64ForMemcpy);
    }
    else {
#endif
        //for the large 9 and 10 byte sdnv
        const uint64_t encoded64BigSdnv = _pdep_u64(valToEncodeU64 >> 56, masksPdepPext2[mask0x80Index]) | masks0x80_2[mask0x80Index];
        //std::cout << "encoded64BigSdnv: " << std::hex << encoded64BigSdnv  << std::dec << std::endl;
        const uint64_t encoded64BigSdnvForMemcpy = boost::endian::big_to_native(encoded64BigSdnv);

        _mm_stream_si64((long long int *)(outputEncoded), encoded64BigSdnvForMemcpy);
        //if (valToEncodeU64 > SDNV64_MAX_8_BYTE) { for (unsigned int i = 0; i < 10; ++i) { std::cout << std::hex << (int)outputEncoded[i] << " "; } std::cout << std::endl; }
        _mm_stream_si64((long long int *)(outputEncoded + memoryOffsets[mask0x80Index]), encoded64ForMemcpy);
        //if (valToEncodeU64 > SDNV64_MAX_8_BYTE) { for (unsigned int i = 0; i < 10; ++i) { std::cout << std::hex << (int)outputEncoded[i] << " "; } std::cout << std::endl; }

        //uint8_t numBytes;  std::cout << "valToEncodeU64: " << valToEncodeU64 << "  deoded: " << SdnvDecodeU64Classic(outputEncoded, &numBytes) << std::endl;
        //std::cout << "numbytes: " << (int) numBytes << std::endl;
#if ENCODE_USE_BRANCHING
    }
#endif
    return mask0x80Index + 1; //sdnvSizeBytes = mask0x80Index + 1;
}

//return decoded value (return invalid number that must be ignored on failure)
//  also sets parameter numBytes taken to decode (set to 0 on failure)
uint32_t SdnvDecodeU32Fast(const uint8_t * data, uint8_t * numBytes) {
    //(Initial Step) Set the result to 0.  Set an index to the first
    //  byte of the encoded SDNV.
    //(Recursion Step) Shift the result left 7 bits.  Add the low-order
    //   7 bits of the value at the index to the result.  If the high-order
    //   bit under the pointer is a 1, advance the index by one byte within
    //   the encoded SDNV and repeat the Recursion Step, otherwise return
    //   the current value of the result.

    //Instruction   Selector mask    Source       Destination
    //PEXT          0xff00fff0       0x12345678   0x00012567
    //PDEP          0xff00fff0       0x00012567   0x12005670


    __m128i sdnvEncoded = _mm_loadu_si64(data); //SSE Load unaligned 64-bit integer from memory into the first element of dst.
    int significantBitsSetMask = _mm_movemask_epi8(sdnvEncoded);//SSE2 most significant bit of the corresponding packed 8-bit integer in a. //_mm_movepi8_mask(sdnvEncoded); 
    const uint8_t maskIndex = static_cast<uint8_t>(boost::multiprecision::detail::find_lsb<int>(~significantBitsSetMask));
    const uint64_t encoded64 = _mm_cvtsi128_si64(sdnvEncoded); //SSE2 Copy the lower 64-bit integer in a to dst.
    const uint64_t u64ByteSwapped = boost::endian::big_to_native(encoded64);
    const uint64_t decoded = _pext_u64(u64ByteSwapped, masksPdepPext1[maskIndex]);
    const bool valid = (maskIndex < 4) + ((maskIndex == 4) * (data[0] <= 0x8f));
    *numBytes = (maskIndex + 1) * valid; //sdnvSizeBytes;
    return static_cast<uint32_t>(decoded);
}

//return decoded value (return invalid number that must be ignored on failure)
//  also sets parameter numBytes taken to decode (set to 0 on failure)
uint64_t SdnvDecodeU64Fast(const uint8_t * data, uint8_t * numBytes) {
    //(Initial Step) Set the result to 0.  Set an index to the first
    //  byte of the encoded SDNV.
    //(Recursion Step) Shift the result left 7 bits.  Add the low-order
    //   7 bits of the value at the index to the result.  If the high-order
    //   bit under the pointer is a 1, advance the index by one byte within
    //   the encoded SDNV and repeat the Recursion Step, otherwise return
    //   the current value of the result.

    //Instruction   Selector mask    Source       Destination
    //PEXT          0xff00fff0       0x12345678   0x00012567
    //PDEP          0xff00fff0       0x00012567   0x12005670


/*
    __m128i sdnvEncoded = _mm_loadu_si64(data); //SSE Load unaligned 64-bit integer from memory into the first element of dst.
    int significantBitsSetMask = _mm_movemask_epi8(sdnvEncoded);//SSE2 most significant bit of the corresponding packed 8-bit integer in a. //_mm_movepi8_mask(sdnvEncoded); 
    if (significantBitsSetMask == 0x00ff) {
        //std::cout << "notice: SdnvDecodeU64Fast encountered an encoded SDNV greater than 8 bytes.. using classic version" << std::endl;
        return SdnvDecodeU64Classic(data, numBytes);
    }
    const unsigned int sdnvSizeBytes = boost::multiprecision::detail::find_lsb<int>(~significantBitsSetMask) + 1;
    const uint64_t encoded64 = _mm_cvtsi128_si64(sdnvEncoded); //SSE2 Copy the lower 64-bit integer in a to dst.
    const uint64_t u64ByteSwapped = boost::endian::big_to_native(encoded64);
    uint64_t decoded = _pext_u64(u64ByteSwapped, masks[sdnvSizeBytes]);
    //std::cout << "significantBitsSetMask: " << significantBitsSetMask << " sdnvsize: " << sdnvSizeBytes << " u64ByteSwapped " << std::hex << u64ByteSwapped << " d " << std::dec << decoded << std::endl;
    *numBytes = sdnvSizeBytes;
    return decoded;
*/

    
    __m128i sdnvEncoded = _mm_loadu_si128((__m128i const*) data); //SSE2 Load 128-bits of integer data from memory into dst. mem_addr does not need to be aligned on any particular boundary.
    int significantBitsSetMask = _mm_movemask_epi8(sdnvEncoded);//SSE2 most significant bit of the corresponding packed 8-bit integer in a. //_mm_movepi8_mask(sdnvEncoded); 
    const uint8_t maskIndex = static_cast<uint8_t>(boost::multiprecision::detail::find_lsb<int>(~significantBitsSetMask));
    const uint64_t encoded64 = _mm_cvtsi128_si64(sdnvEncoded); //SSE2 Copy the lower 64-bit integer in a to dst.
    const uint64_t u64ByteSwapped = boost::endian::big_to_native(encoded64);
    uint64_t decoded = _pext_u64(u64ByteSwapped, masksPdepPext1[maskIndex]);
    decoded <<= decodedShifts[maskIndex];
    sdnvEncoded = _mm_srli_si128(sdnvEncoded, 8);
    const uint64_t encoded64High = _mm_cvtsi128_si64(sdnvEncoded); //SSE2 Copy the lower 64-bit integer in a to dst.
    const uint64_t u64HighByteSwapped = boost::endian::big_to_native(encoded64High);
    const uint64_t decoded2 = _pext_u64(u64HighByteSwapped, masksPdepPext2[maskIndex]);
    decoded |= decoded2;
    const bool valid = (maskIndex < 9) + ((maskIndex == 9) * (data[0] <= 0x81));
    //std::cout << "significantBitsSetMask: " << significantBitsSetMask << " maskIndex: " << maskIndex << " u64ByteSwapped " << std::hex << u64ByteSwapped << " d " << std::dec << decoded << std::endl;
    //std::cout << " u64HighByteSwapped " << std::hex << u64HighByteSwapped << " d2 " << decoded2 << std::dec << std::endl;
    *numBytes = (maskIndex + 1) * valid; //sdnvSizeBytes;
    return decoded;
}


//return num values decoded this iteration
unsigned int SdnvDecodeMultipleU64Fast(const uint8_t * data, uint8_t * numBytes, uint64_t * decodedValues, unsigned int decodedRemaining) {
    //(Initial Step) Set the result to 0.  Set an index to the first
    //  byte of the encoded SDNV.
    //(Recursion Step) Shift the result left 7 bits.  Add the low-order
    //   7 bits of the value at the index to the result.  If the high-order
    //   bit under the pointer is a 1, advance the index by one byte within
    //   the encoded SDNV and repeat the Recursion Step, otherwise return
    //   the current value of the result.

    //Instruction   Selector mask    Source       Destination
    //PEXT          0xff00fff0       0x12345678   0x00012567
    //PDEP          0xff00fff0       0x00012567   0x12005670



    //static const __m128i junk;
    //__m128i sdnvEncoded = _mm_mask_loadu_epi8(junk,0xffff,data);
    __m128i sdnvsEncoded = _mm_loadu_si128((__m128i const*) data); //SSE2 Load 128-bits of integer data from memory into dst. mem_addr does not need to be aligned on any particular boundary.
    //{
    //    const uint64_t encoded64 = _mm_cvtsi128_si64(sdnvsEncoded); //SSE2 Copy the lower 64-bit integer in a to dst.
    //    std::cout << "encoded64lower: " << std::hex << encoded64 << std::dec << std::endl;
    //}
    unsigned int bytesRemainingIn128Buffer = static_cast<unsigned int>(sizeof(__m128i));
    const unsigned int decodedStart = decodedRemaining;
    //std::cout << "called SdnvDecodeU64Fast" << std::endl;
    while (decodedRemaining && bytesRemainingIn128Buffer) {
        //std::cout << "remaining = " << decodedRemaining << " tbd=" << (int)sdnvTotalSizeBytesDecoded << std::endl;
              
        int significantBitsSetMask = _mm_movemask_epi8(sdnvsEncoded);//SSE2 most significant bit of the corresponding packed 8-bit integer in a. //_mm_movepi8_mask(sdnvEncoded); 
        //std::cout << "  significantBitsSetMask: " << std::hex << significantBitsSetMask << std::dec << std::endl;
        const unsigned int maskIndex = boost::multiprecision::detail::find_lsb<int>(~significantBitsSetMask);
        const unsigned int sdnvSizeBytes = maskIndex + 1;
        if (sdnvSizeBytes > bytesRemainingIn128Buffer) {
            //std::cout << "breaking..\n";
            break;
        }
        
        const uint64_t encoded64 = _mm_cvtsi128_si64(sdnvsEncoded); //SSE2 Copy the lower 64-bit integer in a to dst.
        const uint64_t u64ByteSwapped = boost::endian::big_to_native(encoded64);
        uint64_t decoded = _pext_u64(u64ByteSwapped, masksPdepPext1[maskIndex]);
        decoded <<= decodedShifts[maskIndex];
        const uint64_t encoded64High = _mm_cvtsi128_si64(_mm_srli_si128(sdnvsEncoded, 8)); //SSE2 Copy the lower 64-bit integer in a to dst.
        const uint64_t u64HighByteSwapped = boost::endian::big_to_native(encoded64High);
        uint64_t decoded2 = _pext_u64(u64HighByteSwapped, masksPdepPext2[maskIndex]);
        decoded |= decoded2;
        *decodedValues++ = decoded;
        //std::cout << "  sdnvSizeBytes = " << sdnvSizeBytes << std::endl;
        //std::cout << "  decoded = " << decoded << std::endl;
        
//#define DECODE_MULTIPLE_USE_LOOP 1 //slow
#define DECODE_MULTIPLE_USE_SHUFFLE 1
//#define DECODE_MULTIPLE_USE_SWITCH 1

#ifdef DECODE_MULTIPLE_USE_LOOP
        for (unsigned int i = 0; i < sdnvSizeBytes; ++i) {
            sdnvsEncoded = _mm_srli_si128(sdnvsEncoded, 1);
        }
#elif defined DECODE_MULTIPLE_USE_SHUFFLE
        //const __m128i shuffleControlMask = _mm_loadu_si128((__m128i*) &SHUFFLE_SHR_128I[sdnvSizeBytes * sizeof(__m128i)]);
        const __m128i shuffleControlMask = _mm_stream_load_si128((__m128i*) &SHUFFLE_SHR_128I[sdnvSizeBytes * sizeof(__m128i)]); //SSE4.1 requires alignment
        sdnvsEncoded = _mm_shuffle_epi8(sdnvsEncoded, shuffleControlMask);
        //sdnvsEncoded = _mm_shuffle_epi8(sdnvsEncoded, SHUFFLE_CONTROL_MASK_M128I[sdnvSizeBytes]);
#else
        //this switch statement will be optimized by the compiler into a jump table, and sdnvsEncoded will remain in
        // the SIMD registers as opposed to using the function array method resulting in expensive SIMD load operations
        switch (sdnvSizeBytes) {
        case 0: sdnvsEncoded = _mm_srli_si128(sdnvsEncoded, 0); break;
        case 1: sdnvsEncoded = _mm_srli_si128(sdnvsEncoded, 1); break;
        case 2: sdnvsEncoded = _mm_srli_si128(sdnvsEncoded, 2); break;
        case 3: sdnvsEncoded = _mm_srli_si128(sdnvsEncoded, 3); break;
        case 4: sdnvsEncoded = _mm_srli_si128(sdnvsEncoded, 4); break;
        case 5: sdnvsEncoded = _mm_srli_si128(sdnvsEncoded, 5); break;
        case 6: sdnvsEncoded = _mm_srli_si128(sdnvsEncoded, 6); break;
        case 7: sdnvsEncoded = _mm_srli_si128(sdnvsEncoded, 7); break;
        case 8: sdnvsEncoded = _mm_srli_si128(sdnvsEncoded, 8); break;
        case 9: sdnvsEncoded = _mm_srli_si128(sdnvsEncoded, 9); break;
        case 10: sdnvsEncoded = _mm_srli_si128(sdnvsEncoded, 10); break;
        case 11: sdnvsEncoded = _mm_srli_si128(sdnvsEncoded, 11); break;
        case 12: sdnvsEncoded = _mm_srli_si128(sdnvsEncoded, 12); break;
        case 13: sdnvsEncoded = _mm_srli_si128(sdnvsEncoded, 13); break;
        case 14: sdnvsEncoded = _mm_srli_si128(sdnvsEncoded, 14); break;
        case 15: sdnvsEncoded = _mm_srli_si128(sdnvsEncoded, 15); break;
        case 16: sdnvsEncoded = _mm_srli_si128(sdnvsEncoded, 16); break;
        }

        //const __m128i ZERO = _mm_set1_epi32(0);
        //for (unsigned int i = 0; i < sdnvSizeBytes; ++i) {
        //    sdnvsEncoded = _mm_alignr_epi8(ZERO, sdnvsEncoded, 1);// Concatenate 16-byte blocks in a and b into a 32-byte temporary result, shift the result right by imm8 bytes, and store the low 16 bytes in dst.
        //}

#endif
        bytesRemainingIn128Buffer -= sdnvSizeBytes;
        --decodedRemaining;
    }
    //std::cout << "significantBitsSetMask: " << significantBitsSetMask << " sdnvsize: " << sdnvSizeBytes << " u64ByteSwapped " << std::hex << u64ByteSwapped << " d " << std::dec << decoded << std::endl;
    *numBytes = static_cast<unsigned int>(sizeof(__m128i)) - bytesRemainingIn128Buffer;
    return decodedStart - decodedRemaining;
}

//return num values decoded this iteration
unsigned int SdnvDecodeMultiple256BitU64Fast(const uint8_t * data, uint8_t * numBytes, uint64_t * decodedValues, unsigned int decodedRemaining) {
    //(Initial Step) Set the result to 0.  Set an index to the first
    //  byte of the encoded SDNV.
    //(Recursion Step) Shift the result left 7 bits.  Add the low-order
    //   7 bits of the value at the index to the result.  If the high-order
    //   bit under the pointer is a 1, advance the index by one byte within
    //   the encoded SDNV and repeat the Recursion Step, otherwise return
    //   the current value of the result.

    //Instruction   Selector mask    Source       Destination
    //PEXT          0xff00fff0       0x12345678   0x00012567
    //PDEP          0xff00fff0       0x00012567   0x12005670



    //static const __m128i junk;
    //__m128i sdnvEncoded = _mm_mask_loadu_epi8(junk,0xffff,data);
    __m256i sdnvsEncoded = _mm256_loadu_si256((__m256i const*) data); //AVX Load 256-bits of integer data from memory into dst. mem_addr does not need to be aligned on any particular boundary.
    //{
    //    const uint64_t encoded64 = _mm_cvtsi128_si64(_mm256_castsi256_si128(sdnvsEncoded)); //SSE2 Copy the lower 64-bit integer in a to dst.
    //    std::cout << "encoded64lower: " << std::hex << encoded64 << std::dec << std::endl;
    //}
    unsigned int bytesRemainingIn256Buffer = static_cast<unsigned int>(sizeof(__m256i));
    const unsigned int decodedStart = decodedRemaining;
    //std::cout << "called SdnvDecodeU64Fast" << std::endl;
    while (decodedRemaining && bytesRemainingIn256Buffer) {
        //std::cout << "remaining = " << decodedRemaining << " tbd=" << (int)sdnvTotalSizeBytesDecoded << std::endl;

        int significantBitsSetMask = _mm256_movemask_epi8(sdnvsEncoded);//SSE2 most significant bit of the corresponding packed 8-bit integer in a. //_mm_movepi8_mask(sdnvEncoded); 
        //std::cout << "  significantBitsSetMask: " << std::hex << significantBitsSetMask << std::dec << std::endl;
        const unsigned int maskIndex = boost::multiprecision::detail::find_lsb<int>(~significantBitsSetMask);
        const unsigned int sdnvSizeBytes = maskIndex + 1;
        if (sdnvSizeBytes > bytesRemainingIn256Buffer) {
            //std::cout << "breaking..\n";
            break;
        }

        const uint64_t encoded64 = _mm_cvtsi128_si64(_mm256_castsi256_si128(sdnvsEncoded)); //SSE2 Copy the lower 64-bit integer in a to dst.
        const uint64_t u64ByteSwapped = boost::endian::big_to_native(encoded64);
        uint64_t decoded = _pext_u64(u64ByteSwapped, masksPdepPext1[maskIndex]);
        decoded <<= decodedShifts[maskIndex];
        const uint64_t encoded64High = _mm_cvtsi128_si64(_mm256_castsi256_si128(_mm256_srli_si256(sdnvsEncoded, 8))); //SSE2 Copy the lower 64-bit integer in a to dst.
        const uint64_t u64HighByteSwapped = boost::endian::big_to_native(encoded64High);
        uint64_t decoded2 = _pext_u64(u64HighByteSwapped, masksPdepPext2[maskIndex]);
        decoded |= decoded2;
        *decodedValues++ = decoded;

        
        //this switch statement will be optimized by the compiler into a jump table, and sdnvsEncoded and shiftIn will remain in
        // the SIMD registers as opposed to using the function array method resulting in expensive SIMD load operations
        const __m256i shiftIn = _mm256_permute2f128_si256(_mm256_setzero_si256(), sdnvsEncoded, 0x03);
        switch (sdnvSizeBytes) {
        case 0: sdnvsEncoded = _mm256_alignr_epi8(shiftIn, sdnvsEncoded, 0); break;
        case 1: sdnvsEncoded = _mm256_alignr_epi8(shiftIn, sdnvsEncoded, 1); break;
        case 2: sdnvsEncoded = _mm256_alignr_epi8(shiftIn, sdnvsEncoded, 2); break;
        case 3: sdnvsEncoded = _mm256_alignr_epi8(shiftIn, sdnvsEncoded, 3); break;
        case 4: sdnvsEncoded = _mm256_alignr_epi8(shiftIn, sdnvsEncoded, 4); break;
        case 5: sdnvsEncoded = _mm256_alignr_epi8(shiftIn, sdnvsEncoded, 5); break;
        case 6: sdnvsEncoded = _mm256_alignr_epi8(shiftIn, sdnvsEncoded, 6); break;
        case 7: sdnvsEncoded = _mm256_alignr_epi8(shiftIn, sdnvsEncoded, 7); break;
        case 8: sdnvsEncoded = _mm256_alignr_epi8(shiftIn, sdnvsEncoded, 8); break;
        case 9: sdnvsEncoded = _mm256_alignr_epi8(shiftIn, sdnvsEncoded, 9); break;
        case 10: sdnvsEncoded = _mm256_alignr_epi8(shiftIn, sdnvsEncoded, 10); break;
        case 11: sdnvsEncoded = _mm256_alignr_epi8(shiftIn, sdnvsEncoded, 11); break;
        case 12: sdnvsEncoded = _mm256_alignr_epi8(shiftIn, sdnvsEncoded, 12); break;
        case 13: sdnvsEncoded = _mm256_alignr_epi8(shiftIn, sdnvsEncoded, 13); break;
        case 14: sdnvsEncoded = _mm256_alignr_epi8(shiftIn, sdnvsEncoded, 14); break;
        case 15: sdnvsEncoded = _mm256_alignr_epi8(shiftIn, sdnvsEncoded, 15); break;
        case 16: sdnvsEncoded = _mm256_alignr_epi8(shiftIn, sdnvsEncoded, 16); break;
        case 17: sdnvsEncoded = _mm256_alignr_epi8(shiftIn, sdnvsEncoded, 17); break;
        case 18: sdnvsEncoded = _mm256_alignr_epi8(shiftIn, sdnvsEncoded, 18); break;
        case 19: sdnvsEncoded = _mm256_alignr_epi8(shiftIn, sdnvsEncoded, 19); break;
        case 20: sdnvsEncoded = _mm256_alignr_epi8(shiftIn, sdnvsEncoded, 20); break;
        case 21: sdnvsEncoded = _mm256_alignr_epi8(shiftIn, sdnvsEncoded, 21); break;
        case 22: sdnvsEncoded = _mm256_alignr_epi8(shiftIn, sdnvsEncoded, 22); break;
        case 23: sdnvsEncoded = _mm256_alignr_epi8(shiftIn, sdnvsEncoded, 23); break;
        case 24: sdnvsEncoded = _mm256_alignr_epi8(shiftIn, sdnvsEncoded, 24); break;
        case 25: sdnvsEncoded = _mm256_alignr_epi8(shiftIn, sdnvsEncoded, 25); break;
        case 26: sdnvsEncoded = _mm256_alignr_epi8(shiftIn, sdnvsEncoded, 26); break;
        case 27: sdnvsEncoded = _mm256_alignr_epi8(shiftIn, sdnvsEncoded, 27); break;
        case 28: sdnvsEncoded = _mm256_alignr_epi8(shiftIn, sdnvsEncoded, 28); break;
        case 29: sdnvsEncoded = _mm256_alignr_epi8(shiftIn, sdnvsEncoded, 29); break;
        case 30: sdnvsEncoded = _mm256_alignr_epi8(shiftIn, sdnvsEncoded, 30); break;
        case 31: sdnvsEncoded = _mm256_alignr_epi8(shiftIn, sdnvsEncoded, 31); break;
        case 32: sdnvsEncoded = _mm256_alignr_epi8(shiftIn, sdnvsEncoded, 32); break;        
        }

        bytesRemainingIn256Buffer -= sdnvSizeBytes;
        --decodedRemaining;
    }
    //std::cout << "significantBitsSetMask: " << significantBitsSetMask << " sdnvsize: " << sdnvSizeBytes << " u64ByteSwapped " << std::hex << u64ByteSwapped << " d " << std::dec << decoded << std::endl;
    *numBytes = static_cast<unsigned int>(sizeof(__m256i)) - bytesRemainingIn256Buffer;
    return decodedStart - decodedRemaining;
}

#endif //#ifdef SDNV_USE_HARDWARE_ACCELERATION