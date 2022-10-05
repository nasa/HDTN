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
#include <map>
#include "telemetry_definitions_export.h"

struct IngressTelemetry_t{
    TELEMETRY_DEFINITIONS_EXPORT IngressTelemetry_t();

    uint64_t type;
    double bundleDataRate;
    double averageDataRate;
    double totalData;
    uint64_t bundleCountEgress;
    uint64_t bundleCountStorage;

    TELEMETRY_DEFINITIONS_EXPORT uint64_t SerializeToLittleEndian(uint8_t * data, uint64_t bufferSize) const;
};

struct EgressTelemetry_t{
    TELEMETRY_DEFINITIONS_EXPORT EgressTelemetry_t();

    uint64_t type;
    uint64_t egressBundleCount;
    double egressBundleData;
    uint64_t egressMessageCount;

    TELEMETRY_DEFINITIONS_EXPORT uint64_t SerializeToLittleEndian(uint8_t* data, uint64_t bufferSize) const;

};

struct StorageTelemetry_t{
    TELEMETRY_DEFINITIONS_EXPORT StorageTelemetry_t();

    uint64_t type;
    uint64_t totalBundlesErasedFromStorage;
    uint64_t totalBundlesSentToEgressFromStorage;

    TELEMETRY_DEFINITIONS_EXPORT uint64_t SerializeToLittleEndian(uint8_t* data, uint64_t bufferSize) const;
};

struct StorageTelemetryRequest_t {
    TELEMETRY_DEFINITIONS_EXPORT StorageTelemetryRequest_t();

    uint64_t type; //must be 10
    uint64_t priority; //0, 1, or 2
    uint64_t thresholdSecondsFromNow;


    TELEMETRY_DEFINITIONS_EXPORT uint64_t SerializeToLittleEndian(uint8_t* data, uint64_t bufferSize) const;
};

struct StorageExpiringBeforeThresholdTelemetry_t {
    TELEMETRY_DEFINITIONS_EXPORT StorageExpiringBeforeThresholdTelemetry_t();

    uint64_t type;
    uint64_t priority;
    uint64_t thresholdSecondsSinceStartOfYear2000;
    typedef std::pair<uint64_t, uint64_t> bundle_count_plus_bundle_bytes_pair_t;
    std::map<uint64_t, bundle_count_plus_bundle_bytes_pair_t> map_node_id_to_expiring_before_threshold_count;

    TELEMETRY_DEFINITIONS_EXPORT uint64_t SerializeToLittleEndian(uint8_t* data, uint64_t bufferSize) const;
};

struct OutductTelemetry_t {
    TELEMETRY_DEFINITIONS_EXPORT OutductTelemetry_t();

    uint64_t type;
    uint64_t convergenceLayerType;
    uint64_t totalBundlesAcked;
    uint64_t totalBundleBytesAcked;
    uint64_t totalBundlesSent;
    uint64_t totalBundleBytesSent;
    uint64_t totalBundlesFailedToSend;
    

    TELEMETRY_DEFINITIONS_EXPORT uint64_t GetTotalBundlesQueued() const;
    TELEMETRY_DEFINITIONS_EXPORT uint64_t GetTotalBundleBytesQueued() const;
};

struct StcpOutductTelemetry_t : public OutductTelemetry_t {
    TELEMETRY_DEFINITIONS_EXPORT StcpOutductTelemetry_t();

    uint64_t totalStcpBytesSent;

    TELEMETRY_DEFINITIONS_EXPORT uint64_t SerializeToLittleEndian(uint8_t* data, uint64_t bufferSize) const;
};

struct LtpOutductTelemetry_t : public OutductTelemetry_t {
    TELEMETRY_DEFINITIONS_EXPORT LtpOutductTelemetry_t();

    //ltp engine session sender stats
    uint64_t numCheckpointsExpired;
    uint64_t numDiscretionaryCheckpointsNotResent;

    //ltp udp engine
    uint64_t countUdpPacketsSent;
    uint64_t countRxUdpCircularBufferOverruns;

    uint64_t countTxUdpPacketsLimitedByRate;

    TELEMETRY_DEFINITIONS_EXPORT uint64_t SerializeToLittleEndian(uint8_t* data, uint64_t bufferSize) const;
};

TELEMETRY_DEFINITIONS_EXPORT bool PrintSerializedTelemetry(const uint8_t* serialized, uint64_t size);

#endif // HDTN_TELEMETRY_H
