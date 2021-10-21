/***************************************************************************
 * NASA Glenn Research Center, Cleveland, OH
 * Released under the NASA Open Source Agreement (NOSA)
 * May  2021
 ****************************************************************************
 */

#include <iostream>

#include "codec/bpv6-ext-block.h"
#include "codec/bpv6.h"
#include "ingress.h"
#include "Logger.h"
#include "message.hpp"
#include <boost/bind/bind.hpp>
#include <boost/make_unique.hpp>
#include <boost/lexical_cast.hpp>


namespace hdtn {

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


    std::cout << "m_eventsTooManyInStorageQueue: " << m_eventsTooManyInStorageQueue << std::endl;
    hdtn::Logger::getInstance()->logNotification("ingress",
        "m_eventsTooManyInStorageQueue: " + std::to_string(m_eventsTooManyInStorageQueue));
}

int Ingress::Init(const HdtnConfig & hdtnConfig, const bool isCutThroughOnlyTest, zmq::context_t * hdtnOneProcessZmqInprocContextPtr) {

    if (!m_running) {
        m_running = true;
        m_hdtnConfig = hdtnConfig;
        //according to ION.pdf v4.0.1 on page 100 it says:
        //  Remember that the format for this argument is ipn:element_number.0 and that
        //  the final 0 is required, as custodial/administration service is always service 0.
        //HDTN shall default m_myCustodialServiceId to 0 although it is changeable in the hdtn config json file
        M_HDTN_EID_CUSTODY.Set(m_hdtnConfig.m_myNodeId, m_hdtnConfig.m_myCustodialServiceId);

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
            }
        }
        catch (const zmq::error_t & ex) {
            std::cerr << "error: ingress cannot connect bind zmq socket: " << ex.what() << std::endl;
            return 0;
        }
        static const int timeout = 250;  // milliseconds
        m_zmqPullSock_connectingStorageToBoundIngressPtr->set(zmq::sockopt::rcvtimeo, timeout);
        m_zmqPullSock_connectingEgressToBoundIngressPtr->set(zmq::sockopt::rcvtimeo, timeout);
        m_zmqPullSock_connectingEgressBundlesOnlyToBoundIngressPtr->set(zmq::sockopt::rcvtimeo, timeout);

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
            std::cout << "Ingress connected and listening to events from scheduler " << connect_boundSchedulerPubSubPath << std::endl;
        } catch (const zmq::error_t & ex) {
            std::cerr << "error: ingress cannot connect to scheduler socket: " << ex.what() << std::endl;
            return 0;
        }
        
        m_threadZmqAckReaderPtr = boost::make_unique<boost::thread>(
            boost::bind(&Ingress::ReadZmqAcksThreadFunc, this)); //create and start the worker thread
        m_threadTcpclOpportunisticBundlesFromEgressReaderPtr = boost::make_unique<boost::thread>(
            boost::bind(&Ingress::ReadTcpclOpportunisticBundlesFromEgressThreadFunc, this)); //create and start the worker thread

        m_isCutThroughOnlyTest = isCutThroughOnlyTest;
        m_inductManager.LoadInductsFromConfig(boost::bind(&Ingress::WholeBundleReadyCallback, this, boost::placeholders::_1), m_hdtnConfig.m_inductsConfig,
            m_hdtnConfig.m_myNodeId, m_hdtnConfig.m_maxLtpReceiveUdpPacketSizeBytes, m_hdtnConfig.m_maxBundleSizeBytes);

        std::cout << "Ingress running, allowing up to " << m_hdtnConfig.m_zmqMaxMessagesPerPath << " max zmq messages per path." << std::endl;
    }
    return 0;
}


void Ingress::ReadZmqAcksThreadFunc() {

    static constexpr unsigned int NUM_SOCKETS = 3;

    zmq::pollitem_t items[NUM_SOCKETS] = {
        {m_zmqPullSock_connectingEgressToBoundIngressPtr->handle(), 0, ZMQ_POLLIN, 0},
        {m_zmqPullSock_connectingStorageToBoundIngressPtr->handle(), 0, ZMQ_POLLIN, 0},
        {m_zmqSubSock_boundSchedulerToConnectingIngressPtr->handle(), 0, ZMQ_POLLIN, 0}
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
            std::cout << "caught zmq::error_t in Ingress::ReadZmqAcksThreadFunc: " << e.what() << std::endl;
            continue;
        }
        if (rc > 0) {
            if (items[0].revents & ZMQ_POLLIN) { //ack from egress
                EgressAckHdr receivedEgressAckHdr;
                const zmq::recv_buffer_result_t res = m_zmqPullSock_connectingEgressToBoundIngressPtr->recv(zmq::mutable_buffer(&receivedEgressAckHdr, sizeof(hdtn::EgressAckHdr)), zmq::recv_flags::dontwait);
                if (!res) {
                    std::cerr << "error in BpIngressSyscall::ReadZmqAcksThreadFunc: cannot read egress BlockHdr ack" << std::endl;
                    hdtn::Logger::getInstance()->logError("ingress",
                        "Error in BpIngressSyscall::ReadZmqAcksThreadFunc: cannot read egress BlockHdr ack");
                }
                else if ((res->truncated()) || (res->size != sizeof(hdtn::EgressAckHdr))) {
                    std::cerr << "egress EgressAckHdr message mismatch: untruncated = " << res->untruncated_size
                        << " truncated = " << res->size << " expected = " << sizeof(hdtn::EgressAckHdr) << std::endl;
                    hdtn::Logger::getInstance()->logError("ingress",
                        "Egress EgressAckHdr message mismatch: untruncated = " + std::to_string(res->untruncated_size)
                        + " truncated = " + std::to_string(res->size) + " expected = " +
                        std::to_string(sizeof(hdtn::EgressAckHdr)));
                }
                else if (receivedEgressAckHdr.base.type != HDTN_MSGTYPE_EGRESS_ACK_TO_INGRESS) {
                    std::cerr << "error message ack not HDTN_MSGTYPE_EGRESS_ACK_TO_INGRESS\n";
                }
                else {
                    m_egressAckMapQueueMutex.lock();
                    EgressToIngressAckingQueue & egressToIngressAckingObj = m_egressAckMapQueue[receivedEgressAckHdr.finalDestEid];
                    m_egressAckMapQueueMutex.unlock();
                    if (egressToIngressAckingObj.CompareAndPop_ThreadSafe(receivedEgressAckHdr.custodyId)) {
                        egressToIngressAckingObj.NotifyAll();
                        ++totalAcksFromEgress;
                    }
                    else {
                        std::cerr << "error didn't receive expected egress ack" << std::endl;
                        hdtn::Logger::getInstance()->logError("ingress", "Error didn't receive expected egress ack");
                    }

                }
            }
            if (items[1].revents & ZMQ_POLLIN) { //ack from storage
                StorageAckHdr receivedStorageAck;
                const zmq::recv_buffer_result_t res = m_zmqPullSock_connectingStorageToBoundIngressPtr->recv(zmq::mutable_buffer(&receivedStorageAck, sizeof(hdtn::StorageAckHdr)), zmq::recv_flags::dontwait);
                if (!res) {
                    std::cerr << "error in BpIngressSyscall::ReadZmqAcksThreadFunc: cannot read storage BlockHdr ack" << std::endl;
                    hdtn::Logger::getInstance()->logError("ingress",
                        "Error in BpIngressSyscall::ReadZmqAcksThreadFunc: cannot read storage BlockHdr ack");

                }
                else if ((res->truncated()) || (res->size != sizeof(hdtn::StorageAckHdr))) {
                    std::cerr << "egress StorageAckHdr message mismatch: untruncated = " << res->untruncated_size
                        << " truncated = " << res->size << " expected = " << sizeof(hdtn::StorageAckHdr) << std::endl;
                    hdtn::Logger::getInstance()->logError("ingress",
                        "Egress blockhdr message mismatch: untruncated = " + std::to_string(res->untruncated_size)
                        + " truncated = " + std::to_string(res->size) + " expected = " +
                        std::to_string(sizeof(hdtn::StorageAckHdr)));
                }
                else if (receivedStorageAck.base.type != HDTN_MSGTYPE_STORAGE_ACK_TO_INGRESS) {
                    std::cerr << "error message ack not HDTN_MSGTYPE_STORAGE_ACK_TO_INGRESS\n";
                }
                else {
                    bool needsNotify = false;
                    {
                        boost::mutex::scoped_lock lock(m_storageAckQueueMutex);
                        if (m_storageAckQueue.empty()) {
                            std::cerr << "error m_storageAckQueue is empty" << std::endl;
                            hdtn::Logger::getInstance()->logError("ingress", "Error m_storageAckQueue is empty");
                        }
                        else if (m_storageAckQueue.front() == receivedStorageAck.ingressUniqueId) {
                            m_storageAckQueue.pop();
                            needsNotify = true;
                            ++totalAcksFromStorage;
                        }
                        else {
                            std::cerr << "error didn't receive expected storage ack" << std::endl;
                            hdtn::Logger::getInstance()->logError("ingress", "Error didn't receive expected storage ack");
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
        }
    }
    std::cout << "totalAcksFromEgress: " << totalAcksFromEgress << std::endl;
    std::cout << "totalAcksFromStorage: " << totalAcksFromStorage << std::endl;
    std::cout << "m_bundleCountStorage: " << m_bundleCountStorage << std::endl;
    std::cout << "m_bundleCountEgress: " << m_bundleCountEgress << std::endl;
    m_bundleCount = m_bundleCountStorage + m_bundleCountEgress;
    std::cout << "m_bundleCount: " << m_bundleCount << std::endl;
    std::cout << "BpIngressSyscall::ReadZmqAcksThreadFunc thread exiting\n";
    hdtn::Logger::getInstance()->logInfo("ingress", "totalAcksFromEgress: " + std::to_string(totalAcksFromEgress));
    hdtn::Logger::getInstance()->logInfo("ingress", "totalAcksFromStorage: " + std::to_string(totalAcksFromStorage));
    hdtn::Logger::getInstance()->logInfo("ingress", "m_bundleCountStorage: " + std::to_string(m_bundleCountStorage));
    hdtn::Logger::getInstance()->logInfo("ingress", "m_bundleCountEgress: " + std::to_string(m_bundleCountEgress));
    hdtn::Logger::getInstance()->logInfo("ingress", "m_bundleCount: " + std::to_string(m_bundleCount));
    hdtn::Logger::getInstance()->logNotification("ingress", "BpIngressSyscall::ReadZmqAcksThreadFunc thread exiting");
}

void Ingress::ReadTcpclOpportunisticBundlesFromEgressThreadFunc() {
    static constexpr unsigned int NUM_SOCKETS = 1;
    zmq::pollitem_t items[NUM_SOCKETS] = {
        {m_zmqPullSock_connectingEgressBundlesOnlyToBoundIngressPtr->handle(), 0, ZMQ_POLLIN, 0}
    };
    std::size_t totalOpportunisticBundlesFromEgress = 0;
    static const long DEFAULT_BIG_TIMEOUT_POLL = 250;
    while (m_running) { //keep thread alive if running
        int rc = 0;
        try {
            rc = zmq::poll(&items[0], NUM_SOCKETS, DEFAULT_BIG_TIMEOUT_POLL);
        }
        catch (zmq::error_t & e) {
            std::cout << "caught zmq::error_t in Ingress::ReadTcpclOpportunisticBundlesFromEgressThreadFunc: " << e.what() << std::endl;
            continue;
        }
        if ((rc > 0) && (items[0].revents & ZMQ_POLLIN)) { //ack from egress
            zmq::message_t zmqMessage;
            //no header, just a bundle as a zmq message
            if (!m_zmqPullSock_connectingEgressBundlesOnlyToBoundIngressPtr->recv(zmqMessage, zmq::recv_flags::none)) {
                std::cerr << "error in Ingress::ReadTcpclOpportunisticBundlesFromEgressThreadFunc: cannot receive zmq\n";
            }
            else {
                Process(std::move(zmqMessage));
                ++totalOpportunisticBundlesFromEgress;
            }
        }
    }
    std::cout << "totalOpportunisticBundlesFromEgress: " << totalOpportunisticBundlesFromEgress << std::endl;
}

void Ingress::SchedulerEventHandler() {
    //force this hdtn message struct to be aligned on a 64-byte boundary using zmq::mutable_buffer
    static constexpr std::size_t minBufSizeBytes = sizeof(uint64_t) + ((sizeof(IreleaseStartHdr) > sizeof(IreleaseStopHdr)) ? sizeof(IreleaseStartHdr) : sizeof(IreleaseStopHdr));
    m_schedulerRxBufPtrToStdVec64.resize(minBufSizeBytes / sizeof(uint64_t));
    uint64_t * rxBufRawPtrAlign64 = &m_schedulerRxBufPtrToStdVec64[0];
    const zmq::recv_buffer_result_t res = m_zmqSubSock_boundSchedulerToConnectingIngressPtr->recv(zmq::mutable_buffer(rxBufRawPtrAlign64, minBufSizeBytes), zmq::recv_flags::none);
    if (!res) {
        std::cerr << "[Ingress::SchedulerEventHandler] message not received" << std::endl;
        hdtn::Logger::getInstance()->logError("ingress", "[Ingress::SchedulerEventHandler] message not received");
        return;
    }
    else if (res->size < sizeof(hdtn::CommonHdr)) {
        std::cerr << "[Ingress::SchedulerEventHandler] res->size < sizeof(hdtn::CommonHdr)" << std::endl;
        hdtn::Logger::getInstance()->logError("ingress", "[Ingress::SchedulerEventHandler] res->size < sizeof(hdtn::CommonHdr)");
        return;
    }

    CommonHdr *common = (CommonHdr *)rxBufRawPtrAlign64;
    if (common->type == HDTN_MSGTYPE_ILINKUP) {
        hdtn::IreleaseStartHdr * iReleaseStartHdr = (hdtn::IreleaseStartHdr *)rxBufRawPtrAlign64;
        if (res->size != sizeof(hdtn::IreleaseStartHdr)) {
            std::cerr << "[Ingress::SchedulerEventHandler] res->size != sizeof(hdtn::IreleaseStartHdr" << std::endl;
            hdtn::Logger::getInstance()->logError("ingress", "[Ingress::SchedulerEventHandler] res->size != sizeof(hdtn::IreleaseStartHdr");
            return;
        }
        m_eidAvailableSetMutex.lock();
        m_finalDestEidAvailableSet.insert(iReleaseStartHdr->finalDestinationEid);
        m_eidAvailableSetMutex.unlock();
        std::cout << "Ingress sending bundles to egress for finalDestinationEid: (" << iReleaseStartHdr->finalDestinationEid.nodeId
            << "," << iReleaseStartHdr->finalDestinationEid.serviceId << ")" << std::endl;
    }
    else if (common->type == HDTN_MSGTYPE_ILINKDOWN) {
        hdtn::IreleaseStopHdr * iReleaseStoptHdr = (hdtn::IreleaseStopHdr *)rxBufRawPtrAlign64;
        if (res->size != sizeof(hdtn::IreleaseStopHdr)) {
            std::cerr << "[Ingress::SchedulerEventHandler] res->size != sizeof(hdtn::IreleaseStopHdr" << std::endl;
            hdtn::Logger::getInstance()->logError("ingress", "[Ingress::SchedulerEventHandler] res->size != sizeof(hdtn::IreleaseStopHdr");
            return;
        }
        m_eidAvailableSetMutex.lock();
        m_finalDestEidAvailableSet.erase(iReleaseStoptHdr->finalDestinationEid);
        m_eidAvailableSetMutex.unlock();
        std::cout << "Ingress sending bundles to storage for finalDestinationEid: (" << iReleaseStoptHdr->finalDestinationEid.nodeId
            << "," << iReleaseStoptHdr->finalDestinationEid.serviceId << ")" << std::endl;
    }
}


static void CustomCleanupStdVecUint8(void *data, void *hint) {
    //std::cout << "free " << static_cast<std::vector<uint8_t>*>(hint)->size() << std::endl;
    delete static_cast<std::vector<uint8_t>*>(hint);
}

static void CustomCleanupToEgressHdr(void *data, void *hint) {
    delete static_cast<hdtn::ToEgressHdr*>(hint);
}

static void CustomCleanupToStorageHdr(void *data, void *hint) {
    delete static_cast<hdtn::ToStorageHdr*>(hint);
}

bool Ingress::Process(std::vector<uint8_t> && rxBuf) {
    //this is an optimization because we only have one chunk to send
    //The zmq_msg_init_data() function shall initialise the message object referenced by msg
    //to represent the content referenced by the buffer located at address data, size bytes long.
    //No copy of data shall be performed and 0MQ shall take ownership of the supplied buffer.
    //If provided, the deallocation function ffn shall be called once the data buffer is no longer
    //required by 0MQ, with the data and hint arguments supplied to zmq_msg_init_data().
    std::vector<uint8_t> * rxBufRawPointer = new std::vector<uint8_t>(std::move(rxBuf));
    zmq::message_t messageWithDataStolen(rxBufRawPointer->data(), rxBufRawPointer->size(), CustomCleanupStdVecUint8, rxBufRawPointer);
    return Process(std::move(messageWithDataStolen));
}
bool Ingress::Process(zmq::message_t && rxMsg) {
    const std::size_t messageSize = rxMsg.size();
    if (messageSize > m_hdtnConfig.m_maxBundleSizeBytes) { //should never reach here as this is handled by induct
        std::cerr << "error in Ingress::Process: received bundle size (" 
            << messageSize << " bytes) exceeds max bundle size limit of "
            << m_hdtnConfig.m_maxBundleSizeBytes << " bytes\n";
        return false;
    }
    bpv6_primary_block primary;
    if (!cbhe_bpv6_primary_block_decode(&primary, (const char*)rxMsg.data(), 0, messageSize)) {
        std::cerr << "error in Ingress::Process: malformed bundle received\n";
        return false;
    }


    const cbhe_eid_t finalDestEid(primary.dst_node, primary.dst_svc);
    static constexpr uint64_t requiredPrimaryFlagsForCustody = BPV6_BUNDLEFLAG_SINGLETON | BPV6_BUNDLEFLAG_NOFRAGMENT | BPV6_BUNDLEFLAG_CUSTODY;
    const bool requestsCustody = ((primary.flags & requiredPrimaryFlagsForCustody) == requiredPrimaryFlagsForCustody);
    //admin records pertaining to this hdtn node must go to storage.. they signal a deletion from disk
    const uint64_t requiredPrimaryFlagsForAdminRecord = BPV6_BUNDLEFLAG_SINGLETON | BPV6_BUNDLEFLAG_NOFRAGMENT | BPV6_BUNDLEFLAG_ADMIN_RECORD;
    const bool isAdminRecordForHdtnStorage = (((primary.flags & requiredPrimaryFlagsForAdminRecord) == requiredPrimaryFlagsForAdminRecord) && (finalDestEid == M_HDTN_EID_CUSTODY));
    //if (isAdminRecordForHdtnStorage) {
    //    std::cout << "ingress received admin record for final dest eid (" << finalDestEid.nodeId << "," << finalDestEid.serviceId << ")\n";
    //}
    m_eidAvailableSetMutex.lock();
    const bool linkIsUp = (m_finalDestEidAvailableSet.count(finalDestEid) != 0);
    m_eidAvailableSetMutex.unlock();
    bool shouldTryToUseCustThrough = (m_isCutThroughOnlyTest || (linkIsUp && (!requestsCustody) && (!isAdminRecordForHdtnStorage)));
    bool useStorage = !shouldTryToUseCustThrough;
    while (shouldTryToUseCustThrough) { //type egress cut through ("while loop" instead of "if statement" to support breaking to storage)
        shouldTryToUseCustThrough = false; //protection to prevent this loop from ever iterating more than once
        m_egressAckMapQueueMutex.lock();
        EgressToIngressAckingQueue & egressToIngressAckingObj = m_egressAckMapQueue[finalDestEid];
        m_egressAckMapQueueMutex.unlock();
        boost::posix_time::ptime timeoutExpiry((m_hdtnConfig.m_maxIngressBundleWaitOnEgressMilliseconds != 0) ?
            boost::posix_time::special_values::not_a_date_time :
            boost::posix_time::special_values::neg_infin); //allow zero ms to prevent bpgen getting blocked and use storage
        while (egressToIngressAckingObj.GetQueueSize() > m_hdtnConfig.m_zmqMaxMessagesPerPath) { //2000 ms timeout
            if (timeoutExpiry == boost::posix_time::special_values::not_a_date_time) {
                timeoutExpiry = boost::posix_time::microsec_clock::universal_time() + M_MAX_INGRESS_BUNDLE_WAIT_ON_EGRESS_TIME_DURATION;
            }
            else if (timeoutExpiry < boost::posix_time::microsec_clock::universal_time()) {
                std::string msg = "notice in Ingress::Process: cut-through path timed out after " +
                    boost::lexical_cast<std::string>(m_hdtnConfig.m_maxIngressBundleWaitOnEgressMilliseconds) +
                    " milliseconds because it has too many pending egress acks in the queue for finalDestEid (" +
                    boost::lexical_cast<std::string>(finalDestEid.nodeId) + "," + boost::lexical_cast<std::string>(finalDestEid.serviceId) + ")";
                if (m_isCutThroughOnlyTest) {
                    msg += " ..dropping bundle because \"cut through only test\" was specified (not sending to storage)";
                    std::cerr << msg << std::endl;
                    hdtn::Logger::getInstance()->logError("ingress", msg);
                    return false;
                }
                else {
                    msg += " ..sending to storage instead";
                    std::cerr << msg << std::endl;
                    hdtn::Logger::getInstance()->logError("ingress", msg);
                    useStorage = true;
                    break;
                }
                
            }
            egressToIngressAckingObj.WaitUntilNotifiedOr250MsTimeout();
            //thread is now unblocked, and the lock is reacquired by invoking lock.lock()
            ++m_eventsTooManyInEgressQueue;
        }

        const uint64_t ingressToEgressUniqueId = m_ingressToEgressNextUniqueIdAtomic.fetch_add(1, boost::memory_order_relaxed);

        //force natural/64-bit alignment
        hdtn::ToEgressHdr * toEgressHdr = new hdtn::ToEgressHdr();
        zmq::message_t zmqMessageToEgressHdrWithDataStolen(toEgressHdr, sizeof(hdtn::ToEgressHdr), CustomCleanupToEgressHdr, toEgressHdr);
            
        //memset 0 not needed because all values set below
        toEgressHdr->base.type = HDTN_MSGTYPE_EGRESS;
        toEgressHdr->base.flags = static_cast<uint16_t>(primary.flags);
        toEgressHdr->finalDestEid = finalDestEid;
        toEgressHdr->hasCustody = requestsCustody;
        toEgressHdr->isCutThroughFromIngress = 1;
        toEgressHdr->custodyId = ingressToEgressUniqueId;
        {
            //zmq::message_t messageWithDataStolen(hdrPtr.get(), sizeof(hdtn::BlockHdr), CustomIgnoreCleanupBlockHdr); //cleanup will occur in the queue below
            boost::mutex::scoped_lock lock(m_ingressToEgressZmqSocketMutex);
            if (!m_zmqPushSock_boundIngressToConnectingEgressPtr->send(std::move(zmqMessageToEgressHdrWithDataStolen), zmq::send_flags::sndmore | zmq::send_flags::dontwait)) {
                std::cerr << "ingress can't send BlockHdr to egress" << std::endl;
                hdtn::Logger::getInstance()->logError("ingress", "Ingress can't send BlockHdr to egress");
            }
            else {
                egressToIngressAckingObj.PushMove_ThreadSafe(ingressToEgressUniqueId);

                
                if (!m_zmqPushSock_boundIngressToConnectingEgressPtr->send(std::move(rxMsg), zmq::send_flags::dontwait)) {
                    std::cerr << "ingress can't send bundle to egress" << std::endl;
                    hdtn::Logger::getInstance()->logError("ingress", "Ingress can't send bundle to egress");

                }
                else {
                    //success                            
                    m_bundleCountEgress.fetch_add(1, boost::memory_order_relaxed);
                }
            }
        }
        break;
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
                std::cerr << "error: too many pending storage acks in the queue" << std::endl;
                hdtn::Logger::getInstance()->logError("ingress", "Error: too many pending storage acks in the queue");
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
        toStorageHdr->base.flags = static_cast<uint16_t>(primary.flags);
        toStorageHdr->ingressUniqueId = ingressToStorageUniqueId;

        //zmq::message_t messageWithDataStolen(hdrPtr.get(), sizeof(hdtn::BlockHdr), CustomIgnoreCleanupBlockHdr); //cleanup will occur in the queue below

        //zmq threads not thread safe but protected by mutex above
        if (!m_zmqPushSock_boundIngressToConnectingStoragePtr->send(std::move(zmqMessageToStorageHdrWithDataStolen), zmq::send_flags::sndmore | zmq::send_flags::dontwait)) {
            std::cerr << "ingress can't send BlockHdr to storage" << std::endl;
            hdtn::Logger::getInstance()->logError("ingress", "Ingress can't send BlockHdr to storage");
        }
        else {
            m_storageAckQueue.push(ingressToStorageUniqueId);

            if (!m_zmqPushSock_boundIngressToConnectingStoragePtr->send(std::move(rxMsg), zmq::send_flags::dontwait)) {
                std::cerr << "ingress can't send bundle to storage" << std::endl;
                hdtn::Logger::getInstance()->logError("ingress", "Ingress can't send bundle to storage");
            }
            else {
                //success                            
                ++m_bundleCountStorage; //protected by m_storageAckQueueMutex
            }
        }
    }



    m_bundleData.fetch_add(messageSize, boost::memory_order_relaxed);

    return true;
}



void Ingress::WholeBundleReadyCallback(std::vector<uint8_t> & wholeBundleVec) {
    //if more than 1 BpSinkAsync context, must protect shared resources with mutex.  Each BpSinkAsync context has
    //its own processing thread that calls this callback
    Process(std::move(wholeBundleVec));
}

}  // namespace hdtn
