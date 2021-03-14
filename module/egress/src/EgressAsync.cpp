#include <string.h>

#include "EgressAsync.h"
#include <boost/lexical_cast.hpp>
#include <boost/make_shared.hpp>
#include <boost/make_unique.hpp>

hdtn::HegrManagerAsync::HegrManagerAsync() : m_udpSocket(m_ioService), m_work(m_ioService), m_running(false) {
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
        m_threadZmqReaderPtr = boost::make_shared<boost::thread>();
    }

    if (m_udpSocket.is_open()) {
        try {
            m_udpSocket.close();
        } catch (const boost::system::system_error & e) {
            std::cerr << " Error closing udp socket: " << e.what() << std::endl;
        }
    }
    if (!m_ioService.stopped()) {
        m_ioService.stop();
    }
    if(m_ioServiceThreadPtr) {
        m_ioServiceThreadPtr->join();
        m_ioServiceThreadPtr = boost::make_shared<boost::thread>();
    }

}

void hdtn::HegrManagerAsync::Init() {
    m_entryMap.clear();
    m_bundleCount = 0;
    m_bundleData = 0;
    m_messageCount = 0;
    // socket for cut-through mode straight to egress
    m_zmqCtx_ingressEgressPtr = boost::make_shared<zmq::context_t>();
    m_zmqPullSock_boundIngressToConnectingEgressPtr = boost::make_shared<zmq::socket_t>(*m_zmqCtx_ingressEgressPtr, zmq::socket_type::pull);
    m_zmqPullSock_boundIngressToConnectingEgressPtr->connect(HDTN_BOUND_INGRESS_TO_CONNECTING_EGRESS_PATH);
    m_zmqPushSock_connectingEgressToBoundIngressPtr = boost::make_shared<zmq::socket_t>(*m_zmqCtx_ingressEgressPtr, zmq::socket_type::push);
    m_zmqPushSock_connectingEgressToBoundIngressPtr->connect(HDTN_CONNECTING_EGRESS_TO_BOUND_INGRESS_PATH);
    // socket for sending bundles to storage
    m_zmqCtx_storageEgressPtr = boost::make_shared<zmq::context_t>();
    m_zmqPullSock_connectingStorageToBoundEgressPtr = boost::make_shared<zmq::socket_t>(*m_zmqCtx_storageEgressPtr, zmq::socket_type::pull);
    m_zmqPullSock_connectingStorageToBoundEgressPtr->bind(HDTN_CONNECTING_STORAGE_TO_BOUND_EGRESS_PATH);
    m_zmqPushSock_boundEgressToConnectingStoragePtr = boost::make_shared<zmq::socket_t>(*m_zmqCtx_storageEgressPtr, zmq::socket_type::push);
    m_zmqPushSock_boundEgressToConnectingStoragePtr->bind(HDTN_BOUND_EGRESS_TO_CONNECTING_STORAGE_PATH);

    try {
        m_udpSocket.open(boost::asio::ip::udp::v4());
        m_udpSocket.bind(boost::asio::ip::udp::endpoint(boost::asio::ip::udp::v4(), 0)); //bind to 0 (random ephemeral port)

        std::cout << "UDP Bound on ephemeral port " << m_udpSocket.local_endpoint().port() << std::endl;

    } catch (const boost::system::system_error & e) {
        std::cerr << "Error in hdtn::HegrManagerAsync::Init(): " << e.what() << std::endl;
        //m_running = false;
    }


    if (!m_running) {
        m_running = true;
        m_threadZmqReaderPtr = boost::make_shared<boost::thread>(
            boost::bind(&HegrManagerAsync::ReadZmqThreadFunc, this)); //create and start the worker thread

        m_ioServiceThreadPtr = boost::make_shared<boost::thread>(
            boost::bind(&boost::asio::io_service::run, &m_ioService));
    }
}

void hdtn::HegrManagerAsync::OnSuccessfulBundleAck() {
    m_conditionVariableProcessZmqMessages.notify_one();
}

void hdtn::HegrManagerAsync::ProcessZmqMessagesThreadFunc (
    CircularIndexBufferSingleProducerSingleConsumerConfigurable & cb,
    std::vector<hdtn::BlockHdr> & headerMessages,
    std::vector<bool> & isFromStorage,
    std::vector<zmq::message_t> & payloadMessages)
{
    std::cout << "starting ProcessZmqMessagesThreadFunc with cb size " << headerMessages.size() << std::endl;
    std::size_t totalCustodyTransfersSentToStorage = 0;
    std::size_t totalCustodyTransfersSentToIngress = 0;
    struct QueueItem {
        QueueItem(uint32_t segmentId) : m_segmentId(segmentId), m_isStorageAck(true) {}
        QueueItem(const hdtn::BlockHdr & blockHdr) : m_blockHdr(blockHdr), m_isStorageAck(false) {}
        uint32_t m_segmentId; //for storage acks
        hdtn::BlockHdr m_blockHdr; //for ingress acks
        bool m_isStorageAck;
    };
    typedef std::queue<QueueItem> queue_t;
    typedef std::map<uint64_t, queue_t> flowid_needacksqueue_map_t;

    flowid_needacksqueue_map_t flowIdToNeedAcksQueueMap;

    boost::mutex localMutex;
    boost::mutex::scoped_lock lock(localMutex);

    while (m_running || (cb.GetIndexForRead() != UINT32_MAX)) { //keep thread alive if running or cb not empty


        const unsigned int consumeIndex = cb.GetIndexForRead(); //store the volatile

        if (consumeIndex != UINT32_MAX) { //if not empty
            hdtn::BlockHdr & blockHdr = headerMessages[consumeIndex];
            zmq::message_t & zmqMessage = payloadMessages[consumeIndex];
            const uint16_t type = blockHdr.base.type;
            if ((type == HDTN_MSGTYPE_STORE) || (type == HDTN_MSGTYPE_EGRESS)) {
                if (isFromStorage[consumeIndex]) { //reply to storage
                    flowIdToNeedAcksQueueMap[blockHdr.flowId].push(QueueItem(blockHdr.zframe));
                }
                else { //reply to ingress
                    flowIdToNeedAcksQueueMap[blockHdr.flowId].push(QueueItem(blockHdr));
                }
                Forward(blockHdr.flowId, zmqMessage);
                if (zmqMessage.size() != 0) {
                    std::cout << "Error in hdtn::HegrManagerAsync::ProcessZmqMessagesThreadFunc, zmqMessage was not moved" << std::endl;
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
            for (flowid_needacksqueue_map_t::iterator it = flowIdToNeedAcksQueueMap.begin(); it != flowIdToNeedAcksQueueMap.end(); ++it) {
                const uint64_t flowId = it->first;
                //const unsigned int fec = 1; //TODO
                queue_t & q = it->second;
                std::map<unsigned int, boost::shared_ptr<HegrEntryAsync> >::iterator entryIt = m_entryMap.find(static_cast<unsigned int>(flowId));
                if (entryIt != m_entryMap.end()) {
                    const std::size_t numAckedRemaining = entryIt->second->GetTotalBundlesSent() - entryIt->second->GetTotalBundlesAcked();
                    while (q.size() > numAckedRemaining) {
                        const QueueItem & qItem = q.front();
                        if (qItem.m_isStorageAck) {
                            hdtn::BlockHdr blockHdr;
                            blockHdr.base.type = HDTN_MSGTYPE_EGRESS_TRANSFERRED_CUSTODY;
                            blockHdr.flowId = static_cast<uint32_t>(flowId);
                            blockHdr.zframe = qItem.m_segmentId;
                            if (!m_zmqPushSock_boundEgressToConnectingStoragePtr->send(zmq::const_buffer(&blockHdr, sizeof(hdtn::BlockHdr)), zmq::send_flags::dontwait)) {
                                std::cout << "error: m_zmqPushSock_boundEgressToConnectingStoragePtr could not send" << std::endl;
                                break;
                            }
                            ++totalCustodyTransfersSentToStorage;
                        }
                        else {
                            //send ack message by echoing back the block
                            if (!m_zmqPushSock_connectingEgressToBoundIngressPtr->send(zmq::const_buffer(&qItem.m_blockHdr, sizeof(hdtn::BlockHdr)), zmq::send_flags::dontwait)) {
                                std::cout << "error: zmq could not send ingress an ack from egress" << std::endl;
                                break;
                            }
                            ++totalCustodyTransfersSentToIngress;
                        }
                        q.pop();
                    }
                }
            }
            m_conditionVariableProcessZmqMessages.timed_wait(lock, boost::posix_time::milliseconds(100)); // call lock.unlock() and blocks the current thread
            //thread is now unblocked, and the lock is reacquired by invoking lock.lock()
        }

    }

    std::cout << "ProcessZmqMessagesThreadFunc thread exiting\n";
    std::cout << "totalCustodyTransfersSentToStorage: " << totalCustodyTransfersSentToStorage << std::endl;
    std::cout << "totalCustodyTransfersSentToIngress: " << totalCustodyTransfersSentToIngress << std::endl;
}

void hdtn::HegrManagerAsync::ReadZmqThreadFunc() {

    static const unsigned int NUM_ZMQ_MESSAGES_CB = 40;
    CircularIndexBufferSingleProducerSingleConsumerConfigurable cb(NUM_ZMQ_MESSAGES_CB);
    std::vector<hdtn::BlockHdr> headerMessages(NUM_ZMQ_MESSAGES_CB);
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
        zmq::message_t hdr;        
        if (zmq::poll(&items[0], NUM_SOCKETS, DEFAULT_BIG_TIMEOUT_POLL) > 0) {
            for (unsigned int itemIndex = 0; itemIndex < NUM_SOCKETS; ++itemIndex) {
                if ((items[itemIndex].revents & ZMQ_POLLIN) == 0) {
                    continue;
                }
                if (!sockets[itemIndex]->recv(hdr, zmq::recv_flags::none)) {
                    continue;
                }
                ++m_messageCount;
                //char bundle[HMSG_MSG_MAX];
                if (hdr.size() != sizeof(hdtn::BlockHdr)) {
                    std::cerr << "egress blockhdr message mismatch: " << hdr.size() << std::endl;
                    continue; //return -1;
                }
                const unsigned int writeIndex = cb.GetIndexForWrite();
                if (writeIndex == UINT32_MAX) {
                    std::cerr << "error in hdtn::HegrManagerAsync::ReadZmqThreadFunc(): cb is full" << std::endl;
                    continue;
                }
                hdtn::BlockHdr & blockHdr = headerMessages[writeIndex];
                memcpy(&blockHdr, hdr.data(), sizeof(hdtn::BlockHdr));
                const uint16_t type = blockHdr.base.type;
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
}

int hdtn::HegrManagerAsync::Add(int fec, uint64_t flags, const char *dst, int port, uint64_t rateBitsPerSec) {

    if (flags & HEGR_FLAG_STCPv1) {
        boost::shared_ptr<HegrStcpEntryAsync> stcpEntry = boost::make_shared<HegrStcpEntryAsync>();
        stcpEntry->Connect(dst, boost::lexical_cast<std::string>(port));
        if (StcpBundleSource * ptr = stcpEntry->GetStcpBundleSourcePtr()) {
            ptr->SetOnSuccessfulAckCallback(boost::bind(&hdtn::HegrManagerAsync::OnSuccessfulBundleAck, this));
            ptr->UpdateRate(rateBitsPerSec);
        }
        else {
            std::cerr << "ERROR, CANNOT SET STCP CALLBACK" << std::endl;
        }
        m_entryMap[fec] = stcpEntry;
        m_entryMap[fec]->Disable();
        return 1;
    }
    else if (flags & HEGR_FLAG_UDP) {
        boost::shared_ptr<HegrUdpEntryAsync> udpEntry = boost::make_unique<HegrUdpEntryAsync>();
        udpEntry->Connect(dst, boost::lexical_cast<std::string>(port));
        if (UdpBundleSource * ptr = udpEntry->GetUdpBundleSourcePtr()) {
            ptr->SetOnSuccessfulAckCallback(boost::bind(&hdtn::HegrManagerAsync::OnSuccessfulBundleAck, this));
            ptr->UpdateRate(rateBitsPerSec);
        }
        else {
            std::cerr << "ERROR, CANNOT SET UDP CALLBACK" << std::endl;
        }
        m_entryMap[fec] = udpEntry;
        m_entryMap[fec]->Disable();
        return 1;
    }
    else if (flags & HEGR_FLAG_TCPCLv3) {
        boost::shared_ptr<HegrTcpclEntryAsync> tcpclEntry = boost::make_shared<HegrTcpclEntryAsync>();
        tcpclEntry->Connect(dst, boost::lexical_cast<std::string>(port));
        if (TcpclBundleSource * ptr = tcpclEntry->GetTcpclBundleSourcePtr()) {
            ptr->SetOnSuccessfulAckCallback(boost::bind(&hdtn::HegrManagerAsync::OnSuccessfulBundleAck, this));
        }
        else {
            std::cerr << "ERROR, CANNOT SET TCPCL CALLBACK" << std::endl;
        }
        m_entryMap[fec] = tcpclEntry;
        m_entryMap[fec]->Disable();
        return 1;
    }
    else {
        return -HDTN_MSGTYPE_ENOTIMPL;
    }
    return 0;
}

void hdtn::HegrManagerAsync::Down(int fec) {
    try {
        if(boost::shared_ptr<HegrEntryAsync> entry = m_entryMap.at(fec)) {
            entry->Disable();
        }
    }
    catch (const std::out_of_range &) {
        return;
    }
}

void hdtn::HegrManagerAsync::Up(int fec) {
    try {
        if(boost::shared_ptr<HegrEntryAsync> entry = m_entryMap.at(fec)) {
            entry->Enable();
        }
    }
    catch (const std::out_of_range &) {
        return;
    }
}


int hdtn::HegrManagerAsync::Forward(int fec, zmq::message_t & zmqMessage) {
    try {
        if(boost::shared_ptr<HegrEntryAsync> entry = m_entryMap.at(fec)) {
            return entry->Forward(zmqMessage);
        }
    }
    catch (const std::out_of_range &) {
        return 0;
    }
    return 0;
}


/** Leaving function for now. Need to know if these sockets will be removed
throughout running the code. int HegrManager::remove(int fec) { int
shutdown_status; shutdown_status = entry_(fec)->shutdown(); delete entry_(fec);
    return 0;
}
**/




// JCF -- Missing destructor, added below
hdtn::HegrEntryAsync::HegrEntryAsync() : m_label(0), m_flags(0) {}

hdtn::HegrEntryAsync::~HegrEntryAsync() {}

void hdtn::HegrEntryAsync::Init(uint64_t flags) {}

bool hdtn::HegrEntryAsync::Available() { return (m_flags & HEGR_FLAG_ACTIVE) && (m_flags & HEGR_FLAG_UP); }

int hdtn::HegrEntryAsync::Disable() { return -1; }

void hdtn::HegrEntryAsync::Rate(uint64_t rate) {
    //_rate = rate;
}

void hdtn::HegrEntryAsync::Label(uint64_t label) { m_label = label; }

void hdtn::HegrEntryAsync::Name(char *n) {
    // strncpy(_name, n, HEGR_NAME_SZ);
}

int hdtn::HegrEntryAsync::Enable() { return -1; }

void hdtn::HegrEntryAsync::Update(uint64_t delta) { return; }



void hdtn::HegrEntryAsync::Shutdown() {}



hdtn::HegrUdpEntryAsync::HegrUdpEntryAsync() : HegrEntryAsync() {
    m_flags = HEGR_FLAG_ACTIVE | HEGR_FLAG_UDP;
    // memset(_name, 0, HEGR_NAME_SZ);
}

std::size_t hdtn::HegrUdpEntryAsync::GetTotalBundlesAcked() {
    return m_udpBundleSourcePtr->GetTotalUdpPacketsAcked();
}

std::size_t hdtn::HegrUdpEntryAsync::GetTotalBundlesSent() {
    return m_udpBundleSourcePtr->GetTotalUdpPacketsSent();
}

void hdtn::HegrUdpEntryAsync::Init(uint64_t flags) {
    //m_fd = socket(AF_INET, SOCK_DGRAM, 0);
    //memcpy(&m_ipv4, inaddr, sizeof(sockaddr_in));
}

void hdtn::HegrUdpEntryAsync::Shutdown() {
    //close(m_fd);
}

void hdtn::HegrUdpEntryAsync::Rate(uint64_t rate) {
    //_rate = rate;
}

void hdtn::HegrUdpEntryAsync::Update(uint64_t delta) {}

int hdtn::HegrUdpEntryAsync::Enable() {
    printf("[%d] UDP egress port state set to UP - forwarding to ", (int)m_label);
    m_flags |= HEGR_FLAG_UP;
    return 0;
}

int hdtn::HegrUdpEntryAsync::Disable() {
    printf("[%d] UDP egress port state set to DOWN.\n", (int)m_label);
    m_flags &= (~HEGR_FLAG_UP);
    return 0;
}

int hdtn::HegrUdpEntryAsync::Forward(zmq::message_t & zmqMessage) {
    if (!(m_flags & HEGR_FLAG_UP)) {
        return 0;
    }
    if (m_udpBundleSourcePtr && m_udpBundleSourcePtr->Forward(zmqMessage)) {
        return 1;
    }
    std::cerr << "link not ready to forward yet" << std::endl;
    return 1;
}


void hdtn::HegrUdpEntryAsync::Connect(const std::string & hostname, const std::string & port) {
    m_udpBundleSourcePtr = boost::make_unique<UdpBundleSource>(15);
    m_udpBundleSourcePtr->Connect(hostname, port);
}

UdpBundleSource * hdtn::HegrUdpEntryAsync::GetUdpBundleSourcePtr() {
    return (m_udpBundleSourcePtr) ? m_udpBundleSourcePtr.get() : NULL;
}