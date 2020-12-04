#include <netinet/in.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <iostream>
#include "ingress.h"
#include "message.hpp"
#include <cassert>
#include "codec/bpv6.h"
#include "codec/bpv6-ext-block.h"

namespace hdtn{

bp_ingress_syscall::bp_ingress_syscall() {
    _msgbuf.srcbuf = NULL;
    _msgbuf.io = NULL;
    _msgbuf.hdr = NULL;
}

bp_ingress_syscall::~bp_ingress_syscall() {
    if(_msgbuf.srcbuf != NULL) {
        destroy();
    }
}

int bp_ingress_syscall::init(uint32_t type) {
    int i = 0;
    _msgbuf.bufsz = BP_INGRESS_MSG_BUFSZ;
    _msgbuf.nbuf = BP_INGRESS_MSG_NBUF;
    _msgbuf.srcbuf = (char *)malloc(sizeof(struct sockaddr_in) * BP_INGRESS_MSG_NBUF);
    _msgbuf.io = (iovec *)malloc(sizeof(struct iovec) * BP_INGRESS_MSG_NBUF);
    _msgbuf.hdr = (mmsghdr *)malloc(sizeof(struct mmsghdr) * BP_INGRESS_MSG_NBUF);
    for(i = 0; i < BP_INGRESS_MSG_NBUF; ++i) {
        _bufs[i] = (char*) malloc(BP_INGRESS_MSG_BUFSZ);
        //_msgbuf.io[i].iov_base = malloc(BP_INGRESS_MSG_BUFSZ);
    	_msgbuf.io[i].iov_base =  _bufs[i];
        _msgbuf.io[i].iov_len  =  BP_INGRESS_MSG_BUFSZ;
    }
    for(i = 0; i < BP_INGRESS_MSG_NBUF; ++i) {
        _msgbuf.hdr[i].msg_hdr.msg_iov = &_msgbuf.io[i];
        _msgbuf.hdr[i].msg_hdr.msg_iovlen = 1;
        _msgbuf.hdr[i].msg_hdr.msg_name = _msgbuf.srcbuf + (sizeof(struct sockaddr_in) * i);
        _msgbuf.hdr[i].msg_hdr.msg_namelen = sizeof(struct sockaddr_in);
    }

    _zmq_ingr_ctx = new zmq::context_t;
    _zmq_ingr_sock = new zmq::socket_t(*_zmq_ingr_ctx, zmq::socket_type::push);
    _zmq_ingr_sock->bind ("tcp://127.0.0.1:10149");
    return 0;
}

void bp_ingress_syscall::destroy() {
    int i = 0;

    for(i = 0; i < BP_INGRESS_MSG_NBUF; ++i) {
        free(_msgbuf.io[i].iov_base);
        _msgbuf.io[i].iov_base = NULL;
    }

    free(_msgbuf.srcbuf);
    free(_msgbuf.io);
    free(_msgbuf.hdr);
    _msgbuf.srcbuf = NULL;
    _msgbuf.io = NULL;
    _msgbuf.hdr = NULL;
    shutdown(_fd, SHUT_RDWR);
}

int bp_ingress_syscall::netstart(uint16_t port){
    struct sockaddr_in bind_addr;
    int res = 0;
    printf("Starting ingress channel ...\n");

    _fd = socket(AF_INET, SOCK_DGRAM, 0);

    bind_addr.sin_family = AF_INET;
    bind_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    bind_addr.sin_port = htons(port);

    res = bind(_fd, (struct sockaddr *)&bind_addr, sizeof(bind_addr));

    if (res < 0) {
        printf("Unable to bind to port %d (on INADDR_ANY): %s", port, strerror(res));
    }
    else {
    	printf("Ingress bound successfully on port %d ...", port);
	}
    return 0;
}

int bp_ingress_syscall::process(int count) {

	uint64_t timer;
	int i = 0;
	bool ok=true;
	(void) ok;
	uint16_t recvlen=0;
	uint64_t offset = 0;
	uint32_t zframe_seq=0;
	bpv6_primary_block bpv6_primary;
	bpv6_eid dst;
	int rc;
	char hdr_buf[sizeof(block_hdr)];
	memset(&bpv6_primary, 0, sizeof(bpv6_primary_block));
	for(i = 0; i < count; ++i) {
		char tbuf[HMSG_MSG_MAX];
		hdtn::block_hdr hdr;
		memset(&hdr,0,sizeof(hdtn::block_hdr));
		timer = rdtsc();
		recvlen=_msgbuf.hdr[i].msg_len;
		_msgbuf.hdr[i].msg_len= 0;
		memcpy(tbuf, _bufs[i], recvlen);
		hdr.ts = timer;
		offset= bpv6_primary_block_decode(&bpv6_primary, _bufs[i],offset, recvlen);
		dst.node=bpv6_primary.dst_node;
		hdr.flow=dst.node;//for now
		hdr.base.flags=bpv6_primary.flags;
		hdr.base.type=HDTN_MSGTYPE_STORE;  
		//hdr.ts=recvlen;

		int num_chunks=1;
		int bytes_to_send=recvlen;
		int remainder=0;
		int j=0;
		zframe_seq=0;
		if(recvlen>CHUNK_SIZE)//the bundle is bigger than internal message size limit
		{
			num_chunks=recvlen/CHUNK_SIZE;
			bytes_to_send=CHUNK_SIZE;
			remainder=recvlen%CHUNK_SIZE;
			if(remainder !=0)
				num_chunks++;
		}
		for(j=0;j<num_chunks;j++)
		{
			if((j==num_chunks-1)&&(remainder!=0))
				bytes_to_send=remainder;
			hdr.bundle_seq=ing_sequence_num;
			ing_sequence_num++;
			hdr.zframe=zframe_seq;
			zframe_seq++;
			
			memcpy(hdr_buf, &hdr,sizeof(block_hdr));
			_zmq_ingr_sock->send(hdr_buf,sizeof(block_hdr),ZMQ_MORE);
			char data[bytes_to_send];
			memcpy(data,tbuf+(CHUNK_SIZE*j),bytes_to_send);
			_zmq_ingr_sock->send(data,bytes_to_send,0);
			++zmsgs_out;
		}

		++bundle_count;
		bundle_data+=recvlen;
		timer = rdtsc() - timer;
	}
	return 0;
}


int bp_ingress_syscall::update() {
    int res = 0;
    int ernum;
    res = recvmmsg(_fd, _msgbuf.hdr, BP_INGRESS_MSG_NBUF, MSG_DONTWAIT, NULL);
    if(res<0){
    	ernum=errno;
    }
    return res;
}

}

