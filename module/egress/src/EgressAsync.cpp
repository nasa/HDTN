/**
 * @file EgressAsync.cpp
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
 * This file contains the implementation for the HDTN Egress module.
 */

#include "EgressAsync.h"
#include <string>
#include "message.hpp"
#include <boost/thread.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/make_unique.hpp>
#include <boost/date_time.hpp>
#include "OutductManager.h"
#include "Logger.h"
#include "Uri.h"
#include "TimestampUtil.h"
#include "ThreadNamer.h"

namespace hdtn {

static constexpr hdtn::Logger::SubProcess subprocess = hdtn::Logger::SubProcess::egress;

struct Egress::Impl : private boost::noncopyable {

    Impl();
    ~Impl();
    void Stop();
    bool Init(const HdtnConfig& hdtnConfig, const HdtnDistributedConfig& hdtnDistributedConfig, zmq::context_t* hdtnOneProcessZmqInprocContextPtr);

private:
    void RouterEventHandler();
    void ReadZmqThreadFunc();
    void WholeBundleReadyCallback(padded_vector_uint8_t& wholeBundleVec);
    void OnFailedBundleZmqSendCallback(zmq::message_t& movableBundle, std::vector<uint8_t>& userData, uint64_t outductUuid, bool successCallbackCalled);
    void OnSuccessfulBundleSendCallback(std::vector<uint8_t>& userData, uint64_t outductUuid);
    void OnOutductLinkStatusChangedCallback(bool isLinkDownEvent, uint64_t outductUuid);
    void ResendOutductCapabilities();
    void SchedulerEventHandler(hdtn::IreleaseChangeHdr& releaseChangeHdr);

public:
    //telemetry
    AllOutductTelemetry_t m_allOutductTelem;
    std::size_t m_totalCustodyTransfersSentToStorage;
    std::size_t m_totalCustodyTransfersSentToIngress;
private:
    uint64_t m_totalTcpclBundlesReceivedMutexProtected;
    uint64_t m_totalTcpclBundleBytesReceivedMutexProtected;

    std::unique_ptr<zmq::context_t> m_zmqCtxPtr;
    std::unique_ptr<zmq::socket_t> m_zmqPullSock_boundIngressToConnectingEgressPtr;
    std::unique_ptr<zmq::socket_t> m_zmqPushSock_connectingEgressToBoundIngressPtr;
    boost::mutex m_mutex_zmqPushSock_connectingEgressToBoundIngress;
    std::unique_ptr<zmq::socket_t> m_zmqPushSock_connectingEgressBundlesOnlyToBoundIngressPtr;
    std::unique_ptr<zmq::socket_t> m_zmqPullSock_connectingStorageToBoundEgressPtr;
    std::unique_ptr<zmq::socket_t> m_zmqPushSock_boundEgressToConnectingStoragePtr;
    boost::mutex m_mutex_zmqPushSock_boundEgressToConnectingStorage;
    std::unique_ptr<zmq::socket_t> m_zmqPushSock_boundEgressToConnectingSchedulerPtr;
    std::unique_ptr<zmq::socket_t> m_zmqPullSock_connectingRouterToBoundEgressPtr;
    std::unique_ptr<zmq::socket_t> m_zmqSubSock_boundSchedulerToConnectingEgressPtr;

    std::unique_ptr<zmq::socket_t> m_zmqRepSock_connectingTelemToFromBoundEgressPtr;

    std::unique_ptr<zmq::socket_t> m_zmqPairSock_LinkStatusWaitPtr;
    std::unique_ptr<zmq::socket_t> m_zmqPairSock_LinkStatusNotifyOnePtr;

    OutductManager m_outductManager;
    HdtnConfig m_hdtnConfig;

    boost::mutex m_mutexPushBundleToIngress;
    boost::mutex m_mutexLinkStatusUpdate;

    std::unique_ptr<boost::thread> m_threadZmqReaderPtr;
    volatile bool m_running;

    //for blocking until worker-thread startup
    volatile bool m_workerThreadStartupInProgress;
    boost::mutex m_workerThreadStartupMutex;
    boost::condition_variable m_workerThreadStartupConditionVariable;

    std::shared_ptr<std::string> m_lastJsonAoctSharedPtr; //for all outduct capabilities telem to be sent to telem_cmd_interface
};

Egress::Impl::Impl() :
    m_running(false),
    m_totalCustodyTransfersSentToIngress(0),
    m_totalCustodyTransfersSentToStorage(0),
    m_totalTcpclBundlesReceivedMutexProtected(0),
    m_totalTcpclBundleBytesReceivedMutexProtected(0),
    m_workerThreadStartupInProgress(false) {}

Egress::Egress() :
    m_pimpl(boost::make_unique<Egress::Impl>()),
    //references
    m_allOutductTelemRef(m_pimpl->m_allOutductTelem),
    m_totalCustodyTransfersSentToStorage(m_pimpl->m_totalCustodyTransfersSentToStorage),
    m_totalCustodyTransfersSentToIngress(m_pimpl->m_totalCustodyTransfersSentToIngress) {}

Egress::Impl::~Impl() {
    Stop();
}

Egress::~Egress() {
    Stop();
}

void Egress::Stop() {
    m_pimpl->Stop();
}
void Egress::Impl::Stop() {
    m_running = false;
    if(m_threadZmqReaderPtr) {
        try {
            m_threadZmqReaderPtr->join();
            m_threadZmqReaderPtr.reset(); //delete it
        }
        catch (boost::thread_resource_error &e) {
            LOG_ERROR(subprocess) << "error stopping Egress thread: " << e.what();
        }
    }
}

bool Egress::Init(const HdtnConfig& hdtnConfig, const HdtnDistributedConfig& hdtnDistributedConfig, zmq::context_t* hdtnOneProcessZmqInprocContextPtr) {
    return m_pimpl->Init(hdtnConfig, hdtnDistributedConfig, hdtnOneProcessZmqInprocContextPtr);
}
bool Egress::Impl::Init(const HdtnConfig & hdtnConfig, const HdtnDistributedConfig& hdtnDistributedConfig, zmq::context_t * hdtnOneProcessZmqInprocContextPtr) {
    
    if (m_running) {
        LOG_ERROR(subprocess) << "Egress::Init called while Egress is already running";
        return false;
    }

    m_hdtnConfig = hdtnConfig;


    m_zmqCtxPtr = boost::make_unique<zmq::context_t>(); //needed at least by scheduler pubsub (and if one-process is not used)
    try {
        if (hdtnOneProcessZmqInprocContextPtr) {
            // socket for cut-through mode straight to egress
            //The io_threads argument specifies the size of the 0MQ thread pool to handle I/O operations.
            //If your application is using only the inproc transport for messaging you may set this to zero, otherwise set it to at least one.
            m_zmqPullSock_boundIngressToConnectingEgressPtr = boost::make_unique<zmq::socket_t>(*hdtnOneProcessZmqInprocContextPtr, zmq::socket_type::pair);
            m_zmqPullSock_boundIngressToConnectingEgressPtr->connect(std::string("inproc://bound_ingress_to_connecting_egress"));
            m_zmqPushSock_connectingEgressToBoundIngressPtr = boost::make_unique<zmq::socket_t>(*hdtnOneProcessZmqInprocContextPtr, zmq::socket_type::pair);
            m_zmqPushSock_connectingEgressToBoundIngressPtr->connect(std::string("inproc://connecting_egress_to_bound_ingress"));
            // socket for sending bundles from egress via tcpcl outduct opportunistic link (because tcpcl can be bidirectional)
            m_zmqPushSock_connectingEgressBundlesOnlyToBoundIngressPtr = boost::make_unique<zmq::socket_t>(*hdtnOneProcessZmqInprocContextPtr, zmq::socket_type::pair);
            m_zmqPushSock_connectingEgressBundlesOnlyToBoundIngressPtr->connect(std::string("inproc://connecting_egress_bundles_only_to_bound_ingress"));
            // socket for bundles from storage
            m_zmqPullSock_connectingStorageToBoundEgressPtr = boost::make_unique<zmq::socket_t>(*hdtnOneProcessZmqInprocContextPtr, zmq::socket_type::pair);
            m_zmqPullSock_connectingStorageToBoundEgressPtr->bind(std::string("inproc://connecting_storage_to_bound_egress"));
            m_zmqPushSock_boundEgressToConnectingStoragePtr = boost::make_unique<zmq::socket_t>(*hdtnOneProcessZmqInprocContextPtr, zmq::socket_type::pair);
            m_zmqPushSock_boundEgressToConnectingStoragePtr->bind(std::string("inproc://bound_egress_to_connecting_storage"));

            //from telem socket
            m_zmqRepSock_connectingTelemToFromBoundEgressPtr = boost::make_unique<zmq::socket_t>(*hdtnOneProcessZmqInprocContextPtr, zmq::socket_type::pair);
            m_zmqRepSock_connectingTelemToFromBoundEgressPtr->bind(std::string("inproc://connecting_telem_to_from_bound_egress"));

            //socket for sending LinkStatus events from Egress to Scheduler
            m_zmqPushSock_boundEgressToConnectingSchedulerPtr = boost::make_unique<zmq::socket_t>(*hdtnOneProcessZmqInprocContextPtr, zmq::socket_type::pair);
            m_zmqPushSock_boundEgressToConnectingSchedulerPtr->bind(std::string("inproc://bound_egress_to_connecting_scheduler"));

            //socket for getting Route Update events from Router to Egress
            m_zmqPullSock_connectingRouterToBoundEgressPtr = boost::make_unique<zmq::socket_t>(*hdtnOneProcessZmqInprocContextPtr, zmq::socket_type::pair);
            m_zmqPullSock_connectingRouterToBoundEgressPtr->bind(std::string("inproc://connecting_router_to_bound_egress"));
        }
        else {
            // socket for cut-through mode straight to egress
            m_zmqPullSock_boundIngressToConnectingEgressPtr = boost::make_unique<zmq::socket_t>(*m_zmqCtxPtr, zmq::socket_type::pull);
            const std::string connect_boundIngressToConnectingEgressPath(
                std::string("tcp://") +
                hdtnDistributedConfig.m_zmqIngressAddress +
                std::string(":") +
                boost::lexical_cast<std::string>(hdtnDistributedConfig.m_zmqBoundIngressToConnectingEgressPortPath));
            m_zmqPullSock_boundIngressToConnectingEgressPtr->connect(connect_boundIngressToConnectingEgressPath);
            
            m_zmqPushSock_connectingEgressToBoundIngressPtr = boost::make_unique<zmq::socket_t>(*m_zmqCtxPtr, zmq::socket_type::push);
            const std::string connect_connectingEgressToBoundIngressPath(
                std::string("tcp://") +
                hdtnDistributedConfig.m_zmqIngressAddress +
                std::string(":") +
                boost::lexical_cast<std::string>(hdtnDistributedConfig.m_zmqConnectingEgressToBoundIngressPortPath));
            m_zmqPushSock_connectingEgressToBoundIngressPtr->connect(connect_connectingEgressToBoundIngressPath);
            // socket for sending bundles from egress via tcpcl outduct opportunistic link (because tcpcl can be bidirectional)
            m_zmqPushSock_connectingEgressBundlesOnlyToBoundIngressPtr = boost::make_unique<zmq::socket_t>(*m_zmqCtxPtr, zmq::socket_type::push);
            const std::string connect_connectingEgressBundlesOnlyToBoundIngressPath(
                std::string("tcp://") +
                hdtnDistributedConfig.m_zmqIngressAddress +
                std::string(":") +
                boost::lexical_cast<std::string>(hdtnDistributedConfig.m_zmqConnectingEgressBundlesOnlyToBoundIngressPortPath));
            m_zmqPushSock_connectingEgressBundlesOnlyToBoundIngressPtr->connect(connect_connectingEgressBundlesOnlyToBoundIngressPath);
            // socket for bundles from storage
            m_zmqPullSock_connectingStorageToBoundEgressPtr = boost::make_unique<zmq::socket_t>(*m_zmqCtxPtr, zmq::socket_type::pull);
            const std::string bind_connectingStorageToBoundEgressPath(
                std::string("tcp://*:") + boost::lexical_cast<std::string>(hdtnDistributedConfig.m_zmqConnectingStorageToBoundEgressPortPath));
            m_zmqPullSock_connectingStorageToBoundEgressPtr->bind(bind_connectingStorageToBoundEgressPath);
            m_zmqPushSock_boundEgressToConnectingStoragePtr = boost::make_unique<zmq::socket_t>(*m_zmqCtxPtr, zmq::socket_type::push);
            const std::string bind_boundEgressToConnectingStoragePath(
                std::string("tcp://*:") + boost::lexical_cast<std::string>(hdtnDistributedConfig.m_zmqBoundEgressToConnectingStoragePortPath));
            m_zmqPushSock_boundEgressToConnectingStoragePtr->bind(bind_boundEgressToConnectingStoragePath);

            //from telemetry socket
            m_zmqRepSock_connectingTelemToFromBoundEgressPtr = boost::make_unique<zmq::socket_t>(*m_zmqCtxPtr, zmq::socket_type::rep);
            const std::string bind_connectingTelemToFromBoundEgressPath(
                std::string("tcp://*:") + boost::lexical_cast<std::string>(hdtnDistributedConfig.m_zmqConnectingTelemToFromBoundEgressPortPath));
            m_zmqRepSock_connectingTelemToFromBoundEgressPtr->bind(bind_connectingTelemToFromBoundEgressPath);

            //socket for sending LinkStatus events from Egress to Scheduler
            m_zmqPushSock_boundEgressToConnectingSchedulerPtr = boost::make_unique<zmq::socket_t>(*m_zmqCtxPtr, zmq::socket_type::push);
            const std::string bind_boundEgressToConnectingSchedulerPath(
                std::string("tcp://*:") + boost::lexical_cast<std::string>(hdtnDistributedConfig.m_zmqBoundEgressToConnectingSchedulerPortPath));
            m_zmqPushSock_boundEgressToConnectingSchedulerPtr->bind(bind_boundEgressToConnectingSchedulerPath);

            //socket for getting Route Update events from Router to Egress
            m_zmqPullSock_connectingRouterToBoundEgressPtr = boost::make_unique<zmq::socket_t>(*m_zmqCtxPtr, zmq::socket_type::pull);
            const std::string bind_connectingRouterToBoundEgressPath(
                std::string("tcp://*:") + boost::lexical_cast<std::string>(hdtnDistributedConfig.m_zmqConnectingRouterToBoundEgressPortPath));
            m_zmqPullSock_connectingRouterToBoundEgressPtr->bind(bind_connectingRouterToBoundEgressPath);
        }

        // socket for receiving events from scheduler
        m_zmqSubSock_boundSchedulerToConnectingEgressPtr = boost::make_unique<zmq::socket_t>(*m_zmqCtxPtr, zmq::socket_type::sub);
        const std::string connect_boundSchedulerPubSubPath(
            std::string("tcp://") +
            ((hdtnOneProcessZmqInprocContextPtr == NULL) ? hdtnDistributedConfig.m_zmqSchedulerAddress : std::string("localhost")) +
            std::string(":") +
            boost::lexical_cast<std::string>(m_hdtnConfig.m_zmqBoundSchedulerPubSubPortPath));

        try {
            m_zmqSubSock_boundSchedulerToConnectingEgressPtr->connect(connect_boundSchedulerPubSubPath);
            m_zmqSubSock_boundSchedulerToConnectingEgressPtr->set(zmq::sockopt::linger, 0); //prevent hang when deleting the zmqCtxPtr
            LOG_INFO(subprocess) << "Connected to scheduler at " << connect_boundSchedulerPubSubPath << " , subscribing...";
        }
        catch (const zmq::error_t & ex) {
            LOG_ERROR(subprocess) << "Cannot connect to scheduler socket at " << connect_boundSchedulerPubSubPath << " : " << ex.what();
            return false;
        }
        try {
            //Sends one-byte 0x1 message to scheduler XPub socket plus strlen of subscription
            //All release messages shall be prefixed by "aaaaaaaa" before the common header
            //Egress unique subscription shall be "b" (gets all messages that start with "b")
            m_zmqSubSock_boundSchedulerToConnectingEgressPtr->set(zmq::sockopt::subscribe, "b");
            LOG_INFO(subprocess) << "Subscribed to all events from scheduler";
        }
        catch (const zmq::error_t& ex) {
            LOG_ERROR(subprocess) << "Cannot subscribe to all events from scheduler: " << ex.what();
            return false;
        }

        //an inproc to make sure link status sent from within the zmq polling loop
        static const std::string zmqLinkStatusAddress = "inproc://egress_ls";
        m_zmqPairSock_LinkStatusWaitPtr = boost::make_unique<zmq::socket_t>(*m_zmqCtxPtr, zmq::socket_type::pair);
        m_zmqPairSock_LinkStatusNotifyOnePtr = boost::make_unique<zmq::socket_t>(*m_zmqCtxPtr, zmq::socket_type::pair);
        m_zmqPairSock_LinkStatusWaitPtr->bind(zmqLinkStatusAddress);
        m_zmqPairSock_LinkStatusNotifyOnePtr->connect(zmqLinkStatusAddress);

        //Caution: All options, with the exception of ZMQ_SUBSCRIBE, ZMQ_UNSUBSCRIBE and ZMQ_LINGER, only take effect for subsequent socket bind/connects.
        //The value of 0 specifies no linger period. Pending messages shall be discarded immediately when the socket is closed with zmq_close().
        m_zmqRepSock_connectingTelemToFromBoundEgressPtr->set(zmq::sockopt::linger, 0); //prevent hang when deleting the zmqCtxPtr
        m_zmqPushSock_connectingEgressBundlesOnlyToBoundIngressPtr->set(zmq::sockopt::linger, 0); //prevent hang when deleting the zmqCtxPtr
        m_zmqPushSock_boundEgressToConnectingStoragePtr->set(zmq::sockopt::linger, 0); //prevent hang when deleting the zmqCtxPtr
        m_zmqPushSock_connectingEgressToBoundIngressPtr->set(zmq::sockopt::linger, 0); //prevent hang when deleting the zmqCtxPtr
        m_zmqPushSock_boundEgressToConnectingSchedulerPtr->set(zmq::sockopt::linger, 0); //prevent hang when deleting the zmqCtxPtr
        
        //The ZMQ_SNDHWM option shall set the high water mark for outbound messages on the specified socket.
        //The high water mark is a hard limit on the maximum number of outstanding messages MQ shall queue 
        //in memory for any single peer that the specified socket is communicating with.
        //If this limit has been reached the socket shall enter an exceptional state and depending on the socket type,
        //MQ shall take appropriate action such as blocking or dropping sent messages.
        const int hwm = 5; //todo
        m_zmqPushSock_connectingEgressBundlesOnlyToBoundIngressPtr->set(zmq::sockopt::sndhwm, hwm); //flow control
    }
    catch (const zmq::error_t & ex) {
        LOG_ERROR(subprocess) << "egress cannot set up zmq socket: " << ex.what();
        return false;
    }

    //load outducts after all zmq sockets created (in case an outduct link status changed callback is called which uses them)
    if (!m_outductManager.LoadOutductsFromConfig(m_hdtnConfig.m_outductsConfig, m_hdtnConfig.m_myNodeId, m_hdtnConfig.m_maxLtpReceiveUdpPacketSizeBytes, m_hdtnConfig.m_maxBundleSizeBytes,
        boost::bind(&Egress::Impl::WholeBundleReadyCallback, this, boost::placeholders::_1),
        OnFailedBundleVecSendCallback_t(), //egress only sends zmq bundles (not vec8) so this will never be needed
        //boost::bind(&hdtn::HegrManagerAsync::OnFailedBundleVecSendCallback, this, boost::placeholders::_1, boost::placeholders::_2, boost::placeholders::_3),
        boost::bind(&Egress::Impl::OnFailedBundleZmqSendCallback, this, boost::placeholders::_1, boost::placeholders::_2, boost::placeholders::_3, boost::placeholders::_4),
        boost::bind(&Egress::Impl::OnSuccessfulBundleSendCallback, this, boost::placeholders::_1, boost::placeholders::_2),
        boost::bind(&Egress::Impl::OnOutductLinkStatusChangedCallback, this, boost::placeholders::_1, boost::placeholders::_2)
    ))
    {
        return false;
    }


    { //start worker thread
        m_running = true;

        boost::mutex::scoped_lock workerThreadStartupLock(m_workerThreadStartupMutex);
        m_workerThreadStartupInProgress = true;

        m_threadZmqReaderPtr = boost::make_unique<boost::thread>(
            boost::bind(&Egress::Impl::ReadZmqThreadFunc, this)); //create and start the worker thread

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
    }
    return true;
}



static void CustomCleanupEgressAckHdrNoHint(void *data, void *hint) {
    (void)hint;
    delete static_cast<hdtn::EgressAckHdr *>(data);
}

static void CustomCleanupStdVecUint8(void* data, void* hint) {
    delete static_cast<std::vector<uint8_t>*>(hint);
}
static void CustomCleanupStdString(void* data, void* hint) {
    delete static_cast<std::string*>(hint);
}
static void CustomCleanupSharedPtrStdString(void* data, void* hint) {
    std::shared_ptr<std::string>* serializedRawPtrToSharedPtr = static_cast<std::shared_ptr<std::string>* >(hint);
    //LOG_DEBUG(subprocess) << "cleanup refcnt=" << serializedRawPtrToSharedPtr->use_count();
    delete serializedRawPtrToSharedPtr; //reduce ref count and delete shared_ptr object
}

void Egress::Impl::RouterEventHandler() {
    hdtn::RouteUpdateHdr routeUpdateHdr;
    const zmq::recv_buffer_result_t res = m_zmqPullSock_connectingRouterToBoundEgressPtr->recv(
        zmq::mutable_buffer(&routeUpdateHdr, sizeof(routeUpdateHdr)), zmq::recv_flags::dontwait);
    if (!res) {
        LOG_ERROR(subprocess) << "cannot read RouteUpdateHdr";
    }
    else if ((res->truncated()) || (res->size != sizeof(routeUpdateHdr))) {
        LOG_ERROR(subprocess) << "RouteUpdateHdr message mismatch: untruncated = " << res->untruncated_size
            << " truncated = " << res->size << " expected = " << sizeof(routeUpdateHdr);
    }
    else if (routeUpdateHdr.base.type == HDTN_MSGTYPE_ROUTEUPDATE) {
        if (m_outductManager.Reroute_ThreadSafe(routeUpdateHdr.finalDestNodeId, routeUpdateHdr.nextHopNodeId)) {
            ResendOutductCapabilities();
            LOG_INFO(subprocess) << "Updated the outduct based on the optimal Route for finalDestNodeId " << routeUpdateHdr.finalDestNodeId
                << ": New Outduct Next Hop is " << routeUpdateHdr.nextHopNodeId;
        }
        else {
            LOG_INFO(subprocess) << "Failed to update the outduct based on the optimal Route for finalDestNodeId " << routeUpdateHdr.finalDestNodeId
                << " to a next hop outduct node id " << routeUpdateHdr.nextHopNodeId;
        }
    }
    else {
        LOG_ERROR(subprocess) << "RouterEventHandler received unknown message type " << routeUpdateHdr.base.type;
    }
}

void Egress::Impl::ReadZmqThreadFunc() {
    ThreadNamer::SetThisThreadName("egressZmqReader");

#if 0 //not needed because scheduler will alert when link available
    while (m_running) {
        LOG_INFO(subprocess) << "Waiting for Outduct to become ready to forward...";
        boost::this_thread::sleep(boost::posix_time::milliseconds(1000));
        if (m_outductManager.AllReadyToForward()) {
            LOG_INFO(subprocess) << "Outduct ready to forward";
            break;
        }
    }


    if (!m_running) {
        LOG_INFO(subprocess) << "Egress terminated before all outducts could connect";
        return;
    }
#endif

    std::set<uint64_t> availableDestOpportunisticNodeIdsSet;

    // Use a form of receive that times out so we can terminate cleanly.
    static const int timeout = 250;  // milliseconds
    static constexpr unsigned int NUM_SOCKETS = 6;

    //THIS PROBABLY DOESNT WORK SINCE IT HAPPENED AFTER BIND/CONNECT BUT NOT USED ANYWAY BECAUSE OF POLLITEMS
    //m_zmqPullSock_boundIngressToConnectingEgressPtr->set(zmq::sockopt::rcvtimeo, timeout);
    //m_zmqPullSock_connectingStorageToBoundEgressPtr->set(zmq::sockopt::rcvtimeo, timeout);

    zmq::pollitem_t items[NUM_SOCKETS] = {
        {m_zmqPullSock_boundIngressToConnectingEgressPtr->handle(), 0, ZMQ_POLLIN, 0},
        {m_zmqPullSock_connectingStorageToBoundEgressPtr->handle(), 0, ZMQ_POLLIN, 0},
        {m_zmqPullSock_connectingRouterToBoundEgressPtr->handle(), 0, ZMQ_POLLIN, 0},
        {m_zmqRepSock_connectingTelemToFromBoundEgressPtr->handle(), 0, ZMQ_POLLIN, 0},
        {m_zmqPairSock_LinkStatusWaitPtr->handle(), 0, ZMQ_POLLIN, 0},
        {m_zmqSubSock_boundSchedulerToConnectingEgressPtr->handle(), 0, ZMQ_POLLIN, 0}
    };
    zmq::socket_t * const firstTwoSockets[2] = {
        m_zmqPullSock_boundIngressToConnectingEgressPtr.get(),
        m_zmqPullSock_connectingStorageToBoundEgressPtr.get()
    };

    //notify Init function that worker thread startup is complete
    m_workerThreadStartupMutex.lock();
    m_workerThreadStartupInProgress = false;
    m_workerThreadStartupMutex.unlock();
    m_workerThreadStartupConditionVariable.notify_one();

    //Get initial outduct capabilities and send to ingress and storage
    ResendOutductCapabilities();

    static const long DEFAULT_BIG_TIMEOUT_POLL = 250;
    while (m_running) { //keep thread alive if running
        int rc = 0;
        try {
            rc = zmq::poll(&items[0], NUM_SOCKETS, DEFAULT_BIG_TIMEOUT_POLL);
        }
        catch (zmq::error_t & e) {
            LOG_ERROR(subprocess) << "caught zmq::error_t in hdtn::HegrManagerAsync::ReadZmqThreadFunc: " << e.what();
            continue;
        }
        if (rc > 0) {
            for (unsigned int itemIndex = 0; itemIndex < 2; ++itemIndex) { //skip m_zmqPullSignalInprocSockPtr in this loop
                
                if ((items[itemIndex].revents & ZMQ_POLLIN) == 0) {
                    continue;
                }

                const bool isCutThroughFromIngress = (itemIndex == 0);

                hdtn::ToEgressHdr toEgressHeader;
                const zmq::recv_buffer_result_t res = firstTwoSockets[itemIndex]->recv(zmq::mutable_buffer(&toEgressHeader, sizeof(hdtn::ToEgressHdr)), zmq::recv_flags::none);
                if (!res) {
                    LOG_ERROR(subprocess) << "HegrManagerAsync::ReadZmqThreadFunc: cannot read BlockHdr";
                    continue;
                }
                else if ((res->truncated()) || (res->size != sizeof(hdtn::ToEgressHdr))) {
                    LOG_ERROR(subprocess) << "blockhdr message mismatch: untruncated = " << res->untruncated_size 
                        << " truncated = " << res->size << " expected = " << sizeof(hdtn::ToEgressHdr);
                    continue;
                }
                else if ((itemIndex == 0) && (toEgressHeader.base.type == HDTN_MSGTYPE_EGRESS_ADD_OPPORTUNISTIC_LINK)) {
                    LOG_INFO(subprocess) << "adding opportunistic link " << toEgressHeader.finalDestEid.nodeId;
                    availableDestOpportunisticNodeIdsSet.insert(toEgressHeader.finalDestEid.nodeId);
                    continue;
                }
                else if ((itemIndex == 0) && (toEgressHeader.base.type == HDTN_MSGTYPE_EGRESS_REMOVE_OPPORTUNISTIC_LINK)) {
                    LOG_INFO(subprocess) << "removing opportunistic link " << toEgressHeader.finalDestEid.nodeId;
                    availableDestOpportunisticNodeIdsSet.erase(toEgressHeader.finalDestEid.nodeId);
                    continue;
                }
                else if ((itemIndex == 0) && (toEgressHeader.base.type == HDTN_MSGTYPE_BUNDLES_TO_SCHEDULER)) {
                    LOG_INFO(subprocess) << "forwarding bundle to scheduler";
                    zmq::message_t zmqMessageBundleToScheduler;
                    //message guaranteed to be there due to the zmq::send_flags::sndmore
                    if (!firstTwoSockets[itemIndex]->recv(zmqMessageBundleToScheduler, zmq::recv_flags::none)) {
                        LOG_ERROR(subprocess) << "error receiving zmqMessageBundleToScheduler";
                    }
                    else {
                        hdtn::LinkStatusHdr linkStatusMsg;
                        linkStatusMsg.base.type = HDTN_MSGTYPE_BUNDLES_TO_SCHEDULER;
                        while (m_running && !m_zmqPushSock_boundEgressToConnectingSchedulerPtr->send(
                            zmq::const_buffer(&linkStatusMsg, sizeof(linkStatusMsg)), zmq::send_flags::sndmore | zmq::send_flags::dontwait))
                        {
                            LOG_INFO(subprocess) << "waiting for scheduler to become available to send HDTN_MSGTYPE_BUNDLES_TO_SCHEDULER header";
                            boost::this_thread::sleep(boost::posix_time::seconds(1));
                        }
                        while (m_running && !m_zmqPushSock_boundEgressToConnectingSchedulerPtr->send(zmqMessageBundleToScheduler, zmq::send_flags::dontwait)) {
                            LOG_INFO(subprocess) << "waiting for scheduler to become available to send it a scheduler-only bundle received by ingress";
                            boost::this_thread::sleep(boost::posix_time::seconds(1));
                        }
                    }
                    continue;
                }
                else if (toEgressHeader.base.type != HDTN_MSGTYPE_EGRESS) {
                    LOG_ERROR(subprocess) << "toEgressHeader.base.type != HDTN_MSGTYPE_EGRESS";
                    continue;
                }
                
                zmq::message_t zmqMessageBundle;
                //message guaranteed to be there due to the zmq::send_flags::sndmore
                if (!firstTwoSockets[itemIndex]->recv(zmqMessageBundle, zmq::recv_flags::none)) {
                    LOG_ERROR(subprocess) << "error on sockets[itemIndex]->recv";
                    continue;
                }
                const uint64_t zmqMessageBundleSize = zmqMessageBundle.size();
                

                const cbhe_eid_t & finalDestEid = toEgressHeader.finalDestEid;
                //TODO DERMINE IF availableDestOpportunisticNodeIdsSet IS NEEDED
                if ((itemIndex == 1) && (availableDestOpportunisticNodeIdsSet.count(finalDestEid.nodeId) || toEgressHeader.IsOpportunisticLink())) { //from storage and opportunistic link available in ingress
                    hdtn::EgressAckHdr * egressAckPtr = new hdtn::EgressAckHdr();
                    //memset 0 not needed because all values set below
                    egressAckPtr->base.type = HDTN_MSGTYPE_EGRESS_ACK_TO_STORAGE;
                    egressAckPtr->base.flags = 0;
                    egressAckPtr->nextHopNodeId = toEgressHeader.nextHopNodeId;
                    egressAckPtr->finalDestEid = finalDestEid;
                    egressAckPtr->error = 0; //can set later before sending this ack if error
                    egressAckPtr->deleteNow = (toEgressHeader.hasCustody == 0);
                    egressAckPtr->isResponseToStorageCutThrough = toEgressHeader.isCutThroughFromStorage;
                    egressAckPtr->custodyId = toEgressHeader.custodyId;
                    egressAckPtr->outductIndex = toEgressHeader.outductIndex;

                    zmq::message_t messageWithDataStolen(egressAckPtr, sizeof(hdtn::EgressAckHdr), CustomCleanupEgressAckHdrNoHint); //storage can be acked right away since bundle transferred
                    {
                        boost::mutex::scoped_lock lock(m_mutex_zmqPushSock_boundEgressToConnectingStorage);
                        if (!m_zmqPushSock_boundEgressToConnectingStoragePtr->send(std::move(messageWithDataStolen), zmq::send_flags::dontwait)) {
                            LOG_ERROR(subprocess) << "m_zmqPushSock_boundEgressToConnectingStoragePtr could not send";
                            break;
                        }
                        ++m_totalCustodyTransfersSentToStorage;
                    }

                    boost::mutex::scoped_lock lock(m_mutexPushBundleToIngress);
                    static const char messageFlags = 0; //0 => from storage and needs no processing
                    static const zmq::const_buffer messageFlagsConstBuf(&messageFlags, sizeof(messageFlags));
                    if (!m_zmqPushSock_connectingEgressBundlesOnlyToBoundIngressPtr->send(messageFlagsConstBuf, zmq::send_flags::sndmore)) { //blocks if above 5 high water mark
                        LOG_ERROR(subprocess) << "WholeBundleReadyCallback: zmq could not send messageFlagsConstBuf to ingress";
                    }
                    else if (!m_zmqPushSock_connectingEgressBundlesOnlyToBoundIngressPtr->send(std::move(zmqMessageBundle), zmq::send_flags::none)) { //blocks if above 5 high water mark
                        LOG_ERROR(subprocess) << "WholeBundleReadyCallback: zmq could not forward bundle to ingress";
                    }
                    else {
                        ++m_allOutductTelem.m_totalStorageToIngressOpportunisticBundles;
                        m_allOutductTelem.m_totalStorageToIngressOpportunisticBundleBytes += zmqMessageBundleSize;
                    }
                }
                else if (Outduct * outduct = m_outductManager.GetOutductByFinalDestinationEid_ThreadSafe(finalDestEid)) {
                    std::vector<uint8_t> userData(sizeof(hdtn::EgressAckHdr));
                    hdtn::EgressAckHdr* egressAckPtr = (hdtn::EgressAckHdr*)userData.data();
                    //memset 0 not needed because all values set below
                    egressAckPtr->base.type = (isCutThroughFromIngress) ? HDTN_MSGTYPE_EGRESS_ACK_TO_INGRESS : HDTN_MSGTYPE_EGRESS_ACK_TO_STORAGE;
                    egressAckPtr->base.flags = 0;
                    egressAckPtr->nextHopNodeId = toEgressHeader.nextHopNodeId;
                    egressAckPtr->finalDestEid = finalDestEid;
                    egressAckPtr->error = 0; //can set later before sending this ack if error
                    egressAckPtr->deleteNow = (toEgressHeader.hasCustody == 0);
                    egressAckPtr->isResponseToStorageCutThrough = toEgressHeader.isCutThroughFromStorage;
                    egressAckPtr->custodyId = toEgressHeader.custodyId;
                    egressAckPtr->outductIndex = toEgressHeader.outductIndex;
                    outduct->Forward(zmqMessageBundle, std::move(userData));
                    if (zmqMessageBundle.size() != 0) {
                        LOG_ERROR(subprocess) << "hdtn::HegrManagerAsync::ProcessZmqMessagesThreadFunc, zmqMessage was not moved.. bundle shall remain in storage";

                        OnFailedBundleZmqSendCallback(zmqMessageBundle, userData, outduct->GetOutductUuid(), false); //todo is this correct?.. verify userdata not moved
                    }
                    else {
                        m_allOutductTelem.m_totalBundleBytesGivenToOutducts += zmqMessageBundleSize;
                        ++m_allOutductTelem.m_totalBundlesGivenToOutducts;
                    }
                }
                else {
                    LOG_FATAL(subprocess) << "HegrManagerAsync::ProcessZmqMessagesThreadFunc: no outduct for " 
                        << Uri::GetIpnUriString(finalDestEid.nodeId, finalDestEid.serviceId);
                }

            }

            if (items[2].revents & ZMQ_POLLIN) { //events from Router
                LOG_INFO(subprocess) << "Received RouteUpdate event!!";
                RouterEventHandler();
            }

            if (items[3].revents & ZMQ_POLLIN) { //telemetry requests data
                uint8_t telemMsgByte;
                const zmq::recv_buffer_result_t res = m_zmqRepSock_connectingTelemToFromBoundEgressPtr->recv(zmq::mutable_buffer(&telemMsgByte, sizeof(telemMsgByte)), zmq::recv_flags::dontwait);
                if (!res) {
                    LOG_ERROR(subprocess) << "ReadZmqThreadFunc: cannot read telemMsgByte";
                }
                else if ((res->truncated()) || (res->size != sizeof(telemMsgByte))) {
                    LOG_ERROR(subprocess) << "telemMsgByte message mismatch: untruncated = " << res->untruncated_size
                        << " truncated = " << res->size << " expected = " << sizeof(telemMsgByte);
                }
                else if (telemMsgByte != TELEM_REQ_MSG) {
                    LOG_ERROR(subprocess) << "error telemMsgByte not 1";
                }
                else {
                    m_outductManager.PopulateAllOutductTelemetry(m_allOutductTelem); //also sets m_totalBundlesSuccessfullySent, m_totalBundleBytesSuccessfullySent
                    m_mutexPushBundleToIngress.lock();
                    m_allOutductTelem.m_totalTcpclBundlesReceived = m_totalTcpclBundlesReceivedMutexProtected;
                    m_allOutductTelem.m_totalTcpclBundleBytesReceived = m_totalTcpclBundleBytesReceivedMutexProtected;
                    m_mutexPushBundleToIngress.unlock();

                    std::string* allOutductTelemJsonStringPtr = new std::string(m_allOutductTelem.ToJson());
                    std::string& strRef = *allOutductTelemJsonStringPtr;
                    zmq::message_t zmqJsonMessage(&strRef[0], allOutductTelemJsonStringPtr->size(), CustomCleanupStdString, allOutductTelemJsonStringPtr);

                    //send telemetry
                    if (m_lastJsonAoctSharedPtr) {
                        std::string* stringRawPointer = new std::string(std::move(*m_lastJsonAoctSharedPtr));
                        std::string& strAoctRef = *stringRawPointer;
                        zmq::message_t zmqTelemMessageWithDataStolen(&strAoctRef[0], stringRawPointer->size(), CustomCleanupStdString, stringRawPointer);
                        m_lastJsonAoctSharedPtr.reset();
                        //use msg.more() on receiving end to know if this is multipart
                        if (!m_zmqRepSock_connectingTelemToFromBoundEgressPtr->send(std::move(zmqTelemMessageWithDataStolen), zmq::send_flags::sndmore | zmq::send_flags::dontwait)) {
                            LOG_ERROR(subprocess) << "can't send Json Aoct telemetry to telemetry interface";
                        }
                    }

                    if (!m_zmqRepSock_connectingTelemToFromBoundEgressPtr->send(std::move(zmqJsonMessage), zmq::send_flags::dontwait)) {
                        LOG_ERROR(subprocess) << "can't send json telemetry to telem";
                    }
                }
            }

            if (items[4].revents & ZMQ_POLLIN) { //zmq inproc from link changes
                zmq::message_t linkStatusMessage;
                if (!m_zmqPairSock_LinkStatusWaitPtr->recv(linkStatusMessage, zmq::recv_flags::none)) {
                    LOG_ERROR(subprocess) << "cannot read inproc link status message";
                }
                //TODO PREVENT DUPLICATE MESSAGES
                LOG_INFO(subprocess) << "Sending LinkStatus update event to Scheduler";
                while (m_running && !m_zmqPushSock_boundEgressToConnectingSchedulerPtr->send(linkStatusMessage, zmq::send_flags::dontwait)) {
                    LOG_INFO(subprocess) << "waiting for scheduler to become available to send a link status change from an outduct";
                    boost::this_thread::sleep(boost::posix_time::seconds(1));
                }
            }

            if (items[5].revents & ZMQ_POLLIN) { //events from scheduler
                hdtn::IreleaseChangeHdr releaseChangeHdr;
                const zmq::recv_buffer_result_t res = m_zmqSubSock_boundSchedulerToConnectingEgressPtr->recv(zmq::mutable_buffer(&releaseChangeHdr, sizeof(releaseChangeHdr)), zmq::recv_flags::none);
                if (!res) {
                    LOG_ERROR(subprocess) << "unable to receive IreleaseChangeHdr message";
                }
                else if ((res->truncated()) || (res->size != sizeof(releaseChangeHdr))) {
                    LOG_ERROR(subprocess) << "message mismatch with IreleaseChangeHdr: untruncated = " << res->untruncated_size
                        << " truncated = " << res->size << " expected = " << sizeof(releaseChangeHdr);
                }
                else {
                    SchedulerEventHandler(releaseChangeHdr);
                }
            }
        }
    }

    LOG_INFO(subprocess) << "HegrManagerAsync::ReadZmqThreadFunc thread exiting";
    LOG_DEBUG(subprocess) << "m_totalCustodyTransfersSentToStorage: " << m_totalCustodyTransfersSentToStorage;
    LOG_DEBUG(subprocess) << "m_totalCustodyTransfersSentToIngress: " << m_totalCustodyTransfersSentToIngress;
}

//must be called from within ReadZmqThreadFunc to protect m_zmqPushSock_boundEgressToConnectingSchedulerPtr
void Egress::Impl::ResendOutductCapabilities() {
    AllOutductCapabilitiesTelemetry_t allOutductCapabilitiesTelemetry;
    m_outductManager.GetAllOutductCapabilitiesTelemetry_ThreadSafe(allOutductCapabilitiesTelemetry);

    //one serialization in one memory location, 3 shared_ptr references
    std::shared_ptr<std::string>* jsonRawPtrToSharedPtr =
        new std::shared_ptr<std::string>(std::make_shared<std::string>(allOutductCapabilitiesTelemetry.ToJson()));
    std::shared_ptr<std::string>& sharedPtrRef = *jsonRawPtrToSharedPtr;
    std::string& strRef = *sharedPtrRef;

    zmq::message_t zmqMsgToIngress(
        &strRef[0],
        strRef.size(),
        CustomCleanupSharedPtrStdString,
        jsonRawPtrToSharedPtr);

    std::shared_ptr<std::string>* jsonRawPtrToSharedPtr2 = new std::shared_ptr<std::string>(sharedPtrRef); //ref count 2
    zmq::message_t zmqMsgToStorage(
        &strRef[0],
        strRef.size(),
        CustomCleanupSharedPtrStdString,
        jsonRawPtrToSharedPtr2);

    std::shared_ptr<std::string>* jsonRawPtrToSharedPtr3 = new std::shared_ptr<std::string>(sharedPtrRef); //ref count 3
    m_lastJsonAoctSharedPtr = sharedPtrRef; //for sending the latest on telemetry request (ref count 4)
    zmq::message_t zmqMsgToScheduler(
        &strRef[0],
        strRef.size(),
        CustomCleanupSharedPtrStdString,
        jsonRawPtrToSharedPtr3);


    hdtn::EgressAckHdr egressAck;
    //memset 0 not needed because remaining values are "don't care"
    egressAck.base.type = HDTN_MSGTYPE_ALL_OUTDUCT_CAPABILITIES_TELEMETRY;
    zmq::const_buffer headerMessageEgressAck(&egressAck, sizeof(egressAck));

    hdtn::LinkStatusHdr linkStatusHdr;
    //memset 0 not needed because remaining values are "don't care"
    linkStatusHdr.base.type = HDTN_MSGTYPE_ALL_OUTDUCT_CAPABILITIES_TELEMETRY;
    zmq::const_buffer headerMessageLinkStatus(&linkStatusHdr, sizeof(linkStatusHdr));
    
    { //storage
        boost::mutex::scoped_lock lock(m_mutex_zmqPushSock_boundEgressToConnectingStorage);
        while (m_running && !m_zmqPushSock_boundEgressToConnectingStoragePtr->send(headerMessageEgressAck, zmq::send_flags::sndmore | zmq::send_flags::dontwait)) {
            //send returns false if(!res.has_value() AND zmq_errno()=EAGAIN)
            LOG_INFO(subprocess) << "waiting for storage to become available to send outduct capabilities header";
            boost::this_thread::sleep(boost::posix_time::seconds(1));
            //LOG_FATAL(subprocess) << "m_zmqPushSock_boundEgressToConnectingStoragePtr could not send outduct capabilities header: " << zmq_strerror(zmq_errno());
            //return;
        }
        if (m_running && !m_zmqPushSock_boundEgressToConnectingStoragePtr->send(std::move(zmqMsgToStorage), zmq::send_flags::dontwait)) {
            LOG_FATAL(subprocess) << "m_zmqPushSock_boundEgressToConnectingStoragePtr could not send outduct capabilities";
            return;
        }
    }

    { //ingress
        boost::mutex::scoped_lock lock(m_mutex_zmqPushSock_connectingEgressToBoundIngress);
        while (m_running && !m_zmqPushSock_connectingEgressToBoundIngressPtr->send(headerMessageEgressAck, zmq::send_flags::sndmore | zmq::send_flags::dontwait)) {
            LOG_INFO(subprocess) << "waiting for ingress to become available to send outduct capabilities header";
            boost::this_thread::sleep(boost::posix_time::seconds(1));
            //LOG_FATAL(subprocess) << "m_zmqPushSock_connectingEgressToBoundIngressPtr could not send outduct capabilities header";
            //return;
        }
        if (m_running && !m_zmqPushSock_connectingEgressToBoundIngressPtr->send(std::move(zmqMsgToIngress), zmq::send_flags::dontwait)) {
            LOG_FATAL(subprocess) << "m_zmqPushSock_connectingEgressToBoundIngressPtr could not send outduct capabilities";
            return;
        }
    }

    { //scheduler
        while (m_running && !m_zmqPushSock_boundEgressToConnectingSchedulerPtr->send(headerMessageLinkStatus, zmq::send_flags::sndmore | zmq::send_flags::dontwait)) {
            LOG_INFO(subprocess) << "waiting for scheduler to become available to send outduct capabilities header";
            boost::this_thread::sleep(boost::posix_time::seconds(1));
            //LOG_FATAL(subprocess) << "m_zmqPubSock_boundEgressToConnectingSchedulerPtr could not send outduct capabilities header";
            //return;
        }
        if (m_running && !m_zmqPushSock_boundEgressToConnectingSchedulerPtr->send(std::move(zmqMsgToScheduler), zmq::send_flags::dontwait)) {
            LOG_FATAL(subprocess) << "m_zmqPubSock_boundEgressToConnectingSchedulerPtr could not send outduct capabilities";
            return;
        }
        
    }
    if (!m_running) {
        LOG_FATAL(subprocess) << "terminated before ResendOutductCapabilities could finish";
    }
    else {
        LOG_DEBUG(subprocess) << "sent outduct capabilities";
    }
}

static void CustomCleanupPaddedVecUint8(void *data, void *hint) {
    delete static_cast<padded_vector_uint8_t*>(hint);
}

//from egress bidirectional tcpcl outduct receive path and opportunistic link potentially available in ingress (otherwise ingress will give it to storage if unavailable)
void Egress::Impl::WholeBundleReadyCallback(padded_vector_uint8_t & wholeBundleVec) {
    //must protect shared resources with mutex.
    //this is an optimization because we only have one chunk to send
    //The zmq_msg_init_data() function shall initialise the message object referenced by msg
    //to represent the content referenced by the buffer located at address data, size bytes long.
    //No copy of data shall be performed and 0MQ shall take ownership of the supplied buffer.
    //If provided, the deallocation function ffn shall be called once the data buffer is no longer
    //required by 0MQ, with the data and hint arguments supplied to zmq_msg_init_data().
    static const char messageFlags = 1; //1 => from egress and needs processing
    static const zmq::const_buffer messageFlagsConstBuf(&messageFlags, sizeof(messageFlags));
    const uint64_t wholeBundleVecSize = wholeBundleVec.size();
    padded_vector_uint8_t * rxBufRawPointer = new padded_vector_uint8_t(std::move(wholeBundleVec));
    zmq::message_t paddedMessageWithDataStolen(rxBufRawPointer->data() - rxBufRawPointer->get_allocator().PADDING_ELEMENTS_BEFORE,
        rxBufRawPointer->size() + rxBufRawPointer->get_allocator().TOTAL_PADDING_ELEMENTS, CustomCleanupPaddedVecUint8, rxBufRawPointer);
    boost::mutex::scoped_lock lock(m_mutexPushBundleToIngress);
    if (!m_zmqPushSock_connectingEgressBundlesOnlyToBoundIngressPtr->send(messageFlagsConstBuf, zmq::send_flags::sndmore)) { //blocks if above 5 high water mark
        LOG_ERROR(subprocess) << "WholeBundleReadyCallback: zmq could not send messageFlagsConstBuf to ingress";
    }
    else if (!m_zmqPushSock_connectingEgressBundlesOnlyToBoundIngressPtr->send(std::move(paddedMessageWithDataStolen), zmq::send_flags::none)) { //blocks if above 5 high water mark
        LOG_ERROR(subprocess) << "WholeBundleReadyCallback: zmq could not forward bundle to ingress";
    }
    else {
        ++m_totalTcpclBundlesReceivedMutexProtected;
        m_totalTcpclBundleBytesReceivedMutexProtected += wholeBundleVecSize;
    }
}

void Egress::Impl::OnFailedBundleZmqSendCallback(zmq::message_t& movableBundle, std::vector<uint8_t>& userData, uint64_t outductUuid, bool successCallbackCalled) {

    static constexpr bool isLinkDownEvent = true;
    OnOutductLinkStatusChangedCallback(isLinkDownEvent, outductUuid);

    if (successCallbackCalled) { //ltp sender with sessions from disk enabled
        LOG_ERROR(subprocess) << "OnFailedBundleZmqSendCallback called, TODO: send to ingress with needsProcessing set to false, dropping bundle for now";
    }
    else {
        std::vector<uint8_t>* vecUint8RawPointerToUserData = new std::vector<uint8_t>(std::move(userData));
        zmq::message_t zmqUserDataMessageWithDataStolen(vecUint8RawPointerToUserData->data(), vecUint8RawPointerToUserData->size(), CustomCleanupStdVecUint8, vecUint8RawPointerToUserData);
        hdtn::EgressAckHdr* egressAckPtr = (hdtn::EgressAckHdr*)vecUint8RawPointerToUserData->data();
        egressAckPtr->error = 1;

        if (egressAckPtr->base.type == HDTN_MSGTYPE_EGRESS_ACK_TO_INGRESS) {
            //If the type is HDTN_MSGTYPE_EGRESS_ACK_TO_INGRESS, then the bundle came from ingress.  Send the ack to ingress with the error flag set.
            //This will allow ingress to trigger a link down event more quickly than waiting for scheduler.
            //Then generate a HDTN_MSGTYPE_EGRESS_FAILED_BUNDLE_TO_STORAGE message plus the bundle and send to storage.


            //send ack message to ingress
            {
                boost::mutex::scoped_lock lock(m_mutex_zmqPushSock_connectingEgressToBoundIngress);
                if (!m_zmqPushSock_connectingEgressToBoundIngressPtr->send(std::move(zmqUserDataMessageWithDataStolen), zmq::send_flags::dontwait)) {
                    LOG_ERROR(subprocess) << "zmq could not send ingress an ack from egress";
                }
                ++m_totalCustodyTransfersSentToIngress;
            }

            //Send HDTN_MSGTYPE_EGRESS_FAILED_BUNDLE_TO_STORAGE message plus the bundle to storage.
            hdtn::EgressAckHdr* egressAckPtr = new hdtn::EgressAckHdr();
            //memset 0 not needed because all values set below
            egressAckPtr->base.type = HDTN_MSGTYPE_EGRESS_FAILED_BUNDLE_TO_STORAGE;
            zmq::message_t messageFailedHeaderWithDataStolen(egressAckPtr, sizeof(hdtn::EgressAckHdr), CustomCleanupEgressAckHdrNoHint); //storage can be acked right away since bundle transferred
            {
                boost::mutex::scoped_lock lock(m_mutex_zmqPushSock_boundEgressToConnectingStorage);
                if (!m_zmqPushSock_boundEgressToConnectingStoragePtr->send(std::move(messageFailedHeaderWithDataStolen), zmq::send_flags::sndmore | zmq::send_flags::dontwait)) {
                    LOG_ERROR(subprocess) << "m_zmqPushSock_boundEgressToConnectingStoragePtr could not send HDTN_MSGTYPE_EGRESS_FAILED_BUNDLE_TO_STORAGE";
                }
                else if (!m_zmqPushSock_boundEgressToConnectingStoragePtr->send(std::move(movableBundle), zmq::send_flags::dontwait)) {
                    LOG_ERROR(subprocess) << "m_zmqPushSock_boundEgressToConnectingStoragePtr could not send bundle";
                }

            }
        }
        else { //HDTN_MSGTYPE_EGRESS_ACK_TO_STORAGE
            //If the type is HDTN_MSGTYPE_EGRESS_ACK_TO_STORAGE, then the bundle came from storage.  Send the ack to storage with the error flag set.
            //This will allow storage to trigger a link down event more quickly than waiting for scheduler.
            //Since storage already has the bundle, the error flag will prevent deletion and move the bundle back to the "awaiting send" state,
            //but the bundle won't be immediately released again from storage because of the immediate link down event.
            //IF isResponseToStorageCutThrough is set, then storage needs the bundle back in a multipart message, otherwise, storage already has the bundle.
            const bool needsBundleBack = egressAckPtr->isResponseToStorageCutThrough;
            boost::mutex::scoped_lock lock(m_mutex_zmqPushSock_boundEgressToConnectingStorage);
            if (!m_zmqPushSock_boundEgressToConnectingStoragePtr->send(std::move(zmqUserDataMessageWithDataStolen),
                zmq::send_flags::dontwait | ((needsBundleBack) ? zmq::send_flags::sndmore : zmq::send_flags::none)))
            {
                LOG_ERROR(subprocess) << "m_zmqPushSock_boundEgressToConnectingStoragePtr could not send";
            }
            else if (needsBundleBack && (!m_zmqPushSock_boundEgressToConnectingStoragePtr->send(std::move(movableBundle), zmq::send_flags::dontwait))) {
                LOG_ERROR(subprocess) << "m_zmqPushSock_boundEgressToConnectingStoragePtr could not send bundle";
            }
            else {
                ++m_totalCustodyTransfersSentToStorage;
            }
        }
    }
}
void Egress::Impl::OnSuccessfulBundleSendCallback(std::vector<uint8_t>& userData, uint64_t outductUuid) {
    if (userData.empty()) {
        LOG_ERROR(subprocess) << "HegrManagerAsync::OnSuccessfulBundleSendCallback: userData empty";
        return;
    }

    static constexpr bool isLinkDownEvent = false;
    OnOutductLinkStatusChangedCallback(isLinkDownEvent, outductUuid);
    
    //this is an optimization because we only have one chunk to send
    //The zmq_msg_init_data() function shall initialise the message object referenced by msg
    //to represent the content referenced by the buffer located at address data, size bytes long.
    //No copy of data shall be performed and 0MQ shall take ownership of the supplied buffer.
    //If provided, the deallocation function ffn shall be called once the data buffer is no longer
    //required by 0MQ, with the data and hint arguments supplied to zmq_msg_init_data().
    std::vector<uint8_t>* vecUint8RawPointerToUserData = new std::vector<uint8_t>(std::move(userData));
    zmq::message_t zmqUserDataMessageWithDataStolen(vecUint8RawPointerToUserData->data(), vecUint8RawPointerToUserData->size(), CustomCleanupStdVecUint8, vecUint8RawPointerToUserData);
    hdtn::EgressAckHdr* egressAckPtr = (hdtn::EgressAckHdr*)vecUint8RawPointerToUserData->data();
    if (egressAckPtr->base.type == HDTN_MSGTYPE_EGRESS_ACK_TO_STORAGE) {
        boost::mutex::scoped_lock lock(m_mutex_zmqPushSock_boundEgressToConnectingStorage);
        if (!m_zmqPushSock_boundEgressToConnectingStoragePtr->send(std::move(zmqUserDataMessageWithDataStolen), zmq::send_flags::dontwait)) {
            LOG_ERROR(subprocess) << "m_zmqPushSock_boundEgressToConnectingStoragePtr could not send";
        }
        ++m_totalCustodyTransfersSentToStorage;
    }
    else {
        //send ack message by echoing back the block
        boost::mutex::scoped_lock lock(m_mutex_zmqPushSock_connectingEgressToBoundIngress);
        if (!m_zmqPushSock_connectingEgressToBoundIngressPtr->send(std::move(zmqUserDataMessageWithDataStolen), zmq::send_flags::dontwait)) {
            LOG_ERROR(subprocess) << "zmq could not send ingress an ack from egress";
        }
        ++m_totalCustodyTransfersSentToIngress;
    }
}
//Inform scheduler only if the physical link status actually changes or the physical link status is initially unknown.
//Note: LTP "ping" maintains its own physical link status and will also only call this function if the physical link status actually changes
void Egress::Impl::OnOutductLinkStatusChangedCallback(bool isLinkDownEvent, uint64_t outductUuid) {
    
    Outduct* outduct = m_outductManager.GetOutductByOutductUuid(outductUuid);
    if (!outduct) {
        LOG_FATAL(subprocess) << "could not find outduct from OnOutductLinkStatusChangedCallback";
        return;
    }
    const bool linkIsUpPhysically = !isLinkDownEvent;
    const bool informScheduler = ((!outduct->m_physicalLinkStatusIsKnown) || (outduct->m_linkIsUpPhysically != linkIsUpPhysically));
    outduct->m_physicalLinkStatusIsKnown = true;
    outduct->m_linkIsUpPhysically = linkIsUpPhysically;
    if (informScheduler) {
        hdtn::LinkStatusHdr linkStatusMsg;
        memset(&linkStatusMsg, 0, sizeof(hdtn::LinkStatusHdr));
        linkStatusMsg.base.type = HDTN_MSGTYPE_LINKSTATUS;
        linkStatusMsg.unixTimeSecondsSince1970 = TimestampUtil::GetSecondsSinceEpochUnix();
        linkStatusMsg.event = (isLinkDownEvent) ? 0 : 1;
        linkStatusMsg.uuid = outductUuid;

        {
            boost::mutex::scoped_lock lock(m_mutexLinkStatusUpdate);
            if (!m_zmqPairSock_LinkStatusNotifyOnePtr->send(zmq::const_buffer(&linkStatusMsg, sizeof(linkStatusMsg)), zmq::send_flags::dontwait)) {
                LOG_FATAL(subprocess) << "zmq could not send CV_CONST_BUFFER";
            }
        }
    }
}

void Egress::Impl::SchedulerEventHandler(hdtn::IreleaseChangeHdr& releaseChangeHdr) {
    if (releaseChangeHdr.base.type == HDTN_MSGTYPE_ILINKUP) {
        Outduct* outduct = m_outductManager.GetOutductByOutductUuid(releaseChangeHdr.outductArrayIndex);
        if (!outduct) {
            LOG_ERROR(subprocess) << "could not find outduct from scheduler link up event; not adjusting rate";
            return;
        }
        outduct->m_linkIsUpPerTimeSchedule = true;
        const uint64_t startingRateBitsPerSec = outduct->GetStartingMaxSendRateBitsPerSec();
        if (startingRateBitsPerSec == 0) {
            // If the starting rate is 0 ("unlimited"), never override it
            LOG_INFO(subprocess) << "not updating max rate for contact. max rate was initiliazed to 0 (unlimited) "
                << "or isn't supported by convergence layer";
            return;
        }

        LOG_INFO(subprocess) << "setting rate to " << releaseChangeHdr.rateBps << " bps for new contact";
        outduct->SetRate(releaseChangeHdr.rateBps);
    }
    else if (releaseChangeHdr.base.type == HDTN_MSGTYPE_ILINKDOWN) {
        Outduct* outduct = m_outductManager.GetOutductByOutductUuid(releaseChangeHdr.outductArrayIndex);
        if (!outduct) {
            LOG_ERROR(subprocess) << "could not find outduct from scheduler link down event";
            return;
        }
        outduct->m_linkIsUpPerTimeSchedule = false;
    }
}

}  // namespace hdtn
