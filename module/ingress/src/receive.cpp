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

namespace hdtn {

Ingress::Ingress() :
    m_eventsTooManyInStorageQueue(0),
    m_eventsTooManyInEgressQueue(0),
    m_running(false)
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

int Ingress::Init(const HdtnConfig & hdtnConfig) {
    

    if (!m_running) {
        m_running = true;
        m_hdtnConfig = hdtnConfig;

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
        static const int timeout = 250;  // milliseconds
        m_zmqPullSock_connectingStorageToBoundIngressPtr->set(zmq::sockopt::rcvtimeo, timeout);
        m_zmqPullSock_connectingEgressToBoundIngressPtr->set(zmq::sockopt::rcvtimeo, timeout);


	// socket for receiving events from scheduler
        m_zmqCtx_schedulerIngressPtr = boost::make_unique<zmq::context_t>();
        m_zmqSubSock_boundSchedulerToConnectingIngressPtr = boost::make_unique<zmq::socket_t>(*m_zmqCtx_schedulerIngressPtr, 
											      zmq::socket_type::sub);
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
            return false;
        }

        m_threadZmqAckReaderPtr = boost::make_unique<boost::thread>(
            boost::bind(&Ingress::ReadZmqAcksThreadFunc, this)); //create and start the worker thread

        m_inductManager.LoadInductsFromConfig(boost::bind(&Ingress::WholeBundleReadyCallback, this, boost::placeholders::_1), m_hdtnConfig.m_inductsConfig);

        std::cout << "Ingress running, allowing up to " << m_hdtnConfig.m_zmqMaxMessagesPerPath << " max zmq messages per path." << std::endl;
    	//m_sendToStorage[1] = true;
	//m_sendToStorage[2] = true;
    }
    return 0;
}


void Ingress::ReadZmqAcksThreadFunc() {

    static const unsigned int NUM_SOCKETS = 3;

    zmq::pollitem_t items[NUM_SOCKETS] = {
        {m_zmqPullSock_connectingEgressToBoundIngressPtr->handle(), 0, ZMQ_POLLIN, 0},
        {m_zmqPullSock_connectingStorageToBoundIngressPtr->handle(), 0, ZMQ_POLLIN, 0},
	{m_zmqSubSock_boundSchedulerToConnectingIngressPtr->handle(), 0, ZMQ_POLLIN, 0}
    };
    std::size_t totalAcksFromEgress = 0;
    std::size_t totalAcksFromStorage = 0;

    static const long DEFAULT_BIG_TIMEOUT_POLL = 250;
    BlockHdr rblk;
    while (m_running) { //keep thread alive if running
        if (zmq::poll(&items[0], NUM_SOCKETS, DEFAULT_BIG_TIMEOUT_POLL) > 0) {
            if (items[0].revents & ZMQ_POLLIN) { //ack from egress
                const zmq::recv_buffer_result_t res = m_zmqPullSock_connectingEgressToBoundIngressPtr->recv(zmq::mutable_buffer(&rblk, sizeof(hdtn::BlockHdr)), zmq::recv_flags::dontwait);
                if (!res) {
                    std::cerr << "error in BpIngressSyscall::ReadZmqAcksThreadFunc: cannot read egress BlockHdr ack" << std::endl;
                    hdtn::Logger::getInstance()->logError("ingress", 
                        "Error in BpIngressSyscall::ReadZmqAcksThreadFunc: cannot read egress BlockHdr ack");
                }
                else if ((res->truncated()) || (res->size != sizeof(hdtn::BlockHdr))) {
                    std::cerr << "egress blockhdr message mismatch: untruncated = " << res->untruncated_size
                        << " truncated = " << res->size << " expected = " << sizeof(hdtn::BlockHdr) << std::endl;
                    hdtn::Logger::getInstance()->logError("ingress", 
                        "Egress blockhdr message mismatch: untruncated = " + std::to_string(res->untruncated_size)
                        + " truncated = " + std::to_string(res->size) + " expected = " + 
                        std::to_string(sizeof(hdtn::BlockHdr)));
                }
                else {
                    m_egressAckMapQueueMutex.lock();
                    EgressToIngressAckingQueue & egressToIngressAckingObj = m_egressAckMapQueue[rblk.flowId];
                    m_egressAckMapQueueMutex.unlock();
                    if (egressToIngressAckingObj.CompareAndPop_ThreadSafe(rblk)) {
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
                const zmq::recv_buffer_result_t res = m_zmqPullSock_connectingStorageToBoundIngressPtr->recv(zmq::mutable_buffer(&rblk, sizeof(hdtn::BlockHdr)), zmq::recv_flags::dontwait);
                if (!res) {
                    std::cerr << "error in BpIngressSyscall::ReadZmqAcksThreadFunc: cannot read storage BlockHdr ack" << std::endl;
                    hdtn::Logger::getInstance()->logError("ingress", 
                        "Error in BpIngressSyscall::ReadZmqAcksThreadFunc: cannot read storage BlockHdr ack");

                }
                else if ((res->truncated()) || (res->size != sizeof(hdtn::BlockHdr))) {
                    std::cerr << "egress blockhdr message mismatch: untruncated = " << res->untruncated_size
                        << " truncated = " << res->size << " expected = " << sizeof(hdtn::BlockHdr) << std::endl;
                    hdtn::Logger::getInstance()->logError("ingress",                    
                        "Egress blockhdr message mismatch: untruncated = " + std::to_string(res->untruncated_size)
                        + " truncated = " + std::to_string(res->size) + " expected = " + 
                        std::to_string(sizeof(hdtn::BlockHdr)));
                }
                else {
                    bool needsNotify = false;
                    {
                        boost::mutex::scoped_lock lock(m_storageAckQueueMutex);
                        if (m_storageAckQueue.empty()) {
                            std::cerr << "error m_storageAckQueue is empty" << std::endl;
                            hdtn::Logger::getInstance()->logError("ingress", "Error m_storageAckQueue is empty");
                        }
                        else if (*m_storageAckQueue.front() == rblk) {
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

void Ingress::SchedulerEventHandler() {
    zmq::message_t message;
    if (!m_zmqSubSock_boundSchedulerToConnectingIngressPtr->recv(message, zmq::recv_flags::none)) {
	 std::cerr << "Ingress didn't receive event from scheduler" << std::endl;
         return;
     }
     if (message.size() < sizeof(CommonHdr)) {
         return;
     }
     CommonHdr *common = (CommonHdr *)message.data();

     hdtn::BlockHdr blockHdr;
     memcpy(&blockHdr, message.data(), sizeof(hdtn::BlockHdr));

     switch (common->type) {
     	case HDTN_MSGTYPE_ILINKUP:
            std::cout << "Link available send bundles to egress for flowId" << blockHdr.flowId << std::endl;
	    m_sendToStorage[blockHdr.flowId] = false;
	    
            break;
        case HDTN_MSGTYPE_ILINKDOWN:
            std::cout << "Link unavailable Send bundles to storage for flowId " << blockHdr.flowId << std::endl;
            m_sendToStorage[blockHdr.flowId] = true;
            break;
    }
}

static void CustomCleanup(void *data, void *hint) {
    //std::cout << "free " << static_cast<std::vector<uint8_t>*>(hint)->size() << std::endl;
    delete static_cast<std::vector<uint8_t>*>(hint);
}

static void CustomIgnoreCleanupBlockHdr(void *data, void *hint) {}

int Ingress::Process(std::vector<uint8_t> && rxBuf) {  //TODO: make buffer zmq message to reduce copy
    const std::size_t messageSize = rxBuf.size();
	//uint64_t timer = 0;
    uint64_t offset = 0;
    uint32_t zframeSeq = 0;
    bpv6_primary_block bpv6Primary;
    bpv6_eid dst;
    memset(&bpv6Primary, 0, sizeof(bpv6_primary_block));
    {
        const char * const tbuf = (const char*)rxBuf.data(); //char tbuf[HMSG_MSG_MAX];
        
        ////timer = rdtsc();
        //memcpy(tbuf, m_bufs[i], recvlen);
        //hdr.ts = timer;
        offset = bpv6_primary_block_decode(&bpv6Primary, (const char*)rxBuf.data(), offset, messageSize);
        const uint64_t absExpirationUsec = (bpv6Primary.creation * 1000000) + bpv6Primary.sequence + bpv6Primary.lifetime;
        const uint32_t priority = bpv6_bundle_get_priority(bpv6Primary.flags);
        //std::cout << "p: " << priority << " lt " << bpv6Primary.lifetime << " c " << bpv6Primary.creation << ":" << bpv6Primary.sequence << " a: " << absExpirationUsec <<  std::endl;
        dst.node = bpv6Primary.dst_node;

        
        
        //m_storageAckQueueMutex.lock();
        //m_storageAckQueue.
        //
        //m_storageAckQueueMutex.unlock();
        std::unique_ptr<BlockHdr> hdrPtr = boost::make_unique<BlockHdr>();
        hdtn::BlockHdr & hdr = *hdrPtr;
        memset(&hdr, 0, sizeof(hdtn::BlockHdr));
        hdr.flowId = static_cast<uint32_t>(dst.node);  // for now
        hdr.base.flags = static_cast<uint16_t>(bpv6Primary.flags);
	hdr.base.type = (m_sendToStorage[hdr.flowId]) ? HDTN_MSGTYPE_STORE : HDTN_MSGTYPE_EGRESS;
        hdr.ts = absExpirationUsec; //use ts for now
        hdr.ttl = priority; //use ttl for now
        // hdr.ts=recvlen;
        std::size_t numChunks = 1;
        std::size_t bytesToSend = messageSize;
        std::size_t remainder = 0;
	
	//std::cout << "@nadia Ingress hdr type for flowId " << hdr.base.type  << std::endl; 
        
        
        zframeSeq = 0;
        if (messageSize > CHUNK_SIZE)  // the bundle is bigger than internal message size limit
        {
            numChunks = messageSize / CHUNK_SIZE;
            bytesToSend = CHUNK_SIZE;
            remainder = messageSize % CHUNK_SIZE;
            if (remainder != 0) numChunks++;
        }
        for (std::size_t j = 0; j < numChunks; ++j) {
            if ((j == numChunks - 1) && (remainder != 0)) bytesToSend = remainder;
            hdr.bundleSeq = m_ingSequenceNum;  //TODO make thread safe
            m_ingSequenceNum++;
            hdr.zframe = zframeSeq;
            zframeSeq++;
            if (hdr.base.type == HDTN_MSGTYPE_EGRESS) {
                m_egressAckMapQueueMutex.lock();
                EgressToIngressAckingQueue & egressToIngressAckingObj = m_egressAckMapQueue[hdr.flowId];
                m_egressAckMapQueueMutex.unlock();
                for (unsigned int attempt = 0; attempt < 8; ++attempt) { //2000 ms timeout
                    if (egressToIngressAckingObj.GetQueueSize() > m_hdtnConfig.m_zmqMaxMessagesPerPath) {
                        if (attempt == 7) {
                            std::cerr << "error: too many pending egress acks in the queue for flow id " << hdr.flowId << std::endl;
                            hdtn::Logger::getInstance()->logError("ingress", 
                                "Error: too many pending egress acks in the queue for flow id " + std::to_string(hdr.flowId));
                        }
                        egressToIngressAckingObj.WaitUntilNotifiedOr250MsTimeout();
                        //thread is now unblocked, and the lock is reacquired by invoking lock.lock()
                        ++m_eventsTooManyInEgressQueue;
                        continue; //next attempt
                    }
                    {
                        zmq::message_t messageWithDataStolen(hdrPtr.get(), sizeof(hdtn::BlockHdr), CustomIgnoreCleanupBlockHdr); //cleanup will occur in the queue below
                        boost::mutex::scoped_lock lock(m_ingressToEgressZmqSocketMutex);
                        if (!m_zmqPushSock_boundIngressToConnectingEgressPtr->send(messageWithDataStolen, zmq::send_flags::sndmore | zmq::send_flags::dontwait)) {
                            std::cerr << "ingress can't send BlockHdr to egress" << std::endl;
                            hdtn::Logger::getInstance()->logError("ingress", "Ingress can't send BlockHdr to egress");
                        }
                        else {
                            egressToIngressAckingObj.PushMove_ThreadSafe(hdrPtr);
                            if (numChunks == 1) {
                                //this is an optimization because we only have one chunk to send
                                //The zmq_msg_init_data() function shall initialise the message object referenced by msg
                                //to represent the content referenced by the buffer located at address data, size bytes long.
                                //No copy of data shall be performed and 0MQ shall take ownership of the supplied buffer.
                                //If provided, the deallocation function ffn shall be called once the data buffer is no longer
                                //required by 0MQ, with the data and hint arguments supplied to zmq_msg_init_data().
                                std::vector<uint8_t> * rxBufRawPointer = new std::vector<uint8_t>(std::move(rxBuf));
                                zmq::message_t messageWithDataStolen(rxBufRawPointer->data(), rxBufRawPointer->size(), CustomCleanup, rxBufRawPointer);
                                if (!m_zmqPushSock_boundIngressToConnectingEgressPtr->send(std::move(messageWithDataStolen), zmq::send_flags::dontwait)) {
                                    std::cerr << "ingress can't send bundle to egress" << std::endl;
                                    hdtn::Logger::getInstance()->logError("ingress", "Ingress can't send bundle to egress");

                                }
                                else {
                                    //success                            
                                    ++m_bundleCountEgress;
                                }
                            }
                            else {
                                if (!m_zmqPushSock_boundIngressToConnectingEgressPtr->send(zmq::const_buffer(&tbuf[CHUNK_SIZE * j], bytesToSend), zmq::send_flags::dontwait)) {
                                    std::cerr << "ingress can't send bundle to egress" << std::endl;
                                    hdtn::Logger::getInstance()->logError("ingress", "Ingress can't send bundle to egress");
                                }
                                else {
                                    //success                            
                                    ++m_bundleCountEgress;
                                }
                            }
                        }
                    }
                    break;

                }
            }
            else { //storage
                boost::mutex::scoped_lock lock(m_storageAckQueueMutex);
                for(unsigned int attempt = 0; attempt < 8; ++attempt) { //2000 ms timeout
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

                    zmq::message_t messageWithDataStolen(hdrPtr.get(), sizeof(hdtn::BlockHdr), CustomIgnoreCleanupBlockHdr); //cleanup will occur in the queue below

                    //zmq threads not thread safe but protected by mutex above
                    if (!m_zmqPushSock_boundIngressToConnectingStoragePtr->send(messageWithDataStolen, zmq::send_flags::sndmore | zmq::send_flags::dontwait)) {
                        std::cerr << "ingress can't send BlockHdr to storage" << std::endl;
                        hdtn::Logger::getInstance()->logError("ingress", "Ingress can't send BlockHdr to storage");
                    }
                    else {
                        m_storageAckQueue.push(std::move(hdrPtr));
                        if (numChunks == 1) {
                            //this is an optimization because we only have one chunk to send
                            //The zmq_msg_init_data() function shall initialise the message object referenced by msg
                            //to represent the content referenced by the buffer located at address data, size bytes long.
                            //No copy of data shall be performed and 0MQ shall take ownership of the supplied buffer.
                            //If provided, the deallocation function ffn shall be called once the data buffer is no longer
                            //required by 0MQ, with the data and hint arguments supplied to zmq_msg_init_data().
                            std::vector<uint8_t> * rxBufRawPointer = new std::vector<uint8_t>(std::move(rxBuf));
                            zmq::message_t messageWithDataStolen(rxBufRawPointer->data(), rxBufRawPointer->size(), CustomCleanup, rxBufRawPointer);
                            if (!m_zmqPushSock_boundIngressToConnectingStoragePtr->send(std::move(messageWithDataStolen), zmq::send_flags::dontwait)) {
                                std::cerr << "ingress can't send bundle to storage" << std::endl;
                                hdtn::Logger::getInstance()->logError("ingress", "Ingress can't send bundle to storage");
                            }
                            else {
                                //success                            
                                ++m_bundleCountStorage;
                            }
                        }
                        else {
                            if (!m_zmqPushSock_boundIngressToConnectingStoragePtr->send(zmq::const_buffer(&tbuf[CHUNK_SIZE * j], bytesToSend), zmq::send_flags::dontwait)) {
                                std::cerr << "ingress can't send bundle to storage" << std::endl;
                                hdtn::Logger::getInstance()->logError("ingress", "Ingress can't send bundle to storage");
                            }
                            else {
                                //success                            
                                ++m_bundleCountStorage;
                            }
                        }
                    }
                    break;
                    
                }
            }
            
            ++m_zmsgsOut;
        }
        
        m_bundleData += messageSize;
        ////timer = rdtsc() - timer;
    }
    return 0;
}



void Ingress::WholeBundleReadyCallback(std::vector<uint8_t> & wholeBundleVec) {
    //if more than 1 BpSinkAsync context, must protect shared resources with mutex.  Each BpSinkAsync context has
    //its own processing thread that calls this callback
    Process(std::move(wholeBundleVec));
}

}  // namespace hdtn
