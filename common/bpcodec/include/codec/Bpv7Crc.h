/**
 * @file Bpv7Crc.h
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
 *
 * @section DESCRIPTION
 *
 * The Bpv7Crc.h file contains software methods for computing
 * the CRC16 for Bundle Protocol Version 7 and
 * contains both software and hardware methods for computing
 * the CRC32C for Bundle Protocol Version 7.
 */

#ifndef BPV7_CRC_H
#define BPV7_CRC_H
#include <cstdint>
#include <cstddef>
#include "bpcodec_export.h"
class Bpv7Crc {
private:
    Bpv7Crc();
    ~Bpv7Crc();

public:
#ifdef USE_CRC32C_FAST
    BPCODEC_EXPORT static uint32_t Crc32C_Unaligned_Hardware(const uint8_t* dataUnaligned, std::size_t length);
#endif
    BPCODEC_EXPORT static uint32_t Crc32C_Unaligned_Software(const uint8_t* dataUnaligned, std::size_t length);
    BPCODEC_EXPORT static uint32_t Crc32C_Unaligned(const uint8_t* dataUnaligned, std::size_t length);
    BPCODEC_EXPORT static uint16_t Crc16_X25_Unaligned(const uint8_t* dataUnaligned, std::size_t length);

    BPCODEC_EXPORT static uint64_t SerializeCrc16ForBpv7(uint8_t * serialization, const uint16_t crc16);
    BPCODEC_EXPORT static uint64_t SerializeCrc32ForBpv7(uint8_t * serialization, const uint32_t crc32);
    BPCODEC_EXPORT static uint64_t SerializeZeroedCrc16ForBpv7(uint8_t * serialization);
    BPCODEC_EXPORT static uint64_t SerializeZeroedCrc32ForBpv7(uint8_t * serialization);
    BPCODEC_EXPORT static bool DeserializeCrc16ForBpv7(const uint8_t * serialization, uint8_t * numBytesTakenToDecode, uint16_t & crc16);
    BPCODEC_EXPORT static bool DeserializeCrc32ForBpv7(const uint8_t * serialization, uint8_t * numBytesTakenToDecode, uint32_t & crc32);
};
#endif //BPV7_CRC_H
