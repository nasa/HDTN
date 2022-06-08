/**
 * @file Telemetry.h
 * @author  Blake LaFuente
 *
 * @copyright Copyright � 2021 United States Government as represented by
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

struct IngressTelemetry_t{
    uint64_t type = 1;
    double bundleDataRate;
    double averageDataRate;
    double totalData;
    uint64_t bundleCountEgress;
    uint64_t bundleCountStorage;

    TELEMETRY_DEFINITIONS_EXPORT void ToLittleEndianInplace();
    TELEMETRY_DEFINITIONS_EXPORT void ToNativeEndianInplace();
};

struct EgressTelemetry_t{
    uint64_t type = 2;
    uint64_t egressBundleCount;
    double egressBundleData;
    uint64_t egressMessageCount;

    TELEMETRY_DEFINITIONS_EXPORT void ToLittleEndianInplace();
    TELEMETRY_DEFINITIONS_EXPORT void ToNativeEndianInplace();

};

struct StorageTelemetry_t{
    uint64_t type = 3;
    uint64_t totalBundlesErasedFromStorage;
    uint64_t totalBundlesSentToEgressFromStorage;

    TELEMETRY_DEFINITIONS_EXPORT void ToLittleEndianInplace();
    TELEMETRY_DEFINITIONS_EXPORT void ToNativeEndianInplace();
};

#endif // HDTN_TELEMETRY_H
