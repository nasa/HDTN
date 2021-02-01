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

// Prototypes
std::string GetEnv(const std::string& var);
int RunBpgen();
int RunIngress(uint64_t* ptrBundleCount, uint64_t* ptrBundleData);
int RunEgress(uint64_t* ptrBundleCount, uint64_t* ptrBundleData);
bool IntegratedTest1();
pid_t SpawnPythonServer(void);
int KillProcess(pid_t processId);

volatile bool RUN_BPGEN = true;
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
};

IntegratedTestsFixture::IntegratedTestsFixture() {
    //    std::cout << "Called IntegratedTests1Fixture::IntegratedTests1Fixture()"
    //    << std::endl;
}

IntegratedTestsFixture::~IntegratedTestsFixture() {
    //    std::cout << "Called
    //    IntegratedTests1Fixture::~IntegratedTests1Fixture()" << std::endl;
}

void IntegratedTestsFixture::SetUp() {
    //    std::cout << "IntegratedTests1Fixture::SetUp called\n";
}

void IntegratedTestsFixture::TearDown() {
    //    std::cout << "IntegratedTests1Fixture::TearDown called\n";
}

TEST_F(IntegratedTestsFixture, IntegratedTest1) {
    bool result = IntegratedTest1();
    EXPECT_EQ(true, result);
}

bool IntegratedTest1() {
    std::cout << "Running Integrated Tests. " << std::endl << std::flush;

    //    pid_t pidPythonServer = spawnPythonServer();
    //    if (pidPythonServer == -1) {
    //        fprintf(stderr, "failed to fork child process\n");
    //        return -1;
    //    }
    //    printf("Spawned python server with pid %d\n", pidPythonServer);
    //    fflush(stdout);
    //    sleep(1);

    uint64_t bundleCountIngress = 0;
    uint64_t bundleDataIngress = 0;
    std::thread threadIngress(RunIngress, &bundleCountIngress, &bundleDataIngress);
    //    sleep(1);
    uint64_t bundleCountEgress = 0;
    uint64_t bundleDataEgress = 0;
    std::thread threadEgress(RunEgress, &bundleCountEgress, &bundleDataEgress);
    sleep(1);
    std::thread threadBpgen(RunBpgen);
    sleep(3);
    RUN_BPGEN = false;
    threadBpgen.join();
    sleep(2);
    RUN_INGRESS = false;
    std::cout << "Before threadIngress.join(). " << std::endl << std::flush;
    threadIngress.join();
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
    if ((bundleCountEgress == bundleCountEgress) && (bundleDataIngress == bundleDataEgress)) {
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
    int bpMsgNbuf = 32;
    struct mmsghdr* msgbuf;
    uint64_t bundleCount = 0;
    uint64_t bundleData = 0;
    uint64_t rawData = 0;
    uint64_t rate = 50;
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
    close(fd);
    std::cout << "End runBpgen ... " << std::endl << std::flush;
    return 0;
}

int RunIngress(uint64_t* ptrBundleCount, uint64_t* ptrBundleData) {
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
        count = ingress.Update(0.5);  // Use timeout so call does not indefinitely
                                      // block.  Units are seconds
        if (count > 0) {
            ingress.Process(count);
        }
        lastTime = currTime;
    }
    std::cout << "End runIngress ... " << std::endl << std::flush;
    *ptrBundleCount = ingress.m_bundleCount;
    *ptrBundleData = ingress.m_bundleData;
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
        (*ptrBundleData) += bundleSize;
        (*ptBundleCount)++;
    }
    // Clean up resources
    zmqSock->close();
    delete zmqSock;
    delete zmqCtx;
    return 0;
}

pid_t SpawnPythonServer(void) {
    int outfd = open("1.txt", O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (!outfd) {
        perror("open");
        return EXIT_FAILURE;
    }

    // fork a child process, execute vlc, and return it's pid. Returns -1 if fork
    // failed.
    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        return -1;
    }
    // when you call fork(), it creates two copies of your program: a parent, and
    // a child. You can tell them apart by the return value from fork().  If
    // fork() returns 0, this is is the child process.  If fork() returns
    // non-zero, we are the parent and the return value is the PID of the child
    // process. */
    if (pid == 0) {
        // this is the child process.  now we can call one of the exec family of
        // functions to execute VLC.  When you call exec, it replaces the currently
        // running process (the child process) with whatever we pass to exec.  So
        // our child process will now be running VLC.  exec() will never return
        // except in an error case, since it is now running the VLC code and not our
        // code.
        std::string commandArg = GetEnv("HDTN_SOURCE_ROOT") + "/common/regsvr/main.py";
        std::cout << "Running python3 " << commandArg << std::endl << std::flush;
        execlp("python3", commandArg.c_str(), (char*)NULL);
        std::cerr << "ERROR Running python3 " << commandArg << std::endl << std::flush;
        perror("python3");
        abort();
    }
    else {
        // parent, return the child's PID
        return pid;
    }
}

int KillProcess(pid_t processId) {
    // kill will send the specified signal to the specified process. Send a TERM
    // signal to VLC, requesting that it terminate. If that doesn't work, we send
    // a KILL signal.  If that doesn't work, we give up.
    if (kill(processId, SIGTERM) < 0) {
        perror("kill with SIGTERM");
        if (kill(processId, SIGKILL) < 0) {
            perror("kill with SIGKILL");
        }
    }
    // This shows how we can get the exit status of our child process.  It will
    // wait for the the VLC process to exit, then grab it's return value
    int status = 0;
    waitpid(processId, &status, 0);
    return status;
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
