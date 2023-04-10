/**
 * @file ZmqStorageInterface.cpp
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

#include "ZmqStorageInterface.h"
#include "message.hpp"
#include "BundleStorageManagerMT.h"
#include "BundleStorageManagerAsio.h"
#include "Logger.h"
#include <map>
#include <string>
#include <vector>
#include <memory>
#include <boost/thread.hpp>
#include "HdtnConfig.h"
#include "codec/bpv6.h"
#include "TelemetryDefinitions.h"
#include <boost/core/noncopyable.hpp>
#include <set>
#include <unordered_map>
#include <boost/lexical_cast.hpp>
#include <boost/make_unique.hpp>
#include "codec/CustodyIdAllocator.h"
#include "codec/CustodyTransferManager.h"
#include "Uri.h"
#include "CustodyTimers.h"
#include "codec/BundleViewV7.h"
#include "ThreadNamer.h"

typedef std::pair<cbhe_eid_t, bool> eid_plus_isanyserviceid_pair_t;
static constexpr hdtn::Logger::SubProcess subprocess = hdtn::Logger::SubProcess::storage;

struct ZmqStorageInterface::Impl : private boost::noncopyable {
    struct CutThroughQueueData : private boost::noncopyable {
        CutThroughQueueData() = delete;
        CutThroughQueueData(zmq::message_t&& paramBundleToEgress, zmq::message_t&& paramAckToIngress, const cbhe_eid_t& paramFinalDestEid, const uint64_t paramIngressUniqueId) :
            bundleToEgress(std::move(paramBundleToEgress)), ackToIngress(std::move(paramAckToIngress)), finalDestEid(paramFinalDestEid), ingressUniqueId(paramIngressUniqueId) {}
        //CutThroughQueueData(CutThroughQueueData && o) {}
        zmq::message_t bundleToEgress;
        zmq::message_t ackToIngress;
        cbhe_eid_t finalDestEid;
        uint64_t ingressUniqueId;
    };
    struct CutThroughMapAckData : private boost::noncopyable {
        CutThroughMapAckData() = delete;
        CutThroughMapAckData(zmq::message_t&& paramAckToIngress, const uint64_t paramBundleSizeBytes) :
            ackToIngress(std::move(paramAckToIngress)), bundleSizeBytes(paramBundleSizeBytes) {}
        zmq::message_t ackToIngress;
        uint64_t bundleSizeBytes;
    };
    //typedef std::queue<CutThroughQueueData> cut_through_queue_t;
    typedef std::queue<CutThroughQueueData> cut_through_queue_t;
    typedef std::unordered_map<uint64_t, uint64_t> custodyid_to_size_map_t;
    typedef std::unordered_map<uint64_t, CutThroughMapAckData> map_id_to_ackdata_t;
    struct OutductInfo_t : private boost::noncopyable {
        OutductInfo_t() :
            halfOfMaxBundlesInPipeline_StorageToEgressPath(0),
            halfOfMaxBundleSizeBytesInPipeline_StorageToEgressPath(0),
            outductIndex(0),
            nextHopNodeId(0),
            linkIsUp(false),
            stateTryCutThrough(false),
            bytesInPipeline(0)
        {}

        uint64_t halfOfMaxBundlesInPipeline_StorageToEgressPath;
        uint64_t halfOfMaxBundleSizeBytesInPipeline_StorageToEgressPath;
        uint64_t outductIndex;
        uint64_t nextHopNodeId;
        bool linkIsUp;
        bool stateTryCutThrough;
        std::vector<eid_plus_isanyserviceid_pair_t> eidVec;
        custodyid_to_size_map_t mapOpenCustodyIdToBundleSizeBytes;
        map_id_to_ackdata_t mapIngressUniqueIdToIngressAckData;
        cut_through_queue_t cutThroughQueue;
        uint64_t bytesInPipeline;
        friend std::ostream& operator<<(std::ostream& os, const OutductInfo_t& o);

        bool IsOpportunisticLink() const noexcept {
            return (outductIndex == UINT64_MAX);
        }
        
    };
    typedef std::unique_ptr<OutductInfo_t> OutductInfoPtr_t;

    Impl();
    ~Impl();
    void Stop();
    bool Init(const HdtnConfig& hdtnConfig, const HdtnDistributedConfig& hdtnDistributedConfig, zmq::context_t* hdtnOneProcessZmqInprocContextPtr);
    std::size_t GetCurrentNumberOfBundlesDeletedFromStorage();

private:
    bool WriteAcsBundle(const Bpv6CbhePrimaryBlock& primary, const std::vector<uint8_t>& acsBundleSerialized);
    bool Write(zmq::message_t* message,
        cbhe_eid_t& finalDestEidReturned, bool dontWriteIfCustodyFlagSet,
        bool isCertainThatThisBundleHasNoCustodyOrIsNotAdminRecord);
    bool WriteBundle(const PrimaryBlock& bundlePrimaryBlock,
        const uint64_t newCustodyId, const uint8_t* allData, const std::size_t allDataSize);
    uint64_t PeekOne(const std::vector<eid_plus_isanyserviceid_pair_t>& availableDestLinks);
    bool ReleaseOne_NoBlock(const OutductInfo_t& info, const uint64_t maxBundleSizeToRead, uint64_t& returnedBundleSize);
    void RepopulateUpLinksVec();
    void SetLinkDown(OutductInfo_t & info);
    void ThreadFunc();
    void SyncTelemetry();
    void TelemEventsHandler();
    static std::string SprintCustodyidToSizeMap(const custodyid_to_size_map_t& m);
    void DeleteExpiredBundles();
    void DeleteBundleById(const uint64_t custodyId);

public:
    StorageTelemetry_t m_telem;
    cbhe_eid_t M_HDTN_EID_CUSTODY;

private:
    std::unique_ptr<zmq::context_t> m_zmqContextPtr;
    std::unique_ptr<zmq::socket_t> m_zmqPullSock_boundIngressToConnectingStoragePtr;
    std::unique_ptr<zmq::socket_t> m_zmqSubSock_boundReleaseToConnectingStoragePtr;
    std::unique_ptr<zmq::socket_t> m_zmqPushSock_connectingStorageToBoundEgressPtr;
    std::unique_ptr<zmq::socket_t> m_zmqPullSock_boundEgressToConnectingStoragePtr;
    std::unique_ptr<zmq::socket_t> m_zmqPushSock_connectingStorageToBoundIngressPtr;

    std::unique_ptr<zmq::socket_t> m_zmqRepSock_connectingTelemToFromBoundStoragePtr;

    HdtnConfig m_hdtnConfig;

    zmq::context_t* m_hdtnOneProcessZmqInprocContextPtr;
    std::unique_ptr<boost::thread> m_threadPtr;
    volatile bool m_running;

    //variables initialized and used only by ThreadFunc()
    std::unique_ptr<BundleStorageManagerBase> m_bsmPtr;
    std::unique_ptr<CustodyIdAllocator> m_custodyIdAllocatorPtr;
    std::unique_ptr<CustodyTransferManager> m_ctmPtr;
    std::unique_ptr<CustodyTimers> m_custodyTimersPtr;
    BundleViewV6 m_custodySignalRfc5050RenderedBundleView;
    BundleStorageManagerSession_ReadFromDisk m_sessionRead; //reuse this due to expensive heap allocation
    std::vector<OutductInfoPtr_t> m_vectorOutductInfo; //outductIndex to info
    std::map<uint64_t, OutductInfoPtr_t> m_mapOpportunisticNextHopNodeIdToOutductInfo;
    std::vector<OutductInfo_t*> m_vectorUpLinksOutductInfoPtrs; //outductIndex to info

    //for blocking until worker-thread startup
    volatile bool m_workerThreadStartupInProgress;
    boost::mutex m_workerThreadStartupMutex;
    boost::condition_variable m_workerThreadStartupConditionVariable;

    // For deleting bundles
    std::vector<uint64_t> m_expiredIds;
    enum class DeletionPolicy { never, onExpiration, onStorageFull };
    DeletionPolicy m_deletionPolicy;

    const float DELETE_ALL_EXPIRED_THRESHOLD = 0.9f; /* percent. If storage this full, delete all expired bundles */
    const uint64_t MAX_DELETE_EXPIRED_PER_ITER = 100; /* Maximum number of bundles to delete per iteration (no storage pressure) */
};

ZmqStorageInterface::Impl::Impl() :
    m_running(false),
    m_workerThreadStartupInProgress(false),
    m_hdtnOneProcessZmqInprocContextPtr(nullptr),
    m_deletionPolicy(DeletionPolicy::never) {}

ZmqStorageInterface::Impl::~Impl() {
    Stop();
}

ZmqStorageInterface::ZmqStorageInterface() : 
    m_pimpl(boost::make_unique<ZmqStorageInterface::Impl>()),
    //references
    m_telemRef(m_pimpl->m_telem) {}

ZmqStorageInterface::~ZmqStorageInterface() {
    Stop();
}

void ZmqStorageInterface::Stop() {
    m_pimpl->Stop();
}
void ZmqStorageInterface::Impl::Stop() {
    m_running = false; //thread stopping criteria
    if (m_threadPtr) {
        try {
            m_threadPtr->join();
            m_threadPtr.reset();
        }
        catch (boost::thread_resource_error &e) {
            LOG_ERROR(subprocess) << "error stopping Storage thread: " << e.what();
        }
    }
}

bool ZmqStorageInterface::Init(const HdtnConfig& hdtnConfig, const HdtnDistributedConfig& hdtnDistributedConfig, zmq::context_t* hdtnOneProcessZmqInprocContextPtr) {
    return m_pimpl->Init(hdtnConfig, hdtnDistributedConfig, hdtnOneProcessZmqInprocContextPtr);
}
bool ZmqStorageInterface::Impl::Init(const HdtnConfig & hdtnConfig, const HdtnDistributedConfig& hdtnDistributedConfig, zmq::context_t * hdtnOneProcessZmqInprocContextPtr) {

    if (m_running) {
        LOG_ERROR(subprocess) << "ZmqStorageInterface::Init called while ZmqStorageInterface is already running";
        return false;
    }

    m_hdtnConfig = hdtnConfig;
    //according to ION.pdf v4.0.1 on page 100 it says:
    //  Remember that the format for this argument is ipn:element_number.0 and that
    //  the final 0 is required, as custodial/administration service is always service 0.
    //HDTN shall default m_myCustodialServiceId to 0 although it is changeable in the hdtn config json file
    M_HDTN_EID_CUSTODY.Set(m_hdtnConfig.m_myNodeId, m_hdtnConfig.m_myCustodialServiceId);
    m_hdtnOneProcessZmqInprocContextPtr = hdtnOneProcessZmqInprocContextPtr;

    if(m_hdtnConfig.m_storageConfig.m_storageDeletionPolicy == "never") {
        m_deletionPolicy = DeletionPolicy::never;
    } else if(m_hdtnConfig.m_storageConfig.m_storageDeletionPolicy == "on_expiration") {
        m_deletionPolicy = DeletionPolicy::onExpiration;
    } else if(m_hdtnConfig.m_storageConfig.m_storageDeletionPolicy == "on_storage_full") {
        m_deletionPolicy = DeletionPolicy::onStorageFull;
    }

    //{

    m_zmqContextPtr = boost::make_unique<zmq::context_t>();
    

    if (hdtnOneProcessZmqInprocContextPtr) {
        m_zmqPushSock_connectingStorageToBoundEgressPtr = boost::make_unique<zmq::socket_t>(*m_hdtnOneProcessZmqInprocContextPtr, zmq::socket_type::pair);
        m_zmqPullSock_boundEgressToConnectingStoragePtr = boost::make_unique<zmq::socket_t>(*m_hdtnOneProcessZmqInprocContextPtr, zmq::socket_type::pair);
        m_zmqPushSock_connectingStorageToBoundIngressPtr = boost::make_unique<zmq::socket_t>(*m_hdtnOneProcessZmqInprocContextPtr, zmq::socket_type::pair);
        m_zmqPullSock_boundIngressToConnectingStoragePtr = boost::make_unique<zmq::socket_t>(*m_hdtnOneProcessZmqInprocContextPtr, zmq::socket_type::pair);
        m_zmqRepSock_connectingTelemToFromBoundStoragePtr = boost::make_unique<zmq::socket_t>(*m_hdtnOneProcessZmqInprocContextPtr, zmq::socket_type::pair);
        try {
            m_zmqPushSock_connectingStorageToBoundEgressPtr->connect(std::string("inproc://connecting_storage_to_bound_egress")); // egress should bind
            m_zmqPullSock_boundEgressToConnectingStoragePtr->connect(std::string("inproc://bound_egress_to_connecting_storage")); // egress should bind
            m_zmqPushSock_connectingStorageToBoundIngressPtr->connect(std::string("inproc://connecting_storage_to_bound_ingress"));
            m_zmqPullSock_boundIngressToConnectingStoragePtr->connect(std::string("inproc://bound_ingress_to_connecting_storage"));
            m_zmqRepSock_connectingTelemToFromBoundStoragePtr->bind(std::string("inproc://connecting_telem_to_from_bound_storage"));
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
            hdtnDistributedConfig.m_zmqEgressAddress +
            std::string(":") +
            boost::lexical_cast<std::string>(hdtnDistributedConfig.m_zmqConnectingStorageToBoundEgressPortPath));
        
        m_zmqPullSock_boundEgressToConnectingStoragePtr = boost::make_unique<zmq::socket_t>(*m_zmqContextPtr, zmq::socket_type::pull);
        const std::string connect_boundEgressToConnectingStoragePath(
            std::string("tcp://") +
            hdtnDistributedConfig.m_zmqEgressAddress +
            std::string(":") +
            boost::lexical_cast<std::string>(hdtnDistributedConfig.m_zmqBoundEgressToConnectingStoragePortPath));
        
        m_zmqPushSock_connectingStorageToBoundIngressPtr = boost::make_unique<zmq::socket_t>(*m_zmqContextPtr, zmq::socket_type::push);
        const std::string connect_connectingStorageToBoundIngressPath(
            std::string("tcp://") +
            hdtnDistributedConfig.m_zmqIngressAddress +
            std::string(":") +
            boost::lexical_cast<std::string>(hdtnDistributedConfig.m_zmqConnectingStorageToBoundIngressPortPath));

        m_zmqPullSock_boundIngressToConnectingStoragePtr = boost::make_unique<zmq::socket_t>(*m_zmqContextPtr, zmq::socket_type::pull);
        const std::string connect_boundIngressToConnectingStoragePath(
            std::string("tcp://") +
            hdtnDistributedConfig.m_zmqIngressAddress +
            std::string(":") +
            boost::lexical_cast<std::string>(hdtnDistributedConfig.m_zmqBoundIngressToConnectingStoragePortPath));

        //from telemetry socket
        m_zmqRepSock_connectingTelemToFromBoundStoragePtr = boost::make_unique<zmq::socket_t>(*m_zmqContextPtr, zmq::socket_type::rep);
        const std::string bind_connectingTelemToFromBoundStoragePath(
            std::string("tcp://*:") + boost::lexical_cast<std::string>(hdtnDistributedConfig.m_zmqConnectingTelemToFromBoundStoragePortPath));

        try {
            m_zmqPushSock_connectingStorageToBoundEgressPtr->connect(connect_connectingStorageToBoundEgressPath); // egress should bind
            m_zmqPullSock_boundEgressToConnectingStoragePtr->connect(connect_boundEgressToConnectingStoragePath); // egress should bind
            m_zmqPushSock_connectingStorageToBoundIngressPtr->connect(connect_connectingStorageToBoundIngressPath);
            m_zmqPullSock_boundIngressToConnectingStoragePtr->connect(connect_boundIngressToConnectingStoragePath);
            m_zmqRepSock_connectingTelemToFromBoundStoragePtr->bind(bind_connectingTelemToFromBoundStoragePath);
        }
        catch (const zmq::error_t & ex) {
            LOG_ERROR(subprocess) << "error: cannot connect socket: " << ex.what();
            return false;
        }
    }

    m_zmqSubSock_boundReleaseToConnectingStoragePtr = boost::make_unique<zmq::socket_t>(*m_zmqContextPtr, zmq::socket_type::sub);
    const std::string connect_boundSchedulerPubSubPath(
        std::string("tcp://") +
        ((hdtnOneProcessZmqInprocContextPtr == NULL) ? hdtnDistributedConfig.m_zmqSchedulerAddress : std::string("localhost")) +
        std::string(":") +
        boost::lexical_cast<std::string>(m_hdtnConfig.m_zmqBoundSchedulerPubSubPortPath));
    try {
        m_zmqSubSock_boundReleaseToConnectingStoragePtr->connect(connect_boundSchedulerPubSubPath);
        m_zmqSubSock_boundReleaseToConnectingStoragePtr->set(zmq::sockopt::linger, 0); //prevent hang when deleting the zmqCtxPtr
        LOG_INFO(subprocess) << "Connected to scheduler at " << connect_boundSchedulerPubSubPath << " , subscribing...";
    }
    catch (const zmq::error_t& ex) {
        LOG_ERROR(subprocess) << "Cannot connect to scheduler socket at " << connect_boundSchedulerPubSubPath << " : " << ex.what();
        return false;
    }
    try {
        //Sends one-byte 0x1 message to scheduler XPub socket plus strlen of subscription
        //All release messages shall be prefixed by "aaaaaaaa" before the common header
        //Router unique subscription shall be "a" (gets all messages that start with "a") (e.g "aaa", "ab", etc.)
        //Ingress unique subscription shall be "aa"
        //Storage unique subscription shall be "aaa"
        m_zmqSubSock_boundReleaseToConnectingStoragePtr->set(zmq::sockopt::subscribe, "aaa"); 
            LOG_INFO(subprocess) << "Subscribed to all events from scheduler";
    }
    catch (const zmq::error_t& ex) {
        LOG_ERROR(subprocess) << "Cannot subscribe to all events from scheduler: " << ex.what();
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
    
    { //start worker thread
        m_running = true;
        LOG_INFO(subprocess) << "[ZmqStorageInterface] Launching worker thread ...";
        boost::mutex::scoped_lock workerThreadStartupLock(m_workerThreadStartupMutex);
        m_workerThreadStartupInProgress = true;
        
        m_threadPtr = boost::make_unique<boost::thread>(
            boost::bind(&ZmqStorageInterface::Impl::ThreadFunc, this)); //create and start the worker thread

        while (m_workerThreadStartupInProgress) { //lock mutex (above) before checking condition
            //Returns: false if the call is returning because the time specified by abs_time was reached, true otherwise.
            if (!m_workerThreadStartupConditionVariable.timed_wait(workerThreadStartupLock, boost::posix_time::seconds(3))) {
                LOG_ERROR(subprocess) << "timed out waiting (for 3 seconds) for storage worker thread to start up";
                break;
            }
        }
        if (m_workerThreadStartupInProgress) {
            LOG_ERROR(subprocess) << "error: storage thread took too long to start up.. exiting";
            return false;
        }
        else {
            LOG_INFO(subprocess) << "worker thread started";
        }
    }
    return true;
}

bool ZmqStorageInterface::Impl::WriteAcsBundle(const Bpv6CbhePrimaryBlock & primary, const std::vector<uint8_t> & acsBundleSerialized)
{
    const cbhe_eid_t & hdtnSrcEid = primary.m_sourceNodeId;
    const uint64_t newCustodyIdForAcsCustodySignal = m_custodyIdAllocatorPtr->GetNextCustodyIdForNextHopCtebToSend(hdtnSrcEid);

    //write custody signal to disk
    BundleStorageManagerSession_WriteToDisk sessionWrite;
    const uint64_t totalSegmentsRequired = m_bsmPtr->Push(sessionWrite, primary, acsBundleSerialized.size());
    if (totalSegmentsRequired == 0) {
        LOG_ERROR(subprocess) << "out of space for acs custody signal";
        return false;
    }

    const uint64_t totalBytesPushed = m_bsmPtr->PushAllSegments(sessionWrite, primary,
        newCustodyIdForAcsCustodySignal, acsBundleSerialized.data(), acsBundleSerialized.size());
    if (totalBytesPushed != acsBundleSerialized.size()) {
        LOG_ERROR(subprocess) << "totalBytesPushed != acsBundleSerialized.size";
        return false;
    }
    return true;
}

bool ZmqStorageInterface::Impl::Write(zmq::message_t *message,
    cbhe_eid_t & finalDestEidReturned, bool dontWriteIfCustodyFlagSet,
    bool isCertainThatThisBundleHasNoCustodyOrIsNotAdminRecord)
{
    
    
    const uint8_t firstByte = ((const uint8_t*)message->data())[0];
    const bool isBpVersion6 = (firstByte == 6);
    const bool isBpVersion7 = (firstByte == ((4U << 5) | 31U));  //CBOR major type 4, additional information 31 (Indefinite-Length Array)
    if (isBpVersion6) {
        BundleViewV6 bv;
        const bool loadPrimaryBlockOnly = isCertainThatThisBundleHasNoCustodyOrIsNotAdminRecord;
        if (!bv.LoadBundle((uint8_t *)message->data(), message->size(), loadPrimaryBlockOnly)) { //invalid bundle
            LOG_ERROR(subprocess) << "malformed bundle";
            return false;
        }
        const Bpv6CbhePrimaryBlock & primary = bv.m_primaryBlockView.header;
        finalDestEidReturned = primary.m_destinationEid;
        const uint64_t newCustodyId = m_custodyIdAllocatorPtr->GetNextCustodyIdForNextHopCtebToSend(primary.m_sourceNodeId);

        if (!loadPrimaryBlockOnly) {
            static const BPV6_BUNDLEFLAG requiredPrimaryFlagsForCustody = BPV6_BUNDLEFLAG::SINGLETON | BPV6_BUNDLEFLAG::CUSTODY_REQUESTED;
            const bool bpv6CustodyIsRequested = ((primary.m_bundleProcessingControlFlags & requiredPrimaryFlagsForCustody) == requiredPrimaryFlagsForCustody);
            if (bpv6CustodyIsRequested && dontWriteIfCustodyFlagSet) { //don't rewrite a bundle because it will already be stored and eventually deleted on a custody signal
                //this is for bundles that failed to send
                return true;
            }

            //admin records pertaining to this hdtn node do not get written to disk.. they signal a deletion from disk
            static const BPV6_BUNDLEFLAG requiredPrimaryFlagsForAdminRecord = BPV6_BUNDLEFLAG::SINGLETON | BPV6_BUNDLEFLAG::ADMINRECORD;
            if (((primary.m_bundleProcessingControlFlags & requiredPrimaryFlagsForAdminRecord) == requiredPrimaryFlagsForAdminRecord) && (finalDestEidReturned == M_HDTN_EID_CUSTODY)) {
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
                    ++m_telem.m_numAcsPacketsReceived;
                    //check acs
                    Bpv6AdministrativeRecordContentAggregateCustodySignal* acsPtr = dynamic_cast<Bpv6AdministrativeRecordContentAggregateCustodySignal*>(adminRecordBlockPtr->m_adminRecordContentPtr.get());
                    if (acsPtr == NULL) {
                        LOG_ERROR(subprocess) << "error null AggregateCustodySignal";
                        return false;
                    }
                    Bpv6AdministrativeRecordContentAggregateCustodySignal& acs = *(reinterpret_cast<Bpv6AdministrativeRecordContentAggregateCustodySignal*>(acsPtr));
                    if (!acs.DidCustodyTransferSucceed()) {
                        //a failure with a reason code of redundant reception means that
                        // the receiver already has that bundle in custody so it can be
                        // released even though it is a "failure".
                        if (acs.GetReasonCode() != BPV6_CUSTODY_SIGNAL_REASON_CODES_7BIT::REDUNDANT_RECEPTION) {
                            LOG_ERROR(subprocess) << "acs custody transfer failed with reason code " << acs.GetReasonCode();
                            return false;
                        }
                    }

                    //todo figure out what to do with failed custody from next hop
                    for (FragmentSet::data_fragment_set_t::const_iterator it = acs.m_custodyIdFills.cbegin(); it != acs.m_custodyIdFills.cend(); ++it) {
                        m_telem.m_numAcsCustodyTransfers += (it->endIndex + 1) - it->beginIndex;
                        m_custodyIdAllocatorPtr->FreeCustodyIdRange(it->beginIndex, it->endIndex);
                        for (uint64_t currentCustodyId = it->beginIndex; currentCustodyId <= it->endIndex; ++currentCustodyId) {
                            catalog_entry_t* catalogEntryPtr = m_bsmPtr->GetCatalogEntryPtrFromCustodyId(currentCustodyId);
                            if (catalogEntryPtr == NULL) {
                                LOG_ERROR(subprocess) << "error finding catalog entry for bundle identified by acs custody signal";
                                continue;
                            }
                            if (!m_custodyTimersPtr->CancelCustodyTransferTimer(catalogEntryPtr->destEid, currentCustodyId)) {
                                LOG_WARNING(subprocess) << "can't find custody timer associated with bundle identified by acs custody signal";
                            }
                            if (!m_bsmPtr->RemoveReadBundleFromDisk(catalogEntryPtr, currentCustodyId)) {
                                LOG_ERROR(subprocess) << "error freeing bundle identified by acs custody signal from disk";
                                continue;
                            }
                            ++m_telem.m_totalBundlesErasedFromStorageWithCustodyTransfer;
                        }
                    }
                }
                else if (adminRecordType == BPV6_ADMINISTRATIVE_RECORD_TYPE_CODE::CUSTODY_SIGNAL) { //rfc5050 style custody transfer
                    //check acs
                    Bpv6AdministrativeRecordContentCustodySignal* csPtr = dynamic_cast<Bpv6AdministrativeRecordContentCustodySignal*>(adminRecordBlockPtr->m_adminRecordContentPtr.get());
                    if (csPtr == NULL) {
                        LOG_ERROR(subprocess) << "error null CustodySignal";
                        return false;
                    }
                    Bpv6AdministrativeRecordContentCustodySignal& cs = *(reinterpret_cast<Bpv6AdministrativeRecordContentCustodySignal*>(csPtr));
                    if (!cs.DidCustodyTransferSucceed()) {
                        //a failure with a reason code of redundant reception means that
                        // the receiver already has that bundle in custody so it can be
                        // released even though it is a "failure".
                        if (cs.GetReasonCode() != BPV6_CUSTODY_SIGNAL_REASON_CODES_7BIT::REDUNDANT_RECEPTION) {
                            LOG_ERROR(subprocess) << "custody transfer failed with reason code " << cs.GetReasonCode();
                            return false;
                        }
                    }
                    uint64_t* custodyIdPtr;
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
                        custodyIdPtr = m_bsmPtr->GetCustodyIdFromUuid(uuid);
                    }
                    else {
                        cbhe_bundle_uuid_nofragment_t uuid;
                        if (!Uri::ParseIpnUriString(cs.m_bundleSourceEid, uuid.srcEid.nodeId, uuid.srcEid.serviceId)) {
                            LOG_ERROR(subprocess) << "error custody signal with bad ipn string";
                            return false;
                        }
                        uuid.creationSeconds = cs.m_copyOfBundleCreationTimestamp.secondsSinceStartOfYear2000;
                        uuid.sequence = cs.m_copyOfBundleCreationTimestamp.sequenceNumber;
                        custodyIdPtr = m_bsmPtr->GetCustodyIdFromUuid(uuid);
                    }
                    if (custodyIdPtr == NULL) {
                        LOG_ERROR(subprocess) << "error custody signal does not match a bundle in the storage database";
                        return false;
                    }
                    const uint64_t custodyIdFromRfc5050 = *custodyIdPtr;
                    m_custodyIdAllocatorPtr->FreeCustodyId(custodyIdFromRfc5050);
                    catalog_entry_t* catalogEntryPtr = m_bsmPtr->GetCatalogEntryPtrFromCustodyId(custodyIdFromRfc5050);
                    if (catalogEntryPtr == NULL) {
                        LOG_ERROR(subprocess) << "error finding catalog entry for bundle identified by rfc5050 custody signal";
                        return false;
                    }
                    if (!m_custodyTimersPtr->CancelCustodyTransferTimer(catalogEntryPtr->destEid, custodyIdFromRfc5050)) {
                        LOG_WARNING(subprocess) << "notice: can't find custody timer associated with bundle identified by rfc5050 custody signal";
                    }
                    if (!m_bsmPtr->RemoveReadBundleFromDisk(catalogEntryPtr, custodyIdFromRfc5050)) {
                        LOG_ERROR(subprocess) << "error freeing bundle identified by rfc5050 custody signal from disk";
                        return false;
                    }
                    ++m_telem.m_totalBundlesErasedFromStorageWithCustodyTransfer;
                    ++m_telem.m_numRfc5050CustodyTransfers;
                }
                else {
                    LOG_ERROR(subprocess) << "error unknown admin record type";
                    return false;
                }
                return true; //do not proceed past this point so that the signal is not written to disk
            }

            //write non admin records to disk (unless newly generated below)
            if (bpv6CustodyIsRequested) {
                if (!m_ctmPtr->ProcessCustodyOfBundle(bv, true, newCustodyId, BPV6_ACS_STATUS_REASON_INDICES::SUCCESS__NO_ADDITIONAL_INFORMATION,
                    m_custodySignalRfc5050RenderedBundleView)) {
                    LOG_ERROR(subprocess) << "error unable to process custody";
                }
                else if (!bv.Render(message->size() + 200)) { //hdtn modifies bundle for next hop
                    LOG_ERROR(subprocess) << "error unable to render new bundle";
                }
                else {
                    if (m_custodySignalRfc5050RenderedBundleView.m_renderedBundle.size()) {
                        const cbhe_eid_t& hdtnSrcEid = m_custodySignalRfc5050RenderedBundleView.m_primaryBlockView.header.m_sourceNodeId;
                        const uint64_t newCustodyIdFor5050CustodySignal = m_custodyIdAllocatorPtr->GetNextCustodyIdForNextHopCtebToSend(hdtnSrcEid);

                        //write custody signal to disk
                        BundleStorageManagerSession_WriteToDisk sessionWrite;
                        const uint64_t totalSegmentsRequired = m_bsmPtr->Push(sessionWrite,
                            m_custodySignalRfc5050RenderedBundleView.m_primaryBlockView.header,
                            m_custodySignalRfc5050RenderedBundleView.m_renderedBundle.size());
                        if (totalSegmentsRequired == 0) {
                            LOG_ERROR(subprocess) << "out of space for custody signal";
                            return false;
                        }

                        const uint64_t totalBytesPushed = m_bsmPtr->PushAllSegments(sessionWrite, m_custodySignalRfc5050RenderedBundleView.m_primaryBlockView.header,
                            newCustodyIdFor5050CustodySignal, (const uint8_t*)m_custodySignalRfc5050RenderedBundleView.m_renderedBundle.data(),
                            m_custodySignalRfc5050RenderedBundleView.m_renderedBundle.size());
                        if (totalBytesPushed != m_custodySignalRfc5050RenderedBundleView.m_renderedBundle.size()) {
                            LOG_ERROR(subprocess) << "totalBytesPushed != custodySignalRfc5050RenderedBundleView.m_renderedBundle.size()";
                            return false;
                        }
                    }
                }
            }
        }

        //write bundle (modified by hdtn if custody requested) to disk
        return WriteBundle(primary, newCustodyId, (const uint8_t*)bv.m_renderedBundle.data(), bv.m_renderedBundle.size());
    }
    else if (isBpVersion7) {
        BundleViewV7 bv;
        if (!bv.LoadBundle((uint8_t *)message->data(), message->size(), true, true)) { //invalid bundle
            LOG_ERROR(subprocess) << "malformed bundle";
            return false;
        }
        const Bpv7CbhePrimaryBlock & primary = bv.m_primaryBlockView.header;
        finalDestEidReturned = primary.m_destinationEid;

        const uint64_t newCustodyId = m_custodyIdAllocatorPtr->GetNextCustodyIdForNextHopCtebToSend(primary.m_sourceNodeId);

        //write bundle
        return WriteBundle(primary, newCustodyId, (const uint8_t*)bv.m_renderedBundle.data(), bv.m_renderedBundle.size());
    }
    else {
        LOG_ERROR(subprocess) << "error in ZmqStorageInterface Write: unsupported bundle version detected";
        return false;
    }
    
}

bool ZmqStorageInterface::Impl::WriteBundle(const PrimaryBlock& bundlePrimaryBlock,
    const uint64_t newCustodyId, const uint8_t* allData, const std::size_t allDataSize)
{
    //write bundle
    BundleStorageManagerSession_WriteToDisk sessionWrite;
    uint64_t totalSegmentsRequired = m_bsmPtr->Push(sessionWrite, bundlePrimaryBlock, allDataSize);
    if (totalSegmentsRequired == 0) {
        LOG_ERROR(subprocess) << "out of space";
        return false;
    }
    //totalSegmentsStoredOnDisk += totalSegmentsRequired;
    //totalBytesWrittenThisTest += size;
    const uint64_t totalBytesPushed = m_bsmPtr->PushAllSegments(sessionWrite, bundlePrimaryBlock, newCustodyId, allData, allDataSize);
    if (totalBytesPushed != allDataSize) {
        LOG_ERROR(subprocess) << "totalBytesPushed != size";
        return false;
    }
    return true;
}


//return number of bytes to read for specified links
uint64_t ZmqStorageInterface::Impl::PeekOne(const std::vector<eid_plus_isanyserviceid_pair_t> & availableDestLinks) {
    const uint64_t bytesToReadFromDisk = m_bsmPtr->PopTop(m_sessionRead, availableDestLinks);
    if (bytesToReadFromDisk == 0) { //no more of these links to read
        return 0; //no bytes to read
    }

    //this link has a bundle in the fifo
    m_bsmPtr->ReturnTop(m_sessionRead);
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
static void CustomCleanupStdString(void* data, void* hint) {
    delete static_cast<std::string*>(hint);
}

bool ZmqStorageInterface::Impl::ReleaseOne_NoBlock(const OutductInfo_t& info, const uint64_t maxBundleSizeToRead, uint64_t& returnedBundleSize)
{
    const uint64_t bytesToReadFromDisk = m_bsmPtr->PopTop(m_sessionRead, info.eidVec);
    if (bytesToReadFromDisk == 0) { //no more of these links to read
        return false;
    }

    //this link has a bundle in the fifo
        

    //IF YOU DECIDE YOU DON'T WANT TO READ THE BUNDLE AFTER PEEKING AT IT (MAYBE IT'S TOO BIG RIGHT NOW)
    if (bytesToReadFromDisk > maxBundleSizeToRead) {
        LOG_DEBUG(subprocess) << "bundle to read from disk (size=" << bytesToReadFromDisk << " bytes) is too large right now, exceeds " << maxBundleSizeToRead << " bytes";
        m_bsmPtr->ReturnTop(m_sessionRead);
        return false;
        //bytesToReadFromDisk = bsm.PopTop(sessionRead, availableDestLinks); //get it back
    }
        
    std::vector<uint8_t> * vecUint8BundleDataRawPointer = new std::vector<uint8_t>();
    const bool successReadAllSegments = m_bsmPtr->ReadAllSegments(m_sessionRead, *vecUint8BundleDataRawPointer);
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
    toEgressHdr->nextHopNodeId = info.nextHopNodeId;
    toEgressHdr->finalDestEid = m_sessionRead.catalogEntryPtr->destEid;
    toEgressHdr->hasCustody = m_sessionRead.catalogEntryPtr->HasCustody();
    toEgressHdr->isCutThroughFromStorage = 0;
    toEgressHdr->custodyId = m_sessionRead.custodyId;
    toEgressHdr->outductIndex = info.outductIndex;
    
    if (!m_zmqPushSock_connectingStorageToBoundEgressPtr->send(std::move(zmqMessageToEgressHdrWithDataStolen), zmq::send_flags::sndmore | zmq::send_flags::dontwait)) {
        LOG_ERROR(subprocess) << "zmq could not send";
        m_bsmPtr->ReturnTop(m_sessionRead);
        return false;
    }
    if (!m_zmqPushSock_connectingStorageToBoundEgressPtr->send(std::move(zmqBundleDataMessageWithDataStolen), zmq::send_flags::dontwait)) {
        LOG_ERROR(subprocess) << "zmq could not send bundle";
        m_bsmPtr->ReturnTop(m_sessionRead);
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
        
    returnedBundleSize = bytesToReadFromDisk;
    return true;

}


std::ostream& operator<<(std::ostream& os, const ZmqStorageInterface::Impl::OutductInfo_t& o) {
    os << "Currently " << ((o.linkIsUp) ? "" : "NOT")
        << " Releasing nextHopNodeId " << o.nextHopNodeId
        << " with Final Destination Eids : [";
    for (std::size_t i = 0; i < o.eidVec.size(); ++i) {
        const eid_plus_isanyserviceid_pair_t& p = o.eidVec[i];
        if (p.second) { //any service id
            os << Uri::GetIpnUriStringAnyServiceNumber(p.first.nodeId) << ", ";
        }
        else { //fully qualified
            os << Uri::GetIpnUriString(p.first.nodeId, p.first.serviceId) << ", ";
        }
    }
    os << "]";
    return os;
}

void ZmqStorageInterface::Impl::RepopulateUpLinksVec() {
    m_vectorUpLinksOutductInfoPtrs.clear();
    for (std::size_t i = 0; i < m_vectorOutductInfo.size(); ++i) {
        OutductInfo_t& info = *(m_vectorOutductInfo[i]);
        if (info.linkIsUp) {
            m_vectorUpLinksOutductInfoPtrs.push_back(&info);
        }
    }
    for (std::map<uint64_t, OutductInfoPtr_t>::iterator it = m_mapOpportunisticNextHopNodeIdToOutductInfo.begin();
        it != m_mapOpportunisticNextHopNodeIdToOutductInfo.end(); ++it)
    {
        OutductInfo_t& info = *(it->second);
        if (info.linkIsUp) {
            m_vectorUpLinksOutductInfoPtrs.push_back(&info);
        }
    }
}

std::string ZmqStorageInterface::Impl::SprintCustodyidToSizeMap(const custodyid_to_size_map_t& m) {
    std::string message = "custodyid_to_size_map_t totalelements=";
    message += boost::lexical_cast<std::string>(m.size()) + " [";
    for (custodyid_to_size_map_t::const_iterator it = m.cbegin(); it != m.cend(); ++it) {
        message += "cid=" + boost::lexical_cast<std::string>(it->first) + " size=" + boost::lexical_cast<std::string>(it->second) + " ; ";
    }
    message += "]";
    return message;
}

void ZmqStorageInterface::Impl::SetLinkDown(OutductInfo_t & info) {
    if (info.linkIsUp) {
        info.linkIsUp = false;
        if (!info.cutThroughQueue.empty()) {
            LOG_INFO(subprocess) << "Link down event (nextHopNodeId=" << info.nextHopNodeId
                << ") dumping " << info.cutThroughQueue.size() << " queued cut - through bundles to disk";
            while (!info.cutThroughQueue.empty()) {
                CutThroughQueueData& qd = info.cutThroughQueue.front();
                cbhe_eid_t finalDestEidReturnedFromWrite;
                Write(&qd.bundleToEgress, finalDestEidReturnedFromWrite, true, true); //last true because if cut through then definitely no custody or not admin record
                hdtn::StorageAckHdr* storageAckHdr = (hdtn::StorageAckHdr*)qd.ackToIngress.data();
                storageAckHdr->error = 1;
                if (!m_zmqPushSock_connectingStorageToBoundIngressPtr->send(std::move(qd.ackToIngress), zmq::send_flags::dontwait)) {
                    LOG_ERROR(subprocess) << "zmq could not send ingress an ack from storage";
                }
                info.cutThroughQueue.pop();
            }
        }
        RepopulateUpLinksVec();
        LOG_INFO(subprocess) << info;
    }
}

void ZmqStorageInterface::Impl::SyncTelemetry() {
    if (m_bsmPtr) {
        const BundleStorageCatalog& bsc = m_bsmPtr->GetBundleStorageCatalogConstRef();
        m_telem.m_numBundlesOnDisk = bsc.GetNumBundlesInCatalog();
        m_telem.m_numBundleBytesOnDisk = bsc.GetNumBundleBytesInCatalog();
        m_telem.m_totalBundleWriteOperationsToDisk = bsc.GetTotalBundleWriteOperationsToCatalog();
        m_telem.m_totalBundleByteWriteOperationsToDisk = bsc.GetTotalBundleByteWriteOperationsToCatalog();
        m_telem.m_totalBundleEraseOperationsFromDisk = bsc.GetTotalBundleEraseOperationsFromCatalog();
        m_telem.m_totalBundleByteEraseOperationsFromDisk = bsc.GetTotalBundleByteEraseOperationsFromCatalog();

        m_telem.m_usedSpaceBytes = m_bsmPtr->GetUsedSpaceBytes();
        m_telem.m_freeSpaceBytes = m_bsmPtr->GetFreeSpaceBytes();
    }
}

/** Delete bundle from disk
 *
 * @param custodyId The custody ID of the bundle to delete
 */
void ZmqStorageInterface::Impl::DeleteBundleById(const uint64_t custodyId) {
    catalog_entry_t *entry = m_bsmPtr->GetCatalogEntryPtrFromCustodyId(custodyId);
    if(!entry) {
        LOG_ERROR(subprocess) << "Failed to get catalog entry for " << custodyId << " while deleting for expiry";
        return;
    }

    if(entry->HasCustody()) {
        bool success = m_custodyTimersPtr->CancelCustodyTransferTimer(entry->destEid, custodyId);
        if(!success) {
            LOG_ERROR(subprocess) << "Failed to cancel custody timer for " << custodyId << " while deleting for expiry";
        }
        //TODO RFC5050 (s5.13) wants us to send a bundle deletion status report to the report-to endpoint
    }

    m_custodyIdAllocatorPtr->FreeCustodyId(custodyId);

    bool success = m_bsmPtr->RemoveBundleFromDisk(entry, custodyId);
    if(!success) {
        LOG_ERROR(subprocess) << "Failed to remove bundle from disk " << custodyId << " while deleting for expiry";
    }
}

/** Delete expired bundles
 *
 * Deletes expired bundles from disk per storage deletion policy.
 *
 * If storage is less full than DELETE_ALL_EXPIRED_THRESHOLD, then at most
 * MAX_DELETE_EXPIRED_PER_ITER bundles are deleted. Otherwise, all expired
 * bundles are deleted. (Avoid blocking event loop unless full).
 *
 */
void ZmqStorageInterface::Impl::DeleteExpiredBundles() {

    if(m_deletionPolicy == DeletionPolicy::never) {
        return;
    }

    float storageUsagePercentage = m_bsmPtr->GetUsedSpaceBytes()  / (float)m_bsmPtr->GetTotalCapacityBytes();

    if(m_deletionPolicy == DeletionPolicy::onStorageFull && storageUsagePercentage < DELETE_ALL_EXPIRED_THRESHOLD) {
        return;
    }

    // If we reach here then either storage is full or policy == "on_expiration"

    uint64_t numToDelete = storageUsagePercentage >= DELETE_ALL_EXPIRED_THRESHOLD ?
        0 : MAX_DELETE_EXPIRED_PER_ITER;

    uint64_t expiry = TimestampUtil::GetSecondsSinceEpochRfc5050(boost::posix_time::microsec_clock::universal_time());

    m_bsmPtr->GetExpiredBundleIds(expiry, numToDelete, m_expiredIds);

    for(uint64_t custodyId : m_expiredIds) {
        DeleteBundleById(custodyId);
        m_telem.m_totalBundlesErasedFromStorageBecauseExpired++;
    }
}

void ZmqStorageInterface::Impl::ThreadFunc() {
    ThreadNamer::SetThisThreadName("ZmqStorageInterface");
    
    m_custodySignalRfc5050RenderedBundleView.m_frontBuffer.reserve(2000);
    m_custodySignalRfc5050RenderedBundleView.m_backBuffer.reserve(2000);
    m_custodyIdAllocatorPtr = boost::make_unique<CustodyIdAllocator>();
    m_custodyTimersPtr = boost::make_unique<CustodyTimers>(boost::posix_time::milliseconds(m_hdtnConfig.m_retransmitBundleAfterNoCustodySignalMilliseconds));
    const bool IS_HDTN_ACS_AWARE = m_hdtnConfig.m_isAcsAware;
    const uint64_t ACS_MAX_FILLS_PER_ACS_PACKET = m_hdtnConfig.m_acsMaxFillsPerAcsPacket;
    
    static const boost::posix_time::time_duration ACS_SEND_PERIOD = boost::posix_time::milliseconds(m_hdtnConfig.m_acsSendPeriodMilliseconds);
    m_ctmPtr = boost::make_unique<CustodyTransferManager>(IS_HDTN_ACS_AWARE, M_HDTN_EID_CUSTODY.nodeId, M_HDTN_EID_CUSTODY.serviceId);
    LOG_INFO(subprocess) << "Worker thread starting up.";

   

    
    
    if (m_hdtnConfig.m_storageConfig.m_storageImplementation == "stdio_multi_threaded") {
        LOG_INFO(subprocess) << "[ZmqStorageInterface] Initializing BundleStorageManagerMT ... ";
        m_bsmPtr = boost::make_unique<BundleStorageManagerMT>(std::make_shared<StorageConfig>(m_hdtnConfig.m_storageConfig));
    }
    else if (m_hdtnConfig.m_storageConfig.m_storageImplementation == "asio_single_threaded") {
        LOG_INFO(subprocess) << "[ZmqStorageInterface] Initializing BundleStorageManagerAsio ... ";
        m_bsmPtr = boost::make_unique<BundleStorageManagerAsio>(std::make_shared<StorageConfig>(m_hdtnConfig.m_storageConfig));
    }
    else {
        LOG_ERROR(subprocess) << "error in hdtn::ZmqStorageInterface::ThreadFunc: invalid storage implementation " << m_hdtnConfig.m_storageConfig.m_storageImplementation;
        return;
    }
    m_bsmPtr->Start();
    

    
    std::size_t totalEventsNoDataInStorageForAvailableLinks = 0;
    std::size_t totalEventsDataInStorageForCloggedLinks = 0;
    std::size_t numCustodyTransferTimeouts = 0;
    
    
    std::size_t lastIndexToUpLinkVectorOutductInfoRoundRobin = 0;

    

    std::map<uint64_t, bool> mapOuductArrayIndexToPendingLinkChangeEvent; //in case scheduler sends events before egress fully initialized
    bool egressFullyInitialized = false;


    zmq::pollitem_t pollItems[4] = {
        {m_zmqPullSock_boundEgressToConnectingStoragePtr->handle(), 0, ZMQ_POLLIN, 0},
        {m_zmqPullSock_boundIngressToConnectingStoragePtr->handle(), 0, ZMQ_POLLIN, 0},
        {m_zmqSubSock_boundReleaseToConnectingStoragePtr->handle(), 0, ZMQ_POLLIN, 0},
        {m_zmqRepSock_connectingTelemToFromBoundStoragePtr->handle(), 0, ZMQ_POLLIN, 0},
    };
    static const long DEFAULT_BIG_TIMEOUT_POLL = 250;
    long timeoutPoll = DEFAULT_BIG_TIMEOUT_POLL; //0 => no blocking
    boost::posix_time::ptime acsSendNowExpiry = boost::posix_time::microsec_clock::universal_time() + ACS_SEND_PERIOD;

    //notify Init function that worker thread startup is complete
    m_workerThreadStartupMutex.lock();
    m_workerThreadStartupInProgress = false;
    m_workerThreadStartupMutex.unlock();
    m_workerThreadStartupConditionVariable.notify_one();

    while (m_running) {
        int rc = 0;
        try {
            rc = zmq::poll(pollItems, 4, timeoutPoll);
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
                }
                else if ((res->truncated()) || (res->size != sizeof(hdtn::EgressAckHdr))) {
                    LOG_ERROR(subprocess) << "EgressAckHdr wrong size received";
                }
                else if (egressAckHdr.base.type == HDTN_MSGTYPE_EGRESS_ACK_TO_STORAGE) {
                    if (egressFullyInitialized) {
                        OutductInfo_t* outductInfoRawPtr = NULL;
                        if (egressAckHdr.IsOpportunisticLink()) { //isOpportunisticFromStorage
                            std::map<uint64_t, OutductInfoPtr_t>::iterator it = m_mapOpportunisticNextHopNodeIdToOutductInfo.find(egressAckHdr.nextHopNodeId);
                            if (it == m_mapOpportunisticNextHopNodeIdToOutductInfo.end()) {
                                static thread_local bool printedMsg = false;
                                if (!printedMsg) {
                                    LOG_ERROR(subprocess) << "Got an HDTN_MSGTYPE_EGRESS_ACK_TO_STORAGE but could not find opportunistic link nextHopNodeId "
                                        << egressAckHdr.nextHopNodeId << ".  (This message type will now be suppressed.)";
                                    printedMsg = true;
                                }
                            }
                            else {
                                outductInfoRawPtr = it->second.get();
                            }
                        }
                        else {
                            outductInfoRawPtr = m_vectorOutductInfo[egressAckHdr.outductIndex].get();
                            
                        }
                        if (outductInfoRawPtr) {
                            OutductInfo_t& info = *outductInfoRawPtr;
                            if (egressAckHdr.isResponseToStorageCutThrough) {
                                map_id_to_ackdata_t& mapIdToAckData = info.mapIngressUniqueIdToIngressAckData;
                                map_id_to_ackdata_t::iterator it = mapIdToAckData.find(egressAckHdr.custodyId);
                                if (it != mapIdToAckData.end()) {
                                    if (egressAckHdr.error) {
                                        //A bundle that was forwarded without store from storage to egress gets an ack back from egress with the
                                        //error flag set because egress could not send the bundle.
                                        //This will allow storage to trigger a link down event more quickly than waiting for scheduler.
                                        //Since ingress NO LONGER holds the bundle, the error flag will let ingress set a link down event more quickly than scheduler.
                                        //Since isResponseToStorageCutThrough is set, then storage needs the bundle back in a multipart message because storage has not yet written this cut-through bundle to disk.
                                        static thread_local bool printedMsg = false;
                                        if (!printedMsg) {
                                            LOG_WARNING(subprocess) << "Got a link down notification from egress on cut-through path for outduct index "
                                                << egressAckHdr.outductIndex << " for final dest "
                                                << egressAckHdr.finalDestEid
                                                << " because storage forwarding cut-through bundles to egress failed.  (This message type will now be suppressed.)";
                                            printedMsg = true;
                                        }
                                        zmq::message_t zmqBundleDataReceived;
                                        if (!m_zmqPullSock_boundEgressToConnectingStoragePtr->recv(zmqBundleDataReceived, zmq::recv_flags::none)) {
                                            LOG_ERROR(subprocess) << "error in hdtn::ZmqStorageInterface::ThreadFunc (from storage cut-through bundle data) message not received";
                                        }
                                        else {
                                            cbhe_eid_t finalDestEidReturnedFromWrite;
                                            Write(&zmqBundleDataReceived, finalDestEidReturnedFromWrite, true, true); //last true because if cut through then definitely no custody or not admin record
                                            ++m_telem.m_totalBundlesRewrittenToStorageFromFailedEgressSend;
                                        }
                                        SetLinkDown(info);
                                        hdtn::StorageAckHdr* storageAckHdr = (hdtn::StorageAckHdr*)it->second.ackToIngress.data();
                                        storageAckHdr->error = 1;
                                    }
                                    if (!m_zmqPushSock_connectingStorageToBoundIngressPtr->send(std::move(it->second.ackToIngress), zmq::send_flags::dontwait)) {
                                        LOG_ERROR(subprocess) << "zmq could not send ingress a cut-through ack from storage";
                                    }
                                    info.bytesInPipeline -= it->second.bundleSizeBytes;
                                    mapIdToAckData.erase(it);
                                }
                                else {
                                    LOG_ERROR(subprocess) << "Got an HDTN_MSGTYPE_EGRESS_ACK_TO_STORAGE but could not find Ingress Unique Id " << egressAckHdr.custodyId;
                                }
                            }
                            else {
                                custodyid_to_size_map_t& mapCustodyIdToSize = info.mapOpenCustodyIdToBundleSizeBytes;
                                custodyid_to_size_map_t::iterator it = mapCustodyIdToSize.find(egressAckHdr.custodyId);
                                if (it != mapCustodyIdToSize.end()) {
                                    if (egressAckHdr.error) {
                                        //A bundle that was sent from storage to egress gets an ack back from egress with the error flag set because egress could not send the bundle.
                                        //This will allow storage to trigger a link down event more quickly than waiting for scheduler.
                                        //Since storage already has the bundle, the error flag will prevent deletion and move the bundle back to the "awaiting send" state,
                                        //but the bundle won't be immediately released again from storage because of the immediate link down event.
                                        static thread_local bool printedMsg = false;
                                        if (!printedMsg) {
                                            LOG_WARNING(subprocess) << "Got a link down notification from egress for outduct index "
                                                << egressAckHdr.outductIndex << " for final dest "
                                                << egressAckHdr.finalDestEid
                                                << " because storage to egress failed.  (This message type will now be suppressed.)";
                                            printedMsg = true;
                                        }

                                        SetLinkDown(info);
                                        if (!m_bsmPtr->ReturnCustodyIdToAwaitingSend(egressAckHdr.custodyId)) {
                                            LOG_ERROR(subprocess) << "error returning custody id " << egressAckHdr.custodyId << " to awaiting send";
                                        }
                                        m_custodyTimersPtr->CancelCustodyTransferTimer(egressAckHdr.finalDestEid, egressAckHdr.custodyId);
                                    }
                                    else if (egressAckHdr.deleteNow) { //custody not requested, so don't wait on a custody signal to delete the bundle
                                        m_custodyIdAllocatorPtr->FreeCustodyId(egressAckHdr.custodyId);
                                        bool successRemoveBundle = m_bsmPtr->RemoveReadBundleFromDisk(egressAckHdr.custodyId);
                                        if (!successRemoveBundle) {
                                            LOG_ERROR(subprocess) << "error freeing bundle from disk";
                                        }
                                        else {
                                            ++m_telem.m_totalBundlesErasedFromStorageNoCustodyTransfer;
                                        }
                                    }
                                    info.bytesInPipeline -= it->second;
                                    mapCustodyIdToSize.erase(it);
                                }
                                else {
                                    //std::string message = egressAckHdr.custodyId
                                    static thread_local bool printedMsg = false;
                                    if (!printedMsg) {
                                        LOG_ERROR(subprocess) << "Storage got a HDTN_MSGTYPE_EGRESS_ACK_TO_STORAGE for outductIndex="
                                            << egressAckHdr.outductIndex << " but could not find custody id " << egressAckHdr.custodyId
                                            << " .. error=" << (int)egressAckHdr.error
                                            << " isResponseToStorageCutThrough=" << (int)egressAckHdr.isResponseToStorageCutThrough
                                            << " isOpportunisticFromStorage=" << egressAckHdr.IsOpportunisticLink()
                                            << " nextHopNodeId=" << egressAckHdr.nextHopNodeId
                                            << ".  (This message type will now be suppressed.)";
                                        //LOG_DEBUG(subprocess) << SprintCustodyidToSizeMap(mapCustodyIdToSize);
                                        printedMsg = true;
                                    }
                                }
                            }
                        }
                    }
                    else {
                        LOG_ERROR(subprocess) << "Storage got a HDTN_MSGTYPE_EGRESS_ACK_TO_STORAGE before egress fully initialized";
                    }
                }
                else if (egressAckHdr.base.type == HDTN_MSGTYPE_EGRESS_FAILED_BUNDLE_TO_STORAGE) { //bundles sent from ingress to egress but egress could not send
                    zmq::message_t zmqBundleDataReceived;
                    if (!m_zmqPullSock_boundEgressToConnectingStoragePtr->recv(zmqBundleDataReceived, zmq::recv_flags::none)) {
                        LOG_ERROR(subprocess) << "error in hdtn::ZmqStorageInterface::ThreadFunc (from ingress bundle data) message not received";
                    }
                    else {
                        cbhe_eid_t finalDestEidReturnedFromWrite;
                        Write(&zmqBundleDataReceived, finalDestEidReturnedFromWrite, true, true); //last true because if cut through then definitely no custody or not admin record
                        ++m_telem.m_totalBundlesRewrittenToStorageFromFailedEgressSend;
                        finalDestEidReturnedFromWrite.serviceId = 0;
                        if (egressFullyInitialized) {
                            static thread_local bool printedMsg = false;
                            if (!printedMsg) {
                                LOG_WARNING(subprocess) << "Storage got a link down notification from egress (with the failed bundle) for final dest "
                                    << Uri::GetIpnUriStringAnyServiceNumber(finalDestEidReturnedFromWrite.nodeId)
                                    << " because cut through from ingress failed.  (This message type will now be suppressed.)";
                                printedMsg = true;
                            }
                            OutductInfo_t& info = *(m_vectorOutductInfo[egressAckHdr.outductIndex]);
                            SetLinkDown(info);
                        }
                        else {
                            LOG_ERROR(subprocess) << "Storage got a HDTN_MSGTYPE_EGRESS_FAILED_BUNDLE_TO_STORAGE before egress fully initialized";
                        }
                    }
                }
                else if (egressAckHdr.base.type == HDTN_MSGTYPE_ALL_OUTDUCT_CAPABILITIES_TELEMETRY) {
                    AllOutductCapabilitiesTelemetry_t aoct;
                    zmq::message_t zmqMessageOutductTelem;
                    //message guaranteed to be there due to the zmq::send_flags::sndmore
                    if (!m_zmqPullSock_boundEgressToConnectingStoragePtr->recv(zmqMessageOutductTelem, zmq::recv_flags::none)) {
                        LOG_ERROR(subprocess) << "error receiving AllOutductCapabilitiesTelemetry";
                    }
                    else if (!aoct.SetValuesFromJsonCharArray((char*)zmqMessageOutductTelem.data(), zmqMessageOutductTelem.size())) {
                        LOG_ERROR(subprocess) << "error deserializing AllOutductCapabilitiesTelemetry";
                    }
                    else {

                        if (m_vectorOutductInfo.empty()) {
                            LOG_INFO(subprocess) << "Storage received initial " << aoct.outductCapabilityTelemetryList.size() << " outduct capability telemetries from egress";
                            m_vectorOutductInfo.resize(aoct.outductCapabilityTelemetryList.size());
                            for (std::size_t i = 0; i < m_vectorOutductInfo.size(); ++i) {
                                m_vectorOutductInfo[i] = boost::make_unique<OutductInfo_t>();
                            }
                        }
                        else {
                            LOG_INFO(subprocess) << "Storage received updated outduct capability telemetries from egress";
                        }

                        if (m_vectorOutductInfo.size() != aoct.outductCapabilityTelemetryList.size()) {
                            LOG_ERROR(subprocess) << "vectorOutductInfo.size() != aoct.outductCapabilityTelemetryList.size()";
                        }
                        else {
                            uint64_t outductIndex = 0;
                            for (std::list<OutductCapabilityTelemetry_t>::const_iterator itAoct = aoct.outductCapabilityTelemetryList.cbegin(); itAoct != aoct.outductCapabilityTelemetryList.cend(); ++itAoct) {
                                const OutductCapabilityTelemetry_t& oct = *itAoct;
                                OutductInfo_t& outductInfo = *(m_vectorOutductInfo[oct.outductArrayIndex]);
                                outductInfo.halfOfMaxBundlesInPipeline_StorageToEgressPath = oct.maxBundlesInPipeline >> 1;
                                outductInfo.mapIngressUniqueIdToIngressAckData.reserve(oct.maxBundlesInPipeline);
                                outductInfo.mapOpenCustodyIdToBundleSizeBytes.reserve(oct.maxBundlesInPipeline);
                                outductInfo.halfOfMaxBundleSizeBytesInPipeline_StorageToEgressPath = oct.maxBundleSizeBytesInPipeline >> 1;
                                outductInfo.outductIndex = outductIndex++;
                                outductInfo.nextHopNodeId = oct.nextHopNodeId;
                                //outductInfo.linkIsUp = false; set by constructor
                                //outductInfo.bytesInPipeline set to zero by constructor (initialized when vector.resize()
                                outductInfo.eidVec.clear();
                                outductInfo.eidVec.reserve(oct.finalDestinationEidList.size() + oct.finalDestinationNodeIdList.size());
                                for (std::list<cbhe_eid_t>::const_iterator it = oct.finalDestinationEidList.cbegin(); it != oct.finalDestinationEidList.cend(); ++it) {
                                    const cbhe_eid_t& eid = *it;
                                    const eid_plus_isanyserviceid_pair_t key(eid, false); //false => fully qualified service id, true => wildcard (*) service id
                                    outductInfo.eidVec.push_back(key);
                                }
                                for (std::list<uint64_t>::const_iterator it = oct.finalDestinationNodeIdList.cbegin(); it != oct.finalDestinationNodeIdList.cend(); ++it) {
                                    const uint64_t nodeId = *it;
                                    const eid_plus_isanyserviceid_pair_t key(cbhe_eid_t(nodeId, 0), true); //true => any service id.. 0 is don't care
                                    outductInfo.eidVec.push_back(key);
                                }
                            }
                            

                            //in case scheduler sent release messages first
                            for (std::map<uint64_t, bool>::const_iterator it = mapOuductArrayIndexToPendingLinkChangeEvent.cbegin();
                                it != mapOuductArrayIndexToPendingLinkChangeEvent.cend(); ++it)
                            {
                                const uint64_t outductArrayIndex = it->first;
                                const bool linkIsUp = it->second;
                                if (outductArrayIndex < m_vectorOutductInfo.size()) {
                                    OutductInfo_t& info = *(m_vectorOutductInfo[outductArrayIndex]);
                                    if (info.linkIsUp != linkIsUp) {
                                        info.linkIsUp = linkIsUp;
                                        LOG_INFO(subprocess) << "outductArrayIndex=" << outductArrayIndex << ") will " 
                                            << ((linkIsUp) ? "be" : "STOP BEING") << " released from storage";
                                    }
                                }
                                else {
                                    LOG_ERROR(subprocess) << "release message received with out of bounds outductArrayIndex " << outductArrayIndex;
                                }
                            }
                            mapOuductArrayIndexToPendingLinkChangeEvent.clear();

                            RepopulateUpLinksVec();

                            if (!egressFullyInitialized) { //first time this outduct capabilities telemetry received
                                egressFullyInitialized = true;
                                LOG_INFO(subprocess) << "Now running and fully initialized and connected to egress";
                            }
                        }
                    }
                }
                else {
                    LOG_ERROR(subprocess) << "EgressAckHdr unknown type, got " << egressAckHdr.base.type;
                }
            }
            if (pollItems[1].revents & ZMQ_POLLIN) { //from ingress bundle data
                hdtn::ToStorageHdr toStorageHeader;
                const zmq::recv_buffer_result_t res = m_zmqPullSock_boundIngressToConnectingStoragePtr->recv(zmq::mutable_buffer(&toStorageHeader, sizeof(hdtn::ToStorageHdr)), zmq::recv_flags::none);
                if (!res) {
                    LOG_ERROR(subprocess) << "error in hdtn::ZmqStorageInterface::ThreadFunc (from ingress bundle data) message hdr not received";
                }
                else if ((res->truncated()) || (res->size != sizeof(hdtn::ToStorageHdr))) {
                    LOG_ERROR(subprocess) << "error in hdtn::ZmqStorageInterface::ThreadFunc (from ingress bundle data) rhdr.size() != sizeof(hdtn::ToStorageHdr)";
                }
                else if (toStorageHeader.base.type == HDTN_MSGTYPE_STORAGE_ADD_OPPORTUNISTIC_LINK) {
                    const uint64_t nodeId = toStorageHeader.ingressUniqueId;
                    
                    std::pair<std::map<uint64_t, OutductInfoPtr_t>::iterator, bool> ret = m_mapOpportunisticNextHopNodeIdToOutductInfo.
#if (__cplusplus >= 201703L) //try_emplace would be most ideal so it doesnt create and destroy element if exists
                        try_emplace( 
#else
                        emplace(
#endif
                    nodeId, boost::make_unique<OutductInfo_t>());
                    //(true if insertion happened, false if it did not).
                    if (ret.second) {
                        OutductInfo_t& info = *(ret.first->second);
                        info.eidVec.resize(1);
                        const eid_plus_isanyserviceid_pair_t key(cbhe_eid_t(nodeId, 0), true); //true => any service id.. 0 is don't care
                        info.eidVec[0] = key;
                        info.nextHopNodeId = nodeId;
                        info.linkIsUp = true;
                        info.outductIndex = UINT64_MAX;
                        info.halfOfMaxBundlesInPipeline_StorageToEgressPath = 5; //TODO
                        info.halfOfMaxBundleSizeBytesInPipeline_StorageToEgressPath = info.halfOfMaxBundlesInPipeline_StorageToEgressPath * m_hdtnConfig.m_maxBundleSizeBytes;
                        RepopulateUpLinksVec();
                        LOG_INFO(subprocess) << "Adding Opportunistic link from ingress connection.. " << info;
                    }
                    else {
                        LOG_ERROR(subprocess) << "Ignoring Duplicate Message for adding Opportunistic link from ingress connection.. ";
                    }
                }
                else if (toStorageHeader.base.type == HDTN_MSGTYPE_STORAGE_REMOVE_OPPORTUNISTIC_LINK) {
                    const uint64_t nodeId = toStorageHeader.ingressUniqueId;
                    const bool wasErased = (m_mapOpportunisticNextHopNodeIdToOutductInfo.erase(nodeId) == 1);
                    if (wasErased) {
                        RepopulateUpLinksVec();
                    }
                    LOG_INFO(subprocess) << "Removing Opportunistic link from ingress connection.. finalDestEid ("
                        << Uri::GetIpnUriStringAnyServiceNumber(nodeId)
                        << ") will " << ((wasErased) ? "STOP" : "REMAIN STOPPED FROM") << " being released from storage";
                }
                else if (toStorageHeader.base.type == HDTN_MSGTYPE_STORE) {
                    //storageStats.inBytes += sizeof(hdtn::ToStorageHdr);
                    //++storageStats.inMsg;

                    zmq::message_t zmqBundleDataReceived;
                    if (!m_zmqPullSock_boundIngressToConnectingStoragePtr->recv(zmqBundleDataReceived, zmq::recv_flags::none)) {
                        LOG_ERROR(subprocess) << "hdtn::ZmqStorageInterface::ThreadFunc (from ingress bundle data) message not received";
                    }
                    else {
                        //send ack message to ingress
                        //force natural/64-bit alignment
                        hdtn::StorageAckHdr* storageAckHdr = new hdtn::StorageAckHdr();
                        zmq::message_t zmqMessageStorageAckHdrWithDataStolen(storageAckHdr, sizeof(hdtn::StorageAckHdr), CustomCleanupStorageAckHdr, storageAckHdr);

                        //memset 0 not needed because all values set below
                        storageAckHdr->base.type = HDTN_MSGTYPE_STORAGE_ACK_TO_INGRESS;
                        storageAckHdr->base.flags = 0;
                        storageAckHdr->error = 0;
                        storageAckHdr->ingressUniqueId = toStorageHeader.ingressUniqueId;
                        storageAckHdr->outductIndex = toStorageHeader.outductIndex;

                        if ((toStorageHeader.dontStoreBundle)
                            && (toStorageHeader.outductIndex < m_vectorOutductInfo.size()) //if outductIndex is UINT64_MAX then bundle needs stored
                            && m_vectorOutductInfo[toStorageHeader.outductIndex]->linkIsUp)
                        {
                            OutductInfo_t& info = *(m_vectorOutductInfo[toStorageHeader.outductIndex]);
                            info.cutThroughQueue.emplace(std::move(zmqBundleDataReceived),
                                std::move(zmqMessageStorageAckHdrWithDataStolen), toStorageHeader.finalDestEid, toStorageHeader.ingressUniqueId);
                        }
                        else {
                            //storageStats.inBytes += zmqBundleDataReceived.size();

                            cbhe_eid_t finalDestEidReturnedFromWrite;
                            const bool isCertainThatThisBundleHasNoCustodyOrIsNotAdminRecord = (toStorageHeader.isCustodyOrAdminRecord == 0);
                            Write(&zmqBundleDataReceived, finalDestEidReturnedFromWrite, false, isCertainThatThisBundleHasNoCustodyOrIsNotAdminRecord);

                            //storageAckHdr->finalDestEid = finalDestEidReturnedFromWrite; //no longer needed as ingress decodes that

                            if (!m_zmqPushSock_connectingStorageToBoundIngressPtr->send(std::move(zmqMessageStorageAckHdrWithDataStolen), zmq::send_flags::dontwait)) {
                                LOG_ERROR(subprocess) << "zmq could not send ingress an ack from storage";
                            }
                        }
                    }
                }
                else {
                    LOG_ERROR(subprocess) << "hdtn::ZmqStorageInterface::ThreadFunc (from ingress bundle data) unknown message type";
                }
            }
            if (pollItems[2].revents & ZMQ_POLLIN) { //release messages
                hdtn::IreleaseChangeHdr releaseChangeHdr;
                const zmq::recv_buffer_result_t res = m_zmqSubSock_boundReleaseToConnectingStoragePtr->recv(zmq::mutable_buffer(&releaseChangeHdr, sizeof(releaseChangeHdr)), zmq::recv_flags::none);
                if (!res) {
                    LOG_ERROR(subprocess) << "unable to receive IreleaseChangeHdr message";
                }
                else if ((res->truncated()) || (res->size != sizeof(releaseChangeHdr))) {
                    LOG_ERROR(subprocess) << "message mismatch with IreleaseChangeHdr: untruncated = " << res->untruncated_size
                        << " truncated = " << res->size << " expected = " << sizeof(releaseChangeHdr);
                }
                else if (releaseChangeHdr.base.type == HDTN_MSGTYPE_ILINKUP) {
                    LOG_INFO(subprocess) << "link up release message received";
                    if (egressFullyInitialized) {
                        if (releaseChangeHdr.outductArrayIndex < m_vectorOutductInfo.size()) {
                            OutductInfo_t& info = *(m_vectorOutductInfo[releaseChangeHdr.outductArrayIndex]);
                            if (!info.linkIsUp) {
                                info.linkIsUp = true;
                                RepopulateUpLinksVec();
                                LOG_INFO(subprocess) << "outductArrayIndex=" << releaseChangeHdr.outductArrayIndex << " " << info;
                            }
                        }
                        else {
                            LOG_ERROR(subprocess) << "link up message received with out of bounds outductArrayIndex " << releaseChangeHdr.outductArrayIndex;
                        }
                    }
                    else {
                        mapOuductArrayIndexToPendingLinkChangeEvent[releaseChangeHdr.outductArrayIndex] = true;
                        LOG_INFO(subprocess) << "nextHopNodeId: " << releaseChangeHdr.nextHopNodeId
                            << " outductArrayIndex=" << releaseChangeHdr.outductArrayIndex
                            << ") will be released from storage once egress is fully initialized";
                    }
                }
                else if (releaseChangeHdr.base.type == HDTN_MSGTYPE_ILINKDOWN) {
                    if (egressFullyInitialized) {
                        if (releaseChangeHdr.outductArrayIndex < m_vectorOutductInfo.size()) {
                            OutductInfo_t& info = *(m_vectorOutductInfo[releaseChangeHdr.outductArrayIndex]);
                            SetLinkDown(info);
                        }
                        else {
                            LOG_ERROR(subprocess) << "link down message received with out of bounds outductArrayIndex " << releaseChangeHdr.outductArrayIndex;
                        }
                    }
                    else {
                        mapOuductArrayIndexToPendingLinkChangeEvent[releaseChangeHdr.outductArrayIndex] = true;
                        LOG_INFO(subprocess) << "nextHopNodeId: " << releaseChangeHdr.nextHopNodeId
                            << " outductArrayIndex=" << releaseChangeHdr.outductArrayIndex
                            << ") will STOP BEING released from storage once egress is fully initialized";
                    }
                }
                else {
                    LOG_ERROR(subprocess) << "unknown IreleaseChangeHdr message type " << releaseChangeHdr.base.type;
                }
            }
            if (pollItems[3].revents & ZMQ_POLLIN) { //telem requests data
                TelemEventsHandler();
            }
        }

        const boost::posix_time::ptime nowPtime = boost::posix_time::microsec_clock::universal_time();
        if ((acsSendNowExpiry <= nowPtime) || (m_ctmPtr->GetLargestNumberOfFills() > ACS_MAX_FILLS_PER_ACS_PACKET)) {
            //test with generate all
            std::list<BundleViewV6> newAcsRenderedBundleViewList;
            if (m_ctmPtr->GenerateAllAcsBundlesAndClear(newAcsRenderedBundleViewList)) {
                for(std::list<BundleViewV6>::iterator it = newAcsRenderedBundleViewList.begin(); it != newAcsRenderedBundleViewList.end(); ++it) {
                    WriteAcsBundle(it->m_primaryBlockView.header, it->m_frontBuffer);
                }
            }
            acsSendNowExpiry = nowPtime + ACS_SEND_PERIOD;
        }

        uint64_t custodyIdExpiredAndNeedingResent;
        while (m_custodyTimersPtr->PollOneAndPopAnyExpiredCustodyTimer(custodyIdExpiredAndNeedingResent, nowPtime)) {
            if (m_bsmPtr->ReturnCustodyIdToAwaitingSend(custodyIdExpiredAndNeedingResent)) {
                ++numCustodyTransferTimeouts;
            }
            else {
                LOG_ERROR(subprocess) << "error unable to return expired custody id " << custodyIdExpiredAndNeedingResent << " to the awaiting send";
            }
        }

        DeleteExpiredBundles();

        //For each outduct or opportunistic induct, send to Egress the bundles read from disk or the
        //bundles forwarded over the cut-through path from ingress, alternating/multiplexing between the two.
        //Maintain up to that outduct's own sending pipeline limit,
        //which shall not exceed, whatever comes first, either:
        //1.) More bundles than maxNumberOfBundlesInPipeline
        //2.) More total bytes of bundles than maxSumOfBundleBytesInPipeline
        //Note: 
        // - For bundles read from disk, when a bundle is acked from egress using the custody id,
        //   the bundle is deleted from disk and a new bundle can be sent.
        // - For bundles forwarded via cut-through from ingress,
        //   when a bundle is acked from egress using the ingressUniqueId,
        //   an ack is sent to ingress (from storage) and a new bundle can be sent from ingress to storage.
        timeoutPoll = DEFAULT_BIG_TIMEOUT_POLL;
        static constexpr long shortestTimeoutPoll1Ms = 1;
        for (std::size_t count = 0; count < m_vectorUpLinksOutductInfoPtrs.size(); ++count) {
            ++lastIndexToUpLinkVectorOutductInfoRoundRobin;
            if (lastIndexToUpLinkVectorOutductInfoRoundRobin >= m_vectorUpLinksOutductInfoPtrs.size()) {
                lastIndexToUpLinkVectorOutductInfoRoundRobin = 0;
            }
            OutductInfo_t& info = *(m_vectorUpLinksOutductInfoPtrs[lastIndexToUpLinkVectorOutductInfoRoundRobin]);
            const std::size_t totalInPipelineStorageToEgressThisLink = info.mapOpenCustodyIdToBundleSizeBytes.size() + info.mapIngressUniqueIdToIngressAckData.size();
            const uint64_t maxBundleSizeToRead = info.halfOfMaxBundleSizeBytesInPipeline_StorageToEgressPath - info.bytesInPipeline;
            uint64_t returnedBundleSizeReadFromDisk;
            if (totalInPipelineStorageToEgressThisLink < info.halfOfMaxBundlesInPipeline_StorageToEgressPath) { //not clogged by bundle count in pipeline
                if (info.stateTryCutThrough && (!info.cutThroughQueue.empty())
                    && (info.cutThroughQueue.front().bundleToEgress.size() <= maxBundleSizeToRead))
                { //not clogged by bundle total bytes in pipeline
                    CutThroughQueueData& qd = info.cutThroughQueue.front();
                    
                    //force natural/64-bit alignment
                    hdtn::ToEgressHdr* toEgressHdr = new hdtn::ToEgressHdr();
                    zmq::message_t zmqMessageToEgressHdrWithDataStolen(toEgressHdr, sizeof(hdtn::ToEgressHdr), CustomCleanupToEgressHdr, toEgressHdr);

                    //memset 0 not needed because all values set below
                    toEgressHdr->base.type = HDTN_MSGTYPE_EGRESS;
                    toEgressHdr->base.flags = 0;
                    toEgressHdr->nextHopNodeId = info.nextHopNodeId;
                    toEgressHdr->finalDestEid = qd.finalDestEid;
                    toEgressHdr->hasCustody = 0;
                    toEgressHdr->isCutThroughFromStorage = 1;
                    toEgressHdr->custodyId = qd.ingressUniqueId;
                    toEgressHdr->outductIndex = info.outductIndex;

                    const uint64_t bundleSizeBytes = qd.bundleToEgress.size(); //capture before move

                    if (!m_zmqPushSock_connectingStorageToBoundEgressPtr->send(std::move(zmqMessageToEgressHdrWithDataStolen), zmq::send_flags::sndmore | zmq::send_flags::dontwait)) {
                        LOG_ERROR(subprocess) << "could not forward header of cut-through bundle to egress";
                    }
                    else if (!m_zmqPushSock_connectingStorageToBoundEgressPtr->send(std::move(qd.bundleToEgress), zmq::send_flags::dontwait)) {
                        LOG_ERROR(subprocess) << "could not forward cut-through bundle to egress";
                    }
                    //with cut through bundles, don't send an ack to ingress until fully sent confirmation ack from egress, hence the map below to defer that
                    else if (!info.mapIngressUniqueIdToIngressAckData.emplace(std::piecewise_construct,
                        std::forward_as_tuple(qd.ingressUniqueId),
                        std::forward_as_tuple(std::move(qd.ackToIngress), bundleSizeBytes) ).second)
                    {
                        //zmqMessageStorageAckHdrWithDataStolen gets destroyed, unless try_emplace was used
                        LOG_ERROR(subprocess) << "ingressUniqueId " << qd.ingressUniqueId << " already exists in mapIngressUniqueIdToIngressAckZmqMsg";
                    }
                    else { //success
                        info.bytesInPipeline += bundleSizeBytes;
                        timeoutPoll = 0; //no timeout as we need to keep feeding to egress
                        ++m_telem.m_totalBundlesSentToEgressFromStorageForwardCutThrough;
                        m_telem.m_totalBundleBytesSentToEgressFromStorageForwardCutThrough += bundleSizeBytes;
                    }
                    info.cutThroughQueue.pop();
                }
                else if (ReleaseOne_NoBlock(info, maxBundleSizeToRead, returnedBundleSizeReadFromDisk)) { //true => (successfully sent to egress)
                    if (info.mapOpenCustodyIdToBundleSizeBytes.emplace(m_sessionRead.custodyId, returnedBundleSizeReadFromDisk).second) {
                        if (m_sessionRead.catalogEntryPtr->HasCustody()) {
                            m_custodyTimersPtr->StartCustodyTransferTimer(m_sessionRead.catalogEntryPtr->destEid, m_sessionRead.custodyId);
                        }
                        info.bytesInPipeline += returnedBundleSizeReadFromDisk;
                        timeoutPoll = 0; //no timeout as we need to keep feeding to egress
                        ++m_telem.m_totalBundlesSentToEgressFromStorageReadFromDisk;
                        m_telem.m_totalBundleBytesSentToEgressFromStorageReadFromDisk += returnedBundleSizeReadFromDisk;
                    }
                    else {
                        LOG_ERROR(subprocess) << "could not insert custody id into finalDestNodeIdToOpenCustIdsMap";
                    }
                }
                info.stateTryCutThrough = !info.stateTryCutThrough; //alternate/multiplex between disk reads and cut-through forwards to prevent one from getting "starved"
            }
            if (timeoutPoll == 0) {
                break; //return to zmq loop with zero timeout
            }
            else if (timeoutPoll != shortestTimeoutPoll1Ms) { //potentially clogged
                if (PeekOne(info.eidVec) > 0) { //data available in storage for clogged links
                    timeoutPoll = shortestTimeoutPoll1Ms; //shortest timeout 1ms as we wait for acks
                    ++totalEventsDataInStorageForCloggedLinks;
                }
                else { //no data in storage for any available links
                    //timeoutPoll = DEFAULT_BIG_TIMEOUT_POLL;
                    ++totalEventsNoDataInStorageForAvailableLinks;
                }
            }
        }
    }
    LOG_DEBUG(subprocess) << "Storage bundles sent: FromDisk=" << m_telem.m_totalBundlesSentToEgressFromStorageReadFromDisk
        << "  FromCutThroughForward=" << m_telem.m_totalBundlesSentToEgressFromStorageForwardCutThrough;
    LOG_DEBUG(subprocess) << "totalEventsNoDataInStorageForAvailableLinks: " << totalEventsNoDataInStorageForAvailableLinks;
    LOG_DEBUG(subprocess) << "totalEventsDataInStorageForCloggedLinks: " << totalEventsDataInStorageForCloggedLinks;
    LOG_DEBUG(subprocess) << "m_numRfc5050CustodyTransfers: " << m_telem.m_numRfc5050CustodyTransfers;
    LOG_DEBUG(subprocess) << "m_numAcsCustodyTransfers: " << m_telem.m_numAcsCustodyTransfers;
    LOG_DEBUG(subprocess) << "m_numAcsPacketsReceived: " << m_telem.m_numAcsPacketsReceived;
    LOG_DEBUG(subprocess) << "m_totalBundlesErasedFromStorageNoCustodyTransfer: " << m_telem.m_totalBundlesErasedFromStorageNoCustodyTransfer;
    LOG_DEBUG(subprocess) << "m_totalBundlesErasedFromStorageWithCustodyTransfer: " << m_telem.m_totalBundlesErasedFromStorageWithCustodyTransfer;
    LOG_DEBUG(subprocess) << "numCustodyTransferTimeouts: " << numCustodyTransferTimeouts;
    LOG_DEBUG(subprocess) << "m_totalBundlesRewrittenToStorageFromFailedEgressSend: " << m_telem.m_totalBundlesRewrittenToStorageFromFailedEgressSend;
}

void ZmqStorageInterface::Impl::TelemEventsHandler() {
    bool doSendExpiring = false;
    GetExpiringStorageApiCommand_t getExpiringStorageApiCommand;
    uint8_t telemMsgByte;
    const zmq::recv_buffer_result_t res = m_zmqRepSock_connectingTelemToFromBoundStoragePtr->recv(zmq::mutable_buffer(&telemMsgByte, sizeof(telemMsgByte)), zmq::recv_flags::dontwait);
    if (!res) {
        LOG_ERROR(subprocess) << "error in ZmqStorageInterface::ThreadFunc: cannot read telemMsgByte";
        return;
    }
    else if ((res->truncated()) || (res->size != sizeof(telemMsgByte))) {
        LOG_ERROR(subprocess) << "telemMsgByte message mismatch: untruncated = " << res->untruncated_size
            << " truncated = " << res->size << " expected = " << sizeof(telemMsgByte);
        return;
    }

    bool hasApiCalls = telemMsgByte > TELEM_REQ_MSG;
    if (hasApiCalls) {
        zmq::message_t apiMsg;
        do {
            if (!m_zmqRepSock_connectingTelemToFromBoundStoragePtr->recv(apiMsg, zmq::recv_flags::dontwait)) {
                LOG_ERROR(subprocess) << "error receiving api message";
                return;
            }
            std::string apiCall = ApiCommand_t::GetApiCallFromJson(apiMsg.to_string());
            LOG_INFO(subprocess) << "got api call " << apiCall;
            if (apiCall == "get_expiring_storage") {
                if (!getExpiringStorageApiCommand.SetValuesFromJson(apiMsg.to_string())) {
                    return;
                };
                doSendExpiring = true;
            }
        } while(apiMsg.more());
    }

    //send normal storage telemetry
    SyncTelemetry();
    m_telem.m_timestampMilliseconds = TimestampUtil::GetMillisecondsSinceEpochRfc5050();

    std::string* storageTelemJsonStringPtr = new std::string(m_telem.ToJson());
    std::string& strRef = *storageTelemJsonStringPtr;
    zmq::message_t zmqJsonMessage(&strRef[0], storageTelemJsonStringPtr->size(), CustomCleanupStdString, storageTelemJsonStringPtr);

    const zmq::send_flags additionalFlags = (doSendExpiring) ? zmq::send_flags::sndmore : zmq::send_flags::none;
    if (!m_zmqRepSock_connectingTelemToFromBoundStoragePtr->send(std::move(zmqJsonMessage), zmq::send_flags::dontwait | additionalFlags)) {
        LOG_ERROR(subprocess) << "can't send json telemetry to telem";
    }

    if (!doSendExpiring) {
        return;
    }

    //send storage expiring telemetry
    StorageExpiringBeforeThresholdTelemetry_t expiringTelem;
    expiringTelem.priority = getExpiringStorageApiCommand.m_priority;
    expiringTelem.thresholdSecondsSinceStartOfYear2000 = TimestampUtil::GetSecondsSinceEpochRfc5050(
        boost::posix_time::microsec_clock::universal_time() + boost::posix_time::seconds(getExpiringStorageApiCommand.m_thresholdSecondsFromNow));
    if (!m_bsmPtr->GetStorageExpiringBeforeThresholdTelemetry(expiringTelem)) {
        LOG_ERROR(subprocess) << "storage can't get StorageExpiringBeforeThresholdTelemetry";
    }
    else {
        std::string* expiringTelemJsonStringPtr = new std::string(expiringTelem.ToJson());
        std::string& strRefExpiring = *expiringTelemJsonStringPtr;
        zmq::message_t zmqExpiringTelemJsonMessage(&strRefExpiring[0],
            expiringTelemJsonStringPtr->size(), CustomCleanupStdString, expiringTelemJsonStringPtr);

        if (!m_zmqRepSock_connectingTelemToFromBoundStoragePtr->send(std::move(zmqExpiringTelemJsonMessage), zmq::send_flags::dontwait)) {
            LOG_ERROR(subprocess) << "storage can't send json StorageExpiringBeforeThresholdTelemetry_t to telem";
        }
    }
}

std::size_t ZmqStorageInterface::Impl::GetCurrentNumberOfBundlesDeletedFromStorage() {
    return m_telem.m_totalBundlesErasedFromStorageNoCustodyTransfer + m_telem.m_totalBundlesErasedFromStorageWithCustodyTransfer;
}
std::size_t ZmqStorageInterface::GetCurrentNumberOfBundlesDeletedFromStorage() {
    return m_pimpl->GetCurrentNumberOfBundlesDeletedFromStorage();
}
