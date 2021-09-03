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
#include <boost/bind.hpp>
#include <boost/make_unique.hpp>
#include <boost/lexical_cast.hpp>

static const uint64_t PRIMARY_HDTN_NODE = 10; //todo
static const uint64_t PRIMARY_HDTN_SVC = 10; //todo
static const cbhe_eid_t HDTN_EID(PRIMARY_HDTN_NODE, PRIMARY_HDTN_SVC);

namespace hdtn {

Ingress::Ingress() :
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



    std::cout << "m_eventsTooManyInStorageQueue: " << m_eventsTooManyInStorageQueue << std::endl;
    hdtn::Logger::getInstance()->logNotification("ingress",
        "m_eventsTooManyInStorageQueue: " + std::to_string(m_eventsTooManyInStorageQueue));
}

int Ingress::Init(const HdtnConfig & hdtnConfig, bool alwaysSendToStorage, zmq::context_t * hdtnOneProcessZmqInprocContextPtr) {


    if (!m_running) {
        m_running = true;
        m_hdtnConfig = hdtnConfig;

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
        }
        else {
            // socket for cut-through mode straight to egress
            m_zmqCtx_ingressEgressPtr = boost::make_unique<zmq::context_t>();
            m_zmqPushSock_boundIngressToConnectingEgressPtr = boost::make_unique<zmq::socket_t>(*m_zmqCtx_ingressEgressPtr, zmq::socket_type::push);
            const std::string bind_boundIngressToConnectingEgressPath(
                std::string("tcp://*:") + boost::lexical_cast<std::string>(m_hdtnConfig.m_zmqBoundIngressToConnectingEgressPortPath));
            m_zmqPushSock_boundIngressToConnectingEgressPtr->bind(bind_boundIngressToConnectingEgressPath);
            // socket for sending bundles to storage
            m_zmqCtx_ingressStoragePtr = boost::make_unique<zmq::context_t>();
            m_zmqPushSock_boundIngressToConnectingStoragePtr = boost::make_unique<zmq::socket_t>(*m_zmqCtx_ingressStoragePtr, zmq::socket_type::push);
            const std::string bind_boundIngressToConnectingStoragePath(
                std::string("tcp://*:") + boost::lexical_cast<std::string>(m_hdtnConfig.m_zmqBoundIngressToConnectingStoragePortPath));
            m_zmqPushSock_boundIngressToConnectingStoragePtr->bind(bind_boundIngressToConnectingStoragePath);
            // socket for receiving acks from storage
            m_zmqPullSock_connectingStorageToBoundIngressPtr = boost::make_unique<zmq::socket_t>(*m_zmqCtx_ingressStoragePtr, zmq::socket_type::pull);
            const std::string bind_connectingStorageToBoundIngressPath(
                std::string("tcp://*:") + boost::lexical_cast<std::string>(m_hdtnConfig.m_zmqConnectingStorageToBoundIngressPortPath));
            m_zmqPullSock_connectingStorageToBoundIngressPtr->bind(bind_connectingStorageToBoundIngressPath);
            // socket for receiving acks from egress
            m_zmqPullSock_connectingEgressToBoundIngressPtr = boost::make_unique<zmq::socket_t>(*m_zmqCtx_ingressEgressPtr, zmq::socket_type::pull);
            const std::string bind_connectingEgressToBoundIngressPath(
                std::string("tcp://*:") + boost::lexical_cast<std::string>(m_hdtnConfig.m_zmqConnectingEgressToBoundIngressPortPath));
            m_zmqPullSock_connectingEgressToBoundIngressPtr->bind(bind_connectingEgressToBoundIngressPath);
        }
        static const int timeout = 250;  // milliseconds
        m_zmqPullSock_connectingStorageToBoundIngressPtr->set(zmq::sockopt::rcvtimeo, timeout);
        m_zmqPullSock_connectingEgressToBoundIngressPtr->set(zmq::sockopt::rcvtimeo, timeout);

        m_threadZmqAckReaderPtr = boost::make_unique<boost::thread>(
            boost::bind(&Ingress::ReadZmqAcksThreadFunc, this)); //create and start the worker thread

        m_alwaysSendToStorage = alwaysSendToStorage;
        m_inductManager.LoadInductsFromConfig(boost::bind(&Ingress::WholeBundleReadyCallback, this, boost::placeholders::_1), m_hdtnConfig.m_inductsConfig);

        std::cout << "Ingress running, allowing up to " << m_hdtnConfig.m_zmqMaxMessagesPerPath << " max zmq messages per path." << std::endl;
    }
    return 0;
}


void Ingress::ReadZmqAcksThreadFunc() {

    static const unsigned int NUM_SOCKETS = 2;

    zmq::pollitem_t items[NUM_SOCKETS] = {
        {m_zmqPullSock_connectingEgressToBoundIngressPtr->handle(), 0, ZMQ_POLLIN, 0},
        {m_zmqPullSock_connectingStorageToBoundIngressPtr->handle(), 0, ZMQ_POLLIN, 0}
    };
    std::size_t totalAcksFromEgress = 0;
    std::size_t totalAcksFromStorage = 0;

    static const long DEFAULT_BIG_TIMEOUT_POLL = 250;

    while (m_running) { //keep thread alive if running
        if (zmq::poll(&items[0], NUM_SOCKETS, DEFAULT_BIG_TIMEOUT_POLL) > 0) {
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

static void CustomCleanupStdVecUint8(void *data, void *hint) {
    //std::cout << "free " << static_cast<std::vector<uint8_t>*>(hint)->size() << std::endl;
    delete static_cast<std::vector<uint8_t>*>(hint);
}

//static void CustomIgnoreCleanupBlockHdr(void *data, void *hint) {}

static void CustomCleanupToEgressHdr(void *data, void *hint) {
    delete static_cast<hdtn::ToEgressHdr*>(hint);
}

static void CustomCleanupToStorageHdr(void *data, void *hint) {
    delete static_cast<hdtn::ToStorageHdr*>(hint);
}

bool Ingress::Process(std::vector<uint8_t> && rxBuf) {  //TODO: make buffer zmq message to reduce copy
    const std::size_t messageSize = rxBuf.size();
    bpv6_primary_block primary;
    if (!cbhe_bpv6_primary_block_decode(&primary, (const char*)rxBuf.data(), 0, messageSize)) {
        std::cerr << "malformed bundle\n";
        return 0;
    }

    const cbhe_eid_t finalDestEid(primary.dst_node, primary.dst_svc);
    static constexpr uint64_t requiredPrimaryFlagsForCustody = BPV6_BUNDLEFLAG_SINGLETON | BPV6_BUNDLEFLAG_NOFRAGMENT | BPV6_BUNDLEFLAG_CUSTODY;
    const bool requestsCustody = ((primary.flags & requiredPrimaryFlagsForCustody) == requiredPrimaryFlagsForCustody);
    //admin records pertaining to this hdtn node must go to storage.. they signal a deletion from disk
    const uint64_t requiredPrimaryFlagsForAdminRecord = BPV6_BUNDLEFLAG_SINGLETON | BPV6_BUNDLEFLAG_NOFRAGMENT | BPV6_BUNDLEFLAG_ADMIN_RECORD;
    const bool isAdminRecordForHdtnStorage = (((primary.flags & requiredPrimaryFlagsForAdminRecord) == requiredPrimaryFlagsForAdminRecord) && (finalDestEid == HDTN_EID));

    if ((!m_alwaysSendToStorage) && (!requestsCustody) && (!isAdminRecordForHdtnStorage)) { //type egress cut through
        m_egressAckMapQueueMutex.lock();
        EgressToIngressAckingQueue & egressToIngressAckingObj = m_egressAckMapQueue[finalDestEid];
        m_egressAckMapQueueMutex.unlock();
        for (unsigned int attempt = 0; attempt < 8; ++attempt) { //2000 ms timeout
            if (egressToIngressAckingObj.GetQueueSize() > m_hdtnConfig.m_zmqMaxMessagesPerPath) {
                if (attempt == 7) {
                    const std::string msg = "error: too many pending egress acks in the queue for finalDestEid (" +
                        boost::lexical_cast<std::string>(finalDestEid.nodeId) + "," + boost::lexical_cast<std::string>(finalDestEid.serviceId) + ")";
                    std::cerr << msg << std::endl;
                    hdtn::Logger::getInstance()->logError("ingress", msg);
                }
                egressToIngressAckingObj.WaitUntilNotifiedOr250MsTimeout();
                //thread is now unblocked, and the lock is reacquired by invoking lock.lock()
                ++m_eventsTooManyInEgressQueue;
                continue; //next attempt
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

                    //this is an optimization because we only have one chunk to send
                    //The zmq_msg_init_data() function shall initialise the message object referenced by msg
                    //to represent the content referenced by the buffer located at address data, size bytes long.
                    //No copy of data shall be performed and 0MQ shall take ownership of the supplied buffer.
                    //If provided, the deallocation function ffn shall be called once the data buffer is no longer
                    //required by 0MQ, with the data and hint arguments supplied to zmq_msg_init_data().
                    std::vector<uint8_t> * rxBufRawPointer = new std::vector<uint8_t>(std::move(rxBuf));
                    zmq::message_t messageWithDataStolen(rxBufRawPointer->data(), rxBufRawPointer->size(), CustomCleanupStdVecUint8, rxBufRawPointer);
                    if (!m_zmqPushSock_boundIngressToConnectingEgressPtr->send(std::move(messageWithDataStolen), zmq::send_flags::dontwait)) {
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
    }
    else { //storage
        boost::mutex::scoped_lock lock(m_storageAckQueueMutex);
        const uint64_t ingressToStorageUniqueId = m_ingressToStorageNextUniqueId++;
        for (unsigned int attempt = 0; attempt < 8; ++attempt) { //2000 ms timeout
            if (m_storageAckQueue.size() > m_hdtnConfig.m_zmqMaxMessagesPerPath) {
                if (attempt == 7) {
                    std::cerr << "error: too many pending storage acks in the queue" << std::endl;
                    hdtn::Logger::getInstance()->logError("ingress", "Error: too many pending storage acks in the queue");
                }
                m_conditionVariableStorageAckReceived.timed_wait(lock, boost::posix_time::milliseconds(250)); // call lock.unlock() and blocks the current thread
                //thread is now unblocked, and the lock is reacquired by invoking lock.lock()
                ++m_eventsTooManyInStorageQueue;
                continue; //next attempt
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

                //this is an optimization because we only have one chunk to send
                //The zmq_msg_init_data() function shall initialise the message object referenced by msg
                //to represent the content referenced by the buffer located at address data, size bytes long.
                //No copy of data shall be performed and 0MQ shall take ownership of the supplied buffer.
                //If provided, the deallocation function ffn shall be called once the data buffer is no longer
                //required by 0MQ, with the data and hint arguments supplied to zmq_msg_init_data().
                std::vector<uint8_t> * rxBufRawPointer = new std::vector<uint8_t>(std::move(rxBuf));
                zmq::message_t messageWithDataStolen(rxBufRawPointer->data(), rxBufRawPointer->size(), CustomCleanupStdVecUint8, rxBufRawPointer);
                if (!m_zmqPushSock_boundIngressToConnectingStoragePtr->send(std::move(messageWithDataStolen), zmq::send_flags::dontwait)) {
                    std::cerr << "ingress can't send bundle to storage" << std::endl;
                    hdtn::Logger::getInstance()->logError("ingress", "Ingress can't send bundle to storage");
                }
                else {
                    //success                            
                    ++m_bundleCountStorage;
                }

            }
            break;

        }
    }



    m_bundleData.fetch_add(messageSize, boost::memory_order_relaxed);
    ////timer = rdtsc() - timer;

    return 0;
}



void Ingress::WholeBundleReadyCallback(std::vector<uint8_t> & wholeBundleVec) {
    //if more than 1 BpSinkAsync context, must protect shared resources with mutex.  Each BpSinkAsync context has
    //its own processing thread that calls this callback
    Process(std::move(wholeBundleVec));
}

}  // namespace hdtn
