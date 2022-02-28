#ifndef _BP_SINK_PATTERN_H
#define _BP_SINK_PATTERN_H 1

#include <stdint.h>
#include "InductManager.h"
#include "OutductManager.h"
#include "codec/bpv6.h"
#include "codec/CustodyTransferManager.h"
#include <boost/asio.hpp>
#include "TcpclInduct.h"
#include "bp_app_patterns_lib_export.h"

class BpSinkPattern {
public:
    BP_APP_PATTERNS_LIB_EXPORT BpSinkPattern();
    BP_APP_PATTERNS_LIB_EXPORT void Stop();
    BP_APP_PATTERNS_LIB_EXPORT virtual ~BpSinkPattern();
    BP_APP_PATTERNS_LIB_EXPORT bool Init(InductsConfig_ptr & inductsConfigPtr, OutductsConfig_ptr & outductsConfigPtr,
        bool isAcsAware, const cbhe_eid_t & myEid, uint32_t processingLagMs, const uint64_t maxBundleSizeBytes, const uint64_t myBpEchoServiceId = 2047);
protected:
    virtual bool ProcessPayload(const uint8_t * data, const uint64_t size) = 0;
private:
    BP_APP_PATTERNS_LIB_EXPORT void WholeBundleReadyCallback(padded_vector_uint8_t & wholeBundleVec);
    BP_APP_PATTERNS_LIB_EXPORT bool Process(padded_vector_uint8_t & rxBuf, const std::size_t messageSize);
    BP_APP_PATTERNS_LIB_EXPORT void AcsNeedToSend_TimerExpired(const boost::system::error_code& e);
    BP_APP_PATTERNS_LIB_EXPORT void TransferRate_TimerExpired(const boost::system::error_code& e);
    BP_APP_PATTERNS_LIB_EXPORT void SendAcsFromTimerThread();
    BP_APP_PATTERNS_LIB_EXPORT void OnNewOpportunisticLinkCallback(const uint64_t remoteNodeId, Induct * thisInductPtr);
    BP_APP_PATTERNS_LIB_EXPORT void OnDeletedOpportunisticLinkCallback(const uint64_t remoteNodeId);
    BP_APP_PATTERNS_LIB_EXPORT bool Forward_ThreadSafe(const cbhe_eid_t & destEid, std::vector<uint8_t> & bundleToMoveAndSend);
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
    boost::mutex m_mutexCtm;
    boost::mutex m_mutexForward;
    uint64_t m_tcpclOpportunisticRemoteNodeId;
    Induct * m_tcpclInductPtr;
};


#endif  //_BP_SINK_PATTERN_H
