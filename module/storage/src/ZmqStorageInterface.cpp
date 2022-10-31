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
static constexpr hdtn::Logger::SubProcess subprocess = hdtn::Logger::SubProcess::storage;

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
            LOG_ERROR(subprocess) << "error in ZmqStorageInterface::Init: cannot connect inproc socket: " << ex.what();
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
            LOG_ERROR(subprocess) << "error: cannot connect socket: " << ex.what();
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
        LOG_INFO(subprocess) << "release sock connected to " << connect_boundSchedulerPubSubPath;
    }
    catch (const zmq::error_t & ex) {
        LOG_ERROR(subprocess) << "error: cannot connect release socket: " << ex.what();
        return false;
    }

    //uis request
    m_zmqRepSock_connectingUisToFromBoundStoragePtr = boost::make_unique<zmq::socket_t>(*m_zmqContextPtr, zmq::socket_type::rep);
    const std::string bind_connectingUisToFromBoundStoragePath("tcp://*:10304");
    try {
        m_zmqRepSock_connectingUisToFromBoundStoragePtr->bind(bind_connectingUisToFromBoundStoragePath);// config.releaseWorker);
        LOG_INFO(subprocess) << "uis sock bound to " << bind_connectingUisToFromBoundStoragePath;
    }
    catch (const zmq::error_t& ex) {
        LOG_ERROR(subprocess) << "error: cannot bind uis socket: " << ex.what();
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
        LOG_ERROR(subprocess) << "error: cannot set timeout on receive sockets: " << ex.what();
        return false;
    }
    
   
    
    if (!m_running) {
        m_running = true;
        LOG_INFO(subprocess) << "[ZmqStorageInterface] Launching worker thread ...";
        m_threadStartupComplete = false;
        m_threadPtr = boost::make_unique<boost::thread>(
            boost::bind(&ZmqStorageInterface::ThreadFunc, this)); //create and start the worker thread
        for (unsigned int attempt = 0; attempt < 10; ++attempt) {
            if (m_threadStartupComplete) {
                break;
            }
            LOG_DEBUG(subprocess) << "waiting for worker thread to start up...";
            boost::this_thread::sleep(boost::posix_time::seconds(1));
        }
        if (!m_threadStartupComplete) {
            LOG_ERROR(subprocess) << "error: storage thread took too long to start up.. exiting";
            return false;
        }
        else {
            LOG_INFO(subprocess) << "worker thread started";
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
    if (totalSegmentsRequired == 0) {
        LOG_ERROR(subprocess) << "out of space for acs custody signal";
        return false;
    }

    const uint64_t totalBytesPushed = bsm.PushAllSegments(sessionWrite, primary,
        newCustodyIdForAcsCustodySignal, acsBundleSerialized.data(), acsBundleSerialized.size());
    if (totalBytesPushed != acsBundleSerialized.size()) {
        LOG_ERROR(subprocess) << "totalBytesPushed != acsBundleSerialized.size";
        return false;
    }
    return true;
}

static bool Write(zmq::message_t *message, BundleStorageManagerBase & bsm,
    CustodyIdAllocator & custodyIdAllocator, CustodyTransferManager & ctm,
    CustodyTimers & custodyTimers,
    BundleViewV6 & custodySignalRfc5050RenderedBundleView,
    cbhe_eid_t & finalDestEidReturned, ZmqStorageInterface * forStats, bool dontWriteIfCustodyFlagSet)
{
    
    
    const uint8_t firstByte = ((const uint8_t*)message->data())[0];
    const bool isBpVersion6 = (firstByte == 6);
    const bool isBpVersion7 = (firstByte == ((4U << 5) | 31U));  //CBOR major type 4, additional information 31 (Indefinite-Length Array)
    if (isBpVersion6) {
        BundleViewV6 bv;
        if (!bv.LoadBundle((uint8_t *)message->data(), message->size())) { //invalid bundle
            LOG_ERROR(subprocess) << "malformed bundle";
            return false;
        }
        const Bpv6CbhePrimaryBlock & primary = bv.m_primaryBlockView.header;
        finalDestEidReturned = primary.m_destinationEid;

        static const BPV6_BUNDLEFLAG requiredPrimaryFlagsForCustody = BPV6_BUNDLEFLAG::SINGLETON | BPV6_BUNDLEFLAG::CUSTODY_REQUESTED;
        const bool bpv6CustodyIsRequested = ((primary.m_bundleProcessingControlFlags & requiredPrimaryFlagsForCustody) == requiredPrimaryFlagsForCustody);
        if (bpv6CustodyIsRequested && dontWriteIfCustodyFlagSet) { //don't rewrite a bundle because it will already be stored and eventually deleted on a custody signal
            //this is for bundles that failed to send
            return true;
        }

        //admin records pertaining to this hdtn node do not get written to disk.. they signal a deletion from disk
        static const BPV6_BUNDLEFLAG requiredPrimaryFlagsForAdminRecord = BPV6_BUNDLEFLAG::SINGLETON | BPV6_BUNDLEFLAG::ADMINRECORD;
        if (((primary.m_bundleProcessingControlFlags & requiredPrimaryFlagsForAdminRecord) == requiredPrimaryFlagsForAdminRecord) && (finalDestEidReturned == forStats->M_HDTN_EID_CUSTODY)) {
            std::vector<BundleViewV6::Bpv6CanonicalBlockView*> blocks;
            bv.GetCanonicalBlocksByType(BPV6_BLOCK_TYPE_CODE::PAYLOAD, blocks);
            if (blocks.size() != 1) {
                LOG_ERROR(subprocess) << "error admin record does not have a payload block";
                return false;
            }
            Bpv6AdministrativeRecord* adminRecordBlockPtr = dynamic_cast<Bpv6AdministrativeRecord*>(blocks[0]->headerPtr.get());
            if (adminRecordBlockPtr == NULL) {
                LOG_ERROR(subprocess) << "error null Bpv6AdministrativeRecord";
                return false;
            }
            const BPV6_ADMINISTRATIVE_RECORD_TYPE_CODE adminRecordType = adminRecordBlockPtr->m_adminRecordTypeCode;
            
            if (adminRecordType == BPV6_ADMINISTRATIVE_RECORD_TYPE_CODE::AGGREGATE_CUSTODY_SIGNAL) {
                ++forStats->m_numAcsPacketsReceived;
                //check acs
                Bpv6AdministrativeRecordContentAggregateCustodySignal * acsPtr = dynamic_cast<Bpv6AdministrativeRecordContentAggregateCustodySignal*>(adminRecordBlockPtr->m_adminRecordContentPtr.get());
                if (acsPtr == NULL) {
                    LOG_ERROR(subprocess) << "error null AggregateCustodySignal";
                    return false;
                }
                Bpv6AdministrativeRecordContentAggregateCustodySignal & acs = *(reinterpret_cast<Bpv6AdministrativeRecordContentAggregateCustodySignal*>(acsPtr));
                if (!acs.DidCustodyTransferSucceed()) {
                    LOG_ERROR(subprocess) << "custody transfer failed with reason code " << static_cast<unsigned int>(acs.GetReasonCode());
                    return false;
                }

                //todo figure out what to do with failed custody from next hop
                for (std::set<FragmentSet::data_fragment_t>::const_iterator it = acs.m_custodyIdFills.cbegin(); it != acs.m_custodyIdFills.cend(); ++it) {
                    forStats->m_numAcsCustodyTransfers += (it->endIndex + 1) - it->beginIndex;
                    custodyIdAllocator.FreeCustodyIdRange(it->beginIndex, it->endIndex);
                    for (uint64_t currentCustodyId = it->beginIndex; currentCustodyId <= it->endIndex; ++currentCustodyId) {
                        catalog_entry_t * catalogEntryPtr = bsm.GetCatalogEntryPtrFromCustodyId(currentCustodyId);
                        if (catalogEntryPtr == NULL) {
                            LOG_ERROR(subprocess) << "error finding catalog entry for bundle identified by acs custody signal";
                            continue;
                        }
                        if (!custodyTimers.CancelCustodyTransferTimer(catalogEntryPtr->destEid, currentCustodyId)) {
                            LOG_WARNING(subprocess) << "can't find custody timer associated with bundle identified by acs custody signal";
                        }
                        if (!bsm.RemoveReadBundleFromDisk(catalogEntryPtr, currentCustodyId)) {
                            LOG_ERROR(subprocess) << "error freeing bundle identified by acs custody signal from disk";
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
                    LOG_ERROR(subprocess) << "error null CustodySignal";
                    return false;
                }
                Bpv6AdministrativeRecordContentCustodySignal & cs = *(reinterpret_cast<Bpv6AdministrativeRecordContentCustodySignal*>(csPtr));
                if (!cs.DidCustodyTransferSucceed()) {
                    LOG_ERROR(subprocess) << "custody transfer failed with reason code " << cs.GetReasonCode();
                    return false;
                }
                uint64_t * custodyIdPtr;
                if (cs.m_isFragment) {
                    cbhe_bundle_uuid_t uuid;
                    if (!Uri::ParseIpnUriString(cs.m_bundleSourceEid, uuid.srcEid.nodeId, uuid.srcEid.serviceId)) {
                        LOG_ERROR(subprocess) << "error custody signal with bad ipn string";
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
                        LOG_ERROR(subprocess) << "error custody signal with bad ipn string";
                        return false;
                    }
                    uuid.creationSeconds = cs.m_copyOfBundleCreationTimestamp.secondsSinceStartOfYear2000;
                    uuid.sequence = cs.m_copyOfBundleCreationTimestamp.sequenceNumber;
                    custodyIdPtr = bsm.GetCustodyIdFromUuid(uuid);
                }
                if (custodyIdPtr == NULL) {
                    LOG_ERROR(subprocess) << "error custody signal does not match a bundle in the storage database";
                    return false;
                }
                const uint64_t custodyIdFromRfc5050 = *custodyIdPtr;
                custodyIdAllocator.FreeCustodyId(custodyIdFromRfc5050);
                catalog_entry_t * catalogEntryPtr = bsm.GetCatalogEntryPtrFromCustodyId(custodyIdFromRfc5050);
                if (catalogEntryPtr == NULL) {
                    LOG_ERROR(subprocess) << "error finding catalog entry for bundle identified by rfc5050 custody signal";
                    return false;
                }
                if (!custodyTimers.CancelCustodyTransferTimer(catalogEntryPtr->destEid, custodyIdFromRfc5050)) {
                    LOG_WARNING(subprocess) << "notice: can't find custody timer associated with bundle identified by rfc5050 custody signal";
                }
                if (!bsm.RemoveReadBundleFromDisk(catalogEntryPtr, custodyIdFromRfc5050)) {
                    LOG_ERROR(subprocess) << "error freeing bundle identified by rfc5050 custody signal from disk";
                    return false;
                }
                ++forStats->m_totalBundlesErasedFromStorageWithCustodyTransfer;
                ++forStats->m_numRfc5050CustodyTransfers;
            }
            else {
                LOG_ERROR(subprocess) << "error unknown admin record type";
                return false;
            }
            return true; //do not proceed past this point so that the signal is not written to disk
        }

        //write non admin records to disk (unless newly generated below)
        const uint64_t newCustodyId = custodyIdAllocator.GetNextCustodyIdForNextHopCtebToSend(primary.m_sourceNodeId);
        if (bpv6CustodyIsRequested) {
            if (!ctm.ProcessCustodyOfBundle(bv, true, newCustodyId, BPV6_ACS_STATUS_REASON_INDICES::SUCCESS__NO_ADDITIONAL_INFORMATION,
                custodySignalRfc5050RenderedBundleView)) {
                LOG_ERROR(subprocess) << "error unable to process custody";
            }
            else if (!bv.Render(message->size() + 200)) { //hdtn modifies bundle for next hop
                LOG_ERROR(subprocess) << "error unable to render new bundle";
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
                    if (totalSegmentsRequired == 0) {
                        LOG_ERROR(subprocess) << "out of space for custody signal";
                        return false;
                    }

                    const uint64_t totalBytesPushed = bsm.PushAllSegments(sessionWrite, custodySignalRfc5050RenderedBundleView.m_primaryBlockView.header,
                        newCustodyIdFor5050CustodySignal, (const uint8_t*)custodySignalRfc5050RenderedBundleView.m_renderedBundle.data(),
                        custodySignalRfc5050RenderedBundleView.m_renderedBundle.size());
                    if (totalBytesPushed != custodySignalRfc5050RenderedBundleView.m_renderedBundle.size()) {
                        LOG_ERROR(subprocess) << "totalBytesPushed != custodySignalRfc5050RenderedBundleView.m_renderedBundle.size()";
                        return false;
                    }
                }
            }
        }

        //write bundle (modified by hdtn if custody requested) to disk
        BundleStorageManagerSession_WriteToDisk sessionWrite;
        uint64_t totalSegmentsRequired = bsm.Push(sessionWrite, primary, bv.m_renderedBundle.size());
        if (totalSegmentsRequired == 0) {
            LOG_ERROR(subprocess) << "out of space";
            return false;
        }
        //totalSegmentsStoredOnDisk += totalSegmentsRequired;
        //totalBytesWrittenThisTest += size;

        const uint64_t totalBytesPushed = bsm.PushAllSegments(sessionWrite, primary, newCustodyId, (const uint8_t*)bv.m_renderedBundle.data(), bv.m_renderedBundle.size());
        if (totalBytesPushed != bv.m_renderedBundle.size()) {
            LOG_ERROR(subprocess) << "totalBytesPushed != size";
            return false;
        }
        return true;

    }
    else if (isBpVersion7) {
        BundleViewV7 bv;
        if (!bv.LoadBundle((uint8_t *)message->data(), message->size(), true, true)) { //invalid bundle
            LOG_ERROR(subprocess) << "malformed bundle";
            return false;
        }
        const Bpv7CbhePrimaryBlock & primary = bv.m_primaryBlockView.header;
        finalDestEidReturned = primary.m_destinationEid;

        //write bundle
        BundleStorageManagerSession_WriteToDisk sessionWrite;
        uint64_t totalSegmentsRequired = bsm.Push(sessionWrite, primary, bv.m_renderedBundle.size());
        if (totalSegmentsRequired == 0) {
            LOG_ERROR(subprocess) << "out of space";
            return false;
        }
        //totalSegmentsStoredOnDisk += totalSegmentsRequired;
        //totalBytesWrittenThisTest += size;
        const uint64_t newCustodyId = custodyIdAllocator.GetNextCustodyIdForNextHopCtebToSend(primary.m_sourceNodeId);
        const uint64_t totalBytesPushed = bsm.PushAllSegments(sessionWrite, primary, newCustodyId, (const uint8_t*)bv.m_renderedBundle.data(), bv.m_renderedBundle.size());
        if (totalBytesPushed != bv.m_renderedBundle.size()) {
            LOG_ERROR(subprocess) << "totalBytesPushed != size";
            return false;
        }
        return true;
    }
    else {
        LOG_ERROR(subprocess) << "error in ZmqStorageInterface Write: unsupported bundle version detected";
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
    delete static_cast<std::vector<uint8_t>*>(hint);
}

static bool ReleaseOne_NoBlock(BundleStorageManagerSession_ReadFromDisk & sessionRead,
    const std::vector<eid_plus_isanyserviceid_pair_t> & availableDestLinks,
    zmq::socket_t *egressSock, BundleStorageManagerBase & bsm, const uint64_t maxBundleSizeToRead)
{
    const uint64_t bytesToReadFromDisk = bsm.PopTop(sessionRead, availableDestLinks);
    if (bytesToReadFromDisk == 0) { //no more of these links to read
        return false;
    }

    //this link has a bundle in the fifo
        

    //IF YOU DECIDE YOU DON'T WANT TO READ THE BUNDLE AFTER PEEKING AT IT (MAYBE IT'S TOO BIG RIGHT NOW)
    if (bytesToReadFromDisk > maxBundleSizeToRead) {
        LOG_ERROR(subprocess) << "bundle to read from disk is too large right now";
        bsm.ReturnTop(sessionRead);
        return false;
        //bytesToReadFromDisk = bsm.PopTop(sessionRead, availableDestLinks); //get it back
    }
        
    std::vector<uint8_t> * vecUint8BundleDataRawPointer = new std::vector<uint8_t>();
    const bool successReadAllSegments = bsm.ReadAllSegments(sessionRead, *vecUint8BundleDataRawPointer);
    zmq::message_t zmqBundleDataMessageWithDataStolen(vecUint8BundleDataRawPointer->data(), vecUint8BundleDataRawPointer->size(), CustomCleanupStdVecUint8, vecUint8BundleDataRawPointer);
        
    if (!successReadAllSegments) {
        LOG_ERROR(subprocess) << "unable to read all segments from disk";
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
        LOG_ERROR(subprocess) << "zmq could not send";
        bsm.ReturnTop(sessionRead);
        return false;
    }
    if (!egressSock->send(std::move(zmqBundleDataMessageWithDataStolen), zmq::send_flags::dontwait)) {
        LOG_ERROR(subprocess) << "zmq could not send bundle";
        bsm.ReturnTop(sessionRead);
        return false;
    }
    /*
    //if you're happy with the bundle data you read back, then officially remove it from the disk
    if (deleteFromDiskNow) {
        bool successRemoveBundle = bsm.RemoveReadBundleFromDisk(sessionRead);
        if (!successRemoveBundle) {
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
    LOG_INFO(subprocess) << "Currently Releasing Final Destination Eids: " << strVals;
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
    LOG_INFO(subprocess) << "Worker thread starting up.";

   

    
    
    std::unique_ptr<BundleStorageManagerBase> bsmPtr;
    if (m_hdtnConfig.m_storageConfig.m_storageImplementation == "stdio_multi_threaded") {
        LOG_INFO(subprocess) << "[ZmqStorageInterface] Initializing BundleStorageManagerMT ... ";
        bsmPtr = boost::make_unique<BundleStorageManagerMT>(std::make_shared<StorageConfig>(m_hdtnConfig.m_storageConfig));
    }
    else if (m_hdtnConfig.m_storageConfig.m_storageImplementation == "asio_single_threaded") {
        LOG_INFO(subprocess) << "[ZmqStorageInterface] Initializing BundleStorageManagerAsio ... ";
        bsmPtr = boost::make_unique<BundleStorageManagerAsio>(std::make_shared<StorageConfig>(m_hdtnConfig.m_storageConfig));
    }
    else {
        LOG_ERROR(subprocess) << "error in hdtn::ZmqStorageInterface::ThreadFunc: invalid storage implementation " << m_hdtnConfig.m_storageConfig.m_storageImplementation;
        return;
    }
    BundleStorageManagerBase & bsm = *bsmPtr;
    bsm.Start();
    

    typedef std::set<uint64_t> custodyid_set_t;
    typedef std::map<uint64_t, custodyid_set_t> finaldestnodeid_opencustids_map_t;

    std::vector<eid_plus_isanyserviceid_pair_t> availableDestLinksNotCloggedVec;
    availableDestLinksNotCloggedVec.reserve(100); //todo
    std::vector<eid_plus_isanyserviceid_pair_t> availableDestLinksCloggedVec;
    availableDestLinksCloggedVec.reserve(100); //todo

    m_totalBundlesErasedFromStorageNoCustodyTransfer = 0;
    m_totalBundlesRewrittenToStorageFromFailedEgressSend = 0;
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
    finaldestnodeid_opencustids_map_t finalDestNodeIdToOpenCustIdsMap;

    static constexpr std::size_t minBufSizeBytesReleaseMessages = sizeof(uint64_t) + 
        ((sizeof(hdtn::IreleaseStartHdr) > sizeof(hdtn::IreleaseStopHdr)) ? sizeof(hdtn::IreleaseStartHdr) : sizeof(hdtn::IreleaseStopHdr));
    uint64_t rxBufReleaseMessagesAlign64[minBufSizeBytesReleaseMessages / sizeof(uint64_t)];

    zmq::pollitem_t pollItems[5] = {
        {m_zmqPullSock_boundEgressToConnectingStoragePtr->handle(), 0, ZMQ_POLLIN, 0},
        {m_zmqPullSock_boundIngressToConnectingStoragePtr->handle(), 0, ZMQ_POLLIN, 0},
        {m_zmqSubSock_boundReleaseToConnectingStoragePtr->handle(), 0, ZMQ_POLLIN, 0},
        {m_zmqRepSock_connectingGuiToFromBoundStoragePtr->handle(), 0, ZMQ_POLLIN, 0},
        {m_zmqRepSock_connectingUisToFromBoundStoragePtr->handle(), 0, ZMQ_POLLIN, 0}
    };
    static const long DEFAULT_BIG_TIMEOUT_POLL = 250;
    long timeoutPoll = DEFAULT_BIG_TIMEOUT_POLL; //0 => no blocking
    boost::posix_time::ptime acsSendNowExpiry = boost::posix_time::microsec_clock::universal_time() + ACS_SEND_PERIOD;
    m_threadStartupComplete = true;
    while (m_running) {
        int rc = 0;
        try {
            rc = zmq::poll(pollItems, 5, timeoutPoll);
        }
        catch (zmq::error_t & e) {
            LOG_ERROR(subprocess) << "caught zmq::error_t in hdtn::ZmqStorageInterface::ThreadFunc: " << e.what();
            continue;
        }
        if (rc > 0) {            
            if (pollItems[0].revents & ZMQ_POLLIN) { //from egress sock
                hdtn::EgressAckHdr egressAckHdr;
                const zmq::recv_buffer_result_t res = m_zmqPullSock_boundEgressToConnectingStoragePtr->recv(zmq::mutable_buffer(&egressAckHdr, sizeof(egressAckHdr)), zmq::recv_flags::none);
                if (!res) {
                    LOG_ERROR(subprocess) << "EgressAckHdr not received";
                    continue;
                }
                else if ((res->truncated()) || (res->size != sizeof(hdtn::EgressAckHdr))) {
                    LOG_ERROR(subprocess) << "EgressAckHdr wrong size received";
                    continue;
                }
                else if (egressAckHdr.base.type == HDTN_MSGTYPE_EGRESS_ACK_TO_STORAGE) {
                    custodyid_set_t& custodyIdSet = finalDestNodeIdToOpenCustIdsMap[egressAckHdr.finalDestEid.nodeId];
                    custodyid_set_t::iterator it = custodyIdSet.find(egressAckHdr.custodyId);
                    if (it != custodyIdSet.end()) {
                        if (egressAckHdr.error) {
                            //A bundle that was sent from storage to egress gets an ack back from egress with the error flag set because egress could not send the bundle.
                            //This will allow storage to trigger a link down event more quickly than waiting for scheduler.
                            //Since storage already has the bundle, the error flag will prevent deletion and move the bundle back to the "awaiting send" state,
                            //but the bundle won't be immediately released again from storage because of the immediate link down event.
                            if (availableDestLinksSet.erase(eid_plus_isanyserviceid_pair_t(cbhe_eid_t(egressAckHdr.finalDestEid.nodeId, 0), true))) { //false => fully qualified service id, true => wildcard (*) service id, 0 is don't care
                                LOG_WARNING(subprocess) << "Storage got a link down notification from egress for final dest "
                                    << Uri::GetIpnUriStringAnyServiceNumber(egressAckHdr.finalDestEid.nodeId) << " because storage to egress failed";
                                PrintReleasedLinks(availableDestLinksSet);
                            }
                            if (!bsm.ReturnCustodyIdToAwaitingSend(egressAckHdr.custodyId)) {
                                LOG_ERROR(subprocess) << "error returning custody id " << egressAckHdr.custodyId << " to awaiting send";
                            }
                            custodyTimers.CancelCustodyTransferTimer(egressAckHdr.finalDestEid, egressAckHdr.custodyId);
                        }
                        else if (egressAckHdr.deleteNow) { //custody not requested, so don't wait on a custody signal to delete the bundle
                            bool successRemoveBundle = bsm.RemoveReadBundleFromDisk(egressAckHdr.custodyId);
                            if (!successRemoveBundle) {
                                LOG_ERROR(subprocess) << "error freeing bundle from disk";
                            }
                            else {
                                ++m_totalBundlesErasedFromStorageNoCustodyTransfer;
                            }
                        }
                        custodyIdSet.erase(it);
                    }
                    else {
                        LOG_ERROR(subprocess) << "Storage got a HDTN_MSGTYPE_EGRESS_ACK_TO_STORAGE but could not find custody id";
                    }
                }
                else if (egressAckHdr.base.type == HDTN_MSGTYPE_EGRESS_FAILED_BUNDLE_TO_STORAGE) { //bundles sent from ingress to egress but egress could not send
                    zmq::message_t zmqBundleDataReceived;
                    if (!m_zmqPullSock_boundEgressToConnectingStoragePtr->recv(zmqBundleDataReceived, zmq::recv_flags::none)) {
                        LOG_ERROR(subprocess) << "error in hdtn::ZmqStorageInterface::ThreadFunc (from ingress bundle data) message not received";
                        continue;
                    }
                    
                    cbhe_eid_t finalDestEidReturnedFromWrite;
                    Write(&zmqBundleDataReceived, bsm, custodyIdAllocator, ctm, custodyTimers, custodySignalRfc5050RenderedBundleView, finalDestEidReturnedFromWrite, this, true);
                    ++m_totalBundlesRewrittenToStorageFromFailedEgressSend;
                    finalDestEidReturnedFromWrite.serviceId = 0;
                    if (availableDestLinksSet.erase(eid_plus_isanyserviceid_pair_t(finalDestEidReturnedFromWrite, true))) { //false => fully qualified service id, true => wildcard (*) service id, 0 is don't care
                        LOG_WARNING(subprocess) << "Storage got a link down notification from egress for final dest "
                            << Uri::GetIpnUriStringAnyServiceNumber(finalDestEidReturnedFromWrite.nodeId) << " because cut through from ingress failed";
                        PrintReleasedLinks(availableDestLinksSet);
                    }
                    LOG_WARNING(subprocess) << "Notice in ZmqStorageInterface::ThreadFunc: A bundle was send to storage from egress because cut through from ingress failed";
                    
                }
                else {
                    LOG_ERROR(subprocess) << "EgressAckHdr unknown type, got " << egressAckHdr.base.type;
                    continue;
                }
            }
            if (pollItems[1].revents & ZMQ_POLLIN) { //from ingress bundle data
                hdtn::ToStorageHdr toStorageHeader;
                const zmq::recv_buffer_result_t res = m_zmqPullSock_boundIngressToConnectingStoragePtr->recv(zmq::mutable_buffer(&toStorageHeader, sizeof(hdtn::ToStorageHdr)), zmq::recv_flags::none);
                if (!res) {
                    LOG_ERROR(subprocess) << "error in hdtn::ZmqStorageInterface::ThreadFunc (from ingress bundle data) message hdr not received";
                    continue;
                }
                else if ((res->truncated()) || (res->size != sizeof(hdtn::ToStorageHdr))) {
                    LOG_ERROR(subprocess) << "error in hdtn::ZmqStorageInterface::ThreadFunc (from ingress bundle data) rhdr.size() != sizeof(hdtn::ToStorageHdr)";
                    continue;
                }
                else if (toStorageHeader.base.type == HDTN_MSGTYPE_STORAGE_ADD_OPPORTUNISTIC_LINK) {
                    const uint64_t nodeId = toStorageHeader.ingressUniqueId;
                    LOG_INFO(subprocess) << "finalDestEid ("
                        << Uri::GetIpnUriStringAnyServiceNumber(nodeId)
                        << ") will be released from storage";
                    availableDestLinksSet.emplace(cbhe_eid_t(nodeId, 0), true); //true => any service id.. 0 is don't care
                    PrintReleasedLinks(availableDestLinksSet);
                    continue;
                }
                else if (toStorageHeader.base.type == HDTN_MSGTYPE_STORAGE_REMOVE_OPPORTUNISTIC_LINK) {
                    const uint64_t nodeId = toStorageHeader.ingressUniqueId;
                    LOG_INFO(subprocess) << "finalDestEid ("
                        << Uri::GetIpnUriStringAnyServiceNumber(nodeId)
                        << ") will STOP being released from storage";
                    availableDestLinksSet.erase(eid_plus_isanyserviceid_pair_t(cbhe_eid_t(nodeId, 0), true)); //true => any service id.. 0 is don't care
                    PrintReleasedLinks(availableDestLinksSet);
                    continue;
                }
                else if (toStorageHeader.base.type != HDTN_MSGTYPE_STORE) {
                    LOG_ERROR(subprocess) << "error in hdtn::ZmqStorageInterface::ThreadFunc (from ingress bundle data) message type not HDTN_MSGTYPE_STORE";
                    continue;
                }

                storageStats.inBytes += sizeof(hdtn::ToStorageHdr);
                ++storageStats.inMsg;

                zmq::message_t zmqBundleDataReceived;
                if (!m_zmqPullSock_boundIngressToConnectingStoragePtr->recv(zmqBundleDataReceived, zmq::recv_flags::none)) {
                    LOG_ERROR(subprocess) << "error in hdtn::ZmqStorageInterface::ThreadFunc (from ingress bundle data) message not received";
                    continue;
                }
                storageStats.inBytes += zmqBundleDataReceived.size();
                
                cbhe_eid_t finalDestEidReturnedFromWrite;
                Write(&zmqBundleDataReceived, bsm, custodyIdAllocator, ctm, custodyTimers, custodySignalRfc5050RenderedBundleView, finalDestEidReturnedFromWrite, this, false);

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
                    LOG_ERROR(subprocess) << "error: zmq could not send ingress an ack from storage";
                }
            }
            if (pollItems[2].revents & ZMQ_POLLIN) { //release messages
                //force this hdtn message struct to be aligned on a 64-byte boundary using zmq::mutable_buffer
                const zmq::recv_buffer_result_t res = m_zmqSubSock_boundReleaseToConnectingStoragePtr->recv(
                    zmq::mutable_buffer(rxBufReleaseMessagesAlign64, minBufSizeBytesReleaseMessages), zmq::recv_flags::none);
                if (!res) {
                    LOG_ERROR(subprocess) << "[schedule release] message not received";
                    continue;
                }
                else if (res->size < sizeof(hdtn::CommonHdr)) {
                    LOG_ERROR(subprocess) << "[schedule release] res->size < sizeof(hdtn::CommonHdr)";
                    continue;
                }

                LOG_INFO(subprocess) << "release message received";
                hdtn::CommonHdr *commonHdr = (hdtn::CommonHdr *)rxBufReleaseMessagesAlign64;
                if (commonHdr->type == HDTN_MSGTYPE_ILINKUP) {
                    if (res->size != sizeof(hdtn::IreleaseStartHdr)) {
                        LOG_ERROR(subprocess) << "[schedule release] res->size != sizeof(hdtn::IreleaseStartHdr)";
                        continue;
                    }

                    hdtn::IreleaseStartHdr * iReleaseStartHdr = (hdtn::IreleaseStartHdr *)rxBufReleaseMessagesAlign64;
                    LOG_INFO(subprocess) << "finalDestEid (" 
                        + Uri::GetIpnUriStringAnyServiceNumber(iReleaseStartHdr->finalDestinationNodeId) 
                        + ") will be released from storage";
                    availableDestLinksSet.emplace(cbhe_eid_t(iReleaseStartHdr->finalDestinationNodeId, 0), true); //false => fully qualified service id, true => wildcard (*) service id, 0 is don't care
                    availableDestLinksSet.emplace(cbhe_eid_t(iReleaseStartHdr->nextHopNodeId, 0), true); //false => fully qualified service id, true => wildcard (*) service id, 0 is don't care
                }
                else if (commonHdr->type == HDTN_MSGTYPE_ILINKDOWN) {
                    if (res->size != sizeof(hdtn::IreleaseStopHdr)) {
                        LOG_ERROR(subprocess) << "[schedule release] res->size != sizeof(hdtn::IreleaseStopHdr)";
                        continue;
                    }

                    hdtn::IreleaseStopHdr * iReleaseStopHdr = (hdtn::IreleaseStopHdr *)rxBufReleaseMessagesAlign64;
                    LOG_INFO(subprocess) << "finalDestEid (" 
                        + Uri::GetIpnUriStringAnyServiceNumber(iReleaseStopHdr->finalDestinationNodeId)
                        + ") will STOP BEING released from storage";
                    availableDestLinksSet.erase(eid_plus_isanyserviceid_pair_t(cbhe_eid_t(iReleaseStopHdr->finalDestinationNodeId, 0), true)); //false => fully qualified service id, true => wildcard (*) service id, 0 is don't care
                    availableDestLinksSet.erase(eid_plus_isanyserviceid_pair_t(cbhe_eid_t(iReleaseStopHdr->nextHopNodeId, 0), true)); //false => fully qualified service id, true => wildcard (*) service id, 0 is don't care

                }
                PrintReleasedLinks(availableDestLinksSet);
            }
            if (pollItems[3].revents & ZMQ_POLLIN) { //gui requests data
                uint8_t guiMsgByte;
                const zmq::recv_buffer_result_t res = m_zmqRepSock_connectingGuiToFromBoundStoragePtr->recv(zmq::mutable_buffer(&guiMsgByte, sizeof(guiMsgByte)), zmq::recv_flags::dontwait);
                if (!res) {
                    LOG_ERROR(subprocess) << "error in ZmqStorageInterface::ThreadFunc: cannot read guiMsgByte";
                }
                else if ((res->truncated()) || (res->size != sizeof(guiMsgByte))) {
                    LOG_ERROR(subprocess) << "guiMsgByte message mismatch: untruncated = " << res->untruncated_size
                        << " truncated = " << res->size << " expected = " << sizeof(guiMsgByte);
                }
                else if (guiMsgByte == 1) {                    
                    //send telemetry
                    StorageTelemetry_t telem;
                    telem.totalBundlesErasedFromStorage = GetCurrentNumberOfBundlesDeletedFromStorage();
                    telem.totalBundlesSentToEgressFromStorage = m_totalBundlesSentToEgressFromStorage;

                    std::vector<uint8_t>* vecUint8RawPointer = new std::vector<uint8_t>(sizeof(StorageTelemetry_t)); //will be 64-bit aligned
                    uint8_t* telemPtr = vecUint8RawPointer->data();
                    const uint8_t* const telemSerializationBase = telemPtr;
                    uint64_t telemBufferSize = vecUint8RawPointer->size();

                    //start zmq message with telemetry
                    const uint64_t storageTelemSize = telem.SerializeToLittleEndian(telemPtr, telemBufferSize);
                    telemBufferSize -= storageTelemSize;
                    telemPtr += storageTelemSize;

                    vecUint8RawPointer->resize(telemPtr - telemSerializationBase);

                    zmq::message_t zmqTelemMessageWithDataStolen(vecUint8RawPointer->data(), vecUint8RawPointer->size(), CustomCleanupStdVecUint8, vecUint8RawPointer);

                    if (!m_zmqRepSock_connectingGuiToFromBoundStoragePtr->send(std::move(zmqTelemMessageWithDataStolen), zmq::send_flags::dontwait)) {
                        LOG_ERROR(subprocess) << "storage can't send telemetry to gui";
                    }
                }
                /*
                else if (guiMsgByte == 2) {
                    //send telemetry
                    //std::cout << "storage send telem";
                    StorageExpiringBeforeThresholdTelemetry_t telem;
                    telem.priority = 2;
                    telem.thresholdSecondsSinceStartOfYear2000 = TimestampUtil::GetSecondsSinceEpochRfc5050(boost::posix_time::microsec_clock::universal_time() + boost::posix_time::seconds(10000));
                    if (!bsm.GetStorageExpiringBeforeThresholdTelemetry(telem)) {
                    }
                    else {
                        //send telemetry
                        std::vector<uint8_t>* vecUint8RawPointer = new std::vector<uint8_t>(1000); //will be 64-bit aligned
                        uint8_t* telemPtr = vecUint8RawPointer->data();
                        const uint8_t* const telemSerializationBase = telemPtr;
                        uint64_t telemBufferSize = vecUint8RawPointer->size();

                        //start zmq message with telemetry
                        const uint64_t storageTelemSize = telem.SerializeToLittleEndian(telemPtr, telemBufferSize);
                        telemBufferSize -= storageTelemSize;
                        telemPtr += storageTelemSize;

                        vecUint8RawPointer->resize(telemPtr - telemSerializationBase);

                        zmq::message_t zmqTelemMessageWithDataStolen(vecUint8RawPointer->data(), vecUint8RawPointer->size(), CustomCleanupStdVecUint8, vecUint8RawPointer);
                        if (!m_zmqRepSock_connectingGuiToFromBoundStoragePtr->send(std::move(zmqTelemMessageWithDataStolen), zmq::send_flags::dontwait)) {
                        }
                    }
                }
                */
                else {
                    LOG_ERROR(subprocess) << "error guiMsgByte not 1 or 2";
                }
            }
            if (pollItems[4].revents & ZMQ_POLLIN) { //uis requests data
                StorageTelemetryRequest_t telemReq;
                const zmq::recv_buffer_result_t res = m_zmqRepSock_connectingUisToFromBoundStoragePtr->recv(zmq::mutable_buffer(&telemReq, sizeof(telemReq)), zmq::recv_flags::dontwait);
                if (!res) {
                    LOG_ERROR(subprocess) << "error in ZmqStorageInterface::ThreadFunc: cannot read telemReq";
                }
                else if ((res->truncated()) || (res->size != sizeof(telemReq))) {
                    LOG_ERROR(subprocess) << "guiMsgByte message mismatch: untruncated = " << res->untruncated_size
                        << " truncated = " << res->size << " expected = " << sizeof(telemReq);
                }
                else if (telemReq.type == 10) {
                    //send telemetry
                    StorageExpiringBeforeThresholdTelemetry_t telem;
                    telem.priority = telemReq.priority;
                    telem.thresholdSecondsSinceStartOfYear2000 = TimestampUtil::GetSecondsSinceEpochRfc5050(boost::posix_time::microsec_clock::universal_time() + boost::posix_time::seconds(telemReq.thresholdSecondsFromNow));
                    if (!bsm.GetStorageExpiringBeforeThresholdTelemetry(telem)) {
                        LOG_ERROR(subprocess) << "storage can't get StorageExpiringBeforeThresholdTelemetry";
                    }
                    else {
                        //send telemetry
                        std::vector<uint8_t>* vecUint8RawPointer = new std::vector<uint8_t>(1000); //will be 64-bit aligned
                        uint8_t* telemPtr = vecUint8RawPointer->data();
                        const uint8_t* const telemSerializationBase = telemPtr;
                        uint64_t telemBufferSize = vecUint8RawPointer->size();

                        //start zmq message with telemetry
                        const uint64_t storageTelemSize = telem.SerializeToLittleEndian(telemPtr, telemBufferSize);
                        telemBufferSize -= storageTelemSize;
                        telemPtr += storageTelemSize;

                        vecUint8RawPointer->resize(telemPtr - telemSerializationBase);

                        zmq::message_t zmqTelemMessageWithDataStolen(vecUint8RawPointer->data(), vecUint8RawPointer->size(), CustomCleanupStdVecUint8, vecUint8RawPointer);
                        LOG_INFO(subprocess) << "send storage telem to uis with size " << zmqTelemMessageWithDataStolen.size();
                        if (!m_zmqRepSock_connectingUisToFromBoundStoragePtr->send(std::move(zmqTelemMessageWithDataStolen), zmq::send_flags::dontwait)) {
                            LOG_ERROR(subprocess) << "storage can't send telemetry to uis";
                        }
                    }
                }
                else {
                    LOG_ERROR(subprocess) << "error telemReq.type not 10";
                }
            }
        }

        const boost::posix_time::ptime nowPtime = boost::posix_time::microsec_clock::universal_time();
        if ((acsSendNowExpiry <= nowPtime) || (ctm.GetLargestNumberOfFills() > ACS_MAX_FILLS_PER_ACS_PACKET)) {
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
                LOG_ERROR(subprocess) << "error unable to return expired custody id " << custodyIdExpiredAndNeedingResent << " to the awaiting send";
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
                //const bool isAnyServiceId = it->second;
                if (finalDestNodeIdToOpenCustIdsMap[it->first.nodeId].size() < 5) {
                    availableDestLinksNotCloggedVec.push_back(*it);
                }
                else {
                    availableDestLinksCloggedVec.push_back(*it);
                }
            }
            if (availableDestLinksNotCloggedVec.size() > 0) {
                if (ReleaseOne_NoBlock(sessionRead, availableDestLinksNotCloggedVec, m_zmqPushSock_connectingStorageToBoundEgressPtr.get(), bsm, maxBundleSizeToRead)) { //true => (successfully sent to egress)
                    if (finalDestNodeIdToOpenCustIdsMap[sessionRead.catalogEntryPtr->destEid.nodeId].insert(sessionRead.custodyId).second) {
                        if (sessionRead.catalogEntryPtr->HasCustody()) {
                            custodyTimers.StartCustodyTransferTimer(sessionRead.catalogEntryPtr->destEid, sessionRead.custodyId);
                        }
                        timeoutPoll = 0; //no timeout as we need to keep feeding to egress
                        ++m_totalBundlesSentToEgressFromStorage;
                    }
                    else {
                        LOG_ERROR(subprocess) << "could not insert custody id into finalDestNodeIdToOpenCustIdsMap";
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
    LOG_DEBUG(subprocess) << "totalEventsAllLinksClogged: " << totalEventsAllLinksClogged;
    LOG_DEBUG(subprocess) << "totalEventsNoDataInStorageForAvailableLinks: " << totalEventsNoDataInStorageForAvailableLinks;
    LOG_DEBUG(subprocess) << "totalEventsDataInStorageForCloggedLinks: " << totalEventsDataInStorageForCloggedLinks;
    LOG_DEBUG(subprocess) << "m_numRfc5050CustodyTransfers: " << m_numRfc5050CustodyTransfers;
    LOG_DEBUG(subprocess) << "m_numAcsCustodyTransfers: " << m_numAcsCustodyTransfers;
    LOG_DEBUG(subprocess) << "m_numAcsPacketsReceived: " << m_numAcsPacketsReceived;
    LOG_DEBUG(subprocess) << "m_totalBundlesErasedFromStorageNoCustodyTransfer: " << m_totalBundlesErasedFromStorageNoCustodyTransfer;
    LOG_DEBUG(subprocess) << "m_totalBundlesErasedFromStorageWithCustodyTransfer: " << m_totalBundlesErasedFromStorageWithCustodyTransfer;
    LOG_DEBUG(subprocess) << "numCustodyTransferTimeouts: " << numCustodyTransferTimeouts;
    LOG_DEBUG(subprocess) << "m_totalBundlesRewrittenToStorageFromFailedEgressSend: " << m_totalBundlesRewrittenToStorageFromFailedEgressSend;
}

std::size_t ZmqStorageInterface::GetCurrentNumberOfBundlesDeletedFromStorage() {
    return m_totalBundlesErasedFromStorageNoCustodyTransfer + m_totalBundlesErasedFromStorageWithCustodyTransfer;
}
