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


struct StorageTelemetry_t : public JsonSerializable {
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
    uint64_t m_totalBundlesErasedFromStorageBecauseExpired;
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


struct StorageExpiringBeforeThresholdTelemetry_t : public JsonSerializable {
    TELEMETRY_DEFINITIONS_EXPORT StorageExpiringBeforeThresholdTelemetry_t();
    TELEMETRY_DEFINITIONS_EXPORT ~StorageExpiringBeforeThresholdTelemetry_t();
    TELEMETRY_DEFINITIONS_EXPORT bool operator==(const StorageExpiringBeforeThresholdTelemetry_t& o) const; //operator ==
    TELEMETRY_DEFINITIONS_EXPORT bool operator!=(const StorageExpiringBeforeThresholdTelemetry_t& o) const;

    TELEMETRY_DEFINITIONS_EXPORT virtual boost::property_tree::ptree GetNewPropertyTree() const override;
    TELEMETRY_DEFINITIONS_EXPORT virtual bool SetValuesFromPropertyTree(const boost::property_tree::ptree& pt) override;

    uint64_t priority;
    uint64_t thresholdSecondsSinceStartOfYear2000;
    typedef std::pair<uint64_t, uint64_t> bundle_count_plus_bundle_bytes_pair_t;
    std::map<uint64_t, bundle_count_plus_bundle_bytes_pair_t> mapNodeIdToExpiringBeforeThresholdCount;
};

struct OutductCapabilityTelemetry_t : public JsonSerializable {
    TELEMETRY_DEFINITIONS_EXPORT OutductCapabilityTelemetry_t();

    
    uint64_t outductArrayIndex; //outductUuid
    uint64_t maxBundlesInPipeline;
    uint64_t maxBundleSizeBytesInPipeline;
    uint64_t nextHopNodeId;
    bool assumedInitiallyDown;
    std::list<cbhe_eid_t> finalDestinationEidList;
    std::list<uint64_t> finalDestinationNodeIdList;

    TELEMETRY_DEFINITIONS_EXPORT OutductCapabilityTelemetry_t(const OutductCapabilityTelemetry_t& o); //a copy constructor: X(const X&)
    TELEMETRY_DEFINITIONS_EXPORT OutductCapabilityTelemetry_t(OutductCapabilityTelemetry_t&& o) noexcept; //a move constructor: X(X&&)
    TELEMETRY_DEFINITIONS_EXPORT OutductCapabilityTelemetry_t& operator=(const OutductCapabilityTelemetry_t& o); //a copy assignment: operator=(const X&)
    TELEMETRY_DEFINITIONS_EXPORT OutductCapabilityTelemetry_t& operator=(OutductCapabilityTelemetry_t&& o) noexcept; //a move assignment: operator=(X&&)
    TELEMETRY_DEFINITIONS_EXPORT bool operator==(const OutductCapabilityTelemetry_t& o) const; //operator ==
    TELEMETRY_DEFINITIONS_EXPORT bool operator!=(const OutductCapabilityTelemetry_t& o) const; //operator !=

    TELEMETRY_DEFINITIONS_EXPORT virtual boost::property_tree::ptree GetNewPropertyTree() const override;
    TELEMETRY_DEFINITIONS_EXPORT virtual bool SetValuesFromPropertyTree(const boost::property_tree::ptree& pt) override;
};

struct AllOutductCapabilitiesTelemetry_t : public JsonSerializable {
    TELEMETRY_DEFINITIONS_EXPORT AllOutductCapabilitiesTelemetry_t();

    std::list<OutductCapabilityTelemetry_t> outductCapabilityTelemetryList;

    TELEMETRY_DEFINITIONS_EXPORT AllOutductCapabilitiesTelemetry_t(const AllOutductCapabilitiesTelemetry_t& o); //a copy constructor: X(const X&)
    TELEMETRY_DEFINITIONS_EXPORT AllOutductCapabilitiesTelemetry_t(AllOutductCapabilitiesTelemetry_t&& o) noexcept; //a move constructor: X(X&&)
    TELEMETRY_DEFINITIONS_EXPORT AllOutductCapabilitiesTelemetry_t& operator=(const AllOutductCapabilitiesTelemetry_t& o); //a copy assignment: operator=(const X&)
    TELEMETRY_DEFINITIONS_EXPORT AllOutductCapabilitiesTelemetry_t& operator=(AllOutductCapabilitiesTelemetry_t&& o) noexcept; //a move assignment: operator=(X&&)
    TELEMETRY_DEFINITIONS_EXPORT bool operator==(const AllOutductCapabilitiesTelemetry_t& o) const; //operator ==
    TELEMETRY_DEFINITIONS_EXPORT bool operator!=(const AllOutductCapabilitiesTelemetry_t& o) const; //operator !=

    TELEMETRY_DEFINITIONS_EXPORT virtual boost::property_tree::ptree GetNewPropertyTree() const override;
    TELEMETRY_DEFINITIONS_EXPORT virtual bool SetValuesFromPropertyTree(const boost::property_tree::ptree& pt) override;
};


struct InductConnectionTelemetry_t : public JsonSerializable {
    TELEMETRY_DEFINITIONS_EXPORT InductConnectionTelemetry_t();
    TELEMETRY_DEFINITIONS_EXPORT virtual ~InductConnectionTelemetry_t();
    TELEMETRY_DEFINITIONS_EXPORT virtual bool operator==(const InductConnectionTelemetry_t& o) const; //operator ==
    TELEMETRY_DEFINITIONS_EXPORT virtual bool operator!=(const InductConnectionTelemetry_t& o) const;

    TELEMETRY_DEFINITIONS_EXPORT virtual boost::property_tree::ptree GetNewPropertyTree() const override;
    TELEMETRY_DEFINITIONS_EXPORT virtual bool SetValuesFromPropertyTree(const boost::property_tree::ptree& pt) override;

    std::string m_connectionName;
    std::string m_inputName;
    uint64_t m_totalBundlesReceived;
    uint64_t m_totalBundleBytesReceived;
};

struct StcpInductConnectionTelemetry_t : public InductConnectionTelemetry_t {
    TELEMETRY_DEFINITIONS_EXPORT StcpInductConnectionTelemetry_t();
    TELEMETRY_DEFINITIONS_EXPORT virtual ~StcpInductConnectionTelemetry_t() override;
    TELEMETRY_DEFINITIONS_EXPORT virtual bool operator==(const InductConnectionTelemetry_t& o) const override; //operator ==
    TELEMETRY_DEFINITIONS_EXPORT virtual bool operator!=(const InductConnectionTelemetry_t& o) const override;

    TELEMETRY_DEFINITIONS_EXPORT virtual boost::property_tree::ptree GetNewPropertyTree() const override;
    TELEMETRY_DEFINITIONS_EXPORT virtual bool SetValuesFromPropertyTree(const boost::property_tree::ptree& pt) override;

    uint64_t m_totalStcpBytesReceived;
};

struct UdpInductConnectionTelemetry_t : public InductConnectionTelemetry_t {
    TELEMETRY_DEFINITIONS_EXPORT UdpInductConnectionTelemetry_t();
    TELEMETRY_DEFINITIONS_EXPORT virtual ~UdpInductConnectionTelemetry_t() override;
    TELEMETRY_DEFINITIONS_EXPORT virtual bool operator==(const InductConnectionTelemetry_t& o) const override; //operator ==
    TELEMETRY_DEFINITIONS_EXPORT virtual bool operator!=(const InductConnectionTelemetry_t& o) const override;

    TELEMETRY_DEFINITIONS_EXPORT virtual boost::property_tree::ptree GetNewPropertyTree() const override;
    TELEMETRY_DEFINITIONS_EXPORT virtual bool SetValuesFromPropertyTree(const boost::property_tree::ptree& pt) override;

    uint64_t m_countCircularBufferOverruns;
};

struct TcpclV3InductConnectionTelemetry_t : public InductConnectionTelemetry_t {
    TELEMETRY_DEFINITIONS_EXPORT TcpclV3InductConnectionTelemetry_t();
    TELEMETRY_DEFINITIONS_EXPORT virtual ~TcpclV3InductConnectionTelemetry_t() override;
    TELEMETRY_DEFINITIONS_EXPORT virtual bool operator==(const InductConnectionTelemetry_t& o) const override; //operator ==
    TELEMETRY_DEFINITIONS_EXPORT virtual bool operator!=(const InductConnectionTelemetry_t& o) const override;

    TELEMETRY_DEFINITIONS_EXPORT virtual boost::property_tree::ptree GetNewPropertyTree() const override;
    TELEMETRY_DEFINITIONS_EXPORT virtual bool SetValuesFromPropertyTree(const boost::property_tree::ptree& pt) override;

    uint64_t m_totalIncomingFragmentsAcked;
    uint64_t m_totalOutgoingFragmentsSent;
    //bidirectionality (identical to OutductTelemetry_t)
    uint64_t m_totalBundlesSentAndAcked;
    uint64_t m_totalBundleBytesSentAndAcked;
    uint64_t m_totalBundlesSent;
    uint64_t m_totalBundleBytesSent;
    uint64_t m_totalBundlesFailedToSend;
};

struct TcpclV4InductConnectionTelemetry_t : public InductConnectionTelemetry_t {
    TELEMETRY_DEFINITIONS_EXPORT TcpclV4InductConnectionTelemetry_t();
    TELEMETRY_DEFINITIONS_EXPORT virtual ~TcpclV4InductConnectionTelemetry_t() override;
    TELEMETRY_DEFINITIONS_EXPORT virtual bool operator==(const InductConnectionTelemetry_t& o) const override; //operator ==
    TELEMETRY_DEFINITIONS_EXPORT virtual bool operator!=(const InductConnectionTelemetry_t& o) const override;

    TELEMETRY_DEFINITIONS_EXPORT virtual boost::property_tree::ptree GetNewPropertyTree() const override;
    TELEMETRY_DEFINITIONS_EXPORT virtual bool SetValuesFromPropertyTree(const boost::property_tree::ptree& pt) override;

    uint64_t m_totalIncomingFragmentsAcked;
    uint64_t m_totalOutgoingFragmentsSent;
    //bidirectionality (identical to OutductTelemetry_t)
    uint64_t m_totalBundlesSentAndAcked;
    uint64_t m_totalBundleBytesSentAndAcked;
    uint64_t m_totalBundlesSent;
    uint64_t m_totalBundleBytesSent;
    uint64_t m_totalBundlesFailedToSend;
};

struct SlipOverUartInductConnectionTelemetry_t : public InductConnectionTelemetry_t {
    TELEMETRY_DEFINITIONS_EXPORT SlipOverUartInductConnectionTelemetry_t();
    TELEMETRY_DEFINITIONS_EXPORT virtual ~SlipOverUartInductConnectionTelemetry_t() override;
    TELEMETRY_DEFINITIONS_EXPORT virtual bool operator==(const InductConnectionTelemetry_t& o) const override; //operator ==
    TELEMETRY_DEFINITIONS_EXPORT virtual bool operator!=(const InductConnectionTelemetry_t& o) const override;

    TELEMETRY_DEFINITIONS_EXPORT virtual boost::property_tree::ptree GetNewPropertyTree() const override;
    TELEMETRY_DEFINITIONS_EXPORT virtual bool SetValuesFromPropertyTree(const boost::property_tree::ptree& pt) override;

    uint64_t m_totalSlipBytesSent;
    uint64_t m_totalSlipBytesReceived;
    uint64_t m_totalReceivedChunks;
    uint64_t m_largestReceivedBytesPerChunk;
    uint64_t m_averageReceivedBytesPerChunk;
    //bidirectionality (identical to OutductTelemetry_t)
    uint64_t m_totalBundlesSentAndAcked;
    uint64_t m_totalBundleBytesSentAndAcked;
    uint64_t m_totalBundlesSent;
    uint64_t m_totalBundleBytesSent;
    uint64_t m_totalBundlesFailedToSend;
};

struct LtpInductConnectionTelemetry_t : public InductConnectionTelemetry_t {
    TELEMETRY_DEFINITIONS_EXPORT LtpInductConnectionTelemetry_t();
    TELEMETRY_DEFINITIONS_EXPORT virtual ~LtpInductConnectionTelemetry_t() override;
    TELEMETRY_DEFINITIONS_EXPORT virtual bool operator==(const InductConnectionTelemetry_t& o) const override; //operator ==
    TELEMETRY_DEFINITIONS_EXPORT virtual bool operator!=(const InductConnectionTelemetry_t& o) const override;

    TELEMETRY_DEFINITIONS_EXPORT virtual boost::property_tree::ptree GetNewPropertyTree() const override;
    TELEMETRY_DEFINITIONS_EXPORT virtual bool SetValuesFromPropertyTree(const boost::property_tree::ptree& pt) override;

    //session receiver stats
    uint64_t m_numReportSegmentTimerExpiredCallbacks;
    uint64_t m_numReportSegmentsUnableToBeIssued;
    uint64_t m_numReportSegmentsTooLargeAndNeedingSplit;
    uint64_t m_numReportSegmentsCreatedViaSplit;
    uint64_t m_numGapsFilledByOutOfOrderDataSegments;
    uint64_t m_numDelayedFullyClaimedPrimaryReportSegmentsSent;
    uint64_t m_numDelayedFullyClaimedSecondaryReportSegmentsSent;
    uint64_t m_numDelayedPartiallyClaimedPrimaryReportSegmentsSent;
    uint64_t m_numDelayedPartiallyClaimedSecondaryReportSegmentsSent;
    uint64_t m_totalCancelSegmentsStarted;
    uint64_t m_totalCancelSegmentSendRetries;
    uint64_t m_totalCancelSegmentsFailedToSend;
    uint64_t m_totalCancelSegmentsAcknowledged;
    uint64_t m_numRxSessionsCancelledBySender;
    uint64_t m_numStagnantRxSessionsDeleted;

    //ltp udp engine
    uint64_t m_countUdpPacketsSent;
    uint64_t m_countRxUdpCircularBufferOverruns;
    uint64_t m_countTxUdpPacketsLimitedByRate;
    
};

struct InductTelemetry_t : public JsonSerializable {
    TELEMETRY_DEFINITIONS_EXPORT InductTelemetry_t();
    TELEMETRY_DEFINITIONS_EXPORT bool operator==(const InductTelemetry_t& o) const; //operator ==
    TELEMETRY_DEFINITIONS_EXPORT bool operator!=(const InductTelemetry_t& o) const;

    TELEMETRY_DEFINITIONS_EXPORT virtual boost::property_tree::ptree GetNewPropertyTree() const override;
    TELEMETRY_DEFINITIONS_EXPORT virtual bool SetValuesFromPropertyTree(const boost::property_tree::ptree& pt) override;

    std::string m_convergenceLayer;
    std::list<std::unique_ptr<InductConnectionTelemetry_t> > m_listInductConnections;
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
    bool m_linkIsUpPhysically;
    bool m_linkIsUpPerTimeSchedule;

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
    uint64_t m_numTcpReconnectAttempts;
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
    uint64_t m_numDeletedFullyClaimedPendingReports;
    uint64_t m_totalCancelSegmentsStarted;
    uint64_t m_totalCancelSegmentSendRetries;
    uint64_t m_totalCancelSegmentsFailedToSend;
    uint64_t m_totalCancelSegmentsAcknowledged;
    uint64_t m_totalPingsStarted;
    uint64_t m_totalPingRetries;
    uint64_t m_totalPingsFailedToSend;
    uint64_t m_totalPingsAcknowledged;
    uint64_t m_numTxSessionsReturnedToStorage;
    uint64_t m_numTxSessionsCancelledByReceiver;

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
    uint64_t m_numTcpReconnectAttempts;
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
    uint64_t m_numTcpReconnectAttempts;
};

struct SlipOverUartOutductTelemetry_t : public OutductTelemetry_t {
    TELEMETRY_DEFINITIONS_EXPORT SlipOverUartOutductTelemetry_t();
    TELEMETRY_DEFINITIONS_EXPORT virtual ~SlipOverUartOutductTelemetry_t() override;
    TELEMETRY_DEFINITIONS_EXPORT virtual bool operator==(const OutductTelemetry_t& o) const override; //operator ==
    TELEMETRY_DEFINITIONS_EXPORT virtual bool operator!=(const OutductTelemetry_t& o) const override;

    TELEMETRY_DEFINITIONS_EXPORT virtual boost::property_tree::ptree GetNewPropertyTree() const override;
    TELEMETRY_DEFINITIONS_EXPORT virtual bool SetValuesFromPropertyTree(const boost::property_tree::ptree& pt) override;

    uint64_t m_totalSlipBytesSent;
    uint64_t m_totalSlipBytesReceived;
    uint64_t m_totalReceivedChunks;
    uint64_t m_largestReceivedBytesPerChunk;
    uint64_t m_averageReceivedBytesPerChunk;
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

struct ApiCommand_t : public JsonSerializable {
    std::string m_apiCall;

    TELEMETRY_DEFINITIONS_EXPORT ApiCommand_t();
    TELEMETRY_DEFINITIONS_EXPORT virtual ~ApiCommand_t();

    TELEMETRY_DEFINITIONS_EXPORT bool operator==(const ApiCommand_t& o) const;
    TELEMETRY_DEFINITIONS_EXPORT bool operator!=(const ApiCommand_t& o) const;

    TELEMETRY_DEFINITIONS_EXPORT virtual boost::property_tree::ptree GetNewPropertyTree() const override;
    TELEMETRY_DEFINITIONS_EXPORT virtual bool SetValuesFromPropertyTree(const boost::property_tree::ptree& pt) override;

    TELEMETRY_DEFINITIONS_EXPORT static std::shared_ptr<ApiCommand_t> CreateFromJson(const std::string& jsonStr);
};

struct GetStorageApiCommand_t : public ApiCommand_t {
    TELEMETRY_DEFINITIONS_EXPORT GetStorageApiCommand_t();
    TELEMETRY_DEFINITIONS_EXPORT virtual ~GetStorageApiCommand_t() override;
    TELEMETRY_DEFINITIONS_EXPORT static const std::string name;
};

struct GetOutductsApiCommand_t : public ApiCommand_t {
    TELEMETRY_DEFINITIONS_EXPORT GetOutductsApiCommand_t();
    TELEMETRY_DEFINITIONS_EXPORT virtual ~GetOutductsApiCommand_t() override;
    TELEMETRY_DEFINITIONS_EXPORT static const std::string name;
};

struct GetOutductCapabilitiesApiCommand_t : public ApiCommand_t {
    TELEMETRY_DEFINITIONS_EXPORT GetOutductCapabilitiesApiCommand_t();
    TELEMETRY_DEFINITIONS_EXPORT virtual ~GetOutductCapabilitiesApiCommand_t() override;
    TELEMETRY_DEFINITIONS_EXPORT static const std::string name;
};

struct GetInductsApiCommand_t : public ApiCommand_t {
    TELEMETRY_DEFINITIONS_EXPORT GetInductsApiCommand_t();
    TELEMETRY_DEFINITIONS_EXPORT ~GetInductsApiCommand_t();
    TELEMETRY_DEFINITIONS_EXPORT static const std::string name;
};

struct PingApiCommand_t : public ApiCommand_t {
    uint64_t m_nodeId;
    uint64_t m_pingServiceNumber;
    uint64_t m_bpVersion;

    TELEMETRY_DEFINITIONS_EXPORT PingApiCommand_t();
    TELEMETRY_DEFINITIONS_EXPORT virtual ~PingApiCommand_t() override;

    TELEMETRY_DEFINITIONS_EXPORT bool operator==(const ApiCommand_t& o) const;
    TELEMETRY_DEFINITIONS_EXPORT bool operator!=(const ApiCommand_t& o) const;

    TELEMETRY_DEFINITIONS_EXPORT virtual boost::property_tree::ptree GetNewPropertyTree() const override;
    TELEMETRY_DEFINITIONS_EXPORT virtual bool SetValuesFromPropertyTree(const boost::property_tree::ptree& pt) override;
    TELEMETRY_DEFINITIONS_EXPORT static const std::string name;
};

struct UploadContactPlanApiCommand_t : public ApiCommand_t {
    std::string m_contactPlanJson;

    TELEMETRY_DEFINITIONS_EXPORT UploadContactPlanApiCommand_t();
    TELEMETRY_DEFINITIONS_EXPORT virtual ~UploadContactPlanApiCommand_t() override;

    TELEMETRY_DEFINITIONS_EXPORT bool operator==(const ApiCommand_t& o) const;
    TELEMETRY_DEFINITIONS_EXPORT bool operator!=(const ApiCommand_t& o) const;

    TELEMETRY_DEFINITIONS_EXPORT virtual boost::property_tree::ptree GetNewPropertyTree() const override;
    TELEMETRY_DEFINITIONS_EXPORT virtual bool SetValuesFromPropertyTree(const boost::property_tree::ptree& pt) override;

    TELEMETRY_DEFINITIONS_EXPORT static const std::string name;
};

struct GetExpiringStorageApiCommand_t : public ApiCommand_t {
    uint64_t m_priority;
    uint64_t m_thresholdSecondsFromNow;

    TELEMETRY_DEFINITIONS_EXPORT GetExpiringStorageApiCommand_t();
    TELEMETRY_DEFINITIONS_EXPORT virtual ~GetExpiringStorageApiCommand_t() override;

    TELEMETRY_DEFINITIONS_EXPORT bool operator==(const ApiCommand_t& o) const;
    TELEMETRY_DEFINITIONS_EXPORT bool operator!=(const ApiCommand_t& o) const;

    TELEMETRY_DEFINITIONS_EXPORT virtual boost::property_tree::ptree GetNewPropertyTree() const override;
    TELEMETRY_DEFINITIONS_EXPORT virtual bool SetValuesFromPropertyTree(const boost::property_tree::ptree& pt) override;

    TELEMETRY_DEFINITIONS_EXPORT static const std::string name;
};

struct UpdateBpSecApiCommand_t : public ApiCommand_t {
    std::string m_bpSecJson;

    TELEMETRY_DEFINITIONS_EXPORT UpdateBpSecApiCommand_t();
    TELEMETRY_DEFINITIONS_EXPORT virtual ~UpdateBpSecApiCommand_t() override;

    TELEMETRY_DEFINITIONS_EXPORT bool operator==(const ApiCommand_t& o) const;
    TELEMETRY_DEFINITIONS_EXPORT bool operator!=(const ApiCommand_t& o) const;

    TELEMETRY_DEFINITIONS_EXPORT virtual boost::property_tree::ptree GetNewPropertyTree() const override;
    TELEMETRY_DEFINITIONS_EXPORT virtual bool SetValuesFromPropertyTree(const boost::property_tree::ptree& pt) override;

    TELEMETRY_DEFINITIONS_EXPORT static const std::string name;
};

struct GetBpSecApiCommand_t : public ApiCommand_t {
    TELEMETRY_DEFINITIONS_EXPORT GetBpSecApiCommand_t();
    TELEMETRY_DEFINITIONS_EXPORT virtual ~GetBpSecApiCommand_t() override;

    TELEMETRY_DEFINITIONS_EXPORT static const std::string name;
};

struct SetMaxSendRateApiCommand_t : public ApiCommand_t {
    uint64_t m_rateBitsPerSec;
    uint64_t m_outduct;

    TELEMETRY_DEFINITIONS_EXPORT SetMaxSendRateApiCommand_t();

    TELEMETRY_DEFINITIONS_EXPORT bool operator==(const ApiCommand_t& o) const;
    TELEMETRY_DEFINITIONS_EXPORT bool operator!=(const ApiCommand_t& o) const;

    TELEMETRY_DEFINITIONS_EXPORT virtual boost::property_tree::ptree GetNewPropertyTree() const override;
    TELEMETRY_DEFINITIONS_EXPORT virtual bool SetValuesFromPropertyTree(const boost::property_tree::ptree& pt) override;

    TELEMETRY_DEFINITIONS_EXPORT static const std::string name;
};



struct GetHdtnConfigApiCommand_t : public ApiCommand_t {
    TELEMETRY_DEFINITIONS_EXPORT GetHdtnConfigApiCommand_t();
    TELEMETRY_DEFINITIONS_EXPORT virtual ~GetHdtnConfigApiCommand_t() override;
    TELEMETRY_DEFINITIONS_EXPORT static const std::string name;
};

struct ApiResp_t : public JsonSerializable {
    bool m_success;
    std::string m_message;

    TELEMETRY_DEFINITIONS_EXPORT ApiResp_t();
    TELEMETRY_DEFINITIONS_EXPORT virtual ~ApiResp_t();

    TELEMETRY_DEFINITIONS_EXPORT virtual boost::property_tree::ptree GetNewPropertyTree() const override;
    TELEMETRY_DEFINITIONS_EXPORT virtual bool SetValuesFromPropertyTree(const boost::property_tree::ptree& pt) override;
};

/**
 * Represents a ZMQ connection identifier. ZMQ identities are sent by router sockets
 * and used to keep track of and send responses to specific clients. ZMQ connection
 * IDs are always 5 bytes
 */
static constexpr uint8_t ZMQ_CONNECTION_ID_LEN = 5;

class ZmqConnectionId_t {
    public:
        TELEMETRY_DEFINITIONS_EXPORT ZmqConnectionId_t();

        // This constructor generates custom ZMQ connection IDs. It accepts a single byte (uint8_t)
        // and assigns it to the last byte of the ID, while prepending all other bytes with 0's.
        TELEMETRY_DEFINITIONS_EXPORT ZmqConnectionId_t(const uint8_t val);

        // Convert ZmqConnectionId to a zmq::message_t
        TELEMETRY_DEFINITIONS_EXPORT zmq::message_t Msg();

        // Compare two ZmqConnectionId objects
        TELEMETRY_DEFINITIONS_EXPORT bool operator==(const ZmqConnectionId_t& other) const;

        // Compare ZmqConnectionId object with a zmq message
        TELEMETRY_DEFINITIONS_EXPORT bool operator==(const zmq::message_t& msg) const;

    private:
        std::array<uint8_t, ZMQ_CONNECTION_ID_LEN> m_id;
};

/**
 * Custom ZMQ "connection identities". Used for when the telemetry module
 * or the GUI requests data from a module (vs. an external connection)
 */
static ZmqConnectionId_t TELEM_REQ_CONN_ID = ZmqConnectionId_t(1);
static ZmqConnectionId_t GUI_REQ_CONN_ID = ZmqConnectionId_t(2);

#endif // HDTN_TELEMETRY_H
