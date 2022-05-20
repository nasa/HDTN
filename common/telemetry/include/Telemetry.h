/**
 * @file Telemetry.h
 * @author  Blake LaFuente
 *
 * @copyright Copyright © 2021 United States Government as represented by
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
 * 
 */

#ifndef HDTN_TELEMETRY_H
#define HDTN_TELEMETRY_H 1

#include <cstdint>
#include "telemetry_definitions_export.h"

struct IngressTelemetry_t {
    uint64_t data1;

    TELEMETRY_DEFINITIONS_EXPORT void ToLittleEndianInplace();
    TELEMETRY_DEFINITIONS_EXPORT void ToNativeEndianInplace();
};

#endif // HDTN_TELEMETRY_H
