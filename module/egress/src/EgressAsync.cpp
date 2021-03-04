#include <string.h>

#include "EgressAsync.h"
#include <boost/lexical_cast.hpp>
#include <boost/make_shared.hpp>

hdtn::HegrManagerAsync::HegrManagerAsync() : m_udpSocket(m_ioService), m_work(m_ioService), m_running(false) {
    //m_flags = 0;
    //_next = NULL;
}

hdtn::HegrManagerAsync::~HegrManagerAsync() {
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
    m_zmqCtx_boundIngressToConnectingEgressPtr = boost::make_shared<zmq::context_t>();
    m_zmqPullSock_boundIngressToConnectingEgressPtr = boost::make_shared<zmq::socket_t>(*m_zmqCtx_boundIngressToConnectingEgressPtr, zmq::socket_type::pull);
    m_zmqPullSock_boundIngressToConnectingEgressPtr->connect(HDTN_BOUND_INGRESS_TO_CONNECTING_EGRESS_PATH);
    // socket for sending bundles to storage
    m_zmqCtx_connectingStorageToBoundEgressPtr = boost::make_shared<zmq::context_t>();
    m_zmqPullSock_connectingStorageToBoundEgressPtr = boost::make_shared<zmq::socket_t>(*m_zmqCtx_connectingStorageToBoundEgressPtr, zmq::socket_type::pull);
    m_zmqPullSock_connectingStorageToBoundEgressPtr->bind(HDTN_CONNECTING_STORAGE_TO_BOUND_EGRESS_PATH);
    m_zmqPushSock_boundEgressToConnectingStoragePtr = boost::make_shared<zmq::socket_t>(*m_zmqCtx_connectingStorageToBoundEgressPtr, zmq::socket_type::push);
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

void hdtn::HegrManagerAsync::ReadZmqThreadFunc() {

    typedef std::queue<uint32_t> segid_queue_t;
    typedef std::map<uint64_t, segid_queue_t> flowid_needacksqueue_map_t;

    flowid_needacksqueue_map_t flowIdToNeedAcksQueueMap;

    std::size_t totalCustodyTransfersSentToStorage = 0;    
    std::size_t totalEventsQueueNotEmpty = 0;

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
    long timeoutPoll = DEFAULT_BIG_TIMEOUT_POLL; //0 => no blocking

    while (m_running) { //keep thread alive if running
        zmq::message_t hdr;        
        if (zmq::poll(&items[0], NUM_SOCKETS, timeoutPoll) > 0) {


            for (unsigned int itemIndex = 0; itemIndex < NUM_SOCKETS; ++itemIndex) {
                if ((items[itemIndex].revents & ZMQ_POLLIN) == 0) {
                    continue;
                }
                if (!sockets[itemIndex]->recv(hdr, zmq::recv_flags::none)) {
                    continue;
                }
                ++m_messageCount;
                //char bundle[HMSG_MSG_MAX];
                if (hdr.size() < sizeof(hdtn::CommonHdr)) {
                    std::cerr << "[dispatch] message too short: " << hdr.size() << std::endl;
                    continue; //return -1;
                }
                hdtn::CommonHdr *common = (hdtn::CommonHdr *)hdr.data();
                switch (common->type) {
                case HDTN_MSGTYPE_STORE:
                case HDTN_MSGTYPE_EGRESS: //todo
                    boost::shared_ptr<zmq::message_t> zmqMessagePtr = boost::make_shared<zmq::message_t>();
                    // Use a form of receive that times out so we can terminate cleanly.  If no
                    // message was received after timeout go back to top of loop
                    // std::cout << "In runEgress, before recv. " << std::endl << std::flush;
                    while (m_running) {
                        if (!sockets[itemIndex]->recv(*zmqMessagePtr, zmq::recv_flags::none)) {
                            continue;
                        }
                        if (itemIndex == 1) { //reply to storage
                            hdtn::BlockHdr *block = (hdtn::BlockHdr *)hdr.data();
                            flowIdToNeedAcksQueueMap[block->flowId].push(block->zframe);
                        }
                        unsigned int numUnackedBundles = 0;
                        Forward(1, zmqMessagePtr, numUnackedBundles);  //TODO: FIX 1 WHICH SHOULD BE FLOW ID
                        if (numUnackedBundles > 10) {
                            std::cout << numUnackedBundles << std::endl;
                        }

                        m_bundleData += zmqMessagePtr->size();
                        ++m_bundleCount;

                        break;
                    }
                    break;
                }
            }
        }
        
        timeoutPoll = DEFAULT_BIG_TIMEOUT_POLL;
        for (flowid_needacksqueue_map_t::iterator it = flowIdToNeedAcksQueueMap.begin(); it != flowIdToNeedAcksQueueMap.end(); ++it) {
            const uint64_t flowId = it->first;
            const unsigned int fec = 1; //TODO
            segid_queue_t & q = it->second;
            
            
            std::map<unsigned int, boost::shared_ptr<HegrEntryAsync> >::iterator entryIt = m_entryMap.find(fec);
            if (entryIt != m_entryMap.end()) {
                //std::cout << "h0" << std::endl;
                //std::cout << entry.get() << std::endl;
                if (HegrTcpclEntryAsync * entryTcpcl = dynamic_cast<HegrTcpclEntryAsync*>(entryIt->second.get())) {
                    //std::cout << entryTcpcl.get() << std::endl;
                    //std::cout << "h1" << std::endl;
                    const std::size_t numAckedRemaining = entryTcpcl->GetTotalBundlesSent() - entryTcpcl->GetTotalBundlesAcked();
                    //std::cout << "h2 " << numAckedRemaining << " " << q.size() <<  std::endl;
                    while (q.size() > numAckedRemaining) {
                        hdtn::BlockHdr blockHdr;
                        blockHdr.base.type = HDTN_MSGTYPE_EGRESS_TRANSFERRED_CUSTODY;
                        blockHdr.flowId = static_cast<uint32_t>(flowId);
                        blockHdr.zframe = q.front();
                        if (!m_zmqPushSock_boundEgressToConnectingStoragePtr->send(zmq::const_buffer(&blockHdr, sizeof(hdtn::BlockHdr)), zmq::send_flags::dontwait)) {
                            std::cout << "error: m_zmqPushSock_boundEgressToConnectingStoragePtr could not send" << std::endl;
                            break;
                        }
                        ++totalCustodyTransfersSentToStorage;
                        q.pop();
                    }
                        
                }
                    
            }
            
            if (!q.empty()) {
                timeoutPoll = 1; //shortest timeout 1ms as we wait for acks
                ++totalEventsQueueNotEmpty;
            }
            
        }
    }
    std::cout << "totalCustodyTransfersSentToStorage: " << totalCustodyTransfersSentToStorage << std::endl;
    std::cout << "totalEventsQueueNotEmpty: " << totalEventsQueueNotEmpty << std::endl;

    std::cout << "HegrManagerAsync::ReadZmqThreadFunc thread exiting\n";
}

int hdtn::HegrManagerAsync::Add(int fec, uint64_t flags, const char *dst, int port) {

    if (flags & HEGR_FLAG_STCPv1) {
        boost::shared_ptr<HegrStcpEntryAsync> stcpEntry = boost::make_shared<HegrStcpEntryAsync>();
        stcpEntry->Connect(dst, boost::lexical_cast<std::string>(port));
        m_entryMap[fec] = stcpEntry;
        m_entryMap[fec]->Disable();
        return 1;
    }
    else if (flags & HEGR_FLAG_UDP) {
        static const boost::asio::ip::resolver_query_base::flags UDP_RESOLVER_FLAGS = boost::asio::ip::resolver_query_base::canonical_name; //boost resolver flags
        boost::asio::ip::udp::resolver resolver(m_ioService);
        boost::asio::ip::udp::endpoint endpoint = *resolver.resolve(boost::asio::ip::udp::resolver::query(boost::asio::ip::udp::v4(), dst, boost::lexical_cast<std::string>(port), UDP_RESOLVER_FLAGS));
        m_entryMap[fec] = boost::make_shared<HegrUdpEntryAsync>(endpoint, &m_udpSocket);
        m_entryMap[fec]->Disable();
        return 1;
    }
    else if (flags & HEGR_FLAG_TCPCLv3) {
        boost::shared_ptr<HegrTcpclEntryAsync> tcpclEntry = boost::make_shared<HegrTcpclEntryAsync>();
        tcpclEntry->Connect(dst, boost::lexical_cast<std::string>(port));
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


int hdtn::HegrManagerAsync::Forward(int fec, boost::shared_ptr<zmq::message_t> zmqMessagePtr, unsigned int & numUnackedBundles) {
    try {
        if(boost::shared_ptr<HegrEntryAsync> entry = m_entryMap.at(fec)) {
            return entry->Forward(zmqMessagePtr, numUnackedBundles);
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



hdtn::HegrUdpEntryAsync::HegrUdpEntryAsync(const boost::asio::ip::udp::endpoint & udpDestinationEndpoint, boost::asio::ip::udp::socket * const udpSocketPtr) :
HegrEntryAsync(),
m_udpDestinationEndpoint(udpDestinationEndpoint),
m_udpSocketPtr(udpSocketPtr)
{
    m_flags = HEGR_FLAG_ACTIVE | HEGR_FLAG_UDP;
    // memset(_name, 0, HEGR_NAME_SZ);
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

int hdtn::HegrUdpEntryAsync::Forward(boost::shared_ptr<zmq::message_t> zmqMessagePtr, unsigned int & numUnackedBundles) {
    if (!(m_flags & HEGR_FLAG_UP)) {
        return 0;
    }
    numUnackedBundles = 0; //TODO
    const std::size_t bundleSize = zmqMessagePtr->size();
    m_udpSocketPtr->async_send_to(boost::asio::buffer(zmqMessagePtr->data(), bundleSize), m_udpDestinationEndpoint,
                                  boost::bind(&HegrUdpEntryAsync::HandleUdpSendBundle, this, zmqMessagePtr,
                                              boost::asio::placeholders::error,
                                              boost::asio::placeholders::bytes_transferred));
    return 1;
}

void hdtn::HegrUdpEntryAsync::HandleUdpSendBundle(boost::shared_ptr<zmq::message_t> zmqMessagePtr, const boost::system::error_code& error, std::size_t bytes_transferred) {
}


