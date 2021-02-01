#ifndef _HDTN_INGRESS_H
#define _HDTN_INGRESS_H

#include <stdint.h>
#include <sys/socket.h>
#include <sys/time.h>

#include "message.hpp"
#include "paths.hpp"
#include "util/tsc.h"
#include "zmq.hpp"

// Used to receive multiple datagrams (e.g. recvmmsg)
#define BP_INGRESS_STRBUF_SZ (8192)
#define BP_INGRESS_MSG_NBUF (32)
#define BP_INGRESS_MSG_BUFSZ (65536)
#define BP_INGRESS_USE_SYSCALL (1)
#define BP_INGRESS_TYPE_UDP (0x01)
#define BP_INGRESS_TYPE_STCP (0x02)

namespace hdtn {
std::string Datetime();

typedef struct BpMmsgbuf {
    uint32_t nbuf;
    uint32_t bufsz;
    struct mmsghdr *hdr;
    struct iovec *io;
    char *srcbuf;
} BpMmsgbuf;

typedef struct IngressTelemetry {
    uint64_t totalBundles;
    uint64_t totalBytes;
    uint64_t totalZmsgsIn;
    uint64_t totalZmsgsOut;
    uint64_t bundlesSecIn;
    uint64_t mBitsSecIn;
    uint64_t zmsgsSecIn;
    uint64_t zmsgsSecOut;
    double elapsed;
} IngressTelemetry;

class BpIngressSyscall {
public:
    BpIngressSyscall();  // initialize message buffers
    ~BpIngressSyscall();
    void Destroy();
    int Init(uint32_t type);
    int Netstart(uint16_t port);
    int Process(int count);
    int send_telemetry();
    int Update();
    int Update(double time_out_seconds);

    uint64_t m_bundleCount = 0;
    uint64_t m_bundleData = 0;
    uint64_t m_zmsgsIn = 0;
    uint64_t m_zmsgsOut = 0;
    uint64_t m_ingSequenceNum = 0;
    double m_elapsed = 0;
    bool m_forceStorage = false;
    const char *m_cutThroughAddress = HDTN_CUT_THROUGH_PATH;
    const char *m_storageAddress = HDTN_STORAGE_PATH;

private:
    BpMmsgbuf msgbuf_;
    zmq::context_t *m_zmqCutThroughCtx;
    zmq::socket_t *m_zmqCutThroughSock;
    zmq::context_t *m_zmqStorageCtx;
    zmq::socket_t *m_zmqStorageSock;
    zmq::context_t *m_zmqTelemCtx;
    zmq::socket_t *m_zmqTelemSock;
    int m_fd;
    int m_type;
    char *m_bufs[BP_INGRESS_MSG_NBUF];
};

// use an explicit typedef to avoid runtime vcall overhead
#ifdef BP_INGRESS_USE_SYSCALL
typedef BpIngressSyscall BpIngress;
#endif

}  // namespace hdtn

#endif  //_HDTN_INGRESS_H
