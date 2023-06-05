/**
 * @file BpSinkPattern.h
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
 * The BpSinkPattern class is a pure virtual base class for any application
 * that needs to implement receiving user-defined bundles and processing them.
 * The application needs only to override the virtual function ProcessPayload.
 * This class takes an HDTN induct config with one induct in the inductVector
 * for receiving the bundles.
 * In the event that the induct is not a bidirectional TCPCL induct,
 * this class can take an optional HDTN outduct config with one outduct in the outductVector
 * for automatically sending optional custody signals and for automatically
 * sending echo responses via a user-specified echo service number.
 */

#ifndef _BP_SINK_PATTERN_H
#define _BP_SINK_PATTERN_H 1
#include "bp_app_patterns_lib_export.h"
#ifndef CLASS_VISIBILITY_BP_APP_PATTERNS_LIB
#  ifdef _WIN32
#    define CLASS_VISIBILITY_BP_APP_PATTERNS_LIB
#  else
#    define CLASS_VISIBILITY_BP_APP_PATTERNS_LIB BP_APP_PATTERNS_LIB_EXPORT
#  endif
#endif
#include <stdint.h>
#include "InductManager.h"
#include "OutductManager.h"
#include "codec/bpv6.h"
#include "codec/CustodyTransferManager.h"
#include <boost/asio.hpp>
#include "TcpclInduct.h"
#include <queue>
#include <unordered_set>

class CLASS_VISIBILITY_BP_APP_PATTERNS_LIB BpSinkPattern {
public:
    BP_APP_PATTERNS_LIB_EXPORT BpSinkPattern();
    BP_APP_PATTERNS_LIB_EXPORT void Stop();
    BP_APP_PATTERNS_LIB_EXPORT virtual ~BpSinkPattern();
    BP_APP_PATTERNS_LIB_EXPORT bool Init(InductsConfig_ptr & inductsConfigPtr, OutductsConfig_ptr & outductsConfigPtr,
        const boost::filesystem::path& bpSecConfigFilePath,
        bool isAcsAware, const cbhe_eid_t & myEid, uint32_t processingLagMs, const uint64_t maxBundleSizeBytes, const uint64_t myBpEchoServiceId = 2047);
    BP_APP_PATTERNS_LIB_EXPORT void LogStats(PrimaryBlock& primaryBlock,
        bool isBpVersion6);
protected:
    virtual bool ProcessPayload(const uint8_t * data, const uint64_t size) = 0;
private:
    BP_APP_PATTERNS_LIB_NO_EXPORT void WholeBundleReadyCallback(padded_vector_uint8_t & wholeBundleVec);
    BP_APP_PATTERNS_LIB_NO_EXPORT bool Process(padded_vector_uint8_t & rxBuf, const std::size_t messageSize);
    BP_APP_PATTERNS_LIB_NO_EXPORT void AcsNeedToSend_TimerExpired(const boost::system::error_code& e);
    BP_APP_PATTERNS_LIB_NO_EXPORT void TransferRate_TimerExpired(const boost::system::error_code& e);
    BP_APP_PATTERNS_LIB_NO_EXPORT void SendAcsFromTimerThread();
    BP_APP_PATTERNS_LIB_NO_EXPORT void OnNewOpportunisticLinkCallback(const uint64_t remoteNodeId, Induct* thisInductPtr, void* sinkPtr);
    BP_APP_PATTERNS_LIB_NO_EXPORT void OnDeletedOpportunisticLinkCallback(const uint64_t remoteNodeId, Induct* thisInductPtr, void* sinkPtrAboutToBeDeleted);
    BP_APP_PATTERNS_LIB_NO_EXPORT bool Forward_ThreadSafe(const cbhe_eid_t & destEid, padded_vector_uint8_t& bundleToMoveAndSend);
    BP_APP_PATTERNS_LIB_NO_EXPORT void SenderReaderThreadFunc();
    BP_APP_PATTERNS_LIB_NO_EXPORT void OnFailedBundleVecSendCallback(padded_vector_uint8_t& movableBundle, std::vector<uint8_t>& userData, uint64_t outductUuid);
    BP_APP_PATTERNS_LIB_NO_EXPORT void OnSuccessfulBundleSendCallback(std::vector<uint8_t>& userData, uint64_t outductUuid);
    BP_APP_PATTERNS_LIB_NO_EXPORT void OnOutductLinkStatusChangedCallback(bool isLinkDownEvent, uint64_t outductUuid);
public:

    uint64_t m_totalPayloadBytesRx;
    uint64_t m_totalBundleBytesRx;
    uint64_t m_totalBundlesVersion6Rx;
    uint64_t m_totalBundlesVersion7Rx;

    uint64_t m_lastPayloadBytesRx;
    uint64_t m_lastBundleBytesRx;
    uint64_t m_lastBundlesRx;
    boost::posix_time::ptime m_lastPtime;
    cbhe_eid_t m_lastPreviousNode;
    std::vector<uint64_t> m_hopCounts;
    uint64_t m_bpv7Priority;

private:
    uint32_t M_EXTRA_PROCESSING_TIME_MS;

    InductManager m_inductManager;
    OutductManager m_outductManager;
    bool m_hasSendCapability;
    bool m_hasSendCapabilityOverTcpclBidirectionalInduct;
    cbhe_eid_t m_myEid;
    cbhe_eid_t m_myEidEcho;
    std::string m_myEidUriString;
    std::unique_ptr<CustodyTransferManager> m_custodyTransferManagerPtr;
    uint64_t m_nextCtebCustodyId;
    BundleViewV6 m_custodySignalRfc5050RenderedBundleView;
    boost::asio::io_service m_ioService;
    boost::asio::deadline_timer m_timerAcs;
    boost::asio::deadline_timer m_timerTransferRateStats;
    std::unique_ptr<boost::thread> m_ioServiceThreadPtr;
    std::unique_ptr<boost::thread> m_threadSenderReaderPtr;
    boost::condition_variable m_conditionVariableSenderReader;
    typedef std::pair<cbhe_eid_t, padded_vector_uint8_t> desteid_bundle_pair_t;
    std::queue<desteid_bundle_pair_t> m_bundleToSendQueue;
    boost::mutex m_mutexCurrentlySendingBundleIdSet;
    std::unordered_set<uint64_t> m_currentlySendingBundleIdSet;
    boost::condition_variable m_cvCurrentlySendingBundleIdSet;
    boost::mutex m_mutexQueueBundlesThatFailedToSend;
    typedef std::pair<uint64_t, cbhe_eid_t> bundleid_finaldesteid_pair_t;
    typedef std::pair<padded_vector_uint8_t, bundleid_finaldesteid_pair_t> bundle_userdata_pair_t;
    std::queue<bundle_userdata_pair_t> m_queueBundlesThatFailedToSend;
    volatile bool m_linkIsDown;
    volatile bool m_runningSenderThread;
    boost::mutex m_mutexCtm;
    boost::mutex m_mutexSendBundleQueue;
    uint64_t m_tcpclOpportunisticRemoteNodeId;
    Induct * m_tcpclInductPtr;

    struct BpSecImpl;
    std::unique_ptr<BpSecImpl> m_bpsecPimpl; // Pointer to the internal implementation
};


#endif  //_BP_SINK_PATTERN_H
