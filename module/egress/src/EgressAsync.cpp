/***************************************************************************
 * NASA Glenn Research Center, Cleveland, OH
 * Released under the NASA Open Source Agreement (NOSA)
 * May  2021
 ****************************************************************************
 */

#include <string.h>

#include "EgressAsync.h"
#include <boost/lexical_cast.hpp>
#include <boost/make_unique.hpp>
#include "Uri.h"

#include "SignalHandler.h"
#include <fstream>
#include <iostream>
#include "message.hpp"
#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/date_time.hpp>
#include <memory>
#include <fstream>
#include <sstream>

hdtn::HegrManagerAsync::HegrManagerAsync() : m_running(false) {
    //m_flags = 0;
    //_next = NULL;
}

hdtn::HegrManagerAsync::~HegrManagerAsync() {
    Stop();
}

void hdtn::HegrManagerAsync::Stop() {
    m_running = false;
    if(m_threadZmqReaderPtr) {
        m_threadZmqReaderPtr->join();
        m_threadZmqReaderPtr.reset(); //delete it
    }
}

void hdtn::HegrManagerAsync::Init(const HdtnConfig & hdtnConfig, zmq::context_t * hdtnOneProcessZmqInprocContextPtr) {
    
    if (m_running) {
        std::cerr << "error: HegrManagerAsync::Init called while Egress is already running" << std::endl;
        return;
    }

    m_hdtnConfig = hdtnConfig;

    if (!m_outductManager.LoadOutductsFromConfig(m_hdtnConfig.m_outductsConfig, m_hdtnConfig.m_myNodeId, m_hdtnConfig.m_maxLtpReceiveUdpPacketSizeBytes, m_hdtnConfig.m_maxBundleSizeBytes,
        boost::bind(&hdtn::HegrManagerAsync::WholeBundleReadyCallback, this, boost::placeholders::_1),
        OnFailedBundleVecSendCallback_t(), //egress only sends zmq bundles (not vec8) so this will never be needed
        //boost::bind(&hdtn::HegrManagerAsync::OnFailedBundleVecSendCallback, this, boost::placeholders::_1, boost::placeholders::_2, boost::placeholders::_3),
        boost::bind(&hdtn::HegrManagerAsync::OnFailedBundleZmqSendCallback, this, boost::placeholders::_1, boost::placeholders::_2, boost::placeholders::_3),
        boost::bind(&hdtn::HegrManagerAsync::OnSuccessfulBundleSendCallback, this, boost::placeholders::_1, boost::placeholders::_2),
        boost::bind(&hdtn::HegrManagerAsync::OnOutductLinkStatusChangedCallback, this, boost::placeholders::_1, boost::placeholders::_2)
    ))
    {
        return;
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
        m_zmqPubSock_boundEgressToConnectingSchedulerPtr = boost::make_unique<zmq::socket_t>(*m_zmqCtxPtr, zmq::socket_type::pub);
        const std::string bind_boundEgressToConnectingSchedulerPath(
            std::string("tcp://*:") + boost::lexical_cast<std::string>(m_hdtnConfig.m_zmqConnectingEgressToBoundSchedulerPortPath));
        m_zmqPubSock_boundEgressToConnectingSchedulerPtr->bind(bind_boundEgressToConnectingSchedulerPath);

        //Caution: All options, with the exception of ZMQ_SUBSCRIBE, ZMQ_UNSUBSCRIBE and ZMQ_LINGER, only take effect for subsequent socket bind/connects.
        //The value of 0 specifies no linger period. Pending messages shall be discarded immediately when the socket is closed with zmq_close().
        m_zmqRepSock_connectingGuiToFromBoundEgressPtr->set(zmq::sockopt::linger, 0); //prevent hang when deleting the zmqCtxPtr
        m_zmqPushSock_connectingEgressBundlesOnlyToBoundIngressPtr->set(zmq::sockopt::linger, 0); //prevent hang when deleting the zmqCtxPtr
        m_zmqPushSock_boundEgressToConnectingStoragePtr->set(zmq::sockopt::linger, 0); //prevent hang when deleting the zmqCtxPtr
        m_zmqPushSock_connectingEgressToBoundIngressPtr->set(zmq::sockopt::linger, 0); //prevent hang when deleting the zmqCtxPtr
        m_zmqPubSock_boundEgressToConnectingSchedulerPtr->set(zmq::sockopt::linger, 0); //prevent hang when deleting the zmqCtxPtr

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
            std::cout << "Egress connected and listening to events from Router " << connect_boundRouterPubSubPath << std::endl;
        }
        catch (const zmq::error_t & ex) {
            std::cerr << "error: egress cannot connect to router socket: " << ex.what() << std::endl;
            return;
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
        std::cerr << "error: egress cannot set up zmq socket: " << ex.what() << std::endl;
        return;
    }
    if (!m_running) {
        m_running = true;
        m_threadZmqReaderPtr = boost::make_unique<boost::thread>(
            boost::bind(&HegrManagerAsync::ReadZmqThreadFunc, this)); //create and start the worker thread
    }
}

void hdtn::HegrManagerAsync::DoLinkStatusUpdate(bool isLinkDownEvent, uint64_t outductUuid) {
    //TODO PREVENT DUPLICATE MESSAGES
    std::cout << "[Egress] Sending LinkStatus update event to Scheduler" << std::endl;

    hdtn::LinkStatusHdr linkStatusMsg;
    memset(&linkStatusMsg, 0, sizeof(hdtn::LinkStatusHdr));
    linkStatusMsg.base.type = HDTN_MSGTYPE_LINKSTATUS;
    linkStatusMsg.event = (isLinkDownEvent) ? 0 : 1;
    linkStatusMsg.uuid = outductUuid;

    {
        boost::mutex::scoped_lock lock(m_mutexLinkStatusUpdate);
        m_zmqPubSock_boundEgressToConnectingSchedulerPtr->send(zmq::const_buffer(&linkStatusMsg, sizeof(hdtn::LinkStatusHdr)), zmq::send_flags::none);
    }
    
}

static void CustomCleanupEgressAckHdrNoHint(void *data, void *hint) {
    (void)hint;
    //std::cout << "free " << static_cast<hdtn::BlockHdr *>(data)->flowId << std::endl;
    delete static_cast<hdtn::EgressAckHdr *>(data);
}

static void CustomCleanupStdVecUint8(void* data, void* hint) {
    //std::cout << "free " << static_cast<std::vector<uint8_t>*>(hint)->size() << std::endl;
    delete static_cast<std::vector<uint8_t>*>(hint);
}

void hdtn::HegrManagerAsync::RouterEventHandler() {
    zmq::message_t message;
    if (!m_zmqSubSock_boundRouterToConnectingEgressPtr->recv(message, zmq::recv_flags::none)) {
    std::cerr << "Egress didn't receive event from Router process" << std::endl;
        return;
    }
    if (message.size() < sizeof(CommonHdr)) {
        return;
    }
    CommonHdr *common = (CommonHdr *)message.data();
    if (common->type == HDTN_MSGTYPE_ROUTEUPDATE) {
        hdtn::RouteUpdateHdr * routeUpdateHdr = (hdtn::RouteUpdateHdr *)message.data();
        if (m_outductManager.Reroute_ThreadSafe(routeUpdateHdr->finalDestNodeId, routeUpdateHdr->nextHopNodeId)) {
            std::cout << "[Egress] Updated the outduct based on the optimal Route for finalDestNodeId " << routeUpdateHdr->finalDestNodeId
                << ": New Outduct Next Hop is " << routeUpdateHdr->nextHopNodeId << std::endl;
        }
        else {
            std::cout << "[Egress] Failed to update the outduct based on the optimal Route for finalDestNodeId " << routeUpdateHdr->finalDestNodeId
                << " to a next hop outduct node id " << routeUpdateHdr->nextHopNodeId << std::endl;
        }
    }
}

void hdtn::HegrManagerAsync::ReadZmqThreadFunc() {

    while (m_running) {
        std::cout << "Egress Waiting for Outduct to become ready to forward..." << std::endl;
        boost::this_thread::sleep(boost::posix_time::milliseconds(1000));
        if (m_outductManager.AllReadyToForward()) {
            std::cout << "Outduct ready to forward" << std::endl;
            break;
        }
    }
    if (!m_running) {
        std::cout << "Egress Terminated before all outducts could connect" << std::endl;
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

    static const long DEFAULT_BIG_TIMEOUT_POLL = 250;
    while (m_running) { //keep thread alive if running
        int rc = 0;
        try {
            rc = zmq::poll(&items[0], NUM_SOCKETS, DEFAULT_BIG_TIMEOUT_POLL);
        }
        catch (zmq::error_t & e) {
            std::cout << "caught zmq::error_t in hdtn::HegrManagerAsync::ReadZmqThreadFunc: " << e.what() << std::endl;
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
                    std::cerr << "error in HegrManagerAsync::ReadZmqThreadFunc: cannot read BlockHdr" << std::endl;
                    hdtn::Logger::getInstance()->logError("egress", "Error in HegrManagerAsync::ReadZmqThreadFunc: cannot read BlockHdr");
                    continue;
                }
                else if ((res->truncated()) || (res->size != sizeof(hdtn::ToEgressHdr))) {
                    std::cerr << "egress blockhdr message mismatch: untruncated = " << res->untruncated_size 
                        << " truncated = " << res->size << " expected = " << sizeof(hdtn::ToEgressHdr) << std::endl;
                    hdtn::Logger::getInstance()->logError("egress", "Egress blockhdr message mismatch: untruncated = " + 
                        std::to_string(res->untruncated_size) +  " truncated = " + std::to_string(res->size) + " expected = " + 
                        std::to_string(sizeof(hdtn::ToEgressHdr)));
                    continue;
                }
                else if ((itemIndex == 0) && (toEgressHeader.base.type == HDTN_MSGTYPE_EGRESS_ADD_OPPORTUNISTIC_LINK)) {
                    std::cout << "egress adding opportunistic link " << toEgressHeader.finalDestEid.nodeId << std::endl;
                    availableDestOpportunisticNodeIdsSet.insert(toEgressHeader.finalDestEid.nodeId);
                    continue;
                }
                else if ((itemIndex == 0) && (toEgressHeader.base.type == HDTN_MSGTYPE_EGRESS_REMOVE_OPPORTUNISTIC_LINK)) {
                    std::cout << "egress removing opportunistic link " << toEgressHeader.finalDestEid.nodeId << std::endl;
                    availableDestOpportunisticNodeIdsSet.erase(toEgressHeader.finalDestEid.nodeId);
                    continue;
                }
                else if (toEgressHeader.base.type != HDTN_MSGTYPE_EGRESS) {
                    std::cerr << "error: toEgressHeader.base.type != HDTN_MSGTYPE_EGRESS\n";
                    continue;
                }
                else if ((itemIndex == 1) && (toEgressHeader.isCutThroughFromIngress)) {
                    std::cerr << "error: received on storage socket but cut through flag set\n";
                    continue;
                }
                else if ((itemIndex == 0) && (!toEgressHeader.isCutThroughFromIngress)) {
                    std::cerr << "error: received on ingress socket but cut through flag not set\n";
                    continue;
                }
                ++m_telemetry.egressMessageCount;
                
                zmq::message_t zmqMessageBundle;
                //message guaranteed to be there due to the zmq::send_flags::sndmore
                if (!firstTwoSockets[itemIndex]->recv(zmqMessageBundle, zmq::recv_flags::none)) {
                    std::cerr << "error on sockets[itemIndex]->recv\n";
                    continue;
                }
                       
                m_telemetry.egressBundleData += zmqMessageBundle.size();
                ++m_telemetry.egressBundleCount;

                const cbhe_eid_t & finalDestEid = toEgressHeader.finalDestEid;
                if ((itemIndex == 1) && availableDestOpportunisticNodeIdsSet.count(finalDestEid.nodeId)) { //from storage and opportunistic link available in ingress
                    hdtn::EgressAckHdr * egressAckPtr = new hdtn::EgressAckHdr();
                    //memset 0 not needed because all values set below
                    egressAckPtr->base.type = HDTN_MSGTYPE_EGRESS_ACK_TO_STORAGE;
                    egressAckPtr->base.flags = 0;
                    egressAckPtr->finalDestEid = finalDestEid;
                    egressAckPtr->error = 0; //can set later before sending this ack if error
                    egressAckPtr->deleteNow = !toEgressHeader.hasCustody;
                    egressAckPtr->isToStorage = 1;
                    egressAckPtr->custodyId = toEgressHeader.custodyId;

                    zmq::message_t messageWithDataStolen(egressAckPtr, sizeof(hdtn::EgressAckHdr), CustomCleanupEgressAckHdrNoHint); //storage can be acked right away since bundle transferred
                    {
                        boost::mutex::scoped_lock lock(m_mutex_zmqPushSock_boundEgressToConnectingStorage);
                        if (!m_zmqPushSock_boundEgressToConnectingStoragePtr->send(std::move(messageWithDataStolen), zmq::send_flags::dontwait)) {
                            std::cout << "error: m_zmqPushSock_boundEgressToConnectingStoragePtr could not send" << std::endl;
                            break;
                        }
                        ++m_totalCustodyTransfersSentToStorage;
                    }

                    boost::mutex::scoped_lock lock(m_mutexPushBundleToIngress);
                    static const char messageFlags = 0; //0 => from storage and needs no processing
                    static const zmq::const_buffer messageFlagsConstBuf(&messageFlags, sizeof(messageFlags));
                    if (!m_zmqPushSock_connectingEgressBundlesOnlyToBoundIngressPtr->send(messageFlagsConstBuf, zmq::send_flags::sndmore)) { //blocks if above 5 high water mark
                        std::cout << "error in egress WholeBundleReadyCallback: zmq could not send messageFlagsConstBuf to ingress" << std::endl;
                    }
                    else if (!m_zmqPushSock_connectingEgressBundlesOnlyToBoundIngressPtr->send(std::move(zmqMessageBundle), zmq::send_flags::none)) { //blocks if above 5 high water mark
                        std::cout << "error in egress WholeBundleReadyCallback: zmq could not forward bundle to ingress" << std::endl;
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
                    egressAckPtr->deleteNow = !toEgressHeader.hasCustody;
                    egressAckPtr->isToStorage = !toEgressHeader.isCutThroughFromIngress;
                    egressAckPtr->custodyId = toEgressHeader.custodyId;
                    //std::cout << "*****Egress Outduct: " << static_cast<int>(outduct->GetOutductUuid()) << std::endl;
                    outduct->Forward(zmqMessageBundle, std::move(userData));
                    if (zmqMessageBundle.size() != 0) {
                        std::cout << "Error in hdtn::HegrManagerAsync::ProcessZmqMessagesThreadFunc, zmqMessage was not moved.. bundle shall remain in storage" << std::endl;

                        OnFailedBundleZmqSendCallback(zmqMessageBundle, userData, outduct->GetOutductUuid()); //todo is this correct?.. verify userdata not moved
                    }
                }
                else {
                    std::cerr << "critical error in HegrManagerAsync::ProcessZmqMessagesThreadFunc: no outduct for " 
                        << Uri::GetIpnUriString(finalDestEid.nodeId, finalDestEid.serviceId) << std::endl;
                }

            }

            if (items[2].revents & ZMQ_POLLIN) { //events from Router
                std::cout << "[Egress] Received RouteUpdate event!!" << std::endl;
                RouterEventHandler();
            }

            if (items[3].revents & ZMQ_POLLIN) { //gui requests data
                uint8_t guiMsgByte;
                const zmq::recv_buffer_result_t res = m_zmqRepSock_connectingGuiToFromBoundEgressPtr->recv(zmq::mutable_buffer(&guiMsgByte, sizeof(guiMsgByte)), zmq::recv_flags::dontwait);
                if (!res) {
                    std::cerr << "error in egress ReadZmqThreadFunc: cannot read guiMsgByte" << std::endl;
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
                    //std::cout << "egress send telem\n";

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
                        std::cerr << "egress can't send telemetry to gui" << std::endl;
                    }
                }
            }
        }
    }

    std::cout << "HegrManagerAsync::ReadZmqThreadFunc thread exiting\n";
    hdtn::Logger::getInstance()->logNotification("egress", "HegrManagerAsync::ReadZmqThreadFunc thread exiting");
    const std::string msgToStorage = "m_totalCustodyTransfersSentToStorage: " + boost::lexical_cast<std::string>(m_totalCustodyTransfersSentToStorage);
    std::cout << msgToStorage << std::endl;
    hdtn::Logger::getInstance()->logInfo("egress", msgToStorage);
    const std::string msgToIngress = "m_totalCustodyTransfersSentToIngress: " + boost::lexical_cast<std::string>(m_totalCustodyTransfersSentToIngress);
    std::cout << msgToIngress << std::endl;
    hdtn::Logger::getInstance()->logInfo("egress", msgToIngress);
}

static void CustomCleanupPaddedVecUint8(void *data, void *hint) {
    //std::cout << "free " << static_cast<std::vector<uint8_t>*>(hint)->size() << std::endl;
    delete static_cast<padded_vector_uint8_t*>(hint);
}

//from egress bidirectional tcpcl outduct receive path and opportunistic link potentially available in ingress (otherwise ingress will give it to storage if unavailable)
void hdtn::HegrManagerAsync::WholeBundleReadyCallback(padded_vector_uint8_t & wholeBundleVec) {
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
        std::cout << "error in egress WholeBundleReadyCallback: zmq could not send messageFlagsConstBuf to ingress" << std::endl;
    }
    else if (!m_zmqPushSock_connectingEgressBundlesOnlyToBoundIngressPtr->send(std::move(paddedMessageWithDataStolen), zmq::send_flags::none)) { //blocks if above 5 high water mark
        std::cout << "error in egress WholeBundleReadyCallback: zmq could not forward bundle to ingress" << std::endl;
    }
}

void hdtn::HegrManagerAsync::OnFailedBundleZmqSendCallback(zmq::message_t& movableBundle, std::vector<uint8_t>& userData, uint64_t outductUuid) {
    //std::cout << "OnFailedBundleZmqSendCallback size " << movableBundle.size() << " outductUuid " << outductUuid << "\n";
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
                std::cout << "error: zmq could not send ingress an ack from egress" << std::endl;
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
                std::cout << "error: m_zmqPushSock_boundEgressToConnectingStoragePtr could not send HDTN_MSGTYPE_EGRESS_FAILED_BUNDLE_TO_STORAGE" << std::endl;
            }
            else if (!m_zmqPushSock_boundEgressToConnectingStoragePtr->send(std::move(movableBundle), zmq::send_flags::dontwait)) {
                std::cout << "error: m_zmqPushSock_boundEgressToConnectingStoragePtr could not send bundle" << std::endl;
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
            std::cout << "error: m_zmqPushSock_boundEgressToConnectingStoragePtr could not send" << std::endl;
        }
        ++m_totalCustodyTransfersSentToStorage;
    }
}
void hdtn::HegrManagerAsync::OnSuccessfulBundleSendCallback(std::vector<uint8_t>& userData, uint64_t outductUuid) {
    //std::cout << "OnSuccessfulBundleSendCallback size " << userData.size() << " outductUuid " << outductUuid << "\n";
    if (userData.empty()) {
        std::cout << "error in HegrManagerAsync::OnSuccessfulBundleSendCallback: userData empty\n";
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
            std::cout << "error: m_zmqPushSock_boundEgressToConnectingStoragePtr could not send" << std::endl;
        }
        ++m_totalCustodyTransfersSentToStorage;
    }
    else {
        //send ack message by echoing back the block
        boost::mutex::scoped_lock lock(m_mutex_zmqPushSock_connectingEgressToBoundIngress);
        if (!m_zmqPushSock_connectingEgressToBoundIngressPtr->send(std::move(zmqUserDataMessageWithDataStolen), zmq::send_flags::dontwait)) {
            std::cout << "error: zmq could not send ingress an ack from egress" << std::endl;
        }
        ++m_totalCustodyTransfersSentToIngress;
    }
}
void hdtn::HegrManagerAsync::OnOutductLinkStatusChangedCallback(bool isLinkDownEvent, uint64_t outductUuid) {
    //std::cout << "OnOutductLinkStatusChangedCallback isLinkDownEvent:" << isLinkDownEvent << " outductUuid " << outductUuid << "\n";
    DoLinkStatusUpdate(isLinkDownEvent, outductUuid);
}
