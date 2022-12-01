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

namespace hdtn {

static constexpr hdtn::Logger::SubProcess subprocess = hdtn::Logger::SubProcess::egress;

struct Egress::Impl : private boost::noncopyable {

    Impl();
    ~Impl();
    void Stop();
    bool Init(const HdtnConfig& hdtnConfig, zmq::context_t* hdtnOneProcessZmqInprocContextPtr);

private:
    void RouterEventHandler();
    void ReadZmqThreadFunc();
    void WholeBundleReadyCallback(padded_vector_uint8_t& wholeBundleVec);
    void OnFailedBundleZmqSendCallback(zmq::message_t& movableBundle, std::vector<uint8_t>& userData, uint64_t outductUuid);
    void OnSuccessfulBundleSendCallback(std::vector<uint8_t>& userData, uint64_t outductUuid);
    void OnOutductLinkStatusChangedCallback(bool isLinkDownEvent, uint64_t outductUuid);
    void ResendOutductCapabilities();
    void DoLinkStatusUpdate(bool isLinkDownEvent, uint64_t outductUuid);

public:
    //telemetry
    EgressTelemetry_t m_telemetry;
    std::size_t m_totalCustodyTransfersSentToStorage;
    std::size_t m_totalCustodyTransfersSentToIngress;
private:
    std::unique_ptr<zmq::context_t> m_zmqCtxPtr;
    std::unique_ptr<zmq::socket_t> m_zmqPullSock_boundIngressToConnectingEgressPtr;
    std::unique_ptr<zmq::socket_t> m_zmqPushSock_connectingEgressToBoundIngressPtr;
    boost::mutex m_mutex_zmqPushSock_connectingEgressToBoundIngress;
    std::unique_ptr<zmq::socket_t> m_zmqPushSock_connectingEgressBundlesOnlyToBoundIngressPtr;
    std::unique_ptr<zmq::socket_t> m_zmqPullSock_connectingStorageToBoundEgressPtr;
    std::unique_ptr<zmq::socket_t> m_zmqPushSock_boundEgressToConnectingStoragePtr;
    boost::mutex m_mutex_zmqPushSock_boundEgressToConnectingStorage;
    std::unique_ptr<zmq::socket_t> m_zmqSubSock_boundRouterToConnectingEgressPtr;
    std::unique_ptr<zmq::socket_t> m_zmqPushSock_boundEgressToConnectingSchedulerPtr;

    std::unique_ptr<zmq::socket_t> m_zmqRepSock_connectingGuiToFromBoundEgressPtr;

    OutductManager m_outductManager;
    HdtnConfig m_hdtnConfig;

    boost::mutex m_mutexPushBundleToIngress;
    boost::mutex m_mutexLinkStatusUpdate;

    std::unique_ptr<boost::thread> m_threadZmqReaderPtr;
    volatile bool m_running;
};

Egress::Impl::Impl() : m_running(false) {
    //m_flags = 0;
    //_next = NULL;
}
Egress::Egress() :
    m_pimpl(boost::make_unique<Egress::Impl>()),
    //references
    m_telemetry(m_pimpl->m_telemetry),
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
        m_threadZmqReaderPtr->join();
        m_threadZmqReaderPtr.reset(); //delete it
    }
}

bool Egress::Init(const HdtnConfig& hdtnConfig, zmq::context_t* hdtnOneProcessZmqInprocContextPtr) {
    return m_pimpl->Init(hdtnConfig, hdtnOneProcessZmqInprocContextPtr);
}
bool Egress::Impl::Init(const HdtnConfig & hdtnConfig, zmq::context_t * hdtnOneProcessZmqInprocContextPtr) {
    
    if (m_running) {
        LOG_ERROR(subprocess) << "HegrManagerAsync::Init called while Egress is already running";
        return false;
    }

    m_hdtnConfig = hdtnConfig;

    if (!m_outductManager.LoadOutductsFromConfig(m_hdtnConfig.m_outductsConfig, m_hdtnConfig.m_myNodeId, m_hdtnConfig.m_maxLtpReceiveUdpPacketSizeBytes, m_hdtnConfig.m_maxBundleSizeBytes,
        boost::bind(&Egress::Impl::WholeBundleReadyCallback, this, boost::placeholders::_1),
        OnFailedBundleVecSendCallback_t(), //egress only sends zmq bundles (not vec8) so this will never be needed
        //boost::bind(&hdtn::HegrManagerAsync::OnFailedBundleVecSendCallback, this, boost::placeholders::_1, boost::placeholders::_2, boost::placeholders::_3),
        boost::bind(&Egress::Impl::OnFailedBundleZmqSendCallback, this, boost::placeholders::_1, boost::placeholders::_2, boost::placeholders::_3),
        boost::bind(&Egress::Impl::OnSuccessfulBundleSendCallback, this, boost::placeholders::_1, boost::placeholders::_2),
        boost::bind(&Egress::Impl::OnOutductLinkStatusChangedCallback, this, boost::placeholders::_1, boost::placeholders::_2)
    ))
    {
        return false;
    }

    
    m_telemetry.egressBundleCount = 0;
    m_telemetry.egressBundleData = 0;
    m_telemetry.egressMessageCount = 0;

    m_totalCustodyTransfersSentToStorage = 0;
    m_totalCustodyTransfersSentToIngress = 0;

    m_zmqCtxPtr = boost::make_unique<zmq::context_t>(); //needed at least by router (and if one-process is not used)
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

            //from gui socket
            m_zmqRepSock_connectingGuiToFromBoundEgressPtr = boost::make_unique<zmq::socket_t>(*hdtnOneProcessZmqInprocContextPtr, zmq::socket_type::pair);
            m_zmqRepSock_connectingGuiToFromBoundEgressPtr->bind(std::string("inproc://connecting_gui_to_from_bound_egress"));
        }
        else {
            // socket for cut-through mode straight to egress
            m_zmqPullSock_boundIngressToConnectingEgressPtr = boost::make_unique<zmq::socket_t>(*m_zmqCtxPtr, zmq::socket_type::pull);
            const std::string connect_boundIngressToConnectingEgressPath(
                std::string("tcp://") +
                m_hdtnConfig.m_zmqIngressAddress +
                std::string(":") +
                boost::lexical_cast<std::string>(m_hdtnConfig.m_zmqBoundIngressToConnectingEgressPortPath));
            m_zmqPullSock_boundIngressToConnectingEgressPtr->connect(connect_boundIngressToConnectingEgressPath);
            
            m_zmqPushSock_connectingEgressToBoundIngressPtr = boost::make_unique<zmq::socket_t>(*m_zmqCtxPtr, zmq::socket_type::push);
            const std::string connect_connectingEgressToBoundIngressPath(
                std::string("tcp://") +
                m_hdtnConfig.m_zmqIngressAddress +
                std::string(":") +
                boost::lexical_cast<std::string>(m_hdtnConfig.m_zmqConnectingEgressToBoundIngressPortPath));
            m_zmqPushSock_connectingEgressToBoundIngressPtr->connect(connect_connectingEgressToBoundIngressPath);
            // socket for sending bundles from egress via tcpcl outduct opportunistic link (because tcpcl can be bidirectional)
            m_zmqPushSock_connectingEgressBundlesOnlyToBoundIngressPtr = boost::make_unique<zmq::socket_t>(*m_zmqCtxPtr, zmq::socket_type::push);
            const std::string connect_connectingEgressBundlesOnlyToBoundIngressPath(
                std::string("tcp://") +
                m_hdtnConfig.m_zmqIngressAddress +
                std::string(":") +
                boost::lexical_cast<std::string>(m_hdtnConfig.m_zmqConnectingEgressBundlesOnlyToBoundIngressPortPath));
            m_zmqPushSock_connectingEgressBundlesOnlyToBoundIngressPtr->connect(connect_connectingEgressBundlesOnlyToBoundIngressPath);
            // socket for bundles from storage
            m_zmqPullSock_connectingStorageToBoundEgressPtr = boost::make_unique<zmq::socket_t>(*m_zmqCtxPtr, zmq::socket_type::pull);
            const std::string bind_connectingStorageToBoundEgressPath(
                std::string("tcp://*:") + boost::lexical_cast<std::string>(m_hdtnConfig.m_zmqConnectingStorageToBoundEgressPortPath));
            m_zmqPullSock_connectingStorageToBoundEgressPtr->bind(bind_connectingStorageToBoundEgressPath);
            m_zmqPushSock_boundEgressToConnectingStoragePtr = boost::make_unique<zmq::socket_t>(*m_zmqCtxPtr, zmq::socket_type::push);
            const std::string bind_boundEgressToConnectingStoragePath(
                std::string("tcp://*:") + boost::lexical_cast<std::string>(m_hdtnConfig.m_zmqBoundEgressToConnectingStoragePortPath));
            m_zmqPushSock_boundEgressToConnectingStoragePtr->bind(bind_boundEgressToConnectingStoragePath);

            //from gui socket
            m_zmqRepSock_connectingGuiToFromBoundEgressPtr = boost::make_unique<zmq::socket_t>(*m_zmqCtxPtr, zmq::socket_type::rep);
            const std::string bind_connectingGuiToFromBoundEgressPath("tcp://*:10302");
            m_zmqRepSock_connectingGuiToFromBoundEgressPtr->bind(bind_connectingGuiToFromBoundEgressPath);
        }

        //socket for sending LinkStatus events from Egress to Scheduler
        m_zmqPushSock_boundEgressToConnectingSchedulerPtr = boost::make_unique<zmq::socket_t>(*m_zmqCtxPtr, zmq::socket_type::push);
        const std::string bind_boundEgressToConnectingSchedulerPath(
            std::string("tcp://*:") + boost::lexical_cast<std::string>(m_hdtnConfig.m_zmqConnectingEgressToBoundSchedulerPortPath));
        m_zmqPushSock_boundEgressToConnectingSchedulerPtr->bind(bind_boundEgressToConnectingSchedulerPath);

        //Caution: All options, with the exception of ZMQ_SUBSCRIBE, ZMQ_UNSUBSCRIBE and ZMQ_LINGER, only take effect for subsequent socket bind/connects.
        //The value of 0 specifies no linger period. Pending messages shall be discarded immediately when the socket is closed with zmq_close().
        m_zmqRepSock_connectingGuiToFromBoundEgressPtr->set(zmq::sockopt::linger, 0); //prevent hang when deleting the zmqCtxPtr
        m_zmqPushSock_connectingEgressBundlesOnlyToBoundIngressPtr->set(zmq::sockopt::linger, 0); //prevent hang when deleting the zmqCtxPtr
        m_zmqPushSock_boundEgressToConnectingStoragePtr->set(zmq::sockopt::linger, 0); //prevent hang when deleting the zmqCtxPtr
        m_zmqPushSock_connectingEgressToBoundIngressPtr->set(zmq::sockopt::linger, 0); //prevent hang when deleting the zmqCtxPtr
        m_zmqPushSock_boundEgressToConnectingSchedulerPtr->set(zmq::sockopt::linger, 0); //prevent hang when deleting the zmqCtxPtr

        // socket for receiving events from router
        m_zmqSubSock_boundRouterToConnectingEgressPtr = boost::make_unique<zmq::socket_t>(*m_zmqCtxPtr, zmq::socket_type::sub);
        //const std::string connect_boundRouterPubSubPath(std::string("tcp://localhost:10210"));

        const std::string connect_boundRouterPubSubPath(
            std::string("tcp://") +
            m_hdtnConfig.m_zmqRouterAddress +
            std::string(":") +
            boost::lexical_cast<std::string>(m_hdtnConfig.m_zmqBoundRouterPubSubPortPath));
        try {
            m_zmqSubSock_boundRouterToConnectingEgressPtr->connect(connect_boundRouterPubSubPath);
            m_zmqSubSock_boundRouterToConnectingEgressPtr->set(zmq::sockopt::subscribe, "");
            LOG_INFO(subprocess) << "Egress connected and listening to events from Router " << connect_boundRouterPubSubPath;
        }
        catch (const zmq::error_t & ex) {
            LOG_ERROR(subprocess) << "error: egress cannot connect to router socket: " << ex.what();
            return false;
        }
        
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
    if (!m_running) {
        m_running = true;
        m_threadZmqReaderPtr = boost::make_unique<boost::thread>(
            boost::bind(&Egress::Impl::ReadZmqThreadFunc, this)); //create and start the worker thread
    }
    return true;
}

void Egress::Impl::DoLinkStatusUpdate(bool isLinkDownEvent, uint64_t outductUuid) {
    //TODO PREVENT DUPLICATE MESSAGES
    LOG_INFO(subprocess) << "Sending LinkStatus update event to Scheduler";

    hdtn::LinkStatusHdr linkStatusMsg;
    memset(&linkStatusMsg, 0, sizeof(hdtn::LinkStatusHdr));
    linkStatusMsg.base.type = HDTN_MSGTYPE_LINKSTATUS;
    linkStatusMsg.event = (isLinkDownEvent) ? 0 : 1;
    linkStatusMsg.uuid = outductUuid;
    linkStatusMsg.unixTimeSecondsSince1970 = TimestampUtil::GetSecondsSinceEpochUnix();

    {
        boost::mutex::scoped_lock lock(m_mutexLinkStatusUpdate);
        m_zmqPushSock_boundEgressToConnectingSchedulerPtr->send(zmq::const_buffer(&linkStatusMsg, sizeof(hdtn::LinkStatusHdr)), zmq::send_flags::none);
    }
    
}

static void CustomCleanupEgressAckHdrNoHint(void *data, void *hint) {
    (void)hint;
    delete static_cast<hdtn::EgressAckHdr *>(data);
}

static void CustomCleanupStdVecUint8(void* data, void* hint) {
    delete static_cast<std::vector<uint8_t>*>(hint);
}
static void CustomCleanupSharedPtrStdVecUint8(void* data, void* hint) {
    std::shared_ptr<std::vector<uint8_t> >* serializedRawPtrToSharedPtr = static_cast<std::shared_ptr<std::vector<uint8_t> > *>(hint);
    delete serializedRawPtrToSharedPtr; //reduce ref count and delete shared_ptr object
}

void Egress::Impl::RouterEventHandler() {
    zmq::message_t message;
    if (!m_zmqSubSock_boundRouterToConnectingEgressPtr->recv(message, zmq::recv_flags::none)) {
    LOG_ERROR(subprocess) << "Egress didn't receive event from Router process";
        return;
    }
    if (message.size() < sizeof(CommonHdr)) {
        return;
    }
    CommonHdr *common = (CommonHdr *)message.data();
    if (common->type == HDTN_MSGTYPE_ROUTEUPDATE) {
        hdtn::RouteUpdateHdr * routeUpdateHdr = (hdtn::RouteUpdateHdr *)message.data();
        if (m_outductManager.Reroute_ThreadSafe(routeUpdateHdr->finalDestNodeId, routeUpdateHdr->nextHopNodeId)) {
            LOG_INFO(subprocess) << "Updated the outduct based on the optimal Route for finalDestNodeId " << routeUpdateHdr->finalDestNodeId
                << ": New Outduct Next Hop is " << routeUpdateHdr->nextHopNodeId;
        }
        else {
            LOG_INFO(subprocess) << "Failed to update the outduct based on the optimal Route for finalDestNodeId " << routeUpdateHdr->finalDestNodeId
                << " to a next hop outduct node id " << routeUpdateHdr->nextHopNodeId;
        }
    }
}

void Egress::Impl::ReadZmqThreadFunc() {

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

    std::set<uint64_t> availableDestOpportunisticNodeIdsSet;

    // Use a form of receive that times out so we can terminate cleanly.
    static const int timeout = 250;  // milliseconds
    static constexpr unsigned int NUM_SOCKETS = 4;

    //THIS PROBABLY DOESNT WORK SINCE IT HAPPENED AFTER BIND/CONNECT BUT NOT USED ANYWAY BECAUSE OF POLLITEMS
    //m_zmqPullSock_boundIngressToConnectingEgressPtr->set(zmq::sockopt::rcvtimeo, timeout);
    //m_zmqPullSock_connectingStorageToBoundEgressPtr->set(zmq::sockopt::rcvtimeo, timeout);

    zmq::pollitem_t items[NUM_SOCKETS] = {
        {m_zmqPullSock_boundIngressToConnectingEgressPtr->handle(), 0, ZMQ_POLLIN, 0},
        {m_zmqPullSock_connectingStorageToBoundEgressPtr->handle(), 0, ZMQ_POLLIN, 0},
        {m_zmqSubSock_boundRouterToConnectingEgressPtr->handle(), 0, ZMQ_POLLIN, 0},
        {m_zmqRepSock_connectingGuiToFromBoundEgressPtr->handle(), 0, ZMQ_POLLIN, 0}
    };
    zmq::socket_t * const firstTwoSockets[2] = {
        m_zmqPullSock_boundIngressToConnectingEgressPtr.get(),
        m_zmqPullSock_connectingStorageToBoundEgressPtr.get()
    };

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
                else if (toEgressHeader.base.type != HDTN_MSGTYPE_EGRESS) {
                    LOG_ERROR(subprocess) << "toEgressHeader.base.type != HDTN_MSGTYPE_EGRESS";
                    continue;
                }
                else if ((itemIndex == 1) && (toEgressHeader.isCutThroughFromIngress)) {
                    LOG_ERROR(subprocess) << "received on storage socket but cut through flag set";
                    continue;
                }
                else if ((itemIndex == 0) && (!toEgressHeader.isCutThroughFromIngress)) {
                    LOG_ERROR(subprocess) << "received on ingress socket but cut through flag not set";
                    continue;
                }
                ++m_telemetry.egressMessageCount;
                
                zmq::message_t zmqMessageBundle;
                //message guaranteed to be there due to the zmq::send_flags::sndmore
                if (!firstTwoSockets[itemIndex]->recv(zmqMessageBundle, zmq::recv_flags::none)) {
                    LOG_ERROR(subprocess) << "error on sockets[itemIndex]->recv";
                    continue;
                }
                       
                m_telemetry.egressBundleData += zmqMessageBundle.size();
                ++m_telemetry.egressBundleCount;

                const cbhe_eid_t & finalDestEid = toEgressHeader.finalDestEid;
                //TODO DERMINE IF availableDestOpportunisticNodeIdsSet IS NEEDED
                if ((itemIndex == 1) && (availableDestOpportunisticNodeIdsSet.count(finalDestEid.nodeId) || toEgressHeader.isOpportunisticFromStorage)) { //from storage and opportunistic link available in ingress
                    hdtn::EgressAckHdr * egressAckPtr = new hdtn::EgressAckHdr();
                    //memset 0 not needed because all values set below
                    egressAckPtr->base.type = HDTN_MSGTYPE_EGRESS_ACK_TO_STORAGE;
                    egressAckPtr->base.flags = 0;
                    egressAckPtr->finalDestEid = finalDestEid;
                    egressAckPtr->error = 0; //can set later before sending this ack if error
                    egressAckPtr->deleteNow = (toEgressHeader.hasCustody == 0);
                    egressAckPtr->isToStorage = 1;
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
                }
                else if (Outduct * outduct = m_outductManager.GetOutductByFinalDestinationEid_ThreadSafe(finalDestEid)) {
                    std::vector<uint8_t> userData(sizeof(hdtn::EgressAckHdr));
                    hdtn::EgressAckHdr* egressAckPtr = (hdtn::EgressAckHdr*)userData.data();
                    //memset 0 not needed because all values set below
                    egressAckPtr->base.type = (toEgressHeader.isCutThroughFromIngress) ? HDTN_MSGTYPE_EGRESS_ACK_TO_INGRESS : HDTN_MSGTYPE_EGRESS_ACK_TO_STORAGE;
                    egressAckPtr->base.flags = 0;
                    egressAckPtr->finalDestEid = finalDestEid;
                    egressAckPtr->error = 0; //can set later before sending this ack if error
                    egressAckPtr->deleteNow = (toEgressHeader.hasCustody == 0);
                    egressAckPtr->isToStorage = (toEgressHeader.isCutThroughFromIngress == 0);
                    egressAckPtr->isResponseToStorageCutThrough = toEgressHeader.isCutThroughFromStorage;
                    egressAckPtr->custodyId = toEgressHeader.custodyId;
                    egressAckPtr->outductIndex = toEgressHeader.outductIndex;
                    outduct->Forward(zmqMessageBundle, std::move(userData));
                    if (zmqMessageBundle.size() != 0) {
                        LOG_ERROR(subprocess) << "hdtn::HegrManagerAsync::ProcessZmqMessagesThreadFunc, zmqMessage was not moved.. bundle shall remain in storage";

                        OnFailedBundleZmqSendCallback(zmqMessageBundle, userData, outduct->GetOutductUuid()); //todo is this correct?.. verify userdata not moved
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

            if (items[3].revents & ZMQ_POLLIN) { //gui requests data
                uint8_t guiMsgByte;
                const zmq::recv_buffer_result_t res = m_zmqRepSock_connectingGuiToFromBoundEgressPtr->recv(zmq::mutable_buffer(&guiMsgByte, sizeof(guiMsgByte)), zmq::recv_flags::dontwait);
                if (!res) {
                    LOG_ERROR(subprocess) << "ReadZmqThreadFunc: cannot read guiMsgByte";
                }
                else if ((res->truncated()) || (res->size != sizeof(guiMsgByte))) {
                    LOG_ERROR(subprocess) << "guiMsgByte message mismatch: untruncated = " << res->untruncated_size
                        << " truncated = " << res->size << " expected = " << sizeof(guiMsgByte);
                }
                else if (guiMsgByte != 1) {
                    LOG_ERROR(subprocess) << "error guiMsgByte not 1";
                }
                else {
                    //send telemetry

                    std::vector<uint8_t>* vecUint8RawPointer = new std::vector<uint8_t>(1000); //will be 64-bit aligned
                    uint8_t* telemPtr = vecUint8RawPointer->data();
                    const uint8_t* const telemSerializationBase = telemPtr;
                    uint64_t telemBufferSize = vecUint8RawPointer->size();

                    //start zmq message with egress telemetry
                    const uint64_t egressTelemSize = m_telemetry.SerializeToLittleEndian(telemPtr, telemBufferSize);
                    telemBufferSize -= egressTelemSize;
                    telemPtr += egressTelemSize;

                    //append all outduct telemetry to same zmq message right after egress telemetry
                    const uint64_t outductTelemSize = m_outductManager.GetAllOutductTelemetry(telemPtr, telemBufferSize);
                    telemBufferSize -= outductTelemSize;
                    telemPtr += outductTelemSize;

                    vecUint8RawPointer->resize(telemPtr - telemSerializationBase);

                    zmq::message_t zmqTelemMessageWithDataStolen(vecUint8RawPointer->data(), vecUint8RawPointer->size(), CustomCleanupStdVecUint8, vecUint8RawPointer);

                    if (!m_zmqRepSock_connectingGuiToFromBoundEgressPtr->send(std::move(zmqTelemMessageWithDataStolen), zmq::send_flags::dontwait)) {
                        LOG_ERROR(subprocess) << "can't send telemetry to gui";
                    }
                }
            }
        }
    }

    LOG_INFO(subprocess) << "HegrManagerAsync::ReadZmqThreadFunc thread exiting";
    LOG_DEBUG(subprocess) << "m_totalCustodyTransfersSentToStorage: " << m_totalCustodyTransfersSentToStorage;
    LOG_DEBUG(subprocess) << "m_totalCustodyTransfersSentToIngress: " << m_totalCustodyTransfersSentToIngress;
}

void Egress::Impl::ResendOutductCapabilities() {
    AllOutductCapabilitiesTelemetry_t allOutductCapabilitiesTelemetry;
    m_outductManager.GetAllOutductCapabilitiesTelemetry_ThreadSafe(allOutductCapabilitiesTelemetry);
    const uint64_t serializationSize = allOutductCapabilitiesTelemetry.GetSerializationSize();
    std::shared_ptr<std::vector<uint8_t> >* serializedRawPtrToSharedPtr = new std::shared_ptr<std::vector<uint8_t> >(std::make_shared<std::vector<uint8_t> >(serializationSize));
    std::vector<uint8_t> & serialized = *(*serializedRawPtrToSharedPtr);
    zmq::message_t zmqMsgToIngress(
        serializedRawPtrToSharedPtr->get()->data(),
        serializedRawPtrToSharedPtr->get()->size(),
        CustomCleanupSharedPtrStdVecUint8,
        serializedRawPtrToSharedPtr);
    if (!allOutductCapabilitiesTelemetry.SerializeToLittleEndian(serialized.data(), serializationSize)) {
        LOG_FATAL(subprocess) << "Cannot serialize outduct capabilities";
        return;
    }
    std::shared_ptr<std::vector<uint8_t> >* serializedRawPtrToSharedPtr2 = new std::shared_ptr<std::vector<uint8_t> >(*serializedRawPtrToSharedPtr); //ref count 2
    zmq::message_t zmqMsgToStorage(
        serializedRawPtrToSharedPtr2->get()->data(),
        serializedRawPtrToSharedPtr2->get()->size(),
        CustomCleanupSharedPtrStdVecUint8,
        serializedRawPtrToSharedPtr2);

    std::shared_ptr<std::vector<uint8_t> >* serializedRawPtrToSharedPtr3 = new std::shared_ptr<std::vector<uint8_t> >(*serializedRawPtrToSharedPtr); //ref count 3
    zmq::message_t zmqMsgToScheduler(
        serializedRawPtrToSharedPtr3->get()->data(),
        serializedRawPtrToSharedPtr3->get()->size(),
        CustomCleanupSharedPtrStdVecUint8,
        serializedRawPtrToSharedPtr3);

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
        if (!m_zmqPushSock_boundEgressToConnectingStoragePtr->send(headerMessageEgressAck, zmq::send_flags::sndmore | zmq::send_flags::dontwait)) {
            LOG_FATAL(subprocess) << "m_zmqPushSock_boundEgressToConnectingStoragePtr could not send outduct capabilities header";
            return;
        }
        if (!m_zmqPushSock_boundEgressToConnectingStoragePtr->send(std::move(zmqMsgToStorage), zmq::send_flags::dontwait)) {
            LOG_FATAL(subprocess) << "m_zmqPushSock_boundEgressToConnectingStoragePtr could not send outduct capabilities";
            return;
        }
    }

    { //ingress
        boost::mutex::scoped_lock lock(m_mutex_zmqPushSock_connectingEgressToBoundIngress);
        if (!m_zmqPushSock_connectingEgressToBoundIngressPtr->send(headerMessageEgressAck, zmq::send_flags::sndmore | zmq::send_flags::dontwait)) {
            LOG_FATAL(subprocess) << "m_zmqPushSock_connectingEgressToBoundIngressPtr could not send outduct capabilities header";
            return;
        }
        if (!m_zmqPushSock_connectingEgressToBoundIngressPtr->send(std::move(zmqMsgToIngress), zmq::send_flags::dontwait)) {
            LOG_FATAL(subprocess) << "m_zmqPushSock_connectingEgressToBoundIngressPtr could not send outduct capabilities";
            return;
        }
    }

    { //scheduler
        boost::mutex::scoped_lock lock(m_mutexLinkStatusUpdate);
        if (!m_zmqPushSock_boundEgressToConnectingSchedulerPtr->send(headerMessageLinkStatus, zmq::send_flags::sndmore | zmq::send_flags::dontwait)) {
            LOG_FATAL(subprocess) << "m_zmqPubSock_boundEgressToConnectingSchedulerPtr could not send outduct capabilities header";
            return;
        }
        if (!m_zmqPushSock_boundEgressToConnectingSchedulerPtr->send(std::move(zmqMsgToScheduler), zmq::send_flags::dontwait)) {
            LOG_FATAL(subprocess) << "m_zmqPubSock_boundEgressToConnectingSchedulerPtr could not send outduct capabilities";
            return;
        }
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
}

void Egress::Impl::OnFailedBundleZmqSendCallback(zmq::message_t& movableBundle, std::vector<uint8_t>& userData, uint64_t outductUuid) {
    DoLinkStatusUpdate(true, outductUuid);

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
        boost::mutex::scoped_lock lock(m_mutex_zmqPushSock_boundEgressToConnectingStorage);
        if (!m_zmqPushSock_boundEgressToConnectingStoragePtr->send(std::move(zmqUserDataMessageWithDataStolen), zmq::send_flags::dontwait)) {
            LOG_ERROR(subprocess) << "m_zmqPushSock_boundEgressToConnectingStoragePtr could not send";
        }
        ++m_totalCustodyTransfersSentToStorage;
    }
}
void Egress::Impl::OnSuccessfulBundleSendCallback(std::vector<uint8_t>& userData, uint64_t outductUuid) {
    if (userData.empty()) {
        LOG_ERROR(subprocess) << "HegrManagerAsync::OnSuccessfulBundleSendCallback: userData empty";
        return;
    }
    
    //this is an optimization because we only have one chunk to send
    //The zmq_msg_init_data() function shall initialise the message object referenced by msg
    //to represent the content referenced by the buffer located at address data, size bytes long.
    //No copy of data shall be performed and 0MQ shall take ownership of the supplied buffer.
    //If provided, the deallocation function ffn shall be called once the data buffer is no longer
    //required by 0MQ, with the data and hint arguments supplied to zmq_msg_init_data().
    std::vector<uint8_t>* vecUint8RawPointerToUserData = new std::vector<uint8_t>(std::move(userData));
    zmq::message_t zmqUserDataMessageWithDataStolen(vecUint8RawPointerToUserData->data(), vecUint8RawPointerToUserData->size(), CustomCleanupStdVecUint8, vecUint8RawPointerToUserData);
    hdtn::EgressAckHdr* egressAckPtr = (hdtn::EgressAckHdr*)vecUint8RawPointerToUserData->data();
    const bool isToStorage = egressAckPtr->isToStorage;
    if (isToStorage) {
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
void Egress::Impl::OnOutductLinkStatusChangedCallback(bool isLinkDownEvent, uint64_t outductUuid) {
    DoLinkStatusUpdate(isLinkDownEvent, outductUuid);
}

}  // namespace hdtn
