/***************************************************************************
 * NASA Glenn Research Center, Cleveland, OH
 * Released under the NASA Open Source Agreement (NOSA)
 * May  2021
 ****************************************************************************
 */

#include <math.h>

#include <iostream>

#include "message.hpp"
#include "store.hpp"
#include "BundleStorageManagerMT.h"
#include "BundleStorageManagerAsio.h"
#include "Logger.h"
#include <set>
#include <boost/lexical_cast.hpp>
#include <boost/make_unique.hpp>
#include "codec/CustodyIdAllocator.h"
#include "codec/CustodyTransferManager.h"
#include "Uri.h"


hdtn::ZmqStorageInterface::ZmqStorageInterface() : m_running(false) {}

hdtn::ZmqStorageInterface::~ZmqStorageInterface() {
    Stop();
}

void hdtn::ZmqStorageInterface::Stop() {
    m_running = false; //thread stopping criteria
    if (m_threadPtr) {
        m_threadPtr->join();
        m_threadPtr.reset();
    }
}

void hdtn::ZmqStorageInterface::Init(zmq::context_t *ctx, const HdtnConfig & hdtnConfig, zmq::context_t * hdtnOneProcessZmqInprocContextPtr) {
    m_zmqContextPtr = ctx;
    m_hdtnConfig = hdtnConfig;
    //according to ION.pdf v4.0.1 on page 100 it says:
    //  Remember that the format for this argument is ipn:element_number.0 and that
    //  the final 0 is required, as custodial/administration service is always service 0.
    //HDTN shall default m_myCustodialServiceId to 0 although it is changeable in the hdtn config json file
    M_HDTN_EID_CUSTODY.Set(m_hdtnConfig.m_myNodeId, m_hdtnConfig.m_myCustodialServiceId);
    m_hdtnOneProcessZmqInprocContextPtr = hdtnOneProcessZmqInprocContextPtr;
}

static bool WriteAcsBundle(BundleStorageManagerBase & bsm, CustodyIdAllocator & custodyIdAllocator,
    std::pair<bpv6_primary_block, std::vector<uint8_t> > & primaryPlusSerializedBundle)
{
    const bpv6_primary_block & primary = primaryPlusSerializedBundle.first;
    const std::vector<uint8_t> & acsBundleSerialized = primaryPlusSerializedBundle.second;
    const cbhe_eid_t hdtnSrcEid(primary.src_node, primary.src_svc);
    const uint64_t newCustodyIdForAcsCustodySignal = custodyIdAllocator.GetNextCustodyIdForNextHopCtebToSend(hdtnSrcEid);

    //write custody signal to disk
    BundleStorageManagerSession_WriteToDisk sessionWrite;
    const uint64_t totalSegmentsRequired = bsm.Push(sessionWrite, primary, acsBundleSerialized.size());
    //std::cout << "totalSegmentsRequired " << totalSegmentsRequired << "\n";
    if (totalSegmentsRequired == 0) {
        const std::string msg = "out of space for acs custody signal";
        std::cerr << msg << "\n";
        hdtn::Logger::getInstance()->logError("storage", msg);
        return false;
    }

    const uint64_t totalBytesPushed = bsm.PushAllSegments(sessionWrite, primary,
        newCustodyIdForAcsCustodySignal, acsBundleSerialized.data(), acsBundleSerialized.size());
    if (totalBytesPushed != acsBundleSerialized.size()) {
        const std::string msg = "totalBytesPushed != acsBundleSerialized.size";
        std::cerr << msg << "\n";
        hdtn::Logger::getInstance()->logError("storage", msg);
        return false;
    }
    return true;
}

static bool Write(zmq::message_t *message, BundleStorageManagerBase & bsm,
    CustodyIdAllocator & custodyIdAllocator, CustodyTransferManager & ctm,
    std::vector<uint8_t> & bufferSpaceForCustodySignalRfc5050SerializedBundle,
    cbhe_eid_t & finalDestEidReturned, hdtn::ZmqStorageInterface * forStats)
{
    BundleViewV6 bv;
    if (!bv.LoadBundle((uint8_t *)message->data(), message->size())) { //invalid bundle
        std::cerr << "malformed bundle\n";
        return false;
    }
    bpv6_primary_block & primary = bv.m_primaryBlockView.header;
    const cbhe_eid_t finalDestEid(primary.dst_node, primary.dst_svc);
    finalDestEidReturned = finalDestEid;
    const cbhe_eid_t srcEid(primary.src_node, primary.src_svc);

    //admin records pertaining to this hdtn node do not get written to disk.. they signal a deletion from disk
    const uint64_t requiredPrimaryFlagsForAdminRecord = BPV6_BUNDLEFLAG_SINGLETON | BPV6_BUNDLEFLAG_NOFRAGMENT | BPV6_BUNDLEFLAG_ADMIN_RECORD;
    if (((primary.flags & requiredPrimaryFlagsForAdminRecord) == requiredPrimaryFlagsForAdminRecord) && (finalDestEid == forStats->M_HDTN_EID_CUSTODY)) {
        if (bv.GetNumCanonicalBlocks() != 0) { //admin record is not canonical
            std::cerr << "error admin record has canonical block\n";
            return false;
        }
        if (bv.m_applicationDataUnitStartPtr == NULL) { //admin record is not canonical
            std::cerr << "error null application data unit\n";
            return false;
        }
        const uint8_t adminRecordType = (*bv.m_applicationDataUnitStartPtr >> 4);

        if (adminRecordType == static_cast<uint8_t>(BPV6_ADMINISTRATIVE_RECORD_TYPES::AGGREGATE_CUSTODY_SIGNAL)) {
            ++forStats->m_numAcsPacketsReceived;
            //check acs
            AggregateCustodySignal acs;
            if (!acs.Deserialize(bv.m_applicationDataUnitStartPtr, bv.m_renderedBundle.size() - bv.m_primaryBlockView.actualSerializedPrimaryBlockPtr.size())) {
                std::cerr << "malformed ACS\n";
                return false;
            }
            if (!acs.DidCustodyTransferSucceed()) {
                std::cerr << "custody transfer failed with reason code " << static_cast<unsigned int>(acs.GetReasonCode()) << "\n";
                return false;
            }

            for (std::set<FragmentSet::data_fragment_t>::const_iterator it = acs.m_custodyIdFills.cbegin(); it != acs.m_custodyIdFills.cend(); ++it) {
                forStats->m_numAcsCustodyTransfers += (it->endIndex + 1) - it->beginIndex;
                custodyIdAllocator.FreeCustodyIdRange(it->beginIndex, it->endIndex);
                for (uint64_t currentCustodyId = it->beginIndex; currentCustodyId <= it->endIndex; ++currentCustodyId) {
                    bool successRemoveBundle = bsm.RemoveReadBundleFromDisk(currentCustodyId);
                    if (!successRemoveBundle) {
                        std::cout << "error freeing bundle identified by acs custody signal from disk\n";
                        return false;
                    }                    
                    ++forStats->m_totalBundlesErasedFromStorageWithCustodyTransfer;
                }
                
            }
        }
        else if (adminRecordType == static_cast<uint8_t>(BPV6_ADMINISTRATIVE_RECORD_TYPES::CUSTODY_SIGNAL)) { //rfc5050 style custody transfer
            CustodySignal cs;
            uint16_t numBytesTakenToDecode = cs.Deserialize(bv.m_applicationDataUnitStartPtr);
            if (numBytesTakenToDecode == 0) {
                std::cerr << "malformed CustodySignal\n";
                return false;
            }
            if (!cs.DidCustodyTransferSucceed()) {
                std::cerr << "custody transfer failed with reason code " << static_cast<unsigned int>(cs.GetReasonCode()) << "\n";
                return false;
            }
            uint64_t * custodyIdPtr;
            if (cs.m_isFragment) {
                cbhe_bundle_uuid_t uuid;
                if (!Uri::ParseIpnUriString(cs.m_bundleSourceEid, uuid.srcEid.nodeId, uuid.srcEid.serviceId)) {
                    std::cerr << "error custody signal with bad ipn string\n";
                    return false;
                }
                uuid.creationSeconds = cs.m_copyOfBundleCreationTimestampTimeSeconds;
                uuid.sequence = cs.m_copyOfBundleCreationTimestampSequenceNumber;
                uuid.fragmentOffset = cs.m_fragmentOffsetIfPresent;
                uuid.dataLength = cs.m_fragmentLengthIfPresent;
                custodyIdPtr = bsm.GetCustodyIdFromUuid(uuid);
            }
            else {
                cbhe_bundle_uuid_nofragment_t uuid;
                if (!Uri::ParseIpnUriString(cs.m_bundleSourceEid, uuid.srcEid.nodeId, uuid.srcEid.serviceId)) {
                    std::cerr << "error custody signal with bad ipn string\n";
                    return false;
                }
                uuid.creationSeconds = cs.m_copyOfBundleCreationTimestampTimeSeconds;
                uuid.sequence = cs.m_copyOfBundleCreationTimestampSequenceNumber;
                //std::cout << "uuid: " << "cs " << uuid.creationSeconds << "  seq " << uuid.sequence << "  " << uuid.srcEid.nodeId << "," << uuid.srcEid.serviceId << std::endl;
                custodyIdPtr = bsm.GetCustodyIdFromUuid(uuid);
            }
            if (custodyIdPtr == NULL) {
                std::cerr << "error custody signal does not match a bundle in the storage database\n";
                return false;
            }
            bool successRemoveBundle = bsm.RemoveReadBundleFromDisk(*custodyIdPtr);
            if (!successRemoveBundle) {
                std::cout << "error freeing bundle identified by acs custody signal from disk\n";
                return false;
            }
            custodyIdAllocator.FreeCustodyId(*custodyIdPtr);
            ++forStats->m_totalBundlesErasedFromStorageWithCustodyTransfer;
            ++forStats->m_numRfc5050CustodyTransfers;
        }
        else {
            std::cerr << "error unknown admin record type\n";
            return false;
        }
        return true; //do not proceed past this point so that the signal is not written to disk
    }

    //write non admin records to disk (unless newly generated below)
    const uint64_t newCustodyId = custodyIdAllocator.GetNextCustodyIdForNextHopCtebToSend(srcEid);
    static constexpr uint64_t requiredPrimaryFlagsForCustody = BPV6_BUNDLEFLAG_SINGLETON | BPV6_BUNDLEFLAG_NOFRAGMENT | BPV6_BUNDLEFLAG_CUSTODY;
    if ((primary.flags & requiredPrimaryFlagsForCustody) == requiredPrimaryFlagsForCustody) {
        bpv6_primary_block primaryForCustodySignalRfc5050;
        if (!ctm.ProcessCustodyOfBundle(bv, true, newCustodyId, BPV6_ACS_STATUS_REASON_INDICES::SUCCESS__NO_ADDITIONAL_INFORMATION,
            bufferSpaceForCustodySignalRfc5050SerializedBundle, primaryForCustodySignalRfc5050)) {
            std::cerr << "error unable to process custody\n";
        }
        else if (!bv.Render(message->size() + 200)) { //hdtn modifies bundle for next hop
            std::cerr << "error unable to render new bundle\n";
        }
        else {
            if (!bufferSpaceForCustodySignalRfc5050SerializedBundle.empty()) {
                const cbhe_eid_t hdtnSrcEid(primaryForCustodySignalRfc5050.src_node, primaryForCustodySignalRfc5050.src_svc);
                const uint64_t newCustodyIdFor5050CustodySignal = custodyIdAllocator.GetNextCustodyIdForNextHopCtebToSend(hdtnSrcEid);

                //write custody signal to disk
                BundleStorageManagerSession_WriteToDisk sessionWrite;
                const uint64_t totalSegmentsRequired = bsm.Push(sessionWrite, primaryForCustodySignalRfc5050, bufferSpaceForCustodySignalRfc5050SerializedBundle.size());
                //std::cout << "totalSegmentsRequired " << totalSegmentsRequired << "\n";
                if (totalSegmentsRequired == 0) {
                    const std::string msg = "out of space for custody signal";
                    std::cerr << msg << "\n";
                    hdtn::Logger::getInstance()->logError("storage", msg);
                    return false;
                }
                
                const uint64_t totalBytesPushed = bsm.PushAllSegments(sessionWrite, primaryForCustodySignalRfc5050,
                    newCustodyIdFor5050CustodySignal, bufferSpaceForCustodySignalRfc5050SerializedBundle.data(), bufferSpaceForCustodySignalRfc5050SerializedBundle.size());
                if (totalBytesPushed != bufferSpaceForCustodySignalRfc5050SerializedBundle.size()) {
                    const std::string msg = "totalBytesPushed != bufferSpaceForCustodySignalRfc5050SerializedBundle.size";
                    std::cerr << msg << "\n";
                    hdtn::Logger::getInstance()->logError("storage", msg);
                    return false;
                }
            }
        }
    }
    
    

    //write bundle (modified by hdtn if custody requested) to disk
    BundleStorageManagerSession_WriteToDisk sessionWrite;
    uint64_t totalSegmentsRequired = bsm.Push(sessionWrite, primary, bv.m_renderedBundle.size());
    //std::cout << "totalSegmentsRequired " << totalSegmentsRequired << "\n";
    if (totalSegmentsRequired == 0) {
        std::cerr << "out of space\n";
        hdtn::Logger::getInstance()->logError("storage", "Out of space");
        return false;
    }
    //totalSegmentsStoredOnDisk += totalSegmentsRequired;
    //totalBytesWrittenThisTest += size;

    const uint64_t totalBytesPushed = bsm.PushAllSegments(sessionWrite, primary, newCustodyId, (const uint8_t*)bv.m_renderedBundle.data(), bv.m_renderedBundle.size());
    if (totalBytesPushed != bv.m_renderedBundle.size()) {
        const std::string msg = "totalBytesPushed != size";
        std::cerr << msg << "\n";
        hdtn::Logger::getInstance()->logError("storage", msg);
        return false;
    }
    return true;
}

//return number of bytes to read for specified links
static uint64_t PeekOne(const std::vector<cbhe_eid_t> & availableDestLinks, BundleStorageManagerBase & bsm) {
    BundleStorageManagerSession_ReadFromDisk  sessionRead;
    const uint64_t bytesToReadFromDisk = bsm.PopTop(sessionRead, availableDestLinks);
    if (bytesToReadFromDisk == 0) { //no more of these links to read
        return 0; //no bytes to read
    }

    //this link has a bundle in the fifo
    bsm.ReturnTop(sessionRead);
    return bytesToReadFromDisk;
}

static void CustomCleanupToEgressHdr(void *data, void *hint) {
    delete static_cast<hdtn::ToEgressHdr*>(hint);
}
static void CustomCleanupStorageAckHdr(void *data, void *hint) {
    delete static_cast<hdtn::StorageAckHdr*>(hint);
}

static bool ReleaseOne_NoBlock(BundleStorageManagerSession_ReadFromDisk & sessionRead,
    const std::vector<cbhe_eid_t> & availableDestLinks,
    zmq::socket_t *egressSock, BundleStorageManagerBase & bsm, const uint64_t maxBundleSizeToRead)
{
    //std::cout << "reading\n";
    const uint64_t bytesToReadFromDisk = bsm.PopTop(sessionRead, availableDestLinks);
    //std::cout << "bytesToReadFromDisk " << bytesToReadFromDisk << "\n";
    if (bytesToReadFromDisk == 0) { //no more of these links to read
        return false;
    }

    //this link has a bundle in the fifo
        

    //IF YOU DECIDE YOU DON'T WANT TO READ THE BUNDLE AFTER PEEKING AT IT (MAYBE IT'S TOO BIG RIGHT NOW)
    if (bytesToReadFromDisk > maxBundleSizeToRead) {
        std::cerr << "error: bundle to read from disk is too large right now" << std::endl;
        hdtn::Logger::getInstance()->logError("storage", "Error: bundle to read from disk is too large right now");
        bsm.ReturnTop(sessionRead);
        return false;
        //bytesToReadFromDisk = bsm.PopTop(sessionRead, availableDestLinks); //get it back
    }
        
    zmq::message_t zmqMsg(bytesToReadFromDisk);
    uint8_t * bundleReadBack = (uint8_t*)zmqMsg.data();


    const std::size_t numSegmentsToRead = sessionRead.catalogEntryPtr->segmentIdChainVec.size();
    std::size_t totalBytesRead = 0;
    for (std::size_t i = 0; i < numSegmentsToRead; ++i) {
        totalBytesRead += bsm.TopSegment(sessionRead, &bundleReadBack[i*BUNDLE_STORAGE_PER_SEGMENT_SIZE]);
    }
    //std::cout << "totalBytesRead " << totalBytesRead << "\n";
    if (totalBytesRead != bytesToReadFromDisk) {
        std::cout << "error: totalBytesRead != bytesToReadFromDisk\n";
        hdtn::Logger::getInstance()->logError("storage", "Error: totalBytesRead != bytesToReadFromDisk");
        return false;
    }


    //force natural/64-bit alignment
    hdtn::ToEgressHdr * toEgressHdr = new hdtn::ToEgressHdr();
    zmq::message_t zmqMessageToEgressHdrWithDataStolen(toEgressHdr, sizeof(hdtn::ToEgressHdr), CustomCleanupToEgressHdr, toEgressHdr);

    //memset 0 not needed because all values set below
    toEgressHdr->base.type = HDTN_MSGTYPE_EGRESS;
    toEgressHdr->base.flags = 0;
    toEgressHdr->finalDestEid = sessionRead.catalogEntryPtr->destEid;
    toEgressHdr->hasCustody = sessionRead.catalogEntryPtr->HasCustody();
    toEgressHdr->isCutThroughFromIngress = 0;
    toEgressHdr->custodyId = sessionRead.custodyId;
    
    if (!egressSock->send(std::move(zmqMessageToEgressHdrWithDataStolen), zmq::send_flags::sndmore | zmq::send_flags::dontwait)) {
        std::cout << "error: zmq could not send" << std::endl;
        hdtn::Logger::getInstance()->logError("storage", "Error: zmq could not send");
        bsm.ReturnTop(sessionRead);
        return false;
    }
    if (!egressSock->send(std::move(zmqMsg), zmq::send_flags::dontwait)) {
        std::cout << "error: zmq could not send bundle" << std::endl;
        hdtn::Logger::getInstance()->logError("storage", "Error: zmq could not send bundle");
        bsm.ReturnTop(sessionRead);
        return false;
    }
    /*
    //if you're happy with the bundle data you read back, then officially remove it from the disk
    if (deleteFromDiskNow) {
        bool successRemoveBundle = bsm.RemoveReadBundleFromDisk(sessionRead);
        if (!successRemoveBundle) {
            std::cout << "error freeing bundle from disk\n";
            return false;
        }
    }
        */
        
        
    return true;

}

void hdtn::ZmqStorageInterface::ThreadFunc() {
    BundleStorageManagerSession_ReadFromDisk sessionRead; //reuse this due to expensive heap allocation
    std::vector<uint8_t> bufferSpaceForCustodySignalRfc5050SerializedBundle;
    bufferSpaceForCustodySignalRfc5050SerializedBundle.reserve(2000);
    CustodyIdAllocator custodyIdAllocator;
    const bool isAcsAware = true;
    
    static const boost::posix_time::time_duration ACS_SEND_PERIOD = boost::posix_time::milliseconds(1000);
    CustodyTransferManager ctm(isAcsAware, M_HDTN_EID_CUSTODY.nodeId, M_HDTN_EID_CUSTODY.serviceId);
    std::cout << "[storage-worker] Worker thread starting up." << std::endl;
    hdtn::Logger::getInstance()->logNotification("storage", "Worker thread starting up");

    zmq::socket_t inprocBundleDataSock(*m_zmqContextPtr, zmq::socket_type::pair);
    inprocBundleDataSock.connect(HDTN_STORAGE_BUNDLE_DATA_INPROC_PATH);
    zmq::socket_t inprocReleaseMessagesSock(*m_zmqContextPtr, zmq::socket_type::pair);
    inprocReleaseMessagesSock.connect(HDTN_STORAGE_RELEASE_MESSAGES_INPROC_PATH);

    std::unique_ptr<zmq::socket_t> egressSockPtr;
    std::unique_ptr<zmq::socket_t> fromEgressSockPtr;
    std::unique_ptr<zmq::socket_t> toIngressSockPtr;
    if (m_hdtnOneProcessZmqInprocContextPtr) {
        egressSockPtr = boost::make_unique<zmq::socket_t>(*m_hdtnOneProcessZmqInprocContextPtr, zmq::socket_type::pair);
        egressSockPtr->connect(std::string("inproc://connecting_storage_to_bound_egress")); // egress should bind

        fromEgressSockPtr = boost::make_unique<zmq::socket_t>(*m_hdtnOneProcessZmqInprocContextPtr, zmq::socket_type::pair);
        fromEgressSockPtr->connect(std::string("inproc://bound_egress_to_connecting_storage")); // egress should bind

        toIngressSockPtr = boost::make_unique<zmq::socket_t>(*m_hdtnOneProcessZmqInprocContextPtr, zmq::socket_type::pair);
        toIngressSockPtr->connect(std::string("inproc://connecting_storage_to_bound_ingress"));
    }
    else {
        egressSockPtr = boost::make_unique<zmq::socket_t>(*m_zmqContextPtr, zmq::socket_type::push);
        const std::string connect_connectingStorageToBoundEgressPath(
            std::string("tcp://") +
            m_hdtnConfig.m_zmqEgressAddress +
            std::string(":") +
            boost::lexical_cast<std::string>(m_hdtnConfig.m_zmqConnectingStorageToBoundEgressPortPath));
        egressSockPtr->connect(connect_connectingStorageToBoundEgressPath); // egress should bind
        fromEgressSockPtr = boost::make_unique<zmq::socket_t>(*m_zmqContextPtr, zmq::socket_type::pull);
        const std::string connect_boundEgressToConnectingStoragePath(
            std::string("tcp://") +
            m_hdtnConfig.m_zmqEgressAddress +
            std::string(":") +
            boost::lexical_cast<std::string>(m_hdtnConfig.m_zmqBoundEgressToConnectingStoragePortPath));
        fromEgressSockPtr->connect(connect_boundEgressToConnectingStoragePath); // egress should bind
        toIngressSockPtr = boost::make_unique<zmq::socket_t>(*m_zmqContextPtr, zmq::socket_type::push);
        const std::string connect_connectingStorageToBoundIngressPath(
            std::string("tcp://") +
            m_hdtnConfig.m_zmqIngressAddress +
            std::string(":") +
            boost::lexical_cast<std::string>(m_hdtnConfig.m_zmqConnectingStorageToBoundIngressPortPath));
        toIngressSockPtr->connect(connect_connectingStorageToBoundIngressPath);
    }

    zmq::pollitem_t pollItems[3] = {
        {fromEgressSockPtr->handle(), 0, ZMQ_POLLIN, 0},
        {inprocBundleDataSock.handle(), 0, ZMQ_POLLIN, 0},
        {inprocReleaseMessagesSock.handle(), 0, ZMQ_POLLIN, 0}
    };

    // Use a form of receive that times out so we can terminate cleanly.
    static const int timeout = 250;  // milliseconds
    inprocBundleDataSock.set(zmq::sockopt::rcvtimeo, timeout);
    inprocReleaseMessagesSock.set(zmq::sockopt::rcvtimeo, timeout);
    fromEgressSockPtr->set(zmq::sockopt::rcvtimeo, timeout);

    

    CommonHdr startupNotify = {
        HDTN_MSGTYPE_IOK,
        0};
    
    std::unique_ptr<BundleStorageManagerBase> bsmPtr;
    if (m_hdtnConfig.m_storageConfig.m_storageImplementation == "stdio_multi_threaded") {
        std::cout << "[ZmqStorageInterface] Initializing BundleStorageManagerMT ... " << std::endl;
        hdtn::Logger::getInstance()->logNotification("storage", "[ZmqStorageInterface] Initializing BundleStorageManagerMT ... ");
        bsmPtr = boost::make_unique<BundleStorageManagerMT>(boost::make_shared<StorageConfig>(m_hdtnConfig.m_storageConfig));
    }
    else if (m_hdtnConfig.m_storageConfig.m_storageImplementation == "asio_single_threaded") {
        std::cout << "[ZmqStorageInterface] Initializing BundleStorageManagerAsio ... " << std::endl;
        hdtn::Logger::getInstance()->logNotification("storage", "[ZmqStorageInterface] Initializing BundleStorageManagerAsio ... ");
        bsmPtr = boost::make_unique<BundleStorageManagerAsio>(boost::make_shared<StorageConfig>(m_hdtnConfig.m_storageConfig));
    }
    else {
        std::cerr << "error in hdtn::ZmqStorageInterface::ThreadFunc: invalid storage implementation " << m_hdtnConfig.m_storageConfig.m_storageImplementation << std::endl;
        return;
    }
    BundleStorageManagerBase & bsm = *bsmPtr;
    bsm.Start();
    //if (!m_storeFlow.init(m_root)) {
    //    startupNotify.type = HDTN_MSGTYPE_IABORT;
    //    workerSock.send(&startupNotify, sizeof(common_hdr));
    //    return;
    //}
    inprocBundleDataSock.send(zmq::const_buffer(&startupNotify, sizeof(CommonHdr)), zmq::send_flags::none);
    std::cout << "[ZmqStorageInterface] Notified parent that startup is complete." << std::endl;
    hdtn::Logger::getInstance()->logNotification("storage", "[ZmqStorageInterface] Notified parent that startup is complete.");

    typedef std::set<uint64_t> custodyid_set_t;
    typedef std::map<cbhe_eid_t, custodyid_set_t> finaldesteid_opencustids_map_t;

    m_totalBundlesErasedFromStorageNoCustodyTransfer = 0;
    m_totalBundlesErasedFromStorageWithCustodyTransfer = 0;
    m_totalBundlesSentToEgressFromStorage = 0;
    m_numRfc5050CustodyTransfers = 0;
    m_numAcsCustodyTransfers = 0;
    m_numAcsPacketsReceived = 0;
    std::size_t totalEventsAllLinksClogged = 0;
    std::size_t totalEventsNoDataInStorageForAvailableLinks = 0;
    std::size_t totalEventsDataInStorageForCloggedLinks = 0;

    std::set<cbhe_eid_t> availableDestLinksSet;
    finaldesteid_opencustids_map_t finalDestEidToOpenCustIdsMap;

    static const long DEFAULT_BIG_TIMEOUT_POLL = 250;
    long timeoutPoll = DEFAULT_BIG_TIMEOUT_POLL; //0 => no blocking
    boost::posix_time::ptime acsSendNowExpiry = boost::posix_time::microsec_clock::universal_time() + ACS_SEND_PERIOD;
    while (m_running) {        
        if (zmq::poll(pollItems, 3, timeoutPoll) > 0) {            
            if (pollItems[0].revents & ZMQ_POLLIN) { //from egress sock
                hdtn::EgressAckHdr egressAckHdr;
                const zmq::recv_buffer_result_t res = fromEgressSockPtr->recv(zmq::mutable_buffer(&egressAckHdr, sizeof(egressAckHdr)), zmq::recv_flags::none);
                if (!res) {
                    std::cerr << "[storage-worker] EgressAckHdr not received" << std::endl;
                    hdtn::Logger::getInstance()->logError("storage", "[storage-worker] EgressAckHdr not received");
                    return;
                }
                else if ((res->truncated()) || (res->size != sizeof(hdtn::EgressAckHdr))) {
                    std::cerr << "[storage-worker] EgressAckHdr wrong size received" << std::endl;
                    hdtn::Logger::getInstance()->logError("storage", "[storage-worker] EgressAckHdr wrong size received");
                    return;
                }
                else if (egressAckHdr.base.type != HDTN_MSGTYPE_EGRESS_ACK_TO_STORAGE) {
                    std::cerr << "[storage-worker] EgressAckHdr not type HDTN_MSGTYPE_EGRESS_ACK_TO_STORAGE, got " << egressAckHdr.base.type << std::endl;
                    hdtn::Logger::getInstance()->logError("storage", "[storage-worker] EgressAckHdr not type HDTN_MSGTYPE_EGRESS_ACK_TO_STORAGE");
                    return;
                }
                custodyid_set_t & custodyIdSet = finalDestEidToOpenCustIdsMap[egressAckHdr.finalDestEid];
                custodyid_set_t::iterator it = custodyIdSet.find(egressAckHdr.custodyId);
                if (it != custodyIdSet.end()) {
                    if (egressAckHdr.deleteNow) { //custody not requested, so don't wait on a custody signal to delete the bundle
                        bool successRemoveBundle = bsm.RemoveReadBundleFromDisk(egressAckHdr.custodyId);
                        if (!successRemoveBundle) {
                            std::cout << "error freeing bundle from disk\n";
                            hdtn::Logger::getInstance()->logError("storage", "Error freeing bundle from disk");
                        }
                        else {
                            ++m_totalBundlesErasedFromStorageNoCustodyTransfer;
                        }
                    }
                    custodyIdSet.erase(it);
                }
                    
            }
            if (pollItems[1].revents & ZMQ_POLLIN) { //worker inproc bundle data
                zmq::message_t rhdr;
                zmq::message_t rmsg;
                if (!inprocBundleDataSock.recv(rhdr, zmq::recv_flags::none)) {
                    continue;
                }
                //double check alignment (address should not have changed)
                if (((uintptr_t)rhdr.data()) & 7) {
                    std::cerr << "error in hdtn::ZmqStorageInterface::ThreadFunc (from worker inproc bundle data) zmq data not aligned on 8-byte boundary with message size " << rhdr.size() << std::endl;
                    continue;
                }
                //double check size
                if (rhdr.size() != sizeof(hdtn::ToStorageHdr)) {
                    std::cerr << "error in hdtn::ZmqStorageInterface::ThreadFunc (from worker inproc bundle data) rhdr.size() != sizeof(hdtn::ToStorageHdr) " << std::endl;
                    continue;
                }
                //already verified (type == HDTN_MSGTYPE_STORE) in dispatch() step
                if (!inprocBundleDataSock.recv(rmsg, zmq::recv_flags::none)) {
                    continue;
                }
                const hdtn::ToStorageHdr *toStorageHdr = (const hdtn::ToStorageHdr *)rhdr.data(); //rhdr verified above aligned on 8-byte boundary
                if (rhdr.size() != sizeof(hdtn::ToStorageHdr)) {
                    std::cerr << "[storage-worker] Invalid message format - header size mismatch (" << rhdr.size() << ")" << std::endl;
                    hdtn::Logger::getInstance()->logError("storage",
                        "[storage-worker] Invalid message format - header size mismatch (" + std::to_string(rhdr.size()) + ")");
                }
                
                cbhe_eid_t finalDestEidReturnedFromWrite;
                Write(&rmsg, bsm, custodyIdAllocator, ctm, bufferSpaceForCustodySignalRfc5050SerializedBundle, finalDestEidReturnedFromWrite, this);

                //send ack message to ingress
                //force natural/64-bit alignment
                hdtn::StorageAckHdr * storageAckHdr = new hdtn::StorageAckHdr();
                zmq::message_t zmqMessageStorageAckHdrWithDataStolen(storageAckHdr, sizeof(hdtn::StorageAckHdr), CustomCleanupStorageAckHdr, storageAckHdr);

                //memset 0 not needed because all values set below
                storageAckHdr->base.type = HDTN_MSGTYPE_STORAGE_ACK_TO_INGRESS;
                storageAckHdr->base.flags = 0;
                storageAckHdr->error = 0;
                storageAckHdr->finalDestEid = finalDestEidReturnedFromWrite;
                storageAckHdr->ingressUniqueId = toStorageHdr->ingressUniqueId;

                if (!toIngressSockPtr->send(std::move(zmqMessageStorageAckHdrWithDataStolen), zmq::send_flags::dontwait)) {
                    std::cout << "error: zmq could not send ingress an ack from storage" << std::endl;
                    hdtn::Logger::getInstance()->logError("storage", "Error: zmq could not send ingress an ack from storage");
                }
                
                
            }
            if (pollItems[2].revents & ZMQ_POLLIN) { //worker inproc release messages
                zmq::message_t rhdr;
                if (!inprocReleaseMessagesSock.recv(rhdr, zmq::recv_flags::none)) {
                    continue;
                }
                //double check alignment (address should not have changed)
                if (((uintptr_t)rhdr.data()) & 7) {
                    std::cerr << "error in hdtn::ZmqStorageInterface::ThreadFunc (from worker inproc release messages) zmq data not aligned on 8-byte boundary with message size " << rhdr.size() << std::endl;
                    continue;
                }
                hdtn::CommonHdr * commonHdr = (hdtn::CommonHdr *)rhdr.data();
                if (commonHdr->type == HDTN_MSGTYPE_ILINKUP) {
                    hdtn::IreleaseStartHdr * iReleaseStartHdr = (hdtn::IreleaseStartHdr *)rhdr.data(); //rhdr verified above aligned on 8-byte boundary
                    //ReleaseData(iReleaseStartHdr.flowId, iReleaseStartHdr.rate, iReleaseStartHdr.duration, &egressSock, bsm);
                    availableDestLinksSet.insert(iReleaseStartHdr->finalDestinationEid);
                    const std::string msg = "finalDestEid (" + boost::lexical_cast<std::string>(iReleaseStartHdr->finalDestinationEid.nodeId) + ","
                        + boost::lexical_cast<std::string>(iReleaseStartHdr->finalDestinationEid.serviceId) + ") will be released from storage";
                    std::cout << msg << std::endl;
                    hdtn::Logger::getInstance()->logNotification("storage", msg);

                    //availableDestLinksVec.clear();
                    std::string strVals = "[";
                    for (std::set<cbhe_eid_t>::const_iterator it = availableDestLinksSet.cbegin(); it != availableDestLinksSet.cend(); ++it) {
                        //availableDestLinksVec.push_back(*it);
                        strVals += "(" + boost::lexical_cast<std::string>((*it).nodeId) + ","
                            + boost::lexical_cast<std::string>((*it).serviceId) + "), ";
                    }
                    strVals += "]";
                    std::cout << "Currently Releasing Final Destination Eids: " << strVals << std::endl;
                    hdtn::Logger::getInstance()->logNotification("storage", "Currently Releasing Final Destination Eids: " + strVals);

                    //storageStillHasData = true;
                }
                else if (commonHdr->type == HDTN_MSGTYPE_ILINKDOWN) {
                    hdtn::IreleaseStopHdr * iReleaseStoptHdr = (hdtn::IreleaseStopHdr *)rhdr.data(); //rhdr verified above aligned on 8-byte boundary
                    const std::string msg = "finalDestEid (" + boost::lexical_cast<std::string>(iReleaseStoptHdr->finalDestinationEid.nodeId) + ","
                        + boost::lexical_cast<std::string>(iReleaseStoptHdr->finalDestinationEid.serviceId) + ") will STOP BEING released from storage";
                    std::cout << msg << std::endl;
                    hdtn::Logger::getInstance()->logNotification("storage", msg);
                    availableDestLinksSet.erase(iReleaseStoptHdr->finalDestinationEid);
                    //availableDestLinksVec.clear();
                    std::string strVals = "[";
                    for (std::set<cbhe_eid_t>::const_iterator it = availableDestLinksSet.cbegin(); it != availableDestLinksSet.cend(); ++it) {
                        //availableDestLinksVec.push_back(*it);
                        strVals += "(" + boost::lexical_cast<std::string>((*it).nodeId) + ","
                            + boost::lexical_cast<std::string>((*it).serviceId) + "), ";
                    }
                    strVals += "]";
                    std::cout << "Currently Releasing Final Destination Eids: " << strVals << std::endl;
                    hdtn::Logger::getInstance()->logNotification("storage", "Currently Releasing Final Destination Eids: " + strVals);
                }

            }
        }


        if ((acsSendNowExpiry <= boost::posix_time::microsec_clock::universal_time()) || (ctm.GetLargestNumberOfFills() > 100)) { //todo
            //std::cout << "send acs, fills = " << ctm.GetLargestNumberOfFills() << "\n";
            //test with generate all
            std::list<std::pair<bpv6_primary_block, std::vector<uint8_t> > > serializedPrimariesAndBundlesList;
            if (ctm.GenerateAllAcsBundlesAndClear(serializedPrimariesAndBundlesList)) {
                for(std::list<std::pair<bpv6_primary_block, std::vector<uint8_t> > >::iterator it = serializedPrimariesAndBundlesList.begin();
                    it != serializedPrimariesAndBundlesList.end(); ++it)
                {
                    WriteAcsBundle(bsm, custodyIdAllocator, *it);
                }
            }
            acsSendNowExpiry = boost::posix_time::microsec_clock::universal_time() + ACS_SEND_PERIOD;

        }
        
        //Send and maintain a maximum of 5 unacked bundles (per flow id) to Egress.
        //When a bundle is acked from egress using the head segment Id, the bundle is deleted from disk and a new bundle can be sent.
        static const uint64_t maxBundleSizeToRead = UINT64_MAX;// 65535 * 10;
        if (availableDestLinksSet.empty()) {
            timeoutPoll = DEFAULT_BIG_TIMEOUT_POLL;
        }
        else {
            std::vector<cbhe_eid_t> availableDestLinksNotCloggedVec;
            std::vector<cbhe_eid_t> availableDestLinksCloggedVec;
            for (std::set<cbhe_eid_t>::const_iterator it = availableDestLinksSet.cbegin(); it != availableDestLinksSet.cend(); ++it) {
                //std::cout << "flow " << flowId << " sz " << flowIdToOpenSessionsMap[flowId].size() << std::endl;
                if (finalDestEidToOpenCustIdsMap[*it].size() < 5) {//finaldesteid_opencustids_map_t finalDestEidToOpenCustIdsMap;
                    availableDestLinksNotCloggedVec.push_back(*it);
                }
                else {
                    availableDestLinksCloggedVec.push_back(*it);
                }
            }
            if (availableDestLinksNotCloggedVec.size() > 0) {
                if (ReleaseOne_NoBlock(sessionRead, availableDestLinksNotCloggedVec, egressSockPtr.get(), bsm, maxBundleSizeToRead)) { //true => (successfully sent to egress)
                    if (finalDestEidToOpenCustIdsMap[sessionRead.catalogEntryPtr->destEid].insert(sessionRead.custodyId).second) {
                        timeoutPoll = 0; //no timeout as we need to keep feeding to egress
                        ++m_totalBundlesSentToEgressFromStorage;
                    }
                    else {
                        std::cerr << "could not insert custody id into finalDestEidToOpenCustIdsMap\n";
                    }
                }
                else if (PeekOne(availableDestLinksCloggedVec, bsm) > 0) { //data available in storage for clogged links
                    timeoutPoll = 1; //shortest timeout 1ms as we wait for acks
                    ++totalEventsDataInStorageForCloggedLinks;
                }
                else { //no data in storage for any available links
                    timeoutPoll = DEFAULT_BIG_TIMEOUT_POLL;
                    ++totalEventsNoDataInStorageForAvailableLinks;
                }
            }
            else { //all links clogged up and need acks
                timeoutPoll = 1; //shortest timeout 1ms as we wait for acks
                ++totalEventsAllLinksClogged;
            }
        }
        
        //}
        

        /*hdtn::flow_stats stats = m_storeFlow.stats();
        m_workerStats.flow.disk_wbytes = stats.disk_wbytes;
        m_workerStats.flow.disk_wcount = stats.disk_wcount;
        m_workerStats.flow.disk_rbytes = stats.disk_rbytes;
        m_workerStats.flow.disk_rcount = stats.disk_rcount;*/
        
    }
    std::cout << "totalEventsAllLinksClogged: " << totalEventsAllLinksClogged << std::endl;
    std::cout << "totalEventsNoDataInStorageForAvailableLinks: " << totalEventsNoDataInStorageForAvailableLinks << std::endl;
    std::cout << "totalEventsDataInStorageForCloggedLinks: " << totalEventsDataInStorageForCloggedLinks << std::endl;
    std::cout << "m_numRfc5050CustodyTransfers: " << m_numRfc5050CustodyTransfers << std::endl;
    std::cout << "m_numAcsCustodyTransfers: " << m_numAcsCustodyTransfers << std::endl;
    std::cout << "m_numAcsPacketsReceived: " << m_numAcsPacketsReceived << std::endl;
    std::cout << "m_totalBundlesErasedFromStorageNoCustodyTransfer: " << m_totalBundlesErasedFromStorageNoCustodyTransfer << std::endl;
    std::cout << "m_totalBundlesErasedFromStorageWithCustodyTransfer: " << m_totalBundlesErasedFromStorageWithCustodyTransfer << std::endl;
    hdtn::Logger::getInstance()->logInfo("storage", "totalEventsAllLinksClogged: " + 
        std::to_string(totalEventsAllLinksClogged));
    hdtn::Logger::getInstance()->logInfo("storage", "totalEventsNoDataInStorageForAvailableLinks: " + 
        std::to_string(totalEventsNoDataInStorageForAvailableLinks));
    hdtn::Logger::getInstance()->logInfo("storage", "totalEventsDataInStorageForCloggedLinks: " + 
        std::to_string(totalEventsDataInStorageForCloggedLinks));
}




void hdtn::ZmqStorageInterface::launch() {
    if (!m_running) {
        m_running = true;
        std::cout << "[ZmqStorageInterface] Launching worker thread ..." << std::endl;
        hdtn::Logger::getInstance()->logNotification("storage", "[ZmqStorageInterface] Launching worker thread");
        m_threadPtr = boost::make_unique<boost::thread>(
                boost::bind(&ZmqStorageInterface::ThreadFunc, this)); //create and start the worker thread
    }
}
