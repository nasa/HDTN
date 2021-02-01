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
        m_bufs[i] = (char *)malloc(BP_INGRESS_MSG_BUFSZ);
        //_msgbuf.io[i].iov_base = malloc(BP_INGRESS_MSG_BUFSZ);
        msgbuf_.io[i].iov_base = m_bufs[i];
        msgbuf_.io[i].iov_len = BP_INGRESS_MSG_BUFSZ;
    }
    for (i = 0; i < BP_INGRESS_MSG_NBUF; ++i) {
        msgbuf_.hdr[i].msg_hdr.msg_iov = &msgbuf_.io[i];
        msgbuf_.hdr[i].msg_hdr.msg_iovlen = 1;
        msgbuf_.hdr[i].msg_hdr.msg_name = msgbuf_.srcbuf + (sizeof(struct sockaddr_in) * i);
        msgbuf_.hdr[i].msg_hdr.msg_namelen = sizeof(struct sockaddr_in);
    }
    // socket for cut-through mode straight to egress
    m_zmqCutThroughCtx = new zmq::context_t;
    m_zmqCutThroughSock = new zmq::socket_t(*m_zmqCutThroughCtx, zmq::socket_type::push);
    m_zmqCutThroughSock->bind(m_cutThroughAddress);
    // socket for sending bundles to storage
    m_zmqStorageCtx = new zmq::context_t;
    m_zmqStorageSock = new zmq::socket_t(*m_zmqStorageCtx, zmq::socket_type::push);
    m_zmqStorageSock->bind(m_storageAddress);
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
    shutdown(m_fd, SHUT_RDWR);
}

int BpIngressSyscall::Netstart(uint16_t port) {
    struct sockaddr_in bindAddr;
    int res = 0;
    printf("Starting ingress channel ...\n");
    m_fd = socket(AF_INET, SOCK_DGRAM, 0);
    bindAddr.sin_family = AF_INET;
    bindAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    bindAddr.sin_port = htons(port);
    res = bind(m_fd, (struct sockaddr *)&bindAddr, sizeof(bindAddr));
    if (res < 0) {
        printf("Unable to bind to port %d (on INADDR_ANY): %s", port, strerror(res));
    }
    else {
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
    uint32_t zframeSeq = 0;
    bpv6_primary_block bpv6Primary;
    bpv6_eid dst;
    char hdrBuf[sizeof(BlockHdr)];
    memset(&bpv6Primary, 0, sizeof(bpv6_primary_block));
    for (i = 0; i < count; ++i) {
        char tbuf[HMSG_MSG_MAX];
        hdtn::BlockHdr hdr;
        memset(&hdr, 0, sizeof(hdtn::BlockHdr));
        timer = rdtsc();
        recvlen = msgbuf_.hdr[i].msg_len;
        msgbuf_.hdr[i].msg_len = 0;
        memcpy(tbuf, m_bufs[i], recvlen);
        hdr.ts = timer;
        offset = bpv6_primary_block_decode(&bpv6Primary, m_bufs[i], offset, recvlen);
        dst.node = bpv6Primary.dst_node;
        hdr.flowId = dst.node;  // for now
        hdr.base.flags = bpv6Primary.flags;
        hdr.base.type = HDTN_MSGTYPE_STORE;
        // hdr.ts=recvlen;
        int numChunks = 1;
        int bytesToSend = recvlen;
        int remainder = 0;
        int j = 0;
        zframeSeq = 0;
        if (recvlen > CHUNK_SIZE)  // the bundle is bigger than internal message size limit
        {
            numChunks = recvlen / CHUNK_SIZE;
            bytesToSend = CHUNK_SIZE;
            remainder = recvlen % CHUNK_SIZE;
            if (remainder != 0) numChunks++;
        }
        for (j = 0; j < numChunks; j++) {
            if ((j == numChunks - 1) && (remainder != 0)) bytesToSend = remainder;
            hdr.bundleSeq = m_ingSequenceNum;
            m_ingSequenceNum++;
            hdr.zframe = zframeSeq;
            zframeSeq++;
            memcpy(hdrBuf, &hdr, sizeof(BlockHdr));
            m_zmqCutThroughSock->send(hdrBuf, sizeof(BlockHdr), ZMQ_MORE);
            char data[bytesToSend];
            memcpy(data, tbuf + (CHUNK_SIZE * j), bytesToSend);
            m_zmqCutThroughSock->send(data, bytesToSend, 0);
            ++m_zmsgsOut;
        }
        ++m_bundleCount;
        m_bundleData += recvlen;
        timer = rdtsc() - timer;
    }
    return 0;
}

int BpIngressSyscall::Update() {
    int res = 0;
    int ernum;
    res = recvmmsg(m_fd, msgbuf_.hdr, BP_INGRESS_MSG_NBUF, MSG_DONTWAIT, NULL);
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
    res = recvmmsg(m_fd, msgbuf_.hdr, BP_INGRESS_MSG_NBUF, MSG_DONTWAIT, &time_out);
    if (res < 0) {
        ernum = errno;
    }
    return res;
}

}  // namespace hdtn
