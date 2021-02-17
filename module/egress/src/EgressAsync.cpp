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
    m_zmqCutThroughCtx = boost::make_shared<zmq::context_t>();
    m_zmqCutThroughSock = boost::make_shared<zmq::socket_t>(*m_zmqCutThroughCtx, zmq::socket_type::pull);
    m_zmqCutThroughSock->connect(m_cutThroughAddress);
    // socket for sending bundles to storage
    m_zmqReleaseCtx = boost::make_shared<zmq::context_t>();
    m_zmqReleaseSock = boost::make_shared<zmq::socket_t>(*m_zmqReleaseCtx, zmq::socket_type::pull);
    m_zmqReleaseSock->bind(m_releaseAddress);

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
    // Use a form of receive that times out so we can terminate cleanly.
    int timeout = 250;  // milliseconds
    m_zmqCutThroughSock->setsockopt(ZMQ_RCVTIMEO, &timeout, sizeof(int));

    while (m_running) { //keep thread alive if running
        zmq::message_t hdr;
        if(!m_zmqCutThroughSock->recv(&hdr)) {
            continue; //timeout
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
                boost::shared_ptr<zmq::message_t> zmqMessagePtr = boost::make_shared<zmq::message_t>();
                // Use a form of receive that times out so we can terminate cleanly.  If no
                // message was received after timeout go back to top of loop
                // std::cout << "In runEgress, before recv. " << std::endl << std::flush;
                while (m_running) {
                    if (!m_zmqCutThroughSock->recv(zmqMessagePtr.get())) {
                        continue;
                    }
                    Forward(1, zmqMessagePtr);
                    m_bundleData += zmqMessagePtr->size();
                    ++m_bundleCount;
                    break;
                }
                break;
        }

    }

    std::cout << "HegrManagerAsync::ReadZmqThreadFunc thread exiting\n";
}

int hdtn::HegrManagerAsync::Add(int fec, uint64_t flags, const char *dst, int port) {

    if (flags & HEGR_FLAG_STCPv1) {
        //HegrStcpEntry *tcp = new (Entry(fec)) HegrStcpEntry;
        //tcp->Init(&saddr, flags);
        //tcp->Disable();
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


int hdtn::HegrManagerAsync::Forward(int fec, boost::shared_ptr<zmq::message_t> zmqMessagePtr) {
    try {
        if(boost::shared_ptr<HegrEntryAsync> entry = m_entryMap.at(fec)) {
            return entry->Forward(zmqMessagePtr);
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

int hdtn::HegrUdpEntryAsync::Forward(boost::shared_ptr<zmq::message_t> zmqMessagePtr) {
    if (!(m_flags & HEGR_FLAG_UP)) {
        return 0;
    }
    const std::size_t bundleSize = zmqMessagePtr->size();
    m_udpSocketPtr->async_send_to(boost::asio::buffer(zmqMessagePtr->data(), bundleSize), m_udpDestinationEndpoint,
                                  boost::bind(&HegrUdpEntryAsync::HandleUdpSendBundle, this, zmqMessagePtr,
                                              boost::asio::placeholders::error,
                                              boost::asio::placeholders::bytes_transferred));
    return 1;
}

void hdtn::HegrUdpEntryAsync::HandleUdpSendBundle(boost::shared_ptr<zmq::message_t> zmqMessagePtr, const boost::system::error_code& error, std::size_t bytes_transferred) {
}


