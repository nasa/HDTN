#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <cassert>
#include <iostream>

#include "codec/bpv6-ext-block.h"
#include "codec/bpv6.h"
#include "app_patterns/BpSinkPattern.h"
#include <boost/thread.hpp>
#include <boost/bind.hpp>
#include <boost/make_unique.hpp>
#include "Uri.h"


BpSinkPattern::BpSinkPattern() : m_timerAcs(m_ioService), m_timerTransferRateStats(m_ioService) {}

BpSinkPattern::~BpSinkPattern() {
    Stop();
}

void BpSinkPattern::Stop() {

    if (m_ioServiceThreadPtr) {
        m_timerAcs.cancel();
        m_timerTransferRateStats.cancel();
        m_ioServiceThreadPtr->join();
        m_ioServiceThreadPtr.reset(); //delete it
    }

    m_inductManager.Clear();

    std::cout << "totalPayloadBytesRx: " << m_totalPayloadBytesRx << "\n";
    std::cout << "totalBundleBytesRx: " << m_totalBundleBytesRx << "\n";
    std::cout << "totalBundlesRx: " << m_totalBundlesRx << "\n";
}

bool BpSinkPattern::Init(const InductsConfig & inductsConfig, OutductsConfig_ptr & outductsConfigPtr,
    bool isAcsAware, const cbhe_eid_t & myEid, uint32_t processingLagMs, const uint64_t maxBundleSizeBytes)
{

    m_myEid = myEid;
    m_myEidUriString = Uri::GetIpnUriString(m_myEid.nodeId, m_myEid.serviceId);

    m_totalPayloadBytesRx = 0;
    m_totalBundleBytesRx = 0;
    m_totalBundlesRx = 0;

    m_lastPayloadBytesRx = 0;
    m_lastBundleBytesRx = 0;
    m_lastBundlesRx = 0;
    

    m_nextCtebCustodyId = 0;

    M_EXTRA_PROCESSING_TIME_MS = processingLagMs;
    m_inductManager.LoadInductsFromConfig(boost::bind(&BpSinkPattern::WholeBundleReadyCallback, this, boost::placeholders::_1), inductsConfig, myEid.nodeId, UINT16_MAX, maxBundleSizeBytes);

    if (outductsConfigPtr) {
        m_useCustodyTransfer = true;
        m_outductManager.LoadOutductsFromConfig(*outductsConfigPtr, myEid.nodeId, UINT16_MAX);
        while (!m_outductManager.AllReadyToForward()) {
            std::cout << "waiting for outduct to be ready...\n";
            boost::this_thread::sleep(boost::posix_time::milliseconds(500));
        }
        m_custodyTransferManagerPtr = boost::make_unique<CustodyTransferManager>(isAcsAware, myEid.nodeId, myEid.serviceId);
        if (isAcsAware) {
            m_timerAcs.expires_from_now(boost::posix_time::seconds(1));
            m_timerAcs.async_wait(boost::bind(&BpSinkPattern::AcsNeedToSend_TimerExpired, this, boost::asio::placeholders::error));
        }
    }
    else {
        m_useCustodyTransfer = false;
    }
    m_lastPtime = boost::posix_time::microsec_clock::universal_time();
    m_timerTransferRateStats.expires_from_now(boost::posix_time::seconds(5));
    m_timerTransferRateStats.async_wait(boost::bind(&BpSinkPattern::TransferRate_TimerExpired, this, boost::asio::placeholders::error));
    m_ioServiceThreadPtr = boost::make_unique<boost::thread>(boost::bind(&boost::asio::io_service::run, &m_ioService));
    
    return true;
}



bool BpSinkPattern::Process(std::vector<uint8_t> & rxBuf, const std::size_t messageSize) {

    if (M_EXTRA_PROCESSING_TIME_MS) {
        boost::this_thread::sleep(boost::posix_time::milliseconds(M_EXTRA_PROCESSING_TIME_MS));
    }

    
    BundleViewV6 bv;
    if (!bv.SwapInAndLoadBundle(rxBuf)) { //invalid bundle
        std::cerr << "malformed bundle\n";
        return false;
    }
    bpv6_primary_block & primary = bv.m_primaryBlockView.header;
    const cbhe_eid_t finalDestEid(primary.dst_node, primary.dst_svc);
    const cbhe_eid_t srcEid(primary.src_node, primary.src_svc);

    if (finalDestEid != m_myEid) {
        std::cerr << "bundle received has a destination that doesn't match my eid\n";
        return false;
    }


    //accept custody
    if (m_useCustodyTransfer) {
        static constexpr uint64_t requiredPrimaryFlagsForCustody = BPV6_BUNDLEFLAG_SINGLETON | BPV6_BUNDLEFLAG_NOFRAGMENT | BPV6_BUNDLEFLAG_CUSTODY;
        if ((primary.flags & requiredPrimaryFlagsForCustody) == requiredPrimaryFlagsForCustody) {
            const uint64_t newCtebCustodyId = m_nextCtebCustodyId++;
            bpv6_primary_block primaryForCustodySignalRfc5050;
            m_mutexCtm.lock();
            const bool successfullyProcessedCustody = m_custodyTransferManagerPtr->ProcessCustodyOfBundle(bv, true,
                newCtebCustodyId, BPV6_ACS_STATUS_REASON_INDICES::SUCCESS__NO_ADDITIONAL_INFORMATION,
                m_bufferSpaceForCustodySignalRfc5050SerializedBundle, primaryForCustodySignalRfc5050);
            m_mutexCtm.unlock();
            if (!successfullyProcessedCustody) {
                std::cerr << "error unable to process custody\n";
                return false;
            }
            else {
                if (!m_bufferSpaceForCustodySignalRfc5050SerializedBundle.empty()) {
                    //send a classic rfc5050 custody signal due to acs disabled or bundle received has an invalid cteb
                    const cbhe_eid_t custodySignalDestEid(primaryForCustodySignalRfc5050.dst_node, primaryForCustodySignalRfc5050.dst_svc);
                    m_mutexForward.lock();
                    bool successForward = m_outductManager.Forward_Blocking(custodySignalDestEid, m_bufferSpaceForCustodySignalRfc5050SerializedBundle, 3);
                    m_mutexForward.unlock();
                    if (!successForward) {
                        std::cerr << "error forwarding for 3 seconds for custody signal final dest eid (" << custodySignalDestEid.nodeId << "," << custodySignalDestEid.serviceId << ")\n";
                    }
                    else {
                        //std::cout << "success forwarding\n";
                    }
                }
                else if (m_custodyTransferManagerPtr->GetLargestNumberOfFills() > 100) {
                    boost::asio::post(m_ioService, boost::bind(&BpSinkPattern::SendAcsFromTimerThread, this));
                }
            }
        }
    }

    std::vector<BundleViewV6::Bpv6CanonicalBlockView*> blocks;
    bv.GetCanonicalBlocksByType(BPV6_BLOCKTYPE_PAYLOAD, blocks);
    if (blocks.size() != 1) {
        std::cerr << "error payload block not found\n";
        return false;
    }
    bpv6_canonical_block & payloadBlock = blocks[0]->header;
    m_totalPayloadBytesRx += payloadBlock.length;
    m_totalBundleBytesRx += messageSize;
    ++m_totalBundlesRx;

    boost::asio::const_buffer & blockBodyBuffer = blocks[0]->actualSerializedBodyPtr;
    if (!ProcessPayload((const uint8_t *)blockBodyBuffer.data(), blockBodyBuffer.size())) {
        std::cerr << "error ProcessPayload\n";
        return false;
    }
    

    return true;
}

void BpSinkPattern::WholeBundleReadyCallback(std::vector<uint8_t> & wholeBundleVec) {
    //if more than 1 BpSinkAsync context, must protect shared resources with mutex.  Each BpSinkAsync context has
    //its own processing thread that calls this callback
    Process(wholeBundleVec, wholeBundleVec.size());
}

void BpSinkPattern::AcsNeedToSend_TimerExpired(const boost::system::error_code& e) {
    if (e != boost::asio::error::operation_aborted) {
        // Timer was not cancelled, take necessary action.
        SendAcsFromTimerThread();
        
        m_timerAcs.expires_from_now(boost::posix_time::seconds(1));
        m_timerAcs.async_wait(boost::bind(&BpSinkPattern::AcsNeedToSend_TimerExpired, this, boost::asio::placeholders::error));
    }
    else {
        std::cout << "timer stopped\n";
    }
}

void BpSinkPattern::TransferRate_TimerExpired(const boost::system::error_code& e) {
    if (e != boost::asio::error::operation_aborted) {
        // Timer was not cancelled, take necessary action.
        boost::posix_time::ptime finishedTime = boost::posix_time::microsec_clock::universal_time();
        const uint64_t totalPayloadBytesRx = m_totalPayloadBytesRx;
        const uint64_t totalBundleBytesRx = m_totalBundleBytesRx;
        const uint64_t totalBundlesRx = m_totalBundlesRx;
        boost::posix_time::time_duration diff = finishedTime - m_lastPtime;
        const uint64_t diffTotalPayloadBytesRx = totalPayloadBytesRx - m_lastPayloadBytesRx;
        const uint64_t diffBundleBytesRx = totalBundleBytesRx - m_lastBundleBytesRx;
        const uint64_t diffBundlesRx = totalBundlesRx - m_lastBundlesRx;
        if (diffTotalPayloadBytesRx || diffBundleBytesRx || diffBundlesRx) {
            const double payloadRateMbps = (diffTotalPayloadBytesRx * 8.0) / (diff.total_microseconds());
            const double bundleRateMbps = (diffBundleBytesRx * 8.0) / (diff.total_microseconds());
            const double bundlesPerSecond = (diffBundlesRx * 1e6) / (diff.total_microseconds());
            printf("Payload Only Rate: %0.4f Mbits/sec, Total Rate: %0.4f Mbits/sec, %0.4f Bundles/sec: \n", payloadRateMbps, bundleRateMbps, bundlesPerSecond);
        }
        
        m_lastPayloadBytesRx = totalPayloadBytesRx;
        m_lastBundleBytesRx = totalBundleBytesRx;
        m_lastBundlesRx = totalBundlesRx;
        m_lastPtime = finishedTime;
        m_timerTransferRateStats.expires_from_now(boost::posix_time::seconds(5));
        m_timerTransferRateStats.async_wait(boost::bind(&BpSinkPattern::TransferRate_TimerExpired, this, boost::asio::placeholders::error));
    }
    else {
        std::cout << "transfer rate timer stopped\n";
    }
}

void BpSinkPattern::SendAcsFromTimerThread() {
    //std::cout << "send acs, fills = " << ctm.GetLargestNumberOfFills() << "\n";
    std::list<std::pair<bpv6_primary_block, std::vector<uint8_t> > > serializedPrimariesAndBundlesList;
    m_mutexCtm.lock();
    const bool generatedSuccessfully = m_custodyTransferManagerPtr->GenerateAllAcsBundlesAndClear(serializedPrimariesAndBundlesList);
    m_mutexCtm.unlock();
    if (generatedSuccessfully) {
        for (std::list<std::pair<bpv6_primary_block, std::vector<uint8_t> > >::iterator it = serializedPrimariesAndBundlesList.begin();
            it != serializedPrimariesAndBundlesList.end(); ++it)
        {
            //send an acs custody signal due to acs send timer
            const cbhe_eid_t custodySignalDestEid(it->first.dst_node, it->first.dst_svc);
            m_mutexForward.lock();
            bool successForward = m_outductManager.Forward_Blocking(custodySignalDestEid, it->second, 3);
            m_mutexForward.unlock();
            if (!successForward) {
                std::cerr << "error forwarding for 3 seconds\n";
            }
            else {
                //std::cout << "success forwarding\n";
            }
            
        }
    }
}

