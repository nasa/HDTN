#include "codec/Bpv7Crc.h"
#include <boost/crc.hpp>
#ifdef USE_X86_HARDWARE_ACCELERATION
#include <nmmintrin.h>
//#include <iostream>

//idea from https://github.com/komrad36/CRC
uint32_t Bpv7Crc::Crc32C_Unaligned_Hardware(const uint8_t* dataUnaligned, std::size_t length) {
    //https://datatracker.ietf.org/doc/html/rfc4960#appendix-B
    /*The CRC remainder register is initialized with all 1s and the CRC
      is computed with an algorithm that simultaneously multiplies by
      x^32 and divides by the CRC polynomial.*/
    uint64_t crc = UINT32_MAX;

    while (length && (((std::uintptr_t)dataUnaligned) & 7)) { //while not aligned on 8-byte boundary
        //std::cout << "1";
        crc = _mm_crc32_u8(static_cast<uint32_t>(crc), *dataUnaligned++);
        --length;
    }
    //data now aligned on 8-byte boundary
    while (length > 7) { //while length >= 8
        //std::cout << "2";
        const uint64_t * const dataAligned64 = reinterpret_cast<const uint64_t * const>(dataUnaligned);
        crc = _mm_crc32_u64(crc, *dataAligned64);
        dataUnaligned += sizeof(uint64_t);
        length -= sizeof(uint64_t);
    }
    //finish any remaining bytes <= 7
    while (length) {
        //std::cout << "3";
        crc = _mm_crc32_u8(static_cast<uint32_t>(crc), *dataUnaligned++);
        --length;
    }
    //std::cout << "\n";

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
