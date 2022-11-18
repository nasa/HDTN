/***************************************************************************
 * NASA Glenn Research Center, Cleveland, OH
 * Released under the NASA Open Source Agreement (NOSA)
 * May  2021
 ****************************************************************************
 */

#include "codec/bpv6.h"
#include "ingress.h"
#include "Logger.h"
#include "message.hpp"
#include <boost/bind/bind.hpp>
#include <boost/make_unique.hpp>
#include <boost/lexical_cast.hpp>
#include "Uri.h"
#include "codec/BundleViewV6.h"
#include "codec/BundleViewV7.h"

namespace hdtn {

static constexpr hdtn::Logger::SubProcess subprocess = hdtn::Logger::SubProcess::ingress;

Ingress::EgressToIngressAckingSet::EgressToIngressAckingSet() {
    //By default, unordered_set containers have a max_load_factor of 1.0.
    m_ingressToEgressCustodyIdSet.reserve(500); //TODO
}
std::size_t Ingress::EgressToIngressAckingSet::GetSetSize() const noexcept {
    return m_ingressToEgressCustodyIdSet.size();
}
void Ingress::EgressToIngressAckingSet::PushMove_ThreadSafe(const uint64_t ingressToEgressCustody) {
    boost::mutex::scoped_lock lock(m_mutex);
    m_ingressToEgressCustodyIdSet.emplace(ingressToEgressCustody);
}
bool Ingress::EgressToIngressAckingSet::CompareAndPop_ThreadSafe(const uint64_t ingressToEgressCustody) {
    m_mutex.lock();
    const std::size_t retVal = m_ingressToEgressCustodyIdSet.erase(ingressToEgressCustody);
    m_mutex.unlock();
    return (retVal != 0);
}
void Ingress::EgressToIngressAckingSet::WaitUntilNotifiedOr250MsTimeout(const uint64_t waitWhileSizeGtThisValue) {
    static const boost::posix_time::time_duration DURATION = boost::posix_time::milliseconds(250);
    const boost::posix_time::ptime timeoutExpiry(boost::posix_time::microsec_clock::universal_time() + DURATION);
    boost::mutex::scoped_lock lock(m_mutex);
    //timed_wait Returns: false if the call is returning because the time specified by abs_time was reached, true otherwise. (false=>timeout)
    //wait while (queueIsFull AND hasNotTimedOutYet)
    while ((GetSetSize() > waitWhileSizeGtThisValue) && m_conditionVariable.timed_wait(lock, timeoutExpiry)) {} //lock mutex (above) before checking condition
}
void Ingress::EgressToIngressAckingSet::NotifyAll() {
    m_conditionVariable.notify_all();
}



Ingress::Ingress() :
    m_bundleCountStorage(0),
    m_bundleCountEgress(0),
    m_bundleCount(0),
    m_bundleData(0),
    m_elapsed(0),
    m_eventsTooManyInStorageQueue(0),
    m_eventsTooManyInEgressQueue(0),
    m_running(false),
    m_ingressToEgressNextUniqueIdAtomic(0),
    m_ingressToStorageNextUniqueId(0)
{
}

Ingress::~Ingress() {
    Stop();
}

void Ingress::Stop() {
    m_inductManager.Clear();


    m_running = false; //thread stopping criteria

    if (m_threadZmqAckReaderPtr) {
        m_threadZmqAckReaderPtr->join();
        m_threadZmqAckReaderPtr.reset(); //delete it
    }
    if (m_threadTcpclOpportunisticBundlesFromEgressReaderPtr) {
        m_threadTcpclOpportunisticBundlesFromEgressReaderPtr->join();
        m_threadTcpclOpportunisticBundlesFromEgressReaderPtr.reset(); //delete it
    }


    LOG_INFO(subprocess) << "m_eventsTooManyInStorageQueue: " << m_eventsTooManyInStorageQueue;
}

int Ingress::Init(const HdtnConfig & hdtnConfig, zmq::context_t * hdtnOneProcessZmqInprocContextPtr) {

    if (!m_running) {
        m_running = true;
        m_hdtnConfig = hdtnConfig;
        //according to ION.pdf v4.0.1 on page 100 it says:
        //  Remember that the format for this argument is ipn:element_number.0 and that
        //  the final 0 is required, as custodial/administration service is always service 0.
        //HDTN shall default m_myCustodialServiceId to 0 although it is changeable in the hdtn config json file
        M_HDTN_EID_CUSTODY.Set(m_hdtnConfig.m_myNodeId, m_hdtnConfig.m_myCustodialServiceId);

        M_HDTN_EID_ECHO.Set(m_hdtnConfig.m_myNodeId, m_hdtnConfig.m_myBpEchoServiceId);

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

                //from gui socket
                m_zmqRepSock_connectingGuiToFromBoundIngressPtr = boost::make_unique<zmq::socket_t>(*hdtnOneProcessZmqInprocContextPtr, zmq::socket_type::pair);
                m_zmqRepSock_connectingGuiToFromBoundIngressPtr->bind(std::string("inproc://connecting_gui_to_from_bound_ingress"));

            }
            else {
                // socket for cut-through mode straight to egress
                m_zmqPushSock_boundIngressToConnectingEgressPtr = boost::make_unique<zmq::socket_t>(*m_zmqCtxPtr, zmq::socket_type::push);
                const std::string bind_boundIngressToConnectingEgressPath(
                    std::string("tcp://*:") + boost::lexical_cast<std::string>(m_hdtnConfig.m_zmqBoundIngressToConnectingEgressPortPath));
                m_zmqPushSock_boundIngressToConnectingEgressPtr->bind(bind_boundIngressToConnectingEgressPath);
                // socket for sending bundles to storage
                m_zmqPushSock_boundIngressToConnectingStoragePtr = boost::make_unique<zmq::socket_t>(*m_zmqCtxPtr, zmq::socket_type::push);
                const std::string bind_boundIngressToConnectingStoragePath(
                    std::string("tcp://*:") + boost::lexical_cast<std::string>(m_hdtnConfig.m_zmqBoundIngressToConnectingStoragePortPath));
                m_zmqPushSock_boundIngressToConnectingStoragePtr->bind(bind_boundIngressToConnectingStoragePath);
                // socket for receiving acks from storage
                m_zmqPullSock_connectingStorageToBoundIngressPtr = boost::make_unique<zmq::socket_t>(*m_zmqCtxPtr, zmq::socket_type::pull);
                const std::string bind_connectingStorageToBoundIngressPath(
                    std::string("tcp://*:") + boost::lexical_cast<std::string>(m_hdtnConfig.m_zmqConnectingStorageToBoundIngressPortPath));
                m_zmqPullSock_connectingStorageToBoundIngressPtr->bind(bind_connectingStorageToBoundIngressPath);
                // socket for receiving acks from egress
                m_zmqPullSock_connectingEgressToBoundIngressPtr = boost::make_unique<zmq::socket_t>(*m_zmqCtxPtr, zmq::socket_type::pull);
                const std::string bind_connectingEgressToBoundIngressPath(
                    std::string("tcp://*:") + boost::lexical_cast<std::string>(m_hdtnConfig.m_zmqConnectingEgressToBoundIngressPortPath));
                m_zmqPullSock_connectingEgressToBoundIngressPtr->bind(bind_connectingEgressToBoundIngressPath);
                // socket for receiving bundles from egress via tcpcl outduct opportunistic link (because tcpcl can be bidirectional)
                m_zmqPullSock_connectingEgressBundlesOnlyToBoundIngressPtr = boost::make_unique<zmq::socket_t>(*m_zmqCtxPtr, zmq::socket_type::pull);
                const std::string bind_connectingEgressBundlesOnlyToBoundIngressPath(
                    std::string("tcp://*:") + boost::lexical_cast<std::string>(m_hdtnConfig.m_zmqConnectingEgressBundlesOnlyToBoundIngressPortPath));
                m_zmqPullSock_connectingEgressBundlesOnlyToBoundIngressPtr->bind(bind_connectingEgressBundlesOnlyToBoundIngressPath);

                //from gui socket
                m_zmqRepSock_connectingGuiToFromBoundIngressPtr = boost::make_unique<zmq::socket_t>(*m_zmqCtxPtr, zmq::socket_type::rep);
                const std::string bind_connectingGuiToFromBoundIngressPath("tcp://*:10301");
                m_zmqRepSock_connectingGuiToFromBoundIngressPtr->bind(bind_connectingGuiToFromBoundIngressPath);
                
            }
        }
        catch (const zmq::error_t & ex) {
            LOG_ERROR(subprocess) << "cannot connect bind zmq socket: " << ex.what();
            return 0;
        }

        //Caution: All options, with the exception of ZMQ_SUBSCRIBE, ZMQ_UNSUBSCRIBE and ZMQ_LINGER, only take effect for subsequent socket bind/connects.
        //The value of 0 specifies no linger period. Pending messages shall be discarded immediately when the socket is closed with zmq_close().
        m_zmqRepSock_connectingGuiToFromBoundIngressPtr->set(zmq::sockopt::linger, 0); //prevent hang when deleting the zmqCtxPtr
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
        m_hdtnConfig.m_zmqSchedulerAddress +
        std::string(":") +
        boost::lexical_cast<std::string>(m_hdtnConfig.m_zmqBoundSchedulerPubSubPortPath));
        try {
            m_zmqSubSock_boundSchedulerToConnectingIngressPtr->connect(connect_boundSchedulerPubSubPath);
            m_zmqSubSock_boundSchedulerToConnectingIngressPtr->set(zmq::sockopt::subscribe, "");
            LOG_INFO(subprocess) << "Ingress connected and listening to events from scheduler " << connect_boundSchedulerPubSubPath;
        } catch (const zmq::error_t & ex) {
            LOG_ERROR(subprocess) << "ingress cannot connect to scheduler socket: " << ex.what();
            return 0;
        }
        
        m_threadZmqAckReaderPtr = boost::make_unique<boost::thread>(
            boost::bind(&Ingress::ReadZmqAcksThreadFunc, this)); //create and start the worker thread
        m_threadTcpclOpportunisticBundlesFromEgressReaderPtr = boost::make_unique<boost::thread>(
            boost::bind(&Ingress::ReadTcpclOpportunisticBundlesFromEgressThreadFunc, this)); //create and start the worker thread

        m_inductManager.LoadInductsFromConfig(boost::bind(&Ingress::WholeBundleReadyCallback, this, boost::placeholders::_1), m_hdtnConfig.m_inductsConfig,
            m_hdtnConfig.m_myNodeId, m_hdtnConfig.m_maxLtpReceiveUdpPacketSizeBytes, m_hdtnConfig.m_maxBundleSizeBytes,
            boost::bind(&Ingress::OnNewOpportunisticLinkCallback, this, boost::placeholders::_1, boost::placeholders::_2),
            boost::bind(&Ingress::OnDeletedOpportunisticLinkCallback, this, boost::placeholders::_1));

        LOG_INFO(subprocess) << "Ingress running, allowing up to " << m_hdtnConfig.m_zmqMaxMessagesPerPath << " max zmq messages per path.";
    }
    return 0;
}


void Ingress::ReadZmqAcksThreadFunc() {

    static constexpr unsigned int NUM_SOCKETS = 4;

    zmq::pollitem_t items[NUM_SOCKETS] = {
        {m_zmqPullSock_connectingEgressToBoundIngressPtr->handle(), 0, ZMQ_POLLIN, 0},
        {m_zmqPullSock_connectingStorageToBoundIngressPtr->handle(), 0, ZMQ_POLLIN, 0},
        {m_zmqSubSock_boundSchedulerToConnectingIngressPtr->handle(), 0, ZMQ_POLLIN, 0},
        {m_zmqRepSock_connectingGuiToFromBoundIngressPtr->handle(), 0, ZMQ_POLLIN, 0}
    };
    std::size_t totalAcksFromEgress = 0;
    std::size_t totalAcksFromStorage = 0;

    static const long DEFAULT_BIG_TIMEOUT_POLL = 250;

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
                    LOG_ERROR(subprocess) << "BpIngressSyscall::ReadZmqAcksThreadFunc: cannot read egress BlockHdr ack";
                        "Error in BpIngressSyscall::ReadZmqAcksThreadFunc: cannot read egress BlockHdr ack";
                }
                else if ((res->truncated()) || (res->size != sizeof(hdtn::EgressAckHdr))) {
                    LOG_ERROR(subprocess) << "EgressAckHdr message mismatch: untruncated = " << res->untruncated_size
                        << " truncated = " << res->size << " expected = " << sizeof(hdtn::EgressAckHdr);
                }
                else if (receivedEgressAckHdr.base.type == HDTN_MSGTYPE_EGRESS_ACK_TO_INGRESS) {
                    m_egressAckMapSetMutex.lock();
                    EgressToIngressAckingSet & egressToIngressAckingObj = m_egressAckMapSet[receivedEgressAckHdr.finalDestEid.nodeId];
                    m_egressAckMapSetMutex.unlock();
                    if (receivedEgressAckHdr.error) {
                        //trigger a link down event in ingress more quickly than waiting for scheduler.
                        //egress shall send the failed bundle to storage.
                        m_eidAvailableSetMutex.lock();
                        const bool erased = (m_finalDestNodeIdAvailableSet.erase(receivedEgressAckHdr.finalDestEid.nodeId) != 0); //eid with any service id
                        m_eidAvailableSetMutex.unlock();
                        if (erased) {
                            LOG_INFO(subprocess) << "Ingress got a link down notification from egress for final dest node id " << receivedEgressAckHdr.finalDestEid.nodeId;
                        }
                    }
                    if (egressToIngressAckingObj.CompareAndPop_ThreadSafe(receivedEgressAckHdr.custodyId)) {
                        egressToIngressAckingObj.NotifyAll();
                        ++totalAcksFromEgress;
                    }
                    else {
                        LOG_ERROR(subprocess) << "didn't receive expected egress ack";
                    }

                }
                else if (receivedEgressAckHdr.base.type == HDTN_MSGTYPE_ALL_OUTDUCT_CAPABILITIES_TELEMETRY) {
                    AllOutductCapabilitiesTelemetry_t aoct;
                    uint64_t numBytesTakenToDecode;
                    
                    zmq::message_t zmqMessageOutductTelem;
                    //message guaranteed to be there due to the zmq::send_flags::sndmore
                    if (!m_zmqPullSock_connectingEgressToBoundIngressPtr->recv(zmqMessageOutductTelem, zmq::recv_flags::none)) {
                        LOG_ERROR(subprocess) << "error receiving AllOutductCapabilitiesTelemetry";
                    }
                    else if (!aoct.DeserializeFromLittleEndian((uint8_t*)zmqMessageOutductTelem.data(), numBytesTakenToDecode, zmqMessageOutductTelem.size())) {
                        LOG_ERROR(subprocess) << "error deserializing AllOutductCapabilitiesTelemetry";
                    }
                    else {
                        //std::cout << aoct << std::endl;
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
                    bool needsNotify = false;
                    {
                        boost::mutex::scoped_lock lock(m_storageAckQueueMutex);
                        if (m_storageAckQueue.empty()) {
                            LOG_ERROR(subprocess) << "m_storageAckQueue is empty";
                        }
                        else if (m_storageAckQueue.front() == receivedStorageAck.ingressUniqueId) {
                            m_storageAckQueue.pop();
                            needsNotify = true;
                            ++totalAcksFromStorage;
                        }
                        else {
                            LOG_ERROR(subprocess) << "didn't receive expected storage ack";
                        }
                    }
                    if (needsNotify) {
                        m_conditionVariableStorageAckReceived.notify_all();
                    }
                }
            }
            if (items[2].revents & ZMQ_POLLIN) { //events from Scheduler
                SchedulerEventHandler();
            }
            if (items[3].revents & ZMQ_POLLIN) { //gui requests data
                uint8_t guiMsgByte;
                const zmq::recv_buffer_result_t res = m_zmqRepSock_connectingGuiToFromBoundIngressPtr->recv(zmq::mutable_buffer(&guiMsgByte, sizeof(guiMsgByte)), zmq::recv_flags::dontwait);
                if (!res) {
                    LOG_ERROR(subprocess) << "ReadZmqAcksThreadFunc: cannot read guiMsgByte";
                }
                else if ((res->truncated()) || (res->size != sizeof(guiMsgByte))) {
                    LOG_ERROR(subprocess) << "guiMsgByte message mismatch: untruncated = " << res->untruncated_size
                        << " truncated = " << res->size << " expected = " << sizeof(guiMsgByte);
                }
                else if (guiMsgByte != 1) {
                    LOG_ERROR(subprocess) << "guiMsgByte not 1";
                }
                else {
                    //send telemetry
                    IngressTelemetry_t telem;
                    telem.totalData = static_cast<double>(m_bundleData);
                    telem.bundleCountEgress = m_bundleCountEgress;
                    telem.bundleCountStorage = m_bundleCountStorage;
                    if (!m_zmqRepSock_connectingGuiToFromBoundIngressPtr->send(zmq::const_buffer(&telem, sizeof(telem)), zmq::send_flags::dontwait)) {
                        LOG_ERROR(subprocess) << "can't send telemetry to gui";
                    }
                }
            }
        }
    }
    LOG_INFO(subprocess) << "totalAcksFromEgress: " << totalAcksFromEgress;
    LOG_INFO(subprocess) << "totalAcksFromStorage: " << totalAcksFromStorage;
    LOG_INFO(subprocess) << "m_bundleCountStorage: " << m_bundleCountStorage;
    LOG_INFO(subprocess) << "m_bundleCountEgress: " << m_bundleCountEgress;
    m_bundleCount = m_bundleCountStorage + m_bundleCountEgress;
    LOG_INFO(subprocess) << "m_bundleCount: " << m_bundleCount;
    LOG_DEBUG(subprocess) << "BpIngressSyscall::ReadZmqAcksThreadFunc thread exiting";
}

void Ingress::ReadTcpclOpportunisticBundlesFromEgressThreadFunc() {
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

void Ingress::SchedulerEventHandler() {
    //force this hdtn message struct to be aligned on a 64-byte boundary using zmq::mutable_buffer
    static constexpr std::size_t minBufSizeBytes = sizeof(uint64_t) + ((sizeof(IreleaseStartHdr) > sizeof(IreleaseStopHdr)) ? sizeof(IreleaseStartHdr) : sizeof(IreleaseStopHdr));
    m_schedulerRxBufPtrToStdVec64.resize(minBufSizeBytes / sizeof(uint64_t));
    uint64_t * rxBufRawPtrAlign64 = &m_schedulerRxBufPtrToStdVec64[0];
    const zmq::recv_buffer_result_t res = m_zmqSubSock_boundSchedulerToConnectingIngressPtr->recv(zmq::mutable_buffer(rxBufRawPtrAlign64, minBufSizeBytes), zmq::recv_flags::none);
    if (!res) {
        LOG_ERROR(subprocess) << "SchedulerEventHandler: message not received";
        return;
    }
    else if (res->size < sizeof(hdtn::CommonHdr)) {
        LOG_ERROR(subprocess) << "SchedulerEventHandler: res->size < sizeof(hdtn::CommonHdr)";
        return;
    }

    CommonHdr *common = (CommonHdr *)rxBufRawPtrAlign64;
    if (common->type == HDTN_MSGTYPE_ILINKUP) {
        hdtn::IreleaseStartHdr * iReleaseStartHdr = (hdtn::IreleaseStartHdr *)rxBufRawPtrAlign64;
        if (res->size != sizeof(hdtn::IreleaseStartHdr)) {
            LOG_ERROR(subprocess) << "SchedulerEventHandler: res->size != sizeof(hdtn::IreleaseStartHdr";
            return;
        }
        m_eidAvailableSetMutex.lock();
        //m_finalDestEidAvailableSet.insert(iReleaseStartHdr->finalDestinationEid); //fully qualified eid
        //m_finalDestEidAvailableSet.insert(iReleaseStartHdr->nextHopEid);
        m_finalDestNodeIdAvailableSet.insert(iReleaseStartHdr->finalDestinationNodeId); //eid with any service id
        m_finalDestNodeIdAvailableSet.insert(iReleaseStartHdr->nextHopNodeId);
        m_eidAvailableSetMutex.unlock();
        LOG_INFO(subprocess) << "Ingress sending bundles to egress for finalDestinationEid: "
            << Uri::GetIpnUriStringAnyServiceNumber(iReleaseStartHdr->finalDestinationNodeId);
    }
    else if (common->type == HDTN_MSGTYPE_ILINKDOWN) {
        hdtn::IreleaseStopHdr * iReleaseStopHdr = (hdtn::IreleaseStopHdr *)rxBufRawPtrAlign64;
        if (res->size != sizeof(hdtn::IreleaseStopHdr)) {
            LOG_ERROR(subprocess) << "SchedulerEventHandler: res->size != sizeof(hdtn::IreleaseStopHdr";
            return;
        }
        m_eidAvailableSetMutex.lock();
        //m_finalDestEidAvailableSet.erase(iReleaseStopHdr->finalDestinationEid);
        //m_finalDestEidAvailableSet.erase(iReleaseStopHdr->nextHopEid);
        m_finalDestNodeIdAvailableSet.erase(iReleaseStopHdr->finalDestinationNodeId); //eid with any service id
        m_finalDestNodeIdAvailableSet.erase(iReleaseStopHdr->nextHopNodeId);
        m_eidAvailableSetMutex.unlock();
        LOG_INFO(subprocess) << "Sending bundles to storage for finalDestinationEid: "
            << Uri::GetIpnUriStringAnyServiceNumber(iReleaseStopHdr->finalDestinationNodeId);
    }
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

static void CustomCleanupToEgressHdr(void *data, void *hint) {
    delete static_cast<hdtn::ToEgressHdr*>(hint);
}

static void CustomCleanupToStorageHdr(void *data, void *hint) {
    delete static_cast<hdtn::ToStorageHdr*>(hint);
}


bool Ingress::ProcessPaddedData(uint8_t * bundleDataBegin, std::size_t bundleCurrentSize,
    std::unique_ptr<zmq::message_t> & zmqPaddedMessageUnderlyingDataUniquePtr, padded_vector_uint8_t & paddedVecMessageUnderlyingData, const bool usingZmqData, const bool needsProcessing)
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
            static const BPV6_BUNDLEFLAG requiredPrimaryFlagsForEcho = BPV6_BUNDLEFLAG::NO_FLAGS_SET;
            //BPV6_BUNDLEFLAG::SINGLETON | BPV6_BUNDLEFLAG::NOFRAGMENT;
            const bool isEcho = (((primary.m_bundleProcessingControlFlags & requiredPrimaryFlagsForEcho) == requiredPrimaryFlagsForEcho) && (finalDestEid == M_HDTN_EID_ECHO));
            if (isEcho) {
                primary.m_destinationEid = primary.m_sourceNodeId;
                finalDestEid = primary.m_destinationEid;
                LOG_ERROR(subprocess) << "Sending Ping for destination " << primary.m_destinationEid;
                primary.m_sourceNodeId = M_HDTN_EID_ECHO;
                bv.m_primaryBlockView.SetManuallyModified();
                bv.Render(bundleCurrentSize + 10);
                std::vector<uint8_t> * rxBufRawPointer = new std::vector<uint8_t>(std::move(bv.m_frontBuffer));
                zmqMessageToSendUniquePtr = boost::make_unique<zmq::message_t>(std::move(zmq::message_t(rxBufRawPointer->data(), rxBufRawPointer->size(), CustomCleanupStdVecUint8, rxBufRawPointer)));
                bundleCurrentSize = zmqMessageToSendUniquePtr->size();
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
            //admin records pertaining to this hdtn node must go to storage.. they signal a deletion from disk
            static constexpr BPV7_BUNDLEFLAG requiredPrimaryFlagsForAdminRecord = BPV7_BUNDLEFLAG::ADMINRECORD;
            isAdminRecordForHdtnStorage = (((primary.m_bundleProcessingControlFlags & requiredPrimaryFlagsForAdminRecord) == requiredPrimaryFlagsForAdminRecord) && (finalDestEid == M_HDTN_EID_CUSTODY));
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


    m_eidAvailableSetMutex.lock();
    const bool linkIsUp = ((m_finalDestEidAvailableSet.count(finalDestEid) != 0) || (m_finalDestNodeIdAvailableSet.count(finalDestEid.nodeId) != 0));
    m_eidAvailableSetMutex.unlock();
    m_availableDestOpportunisticNodeIdToTcpclInductMapMutex.lock();
    std::map<uint64_t, Induct*>::iterator tcpclInductIterator = m_availableDestOpportunisticNodeIdToTcpclInductMap.find(finalDestEid.nodeId);
    const bool isOpportunisticLinkUp = (tcpclInductIterator != m_availableDestOpportunisticNodeIdToTcpclInductMap.end());
    m_availableDestOpportunisticNodeIdToTcpclInductMapMutex.unlock();
    bool shouldTryToUseCustThrough = ((linkIsUp && (!requestsCustody) && (!isAdminRecordForHdtnStorage)));
    bool useStorage = !shouldTryToUseCustThrough;
    if (isOpportunisticLinkUp) {
        if (tcpclInductIterator->second->ForwardOnOpportunisticLink(finalDestEid.nodeId, *zmqMessageToSendUniquePtr, 3)) { //thread safe forward with 3 second timeout
            shouldTryToUseCustThrough = false;
            useStorage = false;
        }
        else {
            std::string msg = "notice in Ingress::Process: tcpcl opportunistic forward timed out after 3 seconds for "
                + Uri::GetIpnUriString(finalDestEid.nodeId, finalDestEid.serviceId);
            if (shouldTryToUseCustThrough) {
                msg += " ..trying the cut-through path instead";
            }
            else {
                msg += " ..sending to storage instead";
            }
            LOG_ERROR(subprocess) << msg;
        }
    }
    if (shouldTryToUseCustThrough) { //type egress cut through ("while loop" instead of "if statement" to support breaking to storage)
        m_egressAckMapSetMutex.lock();
        EgressToIngressAckingSet & egressToIngressAckingObj = m_egressAckMapSet[finalDestEid.nodeId];
        m_egressAckMapSetMutex.unlock();
        boost::posix_time::ptime timeoutExpiry((m_hdtnConfig.m_maxIngressBundleWaitOnEgressMilliseconds != 0) ?
            boost::posix_time::special_values::not_a_date_time :
            boost::posix_time::special_values::neg_infin); //allow zero ms to prevent bpgen getting blocked and use storage
        while (egressToIngressAckingObj.GetSetSize() > m_hdtnConfig.m_zmqMaxMessagesPerPath) { //2000 ms timeout
            if (timeoutExpiry == boost::posix_time::special_values::not_a_date_time) {
                timeoutExpiry = boost::posix_time::microsec_clock::universal_time() + M_MAX_INGRESS_BUNDLE_WAIT_ON_EGRESS_TIME_DURATION;
            }
            else if (timeoutExpiry < boost::posix_time::microsec_clock::universal_time()) {
                std::string msg = "notice in Ingress::Process: cut-through path timed out after " +
                    boost::lexical_cast<std::string>(m_hdtnConfig.m_maxIngressBundleWaitOnEgressMilliseconds) +
                    " milliseconds because it has too many pending egress acks in the queue for finalDestEid (" +
                    boost::lexical_cast<std::string>(finalDestEid.nodeId) + "," + boost::lexical_cast<std::string>(finalDestEid.serviceId) + ") ..sending to storage instead";
                LOG_ERROR(subprocess) << msg;
                useStorage = true;
                break;
            }
            egressToIngressAckingObj.WaitUntilNotifiedOr250MsTimeout(m_hdtnConfig.m_zmqMaxMessagesPerPath);
            //thread is now unblocked, and the lock is reacquired by invoking lock.lock()
            ++m_eventsTooManyInEgressQueue;
        }
        if (!useStorage) {

            const uint64_t ingressToEgressUniqueId = m_ingressToEgressNextUniqueIdAtomic.fetch_add(1, boost::memory_order_relaxed);

            //force natural/64-bit alignment
            hdtn::ToEgressHdr* toEgressHdr = new hdtn::ToEgressHdr();
            zmq::message_t zmqMessageToEgressHdrWithDataStolen(toEgressHdr, sizeof(hdtn::ToEgressHdr), CustomCleanupToEgressHdr, toEgressHdr);

            //memset 0 not needed because all values set below
            toEgressHdr->base.type = HDTN_MSGTYPE_EGRESS;
            toEgressHdr->base.flags = 0; //flags not used by egress // static_cast<uint16_t>(primary.flags);
            toEgressHdr->finalDestEid = finalDestEid;
            toEgressHdr->hasCustody = requestsCustody;
            toEgressHdr->isCutThroughFromIngress = 1;
            toEgressHdr->custodyId = ingressToEgressUniqueId;
            {
                //zmq::message_t messageWithDataStolen(hdrPtr.get(), sizeof(hdtn::BlockHdr), CustomIgnoreCleanupBlockHdr); //cleanup will occur in the queue below
                boost::mutex::scoped_lock lock(m_ingressToEgressZmqSocketMutex);
                if (!m_zmqPushSock_boundIngressToConnectingEgressPtr->send(std::move(zmqMessageToEgressHdrWithDataStolen), zmq::send_flags::sndmore | zmq::send_flags::dontwait)) {
                    LOG_ERROR(subprocess) << "can't send BlockHdr to egress";
                }
                else {
                    egressToIngressAckingObj.PushMove_ThreadSafe(ingressToEgressUniqueId);


                    if (!m_zmqPushSock_boundIngressToConnectingEgressPtr->send(std::move(*zmqMessageToSendUniquePtr), zmq::send_flags::dontwait)) {
                        LOG_ERROR(subprocess) << "can't send bundle to egress";

                    }
                    else {
                        //success                            
                        m_bundleCountEgress.fetch_add(1, boost::memory_order_relaxed);
                    }
                }
            }
        }
    }

    if (useStorage) { //storage
        boost::mutex::scoped_lock lock(m_storageAckQueueMutex);
        const uint64_t ingressToStorageUniqueId = m_ingressToStorageNextUniqueId++;
        boost::posix_time::ptime timeoutExpiry(boost::posix_time::special_values::not_a_date_time);
        while (m_storageAckQueue.size() > m_hdtnConfig.m_zmqMaxMessagesPerPath) { //2000 ms timeout
            if (timeoutExpiry == boost::posix_time::special_values::not_a_date_time) {
                static const boost::posix_time::time_duration twoSeconds = boost::posix_time::seconds(2);
                timeoutExpiry = boost::posix_time::microsec_clock::universal_time() + twoSeconds;
            }
            if (timeoutExpiry < boost::posix_time::microsec_clock::universal_time()) {
                LOG_ERROR(subprocess) << "too many pending storage acks in the queue";
                return false;
            }
            m_conditionVariableStorageAckReceived.timed_wait(lock, boost::posix_time::milliseconds(250)); // call lock.unlock() and blocks the current thread
            //thread is now unblocked, and the lock is reacquired by invoking lock.lock()
            ++m_eventsTooManyInStorageQueue;
        }

        //force natural/64-bit alignment
        hdtn::ToStorageHdr * toStorageHdr = new hdtn::ToStorageHdr();
        zmq::message_t zmqMessageToStorageHdrWithDataStolen(toStorageHdr, sizeof(hdtn::ToStorageHdr), CustomCleanupToStorageHdr, toStorageHdr);

        //memset 0 not needed because all values set below
        toStorageHdr->base.type = HDTN_MSGTYPE_STORE;
        toStorageHdr->base.flags = 0; //flags not used by storage // static_cast<uint16_t>(primary.flags);
        toStorageHdr->ingressUniqueId = ingressToStorageUniqueId;

        //zmq::message_t messageWithDataStolen(hdrPtr.get(), sizeof(hdtn::BlockHdr), CustomIgnoreCleanupBlockHdr); //cleanup will occur in the queue below

        //zmq threads not thread safe but protected by mutex above
        if (!m_zmqPushSock_boundIngressToConnectingStoragePtr->send(std::move(zmqMessageToStorageHdrWithDataStolen), zmq::send_flags::sndmore | zmq::send_flags::dontwait)) {
            LOG_ERROR(subprocess) << "can't send BlockHdr to storage";
        }
        else {
            m_storageAckQueue.push(ingressToStorageUniqueId);

            if (!m_zmqPushSock_boundIngressToConnectingStoragePtr->send(std::move(*zmqMessageToSendUniquePtr), zmq::send_flags::dontwait)) {
                LOG_ERROR(subprocess) << "can't send bundle to storage";
            }
            else {
                //success                            
                ++m_bundleCountStorage; //protected by m_storageAckQueueMutex
            }
        }
    }



    m_bundleData.fetch_add(bundleCurrentSize, boost::memory_order_relaxed);

    return true;
}


void Ingress::WholeBundleReadyCallback(padded_vector_uint8_t & wholeBundleVec) {
    //if more than 1 BpSinkAsync context, must protect shared resources with mutex.  Each BpSinkAsync context has
    //its own processing thread that calls this callback
    static std::unique_ptr<zmq::message_t> unusedZmqPtr;
    ProcessPaddedData(wholeBundleVec.data(), wholeBundleVec.size(), unusedZmqPtr, wholeBundleVec, false, true);
}

void Ingress::SendOpportunisticLinkMessages(const uint64_t remoteNodeId, bool isAvailable) {
    //force natural/64-bit alignment
    hdtn::ToEgressHdr * toEgressHdr = new hdtn::ToEgressHdr();
    zmq::message_t zmqMessageToEgressHdrWithDataStolen(toEgressHdr, sizeof(hdtn::ToEgressHdr), CustomCleanupToEgressHdr, toEgressHdr);

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
        boost::mutex::scoped_lock lock(m_storageAckQueueMutex);
        if (!m_zmqPushSock_boundIngressToConnectingStoragePtr->send(std::move(zmqMessageToStorageHdrWithDataStolen), zmq::send_flags::dontwait)) {
            LOG_ERROR(subprocess) << "can't send ToStorageHdr Opportunistic link message to storage";
        }
    }
}

void Ingress::OnNewOpportunisticLinkCallback(const uint64_t remoteNodeId, Induct * thisInductPtr) {
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
    else {
        LOG_ERROR(subprocess) << "OnNewOpportunisticLinkCallback: Induct ptr cannot cast to TcpclInduct or TcpclV4Induct";
    }
}
void Ingress::OnDeletedOpportunisticLinkCallback(const uint64_t remoteNodeId) {
    LOG_INFO(subprocess) << "Deleted opportunistic link on Tcpcl induct for ipn:" << remoteNodeId << ".*";
    SendOpportunisticLinkMessages(remoteNodeId, false);
    boost::mutex::scoped_lock lock(m_availableDestOpportunisticNodeIdToTcpclInductMapMutex);
    m_availableDestOpportunisticNodeIdToTcpclInductMap.erase(remoteNodeId);
}

}  // namespace hdtn
