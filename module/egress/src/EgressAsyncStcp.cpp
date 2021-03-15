#include <string.h>

#include "EgressAsync.h"
#include <boost/lexical_cast.hpp>
#include <boost/make_shared.hpp>

namespace hdtn {


HegrStcpEntryAsync::HegrStcpEntryAsync() : HegrEntryAsync() {

}

HegrStcpEntryAsync::~HegrStcpEntryAsync() {

}

std::size_t HegrStcpEntryAsync::GetTotalBundlesAcked() {
    return m_stcpBundleSourcePtr->GetTotalDataSegmentsAcked();
}

std::size_t HegrStcpEntryAsync::GetTotalBundlesSent() {
    return m_stcpBundleSourcePtr->GetTotalDataSegmentsSent();
}

void HegrStcpEntryAsync::Init(uint64_t flags) {
    //m_fd = socket(AF_INET, SOCK_DGRAM, 0);
    //memcpy(&m_ipv4, inaddr, sizeof(sockaddr_in));
}

void HegrStcpEntryAsync::Shutdown() {
    //close(m_fd);
}

void HegrStcpEntryAsync::Rate(uint64_t rate) {
    //_rate = rate;
}

void HegrStcpEntryAsync::Update(uint64_t delta) {

}


int HegrStcpEntryAsync::Enable() {
    printf("[%d] Stcp egress port state set to UP - forwarding to ", (int)m_label);
    m_flags |= HEGR_FLAG_UP;
    return 0;
}

int HegrStcpEntryAsync::Disable() {
    printf("[%d] Stcp egress port state set to DOWN.\n", (int)m_label);
    m_flags &= (~HEGR_FLAG_UP);
    return 0;
}

int HegrStcpEntryAsync::Forward(zmq::message_t & zmqMessage) {
    if (!(m_flags & HEGR_FLAG_UP)) {
        return 0;
    }
    if(m_stcpBundleSourcePtr && m_stcpBundleSourcePtr->Forward(zmqMessage)) {
        return 1;

    }
    std::cerr << "link not ready to forward yet" << std::endl;
    return 1;
}



void HegrStcpEntryAsync::Connect(const std::string & hostname, const std::string & port) {
    m_stcpBundleSourcePtr = boost::make_shared<StcpBundleSource>(15);
    m_stcpBundleSourcePtr->Connect(hostname, port);
}

StcpBundleSource * HegrStcpEntryAsync::GetStcpBundleSourcePtr() {
    return (m_stcpBundleSourcePtr) ? m_stcpBundleSourcePtr.get() : NULL;
}


}; //end namespace hdtn
