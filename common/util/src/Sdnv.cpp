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
    return SdnvEncodeU64Fast(outputEncoded, valToEncodeU32);
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
    return static_cast<uint32_t>(SdnvDecodeU64Fast(inputEncoded, numBytes));
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

//return decoded value (0 if failure), also set parameter numBytes taken to decode
uint32_t SdnvDecodeU32Classic(const uint8_t * inputEncoded, uint8_t * numBytes) {
    //(Initial Step) Set the result to 0.  Set an index to the first
    //  byte of the encoded SDNV.
    //(Recursion Step) Shift the result left 7 bits.  Add the low-order
    //   7 bits of the value at the index to the result.  If the high-order
    //   bit under the pointer is a 1, advance the index by one byte within
    //   the encoded SDNV and repeat the Recursion Step, otherwise return
    //   the current value of the result.

    uint32_t result = 0;
    for (uint8_t byteCount = 1; byteCount <= 5; ++byteCount) {
        result <<= 7;
        const uint8_t currentByte = *inputEncoded++;
        result += (currentByte & 0x7f);
        if ((currentByte & 0x80) == 0) { //if msbit is a 0 then stop
            *numBytes = byteCount;
            return result;
        }
    }
    *numBytes = 0;
    return 0;
}

//return decoded value (0 if failure), also set parameter numBytes taken to decode
uint64_t SdnvDecodeU64Classic(const uint8_t * inputEncoded, uint8_t * numBytes) {
    //(Initial Step) Set the result to 0.  Set an index to the first
    //  byte of the encoded SDNV.
    //(Recursion Step) Shift the result left 7 bits.  Add the low-order
    //   7 bits of the value at the index to the result.  If the high-order
    //   bit under the pointer is a 1, advance the index by one byte within
    //   the encoded SDNV and repeat the Recursion Step, otherwise return
    //   the current value of the result.

    uint64_t result = 0;
    for (uint8_t byteCount = 1; byteCount <= 10; ++byteCount) {
        result <<= 7;
        const uint8_t currentByte = *inputEncoded++;
        result += (currentByte & 0x7f);
        if ((currentByte & 0x80) == 0) { //if msbit is a 0 then stop
            *numBytes = byteCount;
            return result;
        }
    }
    *numBytes = 0;
    return 0;
}

static const uint64_t masks[9] = { //index 1 based for sdnvSizeBytes
    0x00,
    0x7f00000000000000,
    0x7f7f000000000000,
    0x7f7f7f0000000000,
    0x7f7f7f7f00000000,
    0x7f7f7f7f7f000000,
    0x7f7f7f7f7f7f0000,
    0x7f7f7f7f7f7f7f00,
    0x7f7f7f7f7f7f7f7f
};

static const uint64_t masksPdep[8] = { //index 0 based for mask0x80Index
    0x7f00000000000000,
    0x7f7f000000000000,
    0x7f7f7f0000000000,
    0x7f7f7f7f00000000,
    0x7f7f7f7f7f000000,
    0x7f7f7f7f7f7f0000,
    0x7f7f7f7f7f7f7f00,
    0x7f7f7f7f7f7f7f7f
};

static const uint64_t masks0x80[9] = {
    0x00,
    0x8000000000000000,
    0x8080000000000000,
    0x8080800000000000,
    0x8080808000000000,
    0x8080808080000000,
    0x8080808080800000,
    0x8080808080808000,
    0x8080808080808080
};

//return output size
unsigned int SdnvEncodeU64Fast(uint8_t * outputEncoded, const uint64_t valToEncodeU64) {
    if (valToEncodeU64 > SDNV64_MAX_8_BYTE) {
        //std::cout << "notice: SdnvEncodeU64Fast encountered a value that would result in an encoded SDNV greater than 8 bytes.. using classic version" << std::endl;
        return SdnvEncodeU64Classic(outputEncoded, valToEncodeU64);
    }
    const unsigned int msb = (valToEncodeU64 != 0) ? boost::multiprecision::detail::find_msb<uint64_t>(valToEncodeU64) : 0;
    const unsigned int mask0x80Index = (msb / 7);
    //const unsigned int sdnvSizeBytes = mask0x80Index + 1;
    //std::cout << "encode fast: msb: " << msb << std::endl;
    const uint64_t encoded64 = _pdep_u64(valToEncodeU64, masksPdep[mask0x80Index]) | masks0x80[mask0x80Index];
    //std::cout << "encoded64: " << std::hex << encoded64  << std::dec << std::endl;
    const uint64_t encoded64ForMemcpy = boost::endian::big_to_native(encoded64);
#if 0
    const __m128i forStore =_mm_set_epi64x(0, encoded64ForMemcpy); //put encoded64ForMemcpy in element 0
    _mm_storeu_si64(outputEncoded, forStore);//SSE Store 64-bit integer from the first element of a into memory. mem_addr does not need to be aligned on any particular boundary.
#else
    _mm_stream_si64((long long int *)outputEncoded, encoded64ForMemcpy);
#endif
    return mask0x80Index + 1; //sdnvSizeBytes;
}


//return decoded value (0 if failure), also set parameter numBytes taken to decode
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



    //static const __m128i junk;
    //__m128i sdnvEncoded = _mm_mask_loadu_epi8(junk,0xffff,data);
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
}

//#define SIMD_BYTESHIFT_USE_FUNCTION_ARRAY 1

#ifdef SIMD_BYTESHIFT_USE_FUNCTION_ARRAY


typedef void(*m128_shift_func_t)(__m128i &);

BOOST_FORCEINLINE void M128i_ShiftRight0Byte(__m128i & val) { val = _mm_srli_si128(val, 0); }
BOOST_FORCEINLINE void M128i_ShiftRight1Byte(__m128i & val) { val = _mm_srli_si128(val, 1); }
BOOST_FORCEINLINE void M128i_ShiftRight2Byte(__m128i & val) { val = _mm_srli_si128(val, 2); }
BOOST_FORCEINLINE void M128i_ShiftRight3Byte(__m128i & val) { val = _mm_srli_si128(val, 3); }
BOOST_FORCEINLINE void M128i_ShiftRight4Byte(__m128i & val) { val = _mm_srli_si128(val, 4); }
BOOST_FORCEINLINE void M128i_ShiftRight5Byte(__m128i & val) { val = _mm_srli_si128(val, 5); }
BOOST_FORCEINLINE void M128i_ShiftRight6Byte(__m128i & val) { val = _mm_srli_si128(val, 6); }
BOOST_FORCEINLINE void M128i_ShiftRight7Byte(__m128i & val) { val = _mm_srli_si128(val, 7); }
BOOST_FORCEINLINE void M128i_ShiftRight8Byte(__m128i & val) { val = _mm_srli_si128(val, 8); }
BOOST_FORCEINLINE void M128i_ShiftRight9Byte(__m128i & val) { val = _mm_srli_si128(val, 9); }
BOOST_FORCEINLINE void M128i_ShiftRight10Byte(__m128i & val) { val = _mm_srli_si128(val, 10); }
BOOST_FORCEINLINE void M128i_ShiftRight11Byte(__m128i & val) { val = _mm_srli_si128(val, 11); }
BOOST_FORCEINLINE void M128i_ShiftRight12Byte(__m128i & val) { val = _mm_srli_si128(val, 12); }
BOOST_FORCEINLINE void M128i_ShiftRight13Byte(__m128i & val) { val = _mm_srli_si128(val, 13); }
BOOST_FORCEINLINE void M128i_ShiftRight14Byte(__m128i & val) { val = _mm_srli_si128(val, 14); }
BOOST_FORCEINLINE void M128i_ShiftRight15Byte(__m128i & val) { val = _mm_srli_si128(val, 15); }
BOOST_FORCEINLINE void M128i_ShiftRight16Byte(__m128i & val) { val = _mm_srli_si128(val, 16); }

static const m128_shift_func_t M128i_ShiftRightByteFunctions[17] = {
    &M128i_ShiftRight0Byte,
    &M128i_ShiftRight1Byte,
    &M128i_ShiftRight2Byte,
    &M128i_ShiftRight3Byte,
    &M128i_ShiftRight4Byte,
    &M128i_ShiftRight5Byte,
    &M128i_ShiftRight6Byte,
    &M128i_ShiftRight7Byte,
    &M128i_ShiftRight8Byte,
    &M128i_ShiftRight9Byte,
    &M128i_ShiftRight10Byte,
    &M128i_ShiftRight11Byte,
    &M128i_ShiftRight12Byte,
    &M128i_ShiftRight13Byte,
    &M128i_ShiftRight14Byte,
    &M128i_ShiftRight15Byte,
    &M128i_ShiftRight16Byte
};

typedef void(*m256_shift_func_t)(__m256i &);

BOOST_FORCEINLINE void M256i_ShiftRight0Byte(__m256i & val) { val = _mm256_alignr_epi8(_mm256_permute2f128_si256(_mm256_set1_epi32(0), val, 0x03), val, 0); }
BOOST_FORCEINLINE void M256i_ShiftRight1Byte(__m256i & val) { val = _mm256_alignr_epi8(_mm256_permute2f128_si256(_mm256_set1_epi32(0), val, 0x03), val, 1); }
BOOST_FORCEINLINE void M256i_ShiftRight2Byte(__m256i & val) { val = _mm256_alignr_epi8(_mm256_permute2f128_si256(_mm256_set1_epi32(0), val, 0x03), val, 2); }
BOOST_FORCEINLINE void M256i_ShiftRight3Byte(__m256i & val) { val = _mm256_alignr_epi8(_mm256_permute2f128_si256(_mm256_set1_epi32(0), val, 0x03), val, 3); }
BOOST_FORCEINLINE void M256i_ShiftRight4Byte(__m256i & val) { val = _mm256_alignr_epi8(_mm256_permute2f128_si256(_mm256_set1_epi32(0), val, 0x03), val, 4); }
BOOST_FORCEINLINE void M256i_ShiftRight5Byte(__m256i & val) { val = _mm256_alignr_epi8(_mm256_permute2f128_si256(_mm256_set1_epi32(0), val, 0x03), val, 5); }
BOOST_FORCEINLINE void M256i_ShiftRight6Byte(__m256i & val) { val = _mm256_alignr_epi8(_mm256_permute2f128_si256(_mm256_set1_epi32(0), val, 0x03), val, 6); }
BOOST_FORCEINLINE void M256i_ShiftRight7Byte(__m256i & val) { val = _mm256_alignr_epi8(_mm256_permute2f128_si256(_mm256_set1_epi32(0), val, 0x03), val, 7); }
BOOST_FORCEINLINE void M256i_ShiftRight8Byte(__m256i & val) { val = _mm256_alignr_epi8(_mm256_permute2f128_si256(_mm256_set1_epi32(0), val, 0x03), val, 8); }
BOOST_FORCEINLINE void M256i_ShiftRight9Byte(__m256i & val) { val = _mm256_alignr_epi8(_mm256_permute2f128_si256(_mm256_set1_epi32(0), val, 0x03), val, 9); }
BOOST_FORCEINLINE void M256i_ShiftRight10Byte(__m256i & val) { val = _mm256_alignr_epi8(_mm256_permute2f128_si256(_mm256_set1_epi32(0), val, 0x03), val, 10); }
BOOST_FORCEINLINE void M256i_ShiftRight11Byte(__m256i & val) { val = _mm256_alignr_epi8(_mm256_permute2f128_si256(_mm256_set1_epi32(0), val, 0x03), val, 11); }
BOOST_FORCEINLINE void M256i_ShiftRight12Byte(__m256i & val) { val = _mm256_alignr_epi8(_mm256_permute2f128_si256(_mm256_set1_epi32(0), val, 0x03), val, 12); }
BOOST_FORCEINLINE void M256i_ShiftRight13Byte(__m256i & val) { val = _mm256_alignr_epi8(_mm256_permute2f128_si256(_mm256_set1_epi32(0), val, 0x03), val, 13); }
BOOST_FORCEINLINE void M256i_ShiftRight14Byte(__m256i & val) { val = _mm256_alignr_epi8(_mm256_permute2f128_si256(_mm256_set1_epi32(0), val, 0x03), val, 14); }
BOOST_FORCEINLINE void M256i_ShiftRight15Byte(__m256i & val) { val = _mm256_alignr_epi8(_mm256_permute2f128_si256(_mm256_set1_epi32(0), val, 0x03), val, 15); }
BOOST_FORCEINLINE void M256i_ShiftRight16Byte(__m256i & val) { val = _mm256_alignr_epi8(_mm256_permute2f128_si256(_mm256_set1_epi32(0), val, 0x03), val, 16); }
BOOST_FORCEINLINE void M256i_ShiftRight17Byte(__m256i & val) { val = _mm256_alignr_epi8(_mm256_permute2f128_si256(_mm256_set1_epi32(0), val, 0x03), val, 17); }
BOOST_FORCEINLINE void M256i_ShiftRight18Byte(__m256i & val) { val = _mm256_alignr_epi8(_mm256_permute2f128_si256(_mm256_set1_epi32(0), val, 0x03), val, 18); }
BOOST_FORCEINLINE void M256i_ShiftRight19Byte(__m256i & val) { val = _mm256_alignr_epi8(_mm256_permute2f128_si256(_mm256_set1_epi32(0), val, 0x03), val, 19); }
BOOST_FORCEINLINE void M256i_ShiftRight20Byte(__m256i & val) { val = _mm256_alignr_epi8(_mm256_permute2f128_si256(_mm256_set1_epi32(0), val, 0x03), val, 20); }
BOOST_FORCEINLINE void M256i_ShiftRight21Byte(__m256i & val) { val = _mm256_alignr_epi8(_mm256_permute2f128_si256(_mm256_set1_epi32(0), val, 0x03), val, 21); }
BOOST_FORCEINLINE void M256i_ShiftRight22Byte(__m256i & val) { val = _mm256_alignr_epi8(_mm256_permute2f128_si256(_mm256_set1_epi32(0), val, 0x03), val, 22); }
BOOST_FORCEINLINE void M256i_ShiftRight23Byte(__m256i & val) { val = _mm256_alignr_epi8(_mm256_permute2f128_si256(_mm256_set1_epi32(0), val, 0x03), val, 23); }
BOOST_FORCEINLINE void M256i_ShiftRight24Byte(__m256i & val) { val = _mm256_alignr_epi8(_mm256_permute2f128_si256(_mm256_set1_epi32(0), val, 0x03), val, 24); }
BOOST_FORCEINLINE void M256i_ShiftRight25Byte(__m256i & val) { val = _mm256_alignr_epi8(_mm256_permute2f128_si256(_mm256_set1_epi32(0), val, 0x03), val, 25); }
BOOST_FORCEINLINE void M256i_ShiftRight26Byte(__m256i & val) { val = _mm256_alignr_epi8(_mm256_permute2f128_si256(_mm256_set1_epi32(0), val, 0x03), val, 26); }
BOOST_FORCEINLINE void M256i_ShiftRight27Byte(__m256i & val) { val = _mm256_alignr_epi8(_mm256_permute2f128_si256(_mm256_set1_epi32(0), val, 0x03), val, 27); }
BOOST_FORCEINLINE void M256i_ShiftRight28Byte(__m256i & val) { val = _mm256_alignr_epi8(_mm256_permute2f128_si256(_mm256_set1_epi32(0), val, 0x03), val, 28); }
BOOST_FORCEINLINE void M256i_ShiftRight29Byte(__m256i & val) { val = _mm256_alignr_epi8(_mm256_permute2f128_si256(_mm256_set1_epi32(0), val, 0x03), val, 29); }
BOOST_FORCEINLINE void M256i_ShiftRight30Byte(__m256i & val) { val = _mm256_alignr_epi8(_mm256_permute2f128_si256(_mm256_set1_epi32(0), val, 0x03), val, 30); }
BOOST_FORCEINLINE void M256i_ShiftRight31Byte(__m256i & val) { val = _mm256_alignr_epi8(_mm256_permute2f128_si256(_mm256_set1_epi32(0), val, 0x03), val, 31); }
BOOST_FORCEINLINE void M256i_ShiftRight32Byte(__m256i & val) { val = _mm256_alignr_epi8(_mm256_permute2f128_si256(_mm256_set1_epi32(0), val, 0x03), val, 32); }

static const m256_shift_func_t M256i_ShiftRightByteFunctions[33] = {
    &M256i_ShiftRight0Byte,
    &M256i_ShiftRight1Byte,
    &M256i_ShiftRight2Byte,
    &M256i_ShiftRight3Byte,
    &M256i_ShiftRight4Byte,
    &M256i_ShiftRight5Byte,
    &M256i_ShiftRight6Byte,
    &M256i_ShiftRight7Byte,
    &M256i_ShiftRight8Byte,
    &M256i_ShiftRight9Byte,
    &M256i_ShiftRight10Byte,
    &M256i_ShiftRight11Byte,
    &M256i_ShiftRight12Byte,
    &M256i_ShiftRight13Byte,
    &M256i_ShiftRight14Byte,
    &M256i_ShiftRight15Byte,
    &M256i_ShiftRight16Byte,
    &M256i_ShiftRight17Byte,
    &M256i_ShiftRight18Byte,
    &M256i_ShiftRight19Byte,
    &M256i_ShiftRight20Byte,
    &M256i_ShiftRight21Byte,
    &M256i_ShiftRight22Byte,
    &M256i_ShiftRight23Byte,
    &M256i_ShiftRight24Byte,
    &M256i_ShiftRight25Byte,
    &M256i_ShiftRight26Byte,
    &M256i_ShiftRight27Byte,
    &M256i_ShiftRight28Byte,
    &M256i_ShiftRight29Byte,
    &M256i_ShiftRight30Byte,
    &M256i_ShiftRight31Byte,
    &M256i_ShiftRight32Byte
};

#endif // SIMD_BYTESHIFT_USE_FUNCTION_ARRAY

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
        const unsigned int sdnvSizeBytes = boost::multiprecision::detail::find_lsb<int>(~significantBitsSetMask) + 1;
        if (sdnvSizeBytes > bytesRemainingIn128Buffer) {
            //std::cout << "breaking..\n";
            break;
        }
        else if (sdnvSizeBytes > 8) { //((significantBitsSetMask & 0xff) == 0x00ff) {
            //std::cout << "  notice: SdnvDecodeU64Fast encountered an encoded SDNV greater than 8 bytes.. using classic version" << std::endl;
            uint8_t thisSdnvBytesDecoded;
            *decodedValues++ = SdnvDecodeU64Classic((uint8_t*)&sdnvsEncoded, &thisSdnvBytesDecoded);
        }
        else {
            
            const uint64_t encoded64 = _mm_cvtsi128_si64(sdnvsEncoded); //SSE2 Copy the lower 64-bit integer in a to dst.
            
            
            const uint64_t u64ByteSwapped = boost::endian::big_to_native(encoded64);
            const uint64_t decoded = _pext_u64(u64ByteSwapped, masks[sdnvSizeBytes]);
            *decodedValues++ = decoded;
            //std::cout << "  sdnvSizeBytes = " << sdnvSizeBytes << std::endl;
            //std::cout << "  decoded = " << decoded << std::endl;
        }
#ifdef SIMD_BYTESHIFT_USE_FUNCTION_ARRAY
        (*M128i_ShiftRightByteFunctions[sdnvSizeBytes])(sdnvsEncoded);
        
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
        const unsigned int sdnvSizeBytes = boost::multiprecision::detail::find_lsb<int>(~significantBitsSetMask) + 1;
        if (sdnvSizeBytes > bytesRemainingIn256Buffer) {
            //std::cout << "breaking..\n";
            break;
        }
        else if (sdnvSizeBytes > 8) { //((significantBitsSetMask & 0xff) == 0x00ff) {
            //std::cout << "  notice: SdnvDecodeU64Fast encountered an encoded SDNV greater than 8 bytes.. using classic version" << std::endl;
            uint8_t thisSdnvBytesDecoded;
            *decodedValues++ = SdnvDecodeU64Classic((uint8_t*)&sdnvsEncoded, &thisSdnvBytesDecoded);
        }
        else {

            const uint64_t encoded64 = _mm_cvtsi128_si64(_mm256_castsi256_si128(sdnvsEncoded)); //SSE2 Copy the lower 64-bit integer in a to dst.
            //std::cout << "  encoded64lower: " << std::hex << encoded64 << std::dec << std::endl;
            //{
            //    std::cout << "  " << std::setfill('0') << std::setw(16) << std::hex << sdnvsEncoded.m256i_u64[3] 
            //        << " " << std::setfill('0') << std::setw(16) << std::hex << sdnvsEncoded.m256i_u64[2] 
            //        << " " << std::setfill('0') << std::setw(16) << std::hex << sdnvsEncoded.m256i_u64[1] 
            //        << " " << std::setfill('0') << std::setw(16) << std::hex << sdnvsEncoded.m256i_u64[0] << std::dec << std::endl;
            //}
            const uint64_t u64ByteSwapped = boost::endian::big_to_native(encoded64);
            const uint64_t decoded = _pext_u64(u64ByteSwapped, masks[sdnvSizeBytes]);
            *decodedValues++ = decoded;
            //std::cout << "  sdnvSizeBytes = " << sdnvSizeBytes << std::endl;
            //std::cout << "  decoded = " << decoded << std::endl;
        }
#ifdef SIMD_BYTESHIFT_USE_FUNCTION_ARRAY
        (*M256i_ShiftRightByteFunctions[sdnvSizeBytes])(sdnvsEncoded);
        
#else
        //this switch statement will be optimized by the compiler into a jump table, and sdnvsEncoded and shiftIn will remain in
        // the SIMD registers as opposed to using the function array method resulting in expensive SIMD load operations
        const __m256i shiftIn = _mm256_permute2f128_si256(_mm256_set1_epi32(0), sdnvsEncoded, 0x03);
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
#endif
        bytesRemainingIn256Buffer -= sdnvSizeBytes;
        --decodedRemaining;
    }
    //std::cout << "significantBitsSetMask: " << significantBitsSetMask << " sdnvsize: " << sdnvSizeBytes << " u64ByteSwapped " << std::hex << u64ByteSwapped << " d " << std::dec << decoded << std::endl;
    *numBytes = static_cast<unsigned int>(sizeof(__m256i)) - bytesRemainingIn256Buffer;
    return decodedStart - decodedRemaining;
}
