#ifndef _HDTN_INGRESS_H
#define _HDTN_INGRESS_H

#include <stdint.h>
#include <sys/socket.h>
#include <sys/time.h>

#include "message.hpp"
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
std::string datetime();

typedef struct bp_mmsgbuf {
    uint32_t nbuf;
    uint32_t bufsz;
    struct mmsghdr *hdr;
    struct iovec *io;
    char *srcbuf;
} bp_mmsgbuf;

typedef struct ingress_telemetry {
    uint64_t total_bundles;
    uint64_t total_bytes;
    uint64_t total_zmsgs_in;
    uint64_t total_zmsgs_out;
    uint64_t bundles_sec_in;
    uint64_t Mbits_sec_in;
    uint64_t zmsgs_sec_in;
    uint64_t zmsgs_sec_out;
    double elapsed;

} ingress_telemetry;

class bp_ingress_syscall {
   public:
    bp_ingress_syscall();  // initialize message buffers
    ~bp_ingress_syscall();
    void destroy();
    int init(uint32_t type);
    int netstart(uint16_t port);
    int process(int count);
    int send_telemetry();
    int update();
    int update(double time_out_seconds);

    uint64_t bundle_count = 0;
    uint64_t bundle_data = 0;
    uint64_t zmsgs_in = 0;
    uint64_t zmsgs_out = 0;
    uint64_t ing_sequence_num = 0;
    double elapsed = 0;
    bool force_storage = false;

   private:
    bp_mmsgbuf _msgbuf;
    zmq::context_t *_zmq_ingr_ctx;
    zmq::socket_t *_zmq_ingr_sock;
    zmq::context_t *_zmq_telem_ctx;
    zmq::socket_t *_zmq_telem_sock;
    int _fd;
    int _type;
    char *_bufs[BP_INGRESS_MSG_NBUF];
};

// use an explicit typedef to avoid runtime vcall overhead
#ifdef BP_INGRESS_USE_SYSCALL
typedef bp_ingress_syscall bp_ingress;
#endif

}  // namespace hdtn

#endif  //_HDTN_INGRESS_H
