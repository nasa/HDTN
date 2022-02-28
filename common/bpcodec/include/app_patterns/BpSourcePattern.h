#ifndef _BP_SOURCE_PATTERN_H
#define _BP_SOURCE_PATTERN_H 1

#include <string>
#include <boost/thread.hpp>
#include <boost/asio.hpp>
#include "OutductManager.h"
#include "InductManager.h"
#include "codec/bpv6.h"
#include "codec/CustodyTransferManager.h"
#include "bp_app_patterns_lib_export.h"

class BpSourcePattern {
public:
    BP_APP_PATTERNS_LIB_EXPORT BpSourcePattern();
    BP_APP_PATTERNS_LIB_EXPORT virtual ~BpSourcePattern();
    BP_APP_PATTERNS_LIB_EXPORT void Stop();
    BP_APP_PATTERNS_LIB_EXPORT void Start(OutductsConfig_ptr & outductsConfigPtr, InductsConfig_ptr & inductsConfigPtr, bool custodyTransferUseAcs,
        const cbhe_eid_t & myEid, uint32_t bundleRate, const cbhe_eid_t & finalDestEid, const uint64_t myCustodianServiceId,
        const bool requireRxBundleBeforeNextTx = false, const bool forceDisableCustody = false, const bool useBpVersion7 = false);

    uint64_t m_bundleCount;
    uint64_t m_numRfc5050CustodyTransfers;
    uint64_t m_numAcsCustodyTransfers;
    uint64_t m_numAcsPacketsReceived;

    uint64_t m_totalNonAdminRecordBpv6PayloadBytesRx;
    uint64_t m_totalNonAdminRecordBpv6BundleBytesRx;
    uint64_t m_totalNonAdminRecordBpv6BundlesRx;

    uint64_t m_totalNonAdminRecordBpv7PayloadBytesRx;
    uint64_t m_totalNonAdminRecordBpv7BundleBytesRx;
    uint64_t m_totalNonAdminRecordBpv7BundlesRx;

    OutductFinalStats m_outductFinalStats;

protected:
    virtual uint64_t GetNextPayloadLength_Step1() = 0;
    virtual bool CopyPayload_Step2(uint8_t * destinationBuffer) = 0;
    BP_APP_PATTERNS_LIB_EXPORT virtual bool ProcessNonAdminRecordBundlePayload(const uint8_t * data, const uint64_t size);
private:
    BP_APP_PATTERNS_LIB_EXPORT void BpSourcePatternThreadFunc(uint32_t bundleRate);
    BP_APP_PATTERNS_LIB_EXPORT void WholeRxBundleReadyCallback(padded_vector_uint8_t & wholeBundleVec);
    BP_APP_PATTERNS_LIB_EXPORT void OnNewOpportunisticLinkCallback(const uint64_t remoteNodeId, Induct * thisInductPtr);
    BP_APP_PATTERNS_LIB_EXPORT void OnDeletedOpportunisticLinkCallback(const uint64_t remoteNodeId);


    OutductManager m_outductManager;
    InductManager m_inductManager;
    std::unique_ptr<boost::thread> m_bpSourcePatternThreadPtr;
    volatile bool m_running;
    bool m_useCustodyTransfer;
    bool m_custodyTransferUseAcs;
    bool m_useInductForSendingBundles;
    bool m_useBpVersion7;
    cbhe_eid_t m_finalDestinationEid;
    cbhe_eid_t m_myEid;
    uint64_t m_myCustodianServiceId;
    cbhe_eid_t m_myCustodianEid;
    std::string m_myCustodianEidUriString;
    boost::mutex m_mutexCtebSet;
    boost::mutex m_mutexBundleUuidSet;
    std::set<FragmentSet::data_fragment_t> m_outstandingCtebCustodyIdsFragmentSet;
    std::set<cbhe_bundle_uuid_nofragment_t> m_cbheBundleUuidSet;
    bool m_detectedNextCustodianSupportsCteb;
    bool m_requireRxBundleBeforeNextTx;
    volatile bool m_isWaitingForRxBundleBeforeNextTx;
    boost::condition_variable m_waitingForRxBundleBeforeNextTxConditionVariable;
    uint64_t m_tcpclOpportunisticRemoteNodeId;
    Induct * m_tcpclInductPtr;
    //version 7 stuff
    cbhe_eid_t m_lastPreviousNode;
    std::vector<uint64_t> m_hopCounts;
public:
    volatile bool m_allOutductsReady;
};


#endif //_BP_SOURCE_PATTERN_H
