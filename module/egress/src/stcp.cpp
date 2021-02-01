#include <arpa/inet.h>
#include <string.h>

#include "egress.h"

//using namespace hdtn;

void hdtn::HegrStcpEntry::Init(sockaddr_in *inaddr, uint64_t flags) {
    m_fd = socket(AF_INET, SOCK_STREAM, 0);
    memcpy(&m_ipv4, inaddr, sizeof(sockaddr_in));
}

void hdtn::HegrStcpEntry::Shutdown() { close(m_fd); }

void hdtn::HegrStcpEntry::Rate(uint64_t rate) {
    //_rate = rate;
}

void hdtn::HegrStcpEntry::Update(uint64_t delta) {}

int hdtn::HegrStcpEntry::Enable() {
    printf("[%d] TCP egress port state set to UP - forwarding to ", (int)m_label);
    char sbuf[512];
    inet_ntop(AF_INET, &m_ipv4.sin_addr, sbuf, 512);
    printf("%s:%d\n", sbuf, ntohs(m_ipv4.sin_port));

    m_flags |= HEGR_FLAG_UP;
    return 0;
}

int hdtn::HegrStcpEntry::Disable() {
    printf("[%d] TCP egress port state set to DOWN.\n", (int)m_label);
    m_flags &= (~HEGR_FLAG_UP);
    return 0;
}

int hdtn::HegrStcpEntry::Forward(char **msg, int *sz, int count) {
    if (!(m_flags & HEGR_FLAG_UP)) {
        return 0;
    }

    for (int i = 0; i < count; ++i) {
        int res = send(m_fd, msg[i], sz[i], 0);
        if (res < 0) {
            return errno;
        }
    }
    return count;
}

hdtn::HegrStcpEntry::HegrStcpEntry() {
    m_flags = HEGR_FLAG_ACTIVE | HEGR_FLAG_UDP;
    // memset(_name, 0, HEGR_NAME_SZ);
}
