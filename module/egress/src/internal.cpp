#include <arpa/inet.h>
#include <string.h>

#include "egress.h"

//using namespace hdtn;

hdtn::HegrEntry *hdtn::HegrManager::Entry(int offset) { return (HegrEntry *)(((uint8_t *)m_entries) + (offset * HEGR_ENTRY_SZ)); }

hdtn::HegrManager::~HegrManager() {
    for (int i = 0; i < HEGR_ENTRY_COUNT; ++i) {
        // JCF, debuging
        //        HegrEntry * pEntry = entry_(i);
        //        std::cout << "In HegrManager::~HegrManager, i = " << i << " ,
        //        *entry = " << pEntry << std::endl << std::flush;
        Entry(i)->Shutdown();
        // JCF, the following line seg faults.  It looks like the destructor is not
        // implemented.  Could be related to linked list also. JCF, commented out so
        // code development could continue. delete (entry_(i));
    }
    free(m_entries);
}

void hdtn::HegrManager::Init() {
    m_entries = malloc(HEGR_ENTRY_SZ * HEGR_ENTRY_COUNT);
    for (int i = 0; i < HEGR_ENTRY_COUNT; ++i) {
        HegrEntry *tmp = new (Entry(i)) HegrEntry;
        tmp->Label(i);
    }
    // socket for cut-through mode straight to egress
    m_zmqCutThroughCtx = new zmq::context_t;
    m_zmqCutThroughSock = new zmq::socket_t(*m_zmqCutThroughCtx, zmq::socket_type::pull);
    m_zmqCutThroughSock->connect(HDTN_BOUND_INGRESS_TO_CONNECTING_EGRESS_PATH);
    // socket for sending bundles to storage
    m_zmqReleaseCtx = new zmq::context_t;
    m_zmqReleaseSock = new zmq::socket_t(*m_zmqReleaseCtx, zmq::socket_type::pull);
    m_zmqReleaseSock->bind(HDTN_CONNECTING_STORAGE_TO_BOUND_EGRESS_PATH);
}

int hdtn::HegrManager::Add(int fec, uint64_t flags, const char *dst, int port) {
    struct sockaddr_in saddr;
    saddr.sin_port = htons((uint16_t)port);
    saddr.sin_family = AF_INET;
    int conversionStatus;
    conversionStatus = inet_pton(AF_INET, dst, &(saddr.sin_addr));
    if (conversionStatus != 1) {
        printf("Failure to convert IP address from text to binary");
        return 0;
    }
    if (flags & HEGR_FLAG_STCPv1) {
        HegrStcpEntry *tcp = new (Entry(fec)) HegrStcpEntry;
        tcp->Init(&saddr, flags);
        tcp->Disable();
        return 1;
    }
    else if (flags & HEGR_FLAG_UDP) {
        //    HegrStcpEntry *udp = new (Entry(fec)) HegrStcpEntry;
        HegrUdpEntry *udp = new (Entry(fec)) HegrUdpEntry;
        udp->Init(&saddr, flags);
        udp->Disable();
        return 1;
    }
    else {
        return -HDTN_MSGTYPE_ENOTIMPL;
    }
    return 0;
}

void hdtn::HegrManager::Down(int fec) { Entry(fec)->Disable(); }

void hdtn::HegrManager::Up(int fec) { Entry(fec)->Enable(); }

/** Leaving function for now. Need to know if these sockets will be removed
throughout running the code. int HegrManager::remove(int fec) { int
shutdown_status; shutdown_status = entry_(fec)->shutdown(); delete entry_(fec);
    return 0;
}
**/
int hdtn::HegrManager::Forward(int fec, char *msg, int sz) { return Entry(fec)->Forward((char **)(&msg), &sz, 1); }

hdtn::HegrEntry::HegrEntry() {
    m_flags = 0;
    //_next = NULL;
}

// JCF -- Missing destructor, added below
hdtn::HegrEntry::~HegrEntry() {}

void hdtn::HegrEntry::Init(sockaddr_in *inaddr, uint64_t flags) {}

bool hdtn::HegrEntry::Available() { return (m_flags & HEGR_FLAG_ACTIVE) && (m_flags & HEGR_FLAG_UP); }

int hdtn::HegrEntry::Disable() { return -1; }

void hdtn::HegrEntry::Rate(uint64_t rate) {
    //_rate = rate;
}

void hdtn::HegrEntry::Label(uint64_t label) { m_label = label; }

void hdtn::HegrEntry::Name(char *n) {
    // strncpy(_name, n, HEGR_NAME_SZ);
}

int hdtn::HegrEntry::Enable() { return -1; }

void hdtn::HegrEntry::Update(uint64_t delta) { return; }

int hdtn::HegrEntry::Forward(char **msg, int *sz, int count) { return 0; }

void hdtn::HegrEntry::Shutdown() {}
