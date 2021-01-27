#include <errno.h>
#include <math.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <cassert>
#include <iostream>
#include "codec/bpv6-ext-block.h"
#include "codec/bpv6.h"
#include "ingress.h"
#include "message.hpp"

namespace hdtn {

BpIngressSyscall::BpIngressSyscall() {
  msgbuf_.srcbuf = NULL;
  msgbuf_.io = NULL;
  msgbuf_.hdr = NULL;
}

BpIngressSyscall::~BpIngressSyscall() {
  if (msgbuf_.srcbuf != NULL) {
    Destroy();
  }
}

int BpIngressSyscall::Init(uint32_t type) {
  int i = 0;
  msgbuf_.bufsz = BP_INGRESS_MSG_BUFSZ;
  msgbuf_.nbuf = BP_INGRESS_MSG_NBUF;
  msgbuf_.srcbuf = (char *)malloc(sizeof(struct sockaddr_in) * BP_INGRESS_MSG_NBUF);
  msgbuf_.io = (iovec *)malloc(sizeof(struct iovec) * BP_INGRESS_MSG_NBUF);
  msgbuf_.hdr = (mmsghdr *)malloc(sizeof(struct mmsghdr) * BP_INGRESS_MSG_NBUF);
  for (i = 0; i < BP_INGRESS_MSG_NBUF; ++i) {
    bufs_[i] = (char *)malloc(BP_INGRESS_MSG_BUFSZ);
    //_msgbuf.io[i].iov_base = malloc(BP_INGRESS_MSG_BUFSZ);
    msgbuf_.io[i].iov_base = bufs_[i];
    msgbuf_.io[i].iov_len = BP_INGRESS_MSG_BUFSZ;
  }
  for (i = 0; i < BP_INGRESS_MSG_NBUF; ++i) {
    msgbuf_.hdr[i].msg_hdr.msg_iov = &msgbuf_.io[i];
    msgbuf_.hdr[i].msg_hdr.msg_iovlen = 1;
    msgbuf_.hdr[i].msg_hdr.msg_name = msgbuf_.srcbuf + (sizeof(struct sockaddr_in) * i);
    msgbuf_.hdr[i].msg_hdr.msg_namelen = sizeof(struct sockaddr_in);
  }
  // socket for cut-through mode straight to egress
  zmq_cut_through_ctx_ = new zmq::context_t;
  zmq_cut_through_sock_ = new zmq::socket_t(*zmq_cut_through_ctx_, zmq::socket_type::push);
  zmq_cut_through_sock_->bind(cut_through_address_);
  // socket for sending bundles to storage
  zmq_storage_ctx_ = new zmq::context_t;
  zmq_storage_sock_ = new zmq::socket_t(*zmq_storage_ctx_, zmq::socket_type::push);
  zmq_storage_sock_->bind(storage_address_);
  return 0;
}

void BpIngressSyscall::Destroy() {
  for (int i = 0; i < BP_INGRESS_MSG_NBUF; ++i) {
    free(msgbuf_.io[i].iov_base);
    msgbuf_.io[i].iov_base = NULL;
  }
  free(msgbuf_.srcbuf);
  free(msgbuf_.io);
  free(msgbuf_.hdr);
  msgbuf_.srcbuf = NULL;
  msgbuf_.io = NULL;
  msgbuf_.hdr = NULL;
  shutdown(fd_, SHUT_RDWR);
}

int BpIngressSyscall::Netstart(uint16_t port) {
  struct sockaddr_in bind_addr;
  int res = 0;
  printf("Starting ingress channel ...\n");
  fd_ = socket(AF_INET, SOCK_DGRAM, 0);
  bind_addr.sin_family = AF_INET;
  bind_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  bind_addr.sin_port = htons(port);
  res = bind(fd_, (struct sockaddr *)&bind_addr, sizeof(bind_addr));
  if (res < 0) {
    printf("Unable to bind to port %d (on INADDR_ANY): %s", port,strerror(res));
  } else {
    printf("Ingress bound successfully on port %d ...", port);
  }
  return 0;
}

int BpIngressSyscall::Process(int count) {
  uint64_t timer;
  int i = 0;
  bool ok = true;
  (void)ok;
  uint16_t recvlen = 0;
  uint64_t offset = 0;
  uint32_t zframe_seq = 0;
  bpv6_primary_block bpv6_primary;
  bpv6_eid dst;
  char hdr_buf[sizeof(BlockHdr)];
  memset(&bpv6_primary, 0, sizeof(bpv6_primary_block));
  for (i = 0; i < count; ++i) {
    char tbuf[HMSG_MSG_MAX];
    hdtn::BlockHdr hdr;
    memset(&hdr, 0, sizeof(hdtn::BlockHdr));
    timer = rdtsc();
    recvlen = msgbuf_.hdr[i].msg_len;
    msgbuf_.hdr[i].msg_len = 0;
    memcpy(tbuf, bufs_[i], recvlen);
    hdr.ts = timer;
    offset =
        bpv6_primary_block_decode(&bpv6_primary, bufs_[i], offset, recvlen);
    dst.node = bpv6_primary.dst_node;
    hdr.flow_id = dst.node;  // for now
    hdr.base.flags = bpv6_primary.flags;
    hdr.base.type = HDTN_MSGTYPE_STORE;
    // hdr.ts=recvlen;
    int num_chunks = 1;
    int bytes_to_send = recvlen;
    int remainder = 0;
    int j = 0;
    zframe_seq = 0;
    if (recvlen > CHUNK_SIZE)  // the bundle is bigger than internal message size limit
    {
      num_chunks = recvlen / CHUNK_SIZE;
      bytes_to_send = CHUNK_SIZE;
      remainder = recvlen % CHUNK_SIZE;
      if (remainder != 0) num_chunks++;
    }
    for (j = 0; j < num_chunks; j++) {
      if ((j == num_chunks - 1) && (remainder != 0)) bytes_to_send = remainder;
      hdr.bundle_seq = ing_sequence_num_;
      ing_sequence_num_++;
      hdr.zframe = zframe_seq;
      zframe_seq++;
      memcpy(hdr_buf, &hdr, sizeof(BlockHdr));
      zmq_cut_through_sock_->send(hdr_buf, sizeof(BlockHdr), ZMQ_MORE);
      char data[bytes_to_send];
      memcpy(data, tbuf + (CHUNK_SIZE * j), bytes_to_send);
      zmq_cut_through_sock_->send(data, bytes_to_send, 0);
      ++zmsgs_out_;
    }
    ++bundle_count_;
    bundle_data_ += recvlen;
    timer = rdtsc() - timer;
  }
  return 0;
}

int BpIngressSyscall::Update() {
  int res = 0;
  int ernum;
  res = recvmmsg(fd_, msgbuf_.hdr, BP_INGRESS_MSG_NBUF, MSG_DONTWAIT, NULL);
  if (res < 0) {
    ernum = errno;
  }
  return res;
}

int BpIngressSyscall::Update(double time_out_seconds) {
  double fractpart;
  double intpart;
  fractpart = modf(time_out_seconds, &intpart);
  struct timespec time_out;
  time_out.tv_sec = intpart;
  time_out.tv_nsec = fractpart * 1000000000;
  int res = 0;
  int ernum;
  res = recvmmsg(fd_, msgbuf_.hdr, BP_INGRESS_MSG_NBUF, MSG_DONTWAIT, &time_out);
  if (res < 0) {
    ernum = errno;
  }
  return res;
}

}  // namespace hdtn
