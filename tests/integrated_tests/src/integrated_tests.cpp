#include <arpa/inet.h>
#include <codec/bpv6.h>
#include <egress.h>
#include <fcntl.h>
#include <gtest/gtest.h>
#include <ingress.h>
#include <sys/time.h>
#include <unistd.h>
#include <util/tsc.h>

#include <fstream>
#include <iostream>
#include <reg.hpp>
#include <store.hpp>
#include <thread>
#include <zmq.hpp>

// Used for forking python process
#include <signal.h> /* for SIGTERM, SIGKILL */
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h> /* for pid_t            */
#include <sys/wait.h>  /* for waitpid          */
#include <unistd.h>    /* for fork, exec, kill */

#include <boost/process.hpp>
#include <boost/thread.hpp>
#include <boost/assign/list_of.hpp>
#include <string>
#include <vector>

#include <BpSinkAsync.h>


#define BP_MSG_BUFSZ             (65536)
#define BP_BUNDLE_DEFAULT_SZ     (100)
#define BP_GEN_BUNDLE_MAXSZ      (64000)
#define BP_GEN_RATE_MAX          (1 << 30)
#define BP_GEN_TARGET_DEFAULT    "127.0.0.1"
#define BP_GEN_PORT_DEFAULT      (4556)
#define BP_GEN_SRC_NODE_DEFAULT  (1)
#define BP_GEN_DST_NODE_DEFAULT  (2)
#define BP_GEN_BATCH_DEFAULT     (1 << 18)  // write out one entry per this many bundles.
#define BP_GEN_LOGFILE           "bpsink.%lu.csv"

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


// Prototypes
std::string GetEnv(const std::string& var);
int RunBpgen();
int RunBpsink();
int RunIngress(uint64_t* ptrBundleCount, uint64_t* ptrBundleData);
int RunEgress(uint64_t* ptrBundleCount, uint64_t* ptrBundleData);
bool IntegratedTest1();
bool IntegratedTest2();
bool IntegratedTest3();
//pid_t SpawnPythonServer(void);
//int KillProcess(pid_t processId);

volatile bool RUN_BPGEN = true;
volatile bool RUN_BPSINK = true;
volatile bool RUN_INGRESS = true;
volatile bool RUN_EGRESS = true;
volatile bool RUN_STORAGE = true;

// Create a test fixture.
class IntegratedTestsFixture : public testing::Test {
public:
    IntegratedTestsFixture();
    ~IntegratedTestsFixture();
    void SetUp() override;     // This is called after constructor.
    void TearDown() override;  // This is called before destructor.
    bool m_runningPythonServer;
private:
    void StartPythonServer();
    void StopPythonServer();
    boost::process::child * m_ptrChild;
    std::thread * m_ptrThreadPython;
};

void IntegratedTestsFixture::StopPythonServer() {
    std::cout << "StopPythonServer started." << std::endl << std::flush;
    m_runningPythonServer = false;
    if (m_ptrChild) {
        m_ptrChild->terminate();
        m_ptrChild->wait();
        int result = m_ptrChild->exit_code();
        std::cout << "StopPythonServer ended with result = " << result << std::endl << std::flush;
    }
    std::cout << "StopPythonServer ended." << std::endl << std::flush;
}

void IntegratedTestsFixture::StartPythonServer() {
    std::cout << "StartPythonServer started." << std::endl << std::flush;
    m_runningPythonServer = true;
    std::string commandArg = GetEnv("HDTN_SOURCE_ROOT") + "/common/regsvr/main.py";
    std::cout << "Running python3 " << commandArg << std::endl << std::flush;
    m_ptrChild = new boost::process::child(boost::process::search_path("python3"),commandArg);
    while(m_ptrChild->running()) {
        while(m_runningPythonServer) {
            usleep(250000);  // 0.25 seconds
            //std::cout << "StartPythonServer is running. " << std::endl << std::flush;
        }
    }
    std::cout << "StartPythonServer ended." << std::endl << std::flush;
}

IntegratedTestsFixture::IntegratedTestsFixture() {
    //    std::cout << "Called IntegratedTests1Fixture::IntegratedTests1Fixture()" << std::endl;
}

IntegratedTestsFixture::~IntegratedTestsFixture() {
    //    std::cout << "Called IntegratedTests1Fixture::~IntegratedTests1Fixture()" << std::endl;
}

void IntegratedTestsFixture::SetUp() {
    std::cout << "IntegratedTests1Fixture::SetUp called\n";
    m_ptrThreadPython = new std::thread(&IntegratedTestsFixture::StartPythonServer,this);
}

void IntegratedTestsFixture::TearDown() {
    std::cout << "IntegratedTests1Fixture::TearDown called\n";
    this->StopPythonServer();
}

TEST_F(IntegratedTestsFixture, DISABLED_IntegratedTest1) {
    bool result = IntegratedTest1();
    EXPECT_EQ(true, result);
}

TEST_F(IntegratedTestsFixture, DISABLED_IntegratedTest2) {
    bool result = IntegratedTest2();
    EXPECT_EQ(true, result);
}

TEST_F(IntegratedTestsFixture, IntegratedTest3) {
    bool result = IntegratedTest3();
    EXPECT_EQ(true, result);
}

bool IntegratedTest1() {
    std::cout << "Running Integrated Test 1. " << std::endl << std::flush;

    uint64_t bundleCountIngress = 0;
    uint64_t bundleDataIngress = 0;
    sleep(1);
    std::thread threadIngress(RunIngress, &bundleCountIngress, &bundleDataIngress);
    sleep(1);
    uint64_t bundleCountEgress = 0;
    uint64_t bundleDataEgress = 0;
    std::thread threadEgress(RunEgress, &bundleCountEgress, &bundleDataEgress);
    sleep(1);
    std::thread threadBpgen(RunBpgen);
    sleep(3);
    RUN_BPGEN = false;
//    sleep(2);
    RUN_INGRESS = false;
    std::cout << "Before threadIngress.join(). " << std::endl << std::flush;
    threadIngress.join();
    threadBpgen.join();
    std::cout << "After threadIngress.join(). " << std::endl << std::flush;
    sleep(1);
    RUN_EGRESS = false;
    std::cout << "Before threadEgress.join(). " << std::endl << std::flush;
    threadEgress.join();
    std::cout << "After threadEgress.join(). " << std::endl << std::flush;

    //    killProcess(pidPythonServer);

    std::cout << "End Integrated Tests. " << std::endl << std::flush;
    sleep(3);
    std::cout << "bundleCountIngress: " << bundleCountEgress << " , bundleDataIngress: " << bundleDataIngress
              << std::endl
              << std::flush;
    std::cout << "bundleCountEgress: " << bundleCountEgress << " , bundleDataEgress: " << bundleDataEgress
              << std::endl
              << std::flush;
    if ((bundleCountIngress == bundleCountEgress) && (bundleDataIngress == bundleDataEgress)) {
        return true;
    }
    return false;
}

bool IntegratedTest2() {
    std::cout << "Running Integrated Test 2. " << std::endl << std::flush;
    sleep(5);
    return true;
}

bool IntegratedTest3() {
    std::cout << "Running Integrated Test 3. " << std::endl << std::flush;

    uint64_t bundleCountIngress = 0;
    uint64_t bundleDataIngress = 0;
    uint64_t bundleCountEgress = 0;
    uint64_t bundleDataEgress = 0;

    sleep(1);
    std::thread threadEgress(RunEgress, &bundleCountEgress, &bundleDataEgress);
    sleep(1);
    std::thread threadIngress(RunIngress, &bundleCountIngress, &bundleDataIngress);
    sleep(1);
    std::thread threadBpsink(RunBpsink);
    sleep(1);
    std::thread threadBpgen(RunBpgen);

    sleep(3);


    RUN_BPGEN = false;
    threadBpgen.join();
    std::cout << "After threadBpgen.join(). " << std::endl << std::flush;


    RUN_INGRESS = false;
    threadIngress.join();
    std::cout << "After threadIngress.join(). " << std::endl << std::flush;

    RUN_EGRESS = false;
    threadEgress.join();
    std::cout << "After threadEgress.join(). " << std::endl << std::flush;

    RUN_BPSINK = false;
    threadBpsink.join();
    std::cout << "After threadBpsink.join(). " << std::endl << std::flush;


    std::cout << "End Integrated Test 3. " << std::endl << std::flush;
    std::cout << "bundleCountIngress: " << bundleCountEgress << " , bundleDataIngress: " << bundleDataIngress
              << std::endl
              << std::flush;
    std::cout << "bundleCountEgress: " << bundleCountEgress << " , bundleDataEgress: " << bundleDataEgress
              << std::endl
              << std::flush;
    if ((bundleCountIngress == bundleCountEgress) && (bundleDataIngress == bundleDataEgress)) {
        return true;
    }
    return false;
}




int RunBpgen() {
    std::cout << "Start runBpgen ... " << std::endl << std::flush;
    struct BpgenHdr {
        uint64_t seq;
        uint64_t tsc;
        timespec abstime;
    };
    int bpMsgBufsz = 65536;
//    int bpMsgNbuf = 32;
    int bpMsgNbuf = 4;
    struct mmsghdr* msgbuf;
    uint64_t bundleCount = 0;
    uint64_t bundleData = 0;
    uint64_t rawData = 0;
//    uint64_t rate = 50;
    uint64_t rate = 15000;
    std::string target = "127.0.0.1";
    struct sockaddr_in servaddr;
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    int sourceNode = 1;
    int destNode = 1;
    int port = 4556;
    size_t genSz = 1500;
    ssize_t res;
    printf("Generating bundles of size %d\n", (int)genSz);
    if (rate) {
        printf("Generating up to %d bundles / second.\n", (int)rate);
    }
    printf("Bundles will be destinated for %s:%d\n", target.c_str(), port);
    fflush(stdout);
    char* dataBuffer[genSz];
    memset(dataBuffer, 0, genSz);
    BpgenHdr* hdr = (BpgenHdr*)dataBuffer;
    uint64_t lastTime = 0;
    uint64_t currTime = 0;
    uint64_t seq = 0;
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = inet_addr(target.c_str());
    servaddr.sin_port = htons(port);
    msgbuf = (mmsghdr*)malloc(sizeof(struct mmsghdr) * bpMsgNbuf);
    memset(msgbuf, 0, sizeof(struct mmsghdr) * bpMsgNbuf);
    struct iovec* io = (iovec*)malloc(sizeof(struct iovec) * bpMsgNbuf);
    memset(io, 0, sizeof(struct iovec) * bpMsgNbuf);
    char** tmp = (char**)malloc(bpMsgNbuf * sizeof(char*));
    int i = 0;
    for (i = 0; i < bpMsgNbuf; ++i) {
        tmp[i] = (char*)malloc(bpMsgBufsz);
        memset(tmp[i], 0x0, bpMsgBufsz);
    }
    for (i = 0; i < bpMsgNbuf; ++i) {
        msgbuf[i].msg_hdr.msg_iov = &io[i];
        msgbuf[i].msg_hdr.msg_iovlen = 1;
        msgbuf[i].msg_hdr.msg_iov->iov_base = tmp[i];
        msgbuf[i].msg_hdr.msg_iov->iov_len = bpMsgBufsz;
        msgbuf[i].msg_hdr.msg_name = (void*)&servaddr;
        msgbuf[i].msg_hdr.msg_namelen = sizeof(struct sockaddr_in);
    }
    printf("Entering run state ...\n");
    struct timeval tv;
    gettimeofday(&tv, NULL);
    double start = ((double)tv.tv_sec) + ((double)tv.tv_usec / 1000000.0);
    printf("Start: +%f\n", start);
    fflush(stdout);
    uint64_t tscTotal = 0;
    double sval = 0.0;
    if (rate) {
        sval = 1000000.0 / rate;  // sleep val in usec
        sval *= bpMsgNbuf;
        printf("Sleeping for %f usec between bursts\n", sval);
        fflush(stdout);
    }
    uint64_t bseq = 0;
    uint64_t totalBundleCount = 0;
    uint64_t totalSize = 0;
    while (RUN_BPGEN) {
        for (int idx = 0; idx < bpMsgNbuf; ++idx) {
            char* currBuf = (char*)(msgbuf[idx].msg_hdr.msg_iov->iov_base);
            currTime = time(0);
            if (currTime == lastTime) {
                ++seq;
            }
            else {
                gettimeofday(&tv, NULL);
                double elapsed = ((double)tv.tv_sec) + ((double)tv.tv_usec / 1000000.0);
                elapsed -= start;
                start = start + elapsed;
                bundleCount = 0;
                bundleData = 0;
                rawData = 0;
                tscTotal = 0;
                seq = 0;
            }
            lastTime = currTime;
            bpv6_primary_block primary;
            memset(&primary, 0, sizeof(bpv6_primary_block));
            primary.version = 6;
            bpv6_canonical_block block;
            memset(&block, 0, sizeof(bpv6_canonical_block));
            uint64_t bundleLength = 0;
            primary.flags = bpv6_bundle_set_priority(BPV6_PRIORITY_EXPEDITED) |
                            bpv6_bundle_set_gflags(BPV6_BUNDLEFLAG_SINGLETON | BPV6_BUNDLEFLAG_NOFRAGMENT);
            primary.src_node = sourceNode;
            primary.src_svc = 1;
            primary.dst_node = destNode;
            primary.dst_svc = 1;
            primary.creation = (uint64_t)bpv6_unix_to_5050(currTime);
            primary.sequence = seq;
            uint64_t tsc_start = rdtsc();
            bundleLength = bpv6_primary_block_encode(&primary, currBuf, 0, bpMsgBufsz);
            tscTotal += rdtsc() - tsc_start;
            block.type = BPV6_BLOCKTYPE_PAYLOAD;
            block.flags = BPV6_BLOCKFLAG_LAST_BLOCK;
            block.length = genSz;
            tsc_start = rdtsc();
            bundleLength += bpv6_canonical_block_encode(&block, currBuf, bundleLength, bpMsgBufsz);
            tscTotal += rdtsc() - tsc_start;
            hdr->tsc = rdtsc();
            clock_gettime(CLOCK_REALTIME, &hdr->abstime);
            hdr->seq = bseq++;
            memcpy(currBuf + bundleLength, dataBuffer, genSz);
            bundleLength += genSz;
            msgbuf[idx].msg_hdr.msg_iov[0].iov_len = bundleLength;
            ++bundleCount;
            bundleData += genSz;      // payload data
            rawData += bundleLength;  // bundle overhead + payload data
        }                               // End for(idx = 0; idx < BP_MSG_NBUF; ++idx) {
        res = sendmmsg(fd, msgbuf, bpMsgNbuf, 0);
        if (res < 0) {
            perror("cannot send message");
        }
        else {
            totalSize += msgbuf->msg_len;
            totalBundleCount += bundleCount;
            std::cout << "In BPGEN, totalBundleCount: " << totalBundleCount << " , totalSize: " << totalSize
                      << std::endl
                      << std::flush;
        }
        if (rate) {
            usleep((uint64_t)sval);
        }
    }
    sleep(3);
    close(fd);
    std::cout << "End runBpgen ... " << std::endl << std::flush;
    return 0;
}



int RunIngressOld(uint64_t* ptrBundleCount, uint64_t* ptrBundleData) {
    std::cout << "Start runIngress ... " << std::endl << std::flush;
    int ingressPort = 4556;
    hdtn::BpIngress ingress;
    ingress.Init(BP_INGRESS_TYPE_UDP);
    uint64_t lastTime = 0;
    uint64_t currTime = 0;
    // finish registration stuff -ingress will find out what egress services have
    // registered
    hdtn::HdtnRegsvr regsvr;
    regsvr.Init("tcp://127.0.0.1:10140", "ingress", 10149, "PUSH");
    regsvr.Reg();
    hdtn::HdtnEntries res = regsvr.Query();
    for (auto entry : res) {
        std::cout << entry.address << ":" << entry.port << ":" << entry.mode << std::endl;
    }
    printf("Announcing presence of ingress engine ...\n");
    fflush(stdout);
    ingress.Netstart(ingressPort);
    int count = 0;
    struct timeval tv;
    gettimeofday(&tv, NULL);
    double start = ((double)tv.tv_sec) + ((double)tv.tv_usec / 1000000.0);
    printf("Start: +%f\n", start);
    fflush(stdout);
    while (RUN_INGRESS) {
        currTime = time(0);
        gettimeofday(&tv, NULL);
        ingress.m_elapsed = ((double)tv.tv_sec) + ((double)tv.tv_usec / 1000000.0);
        ingress.m_elapsed -= start;
        // count = ingress.update();
        std::cout << "In RunIngress BEFORE Update " << std::endl << std::flush;
        ////////count = ingress.Update(0.5);  // Use timeout so call does not indefinitely
                                      // block.  Units are seconds
        std::cout << "In RunIngress AFTER Update " << std::endl << std::flush;
        if (count > 0) {
            ///////ingress.Process(count);
            std::cout << "In RunIngress.  count = " << count << " , ingress.m_bundleCount = " << ingress.m_bundleCount
                      << " , ingress.m_bundleData = " << ingress.m_bundleData << std::endl << std::flush;
        }
        lastTime = currTime;
    }
    std::cout << "End runIngress ... " << std::endl << std::flush;
    *ptrBundleCount = ingress.m_bundleCount;
    *ptrBundleData = ingress.m_bundleData;
    return 0;
}

int RunIngress(uint64_t* ptrBundleCount, uint64_t* ptrBundleData) {

    //scope to ensure clean exit before return 0
    {
        std::cout << "Start runIngress ... " << std::endl << std::flush;
        int ingressPort = 4556;

        hdtn::BpIngress ingress;
        ingress.Init(BP_INGRESS_TYPE_UDP);

        // finish registration stuff -ingress will find out what egress services have registered
        hdtn::HdtnRegsvr regsvr;
        regsvr.Init(HDTN_REG_SERVER_PATH, "ingress", 10100, "PUSH");
        regsvr.Reg();
        hdtn::HdtnEntries res = regsvr.Query();
        for (auto entry : res) {
            std::cout << entry.address << ":" << entry.port << ":" << entry.mode << std::endl;
        }

        printf("Announcing presence of ingress engine ...\n");

        ingress.Netstart(ingressPort);

        std::cout << "ingress up and running" << std::endl;
        while (RUN_INGRESS) {
            boost::this_thread::sleep(boost::posix_time::millisec(250));
        }

        std::cout << "End runIngress ... " << std::endl << std::flush;
        *ptrBundleCount = ingress.m_bundleCount;
        *ptrBundleData = ingress.m_bundleData;

    }

    std::cout<< "RunIngress: exited cleanly\n";
    return 0;

}

int RunEgress(uint64_t* ptBundleCount, uint64_t* ptrBundleData) {
    std::cout << "Start runEgress ... " << std::endl << std::flush;
    uint64_t messageCount = 0;
    double elapsed = 0;
    hdtn::HegrManager egress;
    struct timeval tv;
    gettimeofday(&tv, NULL);
    double start = ((double)tv.tv_sec) + ((double)tv.tv_usec / 1000000.0);
    printf("Start Egress: +%f\n", start);
    fflush(stdout);
    // finish registration stuff - egress should register, ingress will query
    hdtn::HdtnRegsvr regsvr;
    regsvr.Init("tcp://127.0.0.1:10140", "egress", 10149, "PULL");
    regsvr.Reg();
    hdtn::HdtnEntries res = regsvr.Query();
    for (auto entry : res) {
        std::cout << entry.address << ":" << entry.port << ":" << entry.mode << std::endl << std::flush;
    }
    zmq::context_t* zmqCtx;
    zmq::socket_t* zmqSock;
    zmqCtx = new zmq::context_t;
    zmqSock = new zmq::socket_t(*zmqCtx, zmq::socket_type::pull);
    zmqSock->connect("tcp://127.0.0.1:10149");
    egress.Init();
    int entryStatus;
    entryStatus = egress.Add(1, HEGR_FLAG_UDP, "127.0.0.1", 4557);
    if (!entryStatus) {
        return 0;  // error message prints in add function
    }
    printf("Announcing presence of egress ...\n");
    fflush(stdout);
    for (int i = 0; i < 8; ++i) {
        egress.Up(i);
    }
    int bundleSize = 0;
    // JCF, set timeout
    // Use a form of receive that times out so we can terminate cleanly.
    int timeout = 250;  // milliseconds
    zmqSock->setsockopt(ZMQ_RCVTIMEO, &timeout, sizeof(int));
    while (RUN_EGRESS) {
        gettimeofday(&tv, NULL);
        elapsed = ((double)tv.tv_sec) + ((double)tv.tv_usec / 1000000.0);
        elapsed -= start;
        zmq::message_t hdr;
        zmq::message_t message;
        // JCF
        // Use a form of receive that times out so we can terminate cleanly.  If no
        // message was received after timeout go back to top of loop
        // std::cout << "In runEgress, before recv. " << std::endl << std::flush;
        bool retValue = zmqSock->recv(&hdr);
        // std::cout << "In runEgress, after recv. retValue = " << retValue << " ,
        // hdr.size() = " << hdr.size() << std::endl << std::flush;
        if (!retValue) {
            continue;
        }
        messageCount++;
        char bundle[HMSG_MSG_MAX];
        if (hdr.size() < sizeof(hdtn::CommonHdr)) {
            std::cerr << "[dispatch] message too short: " << hdr.size() << std::endl << std::flush;
            return -1;
        }
        zmqSock->recv(&message);
        bundleSize = message.size();
        memcpy(bundle, message.data(), bundleSize);

        egress.Forward(1, bundle, bundleSize);

        (*ptrBundleData) += bundleSize;
        (*ptBundleCount)++;
    }
    // Clean up resources
    zmqSock->close();
    delete zmqSock;
    delete zmqCtx;
    return 0;
}

std::string GetEnv(const std::string& var) {
    const char* val = std::getenv(var.c_str());
    if (val == nullptr) {  // invalid to assign nullptr to std::string
        return "";
    }
    else {
        return val;
    }
}

//int RunBpsink() {
//    bpv6_primary_block primary;
//    bpv6_canonical_block payload;
//    char* logfile = (char*)malloc(2048);
//    snprintf(logfile, 2048, BP_GEN_LOGFILE, time(0));

//    memset(&primary, 0, sizeof(bpv6_primary_block));
//    memset(&payload, 0, sizeof(bpv6_canonical_block));

//    printf("Stargin RunBpsink...\n");
//    struct mmsghdr* msgbuf;
//    bool use_one_way = true;
//    uint64_t bundle_count = 0;
//    uint64_t bundle_data = 0;
//    uint32_t batch = BP_GEN_BATCH_DEFAULT;
//    char* target = NULL;
//    struct sockaddr_in servaddr;
//    int run_state = 1;
//    int fd = -1;
//    int port = BP_GEN_PORT_DEFAULT;
//    ssize_t res;
//    uint8_t use_tcp = 0;
//    int c;


//    if(use_one_way) {
//        printf("Measuring one-way latency.\n");
//    }

//    if(NULL == target) {
//        target = strdup(BP_GEN_TARGET_DEFAULT);
//    }

//    bzero(&servaddr,sizeof(servaddr));
//    servaddr.sin_family = AF_INET;
//    res = inet_pton(AF_INET, target, &(servaddr.sin_addr) );
//    if(1 != res) {
//        printf("Invalid address specified: %s\n", target);
//        return -1;
//    }
//    servaddr.sin_port = htons(port);

//    printf("Starting server on %s:%d\n", target, port);

//    char* data_buffer[BP_MSG_BUFSZ];
//    memset(data_buffer, 0, BP_MSG_BUFSZ);
//    bpgen_hdr* hdr = (bpgen_hdr*)data_buffer;

//    if(use_tcp) {
//        fd = socket(AF_INET,SOCK_STREAM,0);

//    }
//    else {
//        fd = socket(AF_INET,SOCK_DGRAM,0);

//    }
//    uint64_t last_time = 0;
//    uint64_t curr_time = 0;
//    uint64_t seq = 0;

//    printf("Checking TSC frequency ...\n");
//    uint64_t freq_base = tsc_freq(5000000);

//    res = bind(fd, (sockaddr*) &servaddr, sizeof(sockaddr_in));
//    if(res < 0) {
//        perror("bind() failed");
//        return -3;
//    }
//    if(use_tcp) {
//        printf("Waiting for incoming connection ...\n");
//        int tfd = fd;
//        res = listen(tfd, 1);
//        if(res < 0) {
//            perror("listen() failed");
//            return -2;
//        }
//        socklen_t sa_len = 0;
//        fd = accept(tfd, (sockaddr *)(&servaddr), &sa_len);
//    }

//    msgbuf = (mmsghdr*) malloc(sizeof(struct mmsghdr) * BP_MSG_NBUF);
//    memset(msgbuf, 0, sizeof(struct mmsghdr) * BP_MSG_NBUF);

//    struct iovec* io = (iovec*) malloc(sizeof(struct iovec) * BP_MSG_NBUF);
//    memset(io, 0, sizeof(struct iovec) * BP_MSG_NBUF);

//    char** tmp = (char**) malloc(BP_MSG_NBUF * sizeof(char *));
//    int i = 0;
//    for(i = 0; i < BP_MSG_NBUF; ++i) {
//        tmp[i] = (char*) malloc(BP_MSG_BUFSZ);
//        memset(tmp[i], 0x0, BP_MSG_BUFSZ);
//    }

//    for(i = 0; i < BP_MSG_NBUF; ++i) {
//        msgbuf[i].msg_hdr.msg_iov = &io[i];
//        msgbuf[i].msg_hdr.msg_iovlen = 1;
//        msgbuf[i].msg_hdr.msg_iov->iov_base = tmp[i];
//        msgbuf[i].msg_hdr.msg_iov->iov_len = BP_MSG_BUFSZ;
//        msgbuf[i].msg_hdr.msg_name = (void *)&servaddr;
//        msgbuf[i].msg_hdr.msg_namelen = sizeof(struct sockaddr_in);
//    }
//    FILE* log = fopen(logfile, "w+");
//    if(NULL == log) {
//        perror("fopen()");
//        return -5;
//    }
//    printf("Entering run state ...\n");
//    printf("Writing to logfile: %s\n", logfile);
//    struct timeval tv;
//    gettimeofday(&tv, NULL);
//    double start = ((double)tv.tv_sec) + ((double)tv.tv_usec / 1000000.0);
//    printf("Start: +%f\n", start);
//    uint64_t tsc_total   = 0;
//     int64_t rt_total    = 0;
//    uint64_t bytes_total = 0;

//    double sval = 0.0;
//    uint64_t received_count = 0;
//    uint64_t duplicate_count = 0;
//    uint64_t seq_hval = 0;
//    uint64_t seq_base = 0;

//    while(true) {
//        int res = recvmmsg(fd, msgbuf, BP_MSG_NBUF, 0, NULL);
//        if(res == 0) {
//            continue;
//        }
//        if(res < 0) {
//            perror("recvmmsg");
//            return -1;
//        }

//        for(i = 0; i < res; ++i) {
//            char* mbuf = (char *)msgbuf[i].msg_hdr.msg_iov->iov_base;
//            ssize_t sz = msgbuf[i].msg_hdr.msg_iov->iov_len;
//            uint64_t curr_seq = 0;

//            int32_t offset = 0;
//            offset += bpv6_primary_block_decode(&primary, mbuf, offset, sz);
//            if(0 == offset) {
//                printf("Malformed bundle received - aborting.\n");
//                return -2;
//            }
//            bool is_payload = false;
//            while(!is_payload) {
//                int32_t tres = bpv6_canonical_block_decode(&payload, mbuf, offset, sz);
//                if(tres <= 0) {
//                    printf("Failed to parse extension block - aborting.\n");
//                    return -3;
//                }
//                offset += tres;
//                if(payload.type == BPV6_BLOCKTYPE_PAYLOAD) {
//                    is_payload = true;
//                }
//            }
//            bytes_total += payload.length;
//            bpgen_hdr* data = (bpgen_hdr*)(mbuf + offset);
//            // offset by the first sequence number we see, so that we don't need to restart for each run ...
//            if(seq_base == 0) {
//                seq_base = data->seq;
//                seq_hval = seq_base;
//            }
//            else if(data->seq > seq_hval) {
//                seq_hval = data->seq;
//                ++received_count;
//            }
//            else {
//                ++duplicate_count;
//            }
//            timespec tp;
//            clock_gettime(CLOCK_REALTIME, &tp);
//            int64_t one_way = 1000000 * (((int64_t)tp.tv_sec) - ((int64_t)data->abstime.tv_sec));
//            one_way += (((int64_t)tp.tv_nsec) - ((int64_t)data->abstime.tv_nsec)) / 1000;

//            rt_total += one_way;
//            tsc_total += rdtsc() - data->tsc;
//        }

//        gettimeofday(&tv, NULL);
//        double curr_time = ((double)tv.tv_sec) + ((double)tv.tv_usec / 1000000.0);
//        curr_time -= start;
//        if(duplicate_count + received_count >= batch) {
//            if(received_count == 0) {
//                printf("BUG: batch was entirely duplicates - this shouldn't actually be possible.\n");
//            }
//            else if(use_one_way) {
//                fprintf(log, "%0.6f, %lu, %lu, %lu, %lu, %lu, %lu, %0.4f%%, %0.4f, one_way\n",
//                        curr_time, seq_base, seq_hval, received_count, duplicate_count,
//                        bytes_total, rt_total,
//                        100 - (100 * (received_count / (double)(seq_hval - seq_base))),
//                        1000 * ((rt_total / 1000000.0) / received_count) );

//            }
//            else {
//                fprintf(log, "%0.6f, %lu, %lu, %lu, %lu, %lu, %lu, %0.4f%%, %0.4f, rtt\n",
//                        curr_time, seq_base, seq_hval, received_count, duplicate_count,
//                        bytes_total, tsc_total,
//                        100 - (100 * (received_count / (double)(seq_hval - seq_base))),
//                        1000 * ((tsc_total / (double)freq_base) / received_count) );

//            }
//            fflush(log);

//            duplicate_count = 0;
//            received_count = 0;
//            bytes_total = 0;
//            seq_hval = 0;
//            seq_base = 0;
//            rt_total = 0;
//            tsc_total = 0;
//        }
//    }

//    return 0;
//}

int RunBpsink() {
    //scope to ensure clean exit before return 0
    {
        uint16_t port = 4557;
        bool useTcp = false;
        std::cout << "starting BpSink.." << std::endl;
        hdtn::BpSinkAsync bpSink(port, useTcp);
        bpSink.Init(0);
        bpSink.Netstart();

        std::cout << "ingress up and running" << std::endl;
        while (RUN_BPSINK) {
            boost::this_thread::sleep(boost::posix_time::millisec(250));
        }

       std::cout << "Rx Count, Duplicate Count, Total bytes Rx\n";

        std::cout << bpSink.m_receivedCount << "," << bpSink.m_duplicateCount << "," << bpSink.m_totalBytesRx << "\n";


        std::cout<< "BpSinkAsyncMain.cpp: exiting cleanly..\n";
    }
    std::cout<< "BpSinkAsyncMain: exited cleanly\n";
    return 0;

}

