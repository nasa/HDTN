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
#include "JsonSerializable.h"
#include "EnumAsFlagsMacro.h"

enum class TelemetryType : uint64_t {
    undefined = 0,
    unused1 = 1,
    unused2,
    unused3,
    unused4,
    unused5,
    unused6,
    unused7,
    unused8,
    unused9,
    storageExpiringBeforeThreshold = 10,
    outductCapability,
    allOutductCapability,
    none
};
//MAKE_ENUM_SUPPORT_FLAG_OPERATORS(TelemetryType);
MAKE_ENUM_SUPPORT_OSTREAM_OPERATOR(TelemetryType);

class Telemetry_t : public JsonSerializable {
    public:
        TELEMETRY_DEFINITIONS_EXPORT Telemetry_t();
        TELEMETRY_DEFINITIONS_EXPORT Telemetry_t(TelemetryType type);
        TELEMETRY_DEFINITIONS_EXPORT virtual ~Telemetry_t();

        TELEMETRY_DEFINITIONS_EXPORT bool operator==(const Telemetry_t& o) const; //operator ==
        TELEMETRY_DEFINITIONS_EXPORT bool operator!=(const Telemetry_t& o) const;

        /**
         * Gets the underlying telemetry type
         */
        TELEMETRY_DEFINITIONS_EXPORT TelemetryType GetType();

        /**
         *  Gets the size, in bytes, to allocate for the serialized telemetry
         */
        TELEMETRY_DEFINITIONS_EXPORT virtual uint64_t GetSerializationSize() const;

        /**
         * Serializes a telemetry object into little-endian format
         * @param buffer the buffer to place the serialized result
         * @param bufferSize the size of the provided buffer
         * @return the number of bytes serialized
         */
        TELEMETRY_DEFINITIONS_EXPORT virtual uint64_t SerializeToLittleEndian(uint8_t* buffer, uint64_t bufferSize) const;

        /**
         * Deserializes a little-endian uint8_t buffer into the telemetry object. By default,
         * this function will deserialize all fields that were appended to m_fieldsToSerialize.
         * @return the number of bytes that were deserialized
         */
        TELEMETRY_DEFINITIONS_EXPORT virtual bool DeserializeFromLittleEndian(const uint8_t* serialization, uint64_t& numBytesTakenToDecode, uint64_t bufferSize);

        TELEMETRY_DEFINITIONS_EXPORT virtual boost::property_tree::ptree GetNewPropertyTree() const override;
        TELEMETRY_DEFINITIONS_EXPORT virtual bool SetValuesFromPropertyTree(const boost::property_tree::ptree& pt) override;

    protected:
        TelemetryType m_type;
        std::vector<uint64_t*> m_fieldsToSerialize;
};

struct StorageTelemetry_t : public JsonSerializable
{
    TELEMETRY_DEFINITIONS_EXPORT StorageTelemetry_t();
    TELEMETRY_DEFINITIONS_EXPORT ~StorageTelemetry_t();
    TELEMETRY_DEFINITIONS_EXPORT bool operator==(const StorageTelemetry_t& o) const; //operator ==
    TELEMETRY_DEFINITIONS_EXPORT bool operator!=(const StorageTelemetry_t& o) const;

    TELEMETRY_DEFINITIONS_EXPORT virtual boost::property_tree::ptree GetNewPropertyTree() const override;
    TELEMETRY_DEFINITIONS_EXPORT virtual bool SetValuesFromPropertyTree(const boost::property_tree::ptree& pt) override;

    uint64_t m_timestampMilliseconds;

    //from ZmqStorageInterface
    uint64_t m_totalBundlesErasedFromStorageNoCustodyTransfer;
    uint64_t m_totalBundlesErasedFromStorageWithCustodyTransfer;
    uint64_t m_totalBundlesRewrittenToStorageFromFailedEgressSend;
    uint64_t m_totalBundlesSentToEgressFromStorageReadFromDisk;
    uint64_t m_totalBundleBytesSentToEgressFromStorageReadFromDisk;
    uint64_t m_totalBundlesSentToEgressFromStorageForwardCutThrough;
    uint64_t m_totalBundleBytesSentToEgressFromStorageForwardCutThrough;
    uint64_t m_numRfc5050CustodyTransfers;
    uint64_t m_numAcsCustodyTransfers;
    uint64_t m_numAcsPacketsReceived;
    
    //from BundleStorageCatalog
    uint64_t m_numBundlesOnDisk;
    uint64_t m_numBundleBytesOnDisk;
    uint64_t m_totalBundleWriteOperationsToDisk;
    uint64_t m_totalBundleByteWriteOperationsToDisk;
    uint64_t m_totalBundleEraseOperationsFromDisk;
    uint64_t m_totalBundleByteEraseOperationsFromDisk;

    //from BundleStorageManagerBase's MemoryManager
    uint64_t m_usedSpaceBytes;
    uint64_t m_freeSpaceBytes;
};


struct StorageExpiringBeforeThresholdTelemetry_t : public Telemetry_t {
    TELEMETRY_DEFINITIONS_EXPORT StorageExpiringBeforeThresholdTelemetry_t();
    TELEMETRY_DEFINITIONS_EXPORT virtual ~StorageExpiringBeforeThresholdTelemetry_t() override;
    TELEMETRY_DEFINITIONS_EXPORT bool operator==(const StorageExpiringBeforeThresholdTelemetry_t& o) const; //operator ==
    TELEMETRY_DEFINITIONS_EXPORT bool operator!=(const StorageExpiringBeforeThresholdTelemetry_t& o) const;

    TELEMETRY_DEFINITIONS_EXPORT virtual boost::property_tree::ptree GetNewPropertyTree() const override;
    TELEMETRY_DEFINITIONS_EXPORT virtual bool SetValuesFromPropertyTree(const boost::property_tree::ptree& pt) override;

    uint64_t priority;
    uint64_t thresholdSecondsSinceStartOfYear2000;
    typedef std::pair<uint64_t, uint64_t> bundle_count_plus_bundle_bytes_pair_t;
    std::map<uint64_t, bundle_count_plus_bundle_bytes_pair_t> mapNodeIdToExpiringBeforeThresholdCount;

    TELEMETRY_DEFINITIONS_EXPORT virtual uint64_t SerializeToLittleEndian(uint8_t* data, uint64_t bufferSize) const override;
    TELEMETRY_DEFINITIONS_EXPORT virtual uint64_t GetSerializationSize() const override;
};

class TelemetryFactory {
    public:
        class TelemetryDeserializeUnknownTypeException : public std::exception {
            public:
                const char* what() const noexcept override {
                    return "failed to deserialize telemetry: unknown type";
                }
        };

        class TelemetryDeserializeInvalidFormatException : public std::exception {
            public:
                const char* what() const noexcept override {
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
                const uint8_t* buffer,
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

struct OutductCapabilityTelemetry_t : public Telemetry_t {
    TELEMETRY_DEFINITIONS_EXPORT OutductCapabilityTelemetry_t();

    
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
    TELEMETRY_DEFINITIONS_EXPORT virtual uint64_t GetSerializationSize() const override;
    TELEMETRY_DEFINITIONS_EXPORT virtual uint64_t SerializeToLittleEndian(uint8_t* data, uint64_t bufferSize) const override;
    TELEMETRY_DEFINITIONS_EXPORT virtual bool DeserializeFromLittleEndian(const uint8_t* serialization, uint64_t& numBytesTakenToDecode, uint64_t bufferSize) override;
    TELEMETRY_DEFINITIONS_EXPORT bool operator==(const OutductCapabilityTelemetry_t& o) const; //operator ==
    TELEMETRY_DEFINITIONS_EXPORT bool operator!=(const OutductCapabilityTelemetry_t& o) const; //operator !=
    TELEMETRY_DEFINITIONS_EXPORT friend std::ostream& operator<<(std::ostream& os, const OutductCapabilityTelemetry_t& o);

    TELEMETRY_DEFINITIONS_EXPORT virtual boost::property_tree::ptree GetNewPropertyTree() const override;
    TELEMETRY_DEFINITIONS_EXPORT virtual bool SetValuesFromPropertyTree(const boost::property_tree::ptree& pt) override;
};

struct AllOutductCapabilitiesTelemetry_t : public Telemetry_t {
    TELEMETRY_DEFINITIONS_EXPORT AllOutductCapabilitiesTelemetry_t();

    std::list<OutductCapabilityTelemetry_t> outductCapabilityTelemetryList;

    TELEMETRY_DEFINITIONS_EXPORT AllOutductCapabilitiesTelemetry_t(const AllOutductCapabilitiesTelemetry_t& o); //a copy constructor: X(const X&)
    TELEMETRY_DEFINITIONS_EXPORT AllOutductCapabilitiesTelemetry_t(AllOutductCapabilitiesTelemetry_t&& o); //a move constructor: X(X&&)
    TELEMETRY_DEFINITIONS_EXPORT AllOutductCapabilitiesTelemetry_t& operator=(const AllOutductCapabilitiesTelemetry_t& o); //a copy assignment: operator=(const X&)
    TELEMETRY_DEFINITIONS_EXPORT AllOutductCapabilitiesTelemetry_t& operator=(AllOutductCapabilitiesTelemetry_t&& o); //a move assignment: operator=(X&&)
    TELEMETRY_DEFINITIONS_EXPORT virtual uint64_t GetSerializationSize() const override;
    TELEMETRY_DEFINITIONS_EXPORT virtual uint64_t SerializeToLittleEndian(uint8_t* data, uint64_t bufferSize) const override;
    TELEMETRY_DEFINITIONS_EXPORT virtual bool DeserializeFromLittleEndian(const uint8_t* serialization, uint64_t& numBytesTakenToDecode, uint64_t bufferSize) override;
    TELEMETRY_DEFINITIONS_EXPORT bool operator==(const AllOutductCapabilitiesTelemetry_t& o) const; //operator ==
    TELEMETRY_DEFINITIONS_EXPORT bool operator!=(const AllOutductCapabilitiesTelemetry_t& o) const; //operator !=
    TELEMETRY_DEFINITIONS_EXPORT friend std::ostream& operator<<(std::ostream& os, const AllOutductCapabilitiesTelemetry_t& o);

    TELEMETRY_DEFINITIONS_EXPORT virtual boost::property_tree::ptree GetNewPropertyTree() const override;
    TELEMETRY_DEFINITIONS_EXPORT virtual bool SetValuesFromPropertyTree(const boost::property_tree::ptree& pt) override;
};


struct InductConnectionTelemetry_t : public JsonSerializable {
    TELEMETRY_DEFINITIONS_EXPORT InductConnectionTelemetry_t();
    TELEMETRY_DEFINITIONS_EXPORT bool operator==(const InductConnectionTelemetry_t& o) const; //operator ==
    TELEMETRY_DEFINITIONS_EXPORT bool operator!=(const InductConnectionTelemetry_t& o) const;

    TELEMETRY_DEFINITIONS_EXPORT virtual boost::property_tree::ptree GetNewPropertyTree() const override;
    TELEMETRY_DEFINITIONS_EXPORT virtual bool SetValuesFromPropertyTree(const boost::property_tree::ptree& pt) override;

    std::string m_connectionName;
    std::string m_inputName;
    uint64_t m_totalBundlesReceived;
    uint64_t m_totalBundleBytesReceived;
};

struct InductTelemetry_t : public JsonSerializable {
    TELEMETRY_DEFINITIONS_EXPORT InductTelemetry_t();
    TELEMETRY_DEFINITIONS_EXPORT bool operator==(const InductTelemetry_t& o) const; //operator ==
    TELEMETRY_DEFINITIONS_EXPORT bool operator!=(const InductTelemetry_t& o) const;

    TELEMETRY_DEFINITIONS_EXPORT virtual boost::property_tree::ptree GetNewPropertyTree() const override;
    TELEMETRY_DEFINITIONS_EXPORT virtual bool SetValuesFromPropertyTree(const boost::property_tree::ptree& pt) override;

    std::string m_convergenceLayer;
    std::list<InductConnectionTelemetry_t> m_listInductConnections;
};

struct AllInductTelemetry_t : public JsonSerializable {
    TELEMETRY_DEFINITIONS_EXPORT AllInductTelemetry_t();
    TELEMETRY_DEFINITIONS_EXPORT bool operator==(const AllInductTelemetry_t& o) const; //operator ==
    TELEMETRY_DEFINITIONS_EXPORT bool operator!=(const AllInductTelemetry_t& o) const;

    TELEMETRY_DEFINITIONS_EXPORT virtual boost::property_tree::ptree GetNewPropertyTree() const override;
    TELEMETRY_DEFINITIONS_EXPORT virtual bool SetValuesFromPropertyTree(const boost::property_tree::ptree& pt) override;
    uint64_t m_timestampMilliseconds;
    //ingress specific
    uint64_t m_bundleCountEgress;
    uint64_t m_bundleCountStorage;
    uint64_t m_bundleByteCountEgress;
    uint64_t m_bundleByteCountStorage;
    //inducts specific
    std::list<InductTelemetry_t> m_listAllInducts;
};



struct OutductTelemetry_t : public JsonSerializable
{
    TELEMETRY_DEFINITIONS_EXPORT OutductTelemetry_t();
    TELEMETRY_DEFINITIONS_EXPORT virtual ~OutductTelemetry_t();
    TELEMETRY_DEFINITIONS_EXPORT virtual bool operator==(const OutductTelemetry_t& o) const; //operator ==
    TELEMETRY_DEFINITIONS_EXPORT virtual bool operator!=(const OutductTelemetry_t& o) const;

    TELEMETRY_DEFINITIONS_EXPORT virtual boost::property_tree::ptree GetNewPropertyTree() const override;
    TELEMETRY_DEFINITIONS_EXPORT virtual bool SetValuesFromPropertyTree(const boost::property_tree::ptree& pt) override;

    std::string m_convergenceLayer;
    uint64_t m_totalBundlesAcked;
    uint64_t m_totalBundleBytesAcked;
    uint64_t m_totalBundlesSent;
    uint64_t m_totalBundleBytesSent;
    uint64_t m_totalBundlesFailedToSend;


    TELEMETRY_DEFINITIONS_EXPORT uint64_t GetTotalBundlesQueued() const;
    TELEMETRY_DEFINITIONS_EXPORT uint64_t GetTotalBundleBytesQueued() const;
};

struct StcpOutductTelemetry_t : public OutductTelemetry_t {
    TELEMETRY_DEFINITIONS_EXPORT StcpOutductTelemetry_t();
    TELEMETRY_DEFINITIONS_EXPORT virtual ~StcpOutductTelemetry_t() override;
    TELEMETRY_DEFINITIONS_EXPORT virtual bool operator==(const OutductTelemetry_t& o) const override; //operator ==
    TELEMETRY_DEFINITIONS_EXPORT virtual bool operator!=(const OutductTelemetry_t& o) const override;

    TELEMETRY_DEFINITIONS_EXPORT virtual boost::property_tree::ptree GetNewPropertyTree() const override;
    TELEMETRY_DEFINITIONS_EXPORT virtual bool SetValuesFromPropertyTree(const boost::property_tree::ptree& pt) override;

    uint64_t m_totalStcpBytesSent;
};

struct LtpOutductTelemetry_t : public OutductTelemetry_t {
    TELEMETRY_DEFINITIONS_EXPORT LtpOutductTelemetry_t();
    TELEMETRY_DEFINITIONS_EXPORT virtual ~LtpOutductTelemetry_t() override;
    TELEMETRY_DEFINITIONS_EXPORT virtual bool operator==(const OutductTelemetry_t& o) const override; //operator ==
    TELEMETRY_DEFINITIONS_EXPORT virtual bool operator!=(const OutductTelemetry_t& o) const override;

    TELEMETRY_DEFINITIONS_EXPORT virtual boost::property_tree::ptree GetNewPropertyTree() const override;
    TELEMETRY_DEFINITIONS_EXPORT virtual bool SetValuesFromPropertyTree(const boost::property_tree::ptree& pt) override;

    //ltp engine session sender stats
    uint64_t m_numCheckpointsExpired;
    uint64_t m_numDiscretionaryCheckpointsNotResent;

    //ltp udp engine
    uint64_t m_countUdpPacketsSent;
    uint64_t m_countRxUdpCircularBufferOverruns;
    uint64_t m_countTxUdpPacketsLimitedByRate;
};

struct TcpclV3OutductTelemetry_t : public OutductTelemetry_t {
    TELEMETRY_DEFINITIONS_EXPORT TcpclV3OutductTelemetry_t();
    TELEMETRY_DEFINITIONS_EXPORT virtual ~TcpclV3OutductTelemetry_t() override;
    TELEMETRY_DEFINITIONS_EXPORT virtual bool operator==(const OutductTelemetry_t& o) const override; //operator ==
    TELEMETRY_DEFINITIONS_EXPORT virtual bool operator!=(const OutductTelemetry_t& o) const override;

    TELEMETRY_DEFINITIONS_EXPORT virtual boost::property_tree::ptree GetNewPropertyTree() const override;
    TELEMETRY_DEFINITIONS_EXPORT virtual bool SetValuesFromPropertyTree(const boost::property_tree::ptree& pt) override;

    uint64_t m_totalFragmentsAcked;
    uint64_t m_totalFragmentsSent;
    //bidirectionality (identical to InductConnectionTelemetry_t)
    uint64_t m_totalBundlesReceived;
    uint64_t m_totalBundleBytesReceived;
};

struct TcpclV4OutductTelemetry_t : public OutductTelemetry_t {
    TELEMETRY_DEFINITIONS_EXPORT TcpclV4OutductTelemetry_t();
    TELEMETRY_DEFINITIONS_EXPORT virtual ~TcpclV4OutductTelemetry_t() override;
    TELEMETRY_DEFINITIONS_EXPORT virtual bool operator==(const OutductTelemetry_t& o) const override; //operator ==
    TELEMETRY_DEFINITIONS_EXPORT virtual bool operator!=(const OutductTelemetry_t& o) const override;

    TELEMETRY_DEFINITIONS_EXPORT virtual boost::property_tree::ptree GetNewPropertyTree() const override;
    TELEMETRY_DEFINITIONS_EXPORT virtual bool SetValuesFromPropertyTree(const boost::property_tree::ptree& pt) override;

    uint64_t m_totalFragmentsAcked;
    uint64_t m_totalFragmentsSent;
    //bidirectionality (identical to InductConnectionTelemetry_t)
    uint64_t m_totalBundlesReceived;
    uint64_t m_totalBundleBytesReceived;
};

struct UdpOutductTelemetry_t : public OutductTelemetry_t {
    TELEMETRY_DEFINITIONS_EXPORT UdpOutductTelemetry_t();
    TELEMETRY_DEFINITIONS_EXPORT virtual ~UdpOutductTelemetry_t() override;
    TELEMETRY_DEFINITIONS_EXPORT virtual bool operator==(const OutductTelemetry_t& o) const override; //operator ==
    TELEMETRY_DEFINITIONS_EXPORT virtual bool operator!=(const OutductTelemetry_t& o) const override;

    TELEMETRY_DEFINITIONS_EXPORT virtual boost::property_tree::ptree GetNewPropertyTree() const override;
    TELEMETRY_DEFINITIONS_EXPORT virtual bool SetValuesFromPropertyTree(const boost::property_tree::ptree& pt) override;

    uint64_t m_totalPacketsSent;
    uint64_t m_totalPacketBytesSent;
    uint64_t m_totalPacketsDequeuedForSend;
    uint64_t m_totalPacketBytesDequeuedForSend;
    uint64_t m_totalPacketsLimitedByRate;
};

struct AllOutductTelemetry_t : public JsonSerializable {
    TELEMETRY_DEFINITIONS_EXPORT AllOutductTelemetry_t();
    TELEMETRY_DEFINITIONS_EXPORT bool operator==(const AllOutductTelemetry_t& o) const; //operator ==
    TELEMETRY_DEFINITIONS_EXPORT bool operator!=(const AllOutductTelemetry_t& o) const;

    TELEMETRY_DEFINITIONS_EXPORT virtual boost::property_tree::ptree GetNewPropertyTree() const override;
    TELEMETRY_DEFINITIONS_EXPORT virtual bool SetValuesFromPropertyTree(const boost::property_tree::ptree& pt) override;
    uint64_t m_timestampMilliseconds;
    uint64_t m_totalBundlesGivenToOutducts;
    uint64_t m_totalBundleBytesGivenToOutducts;
    uint64_t m_totalTcpclBundlesReceived;
    uint64_t m_totalTcpclBundleBytesReceived;
    uint64_t m_totalStorageToIngressOpportunisticBundles;
    uint64_t m_totalStorageToIngressOpportunisticBundleBytes;
    uint64_t m_totalBundlesSuccessfullySent;
    uint64_t m_totalBundleBytesSuccessfullySent;
    std::list<std::unique_ptr<OutductTelemetry_t> > m_listAllOutducts;
};

static const uint8_t TELEM_REQ_MSG = 1;

#endif // HDTN_TELEMETRY_H
