/***************************************************************************
 * NASA Glenn Research Center, Cleveland, OH
 * Released under the NASA Open Source Agreement (NOSA)
 * May  2021
 ****************************************************************************
 */

#include <string.h>

#include "EgressAsync.h"
#include <boost/lexical_cast.hpp>
#include <boost/make_shared.hpp>
#include <boost/make_unique.hpp>

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

void hdtn::HegrManagerAsync::Init(const OutductsConfig & outductsConfig) {

    if (m_running) {
        std::cerr << "error: HegrManagerAsync::Init called while Egress is already running" << std::endl;
        return;
    }

    if (!m_outductManager.LoadOutductsFromConfig(outductsConfig)) {
        return;
    }

    m_outductManager.SetOutductManagerOnSuccessfulOutductAckCallback(boost::bind(&HegrManagerAsync::OnSuccessfulBundleAck, this, boost::placeholders::_1));

    for (unsigned int i = 0; i <= 10; ++i) {
        std::cout << "Waiting for all Outducts to become ready to forward..." << std::endl;
        boost::this_thread::sleep(boost::posix_time::milliseconds(500));
        if (m_outductManager.AllReadyToForward()) {
            std::cout << "Outducts ready to forward" << std::endl;
            break;
        }
        if (i == 10) {
            std::cerr << "Outducts unable to connect" << std::endl;
            return;
        }
    }
    m_bundleCount = 0;
    m_bundleData = 0;
    m_messageCount = 0;
    // socket for cut-through mode straight to egress
    m_zmqCtx_ingressEgressPtr = boost::make_unique<zmq::context_t>();
    m_zmqPullSock_boundIngressToConnectingEgressPtr = boost::make_unique<zmq::socket_t>(*m_zmqCtx_ingressEgressPtr, zmq::socket_type::pull);
    m_zmqPullSock_boundIngressToConnectingEgressPtr->connect(HDTN_BOUND_INGRESS_TO_CONNECTING_EGRESS_PATH);
    m_zmqPushSock_connectingEgressToBoundIngressPtr = boost::make_unique<zmq::socket_t>(*m_zmqCtx_ingressEgressPtr, zmq::socket_type::push);
    m_zmqPushSock_connectingEgressToBoundIngressPtr->connect(HDTN_CONNECTING_EGRESS_TO_BOUND_INGRESS_PATH);
    // socket for sending bundles to storage
    m_zmqCtx_storageEgressPtr = boost::make_unique<zmq::context_t>();
    m_zmqPullSock_connectingStorageToBoundEgressPtr = boost::make_unique<zmq::socket_t>(*m_zmqCtx_storageEgressPtr, zmq::socket_type::pull);
    m_zmqPullSock_connectingStorageToBoundEgressPtr->bind(HDTN_CONNECTING_STORAGE_TO_BOUND_EGRESS_PATH);
    m_zmqPushSock_boundEgressToConnectingStoragePtr = boost::make_unique<zmq::socket_t>(*m_zmqCtx_storageEgressPtr, zmq::socket_type::push);
    m_zmqPushSock_boundEgressToConnectingStoragePtr->bind(HDTN_BOUND_EGRESS_TO_CONNECTING_STORAGE_PATH);

    
    if (!m_running) {
        m_running = true;
        m_threadZmqReaderPtr = boost::make_unique<boost::thread>(
            boost::bind(&HegrManagerAsync::ReadZmqThreadFunc, this)); //create and start the worker thread
    }
}

void hdtn::HegrManagerAsync::OnSuccessfulBundleAck(uint64_t outductUuidIndex) {
    m_conditionVariableProcessZmqMessages.notify_one();
}

static void CustomCleanupBlockHdr(void *data, void *hint) {
    //std::cout << "free " << static_cast<hdtn::BlockHdr *>(data)->flowId << std::endl;
    delete static_cast<hdtn::BlockHdr *>(data);
}

void hdtn::HegrManagerAsync::ProcessZmqMessagesThreadFunc (
    CircularIndexBufferSingleProducerSingleConsumerConfigurable & cb,
    std::vector<std::unique_ptr<hdtn::BlockHdr> > & headerMessages,
    std::vector<bool> & isFromStorage,
    std::vector<zmq::message_t> & payloadMessages)
{
    std::cout << "starting ProcessZmqMessagesThreadFunc with cb size " << headerMessages.size() << std::endl;
    hdtn::Logger::getInstance()->logNotification("egress", "Starting ProcessZmqMessagesThreadFunc with cb size " + std::to_string(headerMessages.size()));
    std::size_t totalCustodyTransfersSentToStorage = 0;
    std::size_t totalCustodyTransfersSentToIngress = 0;
    
    typedef std::queue<std::unique_ptr<hdtn::BlockHdr> > queue_t;
    typedef std::map<uint64_t, queue_t> outductuuid_needacksqueue_map_t;

    outductuuid_needacksqueue_map_t outductUuidToNeedAcksQueueMap;

    boost::mutex localMutex;
    boost::mutex::scoped_lock lock(localMutex);

    while (m_running || (cb.GetIndexForRead() != UINT32_MAX)) { //keep thread alive if running or cb not empty


        const unsigned int consumeIndex = cb.GetIndexForRead(); //store the volatile

        if (consumeIndex != UINT32_MAX) { //if not empty
            std::unique_ptr<hdtn::BlockHdr> & blockHdrPtr = headerMessages[consumeIndex];
            zmq::message_t & zmqMessage = payloadMessages[consumeIndex];
            const uint16_t type = blockHdrPtr->base.type;
            if ((type == HDTN_MSGTYPE_STORE) || (type == HDTN_MSGTYPE_EGRESS)) {
                const uint32_t flowId = blockHdrPtr->flowId;
                if (Outduct * outduct = m_outductManager.GetOutductByFlowId(flowId)) {
                    if (isFromStorage[consumeIndex]) { //reply to storage
                        blockHdrPtr->base.type = HDTN_MSGTYPE_EGRESS_TRANSFERRED_CUSTODY;
                    }
                    outductUuidToNeedAcksQueueMap[outduct->GetOutductUuid()].push(std::move(blockHdrPtr));
                    outduct->Forward(zmqMessage);
                    if (zmqMessage.size() != 0) {
                        std::cout << "Error in hdtn::HegrManagerAsync::ProcessZmqMessagesThreadFunc, zmqMessage was not moved" << std::endl;
                        hdtn::Logger::getInstance()->logError("egress", "Error in hdtn::HegrManagerAsync::ProcessZmqMessagesThreadFunc, zmqMessage was not moved");
                    }
                }
                else {
                    std::cerr << "critical error in HegrManagerAsync::ProcessZmqMessagesThreadFunc: no outduct for flow id " << flowId << std::endl;
                }
            }
            cb.CommitRead();
        }
        else {
            //Check for tcpcl acks from a bpsink-like program.
            //When acked, send an ack to storage containing the head segment id so that the bundle can be deleted from storage.
            //We will assume that when the bpsink acks the packet through tcpcl that this will be custody transfer of the bundle
            // and that storage is no longer responsible for it.  Tcpcl must be acked sequentially but storage doesn't care the
            // order of the acks.
            for (outductuuid_needacksqueue_map_t::iterator it = outductUuidToNeedAcksQueueMap.begin(); it != outductUuidToNeedAcksQueueMap.end(); ++it) {
                const uint64_t outductUuid = it->first;
                //const unsigned int fec = 1; //TODO
                queue_t & q = it->second;
                if (Outduct * outduct = m_outductManager.GetOutductByOutductUuid(outductUuid)) {
                    const std::size_t numAckedRemaining = outduct->GetTotalDataSegmentsUnacked();
                    while (q.size() > numAckedRemaining) {
                        std::unique_ptr<hdtn::BlockHdr> & qItem = q.front();
                        const uint16_t type = qItem->base.type;
                        //this is an optimization because we only have one chunk to send
                        //The zmq_msg_init_data() function shall initialise the message object referenced by msg
                        //to represent the content referenced by the buffer located at address data, size bytes long.
                        //No copy of data shall be performed and 0MQ shall take ownership of the supplied buffer.
                        //If provided, the deallocation function ffn shall be called once the data buffer is no longer
                        //required by 0MQ, with the data and hint arguments supplied to zmq_msg_init_data().
                        zmq::message_t messageWithDataStolen(qItem.release(), sizeof(hdtn::BlockHdr), CustomCleanupBlockHdr); //unique_ptr goes "null" with release()
                        if (type == HDTN_MSGTYPE_EGRESS_TRANSFERRED_CUSTODY) {
                            if (!m_zmqPushSock_boundEgressToConnectingStoragePtr->send(std::move(messageWithDataStolen), zmq::send_flags::dontwait)) {
                                std::cout << "error: m_zmqPushSock_boundEgressToConnectingStoragePtr could not send" << std::endl;
                                hdtn::Logger::getInstance()->logError("egress", "Error: m_zmqPushSock_boundEgressToConnectingStoragePtr could not send");
                                break;
                            }
                            ++totalCustodyTransfersSentToStorage;
                        }
                        else {
                            //send ack message by echoing back the block
                            if (!m_zmqPushSock_connectingEgressToBoundIngressPtr->send(std::move(messageWithDataStolen), zmq::send_flags::dontwait)) {
                                std::cout << "error: zmq could not send ingress an ack from egress" << std::endl;
                                hdtn::Logger::getInstance()->logError("egress", "Error: zmq could not send ingress an ack from egress");
                                break;
                            }
                            ++totalCustodyTransfersSentToIngress;
                        }
                        q.pop();
                    }
                }
                else {
                    std::cerr << "critical error in HegrManagerAsync::ProcessZmqMessagesThreadFunc: cannot find outductUuid " << outductUuid << std::endl;
                }
            }
            m_conditionVariableProcessZmqMessages.timed_wait(lock, boost::posix_time::milliseconds(100)); // call lock.unlock() and blocks the current thread
            //thread is now unblocked, and the lock is reacquired by invoking lock.lock()
        }

    }

    std::cout << "ProcessZmqMessagesThreadFunc thread exiting\n";
    hdtn::Logger::getInstance()->logNotification("egress", "ProcessZmqMessagesThreadFunc thread exiting");
    std::cout << "totalCustodyTransfersSentToStorage: " << totalCustodyTransfersSentToStorage << std::endl;
    hdtn::Logger::getInstance()->logInfo("egress", "totalCustodyTransfersSentToStorage: " + std::to_string(totalCustodyTransfersSentToStorage));
    std::cout << "totalCustodyTransfersSentToIngress: " << totalCustodyTransfersSentToIngress << std::endl;
    hdtn::Logger::getInstance()->logInfo("egress", "totalCustodyTransfersSentToIngress: " + std::to_string(totalCustodyTransfersSentToIngress));
}

void hdtn::HegrManagerAsync::ReadZmqThreadFunc() {

    static const unsigned int NUM_ZMQ_MESSAGES_CB = 40;
    CircularIndexBufferSingleProducerSingleConsumerConfigurable cb(NUM_ZMQ_MESSAGES_CB);
    std::vector<std::unique_ptr<hdtn::BlockHdr> > headerMessages(NUM_ZMQ_MESSAGES_CB);
    std::vector<bool> isFromStorage(NUM_ZMQ_MESSAGES_CB);
    std::vector<zmq::message_t> payloadMessages(NUM_ZMQ_MESSAGES_CB);
    boost::thread processZmqMessagesThread(boost::bind(
        &hdtn::HegrManagerAsync::ProcessZmqMessagesThreadFunc,
        this,
        boost::ref(cb),
        boost::ref(headerMessages),
        boost::ref(isFromStorage),
        boost::ref(payloadMessages)));

    // Use a form of receive that times out so we can terminate cleanly.
    static const int timeout = 250;  // milliseconds
    static const unsigned int NUM_SOCKETS = 2;
    m_zmqPullSock_boundIngressToConnectingEgressPtr->set(zmq::sockopt::rcvtimeo, timeout);
    m_zmqPullSock_connectingStorageToBoundEgressPtr->set(zmq::sockopt::rcvtimeo, timeout);
    zmq::pollitem_t items[NUM_SOCKETS] = {
        {m_zmqPullSock_boundIngressToConnectingEgressPtr->handle(), 0, ZMQ_POLLIN, 0},
        {m_zmqPullSock_connectingStorageToBoundEgressPtr->handle(), 0, ZMQ_POLLIN, 0}
    };
    zmq::socket_t * const sockets[NUM_SOCKETS] = {
        m_zmqPullSock_boundIngressToConnectingEgressPtr.get(),
        m_zmqPullSock_connectingStorageToBoundEgressPtr.get()
    };

    static const long DEFAULT_BIG_TIMEOUT_POLL = 250;
    while (m_running) { //keep thread alive if running       
        if (zmq::poll(&items[0], NUM_SOCKETS, DEFAULT_BIG_TIMEOUT_POLL) > 0) {
            for (unsigned int itemIndex = 0; itemIndex < NUM_SOCKETS; ++itemIndex) {
                if ((items[itemIndex].revents & ZMQ_POLLIN) == 0) {
                    continue;
                }

                const unsigned int writeIndex = cb.GetIndexForWrite();
                if (writeIndex == UINT32_MAX) {
                    std::cerr << "error in hdtn::HegrManagerAsync::ReadZmqThreadFunc(): cb is full" << std::endl;
                    hdtn::Logger::getInstance()->logError("egress", "Error in hdtn::HegrManagerAsync::ReadZmqThreadFunc(): cb is full");
                    continue;
                }
                headerMessages[writeIndex] = boost::make_unique<hdtn::BlockHdr>();
                std::unique_ptr<hdtn::BlockHdr> & hdrPtr = headerMessages[writeIndex];
                const zmq::recv_buffer_result_t res = sockets[itemIndex]->recv(zmq::mutable_buffer(hdrPtr.get(), sizeof(hdtn::BlockHdr)), zmq::recv_flags::none);
                if (!res) {
                    std::cerr << "error in HegrManagerAsync::ReadZmqThreadFunc: cannot read BlockHdr" << std::endl;
                    hdtn::Logger::getInstance()->logError("egress", "Error in HegrManagerAsync::ReadZmqThreadFunc: cannot read BlockHdr");
                    continue;
                }
                else if ((res->truncated()) || (res->size != sizeof(hdtn::BlockHdr))) {
                    std::cerr << "egress blockhdr message mismatch: untruncated = " << res->untruncated_size 
                        << " truncated = " << res->size << " expected = " << sizeof(hdtn::BlockHdr) << std::endl;
                    hdtn::Logger::getInstance()->logError("egress", "Egress blockhdr message mismatch: untruncated = " + 
                        std::to_string(res->untruncated_size) +  " truncated = " + std::to_string(res->size) + " expected = " + 
                        std::to_string(sizeof(hdtn::BlockHdr)));
                    continue;
                }
                ++m_messageCount;
                
                const uint16_t type = hdrPtr->base.type;
                if ((type == HDTN_MSGTYPE_STORE) || (type == HDTN_MSGTYPE_EGRESS)) {
                    zmq::message_t & zmqMessage = payloadMessages[writeIndex];
                    // Use a form of receive that times out so we can terminate cleanly.  If no
                    // message was received after timeout go back to top of loop
                    // std::cout << "In runEgress, before recv. " << std::endl << std::flush;
                    while (m_running) {
                        if (!sockets[itemIndex]->recv(zmqMessage, zmq::recv_flags::none)) {
                            continue;
                        }
                        isFromStorage[writeIndex] = (itemIndex == 1);
                       
                        m_bundleData += zmqMessage.size();
                        ++m_bundleCount;

                        cb.CommitWrite();
                        m_conditionVariableProcessZmqMessages.notify_one();
                        break;
                    }                    
                }
            }
        }
    }
    processZmqMessagesThread.join();

    std::cout << "HegrManagerAsync::ReadZmqThreadFunc thread exiting\n";
    hdtn::Logger::getInstance()->logNotification("egress", "HegrManagerAsync::ReadZmqThreadFunc thread exiting");
}
