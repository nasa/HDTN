#include <string.h>
#include <stdio.h>
#include <sys/socket.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include "codec/bpv6.h"
#include "util/tsc.h"

#define BP_MSG_BUFSZ             (65536)
#define BP_BUNDLE_DEFAULT_SZ     (100)
#define BP_GEN_BUNDLE_MAXSZ      (64000)
#define BP_GEN_RATE_MAX          (1 << 30)
#define BP_GEN_TARGET_DEFAULT    "127.0.0.1"
#define BP_GEN_PORT_DEFAULT      (4556)
#define BP_GEN_SRC_NODE_DEFAULT  (1)
#define BP_GEN_DST_NODE_DEFAULT  (2)
#define BP_GEN_BATCH_DEFAULT     (1 << 18)  // write out one entry per this many bundles.
#define BP_GEN_LOGFILE           "bpreflect.%lu.csv"

#ifdef __APPLE__  // If we're on an apple platform, we can really only send one bundle at once

#define BP_MSG_NBUF   (1)

struct mmsghdr {
    struct msghdr msg_hdr;
    unsigned int  msg_len;
};

#else             // If we're on a different platform, then we can use sendmmsg / recvmmsg

#define BP_MSG_NBUF   (32)

#endif

struct bpgen_hdr {
    uint64_t seq;
    uint64_t tsc;
    timespec abstime;
};

int main(int argc, char* argv[]) {
    bpv6_primary_block primary;
    bpv6_canonical_block payload;
    char* logfile = (char*)malloc(2048);
    snprintf(logfile, 2048, BP_GEN_LOGFILE, time(0));

    memset(&primary, 0, sizeof(bpv6_primary_block));
    memset(&payload, 0, sizeof(bpv6_canonical_block));

    printf("Initializing ...\n");
    struct mmsghdr* msgbuf;
    struct sockaddr_in* addrbuf;
    uint64_t bundle_count = 0;
    uint64_t bundle_data = 0;
    uint32_t batch = BP_GEN_BATCH_DEFAULT;
    char* target = NULL;
    char* dst = NULL;
    struct sockaddr_in servaddr;
    struct sockaddr_in dstaddr;
    int run_state = 1;
    int fd = -1;
    int port = BP_GEN_PORT_DEFAULT;
    int dst_port = BP_GEN_PORT_DEFAULT;
    ssize_t res;
    uint8_t use_tcp = 0;
    int c;

    static char usage[] = "usage: %s -d address [-b address] [-D destination_port] [-f logfile.%lu.csv] [-p local_port] [-T]\n";

    while ((c = getopt(argc, argv, "b:d:f:hp:B:D:T")) != -1) {
        switch (c) {
            case 'b':
                target = strdup(optarg);
                break;
            case 'd':
                dst = strdup(optarg);
                break;
            case 'f':
                snprintf(logfile, 2048, "%sbpreflect.%lu.csv", optarg, time(0));
                break;
            case 'B':
                batch = atoi(optarg);
                printf("Batch size is now %d\n", (int)batch);
                break;
            case 'D':
                dst_port = atoi(optarg);
                break;
            case 'p':
                port = atoi(optarg);
                break;
            case 'T':
                use_tcp = 1;
                break;
            case 'h':
            case '?':
                printf(usage, argv[0]);
                return -1;
            default:
                printf("Unknown argument:`%c` (%d)\n", c, c);
                printf(usage, argv[0]);
                return -2;
        }
    }
    if(NULL == dst) {
        //printf("");
        return -1;
    }

    if(NULL == target) {
        target = strdup(BP_GEN_TARGET_DEFAULT);
    }

    bzero(&dstaddr, sizeof(sockaddr_in));
    bzero(&servaddr,sizeof(sockaddr_in));

    dstaddr.sin_family = AF_INET;
    servaddr.sin_family = AF_INET;
    res = inet_pton(AF_INET, dst, &(dstaddr.sin_addr));
    if(1 != res) {
        printf("Invalid address specified: %s\n", dst);
        return -1;
    }
    dstaddr.sin_port = htons(dst_port);
    res = inet_pton(AF_INET, target, &(servaddr.sin_addr) );
    if(1 != res) {
        printf("Invalid address specified: %s\n", target);
        return -1;
    }
    servaddr.sin_port = htons(port);

    printf("Starting reflection server on %s:%d\n", target, port);
    printf("Forwarding traffic to %s:%d\n", dst, port);

    char* data_buffer[BP_MSG_BUFSZ];
    memset(data_buffer, 0, BP_MSG_BUFSZ);
    bpgen_hdr* hdr = (bpgen_hdr*)data_buffer;

    if(use_tcp) {
        fd = socket(AF_INET,SOCK_STREAM,0);
	
    }
    else {
        fd = socket(AF_INET,SOCK_DGRAM,0);
	
    }
    uint64_t last_time = 0;
    uint64_t curr_time = 0;
    uint64_t seq = 0;

    res = bind(fd, (sockaddr*) &servaddr, sizeof(sockaddr_in));
    if(res < 0) {
        perror("bind() failed");
        return -3;
    }
    if(use_tcp) {
        printf("Waiting for incoming connection ...\n");
        int tfd = fd;
        res = listen(tfd, 1);
        if(res < 0) {
            perror("listen() failed");
            return -2;
        }
        socklen_t sa_len = 0;
        fd = accept(tfd, (sockaddr *)(&servaddr), &sa_len);
    }
    
    msgbuf = (mmsghdr*) malloc(sizeof(struct mmsghdr) * BP_MSG_NBUF);
    memset(msgbuf, 0, sizeof(struct mmsghdr) * BP_MSG_NBUF);

    addrbuf = (sockaddr_in*) malloc(sizeof(struct sockaddr_in) * BP_MSG_NBUF);
    memset(addrbuf, 0, sizeof(sockaddr_in) * BP_MSG_NBUF);

    struct iovec* io = (iovec*) malloc(sizeof(struct iovec) * BP_MSG_NBUF);
    memset(io, 0, sizeof(struct iovec) * BP_MSG_NBUF);

    char** tmp = (char**) malloc(BP_MSG_NBUF * sizeof(char *));
    int i = 0;
    for(i = 0; i < BP_MSG_NBUF; ++i) {
        tmp[i] = (char*) malloc(BP_MSG_BUFSZ);
        memset(tmp[i], 0x0, BP_MSG_BUFSZ);
    }

    for(i = 0; i < BP_MSG_NBUF; ++i) {
        msgbuf[i].msg_hdr.msg_iov = &io[i];
        msgbuf[i].msg_hdr.msg_iovlen = 1;
        msgbuf[i].msg_hdr.msg_iov->iov_base = tmp[i];
        msgbuf[i].msg_hdr.msg_iov->iov_len = BP_MSG_BUFSZ;
        msgbuf[i].msg_hdr.msg_name = (void *)&addrbuf[i];
        msgbuf[i].msg_hdr.msg_namelen = sizeof(struct sockaddr_in);
    }
    FILE* log = fopen(logfile, "w+");
    if(NULL == log) {
        perror("fopen()");
        return -5;
    }
    printf("Entering run state ...\n");
    printf("Writing to logfile: %s\n", logfile);
    struct timeval tv;
    gettimeofday(&tv, NULL);
    double start = ((double)tv.tv_sec) + ((double)tv.tv_usec / 1000000.0);
    printf("Start: +%f\n", start);
    uint64_t tsc_total   = 0;
     int64_t rt_total    = 0;
    uint64_t bytes_total = 0;

    double sval = 0.0;
    uint64_t received_count = 0;

    while(true) {
        #ifdef __APPLE__
        int res = recvmsg(fd, &msgbuf->msg_hdr, 0);
        if(res > 0) //recvmsg returns number of bytes, recvmmsg returns number of messages
        {
            msgbuf->msg_len = res;
            res = 1;
        }
        #else
        int res = recvmmsg(fd, msgbuf, BP_MSG_NBUF, 0, NULL);
        #endif

        if(res == 0) {
            continue;
        }
        if(res < 0) {
            perror("recvmmsg");
            return -1;
        }

        for(i = 0; i < res; ++i) {
            char* mbuf = (char *)msgbuf[i].msg_hdr.msg_iov->iov_base;
            ssize_t sz = msgbuf[i].msg_len;
            msgbuf[i].msg_hdr.msg_iov->iov_len = sz;
            memcpy(msgbuf[i].msg_hdr.msg_name, &dstaddr, sizeof(sockaddr_in));
            msgbuf[i].msg_hdr.msg_flags = 0;
            msgbuf[i].msg_hdr.msg_namelen = sizeof(struct sockaddr_in);
            
            uint64_t curr_seq = 0;

            int32_t offset = 0;
            offset += bpv6_primary_block_decode(&primary, mbuf, 0, sz);
            if(0 == offset) {
                printf("Malformed bundle received - aborting.\n");
                return -2;
            }
            uint64_t val_tmp = 0;
            int encode_len = 0;
            val_tmp = primary.src_node;
            primary.src_node = primary.dst_node;
            primary.dst_node = val_tmp;
            val_tmp = primary.src_svc;
            primary.src_svc = primary.dst_svc;
            primary.dst_svc = val_tmp;
            encode_len = bpv6_primary_block_encode(&primary, mbuf, 0, sz);
            if(encode_len != offset) {
                printf("Unable to update primary block: encoded value length mismatch.\n");
                printf("%d expected, %d actual\n", (int)offset, (int)encode_len);
                return -3;
            }

            ++received_count;
            bytes_total += sz;
        }

        // push the datagrams to wherever we need them to go next
        #ifdef __APPLE__
        sendmsg(fd, &msgbuf->msg_hdr, 0);
        #else
        sendmmsg(fd, msgbuf, res, 0);
        #endif
        
        // Fix our buffers to be correct for reception again ...
        for(i = 0; i < BP_MSG_NBUF; ++i) {
            msgbuf[i].msg_hdr.msg_iov->iov_len = BP_MSG_BUFSZ;
        }

        if(received_count >= batch) {
            gettimeofday(&tv, NULL);
            double curr_time = ((double)tv.tv_sec) + ((double)tv.tv_usec / 1000000.0);
            curr_time -= start; 
            fprintf(log, "%0.6f, %lu, %lu, %s, %d\n", curr_time, received_count, bytes_total, dst, dst_port);
            fflush(log);

            received_count = 0;
            bytes_total = 0;
        }
    }

    return 0;
}
