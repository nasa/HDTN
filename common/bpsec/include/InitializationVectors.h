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
#include "bpsec_export.h"

struct InitializationVector12Byte {
    BPSEC_EXPORT InitializationVector12Byte();
    BPSEC_EXPORT ~InitializationVector12Byte();
    BPSEC_EXPORT void Serialize(void* data) const;
    BPSEC_EXPORT void Increment();
    
    uint64_t m_timePart;
    uint32_t m_counterPart;
};

struct InitializationVector16Byte {
    BPSEC_EXPORT InitializationVector16Byte();
    BPSEC_EXPORT ~InitializationVector16Byte();
    BPSEC_EXPORT void Serialize(void* data) const;
    BPSEC_EXPORT void Increment();

    uint64_t m_timePart;
    uint64_t m_counterPart;
};


#endif // INITIALIZATION_VECTORS_H

