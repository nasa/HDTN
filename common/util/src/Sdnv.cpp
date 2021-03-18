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
    const unsigned int sdnvSizeBytes = mask0x80Index + 1;
    //std::cout << "encode fast: msb: " << msb << std::endl;
    const uint64_t encoded64 = _pdep_u64(valToEncodeU64, masks[sdnvSizeBytes]) | masks0x80[mask0x80Index];
    //std::cout << "encoded64: " << std::hex << encoded64  << std::dec << std::endl;
    const uint64_t encoded64ForMemcpy = boost::endian::big_to_native(encoded64);
    const __m128i forStore =_mm_set_epi64x(0, encoded64ForMemcpy); //put encoded64ForMemcpy in element 0
    _mm_storeu_si64(outputEncoded, forStore);//SSE Store 64-bit integer from the first element of a into memory. mem_addr does not need to be aligned on any particular boundary.
    return sdnvSizeBytes;
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
