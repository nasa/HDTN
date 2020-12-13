#include <iostream>
#include <fstream>
#include <gtest/gtest.h>
#include <zmq.hpp>
#include <arpa/inet.h>
#include <sys/time.h>
#include <store.hpp>
#include <thread>
#include <codec/bpv6.h>
#include <util/tsc.h>
#include <reg.hpp>
#include <ingress.h>
#include <egress.h>

// For runStorage
volatile bool RUN_STORAGE = true;
static uint64_t _last_bytes;
static uint64_t _last_count;
// End for runStorage

// For runBpgen
volatile bool RUN_BPGEN = true;
#define BP_MSG_BUFSZ             (65536)
#define BP_BUNDLE_DEFAULT_SZ     (100)
#define BP_GEN_BUNDLE_MAXSZ      (64000)
#define BP_GEN_RATE_MAX          (1 << 30)
#define BP_GEN_TARGET_DEFAULT    "127.0.0.1"
#define BP_GEN_PORT_DEFAULT      (4556)
#define BP_GEN_SRC_NODE_DEFAULT  (1)
#define BP_GEN_DST_NODE_DEFAULT  (2)
#define BP_GEN_LOGFILE           "bpgen.%lu.csv"
#define BP_MSG_NBUF   (32)
struct bpgen_hdr {
    uint64_t seq;
    uint64_t tsc;
    timespec abstime;
};
// End -- for runBpgen

// For runIngress
volatile bool RUN_INGRESS = true;
#define INGRESS_PORT (4556)
// End -- for runIngress

// For runEgress
volatile bool RUN_EGRESS = true;
// End -- for runEgress

int runEgress() {

    uint64_t bundle_count = 0;
    uint64_t bundle_data = 0;
    uint64_t message_count = 0;
    double elapsed = 0;
    //uint64_t start;


    hdtn::hegr_manager egress;
    bool ok = true;
    struct timeval tv;
    gettimeofday(&tv, NULL);
    double start = ((double)tv.tv_sec) + ((double)tv.tv_usec / 1000000.0);
    printf("Start: +%f\n", start);
    //finish registration stuff - egress should register, ingress will query
    hdtn::hdtn_regsvr regsvr;
    regsvr.init("tcp://127.0.0.1:10140", "egress", 10149, "PULL");
    regsvr.reg();
    hdtn::hdtn_entries res = regsvr.query();
    for (auto entry : res) {
        std::cout << entry.address << ":" << entry.port << ":" << entry.mode << std::endl;
    }
    ///
    zmq::context_t *zmq_ctx;
    zmq::socket_t *zmq_sock;
    zmq_ctx = new zmq::context_t;
    zmq_sock = new zmq::socket_t(*zmq_ctx, zmq::socket_type::pull);
    zmq_sock->connect("tcp://127.0.0.1:10149");
    egress.init();
    int entry_status;
    entry_status = egress.add(1, HEGR_FLAG_UDP, "127.0.0.1", 4557);
    if (!entry_status) {
        return 0;  //error message prints in add function
    }
    printf("Announcing presence of egress ...\n");
    for (int i = 0; i < 8; ++i) {
        egress.up(i);
    }
    char bundle[HMSG_MSG_MAX];
    int bundle_size = 0;
    int num_frames = 0;
    int frame_index = 0;
    int max_frames = 0;

    char *type;
    size_t payload_size;
    while (true) {
        gettimeofday(&tv, NULL);
        elapsed = ((double)tv.tv_sec) + ((double)tv.tv_usec / 1000000.0);
        elapsed -= start;
        zmq::message_t hdr;
        zmq::message_t message;
        zmq_sock->recv(&hdr);
        message_count++;
        char bundle[HMSG_MSG_MAX];
        if (hdr.size() < sizeof(hdtn::common_hdr)) {
            std::cerr << "[dispatch] message too short: " << hdr.size() << std::endl;
            return -1;
        }
        hdtn::common_hdr *common = (hdtn::common_hdr *)hdr.data();
        hdtn::block_hdr *block = (hdtn::block_hdr *)common;
        switch (common->type) {
            case HDTN_MSGTYPE_STORE:
                zmq_sock->recv(&message);
                bundle_size = message.size();
                memcpy(bundle, message.data(), bundle_size);
                egress.forward(1, bundle, bundle_size);
                bundle_data += bundle_size;
                bundle_count++;
                break;
        }
    }
    return 0;
}

int runIngress() {
    hdtn::bp_ingress ingress;
    ingress.init(BP_INGRESS_TYPE_UDP);
    uint64_t last_time = 0;
    uint64_t curr_time = 0;
    //finish registration stuff -ingress will find out what egress services have registered
    hdtn::hdtn_regsvr regsvr;
    regsvr.init("tcp://127.0.0.1:10140", "ingress", 10149, "PUSH");
    regsvr.reg();
    hdtn::hdtn_entries res = regsvr.query();
    for (auto entry : res) {
        std::cout << entry.address << ":" << entry.port << ":" << entry.mode << std::endl;
    }

    printf("Announcing presence of ingress engine ...\n");

    ingress.netstart(INGRESS_PORT);
    int count = 0;
    struct timeval tv;
    gettimeofday(&tv, NULL);
    double start = ((double)tv.tv_sec) + ((double)tv.tv_usec / 1000000.0);
    printf("Start: +%f\n", start);
    while (true) {
        curr_time = time(0);
        gettimeofday(&tv, NULL);
        ingress.elapsed = ((double)tv.tv_sec) + ((double)tv.tv_usec / 1000000.0);
        ingress.elapsed -= start;
        count = ingress.update();
        ingress.process(count);
        last_time = curr_time;
    }

    return 0;
}

int runBpgen() {
    printf("Initializing ...\n");
    struct mmsghdr* msgbuf;
    uint64_t bundle_count = 0;
    uint64_t bundle_data = 0;
    uint64_t raw_data = 0;
    uint64_t rate = 0;
    char* target = NULL;
    struct sockaddr_in servaddr;
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

    gen_sz = 1500;
    if (gen_sz < 100) {
        gen_sz = 100;
    }
    if (gen_sz > BP_GEN_BUNDLE_MAXSZ) {
        gen_sz = BP_GEN_BUNDLE_MAXSZ;
    }
    dest_node = 1;


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
    while(RUN_BPGEN) {
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
        res = sendmmsg(fd, msgbuf, BP_MSG_NBUF, 0);
        if(res < 0) {
            perror("cannot send message");
        }
        if(rate) {
            usleep((uint64_t)sval);
        }
    }
    close(fd);
    return 0;
}

int runStorage() {
    double last = 0.0;
    timeval tv;
    gettimeofday(&tv, NULL);
    last = (tv.tv_sec + (tv.tv_usec / 1000000.0));
    hdtn::storage_config config;
    config.regsvr = "tcp://127.0.0.1:10140";
    config.local = "tcp://127.0.0.1:10145";
    config.store_path = "/tmp/hdtn.store";
    hdtn::storage store;
    std::cout << "[store] Initializing storage ..." << std::endl;
    if (!store.init(config)) {
        return -1;
    }
    while (RUN_STORAGE) {
        store.update();
        gettimeofday(&tv, NULL);
        double curr = (tv.tv_sec + (tv.tv_usec / 1000000.0));
        if (curr - last > 1) {
            last = curr;
            hdtn::storage_stats *stats = store.stats();
            uint64_t cbytes = stats->in_bytes - _last_bytes;
            uint64_t ccount = stats->in_msg - _last_count;
            _last_bytes = stats->in_bytes;
            _last_count = stats->in_msg;

            printf("[store] Received: %d msg / %0.2f MB\n", ccount, cbytes / (1024.0 * 1024.0));
        }
    }

    std::cout << "[store] Ending storage ..." << std::endl;
    return 0;
}


std::string GetEnv( const std::string & var ) {
     const char * val = std::getenv( var.c_str() );
     if ( val == nullptr ) { // invalid to assign nullptr to std::string
         return "";
     }
     else {
         return val;
     }
}

void startStorage() {
    std::string hdtnBuildRoot = GetEnv("HDTN_BUILD_ROOT");
    std::string commandStartStorage = hdtnBuildRoot + "/module/storage/hdtn-storage &";
    system(commandStartStorage.c_str());
    std::cout << " >>>>> Started the storage service." << std::endl;
}

void startEgress() {
    std::string hdtnBuildRoot = GetEnv("HDTN_BUILD_ROOT");
    std::string commandStartEgress = hdtnBuildRoot + "/module/egress/hdtn-egress &";
    system(commandStartEgress.c_str());
    std::cout << " >>>>> Started the egress service." << std::endl;
}

void startIngress() {
    std::string hdtnBuildRoot = GetEnv("HDTN_BUILD_ROOT");
    std::string commandStartIngress = hdtnBuildRoot + "/module/ingress/hdtn-ingress &";
    system(commandStartIngress.c_str());
    std::cout << " >>>>> Started the ingress service." << std::endl;
}

void startBpgen() {
    std::string hdtnBuildRoot = GetEnv("HDTN_BUILD_ROOT");
    std::string commandStartBpgen = hdtnBuildRoot + "/common/bpcodec/apps/bpgen &";
    system(commandStartBpgen.c_str());
    std::cout << " >>>>> Started bpgen." << std::endl;
}

void startRegistrationService() {
    // Start the registration service
    std::string hdtnSourceRoot = GetEnv("HDTN_SOURCE_ROOT");
    std::string commandStartReg = "python3 " + hdtnSourceRoot + "/common/regsvr/main.py &";
    system(commandStartReg.c_str());
    std::cout << " >>>>> Started the registration service." << std::endl;
}

void stopStorage() {
    system("killall hdtn-storage");
    std::cout << " >>>>> Stopped the Storage service." << std::endl;
}

void stopEgress() {
    system("killall hdtn-egress");
    std::cout << " >>>>> Stopped the Egress service." << std::endl;
}

void stopIngress() {
    system("killall hdtn-ingress");
    std::cout << " >>>>> Stopped the Ingress service." << std::endl;
}

void stopBpgen() {
    system("killall bpgen");
    std::cout << " >>>>> Stopped Bpgen." << std::endl;
}

void stopRegistrationService() {
    // Stop the registration service
    std::string target = "tcp://127.0.0.1:10140";
    std::string svc = "test";
    uint16_t port = 10140;
    std::string mode = "PUSH";
    zmq::context_t * _zmq_ctx = new zmq::context_t;
    zmq::socket_t * _zmq_sock = new zmq::socket_t(*_zmq_ctx, zmq::socket_type::req);
    char tbuf[255];
    memset(tbuf, 0, 255);
    snprintf(tbuf, 255, "%s:%d:%s", svc.c_str(), port, mode.c_str());
    _zmq_sock->setsockopt(ZMQ_IDENTITY, (void *)tbuf, target.size());
    _zmq_sock->connect(target);
    _zmq_sock->send("SHUTDOWN", strlen("SHUTDOWN"), 0);
    _zmq_sock->close();
    delete _zmq_sock;
    delete _zmq_ctx;
    std::cout << " <<<<< Stopped the registration service." << std::endl;
}



int main(int ac, char*av[]) {

	std::cout << "Running Integrated Tests. " << std::endl << std::flush;

    startRegistrationService();

    //startBpgen();
    //startIngress();
    //startEgress();

    std::thread threadBpgen(runBpgen);
    std::thread threadIngress(runIngress);
    std::thread threadEgress(runEgress);

    sleep(2);
    std::thread threadStorage(runStorage);

    sleep(5);

    //stopBpgen();
    //stopIngress();
    //stopEgress();

    RUN_BPGEN = false;
    RUN_INGRESS = false;
    RUN_EGRESS = false;
    RUN_STORAGE = false;
    threadBpgen.join();
    threadIngress.join();
    threadEgress.join();
    threadStorage.join();

    stopRegistrationService();

    std::cout << "End Integrated Tests. " << std::endl << std::flush;
	return 0;
}
