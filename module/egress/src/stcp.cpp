#include <arpa/inet.h>
#include <string.h>
#include "egress.h"

using namespace hdtn;

void HegrStcpEntry::Init(sockaddr_in *inaddr, uint64_t flags) {
  fd_ = socket(AF_INET, SOCK_STREAM, 0);
  memcpy(&ipv4_, inaddr, sizeof(sockaddr_in));
}

void HegrStcpEntry::Shutdown() { close(fd_); }

void HegrStcpEntry::Rate(uint64_t rate) {
  //_rate = rate;
}

void HegrStcpEntry::Update(uint64_t delta) {}

int HegrStcpEntry::Enable() {
  printf("[%d] TCP egress port state set to UP - forwarding to ", (int)label_);
  char sbuf[512];
  inet_ntop(AF_INET, &ipv4_.sin_addr, sbuf, 512);
  printf("%s:%d\n", sbuf, ntohs(ipv4_.sin_port));

  flags_ |= HEGR_FLAG_UP;
  return 0;
}

int HegrStcpEntry::Disable() {
  printf("[%d] TCP egress port state set to DOWN.\n", (int)label_);
  flags_ &= (~HEGR_FLAG_UP);
  return 0;
}

int HegrStcpEntry::Forward(char **msg, int *sz, int count) {
  if (!(flags_ & HEGR_FLAG_UP)) {
    return 0;
  }

  for (int i = 0; i < count; ++i) {
    int res = send(fd_, msg[i], sz[i], 0);
    if (res < 0) {
      return errno;
    }
  }
  return count;
}

HegrStcpEntry::HegrStcpEntry() {
  flags_ = HEGR_FLAG_ACTIVE | HEGR_FLAG_UDP;
  // memset(_name, 0, HEGR_NAME_SZ);
}
