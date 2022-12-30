/**
 * @file TelemetryDefinitions.h
 * @author  Blake LaFuente
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
 *
 * @section DESCRIPTION
 *
 * 
 */

#ifndef HDTN_TELEMETRY_H
#define HDTN_TELEMETRY_H 1

#include <cstdint>
#include <map>
#include <list>
#include <vector>

#include <zmq.hpp>

#include "telemetry_definitions_export.h"
#include "codec/Cbhe.h"

enum TelemetryType {
    ingress = 1,
    egress,
    storage,
    ltpoutduct,
    stcpoutduct,
};

class Telemetry_t {
    public:
        TELEMETRY_DEFINITIONS_EXPORT Telemetry_t();
        TELEMETRY_DEFINITIONS_EXPORT virtual ~Telemetry_t();

        /**
         * Gets the underlying telemetry type
         */
        TELEMETRY_DEFINITIONS_EXPORT TelemetryType GetType();

        /**
         *  Gets the size, in bytes, to allocate for the serialized telemetry
         */
        TELEMETRY_DEFINITIONS_EXPORT uint64_t GetSerializationSize();

        /**
         * Serializes a telemetry object into little-endian format
         * @param buffer the buffer to place the serialized result
         * @param bufferSize the size of the provided buffer
         * @return the number of bytes serialized
         */
        TELEMETRY_DEFINITIONS_EXPORT virtual uint64_t SerializeToLittleEndian(uint8_t* buffer, uint64_t bufferSize);

        /**
         * Deserializes a little-endian uint8_t buffer into the telemetry object. By default,
         * this function will deserialize all fields that were appended to m_fieldsToSerialize.
         * @return the number of bytes that were deserialized
         */
        TELEMETRY_DEFINITIONS_EXPORT virtual uint64_t DeserializeFromLittleEndian(uint8_t* buffer, uint64_t bufferSize);

    protected:
        uint64_t m_type;
        std::vector<uint64_t*> m_fieldsToSerialize;
};

struct IngressTelemetry_t : public Telemetry_t {
    TELEMETRY_DEFINITIONS_EXPORT IngressTelemetry_t();
    TELEMETRY_DEFINITIONS_EXPORT virtual ~IngressTelemetry_t() override;

    uint64_t totalDataBytes;
    uint64_t bundleCountEgress;
    uint64_t bundleCountStorage;
};

struct EgressTelemetry_t : public Telemetry_t
{
    TELEMETRY_DEFINITIONS_EXPORT EgressTelemetry_t();
    TELEMETRY_DEFINITIONS_EXPORT virtual ~EgressTelemetry_t() override;

    uint64_t egressBundleCount;
    uint64_t totalDataBytes;
    uint64_t egressMessageCount;
};

struct StorageTelemetry_t : public Telemetry_t
{
    TELEMETRY_DEFINITIONS_EXPORT StorageTelemetry_t();
    TELEMETRY_DEFINITIONS_EXPORT virtual ~StorageTelemetry_t() override;

    uint64_t totalBundlesErasedFromStorage;
    uint64_t totalBundlesSentToEgressFromStorage;
    uint64_t usedSpaceBytes;
    uint64_t freeSpaceBytes;
};

struct OutductTelemetry_t : public Telemetry_t
{
    TELEMETRY_DEFINITIONS_EXPORT OutductTelemetry_t();
    TELEMETRY_DEFINITIONS_EXPORT virtual ~OutductTelemetry_t() override;

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
    TELEMETRY_DEFINITIONS_EXPORT virtual ~StcpOutductTelemetry_t() override;

    uint64_t totalStcpBytesSent;
};

struct LtpOutductTelemetry_t : public OutductTelemetry_t {
    TELEMETRY_DEFINITIONS_EXPORT LtpOutductTelemetry_t();
    TELEMETRY_DEFINITIONS_EXPORT virtual ~LtpOutductTelemetry_t() override;

    //ltp engine session sender stats
    uint64_t numCheckpointsExpired;
    uint64_t numDiscretionaryCheckpointsNotResent;

    //ltp udp engine
    uint64_t countUdpPacketsSent;
    uint64_t countRxUdpCircularBufferOverruns;
    uint64_t countTxUdpPacketsLimitedByRate;
};

class TelemetryFactory {
    public:
        class TelemetryDeserializeUnknownTypeException : public std::exception {
            public:
                const char* what() {
                    return "failed to deserialize telemetry: unknown type";
                }
        };

        class TelemetryDeserializeInvalidFormatException : public std::exception {
            public:
                const char* what() {
                    return "failed to deserialize telemetry: invalid format";
                }
        };

        /**
         * Deserializes a buffer that contains one or more serialized telemetry objects.
         * After deserializing, GetType can be used to determine the underlying type of each
         * object.
         * @return a vector of Telemetry_t points
         * @throws TelemetryDeserializeUnknownTypeException
         */
        TELEMETRY_DEFINITIONS_EXPORT static std::vector<std::unique_ptr<Telemetry_t>>
            DeserializeFromLittleEndian(
                uint8_t* buffer,
                uint64_t bufferSize
            );
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

struct OutductCapabilityTelemetry_t {
    TELEMETRY_DEFINITIONS_EXPORT OutductCapabilityTelemetry_t();

    
    uint64_t type;
    uint64_t outductArrayIndex; //outductUuid
    uint64_t maxBundlesInPipeline;
    uint64_t maxBundleSizeBytesInPipeline;
    uint64_t nextHopNodeId;
    std::list<cbhe_eid_t> finalDestinationEidList;
    std::list<uint64_t> finalDestinationNodeIdList;

    TELEMETRY_DEFINITIONS_EXPORT OutductCapabilityTelemetry_t(const OutductCapabilityTelemetry_t& o); //a copy constructor: X(const X&)
    TELEMETRY_DEFINITIONS_EXPORT OutductCapabilityTelemetry_t(OutductCapabilityTelemetry_t&& o); //a move constructor: X(X&&)
    TELEMETRY_DEFINITIONS_EXPORT OutductCapabilityTelemetry_t& operator=(const OutductCapabilityTelemetry_t& o); //a copy assignment: operator=(const X&)
    TELEMETRY_DEFINITIONS_EXPORT OutductCapabilityTelemetry_t& operator=(OutductCapabilityTelemetry_t&& o); //a move assignment: operator=(X&&)
    TELEMETRY_DEFINITIONS_EXPORT uint64_t GetSerializationSize() const;
    TELEMETRY_DEFINITIONS_EXPORT uint64_t SerializeToLittleEndian(uint8_t* data, uint64_t bufferSize) const;
    TELEMETRY_DEFINITIONS_EXPORT bool DeserializeFromLittleEndian(const uint8_t* serialization, uint64_t& numBytesTakenToDecode, uint64_t bufferSize);
    TELEMETRY_DEFINITIONS_EXPORT bool operator==(const OutductCapabilityTelemetry_t& o) const; //operator ==
    TELEMETRY_DEFINITIONS_EXPORT bool operator!=(const OutductCapabilityTelemetry_t& o) const; //operator !=
    TELEMETRY_DEFINITIONS_EXPORT friend std::ostream& operator<<(std::ostream& os, const OutductCapabilityTelemetry_t& o);
};

struct AllOutductCapabilitiesTelemetry_t {
    TELEMETRY_DEFINITIONS_EXPORT AllOutductCapabilitiesTelemetry_t();

    uint64_t type;
    std::list<OutductCapabilityTelemetry_t> outductCapabilityTelemetryList;

    TELEMETRY_DEFINITIONS_EXPORT AllOutductCapabilitiesTelemetry_t(const AllOutductCapabilitiesTelemetry_t& o); //a copy constructor: X(const X&)
    TELEMETRY_DEFINITIONS_EXPORT AllOutductCapabilitiesTelemetry_t(AllOutductCapabilitiesTelemetry_t&& o); //a move constructor: X(X&&)
    TELEMETRY_DEFINITIONS_EXPORT AllOutductCapabilitiesTelemetry_t& operator=(const AllOutductCapabilitiesTelemetry_t& o); //a copy assignment: operator=(const X&)
    TELEMETRY_DEFINITIONS_EXPORT AllOutductCapabilitiesTelemetry_t& operator=(AllOutductCapabilitiesTelemetry_t&& o); //a move assignment: operator=(X&&)
    TELEMETRY_DEFINITIONS_EXPORT uint64_t GetSerializationSize() const;
    TELEMETRY_DEFINITIONS_EXPORT uint64_t SerializeToLittleEndian(uint8_t* data, uint64_t bufferSize) const;
    TELEMETRY_DEFINITIONS_EXPORT bool DeserializeFromLittleEndian(const uint8_t* serialization, uint64_t& numBytesTakenToDecode, uint64_t bufferSize);
    TELEMETRY_DEFINITIONS_EXPORT bool operator==(const AllOutductCapabilitiesTelemetry_t& o) const; //operator ==
    TELEMETRY_DEFINITIONS_EXPORT bool operator!=(const AllOutductCapabilitiesTelemetry_t& o) const; //operator !=
    TELEMETRY_DEFINITIONS_EXPORT friend std::ostream& operator<<(std::ostream& os, const AllOutductCapabilitiesTelemetry_t& o);
};

TELEMETRY_DEFINITIONS_EXPORT bool PrintSerializedTelemetry(const uint8_t* serialized, uint64_t size);

TELEMETRY_DEFINITIONS_EXPORT const uint8_t TELEM_REQ_MSG = 1;

#endif // HDTN_TELEMETRY_H
