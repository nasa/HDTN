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
#include <boost/bind/bind.hpp>
#include <boost/make_unique.hpp>
#include "Uri.h"
#include "TcpclInduct.h"
#include "TcpclV4Induct.h"
#include "codec/BundleViewV7.h"

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
    std::cout << "m_totalBundlesVersion6Rx: " << m_totalBundlesVersion6Rx << "\n";
    std::cout << "m_totalBundlesVersion7Rx: " << m_totalBundlesVersion7Rx << "\n";
}

bool BpSinkPattern::Init(InductsConfig_ptr & inductsConfigPtr, OutductsConfig_ptr & outductsConfigPtr,
    bool isAcsAware, const cbhe_eid_t & myEid, uint32_t processingLagMs, const uint64_t maxBundleSizeBytes, const uint64_t myBpEchoServiceId)
{
    m_tcpclOpportunisticRemoteNodeId = 0;
    m_myEid = myEid;
    m_myEidUriString = Uri::GetIpnUriString(m_myEid.nodeId, m_myEid.serviceId);
    m_myEidEcho.Set(myEid.nodeId, myBpEchoServiceId);

    m_totalPayloadBytesRx = 0;
    m_totalBundleBytesRx = 0;
    m_totalBundlesVersion6Rx = 0;
    m_totalBundlesVersion7Rx = 0;

    m_lastPayloadBytesRx = 0;
    m_lastBundleBytesRx = 0;
    m_lastBundlesRx = 0;
    

    m_nextCtebCustodyId = 0;

    m_tcpclInductPtr = NULL;

    M_EXTRA_PROCESSING_TIME_MS = processingLagMs;
    if (inductsConfigPtr) {
        m_inductManager.LoadInductsFromConfig(boost::bind(&BpSinkPattern::WholeBundleReadyCallback, this, boost::placeholders::_1),
            *inductsConfigPtr, myEid.nodeId, UINT16_MAX, maxBundleSizeBytes,
            boost::bind(&BpSinkPattern::OnNewOpportunisticLinkCallback, this, boost::placeholders::_1, boost::placeholders::_2),
            boost::bind(&BpSinkPattern::OnDeletedOpportunisticLinkCallback, this, boost::placeholders::_1));
    }

    if (outductsConfigPtr) {
        m_hasSendCapability = true;
        m_hasSendCapabilityOverTcpclBidirectionalInduct = false;
        m_outductManager.LoadOutductsFromConfig(*outductsConfigPtr, myEid.nodeId, UINT16_MAX, maxBundleSizeBytes, boost::bind(&BpSinkPattern::WholeBundleReadyCallback, this, boost::placeholders::_1));
        while (!m_outductManager.AllReadyToForward()) {
            std::cout << "waiting for outduct to be ready...\n";
            boost::this_thread::sleep(boost::posix_time::milliseconds(500));
        }
    }
    else if ((inductsConfigPtr) && ((inductsConfigPtr->m_inductElementConfigVector[0].convergenceLayer == "tcpcl_v3") || (inductsConfigPtr->m_inductElementConfigVector[0].convergenceLayer == "tcpcl_v4"))) {
        m_hasSendCapability = true;
        m_hasSendCapabilityOverTcpclBidirectionalInduct = true;
        std::cout << "this bpsink pattern detected tcpcl convergence layer which is bidirectional.. supporting custody transfer\n";
    }
    else {
        m_hasSendCapability = false;
        m_hasSendCapabilityOverTcpclBidirectionalInduct = false;
    }

    if (m_hasSendCapability) {
        m_custodyTransferManagerPtr = boost::make_unique<CustodyTransferManager>(isAcsAware, myEid.nodeId, myEid.serviceId);
        if (isAcsAware) {
            m_timerAcs.expires_from_now(boost::posix_time::seconds(1));
            m_timerAcs.async_wait(boost::bind(&BpSinkPattern::AcsNeedToSend_TimerExpired, this, boost::asio::placeholders::error));
        }
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

    const uint8_t firstByte = rxBuf[0];
    const bool isBpVersion6 = (firstByte == 6);
    const bool isBpVersion7 = (firstByte == ((4U << 5) | 31U));  //CBOR major type 4, additional information 31 (Indefinite-Length Array)
    if (isBpVersion6) {
        BundleViewV6 bv;
        if (!bv.SwapInAndLoadBundle(rxBuf)) { //invalid bundle
            std::cerr << "malformed bundle\n";
            return false;
        }
        bpv6_primary_block & primary = bv.m_primaryBlockView.header;
        const cbhe_eid_t finalDestEid(primary.dst_node, primary.dst_svc);
        const cbhe_eid_t srcEid(primary.src_node, primary.src_svc);



        static constexpr uint64_t requiredPrimaryFlagsForEcho = BPV6_BUNDLEFLAG_SINGLETON;
        const bool isEcho = (((primary.flags & requiredPrimaryFlagsForEcho) == requiredPrimaryFlagsForEcho) && (finalDestEid == m_myEidEcho));
        if (isEcho && (!m_hasSendCapability)) {
            std::cout << "a ping request was received but this bpsinkpattern does not have send capability.. ignoring bundle\n";
            return false;
        }

        if ((!isEcho) && (finalDestEid != m_myEid)) {
            std::cerr << "bundle received has a destination that doesn't match my eid\n";
            return false;
        }

        //accept custody
        if (m_hasSendCapability) { //has bidirectionality
            static constexpr uint64_t requiredPrimaryFlagsForCustody = BPV6_BUNDLEFLAG_SINGLETON | BPV6_BUNDLEFLAG_CUSTODY;

            if (isEcho) {
                primary.dst_node = primary.src_node;
                primary.dst_svc = primary.src_svc;
                primary.src_node = m_myEidEcho.nodeId;
                primary.src_svc = m_myEidEcho.serviceId;
                bv.m_primaryBlockView.SetManuallyModified();
                bv.Render(messageSize + 10);
                Forward_ThreadSafe(srcEid, bv.m_frontBuffer); //srcEid is the new destination
                return true;
            }
            else if ((primary.flags & requiredPrimaryFlagsForCustody) == requiredPrimaryFlagsForCustody) {
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
                        Forward_ThreadSafe(custodySignalDestEid, m_bufferSpaceForCustodySignalRfc5050SerializedBundle);
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
        ++m_totalBundlesVersion6Rx;

        boost::asio::const_buffer & blockBodyBuffer = blocks[0]->actualSerializedBodyPtr;
        if (!ProcessPayload((const uint8_t *)blockBodyBuffer.data(), blockBodyBuffer.size())) {
            std::cerr << "error ProcessPayload\n";
            return false;
        }
    }
    else if (isBpVersion7) {
        BundleViewV7 bv;
        if (!bv.SwapInAndLoadBundle(rxBuf)) { //invalid bundle
            std::cerr << "malformed bpv7 bundle\n";
            return false;
        }
        Bpv7CbhePrimaryBlock & primary = bv.m_primaryBlockView.header;
        const cbhe_eid_t finalDestEid = primary.m_destinationEid;
        const cbhe_eid_t srcEid = primary.m_sourceNodeId;




        static constexpr uint64_t requiredPrimaryFlagsForEcho = 0;
        const bool isEcho = (((primary.m_bundleProcessingControlFlags & requiredPrimaryFlagsForEcho) == requiredPrimaryFlagsForEcho) && (finalDestEid == m_myEidEcho));
        if (isEcho && (!m_hasSendCapability)) {
            std::cout << "a ping request was received but this bpsinkpattern does not have send capability.. ignoring bundle\n";
            return false;
        }

        if ((!isEcho) && (finalDestEid != m_myEid)) {
            std::cerr << "bundle received has a destination that doesn't match my eid\n";
            return false;
        }

        if (m_hasSendCapability) { //has bidirectionality
            if (isEcho) {
                primary.m_destinationEid = primary.m_sourceNodeId;
                primary.m_sourceNodeId = m_myEidEcho;
                bv.m_primaryBlockView.SetManuallyModified();
                bv.Render(messageSize + 10);
                Forward_ThreadSafe(srcEid, bv.m_frontBuffer); //srcEid is the new destination
                return true;
            }
        }
        std::vector<BundleViewV7::Bpv7CanonicalBlockView*> blocks;
        bv.GetCanonicalBlocksByType(BPV7_BLOCKTYPE_PAYLOAD, blocks);

        if (blocks.size() != 1) {
            std::cerr << "error payload block not found\n";
            return false;
        }

        const uint64_t payloadDataLength = blocks[0]->header.m_dataLength;
        const uint8_t * payloadDataPtr = blocks[0]->header.m_dataPtr;
        m_totalPayloadBytesRx += payloadDataLength;
        m_totalBundleBytesRx += messageSize;
        ++m_totalBundlesVersion7Rx;

        if (!ProcessPayload(payloadDataPtr, payloadDataLength)) {
            std::cerr << "error ProcessPayload\n";
            return false;
        }
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
        const uint64_t totalBundlesRx = m_totalBundlesVersion6Rx + m_totalBundlesVersion7Rx;
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
            Forward_ThreadSafe(custodySignalDestEid, it->second);
        }
    }
}

void BpSinkPattern::OnNewOpportunisticLinkCallback(const uint64_t remoteNodeId, Induct * thisInductPtr) {
    if (m_tcpclInductPtr = dynamic_cast<TcpclInduct*>(thisInductPtr)) {
        std::cout << "New opportunistic link detected on Tcpcl induct for ipn:" << remoteNodeId << ".*\n";
        m_tcpclOpportunisticRemoteNodeId = remoteNodeId;
    }
    else if (m_tcpclInductPtr = dynamic_cast<TcpclV4Induct*>(thisInductPtr)) {
        std::cout << "New opportunistic link detected on TcpclV4 induct for ipn:" << remoteNodeId << ".*\n";
        m_tcpclOpportunisticRemoteNodeId = remoteNodeId;
    }
    else {
        std::cerr << "error in BpSinkPattern::OnNewOpportunisticLinkCallback: Induct ptr cannot cast to TcpclInduct or TcpclV4Induct\n";
    }
}
void BpSinkPattern::OnDeletedOpportunisticLinkCallback(const uint64_t remoteNodeId) {
    m_tcpclOpportunisticRemoteNodeId = 0;
    std::cout << "Deleted opportunistic link on Tcpcl induct for ipn:" << remoteNodeId << ".*\n";
}

bool BpSinkPattern::Forward_ThreadSafe(const cbhe_eid_t & destEid, std::vector<uint8_t> & bundleToMoveAndSend) {
    bool successForward = false;
    if (m_hasSendCapabilityOverTcpclBidirectionalInduct) {
        //if (destEid.nodeId != m_tcpclOpportunisticRemoteNodeId) {
        //    std::cerr << "error: node id " << destEid.nodeId << " does not match tcpcl opportunistic link node id " << m_tcpclOpportunisticRemoteNodeId << std::endl;
        //}
        //else if (m_tcpclInductPtr) {
        //note BpSink has no routing capability so it must send to the only connection available to it
        if (m_tcpclInductPtr) {
            m_mutexForward.lock();
            //successForward = m_tcpclInductPtr->ForwardOnOpportunisticLink(destEid.nodeId, bundleToMoveAndSend, 3);
            successForward = m_tcpclInductPtr->ForwardOnOpportunisticLink(m_tcpclOpportunisticRemoteNodeId, bundleToMoveAndSend, 3);
            m_mutexForward.unlock();
        }
        else {
            std::cerr << "error: tcpcl induct does not exist to forward\n";
        }
    }
    else {
        m_mutexForward.lock();
        successForward = m_outductManager.Forward_Blocking(destEid, bundleToMoveAndSend, 3);
        m_mutexForward.unlock();
    }
    if (!successForward) {
        std::cerr << "error forwarding for 3 seconds\n";
    }
    else {
        //std::cout << "success forwarding\n";
    }
    return successForward;
}
