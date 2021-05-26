#include <string.h>
#include <stdio.h>
//#include <sys/socket.h>
#include <unistd.h>
#include <time.h>
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
#define BP_GEN_LOGFILE           "bpgen.%lu.csv"

#ifdef __APPLE__  // If we're on an apple platform, we can really only send one bundle at once

#define BP_MSG_NBUF   (1)

struct mmsghdr {
    struct msghdr msg_hdr;
    unsigned int  msg_len;
};

#else             // If we're on a different platform, then we can use sendmmsg / recvmmsg

#define BP_MSG_NBUF   (32/8)

#endif

struct bpgen_hdr {
    uint64_t seq;
    uint64_t tsc;
    timespec abstime;
};

int main(int argc, char* argv[]) {
    printf("Initializing ...\n");
    struct mmsghdr* msgbuf;
    uint64_t bundle_count = 0;
    uint64_t bundle_data = 0;
    uint64_t raw_data = 0;
    uint64_t rate = 0;
    char* target = NULL;
    struct sockaddr_in servaddr;
    int run_state = 1;
    int fd = -1;
    int source_node = BP_GEN_SRC_NODE_DEFAULT;
    int dest_node = BP_GEN_DST_NODE_DEFAULT;
    int port = BP_GEN_PORT_DEFAULT;
    size_t gen_sz = BP_BUNDLE_DEFAULT_SZ;
    ssize_t res;
    uint8_t use_tcp = 0;
    char* logfile = (char*)malloc(2048);
    snprintf(logfile, 2048, BP_GEN_LOGFILE, time(0));
    FILE* log = NULL;
    int c;
    
    static char usage[] = "usage: %s [-d target_ip]  [-p port] [-s bundle_size] [-r rate] [-m source node ] [-n destination node ] -T\n";

    while ((c = getopt(argc, argv, "d:f:hm:n:p:r:s:T")) != -1) {
        switch (c) {
            case 'd':
                target = strdup(optarg);
                break;
            case 'f':
                snprintf(logfile, 2048, "%sbpgen.%lu.csv", optarg, time(0));
                break;
            case 'r':
                rate = (uint64_t) atoi(optarg);
                if(rate > BP_GEN_RATE_MAX) {
                    rate = BP_GEN_RATE_MAX;
                }
                break;
	        case 'm':
                source_node = atoi(optarg);
                break;
            case 'n':
                dest_node = atoi(optarg);
                break;
            case 'p':
                port = atoi(optarg);
                break;
            case 's':
                gen_sz = (size_t) atoi(optarg);
                if (gen_sz < 100) {
                    gen_sz = 100;
                }
                if (gen_sz > BP_GEN_BUNDLE_MAXSZ) {
                    gen_sz = BP_GEN_BUNDLE_MAXSZ;
                }
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

    log = fopen(logfile, "w+");
    if(NULL == log) {
        perror("fopen()");
        return -5;
    }

    printf("Generating bundles of size %d\n", (int)gen_sz);
    if(rate) {
        printf("Generating up to %d bundles / second.\n", (int)rate);
    }
    if(NULL == target) {
        target = strdup(BP_GEN_TARGET_DEFAULT);
    }
    printf("Bundles will be destinated for %s:%d\n", target, port);

    char* data_buffer[gen_sz];
    memset(data_buffer, 0, gen_sz);
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
    timespec tp;

    bzero(&servaddr,sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = inet_addr(target);
    servaddr.sin_port = htons(port);

    if(use_tcp) {
        printf("Establishing connection to target ...\n");
        res = connect(fd, (struct sockaddr *)&servaddr, sizeof(servaddr));
        if(res < 0) {
            perror("Connection failed");
            return -2;
        }
    }
    
    msgbuf = (mmsghdr*) malloc(sizeof(struct mmsghdr) * BP_MSG_NBUF);
    memset(msgbuf, 0, sizeof(struct mmsghdr) * BP_MSG_NBUF);

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
        msgbuf[i].msg_hdr.msg_name = (void *)&servaddr;
        msgbuf[i].msg_hdr.msg_namelen = sizeof(struct sockaddr_in);
    }

    printf("Entering run state ...\n");
    struct timeval tv;
    gettimeofday(&tv, NULL);
    double start = ((double)tv.tv_sec) + ((double)tv.tv_usec / 1000000.0);
    printf("Start: +%f\n", start);
    uint64_t tsc_total = 0;
    double sval = 0.0;
    if(rate) {
        sval = 1000000.0 / rate;   // sleep val in usec
        sval *= BP_MSG_NBUF;
        printf("Sleeping for %f usec between bursts\n", sval);
    }

    uint64_t bseq = 0;
    while(run_state) {
        int idx = 0;
        for(idx = 0; idx < BP_MSG_NBUF; ++idx) {
            char* curr_buf = (char*)(msgbuf[idx].msg_hdr.msg_iov->iov_base);
            curr_time = time(0);
            if(curr_time == last_time) {
                ++seq;
            }
            else {
                gettimeofday(&tv, NULL);
                double elapsed = ((double)tv.tv_sec) + ((double)tv.tv_usec / 1000000.0);
                elapsed -= start;
                start = start + elapsed;
                fprintf(log, "%0.6f, %lu, %lu, %lu, %lu\n", elapsed, bundle_count, raw_data, bundle_data, tsc_total);
                fflush(log);
                bundle_count = 0;
                bundle_data = 0;
                raw_data = 0;
                tsc_total = 0;
                seq = 0;
            }
            last_time = curr_time;

            bpv6_primary_block primary;
            memset(&primary, 0, sizeof(bpv6_primary_block));
            primary.version = 6;
            bpv6_canonical_block block;
            memset(&block, 0, sizeof(bpv6_canonical_block));
            uint64_t bundle_length = 0;
            primary.flags = bpv6_bundle_set_priority(BPV6_PRIORITY_EXPEDITED) | bpv6_bundle_set_gflags(BPV6_BUNDLEFLAG_SINGLETON | BPV6_BUNDLEFLAG_NOFRAGMENT);
            primary.src_node = source_node;
            primary.src_svc = 1;
            primary.dst_node = dest_node;
            primary.dst_svc = 1;
            primary.creation = (uint64_t)bpv6_unix_to_5050(curr_time);
            primary.sequence = seq;
            uint64_t tsc_start = rdtsc();
            bundle_length = bpv6_primary_block_encode(&primary, curr_buf, 0, BP_MSG_BUFSZ);
            tsc_total += rdtsc() - tsc_start;
            block.type = BPV6_BLOCKTYPE_PAYLOAD;
            block.flags = BPV6_BLOCKFLAG_LAST_BLOCK;
            block.length = gen_sz;
            tsc_start = rdtsc();
            bundle_length += bpv6_canonical_block_encode(&block, curr_buf, bundle_length, BP_MSG_BUFSZ);
            tsc_total += rdtsc() - tsc_start;
            hdr->tsc = rdtsc();
            clock_gettime(CLOCK_REALTIME, &hdr->abstime);
            hdr->seq = bseq++;
            memcpy(curr_buf + bundle_length, data_buffer, gen_sz);
            bundle_length += gen_sz;
            msgbuf[idx].msg_hdr.msg_iov[0].iov_len = bundle_length;
            ++bundle_count;
            bundle_data += gen_sz;     // payload data
            raw_data += bundle_length; // bundle overhead + payload data
        }

#ifdef __APPLE__
        res = sendmsg(fd, &msgbuf[0].msg_hdr, 0);
        if(res < 0) {
            perror("cannot send message");
        }

#else
        res = sendmmsg(fd, msgbuf, BP_MSG_NBUF, 0);
        if(res < 0) {
            perror("cannot send message");
        }

#endif
        if(rate) {
            usleep((uint64_t)sval);
        }
    }

    close(fd);
    return 0;
}
