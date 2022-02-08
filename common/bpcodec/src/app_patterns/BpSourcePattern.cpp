#include <string.h>
#include <iostream>
#include "app_patterns/BpSourcePattern.h"
#include <boost/lexical_cast.hpp>
#include <boost/make_shared.hpp>
#include <boost/make_unique.hpp>
#include "Uri.h"

#include <time.h>
#include "TimestampUtil.h"
#include "codec/bpv6.h"
#include "TcpclInduct.h"
#include "TcpclV4Induct.h"
#include "codec/BundleViewV7.h"

BpSourcePattern::BpSourcePattern() : m_running(false) {

}

BpSourcePattern::~BpSourcePattern() {
    Stop();
    std::cout << "totalNonAdminRecordPayloadBytesRx: " << m_totalNonAdminRecordPayloadBytesRx << "\n";
    std::cout << "totalNonAdminRecordBundleBytesRx: " << m_totalNonAdminRecordBundleBytesRx << "\n";
    std::cout << "totalNonAdminRecordBundlesRx: " << m_totalNonAdminRecordBundlesRx << "\n";
}

void BpSourcePattern::Stop() {
    m_running = false;
//    boost::this_thread::sleep(boost::posix_time::seconds(1));
    if(m_bpSourcePatternThreadPtr) {
        m_bpSourcePatternThreadPtr->join();
        m_bpSourcePatternThreadPtr.reset(); //delete it
    }

    m_outductManager.StopAllOutducts();
    if (Outduct * outduct = m_outductManager.GetOutductByOutductUuid(0)) {
        outduct->GetOutductFinalStats(m_outductFinalStats);
    }
    
}

void BpSourcePattern::Start(OutductsConfig_ptr & outductsConfigPtr, InductsConfig_ptr & inductsConfigPtr, bool custodyTransferUseAcs,
    const cbhe_eid_t & myEid, uint32_t bundleRate, const cbhe_eid_t & finalDestEid, const uint64_t myCustodianServiceId,
    const bool requireRxBundleBeforeNextTx, const bool forceDisableCustody, const bool useBpVersion7) {
    if (m_running) {
        std::cerr << "error: BpSourcePattern::Start called while BpSourcePattern is already running" << std::endl;
        return;
    }
    m_finalDestinationEid = finalDestEid;
    m_myEid = myEid;
    m_myCustodianServiceId = myCustodianServiceId;
    m_myCustodianEid.Set(m_myEid.nodeId, myCustodianServiceId);
    m_myCustodianEidUriString = Uri::GetIpnUriString(m_myEid.nodeId, myCustodianServiceId);
    m_detectedNextCustodianSupportsCteb = false;
    m_requireRxBundleBeforeNextTx = requireRxBundleBeforeNextTx;
    m_useBpVersion7 = useBpVersion7;

    m_totalNonAdminRecordPayloadBytesRx = 0;
    m_totalNonAdminRecordBundleBytesRx = 0;
    m_totalNonAdminRecordBundlesRx = 0;

    m_tcpclInductPtr = NULL;

    OutductOpportunisticProcessReceivedBundleCallback_t outductOpportunisticProcessReceivedBundleCallback; //"null" function by default
    m_custodyTransferUseAcs = custodyTransferUseAcs;
    if (inductsConfigPtr) {
        m_useCustodyTransfer = true;
        m_inductManager.LoadInductsFromConfig(boost::bind(&BpSourcePattern::WholeRxBundleReadyCallback, this, boost::placeholders::_1),
            *inductsConfigPtr, m_myEid.nodeId, UINT16_MAX, 1000000, //todo 1MB max bundle size on custody signals
            boost::bind(&BpSourcePattern::OnNewOpportunisticLinkCallback, this, boost::placeholders::_1, boost::placeholders::_2),
            boost::bind(&BpSourcePattern::OnDeletedOpportunisticLinkCallback, this, boost::placeholders::_1));
    }
    else if ((outductsConfigPtr)
        && ((outductsConfigPtr->m_outductElementConfigVector[0].convergenceLayer == "tcpcl_v3") || (outductsConfigPtr->m_outductElementConfigVector[0].convergenceLayer == "tcpcl_v4"))
        && (outductsConfigPtr->m_outductElementConfigVector[0].tcpclAllowOpportunisticReceiveBundles)
        )
    {
        m_useCustodyTransfer = true;
        outductOpportunisticProcessReceivedBundleCallback = boost::bind(&BpSourcePattern::WholeRxBundleReadyCallback, this, boost::placeholders::_1);
        std::cout << "this bpsource pattern detected tcpcl convergence layer which is bidirectional.. supporting custody transfer\n";
    }
    else {
        m_useCustodyTransfer = false;
    }

    if (forceDisableCustody) { //for bping which needs to receive echo packets instead of admin records
        m_useCustodyTransfer = false;
    }

    
    if (outductsConfigPtr) {
        m_useInductForSendingBundles = false;
        if (!m_outductManager.LoadOutductsFromConfig(*outductsConfigPtr, m_myEid.nodeId, UINT16_MAX,
            10000000, //todo 10MB max rx opportunistic bundle
            outductOpportunisticProcessReceivedBundleCallback)) {
            return;
        }
    }
    else {
        m_useInductForSendingBundles = true;
    }

    m_running = true;
    m_allOutductsReady = false;
   
    
    m_bpSourcePatternThreadPtr = boost::make_unique<boost::thread>(
        boost::bind(&BpSourcePattern::BpSourcePatternThreadFunc, this, bundleRate)); //create and start the worker thread



}


void BpSourcePattern::BpSourcePatternThreadFunc(uint32_t bundleRate) {

    boost::mutex waitingForRxBundleBeforeNextTxMutex;
    boost::mutex::scoped_lock waitingForRxBundleBeforeNextTxLock(waitingForRxBundleBeforeNextTxMutex);

    while (m_running) {
        if (m_useInductForSendingBundles) {
            std::cout << "Waiting for Tcpcl opportunistic link on the induct to become available for forwarding bundles..." << std::endl;
            boost::this_thread::sleep(boost::posix_time::milliseconds(1000));
            if (m_tcpclInductPtr) {
                std::cout << "Induct opportunistic link ready to forward" << std::endl;
                break;
            }
        }
        else {
            std::cout << "Waiting for Outduct to become ready to forward..." << std::endl;
            boost::this_thread::sleep(boost::posix_time::milliseconds(1000));
            if (m_outductManager.AllReadyToForward()) {
                std::cout << "Outduct ready to forward" << std::endl;
                break;
            }
        }
    }
    if (!m_running) {
        std::cout << "BpGen Terminated before a connection could be made" << std::endl;
        return;
    }
    boost::this_thread::sleep(boost::posix_time::milliseconds(1000)); //todo make sure connection from hdtn to bpgen induct
    m_allOutductsReady = true;

    #define BP_MSG_BUFSZ             (65536 * 100) //todo

    boost::posix_time::time_duration sleepValTimeDuration = boost::posix_time::special_values::neg_infin;
    if(bundleRate) {
        std::cout << "Generating up to " << bundleRate << " bundles / second" << std::endl;
        const double sval = 1000000.0 / bundleRate;   // sleep val in usec
        ////sval *= BP_MSG_NBUF;
        const uint64_t sValU64 = static_cast<uint64_t>(sval);
        sleepValTimeDuration = boost::posix_time::microseconds(sValU64);
        std::cout << "Sleeping for " << sValU64 << " usec between bursts" << std::endl;
    }
    else {
        if (m_useInductForSendingBundles) {
            if (m_tcpclInductPtr) {
                std::cout << "bundle rate of zero used.. Going as fast as possible by allowing up to ??? unacked bundles" << std::endl;
            }
            else {
                std::cerr << "error: null induct" << std::endl;
                return;
            }
        }
        else {
            if (Outduct * outduct = m_outductManager.GetOutductByOutductUuid(0)) {
                std::cout << "bundle rate of zero used.. Going as fast as possible by allowing up to " << outduct->GetOutductMaxBundlesInPipeline() << " unacked bundles" << std::endl;
            }
            else {
                std::cerr << "error: null outduct" << std::endl;
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
    std::vector<uint8_t> bundleToSend;
    boost::posix_time::ptime startTime = boost::posix_time::microsec_clock::universal_time();
    while (m_running) { //keep thread alive if running

        
        if(bundleRate) {
            boost::system::error_code ec;
            deadlineTimer.wait(ec);
            if(ec) {
                std::cout << "timer error: " << ec.message() << std::endl;
                return;
            }
            deadlineTimer.expires_at(deadlineTimer.expires_at() + sleepValTimeDuration);
        }
        
        const uint64_t payloadSizeBytes = GetNextPayloadLength_Step1();
        if (payloadSizeBytes == 0) {
            std::cout << "payloadSizeBytes == 0... out of work.. exiting\n";
            m_running = false;
            continue;
        }
        uint64_t bundleLength;
        
        if (m_useBpVersion7) {
            BundleViewV7 bv;
            Bpv7CbhePrimaryBlock & primary = bv.m_primaryBlockView.header;
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
            primary.m_lifetimeMilliseconds = 1000000;
            primary.m_crcType = BPV7_CRC_TYPE::CRC32C;
            bv.m_primaryBlockView.SetManuallyModified();

            //add hop count block (before payload last block)
            {
                std::unique_ptr<Bpv7CanonicalBlock> blockPtr = boost::make_unique<Bpv7HopCountCanonicalBlock>();
                Bpv7HopCountCanonicalBlock & block = *(reinterpret_cast<Bpv7HopCountCanonicalBlock*>(blockPtr.get()));

                block.m_blockProcessingControlFlags = BPV7_BLOCKFLAG::REMOVE_BLOCK_IF_IT_CANT_BE_PROCESSED; //something for checking against
                block.m_blockNumber = 2;
                block.m_crcType = BPV7_CRC_TYPE::CRC32C;
                block.m_hopLimit = 100; //Hop limit MUST be in the range 1 through 255.
                block.m_hopCount = 0; //the hop count value SHOULD initially be zero and SHOULD be increased by 1 on each hop.
                bv.AppendMoveCanonicalBlock(blockPtr);
            }

            //append payload block (must be last block)
            {
                std::unique_ptr<Bpv7CanonicalBlock> payloadBlockPtr = boost::make_unique<Bpv7CanonicalBlock>();
                Bpv7CanonicalBlock & payloadBlock = *payloadBlockPtr;
                //payloadBlock.SetZero();

                payloadBlock.m_blockTypeCode = BPV7_BLOCK_TYPE_CODE::PAYLOAD;
                payloadBlock.m_blockProcessingControlFlags = BPV7_BLOCKFLAG::NO_FLAGS_SET;
                payloadBlock.m_blockNumber = 1; //must be 1
                payloadBlock.m_crcType = BPV7_CRC_TYPE::CRC32C;
                payloadBlock.m_dataLength = payloadSizeBytes;
                payloadBlock.m_dataPtr = NULL; //NULL will preallocate (won't copy or compute crc, user must do that manually below)
                bv.AppendMoveCanonicalBlock(payloadBlockPtr);
            }

            //render bundle to the front buffer
            if (!bv.Render(payloadSizeBytes + 1000)) {
                std::cout << "error rendering bpv7 bundle\n";
                return;
            }

            BundleViewV7::Bpv7CanonicalBlockView & payloadBlockView = bv.m_listCanonicalBlockView.back(); //payload block must be the last block

            //manually copy data to preallocated space and compute crc
            if (!CopyPayload_Step2(payloadBlockView.headerPtr->m_dataPtr)) { //m_dataPtr now points to new allocated or copied data within the serialized block (from after Render())
                std::cout << "copy payload error\n";
                m_running = false;
                continue;
            }
            payloadBlockView.headerPtr->RecomputeCrcAfterDataModification((uint8_t*)payloadBlockView.actualSerializedBlockPtr.data(), payloadBlockView.actualSerializedBlockPtr.size()); //recompute crc
            
            //move the bundle out of bundleView
            bundleToSend = std::move(bv.m_frontBuffer);
            bundleLength = bundleToSend.size();
        }
        else { //bp version 6
            bundleToSend.resize(payloadSizeBytes + 1000);




            uint8_t * buffer = bundleToSend.data();
            uint8_t * const serializationBase = buffer;
            const uint64_t currentTimeRfc5050 = TimestampUtil::GetSecondsSinceEpochRfc5050(); //curr_time = time(0);

            if (currentTimeRfc5050 == lastTimeRfc5050) {
                ++seq;
            }
            else {
                /*gettimeofday(&tv, NULL);
                double elapsed = ((double)tv.tv_sec) + ((double)tv.tv_usec / 1000000.0);
                elapsed -= start;
                start = start + elapsed;
                fprintf(log, "%0.6f, %lu, %lu, %lu, %lu\n", elapsed, bundle_count, raw_data, bundle_data, tsc_total);
                fflush(log);
                bundle_count = 0;
                bundle_data = 0;
                raw_data = 0;
                tsc_total = 0;*/
                seq = 0;
            }
            lastTimeRfc5050 = currentTimeRfc5050;

            Bpv6CbhePrimaryBlock primary;
            primary.SetZero();
            primary.m_bundleProcessingControlFlags = BPV6_BUNDLEFLAG::PRIORITY_EXPEDITED | BPV6_BUNDLEFLAG::SINGLETON | BPV6_BUNDLEFLAG::NOFRAGMENT;
            if (m_useCustodyTransfer) {
                primary.m_bundleProcessingControlFlags |= BPV6_BUNDLEFLAG::CUSTODY_REQUESTED;
                primary.m_custodianEid.Set(m_myEid.nodeId, m_myCustodianServiceId);
            }
            primary.m_sourceNodeId = m_myEid;
            primary.m_destinationEid = m_finalDestinationEid;
            primary.creation = currentTimeRfc5050; //(uint64_t)bpv6_unix_to_5050(curr_time);
            primary.lifetime = 1000;
            primary.sequence = seq;
            uint64_t retVal;
            retVal = primary.SerializeBpv6(buffer);
            if (retVal == 0) {
                std::cout << "primary encode error\n";
                m_running = false;
                continue;
            }
            buffer += retVal;

            if (m_useCustodyTransfer) {
                if (m_custodyTransferUseAcs) {
                    const uint64_t ctebCustodyId = nextCtebCustodyId++;
                    bpv6_canonical_block returnedCanonicalBlock;
                    retVal = CustodyTransferEnhancementBlock::StaticSerializeCtebCanonicalBlock((uint8_t*)buffer, 0, //0=> not last block
                        ctebCustodyId, m_myCustodianEidUriString, returnedCanonicalBlock);

                    if (retVal == 0) {
                        std::cout << "cteb encode error\n";
                        m_running = false;
                        continue;
                    }
                    buffer += retVal;

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
                        std::cerr << "error insert bundle uuid\n";
                        m_running = false;
                        continue;
                    }
                }

            }
            bpv6_canonical_block block;
            //memset 0 not needed because all fields set below
            block.type = BPV6_BLOCKTYPE_PAYLOAD;
            block.flags = BPV6_BLOCKFLAG_LAST_BLOCK;
            block.length = payloadSizeBytes;

            retVal = block.bpv6_canonical_block_encode((char *)buffer, 0, BP_MSG_BUFSZ);
            if (retVal == 0) {
                std::cout << "payload canonical encode error\n";
                m_running = false;
                continue;
            }
            buffer += retVal;


            if (!CopyPayload_Step2(buffer)) {
                std::cout << "copy payload error\n";
                m_running = false;
                continue;
            }

            buffer += payloadSizeBytes;

            bundleLength = buffer - serializationBase;



            bundleToSend.resize(bundleLength);
        }

        //send message
        while (m_running) {
            m_isWaitingForRxBundleBeforeNextTx = true;
            if ((!m_useInductForSendingBundles) && (!m_outductManager.Forward_Blocking(m_finalDestinationEid, bundleToSend, 3))) {
                std::cerr << "BpSourcePattern was unable to send a bundle for 3 seconds on the outduct.. retrying" << std::endl;
            }
            else if (m_useInductForSendingBundles && (!m_tcpclInductPtr->ForwardOnOpportunisticLink(m_tcpclOpportunisticRemoteNodeId, bundleToSend, 3))) {
                //note BpSource has no routing capability so it must send to the only connection available to it
                std::cerr << "BpSourcePattern was unable to send a bundle for 3 seconds on the opportunistic induct.. retrying" << std::endl;
            }
            else { //success forward
                if (bundleToSend.size() != 0) {
                    std::cerr << "error in BpGenAsync::BpGenThreadFunc: bundleToSend was not moved in Forward" << std::endl;
                    std::cerr << "bundleToSend.size() : " << bundleToSend.size() << std::endl;
                }
                ++m_bundleCount;
                bundle_data += payloadSizeBytes;     // payload data
                raw_data += bundleLength; // bundle overhead + payload data
                while (m_requireRxBundleBeforeNextTx && m_running && m_isWaitingForRxBundleBeforeNextTx) {
                    m_waitingForRxBundleBeforeNextTxConditionVariable.timed_wait(waitingForRxBundleBeforeNextTxLock, boost::posix_time::milliseconds(10));
                }
                break;
            }
        }
    }
    //todo m_outductManager.StopAllOutducts(); //wait for all pipelined bundles to complete before getting a finishedTime
    boost::posix_time::ptime finishedTime = boost::posix_time::microsec_clock::universal_time();

    std::cout << "bundle_count: " << m_bundleCount << std::endl;
    std::cout << "bundle_data (payload data): " << bundle_data << " bytes" << std::endl;
    std::cout << "raw_data (bundle overhead + payload data): " << raw_data << " bytes" << std::endl;
    if (bundleRate == 0) {
        std::cout << "numEventsTooManyUnackedBundles: " << numEventsTooManyUnackedBundles << std::endl;
    }

    if (m_useInductForSendingBundles) {
        std::cout << "BpSourcePattern Keeping Tcpcl Induct Opportunistic link open for 4 seconds to finish sending" << std::endl;
        boost::this_thread::sleep(boost::posix_time::seconds(4));
    }
    else if (Outduct * outduct = m_outductManager.GetOutductByOutductUuid(0)) {
        if (outduct->GetConvergenceLayerName() == "ltp_over_udp") {
            std::cout << "BpSourcePattern Keeping UDP open for 4 seconds to acknowledge report segments" << std::endl;
            boost::this_thread::sleep(boost::posix_time::seconds(4));
        }
    }
    if (m_useCustodyTransfer) {
        uint64_t lastNumCustodyTransfers = UINT64_MAX;
        while (true) {
            const uint64_t totalCustodyTransfers = std::max(m_numRfc5050CustodyTransfers, m_numAcsCustodyTransfers);
            if (totalCustodyTransfers == m_bundleCount) {
                std::cout << "BpSourcePattern received all custody transfers" << std::endl;
                break;
            }
            else if (totalCustodyTransfers != lastNumCustodyTransfers) {
                lastNumCustodyTransfers = totalCustodyTransfers;
                std::cout << "BpSourcePattern waiting for an additional 10 seconds to receive custody transfers" << std::endl;
                boost::this_thread::sleep(boost::posix_time::seconds(10));
            }
            else {
                std::cout << "BpSourcePattern received no custody transfers for the last 10 seconds.. exiting" << std::endl;
                break;
            }
        }
    }
    std::cout << "m_numRfc5050CustodyTransfers: " << m_numRfc5050CustodyTransfers << std::endl;
    std::cout << "m_numAcsCustodyTransfers: " << m_numAcsCustodyTransfers << std::endl;
    std::cout << "m_numAcsPacketsReceived: " << m_numAcsPacketsReceived << std::endl;

    boost::posix_time::time_duration diff = finishedTime - startTime;
    {
        const double rateMbps = (bundle_data * 8.0) / (diff.total_microseconds());
        printf("Sent bundle_data (payload data) at %0.4f Mbits/sec\n", rateMbps);
    }
    {
        const double rateMbps = (raw_data * 8.0) / (diff.total_microseconds());
        printf("Sent raw_data (bundle overhead + payload data) at %0.4f Mbits/sec\n", rateMbps);
    }

    std::cout << "BpSourcePattern::BpSourcePatternThreadFunc thread exiting\n";
}

void BpSourcePattern::WholeRxBundleReadyCallback(padded_vector_uint8_t & wholeBundleVec) {
    //if more than 1 Induct, must protect shared resources with mutex.  Each Induct has
    //its own processing thread that calls this callback

    BundleViewV6 bv;
    if (!bv.LoadBundle(wholeBundleVec.data(), wholeBundleVec.size())) {
        std::cerr << "malformed custody signal\n";
        return;
    }
    //check primary
    const Bpv6CbhePrimaryBlock & primary = bv.m_primaryBlockView.header;
    const cbhe_eid_t & receivedFinalDestinationEid = primary.m_destinationEid;
    static const BPV6_BUNDLEFLAG requiredPrimaryFlagsForCustody = BPV6_BUNDLEFLAG::SINGLETON | BPV6_BUNDLEFLAG::NOFRAGMENT | BPV6_BUNDLEFLAG::ADMINRECORD;
    if ((primary.m_bundleProcessingControlFlags & requiredPrimaryFlagsForCustody) != requiredPrimaryFlagsForCustody) { //assume non-admin-record bundle (perhaps a bpecho bundle)

        
        if (receivedFinalDestinationEid != m_myEid) {
            std::cerr << "BpSourcePattern received a bundle with final destination " << receivedFinalDestinationEid
                << " that does not match this destination " << m_myEid << "\n";
            return;
        }

        std::vector<BundleViewV6::Bpv6CanonicalBlockView*> blocks;
        bv.GetCanonicalBlocksByType(BPV6_BLOCKTYPE_PAYLOAD, blocks);
        if (blocks.size() != 1) {
            std::cerr << "error BpSourcePattern received a non-admin-record bundle with no payload block\n";
            return;
        }
        bpv6_canonical_block & payloadBlock = blocks[0]->header;
        m_totalNonAdminRecordPayloadBytesRx += payloadBlock.length;
        m_totalNonAdminRecordBundleBytesRx += bv.m_renderedBundle.size();
        ++m_totalNonAdminRecordBundlesRx;

        boost::asio::const_buffer & blockBodyBuffer = blocks[0]->actualSerializedBodyPtr;
        if (!ProcessNonAdminRecordBundlePayload((const uint8_t *)blockBodyBuffer.data(), blockBodyBuffer.size())) {
            std::cerr << "error ProcessNonAdminRecordBundlePayload\n";
            return;
        }
        m_isWaitingForRxBundleBeforeNextTx = false;
        m_waitingForRxBundleBeforeNextTxConditionVariable.notify_one();
    }
    else { //admin record

        if (receivedFinalDestinationEid != m_myCustodianEid) {
            std::cerr << "BpSourcePattern received an admin record bundle with final destination "
                << Uri::GetIpnUriString(receivedFinalDestinationEid.nodeId, receivedFinalDestinationEid.serviceId)
                << " that does not match this custodial destination " << Uri::GetIpnUriString(m_myCustodianEid.nodeId, m_myCustodianEid.serviceId) << "\n";
            return;
        }

        if (bv.GetNumCanonicalBlocks() != 0) { //admin record is not canonical
            std::cerr << "error admin record has canonical block\n";
            return;
        }
        if (bv.m_applicationDataUnitStartPtr == NULL) { //admin record is not canonical
            std::cerr << "error null application data unit\n";
            return;
        }
        const uint8_t adminRecordType = (*bv.m_applicationDataUnitStartPtr >> 4);

        if (adminRecordType == static_cast<uint8_t>(BPV6_ADMINISTRATIVE_RECORD_TYPES::AGGREGATE_CUSTODY_SIGNAL)) {
            m_detectedNextCustodianSupportsCteb = true;
            ++m_numAcsPacketsReceived;
            //check acs
            AggregateCustodySignal acs;
            if (!acs.Deserialize(bv.m_applicationDataUnitStartPtr, bv.m_renderedBundle.size() - bv.m_primaryBlockView.actualSerializedPrimaryBlockPtr.size())) {
                std::cerr << "malformed ACS\n";
                return;
            }
            if (!acs.DidCustodyTransferSucceed()) {
                std::cerr << "custody transfer failed with reason code " << static_cast<unsigned int>(acs.GetReasonCode()) << "\n";
                return;
            }

            m_mutexCtebSet.lock();
            for (std::set<FragmentSet::data_fragment_t>::const_iterator it = acs.m_custodyIdFills.cbegin(); it != acs.m_custodyIdFills.cend(); ++it) {
                m_numAcsCustodyTransfers += (it->endIndex + 1) - it->beginIndex;
                FragmentSet::RemoveFragment(m_outstandingCtebCustodyIdsFragmentSet, *it);
            }
            m_mutexCtebSet.unlock();
        }
        else if (adminRecordType == static_cast<uint8_t>(BPV6_ADMINISTRATIVE_RECORD_TYPES::CUSTODY_SIGNAL)) { //rfc5050 style custody transfer
            CustodySignal cs;
            uint16_t numBytesTakenToDecode = cs.Deserialize(bv.m_applicationDataUnitStartPtr);
            if (numBytesTakenToDecode == 0) {
                std::cerr << "malformed CustodySignal\n";
                return;
            }
            if (!cs.DidCustodyTransferSucceed()) {
                std::cerr << "custody transfer failed with reason code " << static_cast<unsigned int>(cs.GetReasonCode()) << "\n";
                return;
            }
            if (cs.m_isFragment) {
                std::cerr << "error custody signal with fragmentation received\n";
                return;
            }
            cbhe_bundle_uuid_nofragment_t uuid;
            if (!Uri::ParseIpnUriString(cs.m_bundleSourceEid, uuid.srcEid.nodeId, uuid.srcEid.serviceId)) {
                std::cerr << "error custody signal with bad ipn string\n";
                return;
            }
            uuid.creationSeconds = cs.m_copyOfBundleCreationTimestampTimeSeconds;
            uuid.sequence = cs.m_copyOfBundleCreationTimestampSequenceNumber;
            m_mutexBundleUuidSet.lock();
            const bool success = (m_cbheBundleUuidSet.erase(uuid) != 0);
            m_mutexBundleUuidSet.unlock();
            if (!success) {
                std::cerr << "error rfc5050 custody signal received but bundle uuid not found\n";
                return;
            }
            ++m_numRfc5050CustodyTransfers;
        }
        else {
            std::cerr << "error unknown admin record type\n";
            return;
        }
    }
    
}

bool BpSourcePattern::ProcessNonAdminRecordBundlePayload(const uint8_t * data, const uint64_t size) {
    return true;
}

void BpSourcePattern::OnNewOpportunisticLinkCallback(const uint64_t remoteNodeId, Induct * thisInductPtr) {
    if (m_tcpclInductPtr = dynamic_cast<TcpclInduct*>(thisInductPtr)) {
        std::cout << "New opportunistic link detected on Tcpcl induct for ipn:" << remoteNodeId << ".*\n";
        m_tcpclOpportunisticRemoteNodeId = remoteNodeId;
    }
    else if (m_tcpclInductPtr = dynamic_cast<TcpclV4Induct*>(thisInductPtr)) {
        std::cout << "New opportunistic link detected on TcpclV4 induct for ipn:" << remoteNodeId << ".*\n";
        m_tcpclOpportunisticRemoteNodeId = remoteNodeId;
    }
    else {
        std::cerr << "error in BpSourcePattern::OnNewOpportunisticLinkCallback: Induct ptr cannot cast to TcpclInduct or TcpclV4Induct\n";
    }
}
void BpSourcePattern::OnDeletedOpportunisticLinkCallback(const uint64_t remoteNodeId) {
    m_tcpclOpportunisticRemoteNodeId = 0;
    std::cout << "Deleted opportunistic link on Tcpcl induct for ipn:" << remoteNodeId << ".*\n";
}
