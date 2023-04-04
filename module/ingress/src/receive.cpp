/**
 * @file receive.cpp
 * @author  Brian Tomko <brian.j.tomko@nasa.gov>
 * @author  Gilbert Clark
 *
 * @copyright Copyright © 2021 United States Government as represented by
 * the National Aeronautics and Space Administration.
 * No copyright is claimed in the United States under Title 17, U.S.Code.
 * All Other Rights Reserved.
 *
 * @section LICENSE
 * Released under the NASA Open Source Agreement (NOSA)
 * See LICENSE.md in the source root directory for more information.
 *
 * @section DESCRIPTION
 *
 * This file contains the implemetation for the ingress module of HDTN.
 */
#include "ingress.h"
//#include "util/tsc.h"
#include "codec/bpv6.h"
#include "Logger.h"
#include "message.hpp"
#include <boost/asio.hpp>
#include <boost/thread.hpp>
#include "InductManager.h"
#include <list>
#include <queue>
#include <boost/bind/bind.hpp>
#include <boost/make_unique.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/format.hpp>
#include "Uri.h"
#include "codec/BundleViewV6.h"
#include "codec/BundleViewV7.h"
#include "TcpclInduct.h"
#include "TcpclV4Induct.h"
#include "StcpInduct.h"
#include "FreeListAllocator.h"
#include "TelemetryDefinitions.h"
#include "ThreadNamer.h"
#include <unordered_map>
#if (__cplusplus >= 201703L)
#include <shared_mutex>
#endif

namespace hdtn {

static constexpr hdtn::Logger::SubProcess subprocess = hdtn::Logger::SubProcess::ingress;
static constexpr uint64_t STORAGE_MAX_BUNDLES_IN_PIPELINE = 5;//"zmq-path-to-storage" up to zmqMaxMessageSizeBytes or 5 bundles,
static constexpr uint64_t MY_PING_SERVICE_ID = 1;

struct Ingress::Impl : private boost::noncopyable {

    Impl();
    ~Impl();
    void Stop();
    bool Init(const HdtnConfig& hdtnConfig, const HdtnDistributedConfig& hdtnDistributedConfig, zmq::context_t* hdtnOneProcessZmqInprocContextPtr);

private:
    void ReadZmqAcksThreadFunc();
    void ZmqTelemThreadFunc();
    void SchedulerEventHandler();
    bool ProcessPaddedData(uint8_t* bundleDataBegin, std::size_t bundleCurrentSize,
        std::unique_ptr<zmq::message_t>& zmqPaddedMessageUnderlyingDataUniquePtr, padded_vector_uint8_t& paddedVecMessageUnderlyingData,
        const bool usingZmqData, const bool needsProcessing);
    void ReadTcpclOpportunisticBundlesFromEgressThreadFunc();
    void WholeBundleReadyCallback(padded_vector_uint8_t& wholeBundleVec);
    void OnNewOpportunisticLinkCallback(const uint64_t remoteNodeId, Induct* thisInductPtr, void* sinkPtr);
    void OnDeletedOpportunisticLinkCallback(const uint64_t remoteNodeId, Induct* thisInductPtr, void* sinkPtrAboutToBeDeleted);
    void SendOpportunisticLinkMessages(const uint64_t remoteNodeId, bool isAvailable);
    void SendPing(const uint64_t remoteNodeId, const uint64_t remotePingServiceNumber, const uint64_t bpVersion);
    void ProcessReceivedPingPayload(const uint8_t* data, const uint64_t size, const uint64_t bpVersion);

public:
    uint64_t m_bundleCountStorage; //protected by m_ingressToStorageZmqSocketMutex
    uint64_t m_bundleByteCountStorage; //protected by m_ingressToStorageZmqSocketMutex
    uint64_t m_bundleCountEgress; //protected by m_ingressToEgressZmqSocketMutex
    uint64_t m_bundleByteCountEgress; //protected by m_ingressToEgressZmqSocketMutex

private:
    struct BundlePipelineAckingSet : private boost::noncopyable {
        BundlePipelineAckingSet() = delete; //vector resize() not possible, must use reserve()
        BundlePipelineAckingSet(const uint64_t paramMaxBundlesInPipeline,
            const uint64_t paramMaxBundleSizeBytesInPipeline, const uint64_t paramNextHopNodeId, bool paramLinkIsUp);
        void Update(const uint64_t paramMaxBundlesInPipeline,
            const uint64_t paramMaxBundleSizeBytesInPipeline, const uint64_t paramNextHopNodeId, bool paramLinkIsUp);

        bool CompareAndPop_ThreadSafe(const uint64_t uniqueId, const bool isEgress);

        bool WaitForPipelineAvailabilityAndReserve(const bool checkEgressPipeline, const bool checkStoragePipeline,
            const boost::posix_time::time_duration& timeoutDuration, const uint64_t uniqueId, const uint64_t bundleSizeBytes,
            bool& reservedEgressPipelineAvailability, bool& reservedStoragePipelineAvailability);
        bool WaitForStoragePipelineAvailabilityAndReserve(const boost::posix_time::time_duration& timeoutDuration,
            const uint64_t uniqueId, const uint64_t bundleSizeBytes);
        void NotifyAll();
        uint64_t GetNextHopNodeId() const;
    private:
        
        boost::mutex m_mutex;
        boost::condition_variable m_conditionVariable;
        typedef std::unordered_map<uint64_t, uint64_t,
            std::hash<uint64_t>,
            std::equal_to<uint64_t>,
            FreeListAllocatorDynamic<std::pair<const uint64_t, uint64_t> > > umap_64_to_64_t;
        umap_64_to_64_t m_mapEgressBundleUniqueIdToBundleSizeBytes;
        umap_64_to_64_t m_mapStorageBundleUniqueIdToBundleSizeBytes;
        uint64_t m_egressBytesInPipeline;
        uint64_t m_storageBytesInPipeline;

        uint64_t m_maxBundlesInPipeline;
        uint64_t m_maxBundleSizeBytesInPipeline;
        uint64_t m_nextHopNodeId;
    public:
        bool m_linkIsUp;
    };
    typedef std::unique_ptr<BundlePipelineAckingSet> BundlePipelineAckingSetPtr;

    std::unique_ptr<zmq::context_t> m_zmqCtxPtr;
    std::unique_ptr<zmq::socket_t> m_zmqPushSock_boundIngressToConnectingEgressPtr;
    std::unique_ptr<zmq::socket_t> m_zmqPullSock_connectingEgressToBoundIngressPtr;
    std::unique_ptr<zmq::socket_t> m_zmqPullSock_connectingEgressBundlesOnlyToBoundIngressPtr;
    std::unique_ptr<zmq::socket_t> m_zmqPushSock_boundIngressToConnectingStoragePtr;
    std::unique_ptr<zmq::socket_t> m_zmqPullSock_connectingStorageToBoundIngressPtr;
    std::unique_ptr<zmq::socket_t> m_zmqSubSock_boundSchedulerToConnectingIngressPtr;

    std::unique_ptr<zmq::socket_t> m_zmqRepSock_connectingTelemToFromBoundIngressPtr;

    //std::shared_ptr<zmq::context_t> m_zmqTelemCtx;
    //std::shared_ptr<zmq::socket_t> m_zmqTelemSock;

    InductManager m_inductManager;
    HdtnConfig m_hdtnConfig;
    cbhe_eid_t M_HDTN_EID_CUSTODY;
    cbhe_eid_t M_HDTN_EID_ECHO;
    cbhe_eid_t M_HDTN_EID_PING;
    cbhe_eid_t M_HDTN_EID_TO_SCHEDULER_BUNDLES;
    boost::posix_time::time_duration M_MAX_INGRESS_BUNDLE_WAIT_ON_EGRESS_TIME_DURATION;

    std::unique_ptr<boost::thread> m_threadZmqAckReaderPtr;
    std::unique_ptr<boost::thread> m_threadZmqTelemPtr;
    std::unique_ptr<boost::thread> m_threadTcpclOpportunisticBundlesFromEgressReaderPtr;
    std::vector<BundlePipelineAckingSetPtr> m_vectorBundlePipelineAckingSet; //final dest node id to set
    BundlePipelineAckingSet m_singleStorageBundlePipelineAckingSet; //non-cut-through, outduct index of UINT64_MAX
    std::map<uint64_t, uint64_t> m_mapNextHopNodeIdToOutductArrayIndex;
    std::map<uint64_t, uint64_t> m_mapFinalDestNodeIdToOutductArrayIndex;
    std::map<cbhe_eid_t, uint64_t> m_mapFinalDestEidToOutductArrayIndex;

#if (__cplusplus >= 201703L)
    std::shared_mutex m_sharedMutexFinalDestsToOutductArrayIndexMaps;
    typedef std::shared_lock<std::shared_mutex> ingress_shared_lock_t;
    typedef std::unique_lock<std::shared_mutex> ingress_exclusive_lock_t;
#else
    boost::shared_mutex m_sharedMutexFinalDestsToOutductArrayIndexMaps;
    typedef boost::shared_lock<boost::shared_mutex> ingress_shared_lock_t;
    typedef boost::unique_lock<boost::shared_mutex> ingress_exclusive_lock_t;
#endif



    boost::mutex m_ingressToEgressZmqSocketMutex;
    boost::mutex m_ingressToStorageZmqSocketMutex;
    std::size_t m_eventsTooManyInStorageCutThroughQueue;
    std::size_t m_eventsTooManyInEgressCutThroughQueue;
    std::size_t m_eventsTooManyInAllCutThroughQueues;
    volatile bool m_running;
    volatile bool m_egressFullyInitialized;
    boost::atomic_uint64_t m_nextBundleUniqueIdAtomic;

    std::map<uint64_t, Induct*> m_availableDestOpportunisticNodeIdToTcpclInductMap;
    boost::mutex m_availableDestOpportunisticNodeIdToTcpclInductMapMutex;

    //for blocking until worker-thread startup
    volatile bool m_workerThreadStartupInProgress;
    boost::mutex m_workerThreadStartupMutex;
    boost::condition_variable m_workerThreadStartupConditionVariable;

    //for blocking until telem-thread startup
    volatile bool m_telemThreadStartupInProgress;
    boost::mutex m_telemThreadStartupMutex;
    boost::condition_variable m_telemThreadStartupConditionVariable;
};

Ingress::Impl::BundlePipelineAckingSet::BundlePipelineAckingSet(const uint64_t paramMaxBundlesInPipeline,
    const uint64_t paramMaxBundleSizeBytesInPipeline, const uint64_t paramNextHopNodeId, bool paramLinkIsUp) :
    m_egressBytesInPipeline(0),
    m_storageBytesInPipeline(0)
{
    Update(paramMaxBundlesInPipeline, paramMaxBundleSizeBytesInPipeline, paramNextHopNodeId, paramLinkIsUp);
}
void Ingress::Impl::BundlePipelineAckingSet::Update(const uint64_t paramMaxBundlesInPipeline,
    const uint64_t paramMaxBundleSizeBytesInPipeline, const uint64_t paramNextHopNodeId, bool paramLinkIsUp)
{
    m_maxBundlesInPipeline = paramMaxBundlesInPipeline;
    m_maxBundleSizeBytesInPipeline = paramMaxBundleSizeBytesInPipeline;
    m_nextHopNodeId = paramNextHopNodeId;
    m_linkIsUp = paramLinkIsUp;
    //By default, unordered_set containers have a max_load_factor of 1.0.
    m_mapEgressBundleUniqueIdToBundleSizeBytes.reserve(m_maxBundlesInPipeline); //maxBundlesInPipeline is double of half
    m_mapEgressBundleUniqueIdToBundleSizeBytes.get_allocator().SetMaxListSizeFromGetAllocatorCopy(m_maxBundlesInPipeline + 2);

    m_mapStorageBundleUniqueIdToBundleSizeBytes.reserve(m_maxBundlesInPipeline);
    m_mapStorageBundleUniqueIdToBundleSizeBytes.get_allocator().SetMaxListSizeFromGetAllocatorCopy(m_maxBundlesInPipeline + 2);
}

bool Ingress::Impl::BundlePipelineAckingSet::CompareAndPop_ThreadSafe(const uint64_t uniqueId, const bool isEgress) {
    
    uint64_t& bytesInPipelineRef = (isEgress) ? m_egressBytesInPipeline : m_storageBytesInPipeline;
    umap_64_to_64_t& mapBundleUniqueIdToBundleSizeBytes = (isEgress) ?
        m_mapEgressBundleUniqueIdToBundleSizeBytes : m_mapStorageBundleUniqueIdToBundleSizeBytes;

    boost::mutex::scoped_lock lock(m_mutex);
    umap_64_to_64_t::iterator it = mapBundleUniqueIdToBundleSizeBytes.find(uniqueId);
    if (it == mapBundleUniqueIdToBundleSizeBytes.end()) {
        return false;
    }
    const uint64_t bundleSizeBytes = it->second;
    bytesInPipelineRef -= bundleSizeBytes;
    mapBundleUniqueIdToBundleSizeBytes.erase(it);
    return true;
}

//make sure at least one of [checkEgressPipeline, checkStoragePipeline] are true, otherwise a timeout will occur followed by a return false
//return true if either the egress or storage got reserved, false if timeout
bool Ingress::Impl::BundlePipelineAckingSet::WaitForPipelineAvailabilityAndReserve(const bool checkEgressPipeline, const bool checkStoragePipeline,
    const boost::posix_time::time_duration & timeoutDuration, const uint64_t uniqueId, const uint64_t bundleSizeBytes,
    bool & reservedEgressPipelineAvailability, bool& reservedStoragePipelineAvailability)
{
    reservedEgressPipelineAvailability = false;
    reservedStoragePipelineAvailability = false;
    const uint64_t halfOfMaxBundlesInPipeline = m_maxBundlesInPipeline >> 1;
    const uint64_t halfOfMaxBytesInPipeline = m_maxBundleSizeBytesInPipeline >> 1;
    const boost::posix_time::ptime timeoutExpiry(boost::posix_time::microsec_clock::universal_time() + timeoutDuration);
    boost::mutex::scoped_lock lock(m_mutex);
    //timed_wait Returns: false if the call is returning because the time specified by abs_time was reached, true otherwise. (false=>timeout)
    //wait while (queueIsFull AND hasNotTimedOutYet)
    while (
        ((m_mapEgressBundleUniqueIdToBundleSizeBytes.size() >= (halfOfMaxBundlesInPipeline * checkEgressPipeline))
        || ((m_egressBytesInPipeline + bundleSizeBytes) > (halfOfMaxBytesInPipeline * checkEgressPipeline)))
        && 
        ((m_mapStorageBundleUniqueIdToBundleSizeBytes.size() >= (halfOfMaxBundlesInPipeline * checkStoragePipeline))
        || ((m_storageBytesInPipeline + bundleSizeBytes) > (halfOfMaxBytesInPipeline * checkStoragePipeline)))
        &&
        m_conditionVariable.timed_wait(lock, timeoutExpiry)) {
    }//lock mutex (above) before checking condition

    //egress gets first priority, storage gets second priority
    if (checkEgressPipeline) {
        reservedEgressPipelineAvailability = (m_mapEgressBundleUniqueIdToBundleSizeBytes.size() < halfOfMaxBundlesInPipeline)
            && ((m_egressBytesInPipeline + bundleSizeBytes) <= halfOfMaxBytesInPipeline);
        if (reservedEgressPipelineAvailability) {
            m_mapEgressBundleUniqueIdToBundleSizeBytes.emplace(uniqueId, bundleSizeBytes);
            m_egressBytesInPipeline += bundleSizeBytes;
            return true;
        }
    }
    if (checkStoragePipeline) {
        reservedStoragePipelineAvailability = (m_mapStorageBundleUniqueIdToBundleSizeBytes.size() < halfOfMaxBundlesInPipeline)
            && ((m_storageBytesInPipeline + bundleSizeBytes) <= halfOfMaxBytesInPipeline);
        if (reservedStoragePipelineAvailability) {
            m_mapStorageBundleUniqueIdToBundleSizeBytes.emplace(uniqueId, bundleSizeBytes);
            m_storageBytesInPipeline += bundleSizeBytes;
            return true;
        }
    }
    return false;
}
bool Ingress::Impl::BundlePipelineAckingSet::WaitForStoragePipelineAvailabilityAndReserve(const boost::posix_time::time_duration& timeoutDuration,
    const uint64_t uniqueId, const uint64_t bundleSizeBytes)
{
    bool dontCare1, dontCare2;
    return WaitForPipelineAvailabilityAndReserve(false, true,
        timeoutDuration, uniqueId, bundleSizeBytes,
        dontCare1, dontCare2);
}
void Ingress::Impl::BundlePipelineAckingSet::NotifyAll() {
    m_conditionVariable.notify_all();
}
uint64_t Ingress::Impl::BundlePipelineAckingSet::GetNextHopNodeId() const {
    return m_nextHopNodeId;
}

Ingress::Impl::Impl() : 
    m_bundleCountStorage(0),
    m_bundleByteCountStorage(0),
    m_bundleCountEgress(0),
    m_bundleByteCountEgress(0),
    m_singleStorageBundlePipelineAckingSet(10, 10, UINT64_MAX, false), //initial don't cares for a deleted default constructor, set later
    m_eventsTooManyInStorageCutThroughQueue(0),
    m_eventsTooManyInEgressCutThroughQueue(0),
    m_eventsTooManyInAllCutThroughQueues(0),
    m_running(false),
    m_egressFullyInitialized(false),
    m_nextBundleUniqueIdAtomic(0),
    m_workerThreadStartupInProgress(false),
    m_telemThreadStartupInProgress(false) {}

Ingress::Ingress() :
    m_pimpl(boost::make_unique<Ingress::Impl>()),
    //references
    m_bundleCountStorage(m_pimpl->m_bundleCountStorage),
    m_bundleByteCountStorage(m_pimpl->m_bundleByteCountStorage),
    m_bundleCountEgress(m_pimpl->m_bundleCountEgress),
    m_bundleByteCountEgress(m_pimpl->m_bundleByteCountEgress) {}

Ingress::Impl::~Impl() {
    Stop();
}



Ingress::~Ingress() {
    Stop();
}

void Ingress::Stop() {
    m_pimpl->Stop();
}
void Ingress::Impl::Stop() {
    m_inductManager.Clear();


    m_running = false; //thread stopping criteria

    if (m_threadZmqTelemPtr) {
        try {
            m_threadZmqTelemPtr->join();
            m_threadZmqTelemPtr.reset(); //delete it
        }
        catch (boost::thread_resource_error& e) {
            LOG_ERROR(subprocess) << "unable to stop ingress threadZmqTelemPtr: " << e.what();
        }
    }
    if (m_threadZmqAckReaderPtr) {
        try {
            m_threadZmqAckReaderPtr->join();
            m_threadZmqAckReaderPtr.reset(); //delete it
        }
        catch (boost::thread_resource_error& e) {
            LOG_ERROR(subprocess) << "unable to stop ingress threadZmqAckReaderPtr: " << e.what();
        }
    }
    if (m_threadTcpclOpportunisticBundlesFromEgressReaderPtr) {
        try {
            m_threadTcpclOpportunisticBundlesFromEgressReaderPtr->join();
            m_threadTcpclOpportunisticBundlesFromEgressReaderPtr.reset(); //delete it
        }
        catch (boost::thread_resource_error& e) {
            LOG_ERROR(subprocess) << "unable to stop ingress threadTcpclOpportunisticBundlesFromEgressReaderPtr: " << e.what();
        }
    }


    LOG_DEBUG(subprocess) << "m_eventsTooManyInStorageCutThroughQueue: " << m_eventsTooManyInStorageCutThroughQueue;
    LOG_DEBUG(subprocess) << "m_eventsTooManyInEgressCutThroughQueue: " << m_eventsTooManyInEgressCutThroughQueue;
    LOG_DEBUG(subprocess) << "m_eventsTooManyInAllCutThroughQueues: " << m_eventsTooManyInAllCutThroughQueues;
}

bool Ingress::Init(const HdtnConfig& hdtnConfig, const HdtnDistributedConfig& hdtnDistributedConfig, zmq::context_t* hdtnOneProcessZmqInprocContextPtr) {
    return m_pimpl->Init(hdtnConfig, hdtnDistributedConfig, hdtnOneProcessZmqInprocContextPtr);
}
bool Ingress::Impl::Init(const HdtnConfig& hdtnConfig, const HdtnDistributedConfig& hdtnDistributedConfig, zmq::context_t * hdtnOneProcessZmqInprocContextPtr) {

    if (m_running) {
        LOG_ERROR(subprocess) << "Ingress::Init called while Ingress is already running";
        return false;
    }

    m_hdtnConfig = hdtnConfig;
    //according to ION.pdf v4.0.1 on page 100 it says:
    //  Remember that the format for this argument is ipn:element_number.0 and that
    //  the final 0 is required, as custodial/administration service is always service 0.
    //HDTN shall default m_myCustodialServiceId to 0 although it is changeable in the hdtn config json file
    M_HDTN_EID_CUSTODY.Set(m_hdtnConfig.m_myNodeId, m_hdtnConfig.m_myCustodialServiceId);

    M_HDTN_EID_ECHO.Set(m_hdtnConfig.m_myNodeId, m_hdtnConfig.m_myBpEchoServiceId);

    M_HDTN_EID_PING.Set(m_hdtnConfig.m_myNodeId, MY_PING_SERVICE_ID);

    M_HDTN_EID_TO_SCHEDULER_BUNDLES.Set(m_hdtnConfig.m_myNodeId, m_hdtnConfig.m_mySchedulerServiceId);

    M_MAX_INGRESS_BUNDLE_WAIT_ON_EGRESS_TIME_DURATION = boost::posix_time::milliseconds(m_hdtnConfig.m_maxIngressBundleWaitOnEgressMilliseconds);

    m_zmqCtxPtr = boost::make_unique<zmq::context_t>(); //needed at least by scheduler (and if one-process is not used)
    try {
        if (hdtnOneProcessZmqInprocContextPtr) {

            // socket for cut-through mode straight to egress
            //The io_threads argument specifies the size of the 0MQ thread pool to handle I/O operations.
            //If your application is using only the inproc transport for messaging you may set this to zero, otherwise set it to at least one.      
            m_zmqPushSock_boundIngressToConnectingEgressPtr = boost::make_unique<zmq::socket_t>(*hdtnOneProcessZmqInprocContextPtr, zmq::socket_type::pair);
            m_zmqPushSock_boundIngressToConnectingEgressPtr->bind(std::string("inproc://bound_ingress_to_connecting_egress"));
            // socket for sending bundles to storage
            m_zmqPushSock_boundIngressToConnectingStoragePtr = boost::make_unique<zmq::socket_t>(*hdtnOneProcessZmqInprocContextPtr, zmq::socket_type::pair);
            m_zmqPushSock_boundIngressToConnectingStoragePtr->bind(std::string("inproc://bound_ingress_to_connecting_storage"));
            // socket for receiving acks from storage
            m_zmqPullSock_connectingStorageToBoundIngressPtr = boost::make_unique<zmq::socket_t>(*hdtnOneProcessZmqInprocContextPtr, zmq::socket_type::pair);
            m_zmqPullSock_connectingStorageToBoundIngressPtr->bind(std::string("inproc://connecting_storage_to_bound_ingress"));
            // socket for receiving acks from egress
            m_zmqPullSock_connectingEgressToBoundIngressPtr = boost::make_unique<zmq::socket_t>(*hdtnOneProcessZmqInprocContextPtr, zmq::socket_type::pair);
            m_zmqPullSock_connectingEgressToBoundIngressPtr->bind(std::string("inproc://connecting_egress_to_bound_ingress"));
            // socket for receiving bundles from egress via tcpcl outduct opportunistic link (because tcpcl can be bidirectional)
            m_zmqPullSock_connectingEgressBundlesOnlyToBoundIngressPtr = boost::make_unique<zmq::socket_t>(*hdtnOneProcessZmqInprocContextPtr, zmq::socket_type::pair);
            m_zmqPullSock_connectingEgressBundlesOnlyToBoundIngressPtr->bind(std::string("inproc://connecting_egress_bundles_only_to_bound_ingress"));

            //from telemetry socket
            m_zmqRepSock_connectingTelemToFromBoundIngressPtr = boost::make_unique<zmq::socket_t>(*hdtnOneProcessZmqInprocContextPtr, zmq::socket_type::pair);
            m_zmqRepSock_connectingTelemToFromBoundIngressPtr->bind(std::string("inproc://connecting_telem_to_from_bound_ingress"));
        }
        else {
            // socket for cut-through mode straight to egress
            m_zmqPushSock_boundIngressToConnectingEgressPtr = boost::make_unique<zmq::socket_t>(*m_zmqCtxPtr, zmq::socket_type::push);
            const std::string bind_boundIngressToConnectingEgressPath(
                std::string("tcp://*:") + boost::lexical_cast<std::string>(hdtnDistributedConfig.m_zmqBoundIngressToConnectingEgressPortPath));
            m_zmqPushSock_boundIngressToConnectingEgressPtr->bind(bind_boundIngressToConnectingEgressPath);
            // socket for sending bundles to storage
            m_zmqPushSock_boundIngressToConnectingStoragePtr = boost::make_unique<zmq::socket_t>(*m_zmqCtxPtr, zmq::socket_type::push);
            const std::string bind_boundIngressToConnectingStoragePath(
                std::string("tcp://*:") + boost::lexical_cast<std::string>(hdtnDistributedConfig.m_zmqBoundIngressToConnectingStoragePortPath));
            m_zmqPushSock_boundIngressToConnectingStoragePtr->bind(bind_boundIngressToConnectingStoragePath);
            // socket for receiving acks from storage
            m_zmqPullSock_connectingStorageToBoundIngressPtr = boost::make_unique<zmq::socket_t>(*m_zmqCtxPtr, zmq::socket_type::pull);
            const std::string bind_connectingStorageToBoundIngressPath(
                std::string("tcp://*:") + boost::lexical_cast<std::string>(hdtnDistributedConfig.m_zmqConnectingStorageToBoundIngressPortPath));
            m_zmqPullSock_connectingStorageToBoundIngressPtr->bind(bind_connectingStorageToBoundIngressPath);
            // socket for receiving acks from egress
            m_zmqPullSock_connectingEgressToBoundIngressPtr = boost::make_unique<zmq::socket_t>(*m_zmqCtxPtr, zmq::socket_type::pull);
            const std::string bind_connectingEgressToBoundIngressPath(
                std::string("tcp://*:") + boost::lexical_cast<std::string>(hdtnDistributedConfig.m_zmqConnectingEgressToBoundIngressPortPath));
            m_zmqPullSock_connectingEgressToBoundIngressPtr->bind(bind_connectingEgressToBoundIngressPath);
            // socket for receiving bundles from egress via tcpcl outduct opportunistic link (because tcpcl can be bidirectional)
            m_zmqPullSock_connectingEgressBundlesOnlyToBoundIngressPtr = boost::make_unique<zmq::socket_t>(*m_zmqCtxPtr, zmq::socket_type::pull);
            const std::string bind_connectingEgressBundlesOnlyToBoundIngressPath(
                std::string("tcp://*:") + boost::lexical_cast<std::string>(hdtnDistributedConfig.m_zmqConnectingEgressBundlesOnlyToBoundIngressPortPath));
            m_zmqPullSock_connectingEgressBundlesOnlyToBoundIngressPtr->bind(bind_connectingEgressBundlesOnlyToBoundIngressPath);

            //from telemetry socket
            m_zmqRepSock_connectingTelemToFromBoundIngressPtr = boost::make_unique<zmq::socket_t>(*m_zmqCtxPtr, zmq::socket_type::rep);
            const std::string bind_connectingTelemToFromBoundIngressPath(
                std::string("tcp://*:") + boost::lexical_cast<std::string>(hdtnDistributedConfig.m_zmqConnectingTelemToFromBoundIngressPortPath));
            m_zmqRepSock_connectingTelemToFromBoundIngressPtr->bind(bind_connectingTelemToFromBoundIngressPath);
                
        }
    }
    catch (const zmq::error_t & ex) {
        LOG_ERROR(subprocess) << "cannot connect bind zmq socket: " << ex.what();
        return false;
    }

    //Caution: All options, with the exception of ZMQ_SUBSCRIBE, ZMQ_UNSUBSCRIBE and ZMQ_LINGER, only take effect for subsequent socket bind/connects.
    //The value of 0 specifies no linger period. Pending messages shall be discarded immediately when the socket is closed with zmq_close().
    m_zmqRepSock_connectingTelemToFromBoundIngressPtr->set(zmq::sockopt::linger, 0); //prevent hang when deleting the zmqCtxPtr
    m_zmqPushSock_boundIngressToConnectingEgressPtr->set(zmq::sockopt::linger, 0); //prevent hang when deleting the zmqCtxPtr
    m_zmqPushSock_boundIngressToConnectingStoragePtr->set(zmq::sockopt::linger, 0); //prevent hang when deleting the zmqCtxPtr

    //THIS PROBABLY DOESNT WORK SINCE IT HAPPENED AFTER BIND/CONNECT BUT NOT USED ANYWAY BECAUSE OF POLLITEMS
    //static const int timeout = 250;  // milliseconds
    //m_zmqPullSock_connectingStorageToBoundIngressPtr->set(zmq::sockopt::rcvtimeo, timeout);
    //m_zmqPullSock_connectingEgressToBoundIngressPtr->set(zmq::sockopt::rcvtimeo, timeout);
    //m_zmqPullSock_connectingEgressBundlesOnlyToBoundIngressPtr->set(zmq::sockopt::rcvtimeo, timeout);

    // socket for receiving events from scheduler
    m_zmqSubSock_boundSchedulerToConnectingIngressPtr = boost::make_unique<zmq::socket_t>(*m_zmqCtxPtr, zmq::socket_type::sub);
    const std::string connect_boundSchedulerPubSubPath(
        std::string("tcp://") +
        ((hdtnOneProcessZmqInprocContextPtr == NULL) ? hdtnDistributedConfig.m_zmqSchedulerAddress : std::string("localhost")) +
        std::string(":") +
        boost::lexical_cast<std::string>(m_hdtnConfig.m_zmqBoundSchedulerPubSubPortPath));
        
    try {
        m_zmqSubSock_boundSchedulerToConnectingIngressPtr->connect(connect_boundSchedulerPubSubPath);
        m_zmqSubSock_boundSchedulerToConnectingIngressPtr->set(zmq::sockopt::linger, 0); //prevent hang when deleting the zmqCtxPtr
        LOG_INFO(subprocess) << "Connected to scheduler at " << connect_boundSchedulerPubSubPath << " , subscribing...";
    }
    catch (const zmq::error_t & ex) {
        LOG_ERROR(subprocess) << "Cannot connect to scheduler socket at " << connect_boundSchedulerPubSubPath << " : " << ex.what();
        return false;
    }
    try {
        //Sends one-byte 0x1 message to scheduler XPub socket plus strlen of subscription
        //All release messages shall be prefixed by "aaaaaaaa" before the common header
        //Router unique subscription shall be "a" (gets all messages that start with "a") (e.g "aaa", "ab", etc.)
        //Ingress unique subscription shall be "aa"
        //Storage unique subscription shall be "aaa"
        m_zmqSubSock_boundSchedulerToConnectingIngressPtr->set(zmq::sockopt::subscribe, "aa");
        LOG_INFO(subprocess) << "Subscribed to all events from scheduler";
    }
    catch (const zmq::error_t& ex) {
        LOG_ERROR(subprocess) << "Cannot subscribe to all events from scheduler: " << ex.what();
        return false;
    }

    { //start worker thread
        m_running = true;
        boost::mutex::scoped_lock workerThreadStartupLock(m_workerThreadStartupMutex);
        m_workerThreadStartupInProgress = true;

        m_threadZmqAckReaderPtr = boost::make_unique<boost::thread>(
            boost::bind(&Ingress::Impl::ReadZmqAcksThreadFunc, this)); //create and start the worker thread

        while (m_workerThreadStartupInProgress) { //lock mutex (above) before checking condition
            //Returns: false if the call is returning because the time specified by abs_time was reached, true otherwise.
            if (!m_workerThreadStartupConditionVariable.timed_wait(workerThreadStartupLock, boost::posix_time::seconds(3))) {
                LOG_ERROR(subprocess) << "timed out waiting (for 3 seconds) for worker thread to start up";
                break;
            }
        }
        if (m_workerThreadStartupInProgress) {
            LOG_ERROR(subprocess) << "error: worker thread took too long to start up.. exiting";
            return false;
        }
        else {
            LOG_INFO(subprocess) << "worker thread started";
        }

        //Wait until egress up and running and get the first outduct capabilities telemetry.
        //The m_threadZmqAckReaderPtr will start remaining ingress initialization once egress telemetry received for the first time
    }

    { //start telem thread
        //m_running = true; //already true
        boost::mutex::scoped_lock telemThreadStartupLock(m_telemThreadStartupMutex);
        m_telemThreadStartupInProgress = true;

        m_threadZmqTelemPtr = boost::make_unique<boost::thread>(
            boost::bind(&Ingress::Impl::ZmqTelemThreadFunc, this)); //create and start the telem thread

        while (m_telemThreadStartupInProgress) { //lock mutex (above) before checking condition
            //Returns: false if the call is returning because the time specified by abs_time was reached, true otherwise.
            if (!m_telemThreadStartupConditionVariable.timed_wait(telemThreadStartupLock, boost::posix_time::seconds(3))) {
                LOG_ERROR(subprocess) << "timed out waiting (for 3 seconds) for telem thread to start up";
                break;
            }
        }
        if (m_telemThreadStartupInProgress) {
            LOG_ERROR(subprocess) << "error: telem thread took too long to start up.. exiting";
            return false;
        }
        else {
            LOG_INFO(subprocess) << "telem thread started";
        }
    }
    
    return true;
}

static void CustomCleanupZmqMessage(void *data, void *hint) {
    delete static_cast<zmq::message_t*>(hint);
}
static void CustomCleanupPaddedVecUint8(void *data, void *hint) {
    delete static_cast<padded_vector_uint8_t*>(hint);
}

static void CustomCleanupStdVecUint8(void *data, void *hint) {
    delete static_cast<std::vector<uint8_t>*>(hint);
}
static void CustomCleanupStdString(void* data, void* hint) {
    delete static_cast<std::string*>(hint);
}

static void CustomCleanupToEgressHdr(void *data, void *hint) {
    delete static_cast<hdtn::ToEgressHdr*>(hint);
}

static void CustomCleanupToStorageHdr(void *data, void *hint) {
    delete static_cast<hdtn::ToStorageHdr*>(hint);
}

void Ingress::Impl::ReadZmqAcksThreadFunc() {

    ThreadNamer::SetThisThreadName("ingressZmqAckReader");

    static constexpr unsigned int NUM_SOCKETS = 3;

    zmq::pollitem_t items[NUM_SOCKETS] = {
        {m_zmqPullSock_connectingEgressToBoundIngressPtr->handle(), 0, ZMQ_POLLIN, 0},
        {m_zmqPullSock_connectingStorageToBoundIngressPtr->handle(), 0, ZMQ_POLLIN, 0},
        {m_zmqSubSock_boundSchedulerToConnectingIngressPtr->handle(), 0, ZMQ_POLLIN, 0}
    };
    std::size_t totalAcksFromEgress = 0;
    std::size_t totalAcksFromStorage = 0;

    static const long DEFAULT_BIG_TIMEOUT_POLL = 250;

    //notify Init function that worker thread startup is complete
    m_workerThreadStartupMutex.lock();
    m_workerThreadStartupInProgress = false;
    m_workerThreadStartupMutex.unlock();
    m_workerThreadStartupConditionVariable.notify_one();

    while (m_running) { //keep thread alive if running
        int rc = 0;
        try {
            rc = zmq::poll(&items[0], NUM_SOCKETS, DEFAULT_BIG_TIMEOUT_POLL);
        }
        catch (zmq::error_t & e) {
            LOG_ERROR(subprocess) << "caught zmq::error_t in Ingress::ReadZmqAcksThreadFunc: " << e.what();
            continue;
        }
        if (rc > 0) {
            if (items[0].revents & ZMQ_POLLIN) { //ack from egress
                EgressAckHdr receivedEgressAckHdr;
                const zmq::recv_buffer_result_t res = m_zmqPullSock_connectingEgressToBoundIngressPtr->recv(zmq::mutable_buffer(&receivedEgressAckHdr, sizeof(hdtn::EgressAckHdr)), zmq::recv_flags::dontwait);
                if (!res) {
                    LOG_ERROR(subprocess) << "cannot read EgressAckHdr";
                }
                else if ((res->truncated()) || (res->size != sizeof(hdtn::EgressAckHdr))) {
                    LOG_ERROR(subprocess) << "EgressAckHdr message mismatch: untruncated = " << res->untruncated_size
                        << " truncated = " << res->size << " expected = " << sizeof(hdtn::EgressAckHdr);
                }
                else if (receivedEgressAckHdr.base.type == HDTN_MSGTYPE_EGRESS_ACK_TO_INGRESS) {
                    ingress_shared_lock_t lockShared(m_sharedMutexFinalDestsToOutductArrayIndexMaps);
                    BundlePipelineAckingSet& bundlePipelineAckingSetObj = *(m_vectorBundlePipelineAckingSet[receivedEgressAckHdr.outductIndex]);
                    if (receivedEgressAckHdr.error) {
                        //trigger a link down event in ingress more quickly than waiting for scheduler.
                        //egress shall send the failed bundle to storage.
                        if (bundlePipelineAckingSetObj.m_linkIsUp) {
                            bundlePipelineAckingSetObj.m_linkIsUp = false; //no mutex needed as this flag is only set from ReadZmqAcksThreadFunc
                            LOG_INFO(subprocess) << "Got a link down notification from egress for outductIndex " 
                                << receivedEgressAckHdr.outductIndex;
                        }
                    }
                    if (bundlePipelineAckingSetObj.CompareAndPop_ThreadSafe(receivedEgressAckHdr.custodyId, true)) { //true => isEgress
                        bundlePipelineAckingSetObj.NotifyAll();
                        ++totalAcksFromEgress;
                    }
                    else {
                        LOG_ERROR(subprocess) << "didn't receive expected egress ack";
                    }

                }
                else if (receivedEgressAckHdr.base.type == HDTN_MSGTYPE_ALL_OUTDUCT_CAPABILITIES_TELEMETRY) {
                    AllOutductCapabilitiesTelemetry_t aoct;
                    
                    zmq::message_t zmqMessageOutductTelem;
                    //message guaranteed to be there due to the zmq::send_flags::sndmore
                    if (!m_zmqPullSock_connectingEgressToBoundIngressPtr->recv(zmqMessageOutductTelem, zmq::recv_flags::none)) {
                        LOG_ERROR(subprocess) << "error receiving AllOutductCapabilitiesTelemetry";
                    }
                    else if (!aoct.SetValuesFromJsonCharArray((char*)zmqMessageOutductTelem.data(), zmqMessageOutductTelem.size())) {
                        LOG_ERROR(subprocess) << "error deserializing AllOutductCapabilitiesTelemetry";
                    }
                    else {
                        
                        if (aoct.outductCapabilityTelemetryList.empty()) {
                            LOG_ERROR(subprocess) << "received outductCapabilityTelemetryList is empty!";
                        }
                        else {
                            ingress_exclusive_lock_t lockExclusive(m_sharedMutexFinalDestsToOutductArrayIndexMaps);
                            const bool isInitial = m_vectorBundlePipelineAckingSet.empty();
                            if (isInitial) {
                                LOG_INFO(subprocess) << "Ingress received initial " << aoct.outductCapabilityTelemetryList.size() << " outduct telemetries from egress";
                                m_vectorBundlePipelineAckingSet.reserve(aoct.outductCapabilityTelemetryList.size());
                            }


                            if ((!isInitial) && (m_vectorBundlePipelineAckingSet.size() != aoct.outductCapabilityTelemetryList.size())) {
                                LOG_ERROR(subprocess) << "outduct capability update but m_vectorEgressToIngressAckingSet.size() != aoct.outductCapabilityTelemetryList.size()";
                            }
                            else {
                                m_mapNextHopNodeIdToOutductArrayIndex.clear();
                                m_mapFinalDestNodeIdToOutductArrayIndex.clear();
                                m_mapFinalDestEidToOutductArrayIndex.clear();

                                bool foundError = false;
                                uint64_t expectedIndex = 0;
                                for (std::list<OutductCapabilityTelemetry_t>::const_iterator itAoct = aoct.outductCapabilityTelemetryList.cbegin();
                                    itAoct != aoct.outductCapabilityTelemetryList.cend();
                                    ++itAoct, ++expectedIndex)
                                {
                                    const OutductCapabilityTelemetry_t& oct = *itAoct;
                                    if (oct.outductArrayIndex != expectedIndex) {
                                        foundError = true;
                                        LOG_ERROR(subprocess) << "outduct capability update but out of order outductArrayIndex received";
                                        break;
                                    }

                                    if (isInitial) {
                                        m_vectorBundlePipelineAckingSet.emplace_back(boost::make_unique<BundlePipelineAckingSet>(oct.maxBundlesInPipeline, oct.maxBundleSizeBytesInPipeline, oct.nextHopNodeId, false));
                                    }
                                    else {
                                        LOG_INFO(subprocess) << "Received updated outduct telemetries from egress";
                                        BundlePipelineAckingSet& ackingSet = *(m_vectorBundlePipelineAckingSet[oct.outductArrayIndex]);
                                        ackingSet.Update(oct.maxBundlesInPipeline,
                                            oct.maxBundleSizeBytesInPipeline, oct.nextHopNodeId, ackingSet.m_linkIsUp);
                                    }
                                    m_mapNextHopNodeIdToOutductArrayIndex[oct.nextHopNodeId] = oct.outductArrayIndex;
                                    for (std::list<cbhe_eid_t>::const_iterator it = oct.finalDestinationEidList.cbegin(); it != oct.finalDestinationEidList.cend(); ++it) {
                                        const cbhe_eid_t& eid = *it;
                                        m_mapFinalDestEidToOutductArrayIndex[eid] = oct.outductArrayIndex;
                                    }
                                    for (std::list<uint64_t>::const_iterator it = oct.finalDestinationNodeIdList.cbegin(); it != oct.finalDestinationNodeIdList.cend(); ++it) {
                                        const uint64_t nodeId = *it;
                                        m_mapFinalDestNodeIdToOutductArrayIndex[nodeId] = oct.outductArrayIndex;
                                    }
                                }

                                if ((!foundError) && (!m_egressFullyInitialized)) { //first time this outduct capabilities telemetry received, start remaining ingress threads
                                    m_singleStorageBundlePipelineAckingSet.Update(STORAGE_MAX_BUNDLES_IN_PIPELINE * 2, //*2 because egress map ignored and the acking set divides by 2
                                        m_hdtnConfig.m_maxBundleSizeBytes * 2, UINT64_MAX, false);

                                    m_threadTcpclOpportunisticBundlesFromEgressReaderPtr = boost::make_unique<boost::thread>(
                                        boost::bind(&Ingress::Impl::ReadTcpclOpportunisticBundlesFromEgressThreadFunc, this)); //create and start the worker thread

                                    m_inductManager.LoadInductsFromConfig(boost::bind(&Ingress::Impl::WholeBundleReadyCallback, this, boost::placeholders::_1), m_hdtnConfig.m_inductsConfig,
                                        m_hdtnConfig.m_myNodeId, m_hdtnConfig.m_maxLtpReceiveUdpPacketSizeBytes, m_hdtnConfig.m_maxBundleSizeBytes,
                                        boost::bind(&Ingress::Impl::OnNewOpportunisticLinkCallback, this, boost::placeholders::_1, boost::placeholders::_2, boost::placeholders::_3),
                                        boost::bind(&Ingress::Impl::OnDeletedOpportunisticLinkCallback, this, boost::placeholders::_1, boost::placeholders::_2, boost::placeholders::_3));

                                    m_egressFullyInitialized = true;

                                    LOG_INFO(subprocess) << "Now running and fully initialized and connected to egress";
                                }
                            }
                        }
                    }
                }
                else {
                    LOG_ERROR(subprocess) << "message ack unknown";
                }
            }
            if (items[1].revents & ZMQ_POLLIN) { //ack from storage
                StorageAckHdr receivedStorageAck;
                const zmq::recv_buffer_result_t res = m_zmqPullSock_connectingStorageToBoundIngressPtr->recv(zmq::mutable_buffer(&receivedStorageAck, sizeof(hdtn::StorageAckHdr)), zmq::recv_flags::dontwait);
                if (!res) {
                    LOG_ERROR(subprocess) << "BpIngressSyscall::ReadZmqAcksThreadFunc: cannot read storage BlockHdr ack";

                }
                else if ((res->truncated()) || (res->size != sizeof(hdtn::StorageAckHdr))) {
                    LOG_ERROR(subprocess) << "StorageAckHdr message mismatch: untruncated = " << res->untruncated_size
                        << " truncated = " << res->size << " expected = " << sizeof(hdtn::StorageAckHdr);
                }
                else if (receivedStorageAck.base.type != HDTN_MSGTYPE_STORAGE_ACK_TO_INGRESS) {
                    LOG_ERROR(subprocess) << "message ack not HDTN_MSGTYPE_STORAGE_ACK_TO_INGRESS";
                }
                else {
                    ingress_shared_lock_t lockShared(m_sharedMutexFinalDestsToOutductArrayIndexMaps);
                    BundlePipelineAckingSet& bundlePipelineAckingSetObj = (receivedStorageAck.outductIndex == UINT64_MAX) ?
                        m_singleStorageBundlePipelineAckingSet : (*(m_vectorBundlePipelineAckingSet[receivedStorageAck.outductIndex]));
                    if (receivedStorageAck.error && (receivedStorageAck.outductIndex != UINT64_MAX)) {
                        //trigger a link down event in ingress more quickly than waiting for scheduler.
                        //storage shall write the failed bundle to storage.
                        if (bundlePipelineAckingSetObj.m_linkIsUp) {
                            bundlePipelineAckingSetObj.m_linkIsUp = false; //no mutex needed as this flag is only set from ReadZmqAcksThreadFunc
                            LOG_INFO(subprocess) << "Got a link down notification from storage for outductIndex "
                                << receivedStorageAck.outductIndex;
                        }
                    }
                    if (bundlePipelineAckingSetObj.CompareAndPop_ThreadSafe(receivedStorageAck.ingressUniqueId, false)) { //false => is Storage
                        bundlePipelineAckingSetObj.NotifyAll();
                        ++totalAcksFromStorage;
                    }
                    else {
                        LOG_ERROR(subprocess) << "storage ack with ingressUniqueId " << receivedStorageAck.ingressUniqueId << " not found!";
                    }
                }
            }
            if (items[2].revents & ZMQ_POLLIN) { //events from Scheduler
                SchedulerEventHandler();
            }
            
        }
    }
    LOG_INFO(subprocess) << "totalAcksFromEgress: " << totalAcksFromEgress;
    LOG_INFO(subprocess) << "totalAcksFromStorage: " << totalAcksFromStorage;
    LOG_INFO(subprocess) << "m_bundleCountStorage: " << m_bundleCountStorage;
    LOG_INFO(subprocess) << "m_bundleByteCountStorage: " << m_bundleByteCountStorage;
    LOG_INFO(subprocess) << "m_bundleCountEgress: " << m_bundleCountEgress;
    LOG_INFO(subprocess) << "m_bundleByteCountEgress: " << m_bundleByteCountEgress;
    LOG_INFO(subprocess) << "bundleCount: " << (m_bundleCountStorage + m_bundleCountEgress);
    LOG_DEBUG(subprocess) << "ReadZmqAcksThreadFunc thread exiting";
}

//keep telemetry in its own thread to prevent m_inductManager.PopulateAllInductTelemetry's mutex from
//blocking the egress and storage acks and causing a deadlock, which is noticeable when there is a slow
//outduct along with a TCP/STCP induct connection getting deleted
void Ingress::Impl::ZmqTelemThreadFunc() {
    ThreadNamer::SetThisThreadName("ingressZmqTelem");

    static constexpr unsigned int NUM_SOCKETS = 1;

    zmq::pollitem_t items[NUM_SOCKETS] = {
        {m_zmqRepSock_connectingTelemToFromBoundIngressPtr->handle(), 0, ZMQ_POLLIN, 0}
    };

    static const long DEFAULT_BIG_TIMEOUT_POLL = 250;

    //notify Init function that worker thread startup is complete
    m_telemThreadStartupMutex.lock();
    m_telemThreadStartupInProgress = false;
    m_telemThreadStartupMutex.unlock();
    m_telemThreadStartupConditionVariable.notify_one();

    while (m_running) { //keep thread alive if running
        int rc = 0;
        try {
            rc = zmq::poll(&items[0], NUM_SOCKETS, DEFAULT_BIG_TIMEOUT_POLL);
        }
        catch (zmq::error_t& e) {
            LOG_ERROR(subprocess) << "caught zmq::error_t in Ingress::ReadZmqAcksThreadFunc: " << e.what();
            continue;
        }
        if (rc > 0) {
            if (items[0].revents & ZMQ_POLLIN) { //telem requests data
                uint8_t telemMsgByte;
                const zmq::recv_buffer_result_t res = m_zmqRepSock_connectingTelemToFromBoundIngressPtr->recv(zmq::mutable_buffer(&telemMsgByte, sizeof(telemMsgByte)), zmq::recv_flags::dontwait);
                if (!res) {
                    LOG_ERROR(subprocess) << "ReadZmqAcksThreadFunc: cannot read telemMsgByte";
                }
                else if ((res->truncated()) || (res->size != sizeof(telemMsgByte))) {
                    LOG_ERROR(subprocess) << "telemMsgByte message mismatch: untruncated = " << res->untruncated_size
                        << " truncated = " << res->size << " expected = " << sizeof(telemMsgByte);
                }
                else {
                    const bool hasApiCalls = telemMsgByte > TELEM_REQ_MSG;
                    if (hasApiCalls) {
                        zmq::message_t apiMsg;
                        do {
                            if (!m_zmqRepSock_connectingTelemToFromBoundIngressPtr->recv(apiMsg, zmq::recv_flags::dontwait)) {
                                LOG_ERROR(subprocess) << "error receiving api message";
                                continue;
                            }
                            std::string apiCall = ApiCommand_t::GetApiCallFromJson(apiMsg.to_string());
                            LOG_INFO(subprocess) << "got api call " << apiCall;
                            if (apiCall == "ping") {
                                PingApiCommand_t pingCmd;
                                if (!pingCmd.SetValuesFromJson(apiMsg.to_string())) {
                                    continue;
                                }
                                SendPing(pingCmd.m_nodeId, pingCmd.m_pingServiceNumber, pingCmd.m_bpVersion);
                            }
                        } while (apiMsg.more());
                    }
                    AllInductTelemetry_t allInductTelem;
                    m_inductManager.PopulateAllInductTelemetry(allInductTelem); //sets timestamp
                    m_ingressToEgressZmqSocketMutex.lock();
                    allInductTelem.m_bundleCountEgress = m_bundleCountEgress;
                    allInductTelem.m_bundleByteCountEgress = m_bundleByteCountEgress;
                    m_ingressToEgressZmqSocketMutex.unlock();
                    m_ingressToStorageZmqSocketMutex.lock();
                    allInductTelem.m_bundleCountStorage = m_bundleCountStorage;
                    allInductTelem.m_bundleByteCountStorage = m_bundleByteCountStorage;
                    m_ingressToStorageZmqSocketMutex.unlock();

                    std::string* allInductTelemJsonStringPtr = new std::string(allInductTelem.ToJson());
                    std::string& strRef = *allInductTelemJsonStringPtr;
                    zmq::message_t zmqJsonMessage(&strRef[0], allInductTelemJsonStringPtr->size(), CustomCleanupStdString, allInductTelemJsonStringPtr);

                    if (!m_zmqRepSock_connectingTelemToFromBoundIngressPtr->send(std::move(zmqJsonMessage), zmq::send_flags::dontwait)) {
                        LOG_ERROR(subprocess) << "can't send json telemetry to telem";
                    }
                }
            }

        }

    }
    LOG_DEBUG(subprocess) << "ZmqTelemThreadFunc thread exiting";
}

void Ingress::Impl::ReadTcpclOpportunisticBundlesFromEgressThreadFunc() {
    ThreadNamer::SetThisThreadName("ingressTcpclOpportunisticReader");
    static constexpr unsigned int NUM_SOCKETS = 1;
    zmq::pollitem_t items[NUM_SOCKETS] = {
        {m_zmqPullSock_connectingEgressBundlesOnlyToBoundIngressPtr->handle(), 0, ZMQ_POLLIN, 0}
    };
    uint8_t messageFlags;
    const zmq::mutable_buffer messageFlagsBuffer(&messageFlags, sizeof(messageFlags));
    std::size_t totalOpportunisticBundlesFromEgress = 0;
    static const long DEFAULT_BIG_TIMEOUT_POLL = 250;
    while (m_running) { //keep thread alive if running
        int rc = 0;
        try {
            rc = zmq::poll(&items[0], NUM_SOCKETS, DEFAULT_BIG_TIMEOUT_POLL);
        }
        catch (zmq::error_t & e) {
            LOG_ERROR(subprocess) << "caught zmq::error_t in Ingress::ReadTcpclOpportunisticBundlesFromEgressThreadFunc: " << e.what();
            continue;
        }
        if ((rc > 0) && (items[0].revents & ZMQ_POLLIN)) {
            const zmq::recv_buffer_result_t res = m_zmqPullSock_connectingEgressBundlesOnlyToBoundIngressPtr->recv(messageFlagsBuffer, zmq::recv_flags::none);
            if (!res) {
                LOG_ERROR(subprocess) << "ReadTcpclOpportunisticBundlesFromEgressThreadFunc: messageFlags not received";
                continue;
            }
            else if ((res->truncated()) || (res->size != sizeof(messageFlags))) {
                LOG_ERROR(subprocess) << "ReadTcpclOpportunisticBundlesFromEgressThreadFunc: messageFlags message mismatch: untruncated = " << res->untruncated_size
                    << " truncated = " << res->size << " expected = " << sizeof(messageFlags);
                continue;
            }
            
            
            static padded_vector_uint8_t unusedPaddedVec;
            std::unique_ptr<zmq::message_t> zmqPotentiallyPaddedMessage = boost::make_unique<zmq::message_t>();
            //no header, just a bundle as a zmq message
            if (!m_zmqPullSock_connectingEgressBundlesOnlyToBoundIngressPtr->recv(*zmqPotentiallyPaddedMessage, zmq::recv_flags::none)) {
                LOG_ERROR(subprocess) << "ReadTcpclOpportunisticBundlesFromEgressThreadFunc: cannot receive zmq";
            }
            else {
                if (messageFlags) { //1 => from egress and needs processing (is padded from the convergence layer)
                    uint8_t * paddedDataBegin = (uint8_t *)zmqPotentiallyPaddedMessage->data();
                    uint8_t * bundleDataBegin = paddedDataBegin + PaddedMallocator<uint8_t>::PADDING_ELEMENTS_BEFORE;

                    std::size_t bundleCurrentSize = zmqPotentiallyPaddedMessage->size() - PaddedMallocator<uint8_t>::TOTAL_PADDING_ELEMENTS;
                    ProcessPaddedData(bundleDataBegin, bundleCurrentSize, zmqPotentiallyPaddedMessage, unusedPaddedVec, true, true);
                    ++totalOpportunisticBundlesFromEgress;
                }
                else { //0 => from storage and needs no processing (is not padded)
                    ProcessPaddedData((uint8_t *)zmqPotentiallyPaddedMessage->data(), zmqPotentiallyPaddedMessage->size(), zmqPotentiallyPaddedMessage, unusedPaddedVec, true, false);
                }
            }
        }
    }
    LOG_INFO(subprocess) << "totalOpportunisticBundlesFromEgress: " << totalOpportunisticBundlesFromEgress;
}

void Ingress::Impl::SchedulerEventHandler() {
    hdtn::IreleaseChangeHdr releaseChangeHdr;
    const zmq::recv_buffer_result_t res = m_zmqSubSock_boundSchedulerToConnectingIngressPtr->recv(zmq::mutable_buffer(&releaseChangeHdr, sizeof(releaseChangeHdr)), zmq::recv_flags::none);
    if (!res) {
        LOG_ERROR(subprocess) << "unable to receive IreleaseChangeHdr message";
    }
    else if ((res->truncated()) || (res->size != sizeof(releaseChangeHdr))) {
        LOG_ERROR(subprocess) << "message mismatch with IreleaseChangeHdr: untruncated = " << res->untruncated_size
            << " truncated = " << res->size << " expected = " << sizeof(releaseChangeHdr);
    }
    else if (releaseChangeHdr.base.type == HDTN_MSGTYPE_ILINKUP) {
        ingress_shared_lock_t lockShared(m_sharedMutexFinalDestsToOutductArrayIndexMaps);
        if (releaseChangeHdr.outductArrayIndex < m_vectorBundlePipelineAckingSet.size()) {
            BundlePipelineAckingSet& bundlePipelineAckingSetObj = *(m_vectorBundlePipelineAckingSet[releaseChangeHdr.outductArrayIndex]);
            if (!bundlePipelineAckingSetObj.m_linkIsUp) {
                bundlePipelineAckingSetObj.m_linkIsUp = true; //no mutex needed as this flag is only set from ReadZmqAcksThreadFunc
                LOG_INFO(subprocess) << "Ingress sending bundles to egress for nextHopNodeId: " << releaseChangeHdr.nextHopNodeId
                    << " outductArrayIndex=" << releaseChangeHdr.outductArrayIndex;
            }
        }
        else {
            LOG_ERROR(subprocess) << "link up message received with out of bounds outductArrayIndex " << releaseChangeHdr.outductArrayIndex;
        }
    }
    else if (releaseChangeHdr.base.type == HDTN_MSGTYPE_ILINKDOWN) {
        ingress_shared_lock_t lockShared(m_sharedMutexFinalDestsToOutductArrayIndexMaps);
        if (releaseChangeHdr.outductArrayIndex < m_vectorBundlePipelineAckingSet.size()) {
            BundlePipelineAckingSet& bundlePipelineAckingSetObj = *(m_vectorBundlePipelineAckingSet[releaseChangeHdr.outductArrayIndex]);
            if (bundlePipelineAckingSetObj.m_linkIsUp) {
                bundlePipelineAckingSetObj.m_linkIsUp = false; //no mutex needed as this flag is only set from ReadZmqAcksThreadFunc
                LOG_INFO(subprocess) << "Sending bundles to storage for nextHopNodeId: " << releaseChangeHdr.nextHopNodeId
                    << " since outductArrayIndex=" << releaseChangeHdr.outductArrayIndex << " is down";
            }
        }
        else {
            LOG_ERROR(subprocess) << "link down message received with out of bounds outductArrayIndex " << releaseChangeHdr.outductArrayIndex;
        }
    }
    else if (releaseChangeHdr.base.type == HDTN_MSGTYPE_BUNDLES_FROM_SCHEDULER) {
        std::unique_ptr<zmq::message_t> zmqMessageBundleFromSchedulerPtr = boost::make_unique<zmq::message_t>();
        //message guaranteed to be there due to the zmq::send_flags::sndmore
        if (!m_zmqSubSock_boundSchedulerToConnectingIngressPtr->recv(*zmqMessageBundleFromSchedulerPtr, zmq::recv_flags::none)) {
            LOG_ERROR(subprocess) << "error receiving zmqMessageBundleToScheduler";
        }
        else {
            static padded_vector_uint8_t unusedPaddedVecMessage;
            ProcessPaddedData((uint8_t*)zmqMessageBundleFromSchedulerPtr->data(), zmqMessageBundleFromSchedulerPtr->size(),
                zmqMessageBundleFromSchedulerPtr, unusedPaddedVecMessage, true, false); //last param => does not need processing because it came from scheduler
        }
    }
    else {
        LOG_ERROR(subprocess) << "unknown IreleaseChangeHdr message type " << releaseChangeHdr.base.type;
    }
}

bool Ingress::Impl::ProcessPaddedData(uint8_t * bundleDataBegin, std::size_t bundleCurrentSize,
    std::unique_ptr<zmq::message_t> & zmqPaddedMessageUnderlyingDataUniquePtr,
    padded_vector_uint8_t & paddedVecMessageUnderlyingData, const bool usingZmqData, const bool needsProcessing)
{
    std::unique_ptr<zmq::message_t> zmqMessageToSendUniquePtr; //create on heap as zmq default constructor costly
    if (bundleCurrentSize > m_hdtnConfig.m_maxBundleSizeBytes) { //should never reach here as this is handled by induct
        LOG_ERROR(subprocess) << "Process: received bundle size ("
            << bundleCurrentSize << " bytes) exceeds max bundle size limit of "
            << m_hdtnConfig.m_maxBundleSizeBytes << " bytes";
        return false;
    }
    cbhe_eid_t finalDestEid;
    bool requestsCustody = false;
    bool isAdminRecordForHdtnStorage = false;
    bool isBundleForHdtnScheduler = false;
    const uint8_t firstByte = bundleDataBegin[0];
    const bool isBpVersion6 = (firstByte == 6);
    const bool isBpVersion7 = (firstByte == ((4U << 5) | 31U));  //CBOR major type 4, additional information 31 (Indefinite-Length Array)
    if (isBpVersion6) {
        BundleViewV6 bv;
        if (!bv.LoadBundle(bundleDataBegin, bundleCurrentSize)) {
            LOG_ERROR(subprocess) << "malformed bundle";
            return false;
        }
        Bpv6CbhePrimaryBlock & primary = bv.m_primaryBlockView.header;
        finalDestEid = primary.m_destinationEid;
        if (needsProcessing) {
            static const BPV6_BUNDLEFLAG requiredPrimaryFlagsForCustody = BPV6_BUNDLEFLAG::SINGLETON | BPV6_BUNDLEFLAG::CUSTODY_REQUESTED;
            requestsCustody = ((primary.m_bundleProcessingControlFlags & requiredPrimaryFlagsForCustody) == requiredPrimaryFlagsForCustody);
            //admin records pertaining to this hdtn node must go to storage.. they signal a deletion from disk
            static const BPV6_BUNDLEFLAG requiredPrimaryFlagsForAdminRecord = BPV6_BUNDLEFLAG::SINGLETON | BPV6_BUNDLEFLAG::ADMINRECORD;
            isAdminRecordForHdtnStorage = (((primary.m_bundleProcessingControlFlags & requiredPrimaryFlagsForAdminRecord) == requiredPrimaryFlagsForAdminRecord) && (finalDestEid == M_HDTN_EID_CUSTODY));
            isBundleForHdtnScheduler = (finalDestEid == M_HDTN_EID_TO_SCHEDULER_BUNDLES);
            static const BPV6_BUNDLEFLAG requiredPrimaryFlagsForEcho = BPV6_BUNDLEFLAG::NO_FLAGS_SET;
            //BPV6_BUNDLEFLAG::SINGLETON | BPV6_BUNDLEFLAG::NOFRAGMENT;
            const bool isEcho = (((primary.m_bundleProcessingControlFlags & requiredPrimaryFlagsForEcho) == requiredPrimaryFlagsForEcho) && (finalDestEid == M_HDTN_EID_ECHO));
            if (isEcho) {
                primary.m_destinationEid = primary.m_sourceNodeId;
                finalDestEid = primary.m_destinationEid;
                LOG_INFO(subprocess) << "Sending Ping for destination " << primary.m_destinationEid;
                primary.m_sourceNodeId = M_HDTN_EID_ECHO;
                bv.m_primaryBlockView.SetManuallyModified();
                if (!bv.Render(bundleCurrentSize + 500)) {
                    LOG_ERROR(subprocess) << "cannot render bpv6 echo bundle";
                    return false;
                }
                std::vector<uint8_t> * rxBufRawPointer = new std::vector<uint8_t>(std::move(bv.m_frontBuffer));
                zmqMessageToSendUniquePtr = boost::make_unique<zmq::message_t>(std::move(zmq::message_t(rxBufRawPointer->data(), rxBufRawPointer->size(), CustomCleanupStdVecUint8, rxBufRawPointer)));
                bundleCurrentSize = zmqMessageToSendUniquePtr->size();
            }
            else if (finalDestEid == M_HDTN_EID_PING) {
                std::vector<BundleViewV6::Bpv6CanonicalBlockView*> blocks;
                bv.GetCanonicalBlocksByType(BPV6_BLOCK_TYPE_CODE::PAYLOAD, blocks);
                if (blocks.size() != 1) {
                    LOG_ERROR(subprocess) << "Received a ping response bundle with no payload block";
                    return true;
                }
                Bpv6CanonicalBlock& payloadBlock = *(blocks[0]->headerPtr);
                ProcessReceivedPingPayload(payloadBlock.m_blockTypeSpecificDataPtr, payloadBlock.m_blockTypeSpecificDataLength, 6);
                return true;
            }
        }
        if (!zmqMessageToSendUniquePtr) { //no modifications
            if (usingZmqData) {
                zmq::message_t * rxBufRawPointer = new zmq::message_t(std::move(*zmqPaddedMessageUnderlyingDataUniquePtr));
                zmqMessageToSendUniquePtr = boost::make_unique<zmq::message_t>(bundleDataBegin, bundleCurrentSize, CustomCleanupZmqMessage, rxBufRawPointer);
            }
            else {
                padded_vector_uint8_t * rxBufRawPointer = new padded_vector_uint8_t(std::move(paddedVecMessageUnderlyingData));
                zmqMessageToSendUniquePtr = boost::make_unique<zmq::message_t>(bundleDataBegin, bundleCurrentSize, CustomCleanupPaddedVecUint8, rxBufRawPointer);
            }
        }
    }
    else if (isBpVersion7) {
        BundleViewV7 bv;
        const bool skipCrcVerifyInCanonicalBlocks = !needsProcessing;
        if (!bv.LoadBundle(bundleDataBegin, bundleCurrentSize, skipCrcVerifyInCanonicalBlocks)) { //todo true => skip canonical block crc checks to increase speed
            LOG_ERROR(subprocess) << "Process: malformed version 7 bundle received";
            return false;
        }
        Bpv7CbhePrimaryBlock & primary = bv.m_primaryBlockView.header;
        finalDestEid = primary.m_destinationEid;
        requestsCustody = false; //custody unsupported at this time
        if (needsProcessing) {
            if (finalDestEid == M_HDTN_EID_PING) {
                //get payload block
                std::vector<BundleViewV7::Bpv7CanonicalBlockView*> blocks;
                bv.GetCanonicalBlocksByType(BPV7_BLOCK_TYPE_CODE::PAYLOAD, blocks);
                if (blocks.size() != 1) {
                    LOG_ERROR(subprocess) << "Received a ping response bundle with no payload block";
                    return true;
                }
                Bpv7CanonicalBlock& payloadBlock = *(blocks[0]->headerPtr);
                ProcessReceivedPingPayload(payloadBlock.m_dataPtr, payloadBlock.m_dataLength, 7);
                return true;
            }
            //admin records pertaining to this hdtn node must go to storage.. they signal a deletion from disk
            static constexpr BPV7_BUNDLEFLAG requiredPrimaryFlagsForAdminRecord = BPV7_BUNDLEFLAG::ADMINRECORD;
            isAdminRecordForHdtnStorage = (((primary.m_bundleProcessingControlFlags & requiredPrimaryFlagsForAdminRecord) == requiredPrimaryFlagsForAdminRecord) && (finalDestEid == M_HDTN_EID_CUSTODY));
            isBundleForHdtnScheduler = (finalDestEid == M_HDTN_EID_TO_SCHEDULER_BUNDLES);
            static constexpr BPV7_BUNDLEFLAG requiredPrimaryFlagsForEcho = BPV7_BUNDLEFLAG::NO_FLAGS_SET;
            const bool isEcho = (((primary.m_bundleProcessingControlFlags & requiredPrimaryFlagsForEcho) == requiredPrimaryFlagsForEcho) && (finalDestEid == M_HDTN_EID_ECHO));
            if (!isAdminRecordForHdtnStorage) {
                //get previous node
                std::vector<BundleViewV7::Bpv7CanonicalBlockView*> blocks;
                bv.GetCanonicalBlocksByType(BPV7_BLOCK_TYPE_CODE::PREVIOUS_NODE, blocks);
                if (blocks.size() > 1) {
                    LOG_ERROR(subprocess) << "Process: version 7 bundle received has multiple previous node blocks";
                    return false;
                }
                else if (blocks.size() == 1) { //update existing
                    if (Bpv7PreviousNodeCanonicalBlock* previousNodeBlockPtr = dynamic_cast<Bpv7PreviousNodeCanonicalBlock*>(blocks[0]->headerPtr.get())) {
                        previousNodeBlockPtr->m_previousNode.Set(m_hdtnConfig.m_myNodeId, 0);
                        blocks[0]->SetManuallyModified();
                    }
                    else {
                        LOG_ERROR(subprocess) << "Process: dynamic_cast to Bpv7PreviousNodeCanonicalBlock failed";
                        return false;
                    }
                }
                else { //prepend new previous node block
                    std::unique_ptr<Bpv7CanonicalBlock> blockPtr = boost::make_unique<Bpv7PreviousNodeCanonicalBlock>();
                    Bpv7PreviousNodeCanonicalBlock & block = *(reinterpret_cast<Bpv7PreviousNodeCanonicalBlock*>(blockPtr.get()));

                    block.m_blockProcessingControlFlags = BPV7_BLOCKFLAG::REMOVE_BLOCK_IF_IT_CANT_BE_PROCESSED;
                    block.m_blockNumber = bv.GetNextFreeCanonicalBlockNumber();
                    block.m_crcType = BPV7_CRC_TYPE::CRC32C;
                    block.m_previousNode.Set(m_hdtnConfig.m_myNodeId, 0);
                    bv.PrependMoveCanonicalBlock(blockPtr);
                }

                //get hop count if exists and update it
                bv.GetCanonicalBlocksByType(BPV7_BLOCK_TYPE_CODE::HOP_COUNT, blocks);
                if (blocks.size() > 1) {
                    LOG_ERROR(subprocess) << "Process: version 7 bundle received has multiple hop count blocks";
                    return false;
                }
                else if (blocks.size() == 1) { //update existing
                    if (Bpv7HopCountCanonicalBlock* hopCountBlockPtr = dynamic_cast<Bpv7HopCountCanonicalBlock*>(blocks[0]->headerPtr.get())) {
                        //the hop count value SHOULD initially be zero and SHOULD be increased by 1 on each hop.
                        const uint64_t newHopCount = hopCountBlockPtr->m_hopCount + 1;
                        //When a bundle's hop count exceeds its
                        //hop limit, the bundle SHOULD be deleted for the reason "hop limit
                        //exceeded", following the bundle deletion procedure defined in
                        //Section 5.10.
                        //Hop limit MUST be in the range 1 through 255.
                        if ((newHopCount > hopCountBlockPtr->m_hopLimit) || (newHopCount > 255)) {
                            LOG_INFO(subprocess) << "Process dropping version 7 bundle with hop count " << newHopCount;
                            return false;
                        }
                        hopCountBlockPtr->m_hopCount = newHopCount;
                        blocks[0]->SetManuallyModified();
                    }
                    else {
                        LOG_ERROR(subprocess) << "Process: dynamic_cast to Bpv7HopCountCanonicalBlock failed";
                        return false;
                    }
                }
                if (isEcho) {
                    primary.m_destinationEid = primary.m_sourceNodeId;
                    finalDestEid = primary.m_sourceNodeId;
                    LOG_ERROR(subprocess) << "Sending Ping for destination " << finalDestEid;
                    primary.m_sourceNodeId = M_HDTN_EID_ECHO;
                    bv.m_primaryBlockView.SetManuallyModified();
                }

                if (!bv.RenderInPlace(PaddedMallocator<uint8_t>::PADDING_ELEMENTS_BEFORE)) {
                    LOG_ERROR(subprocess) << "Process: bpv7 RenderInPlace failed";
                    return false;
                }
                bundleCurrentSize = bv.m_renderedBundle.size();
            }
        }
        if (usingZmqData) {
            zmq::message_t * rxBufRawPointer = new zmq::message_t(std::move(*zmqPaddedMessageUnderlyingDataUniquePtr));
            zmqMessageToSendUniquePtr = boost::make_unique<zmq::message_t>((void*)bv.m_renderedBundle.data(), bundleCurrentSize, CustomCleanupZmqMessage, rxBufRawPointer);
        }
        else {
            padded_vector_uint8_t * rxBufRawPointer = new padded_vector_uint8_t(std::move(paddedVecMessageUnderlyingData));
            zmqMessageToSendUniquePtr = boost::make_unique<zmq::message_t>((void*)bv.m_renderedBundle.data(), bundleCurrentSize, CustomCleanupPaddedVecUint8, rxBufRawPointer);
        }
    }
    else {
        LOG_ERROR(subprocess) << "Process: unsupported bundle version received";
        return false;
    }


    if (isBundleForHdtnScheduler) { //forward to egress which will forward to scheduler
        //force natural/64-bit alignment
        hdtn::ToEgressHdr* toEgressHdr = new hdtn::ToEgressHdr();
        zmq::message_t zmqMessageToEgressHdrWithDataStolen(toEgressHdr, sizeof(hdtn::ToEgressHdr), CustomCleanupToEgressHdr, toEgressHdr);
        toEgressHdr->base.type = HDTN_MSGTYPE_BUNDLES_TO_SCHEDULER;
        {
            boost::mutex::scoped_lock lock(m_ingressToEgressZmqSocketMutex);
            if (!m_zmqPushSock_boundIngressToConnectingEgressPtr->send(std::move(zmqMessageToEgressHdrWithDataStolen), zmq::send_flags::sndmore | zmq::send_flags::dontwait)) {
                LOG_ERROR(subprocess) << "can't send toEgressHdr to egress";
            }
            else {
                if (!m_zmqPushSock_boundIngressToConnectingEgressPtr->send(std::move(*zmqMessageToSendUniquePtr), zmq::send_flags::dontwait)) {
                    LOG_ERROR(subprocess) << "can't send bundle intended for scheduler to egress";
                }
                else { //success
                    ++m_bundleCountEgress; //protected by m_ingressToEgressZmqSocketMutex
                    m_bundleByteCountEgress += bundleCurrentSize; //protected by m_ingressToEgressZmqSocketMutex
                }
            }
        }
        return true;
    }
    /*
    Config file changes:
    remove: zmqMaxMessagesPerPath, zmqMaxMessageSizeBytes, zmqRegistrationServerAddress, and zmqRegistrationServerPortPath from hdtn global configs
    add: bufferRxToStorageOnLinkUpSaturation (boolean default=false) to the hdtn global configs
    add: maxSumOfBundleBytesInPipeline (uint64_t) to the outducts
    rename: bundlePipelineLimit to maxNumberOfBundlesInPipeline in the outducts
    checks: an error shall be thrown if (maxBundleSizeBytes * 2) > maxSumOfBundleBytesInPipeline

    Context:
    It is the responsibility of Storage and Ingress, working together, to not overwhelm Egress and exceed the bundle pipeline limits of the Egress outducts.
    Egress can only pop bundles off the zeromq receive queue and throw them into an outduct, but if all components are working together,
    a maxNumberOfBundlesInPipeline exceeded error message should never happen along with its associated link-down event.

    Ingress now knows the capability of the egress outducts via a telemetry message from egress,
    such as bundle pipeline limits in terms of max number of bundles (maxNumberOfBundlesInPipeline) and
    max bundle size bytes (maxSumOfBundleBytesInPipeline). For a particular outduct,
    the max data it can hold in its sending pipeline shall not exceed, whatever comes first, either:
      1.) More bundles than maxNumberOfBundlesInPipeline
      2.) More total bytes of bundles than maxSumOfBundleBytesInPipeline 

    For bundles that are cut-through eligible (i.e. custody not requested and an outduct is available):
    Because ingress is agnostic of what data may be released from storage to that same outduct,
    ingress shall, as first priority, send/saturate half the pipeline capacity through the "zmq-cut-through-path-to-egress"
    (note: there is a unique/separate "zmq-cut-through-path-to-egress" defined PER OUTDUCT);
    egress shall not send an ack message back to ingress until egress receives acknowledgement from the
    convergence layer outduct that the bundle has been fully sent with confirmation from the receiver.
    If that "zmq-cut-through-path-to-egress" becomes full, ingress shall, as second priority,
    send/saturate half the pipeline capacity through the "zmq-cut-through-path-to-storage",
    but flagging the zmq header with a flag telling storage to not store the bundle but rather simply forward it to egress
    (note: there is a unique/separate "zmq-cut-through-path-to-storage" defined PER OUTDUCT);
    storage shall not send an ack message back to ingress until storage receives acknowledgement
    from egress that the bundle has been fully sent.  Storage shall multiplex these "forwarding non-store bundles"
    with "bundles being released from disk" so as to not exceed "half of outduct pipeline capacity",
    as storage also knows egress outducts' capabilities. When both of these zmq paths are saturated,
    this Ingress::ProcessPaddedData function shall block up to the specified time in seconds
    (i.e. hdtn config variable maxIngressBundleWaitOnEgressMilliseconds) to wait until capacity
    frees up on one of these two zmq paths, and in the event of timeout, the bundle gets written to
    disk over the "zmq-path-to-storage" (explained below in the non-cutthrough eligible bundles paragraph).
    Because the return of the Ingress::ProcessPaddedData function must happen prior to the convergence layer
    acknowledging the data (tcp and stcp won't call the operating system's receive function,
    and ltp won't send the "end-of-red-part report segment"), natural flow control is achieved.
    In cases where the induct receives data faster than the outduct can send the data,
    and if the induct convergence layer specifies highRxRateCanBufferToStorage=true in the config file,
    then ingress shall flag bundles as "mandatory store" and send these bundles over "zmq-path-to-storage"
    to be stored (and soon released). There is ONE global "zmq-path-to-storage" defined regardless of the number of outducts,
    and its capacity is maxBundleSizeBytes or 5 bundles, whatever comes first.

    For non-cutthrough eligible bundles due to no link availability or because custody was requested: 
    Ingress shall send/saturate the single "zmq-path-to-storage" up to maxBundleSizeBytes or 5 bundles, whatever comes first,
    but flagging the zmq header with a flag telling storage to store the bundle; storage shall not send an ack message back to ingress
    until storage fully writes the bundle to disk.  This non-cutthrough "zmq-path-to-storage" (with its pipeline limit)
    is a single path shared across all inducts so as not to overwhelm storage, however latency of solid-state disk writes should
    be relatively negligible compared to a long link one-way-light-time latency. When this "zmq-path-to-storage" for disk writing is saturated,
    this Ingress::ProcessPaddedData function shall block up to the specified time in seconds to wait until
    disk write acknowledgements are received by ingress. Natural flow control is achieved (explained above in previous paragraph),
    and hdtn shall never exhaust system memory.

    Worst case ram memory usage is given by summation of all outduct maxSumOfBundleBytesInPipeline,
    plus maxBundleSizeBytes for the single "zmq-path-to-storage".  Until a better implementation for TCPCL opportunistic links is defined,
    if one or more TCPCL outducts are used, the worst case ram memory usage must add (5 x maxBundleSizeBytes) in the event that
    a TCPCL outduct suddenly starts receiving bundles. The 5x value for TCPCL comes from a zeromq high-water-mark setting hardcoded into HDTN.
    */

    //bundle processed, now send to egress or storage

    //First check if a tcpcl opportunistic link is available, and if so, send the bundle outbound via an ingress induct
    //Note that Egress never decodes bundles, so egress has no way of knowing if custody was set.
    //Note: if needsProcessing is false, that means that the bundle came from storage.
    //If custody flag is set, only send the bundle out of an ingress link if and only if the bundle came from storage, because storage handles all things custody related.
    //Otherwise if custody flag is set but it didn't come from storage (came either from egress or ingress), send to storage first, and it will eventually come
    //back to this point in the code once storage processes custody.
    m_availableDestOpportunisticNodeIdToTcpclInductMapMutex.lock();
    std::map<uint64_t, Induct*>::iterator tcpclInductIterator = m_availableDestOpportunisticNodeIdToTcpclInductMap.find(finalDestEid.nodeId);
    const bool isOpportunisticLinkUp = (tcpclInductIterator != m_availableDestOpportunisticNodeIdToTcpclInductMap.end());
    m_availableDestOpportunisticNodeIdToTcpclInductMapMutex.unlock();
    const bool bundleCameFromStorageModule = (!needsProcessing);
    bool sentDataOnOpportunisticLink = false;
    if (isOpportunisticLinkUp && (bundleCameFromStorageModule || !(requestsCustody || isAdminRecordForHdtnStorage))) {
        if (tcpclInductIterator->second->ForwardOnOpportunisticLink(finalDestEid.nodeId, *zmqMessageToSendUniquePtr, 3)) { //thread safe forward with 3 second timeout
            sentDataOnOpportunisticLink = true;
        }
        else {
            LOG_ERROR(subprocess) << "Ingress::Process: tcpcl opportunistic forward timed out after 3 seconds for "
                << Uri::GetIpnUriString(finalDestEid.nodeId, finalDestEid.serviceId) << " ..trying the cut-through path or storage instead";
        }
    }

    if (!sentDataOnOpportunisticLink) {
        //First see if the cut through path is available (to egress).
        //Get the outduct information (which was sent from egress) that the bundle will be going to.
        //Lock a shared mutex for read only (note outduct information only gets written/updated whenever schedules/routes change).
        //This outduct information includes whether the link exists, or is up or down.
        //Note that inducts won't be initialized (won't be at this code location) until egress is fully up and running
        //and the first initial outduct capabilities telemetry is sent to ingress.
        bool useStorage = true; //will be set false if cut-through succeeds below
        const uint64_t fromIngressUniqueId = m_nextBundleUniqueIdAtomic.fetch_add(1, boost::memory_order_relaxed);
        { //begin scope for cut-through shared mutex lock
            uint64_t outductIndex = UINT64_MAX;
            ingress_shared_lock_t lockShared(m_sharedMutexFinalDestsToOutductArrayIndexMaps);
            std::map<cbhe_eid_t, uint64_t>::const_iterator itEid = m_mapFinalDestEidToOutductArrayIndex.find(finalDestEid);
            if (itEid != m_mapFinalDestEidToOutductArrayIndex.cend()) {
                outductIndex = itEid->second;
            }
            else {
                std::map<uint64_t, uint64_t>::const_iterator itNodeId = m_mapFinalDestNodeIdToOutductArrayIndex.find(finalDestEid.nodeId);
                if (itNodeId != m_mapFinalDestNodeIdToOutductArrayIndex.cend()) {
                    outductIndex = itNodeId->second;
                }
            }
            
            bool reservedStorageCutThroughPipelineAvailability = false;
            if (outductIndex != UINT64_MAX) {
                BundlePipelineAckingSet& bundleCutThroughPipelineAckingSetObj = *(m_vectorBundlePipelineAckingSet[outductIndex]);

                const bool shouldTryToUseCustThrough = ((bundleCutThroughPipelineAckingSetObj.m_linkIsUp && (!requestsCustody) && (!isAdminRecordForHdtnStorage)));
                useStorage = !shouldTryToUseCustThrough;

                if (shouldTryToUseCustThrough) { //type egress cut through ("while loop" instead of "if statement" to support breaking to storage)
                    bool reservedEgressPipelineAvailability;
                    static const boost::posix_time::time_duration noDuration = boost::posix_time::seconds(0);
                    const boost::posix_time::time_duration& cutThroughTimeoutRef = (m_hdtnConfig.m_bufferRxToStorageOnLinkUpSaturation)
                        ? noDuration : M_MAX_INGRESS_BUNDLE_WAIT_ON_EGRESS_TIME_DURATION;
                    const bool foundACutThroughPath = bundleCutThroughPipelineAckingSetObj.WaitForPipelineAvailabilityAndReserve(true, true,
                        cutThroughTimeoutRef, fromIngressUniqueId, zmqMessageToSendUniquePtr->size(),
                        reservedEgressPipelineAvailability, reservedStorageCutThroughPipelineAvailability);
                    if (foundACutThroughPath) {
                        if (reservedStorageCutThroughPipelineAvailability) { //pipeline limit exceeded for egress cut-through path
                            useStorage = true;
                            //storage should take on remaining other half of cut-through pipeline capability without actually storing the bundle
                            //pipeline limit may not have been exceeded for storage cut-through path depending on reservedStoragePipelineAvailability
                            ++m_eventsTooManyInEgressCutThroughQueue;
                        }
                        else { //if(reservedEgressPipelineAvailability) //pipeline limits not exceeded for egress cut-through path, continue to send the bundle to egress

                            //force natural/64-bit alignment
                            hdtn::ToEgressHdr* toEgressHdr = new hdtn::ToEgressHdr();
                            zmq::message_t zmqMessageToEgressHdrWithDataStolen(toEgressHdr, sizeof(hdtn::ToEgressHdr), CustomCleanupToEgressHdr, toEgressHdr);

                            //memset 0 not needed because all values set below
                            toEgressHdr->base.type = HDTN_MSGTYPE_EGRESS;
                            toEgressHdr->base.flags = 0; //flags not used by egress // static_cast<uint16_t>(primary.flags);
                            toEgressHdr->nextHopNodeId = bundleCutThroughPipelineAckingSetObj.GetNextHopNodeId();
                            toEgressHdr->finalDestEid = finalDestEid;
                            toEgressHdr->hasCustody = requestsCustody;
                            toEgressHdr->isCutThroughFromStorage = 0;
                            toEgressHdr->custodyId = fromIngressUniqueId;
                            toEgressHdr->outductIndex = outductIndex;
                            {
                                //zmq::message_t messageWithDataStolen(hdrPtr.get(), sizeof(hdtn::BlockHdr), CustomIgnoreCleanupBlockHdr); //cleanup will occur in the queue below
                                boost::mutex::scoped_lock lock(m_ingressToEgressZmqSocketMutex);
                                if (!m_zmqPushSock_boundIngressToConnectingEgressPtr->send(std::move(zmqMessageToEgressHdrWithDataStolen), zmq::send_flags::sndmore | zmq::send_flags::dontwait)) {
                                    LOG_ERROR(subprocess) << "can't send toEgressHdr to egress";
                                    bundleCutThroughPipelineAckingSetObj.CompareAndPop_ThreadSafe(fromIngressUniqueId, true);
                                    useStorage = true;
                                }
                                else {

                                    if (!m_zmqPushSock_boundIngressToConnectingEgressPtr->send(std::move(*zmqMessageToSendUniquePtr), zmq::send_flags::dontwait)) {
                                        LOG_ERROR(subprocess) << "can't send bundle to egress";
                                        bundleCutThroughPipelineAckingSetObj.CompareAndPop_ThreadSafe(fromIngressUniqueId, true);
                                        useStorage = true;
                                    }
                                    else {
                                        //success                            
                                        ++m_bundleCountEgress; //protected by m_ingressToEgressZmqSocketMutex
                                        m_bundleByteCountEgress += bundleCurrentSize; //protected by m_ingressToEgressZmqSocketMutex
                                    }
                                }
                            }
                        }
                    }
                    else { //did not find a cut through path
                        useStorage = true;
                        ++m_eventsTooManyInAllCutThroughQueues;
                    }
                }
            }
        
            if (useStorage) { //storage
                bool storageModuleAvailable = true;
                if (!reservedStorageCutThroughPipelineAvailability) { //cut through path was not available for egress or storage, time to store the bundle
                    ++m_eventsTooManyInStorageCutThroughQueue;
                    static const boost::posix_time::time_duration twoSeconds = boost::posix_time::seconds(2);
                    storageModuleAvailable = m_singleStorageBundlePipelineAckingSet.WaitForStoragePipelineAvailabilityAndReserve(twoSeconds,
                        fromIngressUniqueId, zmqMessageToSendUniquePtr->size());
                    outductIndex = UINT64_MAX;
                }
                if (storageModuleAvailable) {

                    BundlePipelineAckingSet& ackingSetObj = (outductIndex == UINT64_MAX) ?
                        m_singleStorageBundlePipelineAckingSet : (*(m_vectorBundlePipelineAckingSet[outductIndex])); //only used to restore state if zmq fails
                    
                    //force natural/64-bit alignment
                    hdtn::ToStorageHdr* toStorageHdr = new hdtn::ToStorageHdr();
                    zmq::message_t zmqMessageToStorageHdrWithDataStolen(toStorageHdr, sizeof(hdtn::ToStorageHdr), CustomCleanupToStorageHdr, toStorageHdr);

                    //memset 0 not needed because all values set below
                    toStorageHdr->base.type = HDTN_MSGTYPE_STORE;
                    toStorageHdr->base.flags = 0; //flags not used by storage // static_cast<uint16_t>(primary.flags);
                    toStorageHdr->ingressUniqueId = fromIngressUniqueId;
                    toStorageHdr->outductIndex = outductIndex;
                    toStorageHdr->dontStoreBundle = reservedStorageCutThroughPipelineAvailability;
                    toStorageHdr->isCustodyOrAdminRecord = (requestsCustody || isAdminRecordForHdtnStorage);
                    toStorageHdr->finalDestEid = finalDestEid;

                    //zmq threads not thread safe but protected by mutex below
                    boost::mutex::scoped_lock lock(m_ingressToStorageZmqSocketMutex);
                    if (!m_zmqPushSock_boundIngressToConnectingStoragePtr->send(std::move(zmqMessageToStorageHdrWithDataStolen), zmq::send_flags::sndmore | zmq::send_flags::dontwait)) {
                        LOG_ERROR(subprocess) << "can't send toStorageHdr to storage, this bundle will be lost";
                        ackingSetObj.CompareAndPop_ThreadSafe(fromIngressUniqueId, false);
                    }
                    else {
                        if (!m_zmqPushSock_boundIngressToConnectingStoragePtr->send(std::move(*zmqMessageToSendUniquePtr), zmq::send_flags::dontwait)) {
                            LOG_ERROR(subprocess) << "can't send bundle to storage, this bundle will be lost";
                            ackingSetObj.CompareAndPop_ThreadSafe(fromIngressUniqueId, false);
                        }
                        else {
                            //success                            
                            ++m_bundleCountStorage; //protected by m_ingressToStorageZmqSocketMutex
                            m_bundleByteCountStorage += bundleCurrentSize; //protected by m_ingressToStorageZmqSocketMutex
                        }
                    }
                }
                else {
                    LOG_ERROR(subprocess) << "storage module unresponsive, this bundle will be lost";
                }
            }
        } //end scope for cut-through shared mutex lock
    }

    return true;
}


void Ingress::Impl::WholeBundleReadyCallback(padded_vector_uint8_t & wholeBundleVec) {
    //if more than 1 BpSinkAsync context, must protect shared resources with mutex.  Each BpSinkAsync context has
    //its own processing thread that calls this callback
    static std::unique_ptr<zmq::message_t> unusedZmqPtr;
    ProcessPaddedData(wholeBundleVec.data(), wholeBundleVec.size(), unusedZmqPtr, wholeBundleVec, false, true);
}

void Ingress::Impl::SendOpportunisticLinkMessages(const uint64_t remoteNodeId, bool isAvailable) {
    //force natural/64-bit alignment
    hdtn::ToEgressHdr * toEgressHdr = new hdtn::ToEgressHdr();
    zmq::message_t zmqMessageToEgressHdrWithDataStolen(toEgressHdr, sizeof(hdtn::ToEgressHdr), CustomCleanupToEgressHdr, toEgressHdr);

    //not necessary to send to egress first before storage because storage marks bundles as opportunistic before sending them to egress
    //memset 0 not needed because all values set below
    toEgressHdr->base.type = isAvailable ? HDTN_MSGTYPE_EGRESS_ADD_OPPORTUNISTIC_LINK : HDTN_MSGTYPE_EGRESS_REMOVE_OPPORTUNISTIC_LINK;
    toEgressHdr->finalDestEid.nodeId = remoteNodeId; //only used field, rest are don't care
    {
        //zmq::message_t messageWithDataStolen(hdrPtr.get(), sizeof(hdtn::BlockHdr), CustomIgnoreCleanupBlockHdr); //cleanup will occur in the queue below
        boost::mutex::scoped_lock lock(m_ingressToEgressZmqSocketMutex);
        if (!m_zmqPushSock_boundIngressToConnectingEgressPtr->send(std::move(zmqMessageToEgressHdrWithDataStolen), zmq::send_flags::dontwait)) {
            LOG_ERROR(subprocess) << "can't send ToEgressHdr Opportunistic link message to egress";
        }
    }

    //force natural/64-bit alignment
    hdtn::ToStorageHdr * toStorageHdr = new hdtn::ToStorageHdr();
    zmq::message_t zmqMessageToStorageHdrWithDataStolen(toStorageHdr, sizeof(hdtn::ToStorageHdr), CustomCleanupToStorageHdr, toStorageHdr);

    //memset 0 not needed because all values set below
    toStorageHdr->base.type = isAvailable ? HDTN_MSGTYPE_STORAGE_ADD_OPPORTUNISTIC_LINK : HDTN_MSGTYPE_STORAGE_REMOVE_OPPORTUNISTIC_LINK;
    toStorageHdr->ingressUniqueId = remoteNodeId; //use this field as the remote node id
    {
        boost::mutex::scoped_lock lock(m_ingressToStorageZmqSocketMutex);
        if (!m_zmqPushSock_boundIngressToConnectingStoragePtr->send(std::move(zmqMessageToStorageHdrWithDataStolen), zmq::send_flags::dontwait)) {
            LOG_ERROR(subprocess) << "can't send ToStorageHdr Opportunistic link message to storage";
        }
    }
}

void Ingress::Impl::OnNewOpportunisticLinkCallback(const uint64_t remoteNodeId, Induct* thisInductPtr, void* sinkPtr) {
    if (TcpclInduct * tcpclInductPtr = dynamic_cast<TcpclInduct*>(thisInductPtr)) {
        LOG_INFO(subprocess) << "New opportunistic link detected on TcpclV3 induct for ipn:" << remoteNodeId << ".*";
        SendOpportunisticLinkMessages(remoteNodeId, true);
        boost::mutex::scoped_lock lock(m_availableDestOpportunisticNodeIdToTcpclInductMapMutex);
        m_availableDestOpportunisticNodeIdToTcpclInductMap[remoteNodeId] = tcpclInductPtr;
    }
    else if (TcpclV4Induct * tcpclInductPtr = dynamic_cast<TcpclV4Induct*>(thisInductPtr)) {
        LOG_INFO(subprocess) << "New opportunistic link detected on TcpclV4 induct for ipn:" << remoteNodeId << ".*";
        SendOpportunisticLinkMessages(remoteNodeId, true);
        boost::mutex::scoped_lock lock(m_availableDestOpportunisticNodeIdToTcpclInductMapMutex);
        m_availableDestOpportunisticNodeIdToTcpclInductMap[remoteNodeId] = tcpclInductPtr;
    }
    else if (StcpInduct* stcpInductPtr = dynamic_cast<StcpInduct*>(thisInductPtr)) {

    }
    else {
        LOG_ERROR(subprocess) << "OnNewOpportunisticLinkCallback: Induct ptr cannot cast to TcpclInduct or TcpclV4Induct";
    }
}
void Ingress::Impl::OnDeletedOpportunisticLinkCallback(const uint64_t remoteNodeId, Induct* thisInductPtr, void* sinkPtrAboutToBeDeleted) {
    if (StcpInduct* stcpInductPtr = dynamic_cast<StcpInduct*>(thisInductPtr)) {

    }
    else {
        LOG_INFO(subprocess) << "Deleted opportunistic link on Tcpcl induct for ipn:" << remoteNodeId << ".*";
        SendOpportunisticLinkMessages(remoteNodeId, false);
        boost::mutex::scoped_lock lock(m_availableDestOpportunisticNodeIdToTcpclInductMapMutex);
        m_availableDestOpportunisticNodeIdToTcpclInductMap.erase(remoteNodeId);
    }
}

struct ping_data_t {
    ping_data_t() : sequence(0) {}
    uint64_t sequence;
    boost::posix_time::ptime sendTime;
};

void Ingress::Impl::SendPing(const uint64_t remoteNodeId, const uint64_t remotePingServiceNumber, const uint64_t bpVersion) {
    static thread_local uint64_t lastMillisecondsSinceStartOfYear2000 = 0;
    static thread_local uint64_t lastTimeRfc5050 = 0;
    static thread_local uint64_t seq = 0;
    static thread_local ping_data_t pingPayload;
    static const uint64_t payloadSizeBytes = sizeof(pingPayload);
    std::unique_ptr<zmq::message_t> zmqMessageToSendUniquePtr; //create on heap as zmq default constructor costly
    if (bpVersion == 7) {
        BundleViewV7 bv;
        Bpv7CbhePrimaryBlock& primary = bv.m_primaryBlockView.header;
        //primary.SetZero();
        primary.m_bundleProcessingControlFlags = BPV7_BUNDLEFLAG::NOFRAGMENT;  //All BP endpoints identified by ipn-scheme endpoint IDs are singleton endpoints.
        primary.m_sourceNodeId = M_HDTN_EID_PING;
        primary.m_destinationEid.Set(remoteNodeId, remotePingServiceNumber);
        primary.m_reportToEid.Set(0, 0);
        primary.m_creationTimestamp.SetTimeFromNow();
        if (primary.m_creationTimestamp.millisecondsSinceStartOfYear2000 == lastMillisecondsSinceStartOfYear2000) {
            ++seq;
        }
        else {
            seq = 0;
        }
        lastMillisecondsSinceStartOfYear2000 = primary.m_creationTimestamp.millisecondsSinceStartOfYear2000;
        primary.m_creationTimestamp.sequenceNumber = seq;
        primary.m_lifetimeMilliseconds = 100000;
        primary.m_crcType = BPV7_CRC_TYPE::CRC32C;
        bv.m_primaryBlockView.SetManuallyModified();

        //add hop count block (before payload last block)
        {
            std::unique_ptr<Bpv7CanonicalBlock> blockPtr = boost::make_unique<Bpv7HopCountCanonicalBlock>();
            Bpv7HopCountCanonicalBlock& block = *(reinterpret_cast<Bpv7HopCountCanonicalBlock*>(blockPtr.get()));

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
            Bpv7CanonicalBlock& payloadBlock = *payloadBlockPtr;
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
            LOG_ERROR(subprocess) << "error rendering bpv7 bundle";
            return;
        }

        BundleViewV7::Bpv7CanonicalBlockView& payloadBlockView = bv.m_listCanonicalBlockView.back(); //payload block must be the last block

        ++pingPayload.sequence;
        pingPayload.sendTime = boost::posix_time::microsec_clock::universal_time();
        //manually copy data to preallocated space and compute crc
        memcpy(payloadBlockView.headerPtr->m_dataPtr, &pingPayload, sizeof(pingPayload)); //m_dataPtr now points to new allocated or copied data within the serialized block (from after Render())
        payloadBlockView.headerPtr->RecomputeCrcAfterDataModification((uint8_t*)payloadBlockView.actualSerializedBlockPtr.data(), payloadBlockView.actualSerializedBlockPtr.size()); //recompute crc

        //move the bundle out of bundleView
        std::vector<uint8_t>* rxBufRawPointer = new std::vector<uint8_t>(std::move(bv.m_frontBuffer));
        zmqMessageToSendUniquePtr = boost::make_unique<zmq::message_t>(std::move(zmq::message_t(rxBufRawPointer->data(), rxBufRawPointer->size(), CustomCleanupStdVecUint8, rxBufRawPointer)));
    }
    else { //bp version 6
        BundleViewV6 bv;
        Bpv6CbhePrimaryBlock& primary = bv.m_primaryBlockView.header;
        //primary.SetZero();

        primary.m_bundleProcessingControlFlags =
            BPV6_BUNDLEFLAG::PRIORITY_NORMAL |
            BPV6_BUNDLEFLAG::SINGLETON |
            BPV6_BUNDLEFLAG::NOFRAGMENT;

        primary.m_sourceNodeId = M_HDTN_EID_PING;
        primary.m_destinationEid.Set(remoteNodeId, remotePingServiceNumber);

        primary.m_creationTimestamp.SetTimeFromNow();
        if (primary.m_creationTimestamp.secondsSinceStartOfYear2000 == lastTimeRfc5050) {
            ++seq;
        }
        else {
            seq = 0;
        }
        lastTimeRfc5050 = primary.m_creationTimestamp.secondsSinceStartOfYear2000;
        primary.m_creationTimestamp.sequenceNumber = seq;
        primary.m_lifetimeSeconds = 100;
        bv.m_primaryBlockView.SetManuallyModified();

        //append payload block (must be last block)
        {
            std::unique_ptr<Bpv6CanonicalBlock> payloadBlockPtr = boost::make_unique<Bpv6CanonicalBlock>();
            Bpv6CanonicalBlock& payloadBlock = *payloadBlockPtr;
            //payloadBlock.SetZero();

            payloadBlock.m_blockTypeCode = BPV6_BLOCK_TYPE_CODE::PAYLOAD;
            payloadBlock.m_blockProcessingControlFlags = BPV6_BLOCKFLAG::NO_FLAGS_SET;
            payloadBlock.m_blockTypeSpecificDataLength = payloadSizeBytes;
            payloadBlock.m_blockTypeSpecificDataPtr = NULL; //NULL will preallocate (won't copy or compute crc, user must do that manually below)
            bv.AppendMoveCanonicalBlock(payloadBlockPtr);
        }

        //render bundle to the front buffer
        if (!bv.Render(payloadSizeBytes + 1000)) {
            LOG_ERROR(subprocess) << "error rendering bpv6 bundle";
            return;
        }

        BundleViewV6::Bpv6CanonicalBlockView& payloadBlockView = bv.m_listCanonicalBlockView.back(); //payload block is the last block in this case

        ++pingPayload.sequence;
        pingPayload.sendTime = boost::posix_time::microsec_clock::universal_time();

        //manually copy data to preallocated space and compute crc
        memcpy(payloadBlockView.headerPtr->m_blockTypeSpecificDataPtr, &pingPayload, sizeof(pingPayload)); //m_dataPtr now points to new allocated or copied data within the serialized block (from after Render())

        //move the bundle out of bundleView
        std::vector<uint8_t>* rxBufRawPointer = new std::vector<uint8_t>(std::move(bv.m_frontBuffer));
        zmqMessageToSendUniquePtr = boost::make_unique<zmq::message_t>(std::move(zmq::message_t(rxBufRawPointer->data(), rxBufRawPointer->size(), CustomCleanupStdVecUint8, rxBufRawPointer)));
    }
    static padded_vector_uint8_t unusedPaddedVecMessage;
    ProcessPaddedData((uint8_t*)zmqMessageToSendUniquePtr->data(), zmqMessageToSendUniquePtr->size(),
        zmqMessageToSendUniquePtr, unusedPaddedVecMessage,
        true, false); //last param false => does not need processing because it came from here (also needed because not padded data!)
}

void Ingress::Impl::ProcessReceivedPingPayload(const uint8_t* data, const uint64_t size, const uint64_t bpVersion) {
    const boost::posix_time::ptime nowTime = boost::posix_time::microsec_clock::universal_time();
    ping_data_t pingData;
    if (size != sizeof(pingData)) {
        LOG_ERROR(subprocess) << "error in ProcessReceivedPingPayload: received payload size not " << sizeof(pingData);
        return;
    }
    memcpy(&pingData, data, sizeof(pingData));
    const boost::posix_time::time_duration diff = nowTime - pingData.sendTime;
    const double millisecs = (static_cast<double>(diff.total_microseconds())) * 0.001;
    static const boost::format fmtTemplate("Ping Bpv%d received: sequence=%d, took %0.3f milliseconds");
    boost::format fmt(fmtTemplate);
    fmt % bpVersion % pingData.sequence % millisecs;
    LOG_INFO(subprocess) << fmt.str();
}

}  // namespace hdtn
