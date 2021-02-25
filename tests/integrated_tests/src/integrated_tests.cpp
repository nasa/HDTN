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
#include <EgressAsync.h>


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

//#define BP_MSG_NBUF   (32)
//#define BP_MSG_NBUF   (4)

#endif

struct bpgen_hdr {
    uint64_t seq;
    uint64_t tsc;
    timespec abstime;
};


// Prototypes
std::string GetEnv(const std::string& var);
int RunBpgen(uint64_t* ptrTotalBytesSent);
int RunBpsink(uint64_t* ptrTotalBytesReceived);
int RunIngress(uint64_t* ptrBundleCount, uint64_t* ptrBundleData);
int RunEgress(uint64_t* ptrBundleCount, uint64_t* ptrBundleData);
bool TestBpgenIngressEgressBpsink();


volatile bool RUN_BPGEN = true;
volatile bool RUN_BPSINK = true;
volatile bool RUN_INGRESS = true;
volatile bool RUN_EGRESS = true;
volatile bool RUN_STORAGE = true;

std::string ERROR_MESSAGE = "";

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
//    std::cout << "StopPythonServer started." << std::endl << std::flush;
    m_runningPythonServer = false;
    if (m_ptrChild) {
        m_ptrChild->terminate();
        m_ptrChild->wait();
        int result = m_ptrChild->exit_code();
//        std::cout << "StopPythonServer ended with result = " << result << std::endl << std::flush;
    }
//    std::cout << "StopPythonServer ended." << std::endl << std::flush;
}

void IntegratedTestsFixture::StartPythonServer() {
//    std::cout << "StartPythonServer started." << std::endl << std::flush;
    m_runningPythonServer = true;
    std::string commandArg = GetEnv("HDTN_SOURCE_ROOT") + "/common/regsvr/main.py";
//    std::cout << "Running python3 " << commandArg << std::endl << std::flush;
    m_ptrChild = new boost::process::child(boost::process::search_path("python3"),commandArg);
    while(m_ptrChild->running()) {
        while(m_runningPythonServer) {
            usleep(250000);  // 0.25 seconds
            //std::cout << "StartPythonServer is running. " << std::endl << std::flush;
        }
    }
//    std::cout << "StartPythonServer ended." << std::endl << std::flush;
}

IntegratedTestsFixture::IntegratedTestsFixture() {
    //    std::cout << "Called IntegratedTests1Fixture::IntegratedTests1Fixture()" << std::endl;
}

IntegratedTestsFixture::~IntegratedTestsFixture() {
    //    std::cout << "Called IntegratedTests1Fixture::~IntegratedTests1Fixture()" << std::endl;
}

void IntegratedTestsFixture::SetUp() {
//    std::cout << "IntegratedTests1Fixture::SetUp called\n";
    m_ptrThreadPython = new std::thread(&IntegratedTestsFixture::StartPythonServer,this);
}

void IntegratedTestsFixture::TearDown() {
//    std::cout << "IntegratedTests1Fixture::TearDown called\n";
    this->StopPythonServer();
}

TEST_F(IntegratedTestsFixture, IntegratedTest1) {
    bool result = TestBpgenIngressEgressBpsink();
    EXPECT_EQ(true, result) << ERROR_MESSAGE;
}

bool TestBpgenIngressEgressBpsink() {
    ERROR_MESSAGE = "";
//    std::cout << "Running Integrated Test 1. " << std::endl << std::flush;

    uint64_t totalBytesSent = 0;
    uint64_t bundleCountIngress = 0;
    uint64_t bundleDataIngress = 0;
    uint64_t bundleCountEgress = 0;
    uint64_t bundleDataEgress = 0;
    uint64_t totalBytesReceived = 0;

    sleep(1);
    std::thread threadBpsink(RunBpsink,&totalBytesReceived);

    sleep(1);
    std::thread threadEgress(RunEgress, &bundleCountEgress, &bundleDataEgress);

    sleep(1);
    std::thread threadIngress(RunIngress, &bundleCountIngress, &bundleDataIngress);

    sleep(1);
    std::thread threadBpgen(RunBpgen,&totalBytesSent);

    sleep(3);


    RUN_BPGEN = false;
    threadBpgen.join();
//    std::cout << "After threadBpgen.join(). " << std::endl << std::flush;


    RUN_INGRESS = false;
    threadIngress.join();
//    std::cout << "After threadIngress.join(). " << std::endl << std::flush;

    RUN_EGRESS = false;
    threadEgress.join();
//    std::cout << "After threadEgress.join(). " << std::endl << std::flush;

    RUN_BPSINK = false;
    threadBpsink.join();
//    std::cout << "After threadBpsink.join(). " << std::endl << std::flush;


//    std::cout << "End Integrated Test 3. " << std::endl << std::flush;
//    std::cout << "bundleCountIngress: " << bundleCountEgress << " , bundleDataIngress: " << bundleDataIngress
//              << std::endl
//              << std::flush;
//    std::cout << "bundleCountEgress: " << bundleCountEgress << " , bundleDataEgress: " << bundleDataEgress
//              << std::endl
//              << std::flush;

//    if ((bundleCountIngress == bundleCountEgress) && (bundleDataIngress == bundleDataEgress)) {
//        return true;
//    }
    if (totalBytesSent != bundleDataIngress) {
        ERROR_MESSAGE = "Bytes sent by BPGEN (" + std::to_string(totalBytesSent) + ") != bytes received by ingress "
                + std::to_string(bundleDataIngress) + ").";
        return false;
    }
    if (totalBytesSent != bundleDataEgress) {
        ERROR_MESSAGE = "Bytes sent by BPGEN (" + std::to_string(totalBytesSent) + ") != bytes received by egress "
                + std::to_string(bundleDataEgress) + ").";
        return false;
    }
//    if (totalBytesSent != totalBytesReceived) {
//        ERROR_MESSAGE = "Bytes sent by BPGEN (" + std::to_string(totalBytesSent) + ") != bytes received by BPSINK "
//                + std::to_string(totalBytesReceived) + ").";
//        return false;
//    }
    return true;
}




int RunBpgen(uint64_t* ptrTotalBytesSent) {
     *ptrTotalBytesSent = 0;
//    std::cout << "Start runBpgen ... " << std::endl << std::flush;
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
    uint64_t rate = 150;
    std::string target = "127.0.0.1";
    struct sockaddr_in servaddr;
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    int sourceNode = 1;
    int destNode = 1;
    int port = 4556;
    size_t genSz = 1500;
    ssize_t res;
//    printf("Generating bundles of size %d\n", (int)genSz);
//    if (rate) {
//        printf("Generating up to %d bundles / second.\n", (int)rate);
//    }
//  printf("Bundles will be destinated for %s:%d\n", target.c_str(), port);
//    fflush(stdout);
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
//    printf("Entering run state ...\n");
    struct timeval tv;
    gettimeofday(&tv, NULL);
    double start = ((double)tv.tv_sec) + ((double)tv.tv_usec / 1000000.0);
//    printf("Start: +%f\n", start);
    fflush(stdout);
    uint64_t tscTotal = 0;
    double sval = 0.0;
    if (rate) {
        sval = 1000000.0 / rate;  // sleep val in usec
        sval *= bpMsgNbuf;
//        printf("Sleeping for %f usec between bursts\n", sval);
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
            totalSize += msgbuf->msg_len * bpMsgNbuf;
            totalBundleCount += bundleCount;
//            std::cout << "In BPGEN, totalBundleCount: " << totalBundleCount << " , totalSize: " << totalSize
//                      << std::endl
//                      << std::flush;
        }
        if (rate) {
            usleep((uint64_t)sval);
        }
    }
    *ptrTotalBytesSent = totalSize;
//    std::cout << "In BPGEN, totalBundleCount: " << totalBundleCount << " , totalSize: " << totalSize
//              << std::endl
//              << std::flush;
    close(fd);
//    std::cout << "End runBpgen ... " << std::endl << std::flush;
    return 0;
}


int RunIngress(uint64_t* ptrBundleCount, uint64_t* ptrBundleData) {

    //scope to ensure clean exit before return 0
    {
//        std::cout << "Start runIngress ... " << std::endl << std::flush;
        int ingressPort = 4556;

        hdtn::BpIngress ingress;
        ingress.Init(BP_INGRESS_TYPE_UDP);

        // finish registration stuff -ingress will find out what egress services have registered
        hdtn::HdtnRegsvr regsvr;
        regsvr.Init(HDTN_REG_SERVER_PATH, "ingress", 10100, "PUSH");
        regsvr.Reg();
        if(hdtn::HdtnEntries_ptr res = regsvr.Query()) {
            const hdtn::HdtnEntryList_t & entryList = res->m_hdtnEntryList;
            for (hdtn::HdtnEntryList_t::const_iterator it = entryList.cbegin(); it != entryList.cend(); ++it) {
                const hdtn::HdtnEntry & entry = *it;
                std::cout << entry.address << ":" << entry.port << ":" << entry.mode << std::endl;
            }
        }
        else {
            std::cerr << "error: null registration query" << std::endl;
            //return 1;
        }
//        for (auto entry : res) {
//            std::cout << entry.address << ":" << entry.port << ":" << entry.mode << std::endl;
//        }

//        printf("Announcing presence of ingress engine ...\n");

        ingress.Netstart(ingressPort);

//        std::cout << "ingress up and running.  RUN_INGRESS = " << RUN_INGRESS << std::endl << std::flush;
        while (RUN_INGRESS) {
            boost::this_thread::sleep(boost::posix_time::millisec(250));
        }

//        std::cout << ">> End runIngress ... " << std::endl << std::flush;
        *ptrBundleCount = ingress.m_bundleCount;
        *ptrBundleData = ingress.m_bundleData;
    }
//    std::cout<< "RunIngress: exited cleanly\n";
    return 0;

}

int RunEgress(uint64_t* ptrBundleCount, uint64_t* ptrBundleData) {

    //scope to ensure clean exit before return 0
    {
//        std::cout << "Start runEgress ... " << std::endl << std::flush;
        uint16_t port = 4557;
        hdtn::HdtnRegsvr regsvr;
        regsvr.Init(HDTN_REG_SERVER_PATH, "egress", 10100, "PULL");
        regsvr.Reg();
        if(hdtn::HdtnEntries_ptr res = regsvr.Query()) {
            const hdtn::HdtnEntryList_t & entryList = res->m_hdtnEntryList;
            for (hdtn::HdtnEntryList_t::const_iterator it = entryList.cbegin(); it != entryList.cend(); ++it) {
                const hdtn::HdtnEntry & entry = *it;
                std::cout << entry.address << ":" << entry.port << ":" << entry.mode << std::endl;
            }
        }
        else {
            std::cerr << "error: null registration query" << std::endl;
            //return 1;
        }
//        for (auto entry : res) {
//            std::cout << entry.address << ":" << entry.port << ":" << entry.mode << std::endl;
//        }
        hdtn::HegrManagerAsync egress;
        egress.Init();
        int entryStatus;
        entryStatus = egress.Add(1, HEGR_FLAG_UDP, "127.0.0.1", port);
        if (!entryStatus) {
            return 0;  // error message prints in add function
        }
//        printf("Announcing presence of egress ...\n");
        for (int i = 0; i < 8; ++i) {
            egress.Up(i);
        }
//        std::cout << "egress up and running" << std::endl;
        while (RUN_EGRESS) {
            boost::this_thread::sleep(boost::posix_time::millisec(250));
        }
//        std::cout << "Msg Count, Bundle Count, Bundle data bytes\n";
//        std::cout << egress.m_messageCount << "," << egress.m_bundleCount << "," << egress.m_bundleData << "\n";
        *ptrBundleCount = egress.m_bundleCount;
        *ptrBundleData = egress.m_bundleData;
    }
//    std::cout<< "RunEgress: exited cleanly\n";
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

int RunBpsink(uint64_t* ptrTotalBytesReceived) {
    *ptrTotalBytesReceived = 0;
    //scope to ensure clean exit before return 0
    {
        uint16_t port = 4557;
        bool useTcp = false;
//        std::cout << "starting BpSink.." << std::endl;
        hdtn::BpSinkAsync bpSink(port, useTcp, "BpSink");
        bpSink.Init(0);
        bpSink.Netstart();
//        std::cout << "ingress up and running" << std::endl;
        while (RUN_BPSINK) {
            boost::this_thread::sleep(boost::posix_time::millisec(250));
        }
//        std::cout << "BPSINK\n";
//        std::cout << "Rx Count, Duplicate Count, Total bytes Rx\n";
//        std::cout << bpSink.m_receivedCount << "," << bpSink.m_duplicateCount << "," << bpSink.m_totalBytesRx << "\n";
        *ptrTotalBytesReceived = bpSink.m_totalBytesRx;
    }
//    std::cout<< "RunBpsink: exited cleanly\n";
    return 0;
}
