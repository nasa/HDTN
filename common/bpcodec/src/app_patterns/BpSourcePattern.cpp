/**
 * @file BpSourcePattern.cpp
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

#include <string.h>
#include "app_patterns/BpSourcePattern.h"
#include "Logger.h"
#include <boost/lexical_cast.hpp>
#include <boost/format.hpp>
#include <memory>
#include <boost/make_unique.hpp>
#include "Uri.h"

#include <time.h>
#include "TimestampUtil.h"
#include "codec/bpv6.h"
#include "TcpclInduct.h"
#include "TcpclV4Induct.h"
#include "StcpInduct.h"
#include "codec/BundleViewV7.h"
#include "ThreadNamer.h"
#include "BinaryConversions.h"

#ifdef BPSEC_SUPPORT_ENABLED
#include "InitializationVectors.h"
#include "BpSecPolicyManager.h"
#endif

static constexpr hdtn::Logger::SubProcess subprocess = hdtn::Logger::SubProcess::none;

static void CustomCleanupPaddedVecUint8(void* data, void* hint) {
    delete static_cast<padded_vector_uint8_t*>(hint);
}

BpSourcePattern::BpSourcePattern() : m_running(false) {

}

BpSourcePattern::~BpSourcePattern() {
    Stop();
    LOG_INFO(subprocess) << "totalNonAdminRecordBpv6PayloadBytesRx: " << m_totalNonAdminRecordBpv6PayloadBytesRx;
    LOG_INFO(subprocess) << "totalNonAdminRecordBpv6BundleBytesRx: " << m_totalNonAdminRecordBpv6BundleBytesRx;
    LOG_INFO(subprocess) << "totalNonAdminRecordBpv6BundlesRx: " << m_totalNonAdminRecordBpv6BundlesRx;

    LOG_INFO(subprocess) << "totalNonAdminRecordBpv7PayloadBytesRx: " << m_totalNonAdminRecordBpv7PayloadBytesRx;
    LOG_INFO(subprocess) << "totalNonAdminRecordBpv7BundleBytesRx: " << m_totalNonAdminRecordBpv7BundleBytesRx;
    LOG_INFO(subprocess) << "totalNonAdminRecordBpv7BundlesRx: " << m_totalNonAdminRecordBpv7BundlesRx;
    for (std::size_t i = 0; i < m_hopCounts.size(); ++i) {
        if (m_hopCounts[i] != 0) {
            LOG_INFO(subprocess) << "received " << m_hopCounts[i] << " bundles with a hop count of " << i << ".";
        }
    }
}

void BpSourcePattern::Stop() {
    m_running = false; //thread stopping criteria
    //only lock one mutex at a time to prevent deadlock (a worker may call this function on an error condition)
    //lock then unlock each thread's mutex to prevent a missed notify after setting thread stopping criteria above
    m_waitingForRxBundleBeforeNextTxMutex.lock();
    m_waitingForRxBundleBeforeNextTxMutex.unlock();
    m_waitingForRxBundleBeforeNextTxConditionVariable.notify_one();
    //
    m_mutexCurrentlySendingBundleIdSet.lock();
    m_mutexCurrentlySendingBundleIdSet.unlock();
    m_cvCurrentlySendingBundleIdSet.notify_one(); //break out of the timed_wait (wait_until)


//    boost::this_thread::sleep(boost::posix_time::seconds(1));
    if(m_bpSourcePatternThreadPtr) {
        try {
            m_bpSourcePatternThreadPtr->join();
            m_bpSourcePatternThreadPtr.reset(); //delete it
        }
        catch (const boost::thread_resource_error&) {
            LOG_ERROR(subprocess) << "error stopping BpSourcePattern thread";
        }
    }

    m_outductManager.StopAllOutducts();
    if (Outduct * outduct = m_outductManager.GetOutductByOutductUuid(0)) {
        outduct->GetOutductFinalStats(m_outductFinalStats);
    }
    
}

void BpSourcePattern::Start(OutductsConfig_ptr & outductsConfigPtr, InductsConfig_ptr & inductsConfigPtr,
    const boost::filesystem::path& bpSecConfigFilePath,
    bool custodyTransferUseAcs,
    const cbhe_eid_t & myEid, double bundleRate, const cbhe_eid_t & finalDestEid, const uint64_t myCustodianServiceId,
    const unsigned int bundleSendTimeoutSeconds, const uint64_t bundleLifetimeMilliseconds, const uint64_t bundlePriority,
    const bool requireRxBundleBeforeNextTx, const bool forceDisableCustody, const bool useBpVersion7) {
    if (m_running) {
        LOG_ERROR(subprocess) << "BpSourcePattern::Start called while BpSourcePattern is already running";
        return;
    }
    m_bundleSendTimeoutSeconds = bundleSendTimeoutSeconds;
    m_bundleSendTimeoutTimeDuration = boost::posix_time::seconds(m_bundleSendTimeoutSeconds);
    m_bundleLifetimeMilliseconds = bundleLifetimeMilliseconds;
    m_bundlePriority = bundlePriority;
    m_finalDestinationEid = finalDestEid;
    m_myEid = myEid;
    m_myCustodianServiceId = myCustodianServiceId;
    m_myCustodianEid.Set(m_myEid.nodeId, myCustodianServiceId);
    m_myCustodianEidUriString = Uri::GetIpnUriString(m_myEid.nodeId, myCustodianServiceId);
    m_detectedNextCustodianSupportsCteb = false;
    m_requireRxBundleBeforeNextTx = requireRxBundleBeforeNextTx;
    m_useBpVersion7 = useBpVersion7;
    m_linkIsDown = false;
    m_nextBundleId = 0;
    m_hopCounts.assign(256, 0);
    m_lastPreviousNode.Set(0, 0);

    m_totalNonAdminRecordBpv6PayloadBytesRx = 0;
    m_totalNonAdminRecordBpv6BundleBytesRx = 0;
    m_totalNonAdminRecordBpv6BundlesRx = 0;

    m_totalNonAdminRecordBpv7PayloadBytesRx = 0;
    m_totalNonAdminRecordBpv7BundleBytesRx = 0;
    m_totalNonAdminRecordBpv7BundlesRx = 0;

    m_tcpclInductPtr = NULL;

    OutductOpportunisticProcessReceivedBundleCallback_t outductOpportunisticProcessReceivedBundleCallback; //"null" function by default
    m_custodyTransferUseAcs = custodyTransferUseAcs;
    if (inductsConfigPtr) {
        m_currentlySendingBundleIdSet.reserve(1024); //todo
        m_useCustodyTransfer = true;
        m_inductManager.LoadInductsFromConfig(boost::bind(&BpSourcePattern::WholeRxBundleReadyCallback, this, boost::placeholders::_1),
            *inductsConfigPtr, m_myEid.nodeId, UINT16_MAX, 1000000, //todo 1MB max bundle size on custody signals
            boost::bind(&BpSourcePattern::OnNewOpportunisticLinkCallback, this, boost::placeholders::_1, boost::placeholders::_2, boost::placeholders::_3),
            boost::bind(&BpSourcePattern::OnDeletedOpportunisticLinkCallback, this, boost::placeholders::_1, boost::placeholders::_2, boost::placeholders::_3));
    }
    else if ((outductsConfigPtr)
        && ((outductsConfigPtr->m_outductElementConfigVector[0].convergenceLayer == "tcpcl_v3") || (outductsConfigPtr->m_outductElementConfigVector[0].convergenceLayer == "tcpcl_v4"))
        && (outductsConfigPtr->m_outductElementConfigVector[0].tcpclAllowOpportunisticReceiveBundles)
        )
    {
        m_useCustodyTransfer = true;
        outductOpportunisticProcessReceivedBundleCallback = boost::bind(&BpSourcePattern::WholeRxBundleReadyCallback, this, boost::placeholders::_1);
        LOG_INFO(subprocess) << "this bpsource pattern detected tcpcl convergence layer which is bidirectional.. supporting custody transfer";
    }
    else {
        m_useCustodyTransfer = false;
    }

    if (forceDisableCustody) { //for bping which needs to receive echo packets instead of admin records
        m_useCustodyTransfer = false;
    }

    
    if (outductsConfigPtr) {
        m_currentlySendingBundleIdSet.reserve(outductsConfigPtr->m_outductElementConfigVector[0].maxNumberOfBundlesInPipeline);
        m_useInductForSendingBundles = false;
        if (!m_outductManager.LoadOutductsFromConfig(*outductsConfigPtr, m_myEid.nodeId, UINT16_MAX,
            10000000, //todo 10MB max rx opportunistic bundle
            outductOpportunisticProcessReceivedBundleCallback,
            boost::bind(&BpSourcePattern::OnFailedBundleVecSendCallback, this, boost::placeholders::_1, boost::placeholders::_2, boost::placeholders::_3, boost::placeholders::_4),
            OnFailedBundleZmqSendCallback_t(), //bpsourcepattern only sends vec8 bundles (not zmq) so this will never be needed
            boost::bind(&BpSourcePattern::OnSuccessfulBundleSendCallback, this, boost::placeholders::_1, boost::placeholders::_2),
            boost::bind(&BpSourcePattern::OnOutductLinkStatusChangedCallback, this, boost::placeholders::_1, boost::placeholders::_2)))
        {
            return;
        }
    }
    else {
        m_useInductForSendingBundles = true;
    }

    m_running = true;
    m_allOutductsReady = false;
   
    
    m_bpSourcePatternThreadPtr = boost::make_unique<boost::thread>(
        boost::bind(&BpSourcePattern::BpSourcePatternThreadFunc, this, bundleRate, bpSecConfigFilePath)); //create and start the worker thread
    


}


void BpSourcePattern::BpSourcePatternThreadFunc(double bundleRate, const boost::filesystem::path& bpSecConfigFilePath) {

    ThreadNamer::SetThisThreadName("BpSourcePattern");

#ifdef BPSEC_SUPPORT_ENABLED
    BpSecConfig_ptr bpSecConfigPtr;
    BpSecPolicyManager bpSecPolicyManager;
    BpSecPolicyProcessingContext policyProcessingCtx;
    
    if (!bpSecConfigFilePath.empty()) {
        bpSecConfigPtr = BpSecConfig::CreateFromJsonFilePath(bpSecConfigFilePath);
        if (!bpSecConfigPtr) {
            LOG_FATAL(subprocess) << "Error loading BpSec config file: " << bpSecConfigFilePath;
            return;
        }
        else if (!bpSecPolicyManager.LoadFromConfig(*bpSecConfigPtr)) {
            return;
        }
        LOG_INFO(subprocess) << "BpSec enabled.  Using config file: " << bpSecConfigFilePath;
    }
#else
    if (!bpSecConfigFilePath.empty()) {
        LOG_FATAL(subprocess) << "A BpSec config file was specified but BpSec support was not enabled at compile time";
        return;
    }
#endif
    while (m_running) {
        if (m_useInductForSendingBundles) {
            LOG_INFO(subprocess) << "Waiting for Tcpcl opportunistic link on the induct to become available for forwarding bundles...";
            boost::this_thread::sleep(boost::posix_time::milliseconds(1000));
            if (m_tcpclInductPtr) {
                LOG_INFO(subprocess) << "Induct opportunistic link ready to forward";
                break;
            }
        }
        else {
            LOG_INFO(subprocess) << "Waiting for Outduct to become ready to forward...";
            boost::this_thread::sleep(boost::posix_time::milliseconds(1000));
            if (m_outductManager.AllReadyToForward()) {
                LOG_INFO(subprocess) << "Outduct ready to forward";
                break;
            }
        }
    }
    if (!m_running) {
        LOG_INFO(subprocess) << "Terminated before a connection could be made";
        return;
    }
    boost::this_thread::sleep(boost::posix_time::milliseconds(1000)); //todo make sure connection from hdtn to bpgen induct
    m_allOutductsReady = true;

    #define BP_MSG_BUFSZ             (65536 * 100) //todo

    boost::posix_time::time_duration sleepValTimeDuration = boost::posix_time::special_values::neg_infin;
    Outduct* outduct = m_outductManager.GetOutductByOutductUuid(0);
    uint64_t outductMaxBundlesInPipeline = 0;
    if (outduct) {
        outductMaxBundlesInPipeline = outduct->GetOutductMaxNumberOfBundlesInPipeline();
    }
    if(bundleRate) {
        LOG_INFO(subprocess) << "Generating up to " << bundleRate << " bundles / second";
        const double sval = 1000000.0 / bundleRate;   // sleep val in usec
        ////sval *= BP_MSG_NBUF;
        const uint64_t sValU64 = static_cast<uint64_t>(sval);
        sleepValTimeDuration = boost::posix_time::microseconds(sValU64);
        LOG_INFO(subprocess) << "Sleeping for " << sValU64 << " usec between bursts";
    }
    else {
        if (m_useInductForSendingBundles) {
            if (m_tcpclInductPtr) {
                LOG_INFO(subprocess) << "bundle rate of zero used.. Going as fast as possible by allowing up to ??? unacked bundles";
            }
            else {
                LOG_ERROR(subprocess) << "null induct";
                return;
            }
        }
        else {
            if (outduct) {
                if (outduct != m_outductManager.GetOutductByFinalDestinationEid_ThreadSafe(m_finalDestinationEid)) {
                    LOG_ERROR(subprocess) << "outduct 0 does not support finalDestinationEid " << m_finalDestinationEid;
                    return;
                }
                LOG_INFO(subprocess) << "bundle rate of zero used.. Going as fast as possible by allowing up to " << outductMaxBundlesInPipeline << " unacked bundles";
            }
            else {
                LOG_ERROR(subprocess) << "null outduct";
                return;
            }
        }
        
    }


    std::size_t numEventsTooManyUnackedBundles = 0;
    m_bundleCount = 0;
    m_numRfc5050CustodyTransfers = 0;
    m_numAcsCustodyTransfers = 0;
    m_numAcsPacketsReceived = 0;
    uint64_t bundle_data = 0;
    uint64_t raw_data = 0;

   

    uint64_t lastTimeRfc5050 = 0;
    uint64_t lastMillisecondsSinceStartOfYear2000 = 0;
    
    uint64_t seq = 0;
    
    uint64_t nextCtebCustodyId = 0;


    boost::mutex localMutex;
    boost::mutex::scoped_lock lock(localMutex);

    boost::asio::io_service ioService;
    boost::asio::deadline_timer deadlineTimer(ioService, sleepValTimeDuration);
    padded_vector_uint8_t bundleToSend;
    uint8_t* bundleToSendStartPtr = NULL;
    boost::posix_time::ptime startTime = boost::posix_time::microsec_clock::universal_time();
    bool isGeneratingNewBundles = true;
    bool inWaitForNewBundlesState = false;
    static constexpr std::size_t MAX_NUM_BPV7_BLOCK_TYPE_CODES = static_cast<std::size_t>(BPV7_BLOCK_TYPE_CODE::RESERVED_MAX_BLOCK_TYPES);
    uint8_t bpv7BlockTypeToManuallyAssignedBlockNumberLut[MAX_NUM_BPV7_BLOCK_TYPE_CODES] = { 0 };
    bpv7BlockTypeToManuallyAssignedBlockNumberLut[static_cast<std::size_t>(BPV7_BLOCK_TYPE_CODE::HOP_COUNT)] = 2;
    bpv7BlockTypeToManuallyAssignedBlockNumberLut[static_cast<std::size_t>(BPV7_BLOCK_TYPE_CODE::PRIORITY)] = 3;
    bpv7BlockTypeToManuallyAssignedBlockNumberLut[static_cast<std::size_t>(BPV7_BLOCK_TYPE_CODE::PAYLOAD)] = 1; //must be 1
    
#ifdef BPSEC_SUPPORT_ENABLED
    const BpSecPolicy* bpSecPolicyPtr = bpSecPolicyManager.FindPolicy(m_myEid, m_myEid, m_finalDestinationEid, BPSEC_ROLE::SOURCE);
    if (bpSecPolicyPtr) {
        if (!BpSecPolicyManager::PopulateTargetArraysForSecuritySource(
            bpv7BlockTypeToManuallyAssignedBlockNumberLut,
            policyProcessingCtx,
            *bpSecPolicyPtr))
        {
            return;
        }
    }
#endif // BPSEC_SUPPORT_ENABLED
    zmq::message_t zmqMessageToSendWrapper;
    BundleViewV7 bv7;
    BundleViewV6 bv6;
    while (m_running) { //keep thread alive if running
                
        if(bundleRate) {
            boost::system::error_code ec;
            deadlineTimer.wait(ec);
            if(ec) {
                LOG_ERROR(subprocess) << "timer error: " << ec.message();
                return;
            }
            deadlineTimer.expires_at(deadlineTimer.expires_at() + sleepValTimeDuration);
        }
        
        uint64_t payloadSizeBytes;
        uint64_t bundleLength;
        uint64_t bundleId;
        if (m_queueBundlesThatFailedToSend.size()) {
            m_mutexQueueBundlesThatFailedToSend.lock();
            bundle_userdata_pair_t& bup = m_queueBundlesThatFailedToSend.front();
            bundleid_payloadsize_pair_t& bpp = bup.second;
            bundleId = bpp.first;
            payloadSizeBytes = bpp.second;
            bundleToSend = std::move(bup.first);
            m_queueBundlesThatFailedToSend.pop();
            m_mutexQueueBundlesThatFailedToSend.unlock();
            bundleLength = bundleToSend.size();
        }
        else if (inWaitForNewBundlesState) {
            static const boost::posix_time::time_duration timeout(boost::posix_time::milliseconds(250));
            inWaitForNewBundlesState = !TryWaitForDataAvailable(timeout);
            if (!inWaitForNewBundlesState) {
                LOG_INFO(subprocess) << "data available";
            }
            continue;
        }
        else if (isGeneratingNewBundles) {
            payloadSizeBytes = GetNextPayloadLength_Step1();
            if (payloadSizeBytes == 0) {
                LOG_INFO(subprocess) << "payloadSizeBytes == 0... out of work.. waiting for all bundles to fully transmit before exiting";
                isGeneratingNewBundles = false;
                inWaitForNewBundlesState = false;
                continue;
            }
            else if (payloadSizeBytes == UINT64_MAX) {
                LOG_INFO(subprocess) << "waiting for new bundles to become available...";
                inWaitForNewBundlesState = true;
                continue;
            }
            bundleId = m_nextBundleId++;
            if (m_useBpVersion7) {
                bv7.Reset(); //when reusing bv, it must be Reset since not calling any Load functions (loading calls Reset)
                Bpv7CbhePrimaryBlock& primary = bv7.m_primaryBlockView.header;
                //primary.SetZero();
                primary.m_bundleProcessingControlFlags = BPV7_BUNDLEFLAG::NOFRAGMENT;  //All BP endpoints identified by ipn-scheme endpoint IDs are singleton endpoints.
                primary.m_sourceNodeId = m_myEid;
                primary.m_destinationEid = m_finalDestinationEid;
                if (m_useCustodyTransfer) { //not supported yet
                    primary.m_bundleProcessingControlFlags |= BPV7_BUNDLEFLAG::RECEPTION_STATUS_REPORTS_REQUESTED; //??
                    primary.m_reportToEid.Set(m_myEid.nodeId, m_myCustodianServiceId);
                }
                else {
                    primary.m_reportToEid.Set(0, 0);
                }
                primary.m_creationTimestamp.SetTimeFromNow();
                if (primary.m_creationTimestamp.millisecondsSinceStartOfYear2000 == lastMillisecondsSinceStartOfYear2000) {
                    ++seq;
                }
                else {
                    seq = 0;
                }
                lastMillisecondsSinceStartOfYear2000 = primary.m_creationTimestamp.millisecondsSinceStartOfYear2000;
                primary.m_creationTimestamp.sequenceNumber = seq;
                primary.m_lifetimeMilliseconds = m_bundleLifetimeMilliseconds;
                primary.m_crcType = BPV7_CRC_TYPE::CRC32C;
                bv7.m_primaryBlockView.SetManuallyModified();

                //add hop count block (before payload last block)
                {
                    std::unique_ptr<Bpv7CanonicalBlock> blockPtr;
                    static constexpr std::size_t blockTypeCodeAsSizeT = static_cast<std::size_t>(BPV7_BLOCK_TYPE_CODE::HOP_COUNT);
                    if (bv7.m_blockNumberToRecycledCanonicalBlockArray[blockTypeCodeAsSizeT]) {
                        //std::cout << "recycle hop count\n";
                        blockPtr = std::move(bv7.m_blockNumberToRecycledCanonicalBlockArray[blockTypeCodeAsSizeT]);
                    }
                    else {
                        blockPtr = boost::make_unique<Bpv7HopCountCanonicalBlock>();
                    }
                    Bpv7HopCountCanonicalBlock& block = *(reinterpret_cast<Bpv7HopCountCanonicalBlock*>(blockPtr.get()));

                    block.m_blockProcessingControlFlags = BPV7_BLOCKFLAG::REMOVE_BLOCK_IF_IT_CANT_BE_PROCESSED; //something for checking against
                    block.m_blockNumber = bpv7BlockTypeToManuallyAssignedBlockNumberLut[static_cast<std::size_t>(BPV7_BLOCK_TYPE_CODE::HOP_COUNT)];
                    block.m_crcType = BPV7_CRC_TYPE::CRC32C;
                    block.m_hopLimit = 100; //Hop limit MUST be in the range 1 through 255.
                    block.m_hopCount = 0; //the hop count value SHOULD initially be zero and SHOULD be increased by 1 on each hop.
                    bv7.AppendMoveCanonicalBlock(std::move(blockPtr));
                }

                // Append priority block
                {
                    std::unique_ptr<Bpv7CanonicalBlock> blockPtr;
                    static constexpr std::size_t blockTypeCodeAsSizeT = static_cast<std::size_t>(BPV7_BLOCK_TYPE_CODE::PRIORITY);
                    if (bv7.m_blockNumberToRecycledCanonicalBlockArray[blockTypeCodeAsSizeT]) {
                        //std::cout << "recycle priority\n";
                        blockPtr = std::move(bv7.m_blockNumberToRecycledCanonicalBlockArray[blockTypeCodeAsSizeT]);
                    }
                    else {
                        blockPtr = boost::make_unique<Bpv7PriorityCanonicalBlock>();
                    }
                    Bpv7PriorityCanonicalBlock& block = *(reinterpret_cast<Bpv7PriorityCanonicalBlock*>(blockPtr.get()));

                    block.m_blockProcessingControlFlags = BPV7_BLOCKFLAG::REMOVE_BLOCK_IF_IT_CANT_BE_PROCESSED; //something for checking against
                    block.m_blockNumber = bpv7BlockTypeToManuallyAssignedBlockNumberLut[static_cast<std::size_t>(BPV7_BLOCK_TYPE_CODE::PRIORITY)];
                    block.m_crcType = BPV7_CRC_TYPE::CRC32C;
                    block.m_bundlePriority = m_bundlePriority; // MUST be 0 = Bulk, 1 = Normal, or 2 = Expedited
                    bv7.AppendMoveCanonicalBlock(std::move(blockPtr));
                }

                //append payload block (must be last block)
                {
                    std::unique_ptr<Bpv7CanonicalBlock> payloadBlockPtr;
                    static constexpr std::size_t blockTypeCodeAsSizeT = static_cast<std::size_t>(BPV7_BLOCK_TYPE_CODE::PAYLOAD);
                    if (bv7.m_blockNumberToRecycledCanonicalBlockArray[blockTypeCodeAsSizeT]) {
                        //std::cout << "recycle payload\n";
                        payloadBlockPtr = std::move(bv7.m_blockNumberToRecycledCanonicalBlockArray[blockTypeCodeAsSizeT]);
                    }
                    else {
                        payloadBlockPtr = boost::make_unique<Bpv7CanonicalBlock>();
                    }
                    Bpv7CanonicalBlock& payloadBlock = *payloadBlockPtr;
                    //payloadBlock.SetZero();

                    payloadBlock.m_blockTypeCode = BPV7_BLOCK_TYPE_CODE::PAYLOAD;
                    payloadBlock.m_blockProcessingControlFlags = BPV7_BLOCKFLAG::NO_FLAGS_SET;
                    payloadBlock.m_blockNumber = bpv7BlockTypeToManuallyAssignedBlockNumberLut[static_cast<std::size_t>(BPV7_BLOCK_TYPE_CODE::PAYLOAD)]; //must be 1
                    payloadBlock.m_crcType = BPV7_CRC_TYPE::CRC32C;
                    payloadBlock.m_dataLength = payloadSizeBytes;
                    payloadBlock.m_dataPtr = NULL; //NULL will preallocate (won't copy or compute crc, user must do that manually below)
                    bv7.AppendMoveCanonicalBlock(std::move(payloadBlockPtr));
                }

                //render bundle to the front buffer
                if (!bv7.Render(payloadSizeBytes + 1000)) {
                    LOG_ERROR(subprocess) << "error rendering bpv7 bundle";
                    return;
                }

                BundleViewV7::Bpv7CanonicalBlockView& payloadBlockView = bv7.m_listCanonicalBlockView.back(); //payload block must be the last block

                //manually copy data to preallocated space and compute crc
                if (!CopyPayload_Step2(payloadBlockView.headerPtr->m_dataPtr)) { //m_dataPtr now points to new allocated or copied data within the serialized block (from after Render())
                    LOG_ERROR(subprocess) << "copy payload error";
                    m_running = false;
                    continue;
                }
#ifdef BPSEC_SUPPORT_ENABLED
                if (bpSecPolicyPtr) {
                    if (!BpSecPolicyManager::ProcessOutgoingBundle(bv7, policyProcessingCtx, *bpSecPolicyPtr, m_myEid)) {
                        m_running = false;
                        continue;
                    }
                }
                const bool payloadAlreadyHasCrcComputed = (bpSecPolicyPtr && bpSecPolicyPtr->m_bcbTargetsPayloadBlock);
#else
                static constexpr bool payloadAlreadyHasCrcComputed = false;
#endif
                if (payloadAlreadyHasCrcComputed) {
                    //payload already has crc recomputed because encrypt automatically recomputes crcs for the blocks it targets
                }
                else {
                    payloadBlockView.headerPtr->RecomputeCrcAfterDataModification(
                        (uint8_t*)payloadBlockView.actualSerializedBlockPtr.data(),
                        payloadBlockView.actualSerializedBlockPtr.size()); //recompute crc
                }
                //move the bundle out of bundleView
                bundleToSend = std::move(bv7.m_frontBuffer);
                bundleLength = bv7.m_renderedBundle.size();
                if ((bundleToSend.data() != ((uint8_t*)bv7.m_renderedBundle.data())) || (bundleToSend.size() != bv7.m_renderedBundle.size())) {
                    //bundle was rendered in place, wrap inside a zmq message
                    bundleToSendStartPtr = (uint8_t*)bv7.m_renderedBundle.data();
                    if (bundleToSend.data() > bundleToSendStartPtr) {
                        const std::uintptr_t diff = bundleToSend.data() - bundleToSendStartPtr;
                        if (diff > PaddedMallocatorConstants::PADDING_ELEMENTS_BEFORE) {
                            LOG_FATAL(subprocess) << "used " << ((unsigned int)diff) 
                                << " bytes of prepadded data from render in place, exceeding the limit of "
                                << PaddedMallocatorConstants::PADDING_ELEMENTS_BEFORE << " bytes!";
                        }
                        else {
                            //LOG_DEBUG(subprocess) << "using " << ((unsigned int)diff) << " bytes of prepadded data from render in place";
                        }
                    }
                    /*{
                        std::string hexString;
                        BinaryConversions::BytesToHexString(bundleToSendStartPtr, bundleLength, hexString);
                        boost::to_lower(hexString);
                        LOG_DEBUG(subprocess) << "bundle start ptr: " << ((uintptr_t)bundleToSendStartPtr) << " len=" << bundleLength;
                        LOG_DEBUG(subprocess) << "bundle encrypted: " << hexString;
                    }*/
                    padded_vector_uint8_t* rxBufRawPointer = new padded_vector_uint8_t(std::move(bundleToSend));
                    zmqMessageToSendWrapper.rebuild(bundleToSendStartPtr, bundleLength, CustomCleanupPaddedVecUint8, rxBufRawPointer);
                }
            }
            else { //bp version 6
                bv6.Reset(); //when reusing bv, it must be Reset since not calling any Load functions (loading calls Reset)
                Bpv6CbhePrimaryBlock& primary = bv6.m_primaryBlockView.header;
                //primary.SetZero();

                primary.m_bundleProcessingControlFlags = 
                    (BPV6_BUNDLEFLAG(m_bundlePriority << 7) & BPV6_BUNDLEFLAG::PRIORITY_BIT_MASK) |
                    BPV6_BUNDLEFLAG::SINGLETON |
                    BPV6_BUNDLEFLAG::NOFRAGMENT;

                if (m_useCustodyTransfer) {
                    primary.m_bundleProcessingControlFlags |= BPV6_BUNDLEFLAG::CUSTODY_REQUESTED;
                    primary.m_custodianEid.Set(m_myEid.nodeId, m_myCustodianServiceId);
                    primary.m_reportToEid.Set(m_myEid.nodeId, m_myCustodianServiceId);

                }
                primary.m_sourceNodeId = m_myEid;
                primary.m_destinationEid = m_finalDestinationEid;

                primary.m_creationTimestamp.SetTimeFromNow();
                if (primary.m_creationTimestamp.secondsSinceStartOfYear2000 == lastTimeRfc5050) {
                    ++seq;
                }
                else {
                    seq = 0;
                }
                lastTimeRfc5050 = primary.m_creationTimestamp.secondsSinceStartOfYear2000;
                primary.m_creationTimestamp.sequenceNumber = seq;
                primary.m_lifetimeSeconds = m_bundleLifetimeMilliseconds / 1000;
                bv6.m_primaryBlockView.SetManuallyModified();




                if (m_useCustodyTransfer) {
                    if (m_custodyTransferUseAcs) {
                        const uint64_t ctebCustodyId = nextCtebCustodyId++;
                        //add cteb
                        {
                            std::unique_ptr<Bpv6CanonicalBlock> blockPtr;
                            static constexpr std::size_t blockTypeCodeAsSizeT = static_cast<std::size_t>(BPV6_BLOCK_TYPE_CODE::CUSTODY_TRANSFER_ENHANCEMENT);
                            if (bv6.m_blockNumberToRecycledCanonicalBlockArray[blockTypeCodeAsSizeT]) {
                                //std::cout << "recycle cteb\n";
                                blockPtr = std::move(bv6.m_blockNumberToRecycledCanonicalBlockArray[blockTypeCodeAsSizeT]);
                            }
                            else {
                                blockPtr = boost::make_unique<Bpv6CustodyTransferEnhancementBlock>();
                            }
                            Bpv6CustodyTransferEnhancementBlock& block = *(reinterpret_cast<Bpv6CustodyTransferEnhancementBlock*>(blockPtr.get()));
                            //block.SetZero();

                            block.m_blockProcessingControlFlags = BPV6_BLOCKFLAG::NO_FLAGS_SET; //something for checking against
                            block.m_custodyId = ctebCustodyId;
                            block.m_ctebCreatorCustodianEidString = m_myCustodianEidUriString;
                            bv6.AppendMoveCanonicalBlock(std::move(blockPtr));
                        }

                        m_mutexCtebSet.lock();
                        FragmentSet::InsertFragment(m_outstandingCtebCustodyIdsFragmentSet, FragmentSet::data_fragment_t(ctebCustodyId, ctebCustodyId));
                        m_mutexCtebSet.unlock();
                    }
                    //always prepare for rfc5050 style custody transfer if next hop doesn't support acs
                    if (!m_detectedNextCustodianSupportsCteb) { //prevent map from filling up endlessly if we're using cteb
                        cbhe_bundle_uuid_nofragment_t uuid = primary.GetCbheBundleUuidNoFragmentFromPrimary();
                        m_mutexBundleUuidSet.lock();
                        const bool success = m_cbheBundleUuidSet.insert(uuid).second;
                        m_mutexBundleUuidSet.unlock();
                        if (!success) {
                            LOG_ERROR(subprocess) << "error insert bundle uuid";
                            m_running = false;
                            continue;
                        }
                    }

                }

                //append payload block (must be last block)
                {
                    std::unique_ptr<Bpv6CanonicalBlock> payloadBlockPtr;
                    static constexpr std::size_t blockTypeCodeAsSizeT = static_cast<std::size_t>(BPV6_BLOCK_TYPE_CODE::PAYLOAD);
                    if (bv6.m_blockNumberToRecycledCanonicalBlockArray[blockTypeCodeAsSizeT]) {
                        //std::cout << "recycle payload\n";
                        payloadBlockPtr = std::move(bv6.m_blockNumberToRecycledCanonicalBlockArray[blockTypeCodeAsSizeT]);
                    }
                    else {
                        payloadBlockPtr = boost::make_unique<Bpv6CanonicalBlock>();
                    }
                    Bpv6CanonicalBlock& payloadBlock = *payloadBlockPtr;
                    //payloadBlock.SetZero();

                    payloadBlock.m_blockTypeCode = BPV6_BLOCK_TYPE_CODE::PAYLOAD;
                    payloadBlock.m_blockProcessingControlFlags = BPV6_BLOCKFLAG::NO_FLAGS_SET;
                    payloadBlock.m_blockTypeSpecificDataLength = payloadSizeBytes;
                    payloadBlock.m_blockTypeSpecificDataPtr = NULL; //NULL will preallocate (won't copy or compute crc, user must do that manually below)
                    bv6.AppendMoveCanonicalBlock(std::move(payloadBlockPtr));
                }

                //render bundle to the front buffer
                if (!bv6.Render(payloadSizeBytes + 1000)) {
                    LOG_ERROR(subprocess) << "error rendering bpv7 bundle";
                    return;
                }

                BundleViewV6::Bpv6CanonicalBlockView& payloadBlockView = bv6.m_listCanonicalBlockView.back(); //payload block is the last block in this case

                //manually copy data to preallocated space and compute crc
                if (!CopyPayload_Step2(payloadBlockView.headerPtr->m_blockTypeSpecificDataPtr)) { //m_dataPtr now points to new allocated or copied data within the serialized block (from after Render())
                    LOG_ERROR(subprocess) << "copy payload error";
                    m_running = false;
                    continue;
                }

                //move the bundle out of bundleView
                bundleToSend = std::move(bv6.m_frontBuffer);
                bundleLength = bundleToSend.size();

            }
        }
        else if (m_currentlySendingBundleIdSet.empty()) { //natural stopping criteria
            LOG_INFO(subprocess) << "all bundles generated and fully sent";
            m_running = false;
            continue;
        }
        else { //bundles are still being sent
            boost::mutex::scoped_lock waitingForBundlePipelineFreeLock(m_mutexCurrentlySendingBundleIdSet);
            if (m_running && m_currentlySendingBundleIdSet.size()) { //lock mutex (above) before checking condition
                m_cvCurrentlySendingBundleIdSet.wait(waitingForBundlePipelineFreeLock);
            }
            continue;
        }
        
        std::vector<uint8_t> bundleToSendUserData(sizeof(bundleid_payloadsize_pair_t));
        bundleid_payloadsize_pair_t* bundleToSendUserDataPairPtr = (bundleid_payloadsize_pair_t*)bundleToSendUserData.data();
        bundleToSendUserDataPairPtr->first = bundleId;
        bundleToSendUserDataPairPtr->second = payloadSizeBytes;
        
        //send message
        do { //try to send at least once before terminating
            if (m_linkIsDown) {
                //note BpSource has no routing capability so it must send to the only connection available to it
                LOG_ERROR(subprocess) << "Waiting for linkup event.. retrying in 1 second";
                boost::this_thread::sleep(boost::posix_time::seconds(1));
            }
            else { //link is not down, proceed
                if (m_requireRxBundleBeforeNextTx) {
                    m_waitingForRxBundleBeforeNextTxMutex.lock();
                    m_isWaitingForRxBundleBeforeNextTx = true; //set this now before forwarding bundle
                    m_waitingForRxBundleBeforeNextTxMutex.unlock();
                }
                bool successForward = false;


                if (!m_useInductForSendingBundles) { //outduct for forwarding bundles
                    boost::posix_time::ptime timeoutExpiry(boost::posix_time::special_values::not_a_date_time);
                    bool timeout = false;
                    while (m_currentlySendingBundleIdSet.size() >= outductMaxBundlesInPipeline) { //don't check m_running because do while loop checks at end
                        if (timeoutExpiry == boost::posix_time::special_values::not_a_date_time) {
                            const boost::posix_time::ptime nowTime = boost::posix_time::microsec_clock::universal_time();
                            timeoutExpiry = nowTime + m_bundleSendTimeoutTimeDuration;
                        }
                        boost::mutex::scoped_lock waitingForBundlePipelineFreeLock(m_mutexCurrentlySendingBundleIdSet);
                        if (m_currentlySendingBundleIdSet.size() >= outductMaxBundlesInPipeline) { //lock mutex (above) before checking condition
                            //Returns: false if the call is returning because the time specified by abs_time was reached, true otherwise.
                            if (!m_cvCurrentlySendingBundleIdSet.timed_wait(waitingForBundlePipelineFreeLock, timeoutExpiry)) {
                                timeout = true;
                                break;
                            }
                        }
                    }
                    if (timeout) {
                        LOG_ERROR(subprocess) << "Unable to send a bundle for " << m_bundleSendTimeoutSeconds << " seconds on the outduct.. retrying in 1 second";
                        boost::this_thread::sleep(boost::posix_time::seconds(1));
                    }
                    else { //proceed to send bundle (no timeout)
                        //insert bundleId right before forward
                        m_mutexCurrentlySendingBundleIdSet.lock();
                        m_currentlySendingBundleIdSet.insert(bundleId); //ok if already exists
                        m_mutexCurrentlySendingBundleIdSet.unlock();

                        if (bundleToSendStartPtr) { //bundle was rendered in place, wrap inside a zmq message
                            if (!outduct->Forward(zmqMessageToSendWrapper, std::move(bundleToSendUserData))) {
                                LOG_ERROR(subprocess) << "BpSourcePattern unable to send bundle on the outduct.. retrying in 1 second";
                                boost::this_thread::sleep(boost::posix_time::seconds(1));
                            }
                            else {
                                successForward = true;
                                bundleToSendStartPtr = NULL;
                            }
                        }
                        else {
                            if (!outduct->Forward(bundleToSend, std::move(bundleToSendUserData))) {
                                LOG_ERROR(subprocess) << "BpSourcePattern unable to send bundle on the outduct.. retrying in 1 second";
                                boost::this_thread::sleep(boost::posix_time::seconds(1));
                            }
                            else {
                                successForward = true;
                            }
                        }
                    }
                }
                else { //induct for forwarding bundles
                    if (!m_tcpclInductPtr->ForwardOnOpportunisticLink(m_tcpclOpportunisticRemoteNodeId, bundleToSend, m_bundleSendTimeoutSeconds)) {
                        //note BpSource has no routing capability so it must send to the only connection available to it
                        LOG_ERROR(subprocess) << "Unable to send a bundle for " << m_bundleSendTimeoutSeconds << " seconds on the opportunistic induct.. retrying in 1 second";
                        boost::this_thread::sleep(boost::posix_time::seconds(1));
                    }
                    else {
                        successForward = true;
                    }
                }
                if (successForward) { //success forward
                    if (bundleToSend.size() != 0) {
                        LOG_ERROR(subprocess) << "bundleToSend was not moved in Forward";
                        LOG_ERROR(subprocess) << "bundleToSend.size() : " << bundleToSend.size();
                    }
                    ++m_bundleCount;
                    bundle_data += payloadSizeBytes;     // payload data
                    raw_data += bundleLength; // bundle overhead + payload data
                    {
                        boost::mutex::scoped_lock waitingForRxBundleBeforeNextTxLock(m_waitingForRxBundleBeforeNextTxMutex);
                        while (m_requireRxBundleBeforeNextTx && m_running && m_isWaitingForRxBundleBeforeNextTx) { //lock mutex (above) before checking flag m_running and m_isWaitingForRxBundleBeforeNextTx
                            m_waitingForRxBundleBeforeNextTxConditionVariable.wait(waitingForRxBundleBeforeNextTxLock);
                        }
                    }
                    break;
                }
            } //end if link is not down
        } while (m_running);
    }
    //todo m_outductManager.StopAllOutducts(); //wait for all pipelined bundles to complete before getting a finishedTime
    boost::posix_time::ptime finishedTime = boost::posix_time::microsec_clock::universal_time();

    LOG_INFO(subprocess) << "bundle_count: " << m_bundleCount;
    LOG_INFO(subprocess) << "bundle_data (payload data): " << bundle_data << " bytes";
    LOG_INFO(subprocess) << "raw_data (bundle overhead + payload data): " << raw_data << " bytes";
    if (bundleRate == 0) {
        LOG_INFO(subprocess) << "numEventsTooManyUnackedBundles: " << numEventsTooManyUnackedBundles;
    }

    if (m_useInductForSendingBundles) {
        LOG_INFO(subprocess) << "Keeping Tcpcl Induct Opportunistic link open for 4 seconds to finish sending";
        boost::this_thread::sleep(boost::posix_time::seconds(4));
    }
    else if (Outduct * outduct = m_outductManager.GetOutductByOutductUuid(0)) {
        if (outduct->GetConvergenceLayerName() == "ltp_over_udp") {
            LOG_INFO(subprocess) << "Keeping UDP open for 4 seconds to acknowledge report segments";
            boost::this_thread::sleep(boost::posix_time::seconds(4));
        }
    }
    if (m_useCustodyTransfer) {
        uint64_t lastNumCustodyTransfers = UINT64_MAX;
        while (true) {
            const uint64_t totalCustodyTransfers = std::max(m_numRfc5050CustodyTransfers, m_numAcsCustodyTransfers);
            if (totalCustodyTransfers == m_bundleCount) {
                LOG_INFO(subprocess) << "Received all custody transfers";
                break;
            }
            else if (totalCustodyTransfers != lastNumCustodyTransfers) {
                lastNumCustodyTransfers = totalCustodyTransfers;
                LOG_INFO(subprocess) << "BpSourcePattern waiting for an additional 10 seconds to receive custody transfers";
                boost::this_thread::sleep(boost::posix_time::seconds(10));
            }
            else {
                LOG_INFO(subprocess) << "Received no custody transfers for the last 10 seconds.. exiting";
                break;
            }
        }
    }

    if (m_currentlySendingBundleIdSet.size()) { 
        LOG_ERROR(subprocess) << "bundles were still being sent before termination";
    }

    LOG_INFO(subprocess) << "m_numRfc5050CustodyTransfers: " << m_numRfc5050CustodyTransfers;
    LOG_INFO(subprocess) << "m_numAcsCustodyTransfers: " << m_numAcsCustodyTransfers;
    LOG_INFO(subprocess) << "m_numAcsPacketsReceived: " << m_numAcsPacketsReceived;

    boost::posix_time::time_duration diff = finishedTime - startTime;
    {
        const double rateMbps = (bundle_data * 8.0) / (diff.total_microseconds());
        static const boost::format fmtTemplate("Sent bundle_data (payload data) at %0.4f Mbits/sec");
        boost::format fmt(fmtTemplate);
        fmt % rateMbps;
        LOG_INFO(subprocess) << fmt.str();
    }
    {
        const double rateMbps = (raw_data * 8.0) / (diff.total_microseconds());
        static const boost::format fmtTemplate("Sent raw_data (bundle overhead + payload data) at %0.4f Mbits/sec");
        boost::format fmt(fmtTemplate);
        fmt % rateMbps;
        LOG_INFO(subprocess) << fmt.str();
    }

    LOG_INFO(subprocess) << "BpSourcePatternThreadFunc thread exiting";
}

void BpSourcePattern::WholeRxBundleReadyCallback(padded_vector_uint8_t & wholeBundleVec) {
    //if more than 1 Induct, must protect shared resources with mutex.  Each Induct has
    //its own processing thread that calls this callback
    const uint8_t firstByte = wholeBundleVec[0];
    const bool isBpVersion6 = (firstByte == 6);
    const bool isBpVersion7 = (firstByte == ((4U << 5) | 31U));  //CBOR major type 4, additional information 31 (Indefinite-Length Array)
    if (isBpVersion6) {
        BundleViewV6 bv;
        if (!bv.LoadBundle(wholeBundleVec.data(), wholeBundleVec.size())) {
            LOG_ERROR(subprocess) << "malformed Bpv6 BpSourcePattern received";
            return;
        }
        //check primary
        const Bpv6CbhePrimaryBlock & primary = bv.m_primaryBlockView.header;
        const cbhe_eid_t & receivedFinalDestinationEid = primary.m_destinationEid;
        static const BPV6_BUNDLEFLAG requiredPrimaryFlagsForCustody = BPV6_BUNDLEFLAG::ADMINRECORD; // | BPV6_BUNDLEFLAG::SINGLETON | BPV6_BUNDLEFLAG::NOFRAGMENT
        if ((primary.m_bundleProcessingControlFlags & requiredPrimaryFlagsForCustody) != requiredPrimaryFlagsForCustody) { //assume non-admin-record bundle (perhaps a bpecho bundle)


            if (receivedFinalDestinationEid != m_myEid) {
                LOG_ERROR(subprocess) << "BpSourcePattern received a bundle with final destination " << receivedFinalDestinationEid
                    << " that does not match this destination " << m_myEid;
                return;
            }

            std::vector<BundleViewV6::Bpv6CanonicalBlockView*> blocks;
            bv.GetCanonicalBlocksByType(BPV6_BLOCK_TYPE_CODE::PAYLOAD, blocks);
            if (blocks.size() != 1) {
                LOG_ERROR(subprocess) << "BpSourcePattern received a non-admin-record bundle with no payload block";
                return;
            }
            Bpv6CanonicalBlock & payloadBlock = *(blocks[0]->headerPtr);
            m_totalNonAdminRecordBpv6PayloadBytesRx += payloadBlock.m_blockTypeSpecificDataLength;
            m_totalNonAdminRecordBpv6BundleBytesRx += bv.m_renderedBundle.size();
            ++m_totalNonAdminRecordBpv6BundlesRx;

            if (!ProcessNonAdminRecordBundlePayload(payloadBlock.m_blockTypeSpecificDataPtr, payloadBlock.m_blockTypeSpecificDataLength)) {
                LOG_ERROR(subprocess) << "ProcessNonAdminRecordBundlePayload";
                return;
            }
            m_waitingForRxBundleBeforeNextTxMutex.lock();
            m_isWaitingForRxBundleBeforeNextTx = false;
            m_waitingForRxBundleBeforeNextTxMutex.unlock();
            m_waitingForRxBundleBeforeNextTxConditionVariable.notify_one();
        }
        else { //admin record

            if (receivedFinalDestinationEid != m_myCustodianEid) {
                LOG_ERROR(subprocess) << "BpSourcePattern received an admin record bundle with final destination "
                    << Uri::GetIpnUriString(receivedFinalDestinationEid.nodeId, receivedFinalDestinationEid.serviceId)
                    << " that does not match this custodial destination " << Uri::GetIpnUriString(m_myCustodianEid.nodeId, m_myCustodianEid.serviceId);
                return;
            }

            std::vector<BundleViewV6::Bpv6CanonicalBlockView*> blocks;
            bv.GetCanonicalBlocksByType(BPV6_BLOCK_TYPE_CODE::PAYLOAD, blocks);
            if (blocks.size() != 1) {
                LOG_ERROR(subprocess) << "BpSourcePattern received an admin-record bundle with no payload block";
                return;
            }
            Bpv6AdministrativeRecord* adminRecordBlockPtr = dynamic_cast<Bpv6AdministrativeRecord*>(blocks[0]->headerPtr.get());
            if (adminRecordBlockPtr == NULL) {
                LOG_ERROR(subprocess) << "BpSourcePattern cannot cast payload block to admin record";
                return;
            }
            const BPV6_ADMINISTRATIVE_RECORD_TYPE_CODE adminRecordType = adminRecordBlockPtr->m_adminRecordTypeCode;
            if (adminRecordType == BPV6_ADMINISTRATIVE_RECORD_TYPE_CODE::AGGREGATE_CUSTODY_SIGNAL) {
                m_detectedNextCustodianSupportsCteb = true;
                ++m_numAcsPacketsReceived;
                //check acs
                Bpv6AdministrativeRecordContentAggregateCustodySignal * acsPtr = dynamic_cast<Bpv6AdministrativeRecordContentAggregateCustodySignal*>(adminRecordBlockPtr->m_adminRecordContentPtr.get());
                if (acsPtr == NULL) {
                    LOG_ERROR(subprocess) << "BpSourcePattern cannot cast admin record content to Bpv6AdministrativeRecordContentAggregateCustodySignal";
                    return;
                }
                Bpv6AdministrativeRecordContentAggregateCustodySignal & acs = *(reinterpret_cast<Bpv6AdministrativeRecordContentAggregateCustodySignal*>(acsPtr));
                if (!acs.DidCustodyTransferSucceed()) {
                    LOG_ERROR(subprocess) << "acs custody transfer failed with reason code " << acs.GetReasonCode();
                    return;
                }

                m_mutexCtebSet.lock();
                for (FragmentSet::data_fragment_set_t::const_iterator it = acs.m_custodyIdFills.cbegin(); it != acs.m_custodyIdFills.cend(); ++it) {
                    m_numAcsCustodyTransfers += (it->endIndex + 1) - it->beginIndex;
                    FragmentSet::RemoveFragment(m_outstandingCtebCustodyIdsFragmentSet, *it);
                }
                m_mutexCtebSet.unlock();
            }
            else if (adminRecordType == BPV6_ADMINISTRATIVE_RECORD_TYPE_CODE::CUSTODY_SIGNAL) { //rfc5050 style custody transfer
                Bpv6AdministrativeRecordContentCustodySignal * csPtr = dynamic_cast<Bpv6AdministrativeRecordContentCustodySignal*>(adminRecordBlockPtr->m_adminRecordContentPtr.get());
                if (csPtr == NULL) {
                    LOG_ERROR(subprocess) << "BpSourcePattern cannot cast admin record content to Bpv6AdministrativeRecordContentCustodySignal";
                    return;
                }
                Bpv6AdministrativeRecordContentCustodySignal & cs = *(reinterpret_cast<Bpv6AdministrativeRecordContentCustodySignal*>(csPtr));
                if (!cs.DidCustodyTransferSucceed()) {
                    LOG_ERROR(subprocess) << "rfc5050 custody transfer failed with reason code " << cs.GetReasonCode();
                    return;
                }
                if (cs.m_isFragment) {
                    LOG_ERROR(subprocess) << "custody signal with fragmentation received";
                    return;
                }
                cbhe_bundle_uuid_nofragment_t uuid;
                if (!Uri::ParseIpnUriString(cs.m_bundleSourceEid, uuid.srcEid.nodeId, uuid.srcEid.serviceId)) {
                    LOG_ERROR(subprocess) << "custody signal with bad ipn string";
                    return;
                }
                uuid.creationSeconds = cs.m_copyOfBundleCreationTimestamp.secondsSinceStartOfYear2000;
                uuid.sequence = cs.m_copyOfBundleCreationTimestamp.sequenceNumber;
                m_mutexBundleUuidSet.lock();
                const bool success = (m_cbheBundleUuidSet.erase(uuid) != 0);
                m_mutexBundleUuidSet.unlock();
                if (!success) {
                    LOG_ERROR(subprocess) << "rfc5050 custody signal received but bundle uuid not found";
                    return;
                }
                ++m_numRfc5050CustodyTransfers;
            }
            else if(adminRecordType == BPV6_ADMINISTRATIVE_RECORD_TYPE_CODE::BUNDLE_STATUS_REPORT) {
                Bpv6AdministrativeRecordContentBundleStatusReport * srPtr = dynamic_cast<Bpv6AdministrativeRecordContentBundleStatusReport*>(adminRecordBlockPtr->m_adminRecordContentPtr.get());
                if(srPtr == NULL) {
                    LOG_ERROR(subprocess) << "BpSourcePattern cannot cast admin record content to Bpv6AdministrativeRecordContentBundleStatusReport";
                    return;
                }
                Bpv6AdministrativeRecordContentBundleStatusReport & sr = *(reinterpret_cast<Bpv6AdministrativeRecordContentBundleStatusReport*>(srPtr));

                if(static_cast<bool>(sr.m_statusFlags & BPV6_BUNDLE_STATUS_REPORT_STATUS_FLAGS::REPORTING_NODE_DELETED_BUNDLE) && sr.m_reasonCode == BPV6_BUNDLE_STATUS_REPORT_REASON_CODES::LIFETIME_EXPIRED) {

                    static const boost::format fmtTemplate("Received bundle deleted due to lifetime expired for { %s t: %d n: %d }");
                    boost::format fmt(fmtTemplate);
                    fmt % sr.m_bundleSourceEid.c_str()
                        % sr.m_copyOfBundleCreationTimestamp.secondsSinceStartOfYear2000
                        % sr.m_copyOfBundleCreationTimestamp.sequenceNumber;
                    LOG_INFO(subprocess) << fmt.str();
                }
                else {
                    LOG_ERROR(subprocess) << "Received unknown status report";
                }

            }
            else {
                LOG_ERROR(subprocess) << "unknown admin record type";
                return;
            }
        }
    }
    else if (isBpVersion7) {
        BundleViewV7 bv;
        if (!bv.LoadBundle(wholeBundleVec.data(), wholeBundleVec.size())) {
            LOG_ERROR(subprocess) << "malformed Bpv7 BpSourcePattern received";
            return;
        }
        Bpv7CbhePrimaryBlock & primary = bv.m_primaryBlockView.header;
        const cbhe_eid_t finalDestEid = primary.m_destinationEid;
        const cbhe_eid_t srcEid = primary.m_sourceNodeId;



        //get previous node
        std::vector<BundleViewV7::Bpv7CanonicalBlockView*> blocks;
        bv.GetCanonicalBlocksByType(BPV7_BLOCK_TYPE_CODE::PREVIOUS_NODE, blocks);
        if (blocks.size() > 1) {
            LOG_ERROR(subprocess) << "BpSourcePattern::Process: version 7 bundle received has multiple previous node blocks";
            return;
        }
        else if (blocks.size() == 1) {
            if (Bpv7PreviousNodeCanonicalBlock* previousNodeBlockPtr = dynamic_cast<Bpv7PreviousNodeCanonicalBlock*>(blocks[0]->headerPtr.get())) {
                if (m_lastPreviousNode != previousNodeBlockPtr->m_previousNode) {
                    m_lastPreviousNode = previousNodeBlockPtr->m_previousNode;
                    LOG_INFO(subprocess) << "bp version 7 bundles coming in from previous node " << m_lastPreviousNode;
                }
            }
            else {
                LOG_ERROR(subprocess) << "BpSourcePattern::Process: dynamic_cast to Bpv7PreviousNodeCanonicalBlock failed";
                return;
            }
        }

        //get hop count if exists
        bv.GetCanonicalBlocksByType(BPV7_BLOCK_TYPE_CODE::HOP_COUNT, blocks);
        if (blocks.size() > 1) {
            LOG_ERROR(subprocess) << "BpSourcePattern::Process: version 7 bundle received has multiple hop count blocks";
            return;
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
                    LOG_WARNING(subprocess) << "notice: BpSourcePattern::Process dropping version 7 bundle with hop count " << newHopCount;
                    return;
                }
                ++m_hopCounts[newHopCount];
            }
            else {
                LOG_ERROR(subprocess) << "BpSourcePattern::Process: dynamic_cast to Bpv7HopCountCanonicalBlock failed";
                return;
            }
        }

        //get payload block
        bv.GetCanonicalBlocksByType(BPV7_BLOCK_TYPE_CODE::PAYLOAD, blocks);

        if (blocks.size() != 1) {
            LOG_ERROR(subprocess) << "BpSourcePattern::Process: Bpv7 payload block not found";
            return;
        }
        Bpv7CanonicalBlock & payloadBlock = *(blocks[0]->headerPtr);
        const uint64_t payloadDataLength = payloadBlock.m_dataLength;
        const uint8_t * payloadDataPtr = payloadBlock.m_dataPtr;
        m_totalNonAdminRecordBpv7PayloadBytesRx += payloadDataLength;
        m_totalNonAdminRecordBpv7BundleBytesRx += bv.m_renderedBundle.size();;
        ++m_totalNonAdminRecordBpv7BundlesRx;

        if (!ProcessNonAdminRecordBundlePayload(payloadDataPtr, payloadDataLength)) {
            LOG_ERROR(subprocess) << "ProcessNonAdminRecordBundlePayload";
            return;
        }
        m_waitingForRxBundleBeforeNextTxMutex.lock();
        m_isWaitingForRxBundleBeforeNextTx = false;
        m_waitingForRxBundleBeforeNextTxMutex.unlock();
        m_waitingForRxBundleBeforeNextTxConditionVariable.notify_one();
    }
}

bool BpSourcePattern::ProcessNonAdminRecordBundlePayload(const uint8_t * data, const uint64_t size) {
    return true;
}

void BpSourcePattern::OnNewOpportunisticLinkCallback(const uint64_t remoteNodeId, Induct* thisInductPtr, void* sinkPtr) {
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
        LOG_ERROR(subprocess) << "BpSourcePattern::OnNewOpportunisticLinkCallback: Induct ptr cannot cast to TcpclInduct or TcpclV4Induct";
    }
}
void BpSourcePattern::OnDeletedOpportunisticLinkCallback(const uint64_t remoteNodeId, Induct* thisInductPtr, void* sinkPtrAboutToBeDeleted) {
    if (StcpInduct* stcpInductPtr = dynamic_cast<StcpInduct*>(thisInductPtr)) {

    }
    else {
        m_tcpclOpportunisticRemoteNodeId = 0;
        LOG_INFO(subprocess) << "Deleted opportunistic link on Tcpcl induct for ipn:" << remoteNodeId << ".*";
    }
}

void BpSourcePattern::OnFailedBundleVecSendCallback(padded_vector_uint8_t& movableBundle, std::vector<uint8_t>& userData, uint64_t outductUuid, bool successCallbackCalled) {
    if (successCallbackCalled) { //ltp sender with sessions from disk enabled
        LOG_ERROR(subprocess) << "OnFailedBundleVecSendCallback called, dropping bundle for now";
    }
    else {
        bundleid_payloadsize_pair_t* p = (bundleid_payloadsize_pair_t*)userData.data();
        const uint64_t bundleId = p->first;
        LOG_WARNING(subprocess) << "Bundle failed to send: id=" << bundleId << " bundle size=" << movableBundle.size();
        std::size_t sizeErased;
        {
            boost::mutex::scoped_lock lock(m_mutexQueueBundlesThatFailedToSend);
            boost::mutex::scoped_lock lock2(m_mutexCurrentlySendingBundleIdSet);
            m_queueBundlesThatFailedToSend.emplace(std::move(movableBundle), std::move(*p));
            sizeErased = m_currentlySendingBundleIdSet.erase(bundleId);
        }
        if (sizeErased == 0) {
            LOG_ERROR(subprocess) << "BpSourcePattern::OnFailedBundleVecSendCallback: cannot find bundleId " << bundleId;
        }

        if (!m_linkIsDown) {
            LOG_INFO(subprocess) << "Setting link status to DOWN";
            m_linkIsDown = true;
        }
        m_cvCurrentlySendingBundleIdSet.notify_one();
    }
}
void BpSourcePattern::OnSuccessfulBundleSendCallback(std::vector<uint8_t>& userData, uint64_t outductUuid) {
    bundleid_payloadsize_pair_t* p = (bundleid_payloadsize_pair_t*)userData.data();
    const uint64_t bundleId = p->first;
    
    m_mutexCurrentlySendingBundleIdSet.lock();
    const std::size_t sizeErased = m_currentlySendingBundleIdSet.erase(bundleId);
    m_mutexCurrentlySendingBundleIdSet.unlock();
    if (sizeErased == 0) {
        LOG_ERROR(subprocess) << "OnSuccessfulBundleSendCallback: cannot find bundleId " << bundleId;
    }

    if (m_linkIsDown) {
        LOG_INFO(subprocess) << "Setting link status to UP";
        m_linkIsDown = false;
    }
    m_cvCurrentlySendingBundleIdSet.notify_one();
}
void BpSourcePattern::OnOutductLinkStatusChangedCallback(bool isLinkDownEvent, uint64_t outductUuid) {
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

bool BpSourcePattern::TryWaitForDataAvailable(const boost::posix_time::time_duration& timeout) {
    //default behavior (if not overloaded in child class) is to return true so that
    //GetNextPayloadLength_Step1() will return 0 and close the class,
    //although this parent function should never be called.
    return true;
}
