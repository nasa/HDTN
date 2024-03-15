/**
 * @file TelemetryDefinitions.h
 * @author  Blake LaFuente
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

#ifndef CLASS_VISIBILITY_TELEMETRY_DEFINITIONS
#  ifdef _WIN32
#    define CLASS_VISIBILITY_TELEMETRY_DEFINITIONS
#  else
#    define CLASS_VISIBILITY_TELEMETRY_DEFINITIONS TELEMETRY_DEFINITIONS_EXPORT
#  endif
#endif

struct CLASS_VISIBILITY_TELEMETRY_DEFINITIONS StorageTelemetry_t : public JsonSerializable {
    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS StorageTelemetry_t();
    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS ~StorageTelemetry_t();
    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS bool operator==(const StorageTelemetry_t& o) const; //operator ==
    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS bool operator!=(const StorageTelemetry_t& o) const;

    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS virtual boost::property_tree::ptree GetNewPropertyTree() const override;
    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS virtual bool SetValuesFromPropertyTree(const boost::property_tree::ptree& pt) override;

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


struct CLASS_VISIBILITY_TELEMETRY_DEFINITIONS StorageExpiringBeforeThresholdTelemetry_t : public JsonSerializable {
    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS StorageExpiringBeforeThresholdTelemetry_t();
    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS ~StorageExpiringBeforeThresholdTelemetry_t();
    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS bool operator==(const StorageExpiringBeforeThresholdTelemetry_t& o) const; //operator ==
    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS bool operator!=(const StorageExpiringBeforeThresholdTelemetry_t& o) const;

    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS virtual boost::property_tree::ptree GetNewPropertyTree() const override;
    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS virtual bool SetValuesFromPropertyTree(const boost::property_tree::ptree& pt) override;

    uint64_t priority;
    uint64_t thresholdSecondsSinceStartOfYear2000;
    typedef std::pair<uint64_t, uint64_t> bundle_count_plus_bundle_bytes_pair_t;
    std::map<uint64_t, bundle_count_plus_bundle_bytes_pair_t> mapNodeIdToExpiringBeforeThresholdCount;
};

struct CLASS_VISIBILITY_TELEMETRY_DEFINITIONS OutductCapabilityTelemetry_t : public JsonSerializable {
    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS OutductCapabilityTelemetry_t();

    
    uint64_t outductArrayIndex; //outductUuid
    uint64_t maxBundlesInPipeline;
    uint64_t maxBundleSizeBytesInPipeline;
    uint64_t nextHopNodeId;
    bool assumedInitiallyDown;
    std::list<cbhe_eid_t> finalDestinationEidList;
    std::list<uint64_t> finalDestinationNodeIdList;

    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS OutductCapabilityTelemetry_t(const OutductCapabilityTelemetry_t& o); //a copy constructor: X(const X&)
    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS OutductCapabilityTelemetry_t(OutductCapabilityTelemetry_t&& o) noexcept; //a move constructor: X(X&&)
    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS OutductCapabilityTelemetry_t& operator=(const OutductCapabilityTelemetry_t& o); //a copy assignment: operator=(const X&)
    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS OutductCapabilityTelemetry_t& operator=(OutductCapabilityTelemetry_t&& o) noexcept; //a move assignment: operator=(X&&)
    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS bool operator==(const OutductCapabilityTelemetry_t& o) const; //operator ==
    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS bool operator!=(const OutductCapabilityTelemetry_t& o) const; //operator !=

    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS virtual boost::property_tree::ptree GetNewPropertyTree() const override;
    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS virtual bool SetValuesFromPropertyTree(const boost::property_tree::ptree& pt) override;
};

struct CLASS_VISIBILITY_TELEMETRY_DEFINITIONS AllOutductCapabilitiesTelemetry_t : public JsonSerializable {
    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS AllOutductCapabilitiesTelemetry_t();

    std::list<OutductCapabilityTelemetry_t> outductCapabilityTelemetryList;

    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS AllOutductCapabilitiesTelemetry_t(const AllOutductCapabilitiesTelemetry_t& o); //a copy constructor: X(const X&)
    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS AllOutductCapabilitiesTelemetry_t(AllOutductCapabilitiesTelemetry_t&& o) noexcept; //a move constructor: X(X&&)
    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS AllOutductCapabilitiesTelemetry_t& operator=(const AllOutductCapabilitiesTelemetry_t& o); //a copy assignment: operator=(const X&)
    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS AllOutductCapabilitiesTelemetry_t& operator=(AllOutductCapabilitiesTelemetry_t&& o) noexcept; //a move assignment: operator=(X&&)
    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS bool operator==(const AllOutductCapabilitiesTelemetry_t& o) const; //operator ==
    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS bool operator!=(const AllOutductCapabilitiesTelemetry_t& o) const; //operator !=

    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS virtual boost::property_tree::ptree GetNewPropertyTree() const override;
    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS virtual bool SetValuesFromPropertyTree(const boost::property_tree::ptree& pt) override;
};


struct CLASS_VISIBILITY_TELEMETRY_DEFINITIONS InductConnectionTelemetry_t : public JsonSerializable {
    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS InductConnectionTelemetry_t();
    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS virtual ~InductConnectionTelemetry_t();
    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS virtual bool operator==(const InductConnectionTelemetry_t& o) const; //operator ==
    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS virtual bool operator!=(const InductConnectionTelemetry_t& o) const;

    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS virtual boost::property_tree::ptree GetNewPropertyTree() const override;
    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS virtual bool SetValuesFromPropertyTree(const boost::property_tree::ptree& pt) override;

    std::string m_connectionName;
    std::string m_inputName;
    uint64_t m_totalBundlesReceived;
    uint64_t m_totalBundleBytesReceived;
};

struct CLASS_VISIBILITY_TELEMETRY_DEFINITIONS StcpInductConnectionTelemetry_t : public InductConnectionTelemetry_t {
    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS StcpInductConnectionTelemetry_t();
    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS virtual ~StcpInductConnectionTelemetry_t() override;
    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS virtual bool operator==(const InductConnectionTelemetry_t& o) const override; //operator ==
    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS virtual bool operator!=(const InductConnectionTelemetry_t& o) const override;

    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS virtual boost::property_tree::ptree GetNewPropertyTree() const override;
    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS virtual bool SetValuesFromPropertyTree(const boost::property_tree::ptree& pt) override;

    uint64_t m_totalStcpBytesReceived;
};

struct CLASS_VISIBILITY_TELEMETRY_DEFINITIONS UdpInductConnectionTelemetry_t : public InductConnectionTelemetry_t {
    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS UdpInductConnectionTelemetry_t();
    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS virtual ~UdpInductConnectionTelemetry_t() override;
    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS virtual bool operator==(const InductConnectionTelemetry_t& o) const override; //operator ==
    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS virtual bool operator!=(const InductConnectionTelemetry_t& o) const override;

    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS virtual boost::property_tree::ptree GetNewPropertyTree() const override;
    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS virtual bool SetValuesFromPropertyTree(const boost::property_tree::ptree& pt) override;

    uint64_t m_countCircularBufferOverruns;
};

struct CLASS_VISIBILITY_TELEMETRY_DEFINITIONS TcpclV3InductConnectionTelemetry_t : public InductConnectionTelemetry_t {
    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS TcpclV3InductConnectionTelemetry_t();
    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS virtual ~TcpclV3InductConnectionTelemetry_t() override;
    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS virtual bool operator==(const InductConnectionTelemetry_t& o) const override; //operator ==
    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS virtual bool operator!=(const InductConnectionTelemetry_t& o) const override;

    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS virtual boost::property_tree::ptree GetNewPropertyTree() const override;
    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS virtual bool SetValuesFromPropertyTree(const boost::property_tree::ptree& pt) override;

    uint64_t m_totalIncomingFragmentsAcked;
    uint64_t m_totalOutgoingFragmentsSent;
    //bidirectionality (identical to OutductTelemetry_t)
    uint64_t m_totalBundlesSentAndAcked;
    uint64_t m_totalBundleBytesSentAndAcked;
    uint64_t m_totalBundlesSent;
    uint64_t m_totalBundleBytesSent;
    uint64_t m_totalBundlesFailedToSend;
};

struct CLASS_VISIBILITY_TELEMETRY_DEFINITIONS TcpclV4InductConnectionTelemetry_t : public InductConnectionTelemetry_t {
    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS TcpclV4InductConnectionTelemetry_t();
    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS virtual ~TcpclV4InductConnectionTelemetry_t() override;
    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS virtual bool operator==(const InductConnectionTelemetry_t& o) const override; //operator ==
    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS virtual bool operator!=(const InductConnectionTelemetry_t& o) const override;

    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS virtual boost::property_tree::ptree GetNewPropertyTree() const override;
    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS virtual bool SetValuesFromPropertyTree(const boost::property_tree::ptree& pt) override;

    uint64_t m_totalIncomingFragmentsAcked;
    uint64_t m_totalOutgoingFragmentsSent;
    //bidirectionality (identical to OutductTelemetry_t)
    uint64_t m_totalBundlesSentAndAcked;
    uint64_t m_totalBundleBytesSentAndAcked;
    uint64_t m_totalBundlesSent;
    uint64_t m_totalBundleBytesSent;
    uint64_t m_totalBundlesFailedToSend;
};

struct CLASS_VISIBILITY_TELEMETRY_DEFINITIONS SlipOverUartInductConnectionTelemetry_t : public InductConnectionTelemetry_t {
    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS SlipOverUartInductConnectionTelemetry_t();
    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS virtual ~SlipOverUartInductConnectionTelemetry_t() override;
    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS virtual bool operator==(const InductConnectionTelemetry_t& o) const override; //operator ==
    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS virtual bool operator!=(const InductConnectionTelemetry_t& o) const override;

    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS virtual boost::property_tree::ptree GetNewPropertyTree() const override;
    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS virtual bool SetValuesFromPropertyTree(const boost::property_tree::ptree& pt) override;

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

struct CLASS_VISIBILITY_TELEMETRY_DEFINITIONS BpOverEncapLocalStreamInductConnectionTelemetry_t : public InductConnectionTelemetry_t {
    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS BpOverEncapLocalStreamInductConnectionTelemetry_t();
    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS virtual ~BpOverEncapLocalStreamInductConnectionTelemetry_t() override;
    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS virtual bool operator==(const InductConnectionTelemetry_t& o) const override; //operator ==
    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS virtual bool operator!=(const InductConnectionTelemetry_t& o) const override;

    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS virtual boost::property_tree::ptree GetNewPropertyTree() const override;
    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS virtual bool SetValuesFromPropertyTree(const boost::property_tree::ptree& pt) override;

    uint64_t m_totalEncapHeaderBytesSent;
    uint64_t m_totalEncapHeaderBytesReceived;
    uint64_t m_largestEncapHeaderSizeBytesReceived;
    uint64_t m_smallestEncapHeaderSizeBytesReceived;
    uint64_t m_averageEncapHeaderSizeBytesReceived;
    //bidirectionality (identical to OutductTelemetry_t)
    uint64_t m_totalBundlesSentAndAcked;
    uint64_t m_totalBundleBytesSentAndAcked;
    uint64_t m_totalBundlesSent;
    uint64_t m_totalBundleBytesSent;
    uint64_t m_totalBundlesFailedToSend;
};

struct CLASS_VISIBILITY_TELEMETRY_DEFINITIONS LtpInductConnectionTelemetry_t : public InductConnectionTelemetry_t {
    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS LtpInductConnectionTelemetry_t();
    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS virtual ~LtpInductConnectionTelemetry_t() override;
    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS virtual bool operator==(const InductConnectionTelemetry_t& o) const override; //operator ==
    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS virtual bool operator!=(const InductConnectionTelemetry_t& o) const override;

    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS virtual boost::property_tree::ptree GetNewPropertyTree() const override;
    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS virtual bool SetValuesFromPropertyTree(const boost::property_tree::ptree& pt) override;

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

struct CLASS_VISIBILITY_TELEMETRY_DEFINITIONS InductTelemetry_t : public JsonSerializable {
    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS InductTelemetry_t();
    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS bool operator==(const InductTelemetry_t& o) const; //operator ==
    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS bool operator!=(const InductTelemetry_t& o) const;

    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS virtual boost::property_tree::ptree GetNewPropertyTree() const override;
    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS virtual bool SetValuesFromPropertyTree(const boost::property_tree::ptree& pt) override;

    std::string m_convergenceLayer;
    std::list<std::unique_ptr<InductConnectionTelemetry_t> > m_listInductConnections;
};

struct CLASS_VISIBILITY_TELEMETRY_DEFINITIONS AllInductTelemetry_t : public JsonSerializable {
    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS AllInductTelemetry_t();
    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS bool operator==(const AllInductTelemetry_t& o) const; //operator ==
    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS bool operator!=(const AllInductTelemetry_t& o) const;

    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS virtual boost::property_tree::ptree GetNewPropertyTree() const override;
    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS virtual bool SetValuesFromPropertyTree(const boost::property_tree::ptree& pt) override;
    uint64_t m_timestampMilliseconds;
    //ingress specific
    uint64_t m_bundleCountEgress;
    uint64_t m_bundleCountStorage;
    uint64_t m_bundleByteCountEgress;
    uint64_t m_bundleByteCountStorage;
    //inducts specific
    std::list<InductTelemetry_t> m_listAllInducts;
};



struct CLASS_VISIBILITY_TELEMETRY_DEFINITIONS OutductTelemetry_t : public JsonSerializable
{
    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS OutductTelemetry_t();
    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS virtual ~OutductTelemetry_t();
    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS virtual bool operator==(const OutductTelemetry_t& o) const; //operator ==
    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS virtual bool operator!=(const OutductTelemetry_t& o) const;

    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS virtual boost::property_tree::ptree GetNewPropertyTree() const override;
    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS virtual bool SetValuesFromPropertyTree(const boost::property_tree::ptree& pt) override;

    std::string m_convergenceLayer;
    uint64_t m_totalBundlesAcked;
    uint64_t m_totalBundleBytesAcked;
    uint64_t m_totalBundlesSent;
    uint64_t m_totalBundleBytesSent;
    uint64_t m_totalBundlesFailedToSend;
    bool m_linkIsUpPhysically;
    bool m_linkIsUpPerTimeSchedule;

    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS uint64_t GetTotalBundlesQueued() const;
    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS uint64_t GetTotalBundleBytesQueued() const;
};

struct CLASS_VISIBILITY_TELEMETRY_DEFINITIONS StcpOutductTelemetry_t : public OutductTelemetry_t {
    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS StcpOutductTelemetry_t();
    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS virtual ~StcpOutductTelemetry_t() override;
    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS virtual bool operator==(const OutductTelemetry_t& o) const override; //operator ==
    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS virtual bool operator!=(const OutductTelemetry_t& o) const override;

    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS virtual boost::property_tree::ptree GetNewPropertyTree() const override;
    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS virtual bool SetValuesFromPropertyTree(const boost::property_tree::ptree& pt) override;

    uint64_t m_totalStcpBytesSent;
    uint64_t m_numTcpReconnectAttempts;
};

struct CLASS_VISIBILITY_TELEMETRY_DEFINITIONS LtpOutductTelemetry_t : public OutductTelemetry_t {
    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS LtpOutductTelemetry_t();
    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS virtual ~LtpOutductTelemetry_t() override;
    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS virtual bool operator==(const OutductTelemetry_t& o) const override; //operator ==
    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS virtual bool operator!=(const OutductTelemetry_t& o) const override;

    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS virtual boost::property_tree::ptree GetNewPropertyTree() const override;
    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS virtual bool SetValuesFromPropertyTree(const boost::property_tree::ptree& pt) override;

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

struct CLASS_VISIBILITY_TELEMETRY_DEFINITIONS TcpclV3OutductTelemetry_t : public OutductTelemetry_t {
    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS TcpclV3OutductTelemetry_t();
    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS virtual ~TcpclV3OutductTelemetry_t() override;
    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS virtual bool operator==(const OutductTelemetry_t& o) const override; //operator ==
    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS virtual bool operator!=(const OutductTelemetry_t& o) const override;

    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS virtual boost::property_tree::ptree GetNewPropertyTree() const override;
    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS virtual bool SetValuesFromPropertyTree(const boost::property_tree::ptree& pt) override;

    uint64_t m_totalFragmentsAcked;
    uint64_t m_totalFragmentsSent;
    //bidirectionality (identical to InductConnectionTelemetry_t)
    uint64_t m_totalBundlesReceived;
    uint64_t m_totalBundleBytesReceived;
    uint64_t m_numTcpReconnectAttempts;
};

struct CLASS_VISIBILITY_TELEMETRY_DEFINITIONS TcpclV4OutductTelemetry_t : public OutductTelemetry_t {
    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS TcpclV4OutductTelemetry_t();
    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS virtual ~TcpclV4OutductTelemetry_t() override;
    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS virtual bool operator==(const OutductTelemetry_t& o) const override; //operator ==
    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS virtual bool operator!=(const OutductTelemetry_t& o) const override;

    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS virtual boost::property_tree::ptree GetNewPropertyTree() const override;
    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS virtual bool SetValuesFromPropertyTree(const boost::property_tree::ptree& pt) override;

    uint64_t m_totalFragmentsAcked;
    uint64_t m_totalFragmentsSent;
    //bidirectionality (identical to InductConnectionTelemetry_t)
    uint64_t m_totalBundlesReceived;
    uint64_t m_totalBundleBytesReceived;
    uint64_t m_numTcpReconnectAttempts;
};

struct CLASS_VISIBILITY_TELEMETRY_DEFINITIONS SlipOverUartOutductTelemetry_t : public OutductTelemetry_t {
    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS SlipOverUartOutductTelemetry_t();
    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS virtual ~SlipOverUartOutductTelemetry_t() override;
    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS virtual bool operator==(const OutductTelemetry_t& o) const override; //operator ==
    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS virtual bool operator!=(const OutductTelemetry_t& o) const override;

    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS virtual boost::property_tree::ptree GetNewPropertyTree() const override;
    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS virtual bool SetValuesFromPropertyTree(const boost::property_tree::ptree& pt) override;

    uint64_t m_totalSlipBytesSent;
    uint64_t m_totalSlipBytesReceived;
    uint64_t m_totalReceivedChunks;
    uint64_t m_largestReceivedBytesPerChunk;
    uint64_t m_averageReceivedBytesPerChunk;
    //bidirectionality (identical to InductConnectionTelemetry_t)
    uint64_t m_totalBundlesReceived;
    uint64_t m_totalBundleBytesReceived;
};

struct CLASS_VISIBILITY_TELEMETRY_DEFINITIONS BpOverEncapLocalStreamOutductTelemetry_t : public OutductTelemetry_t {
    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS BpOverEncapLocalStreamOutductTelemetry_t();
    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS virtual ~BpOverEncapLocalStreamOutductTelemetry_t() override;
    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS virtual bool operator==(const OutductTelemetry_t& o) const override; //operator ==
    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS virtual bool operator!=(const OutductTelemetry_t& o) const override;

    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS virtual boost::property_tree::ptree GetNewPropertyTree() const override;
    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS virtual bool SetValuesFromPropertyTree(const boost::property_tree::ptree& pt) override;

    uint64_t m_totalEncapHeaderBytesSent;
    uint64_t m_totalEncapHeaderBytesReceived;
    uint64_t m_largestEncapHeaderSizeBytesSent;
    uint64_t m_smallestEncapHeaderSizeBytesSent;
    uint64_t m_averageEncapHeaderSizeBytesSent;
    //bidirectionality (identical to InductConnectionTelemetry_t)
    uint64_t m_totalBundlesReceived;
    uint64_t m_totalBundleBytesReceived;
};

struct CLASS_VISIBILITY_TELEMETRY_DEFINITIONS UdpOutductTelemetry_t : public OutductTelemetry_t {
    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS UdpOutductTelemetry_t();
    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS virtual ~UdpOutductTelemetry_t() override;
    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS virtual bool operator==(const OutductTelemetry_t& o) const override; //operator ==
    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS virtual bool operator!=(const OutductTelemetry_t& o) const override;

    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS virtual boost::property_tree::ptree GetNewPropertyTree() const override;
    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS virtual bool SetValuesFromPropertyTree(const boost::property_tree::ptree& pt) override;

    uint64_t m_totalPacketsSent;
    uint64_t m_totalPacketBytesSent;
    uint64_t m_totalPacketsDequeuedForSend;
    uint64_t m_totalPacketBytesDequeuedForSend;
    uint64_t m_totalPacketsLimitedByRate;
};

struct CLASS_VISIBILITY_TELEMETRY_DEFINITIONS AllOutductTelemetry_t : public JsonSerializable {
    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS AllOutductTelemetry_t();
    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS bool operator==(const AllOutductTelemetry_t& o) const; //operator ==
    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS bool operator!=(const AllOutductTelemetry_t& o) const;

    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS virtual boost::property_tree::ptree GetNewPropertyTree() const override;
    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS virtual bool SetValuesFromPropertyTree(const boost::property_tree::ptree& pt) override;
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

struct CLASS_VISIBILITY_TELEMETRY_DEFINITIONS ApiCommand_t : public JsonSerializable {
    std::string m_apiCall;

    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS ApiCommand_t();
    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS virtual ~ApiCommand_t();

    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS bool operator==(const ApiCommand_t& o) const;
    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS bool operator!=(const ApiCommand_t& o) const;

    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS virtual boost::property_tree::ptree GetNewPropertyTree() const override;
    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS virtual bool SetValuesFromPropertyTree(const boost::property_tree::ptree& pt) override;

    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS static std::shared_ptr<ApiCommand_t> CreateFromJson(const std::string& jsonStr);
};

struct CLASS_VISIBILITY_TELEMETRY_DEFINITIONS GetStorageApiCommand_t : public ApiCommand_t {
    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS GetStorageApiCommand_t();
    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS virtual ~GetStorageApiCommand_t() override;
    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS static const std::string name;
};

struct CLASS_VISIBILITY_TELEMETRY_DEFINITIONS GetOutductsApiCommand_t : public ApiCommand_t {
    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS GetOutductsApiCommand_t();
    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS virtual ~GetOutductsApiCommand_t() override;
    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS static const std::string name;
};

struct CLASS_VISIBILITY_TELEMETRY_DEFINITIONS GetOutductCapabilitiesApiCommand_t : public ApiCommand_t {
    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS GetOutductCapabilitiesApiCommand_t();
    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS virtual ~GetOutductCapabilitiesApiCommand_t() override;
    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS static const std::string name;
};

struct CLASS_VISIBILITY_TELEMETRY_DEFINITIONS GetInductsApiCommand_t : public ApiCommand_t {
    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS GetInductsApiCommand_t();
    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS ~GetInductsApiCommand_t();
    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS static const std::string name;
};

struct CLASS_VISIBILITY_TELEMETRY_DEFINITIONS PingApiCommand_t : public ApiCommand_t {
    uint64_t m_nodeId;
    uint64_t m_pingServiceNumber;
    uint64_t m_bpVersion;

    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS PingApiCommand_t();
    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS virtual ~PingApiCommand_t() override;

    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS bool operator==(const ApiCommand_t& o) const;
    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS bool operator!=(const ApiCommand_t& o) const;

    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS virtual boost::property_tree::ptree GetNewPropertyTree() const override;
    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS virtual bool SetValuesFromPropertyTree(const boost::property_tree::ptree& pt) override;
    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS static const std::string name;
};

struct CLASS_VISIBILITY_TELEMETRY_DEFINITIONS UploadContactPlanApiCommand_t : public ApiCommand_t {
    std::string m_contactPlanJson;

    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS UploadContactPlanApiCommand_t();
    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS virtual ~UploadContactPlanApiCommand_t() override;

    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS bool operator==(const ApiCommand_t& o) const;
    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS bool operator!=(const ApiCommand_t& o) const;

    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS virtual boost::property_tree::ptree GetNewPropertyTree() const override;
    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS virtual bool SetValuesFromPropertyTree(const boost::property_tree::ptree& pt) override;

    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS static const std::string name;
};

struct CLASS_VISIBILITY_TELEMETRY_DEFINITIONS GetExpiringStorageApiCommand_t : public ApiCommand_t {
    uint64_t m_priority;
    uint64_t m_thresholdSecondsFromNow;

    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS GetExpiringStorageApiCommand_t();
    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS virtual ~GetExpiringStorageApiCommand_t() override;

    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS bool operator==(const ApiCommand_t& o) const;
    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS bool operator!=(const ApiCommand_t& o) const;

    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS virtual boost::property_tree::ptree GetNewPropertyTree() const override;
    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS virtual bool SetValuesFromPropertyTree(const boost::property_tree::ptree& pt) override;

    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS static const std::string name;
};

struct CLASS_VISIBILITY_TELEMETRY_DEFINITIONS UpdateBpSecApiCommand_t : public ApiCommand_t {
    std::string m_bpSecJson;

    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS UpdateBpSecApiCommand_t();
    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS virtual ~UpdateBpSecApiCommand_t() override;

    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS bool operator==(const ApiCommand_t& o) const;
    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS bool operator!=(const ApiCommand_t& o) const;

    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS virtual boost::property_tree::ptree GetNewPropertyTree() const override;
    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS virtual bool SetValuesFromPropertyTree(const boost::property_tree::ptree& pt) override;

    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS static const std::string name;
};

struct CLASS_VISIBILITY_TELEMETRY_DEFINITIONS GetBpSecApiCommand_t : public ApiCommand_t {
    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS GetBpSecApiCommand_t();
    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS virtual ~GetBpSecApiCommand_t() override;

    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS static const std::string name;
};

struct CLASS_VISIBILITY_TELEMETRY_DEFINITIONS SetMaxSendRateApiCommand_t : public ApiCommand_t {
    uint64_t m_rateBitsPerSec;
    uint64_t m_outduct;

    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS SetMaxSendRateApiCommand_t();

    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS bool operator==(const ApiCommand_t& o) const;
    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS bool operator!=(const ApiCommand_t& o) const;

    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS virtual boost::property_tree::ptree GetNewPropertyTree() const override;
    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS virtual bool SetValuesFromPropertyTree(const boost::property_tree::ptree& pt) override;

    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS static const std::string name;
};

struct CLASS_VISIBILITY_TELEMETRY_DEFINITIONS GetHdtnConfigApiCommand_t : public ApiCommand_t {
    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS GetHdtnConfigApiCommand_t();
    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS static const std::string name;
};

struct GetHdtnVersionApiCommand_t : public ApiCommand_t {
    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS GetHdtnVersionApiCommand_t();
    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS virtual ~GetHdtnVersionApiCommand_t() override;
    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS static const std::string name;
};

struct CLASS_VISIBILITY_TELEMETRY_DEFINITIONS ApiResp_t : public JsonSerializable {
    bool m_success;
    std::string m_message;

    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS ApiResp_t();
    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS virtual ~ApiResp_t();

    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS virtual boost::property_tree::ptree GetNewPropertyTree() const override;
    CLASS_VISIBILITY_TELEMETRY_DEFINITIONS virtual bool SetValuesFromPropertyTree(const boost::property_tree::ptree& pt) override;
};

/**
 * Represents a ZMQ connection identifier. ZMQ identities are sent by router sockets
 * and used to keep track of and send responses to specific clients. ZMQ connection
 * IDs are always 5 bytes
 */
static constexpr uint8_t ZMQ_CONNECTION_ID_LEN = 5;

class CLASS_VISIBILITY_TELEMETRY_DEFINITIONS ZmqConnectionId_t {
    public:
        CLASS_VISIBILITY_TELEMETRY_DEFINITIONS ZmqConnectionId_t();

        // This constructor generates custom ZMQ connection IDs. It accepts a single byte (uint8_t)
        // and assigns it to the last byte of the ID, while prepending all other bytes with 0's.
        CLASS_VISIBILITY_TELEMETRY_DEFINITIONS ZmqConnectionId_t(const uint8_t val);

        // Convert ZmqConnectionId to a zmq::message_t
        CLASS_VISIBILITY_TELEMETRY_DEFINITIONS zmq::message_t Msg();

        // Compare two ZmqConnectionId objects
        CLASS_VISIBILITY_TELEMETRY_DEFINITIONS bool operator==(const ZmqConnectionId_t& other) const;

        // Compare ZmqConnectionId object with a zmq message
        CLASS_VISIBILITY_TELEMETRY_DEFINITIONS bool operator==(const zmq::message_t& msg) const;

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
