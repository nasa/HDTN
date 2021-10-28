#ifndef _BP_SOURCE_PATTERN_H
#define _BP_SOURCE_PATTERN_H 1

#include <string>
#include <boost/thread.hpp>
#include <boost/asio.hpp>
#include "OutductManager.h"
#include "InductManager.h"
#include "codec/bpv6.h"
#include "codec/CustodyTransferEnhancementBlock.h"
#include "codec/CustodyTransferManager.h"

class BpSourcePattern {
public:
    BpSourcePattern();
    virtual ~BpSourcePattern();
    void Stop();
    void Start(const OutductsConfig & outductsConfig, InductsConfig_ptr & inductsConfigPtr, bool custodyTransferUseAcs,
        const cbhe_eid_t & myEid, uint32_t bundleRate, const cbhe_eid_t & finalDestEid, const uint64_t myCustodianServiceId,
        const bool requireRxBundleBeforeNextTx = false, const bool forceDisableCustody = false);

    uint64_t m_bundleCount;
    uint64_t m_numRfc5050CustodyTransfers;
    uint64_t m_numAcsCustodyTransfers;
    uint64_t m_numAcsPacketsReceived;

    uint64_t m_totalNonAdminRecordPayloadBytesRx;
    uint64_t m_totalNonAdminRecordBundleBytesRx;
    uint64_t m_totalNonAdminRecordBundlesRx;

    OutductFinalStats m_outductFinalStats;

protected:
    virtual uint64_t GetNextPayloadLength_Step1() = 0;
    virtual bool CopyPayload_Step2(uint8_t * destinationBuffer) = 0;
    virtual bool ProcessNonAdminRecordBundlePayload(const uint8_t * data, const uint64_t size);
private:
    void BpSourcePatternThreadFunc(uint32_t bundleRate);
    void WholeRxBundleReadyCallback(std::vector<uint8_t> & wholeBundleVec);


    OutductManager m_outductManager;
    InductManager m_inductManager;
    std::unique_ptr<boost::thread> m_bpSourcePatternThreadPtr;
    volatile bool m_running;
    bool m_useCustodyTransfer;
    bool m_custodyTransferUseAcs;
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
public:
    volatile bool m_allOutductsReady;
};


#endif //_BP_SOURCE_PATTERN_H
