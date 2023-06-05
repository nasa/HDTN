/**
 * @file BpSinkPattern.cpp
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
 */

#include "codec/bpv6.h"
#include "app_patterns/BpSinkPattern.h"
#include <boost/thread.hpp>
#include <boost/bind/bind.hpp>
#include <boost/format.hpp>
#include <boost/make_unique.hpp>
#include "Uri.h"
#include "TcpclInduct.h"
#include "TcpclV4Induct.h"
#include "StcpInduct.h"
#include "codec/BundleViewV7.h"
#include "Logger.h"
#include "StatsLogger.h"
#include "ThreadNamer.h"
#include "BinaryConversions.h"
#ifdef BPSEC_SUPPORT_ENABLED
#include "BpSecManager.h"
#include "BpSecPolicyManager.h"
# define DO_BPSEC_TEST 1

struct BpSinkPattern::BpSecImpl {
    BpSecPolicyManager m_bpSecPolicyManager;
    BpSecPolicyProcessingContext m_policyProcessingCtx;
};
#else
struct BpSinkPattern::BpSecImpl {};
//std::unique_ptr<BpSecImpl> m_bpsecPimpl;
#endif

static constexpr hdtn::Logger::SubProcess subprocess = hdtn::Logger::SubProcess::none;

BpSinkPattern::BpSinkPattern() : m_timerAcs(m_ioService), m_timerTransferRateStats(m_ioService)
#ifdef BPSEC_SUPPORT_ENABLED
, m_bpsecPimpl(boost::make_unique<BpSecImpl>())
#endif
{}

BpSinkPattern::~BpSinkPattern() {
    Stop();
}

void BpSinkPattern::Stop() {

    if (m_ioServiceThreadPtr) {
        try {
            m_timerAcs.cancel();
        }
        catch (const boost::system::system_error& e) {
            LOG_WARNING(subprocess) << "BpSinkPattern::Stop calling timerAcs.cancel(): " << e.what();
        }
        try {
            m_timerTransferRateStats.cancel();
        }
        catch (const boost::system::system_error& e) {
            LOG_WARNING(subprocess) << "BpSinkPattern::Stop calling timerTransferRateStats.cancel(): " << e.what();
        }
        try {
            m_ioServiceThreadPtr->join();
            m_ioServiceThreadPtr.reset(); //delete it
        }
        catch (const boost::thread_resource_error&) {
            LOG_ERROR(subprocess) << "error stopping BpSinkPattern io_service thread";
        }
    }

    m_runningSenderThread = false; //thread stopping criteria
    //only lock one mutex at a time to prevent deadlock (a worker may call this function on an error condition)
    //lock then unlock each thread's mutex to prevent a missed notify after setting thread stopping criteria above
    m_mutexSendBundleQueue.lock();
    m_mutexSendBundleQueue.unlock();
    m_conditionVariableSenderReader.notify_one();
    //
    m_mutexCurrentlySendingBundleIdSet.lock();
    m_mutexCurrentlySendingBundleIdSet.unlock();
    m_cvCurrentlySendingBundleIdSet.notify_one(); //break out of the timed_wait (wait_until)

    if (m_threadSenderReaderPtr) {
        try {
            m_threadSenderReaderPtr->join();
            m_threadSenderReaderPtr.reset(); //delete it
        }
        catch (const boost::thread_resource_error&) {
            LOG_ERROR(subprocess) << "error stopping BpSinkPattern threadSenderReader";
        }
    }

    m_inductManager.Clear();

    LOG_INFO(subprocess) << "totalPayloadBytesRx: " << m_totalPayloadBytesRx;
    LOG_INFO(subprocess) << "totalBundleBytesRx: " << m_totalBundleBytesRx;
    LOG_INFO(subprocess) << "m_totalBundlesVersion6Rx: " << m_totalBundlesVersion6Rx;
    LOG_INFO(subprocess) << "m_totalBundlesVersion7Rx: " << m_totalBundlesVersion7Rx;
    for (std::size_t i = 0; i < m_hopCounts.size(); ++i) {
        if (m_hopCounts[i] != 0) {
            LOG_INFO(subprocess) << "received " << m_hopCounts[i] << " bundles with a hop count of " << i << ".";
        }
    }
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
    m_hopCounts.assign(256, 0);
    m_lastPreviousNode.Set(0, 0);

    m_lastPayloadBytesRx = 0;
    m_lastBundleBytesRx = 0;
    m_lastBundlesRx = 0;
    

    m_nextCtebCustodyId = 0;

    m_tcpclInductPtr = NULL;

    m_linkIsDown = false;

#ifdef DO_BPSEC_TEST
    { //simulate loading from policy file
        BpSecPolicy* p = m_bpsecPimpl->m_bpSecPolicyManager.CreateAndGetNewPolicy(
            "ipn:10.1",
            "ipn:*.*",
            "ipn:*.*",
            BPSEC_ROLE::ACCEPTOR);
        p->m_doConfidentiality = true;
        if (p->m_doConfidentiality) {
            static const std::string dataEncryptionKeyString(
                "81776572747975696f70617364666768"
                "71776572747975696f70617364666768"
            );
            BinaryConversions::HexStringToBytes(dataEncryptionKeyString, p->m_dataEncryptionKey);
        }

        p->m_doIntegrity = true;
        if (p->m_doIntegrity) {
            static const std::string hmacKeyString(
                "8a2b1a2b1a2b1a2b1a2b1a2b1a2b1a2b"
            );
            BinaryConversions::HexStringToBytes(hmacKeyString, p->m_hmacKey);
        }
    }
#endif // DO_BPSEC_TEST

    M_EXTRA_PROCESSING_TIME_MS = processingLagMs;
    if (inductsConfigPtr) {
        m_currentlySendingBundleIdSet.reserve(1024); //todo
        m_inductManager.LoadInductsFromConfig(boost::bind(&BpSinkPattern::WholeBundleReadyCallback, this, boost::placeholders::_1),
            *inductsConfigPtr, myEid.nodeId, UINT16_MAX, maxBundleSizeBytes,
            boost::bind(&BpSinkPattern::OnNewOpportunisticLinkCallback, this, boost::placeholders::_1, boost::placeholders::_2, boost::placeholders::_3),
            boost::bind(&BpSinkPattern::OnDeletedOpportunisticLinkCallback, this, boost::placeholders::_1, boost::placeholders::_2, boost::placeholders::_3));
    }

    if (outductsConfigPtr) {
        m_hasSendCapability = true;
        m_hasSendCapabilityOverTcpclBidirectionalInduct = false;
        //m_outductManager.LoadOutductsFromConfig(*outductsConfigPtr, myEid.nodeId, UINT16_MAX, maxBundleSizeBytes,
        //    boost::bind(&BpSinkPattern::WholeBundleReadyCallback, this, boost::placeholders::_1));
        if (!m_outductManager.LoadOutductsFromConfig(*outductsConfigPtr, myEid.nodeId, UINT16_MAX, maxBundleSizeBytes,
            boost::bind(&BpSinkPattern::WholeBundleReadyCallback, this, boost::placeholders::_1),
            boost::bind(&BpSinkPattern::OnFailedBundleVecSendCallback, this, boost::placeholders::_1, boost::placeholders::_2, boost::placeholders::_3),
            OnFailedBundleZmqSendCallback_t(), //BpSinkPattern only sends vec8 bundles (not zmq) so this will never be needed
            boost::bind(&BpSinkPattern::OnSuccessfulBundleSendCallback, this, boost::placeholders::_1, boost::placeholders::_2),
            boost::bind(&BpSinkPattern::OnOutductLinkStatusChangedCallback, this, boost::placeholders::_1, boost::placeholders::_2)))
        {
            return false;
        }
        while (!m_outductManager.AllReadyToForward()) {
            LOG_INFO(subprocess) << "waiting for outduct to be ready...";
            boost::this_thread::sleep(boost::posix_time::milliseconds(500));
        }
        m_currentlySendingBundleIdSet.reserve(outductsConfigPtr->m_outductElementConfigVector[0].maxNumberOfBundlesInPipeline);
    }
    else if ((inductsConfigPtr) && ((inductsConfigPtr->m_inductElementConfigVector[0].convergenceLayer == "tcpcl_v3") || (inductsConfigPtr->m_inductElementConfigVector[0].convergenceLayer == "tcpcl_v4"))) {
        m_hasSendCapability = true;
        m_hasSendCapabilityOverTcpclBidirectionalInduct = true;
        LOG_INFO(subprocess) << "this bpsink pattern detected tcpcl convergence layer which is bidirectional.. supporting custody transfer";
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
    ThreadNamer::SetIoServiceThreadName(m_ioService, "ioServiceBpSinkPattern");

    m_runningSenderThread = true;
    m_threadSenderReaderPtr = boost::make_unique<boost::thread>(
        boost::bind(&BpSinkPattern::SenderReaderThreadFunc, this)); //create and start the worker thread
    
    return true;
}



bool BpSinkPattern::Process(padded_vector_uint8_t & rxBuf, const std::size_t messageSize) {

    if (M_EXTRA_PROCESSING_TIME_MS) {
        boost::this_thread::sleep(boost::posix_time::milliseconds(M_EXTRA_PROCESSING_TIME_MS));
    }

    const uint8_t firstByte = rxBuf[0];
    const bool isBpVersion6 = (firstByte == 6);
    const bool isBpVersion7 = (firstByte == ((4U << 5) | 31U));  //CBOR major type 4, additional information 31 (Indefinite-Length Array)
    if (isBpVersion6) {
        static thread_local BundleViewV6 bv;
        if (!bv.LoadBundle(rxBuf.data(), rxBuf.size())) { //invalid bundle
            LOG_ERROR(subprocess) << "malformed bundle";
            return false;
        }
        Bpv6CbhePrimaryBlock & primary = bv.m_primaryBlockView.header;
        const cbhe_eid_t finalDestEid(primary.m_destinationEid);
        const cbhe_eid_t srcEid(primary.m_sourceNodeId);



        static constexpr BPV6_BUNDLEFLAG requiredPrimaryFlagsForEcho = BPV6_BUNDLEFLAG::SINGLETON;
        const bool isEcho = (((primary.m_bundleProcessingControlFlags & requiredPrimaryFlagsForEcho) == requiredPrimaryFlagsForEcho) && (finalDestEid == m_myEidEcho));
        if (isEcho && (!m_hasSendCapability)) {
            LOG_INFO(subprocess) << "a ping request was received but this bpsinkpattern does not have send capability.. ignoring bundle";
            return false;
        }

        if ((!isEcho) && (finalDestEid != m_myEid)) {
            LOG_ERROR(subprocess) << "bundle received has a destination that doesn't match my eid";
            return false;
        }

        //accept custody
        if (m_hasSendCapability) { //has bidirectionality
            static const BPV6_BUNDLEFLAG requiredPrimaryFlagsForCustody = BPV6_BUNDLEFLAG::SINGLETON | BPV6_BUNDLEFLAG::CUSTODY_REQUESTED;

            if (isEcho) {
                primary.m_destinationEid = primary.m_sourceNodeId;
                primary.m_sourceNodeId = m_myEidEcho;
                bv.m_primaryBlockView.SetManuallyModified();
                if (!bv.Render(messageSize + 500)) {
                    LOG_ERROR(subprocess) << "cannot render bpv6 echo bundle";
                    return false;
                }
                Forward_ThreadSafe(srcEid, bv.m_frontBuffer); //srcEid is the new destination
                return true;
            }
            else if ((primary.m_bundleProcessingControlFlags & requiredPrimaryFlagsForCustody) == requiredPrimaryFlagsForCustody) {
                const uint64_t newCtebCustodyId = m_nextCtebCustodyId++;
                m_mutexCtm.lock();
                const bool successfullyProcessedCustody = m_custodyTransferManagerPtr->ProcessCustodyOfBundle(bv, true,
                    newCtebCustodyId, BPV6_ACS_STATUS_REASON_INDICES::SUCCESS__NO_ADDITIONAL_INFORMATION,
                    m_custodySignalRfc5050RenderedBundleView);
                m_mutexCtm.unlock();
                if (!successfullyProcessedCustody) {
                    LOG_ERROR(subprocess) << "unable to process custody";
                    return false;
                }
                else {
                    if (m_custodySignalRfc5050RenderedBundleView.m_renderedBundle.size()) {
                        //send a classic rfc5050 custody signal due to acs disabled or bundle received has an invalid cteb
                        const cbhe_eid_t & custodySignalDestEid = m_custodySignalRfc5050RenderedBundleView.m_primaryBlockView.header.m_destinationEid;
                        Forward_ThreadSafe(custodySignalDestEid, m_custodySignalRfc5050RenderedBundleView.m_frontBuffer);
                    }
                    else if (m_custodyTransferManagerPtr->GetLargestNumberOfFills() > 100) {
                        boost::asio::post(m_ioService, boost::bind(&BpSinkPattern::SendAcsFromTimerThread, this));
                    }
                }
            }
        }

        std::vector<BundleViewV6::Bpv6CanonicalBlockView*> blocks;
        bv.GetCanonicalBlocksByType(BPV6_BLOCK_TYPE_CODE::PAYLOAD, blocks);
        if (blocks.size() != 1) {
            LOG_ERROR(subprocess) << "payload block not found";
            return false;
        }
        Bpv6CanonicalBlock & payloadBlock = *(blocks[0]->headerPtr);
        m_totalPayloadBytesRx += payloadBlock.m_blockTypeSpecificDataLength;
        m_totalBundleBytesRx += messageSize;
        ++m_totalBundlesVersion6Rx;

        if (!ProcessPayload(payloadBlock.m_blockTypeSpecificDataPtr, payloadBlock.m_blockTypeSpecificDataLength)) {
            LOG_ERROR(subprocess) << "ProcessPayload";
            return false;
        }

        #ifdef DO_STATS_LOGGING
            std::vector<hdtn::StatsLogger::metric_t> metrics;
            metrics.push_back(hdtn::StatsLogger::metric_t("bundle_source_to_sink_latency_s", primary.GetSecondsSinceCreate()));
            hdtn::StatsLogger::Log("bundle_source_to_sink_latency_s", metrics);
        #endif
    }
    else if (isBpVersion7) {
        static thread_local BundleViewV7 bv;
        if (!bv.LoadBundle(rxBuf.data(), rxBuf.size())) { //invalid bundle
            LOG_ERROR(subprocess) << "malformed bpv7 bundle";
            return false;
        }
        Bpv7CbhePrimaryBlock & primary = bv.m_primaryBlockView.header;
        const cbhe_eid_t finalDestEid = primary.m_destinationEid;
        const cbhe_eid_t srcEid = primary.m_sourceNodeId;

#ifdef BPSEC_SUPPORT_ENABLED
        //process acceptor and verifier roles
        if (!m_bpsecPimpl->m_bpSecPolicyManager.ProcessReceivedBundle(bv, m_bpsecPimpl->m_policyProcessingCtx)) {
            return false;
        }
#endif // BPSEC_SUPPORT_ENABLED


        static constexpr BPV7_BUNDLEFLAG requiredPrimaryFlagsForEcho = BPV7_BUNDLEFLAG::NO_FLAGS_SET;
        const bool isEcho = (((primary.m_bundleProcessingControlFlags & requiredPrimaryFlagsForEcho) == requiredPrimaryFlagsForEcho) && (finalDestEid == m_myEidEcho));
        if (isEcho && (!m_hasSendCapability)) {
            LOG_INFO(subprocess) << "a ping request was received but this bpsinkpattern does not have send capability.. ignoring bundle";
            return false;
        }

        if ((!isEcho) && (finalDestEid != m_myEid)) {
            LOG_ERROR(subprocess) << "bundle received has a destination that doesn't match my eid";
            return false;
        }

        
        //get previous node
        std::vector<BundleViewV7::Bpv7CanonicalBlockView*> blocks;
        bv.GetCanonicalBlocksByType(BPV7_BLOCK_TYPE_CODE::PREVIOUS_NODE, blocks);
        if (blocks.size() > 1) {
            LOG_ERROR(subprocess) << "version 7 bundle received has multiple previous node blocks";
            return false;
        }
        else if (blocks.size() == 1) { 
            if (Bpv7PreviousNodeCanonicalBlock* previousNodeBlockPtr = dynamic_cast<Bpv7PreviousNodeCanonicalBlock*>(blocks[0]->headerPtr.get())) {
                if (m_lastPreviousNode != previousNodeBlockPtr->m_previousNode) {
                    m_lastPreviousNode = previousNodeBlockPtr->m_previousNode;
                    LOG_INFO(subprocess) << "bp version 7 bundles coming in from previous node " << m_lastPreviousNode;
                }
                //update the previous node in case this is an echo
                previousNodeBlockPtr->m_previousNode = m_myEidEcho;
                blocks[0]->SetManuallyModified();
            }
            else {
                LOG_ERROR(subprocess) << "dynamic_cast to Bpv7PreviousNodeCanonicalBlock failed";
                return false;
            }
        }
        else if (isEcho) { //prepend new previous node block
            std::unique_ptr<Bpv7CanonicalBlock> blockPtr = boost::make_unique<Bpv7PreviousNodeCanonicalBlock>();
            Bpv7PreviousNodeCanonicalBlock & block = *(reinterpret_cast<Bpv7PreviousNodeCanonicalBlock*>(blockPtr.get()));

            block.m_blockProcessingControlFlags = BPV7_BLOCKFLAG::REMOVE_BLOCK_IF_IT_CANT_BE_PROCESSED;
            block.m_blockNumber = bv.GetNextFreeCanonicalBlockNumber();
            block.m_crcType = BPV7_CRC_TYPE::CRC32C;
            block.m_previousNode = m_myEidEcho;
            bv.PrependMoveCanonicalBlock(std::move(blockPtr));
        }

        //get hop count if exists
        bv.GetCanonicalBlocksByType(BPV7_BLOCK_TYPE_CODE::HOP_COUNT, blocks);
        if (blocks.size() > 1) {
            LOG_ERROR(subprocess) << "version 7 bundle received has multiple hop count blocks";
            return false;
        }
        else if (blocks.size() == 1) {
            if (Bpv7HopCountCanonicalBlock* hopCountBlockPtr = dynamic_cast<Bpv7HopCountCanonicalBlock*>(blocks[0]->headerPtr.get())) {
                //the hop count value SHOULD initially be zero and SHOULD be increased by 1 on each hop.
                const uint64_t newHopCount = hopCountBlockPtr->m_hopCount + 1;
                //When a bundle's hop count exceeds its
                //hop limit, the bundle SHOULD be deleted for the reason "hop limit
                //exceeded", following the bundle deletion procedure defined in
                //Section 5.10.
                //Hop limit MUST be in the range 1 through 255.
                if ((newHopCount > hopCountBlockPtr->m_hopLimit) || (newHopCount > 255)) {
                    LOG_WARNING(subprocess) << "dropping version 7 bundle with hop count " << newHopCount;
                    return false;
                }
                ++m_hopCounts[newHopCount];
                //update the hop count in case this is an echo
                hopCountBlockPtr->m_hopCount = newHopCount;
                blocks[0]->SetManuallyModified();
            }
            else {
                LOG_ERROR(subprocess) << "dynamic_cast to Bpv7HopCountCanonicalBlock failed";
                return false;
            }
        }

        bv.GetCanonicalBlocksByType(BPV7_BLOCK_TYPE_CODE::PRIORITY, blocks);
        if (blocks.size() > 1) {
            LOG_ERROR(subprocess) << "version 7 bundle received has multiple priority blocks";
            return false;
        }
        else if (blocks.size() == 1) {
            if (Bpv7PriorityCanonicalBlock* priorityBlockPtr = dynamic_cast<Bpv7PriorityCanonicalBlock*>(blocks[0]->headerPtr.get())) {
                m_bpv7Priority = BPV7_PRIORITY::INVALID;

                if (priorityBlockPtr->m_bundlePriority > BPV7_PRIORITY::MAX_PRIORITY) {
                    LOG_WARNING(subprocess) << "notice: dropping version 7 bundle with priority " << priorityBlockPtr->m_bundlePriority;
                    return false;
                }

                m_bpv7Priority = priorityBlockPtr->m_bundlePriority;
            }
            else {
                LOG_ERROR(subprocess) << "dynamic_cast to Bpv7PriorityCanonicalBlock failed";
                return false;
            }           
        }
        else {
            m_bpv7Priority = BPV7_PRIORITY::DEFAULT;
        }

        if (m_hasSendCapability) { //has bidirectionality
            if (isEcho) {
                primary.m_destinationEid = primary.m_sourceNodeId;
                primary.m_sourceNodeId = m_myEidEcho;
                bv.m_primaryBlockView.SetManuallyModified();
                if (!bv.Render(messageSize + 500)) {
                    LOG_ERROR(subprocess) << "cannot render bpv7 echo bundle";
                    return false;
                }
                Forward_ThreadSafe(srcEid, bv.m_frontBuffer); //srcEid is the new destination
                return true;
            }
        }

        //get payload block
        bv.GetCanonicalBlocksByType(BPV7_BLOCK_TYPE_CODE::PAYLOAD, blocks);

        if (blocks.size() != 1) {
            LOG_ERROR(subprocess) << "payload block not found";
            return false;
        }

        const uint64_t payloadDataLength = blocks[0]->headerPtr->m_dataLength;
        const uint8_t * payloadDataPtr = blocks[0]->headerPtr->m_dataPtr;
        m_totalPayloadBytesRx += payloadDataLength;
        m_totalBundleBytesRx += messageSize;
        ++m_totalBundlesVersion7Rx;

        if (!ProcessPayload(payloadDataPtr, payloadDataLength)) {
            LOG_ERROR(subprocess) << "ProcessPayload";
            return false;
        }

        #ifdef DO_STATS_LOGGING
            std::vector<hdtn::StatsLogger::metric_t> metrics;
            metrics.push_back(hdtn::StatsLogger::metric_t("bundle_source_to_sink_latency_ms", primary.GetMillisecondsSinceCreate()));
            hdtn::StatsLogger::Log("bundle_source_to_sink_latency_ms", metrics);
        #endif
    }

    return true;
}

void BpSinkPattern::WholeBundleReadyCallback(padded_vector_uint8_t & wholeBundleVec) {
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
        LOG_INFO(subprocess) << "timer stopped";
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
            double payloadRateMbps = (diffTotalPayloadBytesRx * 8.0) / (diff.total_microseconds());
            double bundleRateMbps = (diffBundleBytesRx * 8.0) / (diff.total_microseconds());
            double bundlesPerSecond = (diffBundlesRx * 1e6) / (diff.total_microseconds());

            static const boost::format fmtTemplate("Payload Only Rate: %0.4f Mbits/sec, Total Rate: %0.4f Mbits/sec, Bundles/sec: %0.4f");
            boost::format fmt(fmtTemplate);
            fmt % payloadRateMbps % bundleRateMbps % bundlesPerSecond;
            LOG_INFO(subprocess) << fmt.str();
        }
        
        m_lastPayloadBytesRx = totalPayloadBytesRx;
        m_lastBundleBytesRx = totalBundleBytesRx;
        m_lastBundlesRx = totalBundlesRx;
        m_lastPtime = finishedTime;
        m_timerTransferRateStats.expires_from_now(boost::posix_time::seconds(5));
        m_timerTransferRateStats.async_wait(boost::bind(&BpSinkPattern::TransferRate_TimerExpired, this, boost::asio::placeholders::error));
    }
    else {
        LOG_INFO(subprocess) << "transfer rate timer stopped";
    }
}

void BpSinkPattern::SendAcsFromTimerThread() {
    std::list<BundleViewV6> newAcsRenderedBundleViewList;
    m_mutexCtm.lock();
    const bool generatedSuccessfully = m_custodyTransferManagerPtr->GenerateAllAcsBundlesAndClear(newAcsRenderedBundleViewList);
    m_mutexCtm.unlock();
    if (generatedSuccessfully) {
        for (std::list<BundleViewV6>::iterator it = newAcsRenderedBundleViewList.begin(); it != newAcsRenderedBundleViewList.end(); ++it) {
            //send an acs custody signal due to acs send timer
            const cbhe_eid_t & custodySignalDestEid = it->m_primaryBlockView.header.m_destinationEid;
            Forward_ThreadSafe(custodySignalDestEid, it->m_frontBuffer);
        }
    }
}

void BpSinkPattern::OnNewOpportunisticLinkCallback(const uint64_t remoteNodeId, Induct* thisInductPtr, void* sinkPtr) {
    if (m_tcpclInductPtr = dynamic_cast<TcpclInduct*>(thisInductPtr)) {
        LOG_INFO(subprocess) << "New opportunistic link detected on Tcpcl induct for ipn:" << remoteNodeId << ".*";
        m_tcpclOpportunisticRemoteNodeId = remoteNodeId;
    }
    else if (m_tcpclInductPtr = dynamic_cast<TcpclV4Induct*>(thisInductPtr)) {
        LOG_INFO(subprocess) << "New opportunistic link detected on TcpclV4 induct for ipn:" << remoteNodeId << ".*";
        m_tcpclOpportunisticRemoteNodeId = remoteNodeId;
    }
    else if (StcpInduct* stcpInductPtr = dynamic_cast<StcpInduct*>(thisInductPtr)) {

    }
    else {
        LOG_ERROR(subprocess) << "Induct ptr cannot cast to TcpclInduct or TcpclV4Induct";
    }
}
void BpSinkPattern::OnDeletedOpportunisticLinkCallback(const uint64_t remoteNodeId, Induct* thisInductPtr, void* sinkPtrAboutToBeDeleted) {
    if (StcpInduct* stcpInductPtr = dynamic_cast<StcpInduct*>(thisInductPtr)) {

    }
    else {
        m_tcpclOpportunisticRemoteNodeId = 0;
        LOG_INFO(subprocess) << "Deleted opportunistic link on Tcpcl induct for ipn:" << remoteNodeId << ".*";
    }
}

bool BpSinkPattern::Forward_ThreadSafe(const cbhe_eid_t & destEid, padded_vector_uint8_t& bundleToMoveAndSend) {
    if (m_bundleToSendQueue.size() > 2000) { //todo
        return false;
    }
    m_mutexSendBundleQueue.lock();
    m_bundleToSendQueue.emplace(destEid, std::move(bundleToMoveAndSend));
    m_mutexSendBundleQueue.unlock();
    m_conditionVariableSenderReader.notify_one();
    return true;
}

void BpSinkPattern::SenderReaderThreadFunc() {

    ThreadNamer::SetThisThreadName("BpSinkPatternSenderReader");

    uint64_t m_nextBundleId = 0;
    cbhe_eid_t destEid;
    padded_vector_uint8_t bundleToSend;
    Outduct* outduct = NULL;
    uint64_t outductMaxBundlesInPipeline = 0;
    
    const uint64_t TIMEOUT_SECONDS = 3;
    const boost::posix_time::time_duration TIMEOUT_DURATION = boost::posix_time::seconds(TIMEOUT_SECONDS);
    uint64_t nextBundleId = 0;
    uint64_t thisBundleId = 0;
    if (!m_hasSendCapabilityOverTcpclBidirectionalInduct) { //outduct
        outduct = m_outductManager.GetOutductByOutductUuid(0);
        if (outduct) {
            outductMaxBundlesInPipeline = outduct->GetOutductMaxNumberOfBundlesInPipeline();
        }
    }
    while (m_runningSenderThread) {
        
        if (bundleToSend.size()) { //last bundle didn't send due to timeout, try to resend this
            //no work to do
        }
        else if(m_queueBundlesThatFailedToSend.size()) {
            boost::mutex::scoped_lock lock(m_mutexQueueBundlesThatFailedToSend);
            bundle_userdata_pair_t& bup = m_queueBundlesThatFailedToSend.front();
            bundleid_finaldesteid_pair_t& bfp = bup.second;
            thisBundleId = bfp.first;
            destEid = bfp.second;
            bundleToSend = std::move(bup.first);
            m_queueBundlesThatFailedToSend.pop();
        }
        else if (m_bundleToSendQueue.size()) {
            {
                boost::mutex::scoped_lock lock(m_mutexSendBundleQueue);
                if (m_bundleToSendQueue.empty()) {
                    continue;
                }
                desteid_bundle_pair_t& dbp = m_bundleToSendQueue.front();
                destEid = dbp.first;
                bundleToSend = std::move(dbp.second);
                m_bundleToSendQueue.pop();
            }
            thisBundleId = nextBundleId++;
        }
        else { //empty
            boost::mutex::scoped_lock senderReaderLock(m_mutexSendBundleQueue);
            if (m_runningSenderThread && m_bundleToSendQueue.empty()) { //lock mutex (above) before checking flag
                m_conditionVariableSenderReader.wait(senderReaderLock); // call lock.unlock() and blocks the current thread
            }
            //thread is now unblocked, and the lock is reacquired by invoking lock.lock()
            continue;
        }

        if (m_linkIsDown) {
            //note BpSource has no routing capability so it must send to the only connection available to it
            LOG_ERROR(subprocess) << "BpSinkPattern waiting for linkup event before sending queued bundles.. retrying in 1 second";
            boost::this_thread::sleep(boost::posix_time::seconds(1));
            continue;
        }

        if ((!m_hasSendCapabilityOverTcpclBidirectionalInduct) && (outduct != m_outductManager.GetOutductByFinalDestinationEid_ThreadSafe(destEid))) { //outduct
            LOG_ERROR(subprocess) << destEid << " does not match outduct.. dropping bundle to be sent";
            bundleToSend.clear();
            continue;
        }

        std::vector<uint8_t> bundleToSendUserData(sizeof(bundleid_finaldesteid_pair_t));
        bundleid_finaldesteid_pair_t* bundleToSendUserDataPairPtr = (bundleid_finaldesteid_pair_t*)bundleToSendUserData.data();
        bundleToSendUserDataPairPtr->first = thisBundleId;
        bundleToSendUserDataPairPtr->second = destEid;

        if (m_hasSendCapabilityOverTcpclBidirectionalInduct) {
            if (destEid.nodeId != m_tcpclOpportunisticRemoteNodeId) {
                LOG_WARNING(subprocess) << "node id " << destEid.nodeId << " does not match tcpcl opportunistic link node id " << m_tcpclOpportunisticRemoteNodeId;
                //bundleToSend.clear();
                //continue;
            }
            //else if (m_tcpclInductPtr) {
            //note BpSink has no routing capability so it must send to the only connection available to it
            if (m_tcpclInductPtr) {
                if (!m_tcpclInductPtr->ForwardOnOpportunisticLink(m_tcpclOpportunisticRemoteNodeId, bundleToSend, 3)) {
                    LOG_ERROR(subprocess) << "BpSinkPattern was unable to send a bundle for " << TIMEOUT_SECONDS << " seconds on the bidirectional tcpcl induct.. retrying in 1 second";
                    boost::this_thread::sleep(boost::posix_time::seconds(1));
                    continue;
                }
            }
            else {
                LOG_ERROR(subprocess) << "tcpcl induct does not exist to forward";
            }
        }
        else { //outduct for forwarding bundles
            boost::posix_time::ptime timeoutExpiry(boost::posix_time::special_values::not_a_date_time);
            bool timeout = false;
            while (m_runningSenderThread && (m_currentlySendingBundleIdSet.size() >= outductMaxBundlesInPipeline)) {
                if (timeoutExpiry == boost::posix_time::special_values::not_a_date_time) {
                    const boost::posix_time::ptime nowTime = boost::posix_time::microsec_clock::universal_time();
                    timeoutExpiry = nowTime + TIMEOUT_DURATION;
                }
                boost::mutex::scoped_lock waitingForBundlePipelineFreeLock(m_mutexCurrentlySendingBundleIdSet);
                if (m_runningSenderThread && (m_currentlySendingBundleIdSet.size() >= outductMaxBundlesInPipeline)) { //lock mutex (above) before checking condition
                    //Returns: false if the call is returning because the time specified by abs_time was reached, true otherwise.
                    if (!m_cvCurrentlySendingBundleIdSet.timed_wait(waitingForBundlePipelineFreeLock, timeoutExpiry)) {
                        timeout = true;
                        break;
                    }
                }
            }
            if (timeout) {
                LOG_ERROR(subprocess) << "BpSinkPattern was unable to send a bundle for " << TIMEOUT_SECONDS << " seconds on the outduct";
                continue;
            }
            if (!m_runningSenderThread) {
                LOG_ERROR(subprocess) << "BpSinkPattern terminating before all bundles sent";
                continue;
            }

            //insert bundleId right before forward
            m_mutexCurrentlySendingBundleIdSet.lock();
            m_currentlySendingBundleIdSet.insert(thisBundleId); //ok if already exists
            m_mutexCurrentlySendingBundleIdSet.unlock();

            if (!outduct->Forward(bundleToSend, std::move(bundleToSendUserData))) {
                LOG_ERROR(subprocess) << "BpSinkPattern unable to send bundle on the outduct.. retrying in 1 second";
                boost::this_thread::sleep(boost::posix_time::seconds(1));
            }
        }
    }

    LOG_INFO(subprocess) << "BpSinkPattern::SenderReaderThreadFunc thread exiting";

}

void BpSinkPattern::OnFailedBundleVecSendCallback(padded_vector_uint8_t& movableBundle, std::vector<uint8_t>& userData, uint64_t outductUuid) {
    bundleid_finaldesteid_pair_t* p = (bundleid_finaldesteid_pair_t*)userData.data();
    const uint64_t bundleId = p->first;
    LOG_INFO(subprocess) << "Bundle failed to send: id=" << bundleId << " bundle size=" << movableBundle.size();
    std::size_t sizeErased;
    {
        boost::mutex::scoped_lock lock(m_mutexQueueBundlesThatFailedToSend);
        boost::mutex::scoped_lock lock2(m_mutexCurrentlySendingBundleIdSet);
        m_queueBundlesThatFailedToSend.emplace(std::move(movableBundle), std::move(*p));
        sizeErased = m_currentlySendingBundleIdSet.erase(bundleId);
    }
    
    if (sizeErased == 0) {
        LOG_ERROR(subprocess) << "cannot find bundleId " << bundleId;
    }

    if (!m_linkIsDown) {
        LOG_INFO(subprocess) << "Setting link status to DOWN";
        m_linkIsDown = true;
    }
    m_cvCurrentlySendingBundleIdSet.notify_one();
}
void BpSinkPattern::OnSuccessfulBundleSendCallback(std::vector<uint8_t>& userData, uint64_t outductUuid) {
    bundleid_finaldesteid_pair_t* p = (bundleid_finaldesteid_pair_t*)userData.data();
    const uint64_t bundleId = p->first;

    m_mutexCurrentlySendingBundleIdSet.lock();
    const std::size_t sizeErased = m_currentlySendingBundleIdSet.erase(bundleId);
    m_mutexCurrentlySendingBundleIdSet.unlock();
    if (sizeErased == 0) {
        LOG_ERROR(subprocess) << "cannot find bundleId " << bundleId;
    }

    if (m_linkIsDown) {
        LOG_INFO(subprocess) << "Setting link status to UP";
        m_linkIsDown = false;
    }
    m_cvCurrentlySendingBundleIdSet.notify_one();
}
void BpSinkPattern::OnOutductLinkStatusChangedCallback(bool isLinkDownEvent, uint64_t outductUuid) {
    LOG_INFO(subprocess) << "OnOutductLinkStatusChangedCallback isLinkDownEvent:" << isLinkDownEvent << " outductUuid " << outductUuid;
    const bool linkIsAlreadyDown = m_linkIsDown;
    if (isLinkDownEvent && (!linkIsAlreadyDown)) {
        LOG_INFO(subprocess) << "Setting link status to DOWN";
    }
    else if ((!isLinkDownEvent) && linkIsAlreadyDown) {
        LOG_INFO(subprocess) << "Setting link status to UP";
    }
    m_linkIsDown = isLinkDownEvent;
    m_cvCurrentlySendingBundleIdSet.notify_one();
}
