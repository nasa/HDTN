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

    if (!m_outductManager.LoadOutductsFromConfig(m_hdtnConfig.m_outductsConfig, m_hdtnConfig.m_myNodeId, m_hdtnConfig.m_maxLtpReceiveUdpPacketSizeBytes)) {
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
        //socket for interrupt to zmq_poll (acts as condition_variable.notify_one())
        m_zmqPullSignalInprocSockPtr = boost::make_unique<zmq::socket_t>(*hdtnOneProcessZmqInprocContextPtr, zmq::socket_type::pair);
        m_zmqPushSignalInprocSockPtr = boost::make_unique<zmq::socket_t>(*hdtnOneProcessZmqInprocContextPtr, zmq::socket_type::pair);
    }
    else {
        // socket for cut-through mode straight to egress
        m_zmqCtxPtr = boost::make_unique<zmq::context_t>();
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
        // socket for bundles from storage
        m_zmqPullSock_connectingStorageToBoundEgressPtr = boost::make_unique<zmq::socket_t>(*m_zmqCtxPtr, zmq::socket_type::pull);
        const std::string bind_connectingStorageToBoundEgressPath(
            std::string("tcp://*:") + boost::lexical_cast<std::string>(m_hdtnConfig.m_zmqConnectingStorageToBoundEgressPortPath));
        m_zmqPullSock_connectingStorageToBoundEgressPtr->bind(bind_connectingStorageToBoundEgressPath);
        m_zmqPushSock_boundEgressToConnectingStoragePtr = boost::make_unique<zmq::socket_t>(*m_zmqCtxPtr, zmq::socket_type::push);
        const std::string bind_boundEgressToConnectingStoragePath(
            std::string("tcp://*:") + boost::lexical_cast<std::string>(m_hdtnConfig.m_zmqBoundEgressToConnectingStoragePortPath));
        m_zmqPushSock_boundEgressToConnectingStoragePtr->bind(bind_boundEgressToConnectingStoragePath);
        //socket for interrupt to zmq_poll (acts as condition_variable.notify_one())
        m_zmqPullSignalInprocSockPtr = boost::make_unique<zmq::socket_t>(*m_zmqCtxPtr, zmq::socket_type::pair);
        m_zmqPushSignalInprocSockPtr = boost::make_unique<zmq::socket_t>(*m_zmqCtxPtr, zmq::socket_type::pair);
    }
    m_zmqPullSignalInprocSockPtr->bind(std::string("inproc://egress_condition_variable_notify_one"));
    m_zmqPushSignalInprocSockPtr->connect(std::string("inproc://egress_condition_variable_notify_one"));
    
    if (!m_running) {
        m_running = true;
        m_threadZmqReaderPtr = boost::make_unique<boost::thread>(
            boost::bind(&HegrManagerAsync::ReadZmqThreadFunc, this)); //create and start the worker thread
    }
}

void hdtn::HegrManagerAsync::OnSuccessfulBundleAck(uint64_t outductUuidIndex) {
    //m_conditionVariableProcessZmqMessages.notify_one();
    if (m_needToSendSignal) {
        boost::mutex::scoped_lock lock(m_mutexPushSignal);
        if (m_needToSendSignal) {
            static const char signalByte = 0;
            static const zmq::const_buffer signalByteConstBuf(&signalByte, sizeof(signalByte));
            m_needToSendSignal = false;
            ++m_totalEgressInprocSignalsSent;
            if (!m_zmqPushSignalInprocSockPtr->send(signalByteConstBuf, zmq::send_flags::dontwait)) {
                std::cout << "error in hdtn::HegrManagerAsync::OnSuccessfulBundleAck: unable to send signal\n";
            }
        }
    }
}

static void CustomCleanupEgressAckHdrNoHint(void *data, void *hint) {
    (void)hint;
    //std::cout << "free " << static_cast<hdtn::BlockHdr *>(data)->flowId << std::endl;
    delete static_cast<hdtn::EgressAckHdr *>(data);
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
    m_needToSendSignal = true;
    std::size_t totalCustodyTransfersSentToStorage = 0;
    std::size_t totalCustodyTransfersSentToIngress = 0;
    std::size_t totalEgressInprocSignalsReceived = 0;
    m_totalEgressInprocSignalsSent = 0;

    char junkChar;
    const zmq::mutable_buffer signalRxBufferJunk(&junkChar, sizeof(junkChar));

    typedef std::queue<std::unique_ptr<hdtn::EgressAckHdr> > queue_t;
    typedef std::map<uint64_t, queue_t> outductuuid_needacksqueue_map_t;

    outductuuid_needacksqueue_map_t outductUuidToNeedAcksQueueMap;


    // Use a form of receive that times out so we can terminate cleanly.
    static const int timeout = 250;  // milliseconds
    static constexpr unsigned int NUM_SOCKETS = 3;
    m_zmqPullSock_boundIngressToConnectingEgressPtr->set(zmq::sockopt::rcvtimeo, timeout);
    m_zmqPullSock_connectingStorageToBoundEgressPtr->set(zmq::sockopt::rcvtimeo, timeout);
    zmq::pollitem_t items[NUM_SOCKETS] = {
        {m_zmqPullSock_boundIngressToConnectingEgressPtr->handle(), 0, ZMQ_POLLIN, 0},
        {m_zmqPullSock_connectingStorageToBoundEgressPtr->handle(), 0, ZMQ_POLLIN, 0},
        {m_zmqPullSignalInprocSockPtr->handle(), 0, ZMQ_POLLIN, 0}
    };
    zmq::socket_t * const sockets[NUM_SOCKETS-1] = {
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
            for (unsigned int itemIndex = 0; itemIndex < (NUM_SOCKETS - 1); ++itemIndex) { //skip m_zmqPullSignalInprocSockPtr in this loop
                if ((items[itemIndex].revents & ZMQ_POLLIN) == 0) {
                    continue;
                }

                hdtn::ToEgressHdr toEgressHeader;
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
                
                
                zmq::message_t zmqMessage;
                //message guaranteed to be there due to the zmq::send_flags::sndmore
                if (!sockets[itemIndex]->recv(zmqMessage, zmq::recv_flags::none)) {
                    std::cerr << "error on sockets[itemIndex]->recv\n";
                    continue;
                }
                       
                m_bundleData += zmqMessage.size();
                ++m_bundleCount;

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

            }
            if ((items[NUM_SOCKETS - 1].revents & ZMQ_POLLIN)) { //m_zmqPullSignalInprocSockPtr                
                const zmq::recv_buffer_result_t res = m_zmqPullSignalInprocSockPtr->recv(signalRxBufferJunk, zmq::recv_flags::none);
                if (!res) {
                    std::cerr << "error in HegrManagerAsync::ReadZmqThreadFunc: signal not received" << std::endl;
                }
                else if ((res->truncated()) || (res->size != sizeof(junkChar))) {
                    std::cerr << "error in HegrManagerAsync::ReadZmqThreadFunc: signal message mismatch: untruncated = " << res->untruncated_size
                        << " truncated = " << res->size << " expected = " << sizeof(junkChar) << std::endl;
                }
                else {
                    ++totalEgressInprocSignalsReceived;
                }
                m_needToSendSignal = true;
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
            }
        }
        
    }

    std::cout << "HegrManagerAsync::ReadZmqThreadFunc thread exiting\n";
    hdtn::Logger::getInstance()->logNotification("egress", "HegrManagerAsync::ReadZmqThreadFunc thread exiting");
    const std::string msgToStorage = "totalCustodyTransfersSentToStorage: " + boost::lexical_cast<std::string>(totalCustodyTransfersSentToStorage);
    std::cout << msgToStorage << std::endl;
    hdtn::Logger::getInstance()->logInfo("egress", msgToStorage);
    const std::string msgToIngress = "totalCustodyTransfersSentToIngress: " + boost::lexical_cast<std::string>(totalCustodyTransfersSentToIngress);
    std::cout << msgToIngress << std::endl;
    hdtn::Logger::getInstance()->logInfo("egress", msgToIngress);
    const std::string msgInprocRx = "totalEgressInprocSignalsReceived: " + boost::lexical_cast<std::string>(totalEgressInprocSignalsReceived);
    std::cout << msgInprocRx << std::endl;
    hdtn::Logger::getInstance()->logInfo("egress", msgInprocRx);
    const std::string msgInprocTx = "m_totalEgressInprocSignalsSent: " + boost::lexical_cast<std::string>(m_totalEgressInprocSignalsSent);
    std::cout << msgInprocTx << std::endl;
    hdtn::Logger::getInstance()->logInfo("egress", msgInprocTx);
    //
}
