/**
 * @file InitializationVectors.h
 * @author  Brian Tomko <brian.j.tomko@nasa.gov>
 *
 * @copyright Copyright  2021 United States Government as represented by
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
 * This BPSecManager generates initialization vectors for
 * the AES-GCM cipher per RFC9173 section 4.3.1.
 *
 */


#ifndef INITIALIZATION_VECTORS_H
#define INITIALIZATION_VECTORS_H 1

#include <cstdint>
#include <vector>
#include "bpsec_export.h"

struct InitializationVector12Byte {
    BPSEC_EXPORT InitializationVector12Byte();
    BPSEC_EXPORT InitializationVector12Byte(const uint64_t timePart);
    BPSEC_EXPORT ~InitializationVector12Byte();
    BPSEC_EXPORT void Serialize(void* data) const;
    BPSEC_EXPORT void Increment();
    
    uint64_t m_timePart;
    uint32_t m_counterPart;
};

struct InitializationVector16Byte {
    BPSEC_EXPORT InitializationVector16Byte();
    BPSEC_EXPORT InitializationVector16Byte(const uint64_t timePart);
    BPSEC_EXPORT ~InitializationVector16Byte();
    BPSEC_EXPORT void Serialize(void* data) const;
    BPSEC_EXPORT void Increment();

    uint64_t m_timePart;
    uint64_t m_counterPart;
};

struct InitializationVectorsForOneThread {
private:
    InitializationVectorsForOneThread() = delete;
    InitializationVectorsForOneThread(const uint64_t timePart);
public:
    
    //12-bit iv's guaranteed to be unique from other iv's within hdtn as long as
    //a single induct thread isn't a security source for more than (100000 * 2^32) bundles before restarting HDTN.
    //16-bit iv's always guaranteed unique (100000 * 2^64) bundles before restarting HDTN..
    static constexpr uint64_t MIN_DIFF_MICROSECONDS = 100000;//100ms

    BPSEC_EXPORT static InitializationVectorsForOneThread Create();
    BPSEC_EXPORT void SerializeAndIncrement(const bool use12ByteIv);

    InitializationVector12Byte m_iv12;
    InitializationVector16Byte m_iv16;
    std::vector<uint8_t> m_initializationVector;
};


#endif // INITIALIZATION_VECTORS_H

