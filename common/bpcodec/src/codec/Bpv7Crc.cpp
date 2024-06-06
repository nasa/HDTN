/**
 * @file Bpv7Crc.cpp
 * @author  Brian Tomko <brian.j.tomko@nasa.gov>
 *
 * @copyright Copyright (c) 2021 United States Government as represented by
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
#include <boost/make_unique.hpp>
#include <map>
typedef boost::crc_optimal<32, 0x1EDC6F41, UINT32_MAX, UINT32_MAX, true, true> boost_crc32c_t;

#ifdef USE_CRC32C_FAST
# ifdef HAVE_SSE2NEON_H
#include "sse2neon.h"
# else
#include <nmmintrin.h>
# endif
#include <boost/config/detail/suffix.hpp>
#endif

//use the algorithm located in ZlibCrc32cCombine.cpp
#define USE_ZLIB_CRC_COMBINE_ALGORITHM 1

#define CRC32C_INIT UINT32_MAX

static BOOST_FORCEINLINE uint32_t Crc32C_Finalize(uint32_t crc) {
    //return static_cast<uint32_t>(crc ^ UINT32_MAX); //final xor value
    return ~crc; //final xor value
}

#ifdef USE_CRC32C_FAST
static uint32_t Crc32C_Unaligned_Hardware_AddBytes(uint32_t crc, const uint8_t* dataUnaligned, std::size_t length) {
    while (length && (((std::uintptr_t)dataUnaligned) & 7)) { //while not aligned on 8-byte boundary
        crc = _mm_crc32_u8(crc, *dataUnaligned++);
        --length;
    }
    //data now aligned on 8-byte boundary
    while (length > 7) { //while length >= 8
        const uint64_t* const dataAligned64 = reinterpret_cast<const uint64_t*>(dataUnaligned);
        crc = static_cast<uint32_t>(_mm_crc32_u64(crc, *dataAligned64));
        dataUnaligned += sizeof(uint64_t);
        length -= sizeof(uint64_t);
    }
    //finish any remaining bytes <= 7
    while (length) {
        crc = _mm_crc32_u8(crc, *dataUnaligned++);
        --length;
    }
    return crc;
}

#ifndef USE_ZLIB_CRC_COMBINE_ALGORITHM
static uint32_t Crc32C_Unaligned_Hardware_AddZeros(uint32_t crc, std::size_t length) {
    while (length > 7) { //while length >= 8
        crc = static_cast<uint32_t>(_mm_crc32_u64(crc, 0));
        length -= sizeof(uint64_t);
    }
    //finish any remaining bytes <= 7
    while (length) {
        crc = _mm_crc32_u8(crc, 0);
        --length;
    }
    return crc;
}
#endif

//idea from https://github.com/komrad36/CRC
uint32_t Bpv7Crc::Crc32C_Unaligned_Hardware(const uint8_t* dataUnaligned, std::size_t length) {
    //https://datatracker.ietf.org/doc/html/rfc4960#appendix-B
    /*The CRC remainder register is initialized with all 1s and the CRC
      is computed with an algorithm that simultaneously multiplies by
      x^32 and divides by the CRC polynomial.*/
    uint32_t crc = CRC32C_INIT;

    crc = Crc32C_Unaligned_Hardware_AddBytes(crc, dataUnaligned, length);

    return Crc32C_Finalize(crc);
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
    boost_crc32c_t crc;
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


#ifdef USE_CRC32C_FAST
class Crc32c_InOrderChunks::Impl {
public:
    Impl() : m_crc(0) {
        Reset();
    }
    ~Impl() {}
    void Reset() {
        m_crc = CRC32C_INIT;
    }
    void AddUnalignedBytes(const uint8_t* dataUnaligned, std::size_t length) {
        m_crc = Crc32C_Unaligned_Hardware_AddBytes(m_crc, dataUnaligned, length);
    }
    uint32_t FinalizeAndGet() const {
        return Crc32C_Finalize(m_crc);
    }
private:
    uint32_t m_crc;
};
#else
class Crc32c_InOrderChunks::Impl {
public:
    Impl() {
        Reset();
    }
    ~Impl() {}
    void Reset() {
        m_crc.reset();
    }
    void AddUnalignedBytes(const uint8_t* dataUnaligned, std::size_t length) {
        m_crc.process_bytes(dataUnaligned, length);
    }
    uint32_t FinalizeAndGet() const {
        return m_crc();
    }
private:
    boost_crc32c_t m_crc;
};
#endif

Crc32c_InOrderChunks::Crc32c_InOrderChunks() :
    m_pimpl(boost::make_unique<Crc32c_InOrderChunks::Impl>()) {}

Crc32c_InOrderChunks::~Crc32c_InOrderChunks() {}

void Crc32c_InOrderChunks::Reset() {
    m_pimpl->Reset();
}

void Crc32c_InOrderChunks::AddUnalignedBytes(const uint8_t* dataUnaligned, std::size_t length) {
    m_pimpl->AddUnalignedBytes(dataUnaligned, length);
}

uint32_t Crc32c_InOrderChunks::FinalizeAndGet() const {
    return m_pimpl->FinalizeAndGet();
}


class Crc32c_ReceiveOutOfOrderChunks::Impl {
public:
    struct CrcAndLength {
        uint32_t crc;
        size_t length;
    };
    Impl() {
        Reset();
    }
    ~Impl() {}
    void Reset() {
        m_mapOffsetToCrcAndLength.clear();
    }
    bool AddUnalignedBytes(const uint8_t* dataUnaligned, std::size_t offset, std::size_t length);
    uint32_t FinalizeAndGet() const;
private:
    typedef std::map<std::size_t, CrcAndLength> map_offset_to_crc_and_length_t;
    map_offset_to_crc_and_length_t m_mapOffsetToCrcAndLength;
};

Crc32c_ReceiveOutOfOrderChunks::Crc32c_ReceiveOutOfOrderChunks() :
    m_pimpl(boost::make_unique<Crc32c_ReceiveOutOfOrderChunks::Impl>()) {}

Crc32c_ReceiveOutOfOrderChunks::~Crc32c_ReceiveOutOfOrderChunks() {}

void Crc32c_ReceiveOutOfOrderChunks::Reset() {
    m_pimpl->Reset();
}

bool Crc32c_ReceiveOutOfOrderChunks::AddUnalignedBytes(const uint8_t* dataUnaligned, std::size_t offset, std::size_t length) {
    return m_pimpl->AddUnalignedBytes(dataUnaligned, offset, length);
}


uint32_t Crc32c_ReceiveOutOfOrderChunks::FinalizeAndGet() const {
    return m_pimpl->FinalizeAndGet();
}



//https://stackoverflow.com/questions/23122312/crc-calculation-of-a-mostly-static-data-stream/23126768#23126768
/*
Trick #1: CRCs are linear. So if you have stream X and stream Y of the same length
and exclusive-or the two streams bit-by-bit to get Z, i.e. Z = X ^ Y (using the C notation for exclusive-or),
then CRC(Z) = CRC(X) ^ CRC(Y).
For the problem at hand we have two streams A and B of differing length that we want to concatenate into stream Z.
What we have available are CRC(A) and CRC(B). What we want is a quick way to compute CRC(Z).
The trick is to construct X = A concatenated with length(B) zero bits, and Y = length(A) zero bits concatenated with B.
So if we represent concatenation simply by juxtaposition of the symbols,
X = A0, Y = 0B, then X^Y = Z = AB. Then we have CRC(Z) = CRC(A0) ^ CRC(0B).

Now we need to know CRC(A0) and CRC(0B). CRC(0B) is easy.
If we feed a bunch of zeros to the CRC machine starting with zero,
the register is still filled with zeros. So it's as if we did nothing at all.
Therefore CRC(0B) = CRC(B).

CRC(A0) requires more work however.
Taking a non-zero CRC and feeding zeros to the CRC machine doesn't leave it alone.
Every zero changes the register contents. So to get CRC(A0),
we need to set the register to CRC(A), and then run length(B) zeros through it.
Then we can exclusive-or the result of that with CRC(B) = CRC(0B),
and we get what we want, which is CRC(Z) = CRC(AB). Voila!
*/
uint32_t Crc32c_ReceiveOutOfOrderChunks::Impl::FinalizeAndGet() const {
    if (m_mapOffsetToCrcAndLength.size() == 0) {
        return CRC32C_INIT;
    }
    map_offset_to_crc_and_length_t::const_iterator it = m_mapOffsetToCrcAndLength.cbegin();
    uint32_t crc = it->second.crc;
    size_t offsetA = it->first;
    size_t lengthA = it->second.length;
    ++it;
    for (; it != m_mapOffsetToCrcAndLength.cend(); ++it) {
        const size_t offsetB = it->first;
        if ((offsetB - offsetA) != lengthA) {
            return 0; //invalid
        }
        const uint32_t crcA = crc; // i-1;
        const uint32_t crc0B = it->second.crc; // CRC(0B) = CRC(B).
        const std::size_t lengthB = it->second.length;

#ifdef USE_ZLIB_CRC_COMBINE_ALGORITHM
        crc = Bpv7Crc::Crc32cCombine(crcA, crc0B, lengthB);
#else
# ifdef USE_CRC32C_FAST
        const uint32_t crcA0 = Crc32C_Unaligned_Hardware_AddZeros(crcA, lengthB);
# else
        boost_crc32c_t crcSoft(boost::detail::reflect_unsigned<uint32_t>(crcA));
        for (std::size_t z = 0; z < lengthB; ++z) {
            crcSoft.process_byte(0);
        }
        const uint32_t crcA0 = ~crcSoft();
# endif
        crc = crcA0 ^ crc0B;
#endif
        offsetA = it->first;
        lengthA = it->second.length;

    }
    return Crc32C_Finalize(crc);
}
bool Crc32c_ReceiveOutOfOrderChunks::Impl::AddUnalignedBytes(const uint8_t* dataUnaligned, std::size_t offset, std::size_t length) {
    CrcAndLength crcAndLength;
#ifdef USE_CRC32C_FAST
    crcAndLength.crc = Crc32C_Unaligned_Hardware_AddBytes((offset == 0) ? CRC32C_INIT : 0, dataUnaligned, length);
#else
    boost_crc32c_t crcSoft((offset == 0) ? CRC32C_INIT : 0); //reflect identical to constants (not needed)
    crcSoft.process_bytes(dataUnaligned, length);
    crcAndLength.crc = ~crcSoft();
#endif
    crcAndLength.length = length;
    return m_mapOffsetToCrcAndLength.emplace(offset, crcAndLength).second;
}
