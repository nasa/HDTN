#ifndef _BPGEN_ASYNC_H
#define _BPGEN_ASYNC_H 1

#include <string>
#include <boost/thread.hpp>
#include <boost/asio.hpp>
#include "OutductManager.h"
#include "InductManager.h"
#include "codec/bpv6.h"
#include "codec/CustodyTransferEnhancementBlock.h"
#include "codec/CustodyTransferManager.h"

class BpGenAsync {
public:
    BpGenAsync();
    ~BpGenAsync();
    void Stop();
    void Start(const OutductsConfig & outductsConfig, InductsConfig_ptr & inductsConfigPtr, bool custodyTransferUseAcs, const cbhe_eid_t & myEid, uint32_t bundleSizeBytes, uint32_t bundleRate, uint64_t destFlowId = 2);

    uint64_t m_bundleCount;
    uint64_t m_numRfc5050CustodyTransfers;
    uint64_t m_numAcsCustodyTransfers;
    uint64_t m_numAcsPacketsReceived;

    OutductFinalStats m_outductFinalStats;


private:
    void BpGenThreadFunc(uint32_t bundleSizeBytes, uint32_t bundleRate, uint64_t destFlowId);
    void WholeCustodySignalBundleReadyCallback(std::vector<uint8_t> & wholeBundleVec);


    OutductManager m_outductManager;
    InductManager m_inductManager;
    std::unique_ptr<boost::thread> m_bpGenThreadPtr;
    volatile bool m_running;
    bool m_useCustodyTransfer;
    bool m_custodyTransferUseAcs;
    cbhe_eid_t m_myEid;
    std::string m_myEidUriString;
    boost::mutex m_mutexCtebSet;
    boost::mutex m_mutexBundleUuidSet;
    std::set<FragmentSet::data_fragment_t> m_outstandingCtebCustodyIdsFragmentSet;
    std::set<cbhe_bundle_uuid_nofragment_t> m_cbheBundleUuidSet;
    bool m_detectedNextCustodianSupportsCteb;
public:
    volatile bool m_allOutductsReady;
};


#endif //_BPGEN_ASYNC_H
