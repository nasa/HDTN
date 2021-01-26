#include <arpa/inet.h>
#include <string.h>
#include "egress.h"

using namespace hdtn;

void HegrUdpEntry::Init(sockaddr_in *inaddr, uint64_t flags) {
  fd_ = socket(AF_INET, SOCK_DGRAM, 0);
  memcpy(&ipv4_, inaddr, sizeof(sockaddr_in));
}

void HegrUdpEntry::Shutdown() { close(fd_); }

void HegrUdpEntry::Rate(uint64_t rate) {
  //_rate = rate;
}

void HegrUdpEntry::Update(uint64_t delta) {}

int HegrUdpEntry::Enable() {
  printf("[%d] UDP egress port state set to UP - forwarding to ", (int)label_);
  char sbuf[512];
  inet_ntop(AF_INET, &ipv4_.sin_addr, sbuf, 512);
  printf("%s:%d\n", sbuf, ntohs(ipv4_.sin_port));
  flags_ |= HEGR_FLAG_UP;
  return 0;
}

int HegrUdpEntry::Disable() {
  printf("[%d] UDP egress port state set to DOWN.\n", (int)label_);
  flags_ &= (~HEGR_FLAG_UP);
  return 0;
}

int HegrUdpEntry::Forward(char **msg, int *sz, int count) {
  if (!(flags_ & HEGR_FLAG_UP)) {
    return 0;
  }

  for (int i = 0; i < count; ++i) {
    int res = sendto(fd_, msg[i], sz[i], MSG_CONFIRM, (sockaddr *)&ipv4_,sizeof(sockaddr_in));
    if (res < 0) {
      return errno;
    }
  }
  return count;
}

HegrUdpEntry::HegrUdpEntry() {
  flags_ = HEGR_FLAG_ACTIVE | HEGR_FLAG_UDP;
  // memset(_name, 0, HEGR_NAME_SZ);
}
