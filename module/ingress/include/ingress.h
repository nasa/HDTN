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
  uint64_t total_bundles;
  uint64_t total_bytes;
  uint64_t total_zmsgs_in;
  uint64_t total_zmsgs_out;
  uint64_t bundles_sec_in;
  uint64_t Mbits_sec_in;
  uint64_t zmsgs_sec_in;
  uint64_t zmsgs_sec_out;
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

  uint64_t bundle_count_ = 0;
  uint64_t bundle_data_ = 0;
  uint64_t zmsgs_in_ = 0;
  uint64_t zmsgs_out_ = 0;
  uint64_t ing_sequence_num_ = 0;
  double elapsed_ = 0;
  bool force_storage_ = false;
  const char *cut_through_address_ = HDTN_CUT_THROUGH_PATH;
  const char *storage_address_ = HDTN_STORAGE_PATH;

 private:
  BpMmsgbuf msgbuf_;
  zmq::context_t *zmq_cut_through_ctx_;
  zmq::socket_t *zmq_cut_through_sock_;
  zmq::context_t *zmq_storage_ctx_;
  zmq::socket_t *zmq_storage_sock_;
  zmq::context_t *zmq_telem_ctx_;
  zmq::socket_t *zmq_telem_sock_;
  int fd_;
  int type_;
  char *bufs_[BP_INGRESS_MSG_NBUF];
};

// use an explicit typedef to avoid runtime vcall overhead
#ifdef BP_INGRESS_USE_SYSCALL
typedef BpIngressSyscall BpIngress;
#endif

}  // namespace hdtn

#endif  //_HDTN_INGRESS_H
