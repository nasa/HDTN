#include <string.h>

#include "EgressAsync.h"
#include <boost/lexical_cast.hpp>
#include <boost/make_shared.hpp>
#include <boost/make_unique.hpp>

namespace hdtn {


HegrTcpclEntryAsync::HegrTcpclEntryAsync() : HegrEntryAsync() {

}

HegrTcpclEntryAsync::~HegrTcpclEntryAsync() {

}

std::size_t HegrTcpclEntryAsync::GetTotalBundlesAcked() {
    return m_tcpclBundleSourcePtr->GetTotalDataSegmentsAcked();
}

std::size_t HegrTcpclEntryAsync::GetTotalBundlesSent() {
    return m_tcpclBundleSourcePtr->GetTotalDataSegmentsSent();
}

void HegrTcpclEntryAsync::Init(uint64_t flags) {
    //m_fd = socket(AF_INET, SOCK_DGRAM, 0);
    //memcpy(&m_ipv4, inaddr, sizeof(sockaddr_in));
}

void HegrTcpclEntryAsync::Shutdown() {
    //close(m_fd);
}

void HegrTcpclEntryAsync::Rate(uint64_t rate) {
    //_rate = rate;
}

void HegrTcpclEntryAsync::Update(uint64_t delta) {

}


int HegrTcpclEntryAsync::Enable() {
    printf("[%d] UDP egress port state set to UP - forwarding to ", (int)m_label);
    m_flags |= HEGR_FLAG_UP;
    return 0;
}

int HegrTcpclEntryAsync::Disable() {
    printf("[%d] UDP egress port state set to DOWN.\n", (int)m_label);
    m_flags &= (~HEGR_FLAG_UP);
    return 0;
}

int HegrTcpclEntryAsync::Forward(zmq::message_t & zmqMessage) {
    if (!(m_flags & HEGR_FLAG_UP)) {
        return 0;
    }
    if(m_tcpclBundleSourcePtr && m_tcpclBundleSourcePtr->Forward(zmqMessage)) {
        return 1;

    }
    std::cerr << "link not ready to forward yet" << std::endl;
    return 1;
}



void HegrTcpclEntryAsync::Connect(const std::string & hostname, const std::string & port) {
    m_tcpclBundleSourcePtr = boost::make_unique<TcpclBundleSource>(30, "EGRESS");
    m_tcpclBundleSourcePtr->Connect(hostname, port);
}

TcpclBundleSource * HegrTcpclEntryAsync::GetTcpclBundleSourcePtr() {
    return (m_tcpclBundleSourcePtr) ? m_tcpclBundleSourcePtr.get() : NULL;
}

}; //end namespace hdtn
