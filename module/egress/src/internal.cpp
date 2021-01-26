#include <arpa/inet.h>
#include <string.h>
#include "egress.h"

using namespace hdtn;

HegrEntry *HegrManager::entry_(int offset) {
  return (HegrEntry *)(((uint8_t *)entries_) + (offset * HEGR_ENTRY_SZ));
}

HegrManager::~HegrManager() {
  for (int i = 0; i < HEGR_ENTRY_COUNT; ++i) {
    // JCF, debuging
    //        HegrEntry * pEntry = entry_(i);
    //        std::cout << "In HegrManager::~HegrManager, i = " << i << " ,
    //        *entry = " << pEntry << std::endl << std::flush;
    entry_(i)->Shutdown();
    // JCF, the following line seg faults.  It looks like the destructor is not
    // implemented.  Could be related to linked list also. JCF, commented out so
    // code development could continue. delete (entry_(i));
  }
  free(entries_);
}

void HegrManager::Init() {
  entries_ = malloc(HEGR_ENTRY_SZ * HEGR_ENTRY_COUNT);
  for (int i = 0; i < HEGR_ENTRY_COUNT; ++i) {
    HegrEntry *tmp = new (entry_(i)) HegrEntry;
    tmp->Label(i);
  }
  // socket for cut-through mode straight to egress
  zmq_cut_through_address_ = new zmq::context_t;
  zmq_cut_through_sock_ =
      new zmq::socket_t(*zmq_cut_through_address_, zmq::socket_type::pull);
  zmq_cut_through_sock_->connect(cut_through_address_);
  // socket for sending bundles to storage
  zmq_release_ctx_ = new zmq::context_t;
  zmq_release_sock_ =
      new zmq::socket_t(*zmq_release_ctx_, zmq::socket_type::pull);
  zmq_release_sock_->bind(release_address_);
}

int HegrManager::Add(int fec, uint64_t flags, const char *dst, int port) {
  struct sockaddr_in saddr;
  saddr.sin_port = htons((uint16_t)port);
  saddr.sin_family = AF_INET;
  int conversion_status;
  conversion_status = inet_pton(AF_INET, dst, &(saddr.sin_addr));
  if (conversion_status != 1) {
    printf("Failure to convert IP address from text to binary");
    return 0;
  }
  if (flags & HEGR_FLAG_STCPv1) {
    HegrStcpEntry *tcp = new (entry_(fec)) HegrStcpEntry;
    tcp->Init(&saddr, flags);
    tcp->Disable();
    return 1;
  } else if (flags & HEGR_FLAG_UDP) {
    HegrStcpEntry *udp = new (entry_(fec)) HegrStcpEntry;
    udp->Init(&saddr, flags);
    udp->Disable();
    return 1;
  } else {
    return -HDTN_MSGTYPE_ENOTIMPL;
  }
  return 0;
}

void HegrManager::Down(int fec) { entry_(fec)->Disable(); }

void HegrManager::Up(int fec) { entry_(fec)->Enable(); }

/** Leaving function for now. Need to know if these sockets will be removed
throughout running the code. int HegrManager::remove(int fec) { int
shutdown_status; shutdown_status = entry_(fec)->shutdown(); delete entry_(fec);
    return 0;
}
**/
int HegrManager::Forward(int fec, char *msg, int sz) {
  return entry_(fec)->Forward((char **)(&msg), &sz, 1);
}

HegrEntry::HegrEntry() {
  flags_ = 0;
  //_next = NULL;
}

// JCF -- Missing destructor, added below
HegrEntry::~HegrEntry() {}

void HegrEntry::Init(sockaddr_in *inaddr, uint64_t flags) {}

bool HegrEntry::Available() {
  return (flags_ & HEGR_FLAG_ACTIVE) && (flags_ & HEGR_FLAG_UP);
}

int HegrEntry::Disable() { return -1; }

void HegrEntry::Rate(uint64_t rate) {
  //_rate = rate;
}

void HegrEntry::Label(uint64_t label) { label_ = label; }

void HegrEntry::Name(char *n) {
  // strncpy(_name, n, HEGR_NAME_SZ);
}

int HegrEntry::Enable() { return -1; }

void HegrEntry::Update(uint64_t delta) { return; }

int HegrEntry::Forward(char **msg, int *sz, int count) { return 0; }

void HegrEntry::Shutdown() {}
