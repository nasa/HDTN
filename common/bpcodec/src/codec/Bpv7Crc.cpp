/**
 * @file Bpv7Crc.cpp
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

#include "codec/Bpv7Crc.h"
#include <boost/crc.hpp>
#ifdef USE_CRC32C_FAST
#include <nmmintrin.h>
#include <boost/endian/conversion.hpp>
//#include <iostream>

//idea from https://github.com/komrad36/CRC
uint32_t Bpv7Crc::Crc32C_Unaligned_Hardware(const uint8_t* dataUnaligned, std::size_t length) {
    //https://datatracker.ietf.org/doc/html/rfc4960#appendix-B
    /*The CRC remainder register is initialized with all 1s and the CRC
      is computed with an algorithm that simultaneously multiplies by
      x^32 and divides by the CRC polynomial.*/
    uint64_t crc = UINT32_MAX;

    while (length && (((std::uintptr_t)dataUnaligned) & 7)) { //while not aligned on 8-byte boundary
        crc = _mm_crc32_u8(static_cast<uint32_t>(crc), *dataUnaligned++);
        --length;
    }
    //data now aligned on 8-byte boundary
    while (length > 7) { //while length >= 8
        const uint64_t * const dataAligned64 = reinterpret_cast<const uint64_t *>(dataUnaligned);
        crc = _mm_crc32_u64(crc, *dataAligned64);
        dataUnaligned += sizeof(uint64_t);
        length -= sizeof(uint64_t);
    }
    //finish any remaining bytes <= 7
    while (length) {
        crc = _mm_crc32_u8(static_cast<uint32_t>(crc), *dataUnaligned++);
        --length;
    }

    return static_cast<uint32_t>(crc ^ UINT32_MAX); //final xor value
}
uint32_t Bpv7Crc::Crc32C_Unaligned(const uint8_t* dataUnaligned, std::size_t length) {
    return Bpv7Crc::Crc32C_Unaligned_Hardware(dataUnaligned, length);
}
#else
uint32_t Bpv7Crc::Crc32C_Unaligned(const uint8_t* dataUnaligned, std::size_t length) {
    return Bpv7Crc::Crc32C_Unaligned_Software(dataUnaligned, length);
}
#endif

//https://stackoverflow.com/a/16623151
uint32_t Bpv7Crc::Crc32C_Unaligned_Software(const uint8_t* dataUnaligned, std::size_t length) {
    boost::crc_optimal<32, 0x1EDC6F41, UINT32_MAX, UINT32_MAX, true, true> crc;
    crc.process_bytes(dataUnaligned, length);
    return crc();
}

uint16_t Bpv7Crc::Crc16_X25_Unaligned(const uint8_t* dataUnaligned, std::size_t length) {
    boost::crc_optimal<16, 0x1021, UINT16_MAX, UINT16_MAX, true, true> crc;
    crc.process_bytes(dataUnaligned, length);
    return crc();
}



//4.2.2. CRC
//CRC SHALL be omitted from a block if and only if the block's CRC
//type code is zero.
//When not omitted, the CRC SHALL be represented as a CBOR byte string
//of two bytes (that is, CBOR additional information 2, if CRC type is
//1) or of four bytes (that is, CBOR additional information 4, if CRC
//type is 2); in each case the sequence of bytes SHALL constitute an
//unsigned integer value (of 16 or 32 bits, respectively) in network
//byte order.
uint64_t Bpv7Crc::SerializeCrc16ForBpv7(uint8_t * serialization, const uint16_t crc16) {
    *serialization++ = (2U << 5) | 2U; //major type 2, additional information 2 (byte string of length 2)
    *serialization++ = static_cast<uint8_t>(crc16 >> 8); //msb first
    *serialization = static_cast<uint8_t>(crc16); //lsb last
    return 3;
}
uint64_t Bpv7Crc::SerializeCrc32ForBpv7(uint8_t * serialization, const uint32_t crc32) {
    *serialization++ = (2U << 5) | 4U; //major type 2, additional information 4 (byte string of length 4)
    *serialization++ = static_cast<uint8_t>(crc32 >> 24); //msb first
    *serialization++ = static_cast<uint8_t>(crc32 >> 16);
    *serialization++ = static_cast<uint8_t>(crc32 >> 8);
    *serialization = static_cast<uint8_t>(crc32); //lsb last
    return 5;
}
uint64_t Bpv7Crc::SerializeZeroedCrc16ForBpv7(uint8_t * serialization) {
    *serialization++ = (2U << 5) | 2U; //major type 2, additional information 2 (byte string of length 2)
    *serialization++ = 0;
    *serialization = 0;
    return 3;
}
uint64_t Bpv7Crc::SerializeZeroedCrc32ForBpv7(uint8_t * serialization) {
    *serialization++ = (2U << 5) | 4U; //major type 2, additional information 4 (byte string of length 4)
    *serialization++ = 0;
    *serialization++ = 0;
    *serialization++ = 0;
    *serialization = 0;
    return 5;
}
bool Bpv7Crc::DeserializeCrc16ForBpv7(const uint8_t * serialization, uint8_t * numBytesTakenToDecode, uint16_t & crc16) {
    const uint8_t initialCborByte = *serialization++;
    if (initialCborByte != ((2U << 5) | 2U)) { //major type 2, additional information 2 (byte string of length 2)
        return false;
    }
    crc16 = *serialization++; //msb first
    crc16 <<= 8;
    crc16 |= *serialization; //lsb last

    *numBytesTakenToDecode = 3;
    return true;
}
bool Bpv7Crc::DeserializeCrc32ForBpv7(const uint8_t * serialization, uint8_t * numBytesTakenToDecode, uint32_t & crc32) {
    const uint8_t initialCborByte = *serialization++;
    if (initialCborByte != ((2U << 5) | 4U)) { //major type 2, additional information 4 (byte string of length 4)
        return false;
    }
    crc32 = *serialization++; //msb first
    crc32 <<= 8;
    crc32 |= *serialization++;
    crc32 <<= 8;
    crc32 |= *serialization++;
    crc32 <<= 8;
    crc32 |= *serialization; //lsb last

    *numBytesTakenToDecode = 5;
    return true;
}
