#include <arpa/inet.h>
#include "egress.h"
#include <string.h>
using namespace hdtn3;

void hegr_stcp_entry::init(sockaddr_in* inaddr, uint64_t flags) {
    _fd = socket(AF_INET, SOCK_STREAM, 0);
    memcpy(&_ipv4, inaddr, sizeof(sockaddr_in));
}

void hegr_stcp_entry::shutdown() {
    close(_fd);
}

void hegr_stcp_entry::rate(uint64_t rate) {
    //_rate = rate;
}

void hegr_stcp_entry::update(uint64_t delta) {

}

int hegr_stcp_entry::enable() {
    printf("[%d] TCP egress port state set to UP - forwarding to ", (int)_label);
    char sbuf[512];
    inet_ntop(AF_INET, &_ipv4.sin_addr, sbuf, 512);
    printf("%s:%d\n", sbuf, ntohs(_ipv4.sin_port));

    _flags |= HEGR_FLAG_UP;
    return 0;
}

int hegr_stcp_entry::disable() {
    printf("[%d] TCP egress port state set to DOWN.\n", (int)_label);
    _flags &= (~HEGR_FLAG_UP);
    return 0;
}

int hegr_stcp_entry::forward(char** msg, int* sz, int count) {
    if(!(_flags & HEGR_FLAG_UP)) {
        return 0;
    }

    for(int i = 0; i < count; ++i) {
        int res = send(_fd, msg[i], sz[i], 0);
        if(res < 0) {
            return errno;
        }
    }
    return count;
}

hegr_stcp_entry::hegr_stcp_entry() {
    _flags = HEGR_FLAG_ACTIVE | HEGR_FLAG_UDP;
    //memset(_name, 0, HEGR_NAME_SZ);

}
