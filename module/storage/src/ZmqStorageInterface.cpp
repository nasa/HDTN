/**
 * @file ZmqStorageInterface.cpp
 * @author  Brian Tomko <brian.j.tomko@nasa.gov>
 *
 * @copyright Copyright � 2021 United States Government as represented by
 * the National Aeronautics and Space Administration.
 * No copyright is claimed in the United States under Title 17, U.S.Code.
 * All Other Rights Reserved.
 *
 * @section LICENSE
 * Released under the NASA Open Source Agreement (NOSA)
 * See LICENSE.md in the source root directory for more information.
 */

#include <iostream>
#include "message.hpp"
#include "ZmqStorageInterface.h"
#include "BundleStorageManagerMT.h"
#include "BundleStorageManagerAsio.h"
#include "Logger.h"
#include <set>
#include <boost/lexical_cast.hpp>
#include <boost/make_unique.hpp>
#include "codec/CustodyIdAllocator.h"
#include "codec/CustodyTransferManager.h"
#include "Uri.h"
#include "CustodyTimers.h"
#include "codec/BundleViewV7.h"

typedef std::pair<cbhe_eid_t, bool> eid_plus_isanyserviceid_pair_t;

ZmqStorageInterface::ZmqStorageInterface() : m_running(false) {}

ZmqStorageInterface::~ZmqStorageInterface() {
    Stop();
}

void ZmqStorageInterface::Stop() {
    m_running = false; //thread stopping criteria
    if (m_threadPtr) {
        m_threadPtr->join();
        m_threadPtr.reset();
    }
}

bool ZmqStorageInterface::Init(const HdtnConfig & hdtnConfig, zmq::context_t * hdtnOneProcessZmqInprocContextPtr) {
    m_hdtnConfig = hdtnConfig;
    //according to ION.pdf v4.0.1 on page 100 it says:
    //  Remember that the format for this argument is ipn:element_number.0 and that
    //  the final 0 is required, as custodial/administration service is always service 0.
    //HDTN shall default m_myCustodialServiceId to 0 although it is changeable in the hdtn config json file
    M_HDTN_EID_CUSTODY.Set(m_hdtnConfig.m_myNodeId, m_hdtnConfig.m_myCustodialServiceId);
    m_hdtnOneProcessZmqInprocContextPtr = hdtnOneProcessZmqInprocContextPtr;

    //{

    m_zmqContextPtr = boost::make_unique<zmq::context_t>();
    

    if (hdtnOneProcessZmqInprocContextPtr) {
        m_zmqPushSock_connectingStorageToBoundEgressPtr = boost::make_unique<zmq::socket_t>(*m_hdtnOneProcessZmqInprocContextPtr, zmq::socket_type::pair);
        m_zmqPullSock_boundEgressToConnectingStoragePtr = boost::make_unique<zmq::socket_t>(*m_hdtnOneProcessZmqInprocContextPtr, zmq::socket_type::pair);
        m_zmqPushSock_connectingStorageToBoundIngressPtr = boost::make_unique<zmq::socket_t>(*m_hdtnOneProcessZmqInprocContextPtr, zmq::socket_type::pair);
        m_zmqPullSock_boundIngressToConnectingStoragePtr = boost::make_unique<zmq::socket_t>(*m_hdtnOneProcessZmqInprocContextPtr, zmq::socket_type::pair);
        m_zmqRepSock_connectingGuiToFromBoundStoragePtr = boost::make_unique<zmq::socket_t>(*m_hdtnOneProcessZmqInprocContextPtr, zmq::socket_type::pair);
        try {
            m_zmqPushSock_connectingStorageToBoundEgressPtr->connect(std::string("inproc://connecting_storage_to_bound_egress")); // egress should bind
            m_zmqPullSock_boundEgressToConnectingStoragePtr->connect(std::string("inproc://bound_egress_to_connecting_storage")); // egress should bind
            m_zmqPushSock_connectingStorageToBoundIngressPtr->connect(std::string("inproc://connecting_storage_to_bound_ingress"));
            m_zmqPullSock_boundIngressToConnectingStoragePtr->connect(std::string("inproc://bound_ingress_to_connecting_storage"));
            m_zmqRepSock_connectingGuiToFromBoundStoragePtr->bind(std::string("inproc://connecting_gui_to_from_bound_storage"));
        }
        catch (const zmq::error_t & ex) {
            std::cerr << "error in ZmqStorageInterface::Init: cannot connect inproc socket: " << ex.what() << std::endl;
            hdtn::Logger::getInstance()->logError("storage", "error in ZmqStorageInterface::Init: cannot connect inproc socket: " + std::string(ex.what()));
            return false;
        }
    }
    else {
        m_zmqPushSock_connectingStorageToBoundEgressPtr = boost::make_unique<zmq::socket_t>(*m_zmqContextPtr, zmq::socket_type::push);
        const std::string connect_connectingStorageToBoundEgressPath(
            std::string("tcp://") +
            m_hdtnConfig.m_zmqEgressAddress +
            std::string(":") +
            boost::lexical_cast<std::string>(m_hdtnConfig.m_zmqConnectingStorageToBoundEgressPortPath));
        
        m_zmqPullSock_boundEgressToConnectingStoragePtr = boost::make_unique<zmq::socket_t>(*m_zmqContextPtr, zmq::socket_type::pull);
        const std::string connect_boundEgressToConnectingStoragePath(
            std::string("tcp://") +
            m_hdtnConfig.m_zmqEgressAddress +
            std::string(":") +
            boost::lexical_cast<std::string>(m_hdtnConfig.m_zmqBoundEgressToConnectingStoragePortPath));
        
        m_zmqPushSock_connectingStorageToBoundIngressPtr = boost::make_unique<zmq::socket_t>(*m_zmqContextPtr, zmq::socket_type::push);
        const std::string connect_connectingStorageToBoundIngressPath(
            std::string("tcp://") +
            m_hdtnConfig.m_zmqIngressAddress +
            std::string(":") +
            boost::lexical_cast<std::string>(m_hdtnConfig.m_zmqConnectingStorageToBoundIngressPortPath));

        m_zmqPullSock_boundIngressToConnectingStoragePtr = boost::make_unique<zmq::socket_t>(*m_zmqContextPtr, zmq::socket_type::pull);
        const std::string connect_boundIngressToConnectingStoragePath(
            std::string("tcp://") +
            m_hdtnConfig.m_zmqIngressAddress +
            std::string(":") +
            boost::lexical_cast<std::string>(m_hdtnConfig.m_zmqBoundIngressToConnectingStoragePortPath));

        //from gui socket
        m_zmqRepSock_connectingGuiToFromBoundStoragePtr = boost::make_unique<zmq::socket_t>(*m_zmqContextPtr, zmq::socket_type::rep);
        const std::string bind_connectingGuiToFromBoundStoragePath("tcp://*:10303");

        try {
            m_zmqPushSock_connectingStorageToBoundEgressPtr->connect(connect_connectingStorageToBoundEgressPath); // egress should bind
            m_zmqPullSock_boundEgressToConnectingStoragePtr->connect(connect_boundEgressToConnectingStoragePath); // egress should bind
            m_zmqPushSock_connectingStorageToBoundIngressPtr->connect(connect_connectingStorageToBoundIngressPath);
            m_zmqPullSock_boundIngressToConnectingStoragePtr->connect(connect_boundIngressToConnectingStoragePath);
            m_zmqRepSock_connectingGuiToFromBoundStoragePtr->bind(bind_connectingGuiToFromBoundStoragePath);
        }
        catch (const zmq::error_t & ex) {
            std::cerr << "error: cannot connect socket: " << ex.what() << std::endl;
            hdtn::Logger::getInstance()->logError("storage", "Error: cannot connect socket: " + std::string(ex.what()));
            return false;
        }
    }

    m_zmqSubSock_boundReleaseToConnectingStoragePtr = boost::make_unique<zmq::socket_t>(*m_zmqContextPtr, zmq::socket_type::sub);
    const std::string connect_boundSchedulerPubSubPath(
        std::string("tcp://") +
        m_hdtnConfig.m_zmqSchedulerAddress +
        std::string(":") +
        boost::lexical_cast<std::string>(m_hdtnConfig.m_zmqBoundSchedulerPubSubPortPath));
    try {
        m_zmqSubSock_boundReleaseToConnectingStoragePtr->connect(connect_boundSchedulerPubSubPath);// config.releaseWorker);
        m_zmqSubSock_boundReleaseToConnectingStoragePtr->set(zmq::sockopt::subscribe, "");
        std::cout << "release sock connected to " << connect_boundSchedulerPubSubPath << std::endl;
        hdtn::Logger::getInstance()->logNotification("storage", "Release sock connected to " + connect_boundSchedulerPubSubPath);
    }
    catch (const zmq::error_t & ex) {
        std::cerr << "error: cannot connect release socket: " << ex.what() << std::endl;
        hdtn::Logger::getInstance()->logError("storage", "Error: cannot connect release socket: " + std::string(ex.what()));
        return false;
    }

    // Use a form of receive that times out so we can terminate cleanly.
    try {
        static const int timeout = 250;  // milliseconds
        m_zmqPullSock_boundEgressToConnectingStoragePtr->set(zmq::sockopt::rcvtimeo, timeout);
        m_zmqPullSock_boundIngressToConnectingStoragePtr->set(zmq::sockopt::rcvtimeo, timeout);
        m_zmqSubSock_boundReleaseToConnectingStoragePtr->set(zmq::sockopt::rcvtimeo, timeout);
    }
    catch (const zmq::error_t & ex) {
        std::cerr << "error: cannot set timeout on receive sockets: " << ex.what() << std::endl;
        hdtn::Logger::getInstance()->logError("storage", "Error: cannot set timeout on receive sockets: " + std::string(ex.what()));
        return false;
    }
    
   
    
    if (!m_running) {
        m_running = true;
        std::cout << "[ZmqStorageInterface] Launching worker thread ..." << std::endl;
        hdtn::Logger::getInstance()->logNotification("storage", "[ZmqStorageInterface] Launching worker thread");
        m_threadStartupComplete = false;
        m_threadPtr = boost::make_unique<boost::thread>(
            boost::bind(&ZmqStorageInterface::ThreadFunc, this)); //create and start the worker thread
        for (unsigned int attempt = 0; attempt < 10; ++attempt) {
            if (m_threadStartupComplete) {
                break;
            }
            std::cout << "waiting for worker thread to start up...\n";
            boost::this_thread::sleep(boost::posix_time::seconds(1));
        }
        if (!m_threadStartupComplete) {
            std::cout << "error: storage thread took too long to start up.. exiting\n";
            return false;
        }
        else {
            std::cout << "worker thread started\n";
        }
    }
    return true;
}

static bool WriteAcsBundle(BundleStorageManagerBase & bsm, CustodyIdAllocator & custodyIdAllocator,
    const Bpv6CbhePrimaryBlock & primary, const std::vector<uint8_t> & acsBundleSerialized)
{
    const cbhe_eid_t & hdtnSrcEid = primary.m_sourceNodeId;
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
    CustodyTimers & custodyTimers,
    BundleViewV6 & custodySignalRfc5050RenderedBundleView,
    cbhe_eid_t & finalDestEidReturned, ZmqStorageInterface * forStats)
{
    
    
    const uint8_t firstByte = ((const uint8_t*)message->data())[0];
    const bool isBpVersion6 = (firstByte == 6);
    const bool isBpVersion7 = (firstByte == ((4U << 5) | 31U));  //CBOR major type 4, additional information 31 (Indefinite-Length Array)
    if (isBpVersion6) {
        BundleViewV6 bv;
        if (!bv.LoadBundle((uint8_t *)message->data(), message->size())) { //invalid bundle
            std::cerr << "malformed bundle\n";
            return false;
        }
        const Bpv6CbhePrimaryBlock & primary = bv.m_primaryBlockView.header;
        finalDestEidReturned = primary.m_destinationEid;
        

        //admin records pertaining to this hdtn node do not get written to disk.. they signal a deletion from disk
        static const BPV6_BUNDLEFLAG requiredPrimaryFlagsForAdminRecord = BPV6_BUNDLEFLAG::SINGLETON | BPV6_BUNDLEFLAG::ADMINRECORD;
        if (((primary.m_bundleProcessingControlFlags & requiredPrimaryFlagsForAdminRecord) == requiredPrimaryFlagsForAdminRecord) && (finalDestEidReturned == forStats->M_HDTN_EID_CUSTODY)) {
            std::vector<BundleViewV6::Bpv6CanonicalBlockView*> blocks;
            bv.GetCanonicalBlocksByType(BPV6_BLOCK_TYPE_CODE::PAYLOAD, blocks);
            if (blocks.size() != 1) {
                std::cerr << "error admin record does not have a payload block\n";
                return false;
            }
            Bpv6AdministrativeRecord* adminRecordBlockPtr = dynamic_cast<Bpv6AdministrativeRecord*>(blocks[0]->headerPtr.get());
            if (adminRecordBlockPtr == NULL) {
                std::cerr << "error null Bpv6AdministrativeRecord\n";
                return false;
            }
            const BPV6_ADMINISTRATIVE_RECORD_TYPE_CODE adminRecordType = adminRecordBlockPtr->m_adminRecordTypeCode;
            
            if (adminRecordType == BPV6_ADMINISTRATIVE_RECORD_TYPE_CODE::AGGREGATE_CUSTODY_SIGNAL) {
                ++forStats->m_numAcsPacketsReceived;
                //check acs
                Bpv6AdministrativeRecordContentAggregateCustodySignal * acsPtr = dynamic_cast<Bpv6AdministrativeRecordContentAggregateCustodySignal*>(adminRecordBlockPtr->m_adminRecordContentPtr.get());
                if (acsPtr == NULL) {
                    std::cerr << "error null AggregateCustodySignal\n";
                    return false;
                }
                Bpv6AdministrativeRecordContentAggregateCustodySignal & acs = *(reinterpret_cast<Bpv6AdministrativeRecordContentAggregateCustodySignal*>(acsPtr));
                if (!acs.DidCustodyTransferSucceed()) {
                    std::cerr << "custody transfer failed with reason code " << static_cast<unsigned int>(acs.GetReasonCode()) << "\n";
                    return false;
                }

                //todo figure out what to do with failed custody from next hop
                for (std::set<FragmentSet::data_fragment_t>::const_iterator it = acs.m_custodyIdFills.cbegin(); it != acs.m_custodyIdFills.cend(); ++it) {
                    forStats->m_numAcsCustodyTransfers += (it->endIndex + 1) - it->beginIndex;
                    custodyIdAllocator.FreeCustodyIdRange(it->beginIndex, it->endIndex);
                    for (uint64_t currentCustodyId = it->beginIndex; currentCustodyId <= it->endIndex; ++currentCustodyId) {
                        catalog_entry_t * catalogEntryPtr = bsm.GetCatalogEntryPtrFromCustodyId(currentCustodyId);
                        if (catalogEntryPtr == NULL) {
                            std::cout << "error finding catalog entry for bundle identified by acs custody signal\n";
                            continue;
                        }
                        if (!custodyTimers.CancelCustodyTransferTimer(catalogEntryPtr->destEid, currentCustodyId)) {
                            std::cout << "notice: can't find custody timer associated with bundle identified by acs custody signal\n";
                        }
                        if (!bsm.RemoveReadBundleFromDisk(catalogEntryPtr, currentCustodyId)) {
                            std::cout << "error freeing bundle identified by acs custody signal from disk\n";
                            continue;
                        }
                        ++forStats->m_totalBundlesErasedFromStorageWithCustodyTransfer;
                    }
                }
            }
            else if (adminRecordType == BPV6_ADMINISTRATIVE_RECORD_TYPE_CODE::CUSTODY_SIGNAL) { //rfc5050 style custody transfer
                //check acs
                Bpv6AdministrativeRecordContentCustodySignal * csPtr = dynamic_cast<Bpv6AdministrativeRecordContentCustodySignal*>(adminRecordBlockPtr->m_adminRecordContentPtr.get());
                if (csPtr == NULL) {
                    std::cerr << "error null CustodySignal\n";
                    return false;
                }
                Bpv6AdministrativeRecordContentCustodySignal & cs = *(reinterpret_cast<Bpv6AdministrativeRecordContentCustodySignal*>(csPtr));
                if (!cs.DidCustodyTransferSucceed()) {
                    std::cerr << "custody transfer failed with reason code " << cs.GetReasonCode() << "\n";
                    return false;
                }
                uint64_t * custodyIdPtr;
                if (cs.m_isFragment) {
                    cbhe_bundle_uuid_t uuid;
                    if (!Uri::ParseIpnUriString(cs.m_bundleSourceEid, uuid.srcEid.nodeId, uuid.srcEid.serviceId)) {
                        std::cerr << "error custody signal with bad ipn string\n";
                        return false;
                    }
                    uuid.creationSeconds = cs.m_copyOfBundleCreationTimestamp.secondsSinceStartOfYear2000;
                    uuid.sequence = cs.m_copyOfBundleCreationTimestamp.sequenceNumber;
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
                    uuid.creationSeconds = cs.m_copyOfBundleCreationTimestamp.secondsSinceStartOfYear2000;
                    uuid.sequence = cs.m_copyOfBundleCreationTimestamp.sequenceNumber;
                    //std::cout << "uuid: " << "cs " << uuid.creationSeconds << "  seq " << uuid.sequence << "  " << uuid.srcEid.nodeId << "," << uuid.srcEid.serviceId << std::endl;
                    custodyIdPtr = bsm.GetCustodyIdFromUuid(uuid);
                }
                if (custodyIdPtr == NULL) {
                    std::cerr << "error custody signal does not match a bundle in the storage database\n";
                    return false;
                }
                const uint64_t custodyIdFromRfc5050 = *custodyIdPtr;
                custodyIdAllocator.FreeCustodyId(custodyIdFromRfc5050);
                catalog_entry_t * catalogEntryPtr = bsm.GetCatalogEntryPtrFromCustodyId(custodyIdFromRfc5050);
                if (catalogEntryPtr == NULL) {
                    std::cout << "error finding catalog entry for bundle identified by rfc5050 custody signal\n";
                    return false;
                }
                if (!custodyTimers.CancelCustodyTransferTimer(catalogEntryPtr->destEid, custodyIdFromRfc5050)) {
                    std::cout << "notice: can't find custody timer associated with bundle identified by rfc5050 custody signal\n";
                }
                if (!bsm.RemoveReadBundleFromDisk(catalogEntryPtr, custodyIdFromRfc5050)) {
                    std::cout << "error freeing bundle identified by rfc5050 custody signal from disk\n";
                    return false;
                }
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
        const uint64_t newCustodyId = custodyIdAllocator.GetNextCustodyIdForNextHopCtebToSend(primary.m_sourceNodeId);
        static const BPV6_BUNDLEFLAG requiredPrimaryFlagsForCustody = BPV6_BUNDLEFLAG::SINGLETON | BPV6_BUNDLEFLAG::CUSTODY_REQUESTED;
        if ((primary.m_bundleProcessingControlFlags & requiredPrimaryFlagsForCustody) == requiredPrimaryFlagsForCustody) {
            if (!ctm.ProcessCustodyOfBundle(bv, true, newCustodyId, BPV6_ACS_STATUS_REASON_INDICES::SUCCESS__NO_ADDITIONAL_INFORMATION,
                custodySignalRfc5050RenderedBundleView)) {
                std::cerr << "error unable to process custody\n";
            }
            else if (!bv.Render(message->size() + 200)) { //hdtn modifies bundle for next hop
                std::cerr << "error unable to render new bundle\n";
            }
            else {
                if (custodySignalRfc5050RenderedBundleView.m_renderedBundle.size()) {
                    const cbhe_eid_t & hdtnSrcEid = custodySignalRfc5050RenderedBundleView.m_primaryBlockView.header.m_sourceNodeId;
                    const uint64_t newCustodyIdFor5050CustodySignal = custodyIdAllocator.GetNextCustodyIdForNextHopCtebToSend(hdtnSrcEid);

                    //write custody signal to disk
                    BundleStorageManagerSession_WriteToDisk sessionWrite;
                    const uint64_t totalSegmentsRequired = bsm.Push(sessionWrite,
                        custodySignalRfc5050RenderedBundleView.m_primaryBlockView.header,
                        custodySignalRfc5050RenderedBundleView.m_renderedBundle.size());
                    //std::cout << "totalSegmentsRequired " << totalSegmentsRequired << "\n";
                    if (totalSegmentsRequired == 0) {
                        const std::string msg = "out of space for custody signal";
                        std::cerr << msg << "\n";
                        hdtn::Logger::getInstance()->logError("storage", msg);
                        return false;
                    }

                    const uint64_t totalBytesPushed = bsm.PushAllSegments(sessionWrite, custodySignalRfc5050RenderedBundleView.m_primaryBlockView.header,
                        newCustodyIdFor5050CustodySignal, (const uint8_t*)custodySignalRfc5050RenderedBundleView.m_renderedBundle.data(),
                        custodySignalRfc5050RenderedBundleView.m_renderedBundle.size());
                    if (totalBytesPushed != custodySignalRfc5050RenderedBundleView.m_renderedBundle.size()) {
                        const std::string msg = "totalBytesPushed != custodySignalRfc5050RenderedBundleView.m_renderedBundle.size()";
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
    else if (isBpVersion7) {
        BundleViewV7 bv;
        if (!bv.LoadBundle((uint8_t *)message->data(), message->size(), true, true)) { //invalid bundle
            std::cerr << "malformed bundle\n";
            return false;
        }
        const Bpv7CbhePrimaryBlock & primary = bv.m_primaryBlockView.header;
        finalDestEidReturned = primary.m_destinationEid;

        //write bundle
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
        const uint64_t newCustodyId = custodyIdAllocator.GetNextCustodyIdForNextHopCtebToSend(primary.m_sourceNodeId);
        const uint64_t totalBytesPushed = bsm.PushAllSegments(sessionWrite, primary, newCustodyId, (const uint8_t*)bv.m_renderedBundle.data(), bv.m_renderedBundle.size());
        if (totalBytesPushed != bv.m_renderedBundle.size()) {
            const std::string msg = "totalBytesPushed != size";
            std::cerr << msg << "\n";
            hdtn::Logger::getInstance()->logError("storage", msg);
            return false;
        }
        return true;
    }
    else {
        std::cout << "error in ZmqStorageInterface Write: unsupported bundle version detected\n";
        return false;
    }
    
}

//return number of bytes to read for specified links
static uint64_t PeekOne(const std::vector<eid_plus_isanyserviceid_pair_t> & availableDestLinks, BundleStorageManagerBase & bsm) {
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
static void CustomCleanupStdVecUint8(void *data, void *hint) {
    //std::cout << "free " << static_cast<std::vector<uint8_t>*>(hint)->size() << std::endl;
    delete static_cast<std::vector<uint8_t>*>(hint);
}

static bool ReleaseOne_NoBlock(BundleStorageManagerSession_ReadFromDisk & sessionRead,
    const std::vector<eid_plus_isanyserviceid_pair_t> & availableDestLinks,
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
        
    std::vector<uint8_t> * vecUint8BundleDataRawPointer = new std::vector<uint8_t>();
    const bool successReadAllSegments = bsm.ReadAllSegments(sessionRead, *vecUint8BundleDataRawPointer);
    zmq::message_t zmqBundleDataMessageWithDataStolen(vecUint8BundleDataRawPointer->data(), vecUint8BundleDataRawPointer->size(), CustomCleanupStdVecUint8, vecUint8BundleDataRawPointer);
        
    //std::cout << "totalBytesRead " << totalBytesRead << "\n";
    if (!successReadAllSegments) {
        std::cout << "error: unable to read all segments from disk\n";
        hdtn::Logger::getInstance()->logError("storage", "error: unable to read all segments from disk");
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
    if (!egressSock->send(std::move(zmqBundleDataMessageWithDataStolen), zmq::send_flags::dontwait)) {
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

static void PrintReleasedLinks(const std::set<eid_plus_isanyserviceid_pair_t> & availableDestLinksSet) {
    std::string strVals = "[";
    for (std::set<eid_plus_isanyserviceid_pair_t>::const_iterator it = availableDestLinksSet.cbegin(); it != availableDestLinksSet.cend(); ++it) {
        if (it->second) { //any service id
            strVals += Uri::GetIpnUriStringAnyServiceNumber((*it).first.nodeId) + ", ";
        }
        else { //fully qualified
            strVals += Uri::GetIpnUriString((*it).first.nodeId, (*it).first.serviceId) + ", ";
        }
    }
    strVals += "]";
    std::cout << "Currently Releasing Final Destination Eids: " << strVals << std::endl;
    hdtn::Logger::getInstance()->logNotification("storage", "Currently Releasing Final Destination Eids: " + strVals);
}

void ZmqStorageInterface::ThreadFunc() {
    BundleStorageManagerSession_ReadFromDisk sessionRead; //reuse this due to expensive heap allocation
    BundleViewV6 custodySignalRfc5050RenderedBundleView;
    custodySignalRfc5050RenderedBundleView.m_frontBuffer.reserve(2000);
    custodySignalRfc5050RenderedBundleView.m_backBuffer.reserve(2000);
    CustodyIdAllocator custodyIdAllocator;
    CustodyTimers custodyTimers(boost::posix_time::milliseconds(m_hdtnConfig.m_retransmitBundleAfterNoCustodySignalMilliseconds));
    const bool IS_HDTN_ACS_AWARE = m_hdtnConfig.m_isAcsAware;
    const uint64_t ACS_MAX_FILLS_PER_ACS_PACKET = m_hdtnConfig.m_acsMaxFillsPerAcsPacket;
    
    static const boost::posix_time::time_duration ACS_SEND_PERIOD = boost::posix_time::milliseconds(m_hdtnConfig.m_acsSendPeriodMilliseconds);
    CustodyTransferManager ctm(IS_HDTN_ACS_AWARE, M_HDTN_EID_CUSTODY.nodeId, M_HDTN_EID_CUSTODY.serviceId);
    std::cout << "[storage-worker] Worker thread starting up." << std::endl;
    hdtn::Logger::getInstance()->logNotification("storage", "Worker thread starting up");

   

    
    
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
    

    typedef std::set<uint64_t> custodyid_set_t;
    typedef std::map<cbhe_eid_t, custodyid_set_t> finaldesteid_opencustids_map_t;

    std::vector<eid_plus_isanyserviceid_pair_t> availableDestLinksNotCloggedVec;
    availableDestLinksNotCloggedVec.reserve(100); //todo
    std::vector<eid_plus_isanyserviceid_pair_t> availableDestLinksCloggedVec;
    availableDestLinksCloggedVec.reserve(100); //todo

    m_totalBundlesErasedFromStorageNoCustodyTransfer = 0;
    m_totalBundlesErasedFromStorageWithCustodyTransfer = 0;
    m_totalBundlesSentToEgressFromStorage = 0;
    m_numRfc5050CustodyTransfers = 0;
    m_numAcsCustodyTransfers = 0;
    m_numAcsPacketsReceived = 0;
    std::size_t totalEventsAllLinksClogged = 0;
    std::size_t totalEventsNoDataInStorageForAvailableLinks = 0;
    std::size_t totalEventsDataInStorageForCloggedLinks = 0;
    std::size_t numCustodyTransferTimeouts = 0;

    std::set<eid_plus_isanyserviceid_pair_t> availableDestLinksSet;
    finaldesteid_opencustids_map_t finalDestEidToOpenCustIdsMap;

    static constexpr std::size_t minBufSizeBytesReleaseMessages = sizeof(uint64_t) + 
        ((sizeof(hdtn::IreleaseStartHdr) > sizeof(hdtn::IreleaseStopHdr)) ? sizeof(hdtn::IreleaseStartHdr) : sizeof(hdtn::IreleaseStopHdr));
    uint64_t rxBufReleaseMessagesAlign64[minBufSizeBytesReleaseMessages / sizeof(uint64_t)];

    zmq::pollitem_t pollItems[4] = {
        {m_zmqPullSock_boundEgressToConnectingStoragePtr->handle(), 0, ZMQ_POLLIN, 0},
        {m_zmqPullSock_boundIngressToConnectingStoragePtr->handle(), 0, ZMQ_POLLIN, 0},
        {m_zmqSubSock_boundReleaseToConnectingStoragePtr->handle(), 0, ZMQ_POLLIN, 0},
        {m_zmqRepSock_connectingGuiToFromBoundStoragePtr->handle(), 0, ZMQ_POLLIN, 0}
    };
    static const long DEFAULT_BIG_TIMEOUT_POLL = 250;
    long timeoutPoll = DEFAULT_BIG_TIMEOUT_POLL; //0 => no blocking
    boost::posix_time::ptime acsSendNowExpiry = boost::posix_time::microsec_clock::universal_time() + ACS_SEND_PERIOD;
    m_threadStartupComplete = true;
    while (m_running) {
        int rc = 0;
        try {
            rc = zmq::poll(pollItems, 4, timeoutPoll);
        }
        catch (zmq::error_t & e) {
            std::cout << "caught zmq::error_t in hdtn::ZmqStorageInterface::ThreadFunc: " << e.what() << std::endl;
            continue;
        }
        if (rc > 0) {            
            if (pollItems[0].revents & ZMQ_POLLIN) { //from egress sock
                hdtn::EgressAckHdr egressAckHdr;
                const zmq::recv_buffer_result_t res = m_zmqPullSock_boundEgressToConnectingStoragePtr->recv(zmq::mutable_buffer(&egressAckHdr, sizeof(egressAckHdr)), zmq::recv_flags::none);
                if (!res) {
                    std::cerr << "[storage-worker] EgressAckHdr not received" << std::endl;
                    hdtn::Logger::getInstance()->logError("storage", "[storage-worker] EgressAckHdr not received");
                    continue;
                }
                else if ((res->truncated()) || (res->size != sizeof(hdtn::EgressAckHdr))) {
                    std::cerr << "[storage-worker] EgressAckHdr wrong size received" << std::endl;
                    hdtn::Logger::getInstance()->logError("storage", "[storage-worker] EgressAckHdr wrong size received");
                    continue;
                }
                else if (egressAckHdr.base.type != HDTN_MSGTYPE_EGRESS_ACK_TO_STORAGE) {
                    std::cerr << "[storage-worker] EgressAckHdr not type HDTN_MSGTYPE_EGRESS_ACK_TO_STORAGE, got " << egressAckHdr.base.type << std::endl;
                    hdtn::Logger::getInstance()->logError("storage", "[storage-worker] EgressAckHdr not type HDTN_MSGTYPE_EGRESS_ACK_TO_STORAGE");
                    continue;
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
            if (pollItems[1].revents & ZMQ_POLLIN) { //from ingress bundle data
                hdtn::ToStorageHdr toStorageHeader;
                const zmq::recv_buffer_result_t res = m_zmqPullSock_boundIngressToConnectingStoragePtr->recv(zmq::mutable_buffer(&toStorageHeader, sizeof(hdtn::ToStorageHdr)), zmq::recv_flags::none);
                if (!res) {
                    std::cerr << "error in hdtn::ZmqStorageInterface::ThreadFunc (from ingress bundle data) message hdr not received" << std::endl;
                    hdtn::Logger::getInstance()->logError("storage", "error in hdtn::ZmqStorageInterface::ThreadFunc (from ingress bundle data) message hdr not received");
                    continue;
                }
                else if ((res->truncated()) || (res->size != sizeof(hdtn::ToStorageHdr))) {
                    std::cerr << "error in hdtn::ZmqStorageInterface::ThreadFunc (from ingress bundle data) rhdr.size() != sizeof(hdtn::ToStorageHdr)" << std::endl;
                    hdtn::Logger::getInstance()->logError("storage", "error in hdtn::ZmqStorageInterface::ThreadFunc (from ingress bundle data) rhdr.size() != sizeof(hdtn::ToStorageHdr)");
                    continue;
                }
                else if (toStorageHeader.base.type == HDTN_MSGTYPE_STORAGE_ADD_OPPORTUNISTIC_LINK) {
                    const uint64_t nodeId = toStorageHeader.ingressUniqueId;
                    const std::string msg = "finalDestEid ("
                        + Uri::GetIpnUriStringAnyServiceNumber(nodeId)
                        + ") will be released from storage";
                    std::cout << msg << std::endl;
                    hdtn::Logger::getInstance()->logNotification("storage", msg);
                    availableDestLinksSet.emplace(cbhe_eid_t(nodeId, 0), true); //true => any service id.. 0 is don't care
                    PrintReleasedLinks(availableDestLinksSet);
                    continue;
                }
                else if (toStorageHeader.base.type == HDTN_MSGTYPE_STORAGE_REMOVE_OPPORTUNISTIC_LINK) {
                    const uint64_t nodeId = toStorageHeader.ingressUniqueId;
                    const std::string msg = "finalDestEid ("
                        + Uri::GetIpnUriStringAnyServiceNumber(nodeId)
                        + ") will STOP being released from storage";
                    std::cout << msg << std::endl;
                    hdtn::Logger::getInstance()->logNotification("storage", msg);
                    availableDestLinksSet.erase(eid_plus_isanyserviceid_pair_t(cbhe_eid_t(nodeId, 0), true)); //true => any service id.. 0 is don't care
                    PrintReleasedLinks(availableDestLinksSet);
                    continue;
                }
                else if (toStorageHeader.base.type != HDTN_MSGTYPE_STORE) {
                    std::cerr << "error in hdtn::ZmqStorageInterface::ThreadFunc (from ingress bundle data) message type not HDTN_MSGTYPE_STORE" << std::endl;
                    hdtn::Logger::getInstance()->logError("storage", "error in hdtn::ZmqStorageInterface::ThreadFunc (from ingress bundle data) message type not HDTN_MSGTYPE_STORE");
                    continue;
                }

                storageStats.inBytes += sizeof(hdtn::ToStorageHdr);
                ++storageStats.inMsg;

                zmq::message_t zmqBundleDataReceived;
                if (!m_zmqPullSock_boundIngressToConnectingStoragePtr->recv(zmqBundleDataReceived, zmq::recv_flags::none)) {
                    std::cerr << "error in hdtn::ZmqStorageInterface::ThreadFunc (from ingress bundle data) message not received" << std::endl;
                    hdtn::Logger::getInstance()->logError("storage", "error in hdtn::ZmqStorageInterface::ThreadFunc (from ingress bundle data) message not received");
                    continue;
                }
                storageStats.inBytes += zmqBundleDataReceived.size();
                
                cbhe_eid_t finalDestEidReturnedFromWrite;
                Write(&zmqBundleDataReceived, bsm, custodyIdAllocator, ctm, custodyTimers, custodySignalRfc5050RenderedBundleView, finalDestEidReturnedFromWrite, this);

                //send ack message to ingress
                //force natural/64-bit alignment
                hdtn::StorageAckHdr * storageAckHdr = new hdtn::StorageAckHdr();
                zmq::message_t zmqMessageStorageAckHdrWithDataStolen(storageAckHdr, sizeof(hdtn::StorageAckHdr), CustomCleanupStorageAckHdr, storageAckHdr);

                //memset 0 not needed because all values set below
                storageAckHdr->base.type = HDTN_MSGTYPE_STORAGE_ACK_TO_INGRESS;
                storageAckHdr->base.flags = 0;
                storageAckHdr->error = 0;
                storageAckHdr->finalDestEid = finalDestEidReturnedFromWrite;
                storageAckHdr->ingressUniqueId = toStorageHeader.ingressUniqueId;

                if (!m_zmqPushSock_connectingStorageToBoundIngressPtr->send(std::move(zmqMessageStorageAckHdrWithDataStolen), zmq::send_flags::dontwait)) {
                    std::cout << "error: zmq could not send ingress an ack from storage" << std::endl;
                    hdtn::Logger::getInstance()->logError("storage", "Error: zmq could not send ingress an ack from storage");
                }
            }
            if (pollItems[2].revents & ZMQ_POLLIN) { //release messages
                //force this hdtn message struct to be aligned on a 64-byte boundary using zmq::mutable_buffer
                const zmq::recv_buffer_result_t res = m_zmqSubSock_boundReleaseToConnectingStoragePtr->recv(
                    zmq::mutable_buffer(rxBufReleaseMessagesAlign64, minBufSizeBytesReleaseMessages), zmq::recv_flags::none);
                if (!res) {
                    std::cerr << "[schedule release] message not received" << std::endl;
                    hdtn::Logger::getInstance()->logError("storage", "[schedule release] message not received");
                    continue;
                }
                else if (res->size < sizeof(hdtn::CommonHdr)) {
                    std::cerr << "[schedule release] res->size < sizeof(hdtn::CommonHdr)" << std::endl;
                    hdtn::Logger::getInstance()->logError("storage", "[schedule release] res->size < sizeof(hdtn::CommonHdr)");
                    continue;
                }

                std::cout << "release message received\n";
                hdtn::Logger::getInstance()->logNotification("storage", "Release message received");
                hdtn::CommonHdr *commonHdr = (hdtn::CommonHdr *)rxBufReleaseMessagesAlign64;
                if (commonHdr->type == HDTN_MSGTYPE_ILINKUP) {
                    if (res->size != sizeof(hdtn::IreleaseStartHdr)) {
                        std::cerr << "[schedule release] res->size != sizeof(hdtn::IreleaseStartHdr)" << std::endl;
                        hdtn::Logger::getInstance()->logError("storage", "[schedule release] res->size != sizeof(hdtn::IreleaseStartHdr)");
                        continue;
                    }

                    hdtn::IreleaseStartHdr * iReleaseStartHdr = (hdtn::IreleaseStartHdr *)rxBufReleaseMessagesAlign64;
                    const std::string msg = "finalDestEid (" 
                        + Uri::GetIpnUriString(iReleaseStartHdr->finalDestinationEid.nodeId, iReleaseStartHdr->finalDestinationEid.serviceId) 
                        + ") will be released from storage";
                    std::cout << msg << std::endl;
                    hdtn::Logger::getInstance()->logNotification("storage", msg);
                    availableDestLinksSet.emplace(iReleaseStartHdr->finalDestinationEid, false); //false => fully qualified service id
                    availableDestLinksSet.emplace(iReleaseStartHdr->nextHopEid, false);
		}
                else if (commonHdr->type == HDTN_MSGTYPE_ILINKDOWN) {
                    if (res->size != sizeof(hdtn::IreleaseStopHdr)) {
                        std::cerr << "[schedule release] res->size != sizeof(hdtn::IreleaseStopHdr)" << std::endl;
                        hdtn::Logger::getInstance()->logError("storage", "[schedule release] res->size != sizeof(hdtn::IreleaseStopHdr)");
                        continue;
                    }

                    hdtn::IreleaseStopHdr * iReleaseStoptHdr = (hdtn::IreleaseStopHdr *)rxBufReleaseMessagesAlign64;
                    const std::string msg = "finalDestEid (" + boost::lexical_cast<std::string>(iReleaseStoptHdr->finalDestinationEid.nodeId) + ","
                        + boost::lexical_cast<std::string>(iReleaseStoptHdr->finalDestinationEid.serviceId) + ") will STOP BEING released from storage";
                    std::cout << msg << std::endl;
                    hdtn::Logger::getInstance()->logNotification("storage", msg);
                    availableDestLinksSet.erase(eid_plus_isanyserviceid_pair_t(iReleaseStoptHdr->finalDestinationEid, false)); //false => fully qualified service id
                    availableDestLinksSet.erase(eid_plus_isanyserviceid_pair_t(iReleaseStoptHdr->nextHopEid, false)); //false => fully qualified service id

		}
                PrintReleasedLinks(availableDestLinksSet);
            }
            if (pollItems[3].revents & ZMQ_POLLIN) { //gui requests data
                uint8_t guiMsgByte;
                const zmq::recv_buffer_result_t res = m_zmqRepSock_connectingGuiToFromBoundStoragePtr->recv(zmq::mutable_buffer(&guiMsgByte, sizeof(guiMsgByte)), zmq::recv_flags::dontwait);
                if (!res) {
                    std::cerr << "error in ZmqStorageInterface::ThreadFunc: cannot read guiMsgByte" << std::endl;
                }
                else if ((res->truncated()) || (res->size != sizeof(guiMsgByte))) {
                    std::cerr << "guiMsgByte message mismatch: untruncated = " << res->untruncated_size
                        << " truncated = " << res->size << " expected = " << sizeof(guiMsgByte) << std::endl;
                }
                else if (guiMsgByte != 1) {
                    std::cerr << "error guiMsgByte not 1\n";
                }
                else {
                    //send telemetry
                    //std::cout << "storage send telem\n";
                    StorageTelemetry_t telem;
                    telem.totalBundlesErasedFromStorage = GetCurrentNumberOfBundlesDeletedFromStorage();
                    telem.totalBundlesSentToEgressFromStorage = m_totalBundlesSentToEgressFromStorage;
                    if (!m_zmqRepSock_connectingGuiToFromBoundStoragePtr->send(zmq::const_buffer(&telem, sizeof(telem)), zmq::send_flags::dontwait)) {
                        std::cerr << "storage can't send telemetry to gui" << std::endl;
                    }
                }
            }
        }

        const boost::posix_time::ptime nowPtime = boost::posix_time::microsec_clock::universal_time();
        if ((acsSendNowExpiry <= nowPtime) || (ctm.GetLargestNumberOfFills() > ACS_MAX_FILLS_PER_ACS_PACKET)) {
            //std::cout << "send acs, fills = " << ctm.GetLargestNumberOfFills() << "\n";
            //test with generate all
            std::list<BundleViewV6> newAcsRenderedBundleViewList;
            if (ctm.GenerateAllAcsBundlesAndClear(newAcsRenderedBundleViewList)) {
                for(std::list<BundleViewV6>::iterator it = newAcsRenderedBundleViewList.begin(); it != newAcsRenderedBundleViewList.end(); ++it) {
                    WriteAcsBundle(bsm, custodyIdAllocator, it->m_primaryBlockView.header, it->m_frontBuffer);
                }
            }
            acsSendNowExpiry = nowPtime + ACS_SEND_PERIOD;
        }

        uint64_t custodyIdExpiredAndNeedingResent;
        while (custodyTimers.PollOneAndPopAnyExpiredCustodyTimer(custodyIdExpiredAndNeedingResent, nowPtime)) {
            if (bsm.ReturnCustodyIdToAwaitingSend(custodyIdExpiredAndNeedingResent)) {
                ++numCustodyTransferTimeouts;
            }
            else {
                std::cerr << "error unable to return expired custody id " << custodyIdExpiredAndNeedingResent << " to the awaiting send\n";
            }
        }
        
        
        //Send and maintain a maximum of 5 unacked bundles (per flow id) to Egress.
        //When a bundle is acked from egress using the head segment Id, the bundle is deleted from disk and a new bundle can be sent.
        static const uint64_t maxBundleSizeToRead = UINT64_MAX;// 65535 * 10;
        if (availableDestLinksSet.empty()) {
            timeoutPoll = DEFAULT_BIG_TIMEOUT_POLL;
        }
        else {
            availableDestLinksNotCloggedVec.resize(0); 
            availableDestLinksCloggedVec.resize(0);
            for (std::set<eid_plus_isanyserviceid_pair_t>::const_iterator it = availableDestLinksSet.cbegin(); it != availableDestLinksSet.cend(); ++it) {
                //std::cout << "flow " << flowId << " sz " << flowIdToOpenSessionsMap[flowId].size() << std::endl;
                //const bool isAnyServiceId = it->second;
                if (finalDestEidToOpenCustIdsMap[it->first].size() < 5) {//finaldesteid_opencustids_map_t finalDestEidToOpenCustIdsMap;
                    availableDestLinksNotCloggedVec.push_back(*it);
                }
                else {
                    availableDestLinksCloggedVec.push_back(*it);
                }
            }
            if (availableDestLinksNotCloggedVec.size() > 0) {
                if (ReleaseOne_NoBlock(sessionRead, availableDestLinksNotCloggedVec, m_zmqPushSock_connectingStorageToBoundEgressPtr.get(), bsm, maxBundleSizeToRead)) { //true => (successfully sent to egress)
                    if (finalDestEidToOpenCustIdsMap[sessionRead.catalogEntryPtr->destEid].insert(sessionRead.custodyId).second) {
                        if (sessionRead.catalogEntryPtr->HasCustody()) {
                            custodyTimers.StartCustodyTransferTimer(sessionRead.catalogEntryPtr->destEid, sessionRead.custodyId);
                        }
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
    std::cout << "numCustodyTransferTimeouts: " << numCustodyTransferTimeouts << std::endl;
    hdtn::Logger::getInstance()->logInfo("storage", "totalEventsAllLinksClogged: " + 
        std::to_string(totalEventsAllLinksClogged));
    hdtn::Logger::getInstance()->logInfo("storage", "totalEventsNoDataInStorageForAvailableLinks: " + 
        std::to_string(totalEventsNoDataInStorageForAvailableLinks));
    hdtn::Logger::getInstance()->logInfo("storage", "totalEventsDataInStorageForCloggedLinks: " + 
        std::to_string(totalEventsDataInStorageForCloggedLinks));
}

std::size_t ZmqStorageInterface::GetCurrentNumberOfBundlesDeletedFromStorage() {
    return m_totalBundlesErasedFromStorageNoCustodyTransfer + m_totalBundlesErasedFromStorageWithCustodyTransfer;
}
