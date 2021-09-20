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

void hdtn::HegrManagerAsync::Init(const HdtnConfig & hdtnConfig, zmq::context_t * hdtnOneProcessZmqInprocContextPtr) {

    if (m_running) {
        std::cerr << "error: HegrManagerAsync::Init called while Egress is already running" << std::endl;
        return;
    }

    m_hdtnConfig = hdtnConfig;

    if (!m_outductManager.LoadOutductsFromConfig(m_hdtnConfig.m_outductsConfig, m_hdtnConfig.m_myNodeId)) {
        return;
    }

    m_outductManager.SetOutductManagerOnSuccessfulOutductAckCallback(boost::bind(&HegrManagerAsync::OnSuccessfulBundleAck, this, boost::placeholders::_1));

    
    m_bundleCount = 0;
    m_bundleData = 0;
    m_messageCount = 0;
    if (hdtnOneProcessZmqInprocContextPtr) {
        // socket for cut-through mode straight to egress
        //The io_threads argument specifies the size of the 0MQ thread pool to handle I/O operations.
        //If your application is using only the inproc transport for messaging you may set this to zero, otherwise set it to at least one.
        m_zmqPullSock_boundIngressToConnectingEgressPtr = boost::make_unique<zmq::socket_t>(*hdtnOneProcessZmqInprocContextPtr, zmq::socket_type::pair);
        m_zmqPullSock_boundIngressToConnectingEgressPtr->connect(std::string("inproc://bound_ingress_to_connecting_egress"));
        m_zmqPushSock_connectingEgressToBoundIngressPtr = boost::make_unique<zmq::socket_t>(*hdtnOneProcessZmqInprocContextPtr, zmq::socket_type::pair);
        m_zmqPushSock_connectingEgressToBoundIngressPtr->connect(std::string("inproc://connecting_egress_to_bound_ingress"));
        // socket for bundles from storage
        m_zmqPullSock_connectingStorageToBoundEgressPtr = boost::make_unique<zmq::socket_t>(*hdtnOneProcessZmqInprocContextPtr, zmq::socket_type::pair);
        m_zmqPullSock_connectingStorageToBoundEgressPtr->bind(std::string("inproc://connecting_storage_to_bound_egress"));
        m_zmqPushSock_boundEgressToConnectingStoragePtr = boost::make_unique<zmq::socket_t>(*hdtnOneProcessZmqInprocContextPtr, zmq::socket_type::pair);
        m_zmqPushSock_boundEgressToConnectingStoragePtr->bind(std::string("inproc://bound_egress_to_connecting_storage"));
    }
    else {
        // socket for cut-through mode straight to egress
        m_zmqCtx_ingressEgressPtr = boost::make_unique<zmq::context_t>();
        m_zmqPullSock_boundIngressToConnectingEgressPtr = boost::make_unique<zmq::socket_t>(*m_zmqCtx_ingressEgressPtr, zmq::socket_type::pull);
        const std::string connect_boundIngressToConnectingEgressPath(
            std::string("tcp://") +
            m_hdtnConfig.m_zmqIngressAddress +
            std::string(":") +
            boost::lexical_cast<std::string>(m_hdtnConfig.m_zmqBoundIngressToConnectingEgressPortPath));
        m_zmqPullSock_boundIngressToConnectingEgressPtr->connect(connect_boundIngressToConnectingEgressPath);
        m_zmqPushSock_connectingEgressToBoundIngressPtr = boost::make_unique<zmq::socket_t>(*m_zmqCtx_ingressEgressPtr, zmq::socket_type::push);
        const std::string connect_connectingEgressToBoundIngressPath(
            std::string("tcp://") +
            m_hdtnConfig.m_zmqIngressAddress +
            std::string(":") +
            boost::lexical_cast<std::string>(m_hdtnConfig.m_zmqConnectingEgressToBoundIngressPortPath));
        m_zmqPushSock_connectingEgressToBoundIngressPtr->connect(connect_connectingEgressToBoundIngressPath);
        // socket for bundles from storage
        m_zmqCtx_storageEgressPtr = boost::make_unique<zmq::context_t>();
        m_zmqPullSock_connectingStorageToBoundEgressPtr = boost::make_unique<zmq::socket_t>(*m_zmqCtx_storageEgressPtr, zmq::socket_type::pull);
        const std::string bind_connectingStorageToBoundEgressPath(
            std::string("tcp://*:") + boost::lexical_cast<std::string>(m_hdtnConfig.m_zmqConnectingStorageToBoundEgressPortPath));
        m_zmqPullSock_connectingStorageToBoundEgressPtr->bind(bind_connectingStorageToBoundEgressPath);
        m_zmqPushSock_boundEgressToConnectingStoragePtr = boost::make_unique<zmq::socket_t>(*m_zmqCtx_storageEgressPtr, zmq::socket_type::push);
        const std::string bind_boundEgressToConnectingStoragePath(
            std::string("tcp://*:") + boost::lexical_cast<std::string>(m_hdtnConfig.m_zmqBoundEgressToConnectingStoragePortPath));
        m_zmqPushSock_boundEgressToConnectingStoragePtr->bind(bind_boundEgressToConnectingStoragePath);
    }
    
    if (!m_running) {
        m_running = true;
        m_threadZmqReaderPtr = boost::make_unique<boost::thread>(
            boost::bind(&HegrManagerAsync::ReadZmqThreadFunc, this)); //create and start the worker thread
    }
}

void hdtn::HegrManagerAsync::OnSuccessfulBundleAck(uint64_t outductUuidIndex) {
    m_conditionVariableProcessZmqMessages.notify_one();
}

static void CustomCleanupEgressAckHdrNoHint(void *data, void *hint) {
    (void)hint;
    //std::cout << "free " << static_cast<hdtn::BlockHdr *>(data)->flowId << std::endl;
    delete static_cast<hdtn::EgressAckHdr *>(data);
}

void hdtn::HegrManagerAsync::ProcessZmqMessagesThreadFunc (
    CircularIndexBufferSingleProducerSingleConsumerConfigurable & cb,
    std::vector<hdtn::ToEgressHdr> & toEgressHeaderMessages,
    std::vector<zmq::message_t> & payloadMessages)
{
    std::cout << "starting ProcessZmqMessagesThreadFunc with cb size " << toEgressHeaderMessages.size() << std::endl;
    hdtn::Logger::getInstance()->logNotification("egress", "Starting ProcessZmqMessagesThreadFunc with cb size " + std::to_string(toEgressHeaderMessages.size()));
    std::size_t totalCustodyTransfersSentToStorage = 0;
    std::size_t totalCustodyTransfersSentToIngress = 0;
    
    typedef std::queue<std::unique_ptr<hdtn::EgressAckHdr> > queue_t;
    typedef std::map<uint64_t, queue_t> outductuuid_needacksqueue_map_t;

    outductuuid_needacksqueue_map_t outductUuidToNeedAcksQueueMap;

    boost::mutex localMutex;
    boost::mutex::scoped_lock lock(localMutex);

    while (m_running || (cb.GetIndexForRead() != UINT32_MAX)) { //keep thread alive if running or cb not empty


        const unsigned int consumeIndex = cb.GetIndexForRead(); //store the volatile

        if (consumeIndex != UINT32_MAX) { //if not empty
            const hdtn::ToEgressHdr & toEgressHeader = toEgressHeaderMessages[consumeIndex];
            zmq::message_t & zmqMessage = payloadMessages[consumeIndex];
            const cbhe_eid_t & finalDestEid = toEgressHeader.finalDestEid;
            if (Outduct * outduct = m_outductManager.GetOutductByFinalDestinationEid(finalDestEid)) {
                std::unique_ptr<hdtn::EgressAckHdr> egressAckPtr = boost::make_unique<hdtn::EgressAckHdr>();
                //memset 0 not needed because all values set below
                egressAckPtr->base.type = (toEgressHeader.isCutThroughFromIngress) ? HDTN_MSGTYPE_EGRESS_ACK_TO_INGRESS : HDTN_MSGTYPE_EGRESS_ACK_TO_STORAGE;
                egressAckPtr->base.flags = 0;
                egressAckPtr->finalDestEid = finalDestEid;
                egressAckPtr->error = 0; //can set later before sending this ack if error
                egressAckPtr->deleteNow = !toEgressHeader.hasCustody;
                egressAckPtr->isToStorage = !toEgressHeader.isCutThroughFromIngress;
                egressAckPtr->custodyId = toEgressHeader.custodyId;

                outductUuidToNeedAcksQueueMap[outduct->GetOutductUuid()].push(std::move(egressAckPtr));
                outduct->Forward(zmqMessage);
                if (zmqMessage.size() != 0) {
                    std::cout << "Error in hdtn::HegrManagerAsync::ProcessZmqMessagesThreadFunc, zmqMessage was not moved" << std::endl;
                    hdtn::Logger::getInstance()->logError("egress", "Error in hdtn::HegrManagerAsync::ProcessZmqMessagesThreadFunc, zmqMessage was not moved");
                }
            }
            else {
                std::cerr << "critical error in HegrManagerAsync::ProcessZmqMessagesThreadFunc: no outduct for flow id " << finalDestEid.nodeId << std::endl;
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
                        std::unique_ptr<hdtn::EgressAckHdr> & qItem = q.front();
                        const bool isToStorage = qItem->isToStorage;
                        //this is an optimization because we only have one chunk to send
                        //The zmq_msg_init_data() function shall initialise the message object referenced by msg
                        //to represent the content referenced by the buffer located at address data, size bytes long.
                        //No copy of data shall be performed and 0MQ shall take ownership of the supplied buffer.
                        //If provided, the deallocation function ffn shall be called once the data buffer is no longer
                        //required by 0MQ, with the data and hint arguments supplied to zmq_msg_init_data().
                        zmq::message_t messageWithDataStolen(qItem.release(), sizeof(hdtn::EgressAckHdr), CustomCleanupEgressAckHdrNoHint); //unique_ptr goes "null" with release()
                        if (isToStorage) {
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

    static const unsigned int NUM_ZMQ_MESSAGES_CB = 40;
    CircularIndexBufferSingleProducerSingleConsumerConfigurable cb(NUM_ZMQ_MESSAGES_CB);
    std::vector<hdtn::ToEgressHdr> toEgressHeaderMessages(NUM_ZMQ_MESSAGES_CB);
    std::vector<zmq::message_t> payloadMessages(NUM_ZMQ_MESSAGES_CB);
    boost::thread processZmqMessagesThread(boost::bind(
        &hdtn::HegrManagerAsync::ProcessZmqMessagesThreadFunc,
        this,
        boost::ref(cb),
        boost::ref(toEgressHeaderMessages),
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
                hdtn::ToEgressHdr & toEgressHeader = toEgressHeaderMessages[writeIndex];
                const zmq::recv_buffer_result_t res = sockets[itemIndex]->recv(zmq::mutable_buffer(&toEgressHeader, sizeof(hdtn::ToEgressHdr)), zmq::recv_flags::none);
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
                ++m_messageCount;
                
                
                zmq::message_t & zmqMessage = payloadMessages[writeIndex];
                //message guaranteed to be there due to the zmq::send_flags::sndmore
                if (!sockets[itemIndex]->recv(zmqMessage, zmq::recv_flags::none)) {
                    std::cerr << "error on sockets[itemIndex]->recv\n";
                    continue;
                }
                       
                m_bundleData += zmqMessage.size();
                ++m_bundleCount;

                cb.CommitWrite();
                m_conditionVariableProcessZmqMessages.notify_one();
            }
        }
    }
    processZmqMessagesThread.join();

    std::cout << "HegrManagerAsync::ReadZmqThreadFunc thread exiting\n";
    hdtn::Logger::getInstance()->logNotification("egress", "HegrManagerAsync::ReadZmqThreadFunc thread exiting");
}
