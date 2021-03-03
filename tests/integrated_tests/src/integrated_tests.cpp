#ifndef _WIN32
#include <arpa/inet.h>
#include <sys/time.h>
#include <unistd.h>
#include <util/tsc.h>
#include <fcntl.h>
#include <signal.h> /* for SIGTERM, SIGKILL */
#include <sys/types.h> /* for pid_t            */
#include <sys/wait.h>  /* for waitpid          */
#include <unistd.h>    /* for fork, exec, kill */
#include <egress.h>
#endif // !_WIN32


#include <codec/bpv6.h>


#include <ingress.h>


#include <fstream>
#include <iostream>
#include <reg.hpp>
#include <store.hpp>
#include <thread>
#include <zmq.hpp>

// Used for forking python process

#include <stdio.h>
#include <stdlib.h>


#include <boost/process.hpp>
#include <boost/thread.hpp>
#include <boost/assign/list_of.hpp>
#include <boost/lexical_cast.hpp>

#include <string>
#include <vector>

#include <BpGenAsync.h>
#include <BpSinkAsync.h>
#include <EgressAsync.h>

#include <boost/test/unit_test.hpp>
#include "Environment.h"



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
static void DurationEndedThreadFunction(const boost::system::error_code& e);
int RunBpgenAsync(bool useTcpcl, bool useStcp, uint32_t bundleRate, uint64_t* ptrBundleCount);
int RunIngress(bool useStcp, bool alwaysSendToStorage, uint64_t* ptrBundleCount, uint64_t* ptrBundleData);
int RunEgressAsync(bool useTcpcl, bool useStcp, uint64_t* ptrBundleCountEgress, uint64_t* ptrBundleDataEgress);
int RunBpsinkAsync(bool useTcpcl, bool useStcp, uint64_t* ptrTotalBundlesBpsink, uint64_t* ptrDuplicateBundlesBpsink,
                   uint64_t* ptrTotalBytesBpsink);
int RunReleaseMessageSender();
bool TestCutThroughTcpcl();
bool TestCutThroughUdp();
bool TestCutThroughStcp();
bool TestStorage();

volatile bool RUN_BPGEN = true;
volatile bool RUN_BPSINK = true;
volatile bool RUN_INGRESS = true;
volatile bool RUN_EGRESS = true;
volatile bool RUN_RELEASE_MESSAGE_SENDER = true;
volatile bool RUN_STORAGE = true;

std::string ERROR_MESSAGE = "";

// Create a test fixture.
class BoostIntegratedTestsFixture {
public:
    BoostIntegratedTestsFixture();
    ~BoostIntegratedTestsFixture();
    bool m_runningPythonServer;
private:
    void StartPythonServer();
    void StopPythonServer();
    boost::process::child * m_ptrChild;
    std::thread * m_ptrThreadPython;
};

BoostIntegratedTestsFixture::BoostIntegratedTestsFixture() {
    std::cout << "Called BoostIntegratedTestsFixture::BoostIntegratedTestsFixture()" << std::endl;
    m_ptrThreadPython = new std::thread(&BoostIntegratedTestsFixture::StartPythonServer,this);
}

BoostIntegratedTestsFixture::~BoostIntegratedTestsFixture() {
    std::cout << "Called BoostIntegratedTestsFixture::~BoostIntegratedTestsFixture()" << std::endl;
    this->StopPythonServer();
}

void BoostIntegratedTestsFixture::StopPythonServer() {
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

void BoostIntegratedTestsFixture::StartPythonServer() {
    std::cout << "StartPythonServer started." << std::endl << std::flush;
    m_runningPythonServer = true;
    const boost::filesystem::path commandArg = boost::filesystem::path(GetEnv("HDTN_SOURCE_ROOT")) / "common" / "regsvr" / "main.py";
#ifdef _WIN32
    const std::string pythonExe = "python";
#else
    const std::string pythonExe = "python3";
#endif
    std::cout << "Running python3 " << commandArg << std::endl << std::flush;
    m_ptrChild = new boost::process::child(boost::process::search_path(pythonExe),commandArg);
    while(m_ptrChild->running()) {
        while(m_runningPythonServer) {
            boost::this_thread::sleep(boost::posix_time::milliseconds(250));
            //std::cout << "StartPythonServer is running. " << std::endl << std::flush;
        }
    }
    std::cout << "StartPythonServer ended." << std::endl << std::flush;
}

static void DurationEndedThreadFunction(const boost::system::error_code& e) {
    if (e != boost::asio::error::operation_aborted) {
        // Timer was not cancelled, take necessary action.
        std::cout << "BpGen reached duration.. exiting\n";
    }
    else {
        std::cout << "Unknown error occurred in DurationEndedThreadFunction " << e.message() << std::endl;
    }
    RUN_BPGEN = false;
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

int RunBpgenAsync(bool useTcpcl, bool useStcp, uint32_t bundleRate, uint64_t* ptrBundleCount) {
    //scope to ensure clean exit before return 0
    {
        std::string destinationAddress = "localhost";
        std::string thisLocalEidString = "BpGen";
        uint16_t port = 4556;
        uint32_t bundleSizeBytes = 100;
        uint32_t tcpclFragmentSize = 0;
        uint32_t durationSeconds = 0;

        std::cout << "starting BpGenAsync.." << std::endl;

        BpGenAsync bpGen;
        bpGen.Start(destinationAddress, boost::lexical_cast<std::string>(port), useTcpcl, useStcp, bundleSizeBytes, bundleRate, tcpclFragmentSize, thisLocalEidString);

        boost::asio::io_service ioService;
        boost::asio::deadline_timer deadlineTimer(ioService, boost::posix_time::seconds(durationSeconds));
        if (durationSeconds) {
            deadlineTimer.async_wait(boost::bind(&DurationEndedThreadFunction, boost::asio::placeholders::error));
        }

        std::cout << "BpGenAsync up and running" << std::endl;
        while (RUN_BPGEN) {
            boost::this_thread::sleep(boost::posix_time::millisec(250));
        }
        bpGen.Stop();
        *ptrBundleCount = bpGen.m_bundleCount;
    }
    return 0;
}

int RunIngress(bool useStcp, bool alwaysSendToStorage, uint64_t* ptrBundleCount, uint64_t* ptrBundleData) {

    //scope to ensure clean exit before return 0
    {
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
                //std::cout << entry.address << ":" << entry.port << ":" << entry.mode << std::endl;
            }
        }
        else {
            std::cerr << "error: null registration query" << std::endl;
            //return 1;
        }
//        ingress.Netstart(ingressPort,true,false,false);
        ingress.Netstart(ingressPort, !useStcp, useStcp,alwaysSendToStorage);
        while (RUN_INGRESS) {
            boost::this_thread::sleep(boost::posix_time::millisec(250));
            ingress.RemoveInactiveTcpConnections();
        }
        *ptrBundleCount = ingress.m_bundleCount;
        *ptrBundleData = ingress.m_bundleData;
    }
    return 0;

}

int RunEgressAsync(bool useTcpcl, bool useStcp, uint64_t* ptrBundleCountEgress, uint64_t* ptrBundleDataEgress) {

    //scope to ensure clean exit before return 0
    {
        uint16_t port = 4557;
        hdtn::HdtnRegsvr regsvr;
//        regsvr.Init(HDTN_REG_SERVER_PATH, "egress", 10120, "PULL");
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
        hdtn::HegrManagerAsync egress;
        egress.Init();
        int entryStatus;
        entryStatus = egress.Add(1, (useTcpcl) ? HEGR_FLAG_TCPCLv3 : (useStcp) ? HEGR_FLAG_STCPv1 : HEGR_FLAG_UDP,
                                 "127.0.0.1", port);

        if (!entryStatus) {
            return 0;  // error message prints in add function
        }
        for (int i = 0; i < 8; ++i) {
            egress.Up(i);
        }
        while (RUN_EGRESS) {
            boost::this_thread::sleep(boost::posix_time::millisec(250));
        }
        *ptrBundleCountEgress = egress.m_bundleCount;
        *ptrBundleDataEgress = egress.m_bundleData;
    }
    return 0;
}

int RunBpsinkAsync(bool useTcpcl, bool useStcp, uint64_t* ptrTotalBundlesBpsink, uint64_t* ptrDuplicateBundlesBpsink,
                   uint64_t* ptrTotalBytesBpsink) {
    *ptrTotalBundlesBpsink = 0;
    //scope to ensure clean exit before return 0
    {
        uint16_t port = 4557;
        std::string thisLocalEidString = "BpSink";
        hdtn::BpSinkAsync bpSink(port, useTcpcl, useStcp, thisLocalEidString);
        bpSink.Init(0);
        bpSink.Netstart();
        while (RUN_BPSINK) {
            boost::this_thread::sleep(boost::posix_time::millisec(250));
        }
        *ptrTotalBundlesBpsink = bpSink.m_receivedCount;
        *ptrDuplicateBundlesBpsink = bpSink.m_duplicateCount;
        *ptrTotalBytesBpsink = bpSink.m_totalBytesRx;
    }
    return 0;
}

int RunReleaseMessageSender() {
    //scope to ensure clean exit before return 0
    {
        std::cout << "In RunReleaseMessageSender begin.\n";
        unsigned int flowId = 2;
        zmq::context_t ctx;
        zmq::socket_t socket(ctx, zmq::socket_type::pub);
        socket.bind(HDTN_BOUND_SCHEDULER_PUBSUB_PATH);

        while (RUN_RELEASE_MESSAGE_SENDER) {
            boost::this_thread::sleep(boost::posix_time::millisec(250));
        }
        std::cout << "In RunReleaseMessageSender after while loop\n";

        // Start
        hdtn::IreleaseStartHdr releaseMsg;
        memset(&releaseMsg, 0, sizeof(hdtn::IreleaseStartHdr));
        releaseMsg.base.type = HDTN_MSGTYPE_IRELSTART;
        releaseMsg.flowId = flowId;
        releaseMsg.rate = 0;  //not implemented
        releaseMsg.duration = 20;//not implemented
        socket.send(zmq::const_buffer(&releaseMsg, sizeof(hdtn::IreleaseStartHdr)), zmq::send_flags::none);
        std::cout << "Start Release message sent \n";
        boost::this_thread::sleep(boost::posix_time::seconds(1));

    }
    return 0;
}

int RunStorage() {
    //scope to ensure clean exit before return 0
    {
        std::string storePath = (Environment::GetPathHdtnSourceRoot()
                                 / "module/storage/storage-brian/unit_tests/storageConfigRelativePaths.json").string();
        hdtn::storageConfig config;
        config.regsvr = HDTN_REG_SERVER_PATH;
        config.local = HDTN_CONNECTING_STORAGE_TO_BOUND_EGRESS_PATH;
        config.releaseWorker = HDTN_BOUND_SCHEDULER_PUBSUB_PATH;
        config.storePath = storePath;
        hdtn::storage store;
        std::cout << "[store] Initializing storage manager ..." << std::endl;
        if (!store.init(config)) {
            return -1;
        }
        while (RUN_STORAGE) {
            store.update(); // also delays 250 milli
        }
    }
    return 0;
}

bool TestCutThroughTcpcl() {

    std::cout << "Running Integrated TestCutThroughTcpcl. " << std::endl << std::flush;

    RUN_BPGEN = true;
    RUN_BPSINK = true;
    RUN_INGRESS = true;
    RUN_EGRESS = true;
    std::string ERROR_MESSAGE = "";
    uint64_t bundlesSentBpgen = 0;
    uint64_t totalBundlesBpsink = 0;
    uint64_t duplicateBundlesBpsink = 0;
    uint64_t totalBytesBpsink = 0;
    uint64_t bundleDataEgress = 0;
    uint64_t bundleCountEgress = 0;
    uint64_t bundleCountIngress = 0;
    uint64_t bundleDataIngress = 0;
    bool alwaysSendToStorage = false;
    bool useTcpcl = true;
    bool useStcp = false;
    uint32_t bundleRate = 1500;

    boost::this_thread::sleep(boost::posix_time::seconds(1));
    std::thread threadBpsink(RunBpsinkAsync,useTcpcl,useStcp,&totalBundlesBpsink,&duplicateBundlesBpsink,&totalBytesBpsink);

    boost::this_thread::sleep(boost::posix_time::seconds(3));
    std::thread threadEgress(RunEgressAsync,useTcpcl,useStcp,&bundleCountEgress, &bundleDataEgress);

    boost::this_thread::sleep(boost::posix_time::seconds(3));
    std::thread threadIngress(RunIngress,useStcp,alwaysSendToStorage,&bundleCountIngress, &bundleDataIngress);

    boost::this_thread::sleep(boost::posix_time::seconds(3));
    std::thread threadBpgen(RunBpgenAsync,useTcpcl,useStcp,bundleRate,&bundlesSentBpgen);

    boost::this_thread::sleep(boost::posix_time::seconds(10));

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

    std::cout << "bundlesSentBpgen: " << bundlesSentBpgen << std::endl << std::flush;
    std::cout << "bundleCountIngress: " << bundleCountIngress << std::endl << std::flush;
    std::cout << "bundleCountEgress: " << bundleCountEgress << std::endl << std::flush;
    std::cout << "totalBundlesBpsink: " << totalBundlesBpsink << std::endl << std::flush;


    if (bundlesSentBpgen != bundleCountIngress) {
        ERROR_MESSAGE = "Bundles sent by BPGEN (" + std::to_string(bundlesSentBpgen) + ") !=  bundles received by ingress "
                + std::to_string(bundleCountIngress) + ").";
        return false;
    }
    if (bundlesSentBpgen != bundleCountEgress) {
        ERROR_MESSAGE = "Bundles sent by BPGEN (" + std::to_string(bundlesSentBpgen) + ") != bundles received by egress "
                + std::to_string(bundleCountEgress) + ").";
        return false;
    }
    if (bundlesSentBpgen != totalBundlesBpsink) {
        ERROR_MESSAGE = "Bundles sent by BPGEN (" + std::to_string(bundlesSentBpgen) + ") != bundles received by BPSINK "
                + std::to_string(totalBundlesBpsink) + ").";
        return false;
    }
    return true;
}

bool TestCutThroughUdp() {

    std::cout << "Running Integrated TestCutThroughUdp. " << std::endl << std::flush;

    RUN_BPGEN = true;
    RUN_BPSINK = true;
    RUN_INGRESS = true;
    RUN_EGRESS = true;
    std::string ERROR_MESSAGE = "";
    uint64_t bundlesSentBpgen = 0;
    uint64_t totalBundlesBpsink = 0;
    uint64_t duplicateBundlesBpsink = 0;
    uint64_t totalBytesBpsink = 0;
    uint64_t bundleDataEgress = 0;
    uint64_t bundleCountEgress = 0;
    uint64_t bundleCountIngress = 0;
    uint64_t bundleDataIngress = 0;
    bool alwaysSendToStorage = false;
    bool useTcpcl = false;
    bool useStcp = false;
    uint32_t bundleRate = 1500;

    boost::this_thread::sleep(boost::posix_time::seconds(1));
    std::thread threadBpsink(RunBpsinkAsync,useTcpcl,useStcp,&totalBundlesBpsink,&duplicateBundlesBpsink,&totalBytesBpsink);

    boost::this_thread::sleep(boost::posix_time::seconds(3));
    std::thread threadEgress(RunEgressAsync,useTcpcl,useStcp,&bundleCountEgress, &bundleDataEgress);

    boost::this_thread::sleep(boost::posix_time::seconds(3));
    std::thread threadIngress(RunIngress,useStcp,alwaysSendToStorage,&bundleCountIngress, &bundleDataIngress);

    boost::this_thread::sleep(boost::posix_time::seconds(3));
    std::thread threadBpgen(RunBpgenAsync,useTcpcl,useStcp,bundleRate,&bundlesSentBpgen);

    boost::this_thread::sleep(boost::posix_time::seconds(10));

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

    std::cout << "bundlesSentBpgen: " << bundlesSentBpgen << std::endl << std::flush;
    std::cout << "bundleCountIngress: " << bundleCountIngress << std::endl << std::flush;
    std::cout << "bundleCountEgress: " << bundleCountEgress << std::endl << std::flush;
    std::cout << "totalBundlesBpsink: " << totalBundlesBpsink << std::endl << std::flush;


    if (bundlesSentBpgen != bundleCountIngress) {
        ERROR_MESSAGE = "Bundles sent by BPGEN (" + std::to_string(bundlesSentBpgen) + ") !=  bundles received by ingress "
                + std::to_string(bundleCountIngress) + ").";
        return false;
    }
    if (bundlesSentBpgen != bundleCountEgress) {
        ERROR_MESSAGE = "Bundles sent by BPGEN (" + std::to_string(bundlesSentBpgen) + ") != bundles received by egress "
                + std::to_string(bundleCountEgress) + ").";
        return false;
    }
    if (bundlesSentBpgen != totalBundlesBpsink) {
        ERROR_MESSAGE = "Bundles sent by BPGEN (" + std::to_string(bundlesSentBpgen) + ") != bundles received by BPSINK "
                + std::to_string(totalBundlesBpsink) + ").";
        return false;
    }
    return true;
}

bool TestCutThroughStcp() {

    std::cout << "Running Integrated TestCutThroughStcp. " << std::endl << std::flush;

    RUN_BPGEN = true;
    RUN_BPSINK = true;
    RUN_INGRESS = true;
    RUN_EGRESS = true;
    std::string ERROR_MESSAGE = "";
    uint64_t bundlesSentBpgen = 0;
    uint64_t totalBundlesBpsink = 0;
    uint64_t duplicateBundlesBpsink = 0;
    uint64_t totalBytesBpsink = 0;
    uint64_t bundleDataEgress = 0;
    uint64_t bundleCountEgress = 0;
    uint64_t bundleCountIngress = 0;
    uint64_t bundleDataIngress = 0;
    bool alwaysSendToStorage = false;
    bool useTcpcl = false;
    bool useStcp = true;
    uint32_t bundleRate = 1500;

    boost::this_thread::sleep(boost::posix_time::seconds(1));
    std::thread threadBpsink(RunBpsinkAsync,useTcpcl,useStcp,&totalBundlesBpsink,&duplicateBundlesBpsink,&totalBytesBpsink);

    boost::this_thread::sleep(boost::posix_time::seconds(3));
    std::thread threadEgress(RunEgressAsync,useTcpcl,useStcp,&bundleCountEgress, &bundleDataEgress);

    boost::this_thread::sleep(boost::posix_time::seconds(3));
    std::thread threadIngress(RunIngress,useStcp,alwaysSendToStorage,&bundleCountIngress, &bundleDataIngress);

    boost::this_thread::sleep(boost::posix_time::seconds(3));
    std::thread threadBpgen(RunBpgenAsync,useTcpcl,useStcp,bundleRate,&bundlesSentBpgen);

    boost::this_thread::sleep(boost::posix_time::seconds(10));

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

    std::cout << "bundlesSentBpgen: " << bundlesSentBpgen << std::endl << std::flush;
    std::cout << "bundleCountIngress: " << bundleCountIngress << std::endl << std::flush;
    std::cout << "bundleCountEgress: " << bundleCountEgress << std::endl << std::flush;
    std::cout << "totalBundlesBpsink: " << totalBundlesBpsink << std::endl << std::flush;


    if (bundlesSentBpgen != bundleCountIngress) {
        ERROR_MESSAGE = "Bundles sent by BPGEN (" + std::to_string(bundlesSentBpgen) + ") !=  bundles received by ingress "
                + std::to_string(bundleCountIngress) + ").";
        return false;
    }
    if (bundlesSentBpgen != bundleCountEgress) {
        ERROR_MESSAGE = "Bundles sent by BPGEN (" + std::to_string(bundlesSentBpgen) + ") != bundles received by egress "
                + std::to_string(bundleCountEgress) + ").";
        return false;
    }
    if (bundlesSentBpgen != totalBundlesBpsink) {
        ERROR_MESSAGE = "Bundles sent by BPGEN (" + std::to_string(bundlesSentBpgen) + ") != bundles received by BPSINK "
                + std::to_string(totalBundlesBpsink) + ").";
        return false;
    }
    return true;
}

bool TestStorage() {

    std::cout << "Running Integrated TestStorage. " << std::endl << std::flush;

    RUN_BPGEN = true;
    RUN_BPSINK = true;
    RUN_EGRESS = true;
    RUN_INGRESS = true;
    RUN_RELEASE_MESSAGE_SENDER = true;
    RUN_STORAGE = true;

    std::string ERROR_MESSAGE = "";
    uint64_t bundlesSentBpgen = 0;
    uint64_t totalBundlesBpsink = 0;
    uint64_t duplicateBundlesBpsink = 0;
    uint64_t totalBytesBpsink = 0;
    uint64_t bundleDataEgress = 0;
    uint64_t bundleCountEgress = 0;
    uint64_t bundleCountIngress = 0;
    uint64_t bundleDataIngress = 0;
    bool alwaysSendToStorage = true;
    bool useTcpcl = true;
    bool useStcp = false;
    uint32_t bundleRate = 300;

    boost::this_thread::sleep(boost::posix_time::seconds(3));
    std::thread threadBpsink(RunBpsinkAsync,useTcpcl,useStcp,&totalBundlesBpsink,&duplicateBundlesBpsink,&totalBytesBpsink);

    boost::this_thread::sleep(boost::posix_time::seconds(3));
    std::thread threadEgress(RunEgressAsync,useTcpcl,useStcp,&bundleCountEgress, &bundleDataEgress);

    boost::this_thread::sleep(boost::posix_time::seconds(3));
    std::thread threadIngress(RunIngress,useStcp,alwaysSendToStorage,&bundleCountIngress, &bundleDataIngress);

    boost::this_thread::sleep(boost::posix_time::seconds(3));
    std::thread threadRunReleaseMessageSender(RunReleaseMessageSender);

    boost::this_thread::sleep(boost::posix_time::seconds(3));
    std::thread threadRunStorage(RunStorage);

    boost::this_thread::sleep(boost::posix_time::seconds(3));
    std::thread threadBpgen(RunBpgenAsync,useTcpcl,useStcp,bundleRate,&bundlesSentBpgen);

    boost::this_thread::sleep(boost::posix_time::seconds(10));

    RUN_BPGEN = false;
    threadBpgen.join();
    boost::this_thread::sleep(boost::posix_time::seconds(3));
    RUN_RELEASE_MESSAGE_SENDER = false;
    threadRunReleaseMessageSender.join();
    boost::this_thread::sleep(boost::posix_time::seconds(10));
    RUN_STORAGE = false;
    threadRunStorage.join();
    RUN_INGRESS = false;
    threadIngress.join();
    RUN_EGRESS = false;
    threadEgress.join();
    RUN_BPSINK = false;
    threadBpsink.join();

    std::cout << "bundlesSentBpgen: " << bundlesSentBpgen << std::endl << std::flush;
    std::cout << "bundleCountIngress: " << bundleCountIngress << std::endl << std::flush;
    std::cout << "bundleCountEgress: " << bundleCountEgress << std::endl << std::flush;
    std::cout << "totalBundlesBpsink: " << totalBundlesBpsink << std::endl << std::flush;

    if (bundlesSentBpgen != bundleCountIngress) {
        ERROR_MESSAGE = "Bundles sent by BPGEN (" + std::to_string(bundlesSentBpgen) + ") !=  bundles received by ingress "
                + std::to_string(bundleCountIngress) + ").";
        std::cerr << ERROR_MESSAGE << std::endl << std::flush;
        return false;
    }
    if (bundlesSentBpgen != bundleCountEgress) {
        ERROR_MESSAGE = "Bundles sent by BPGEN (" + std::to_string(bundlesSentBpgen) + ") != bundles received by egress "
                + std::to_string(bundleCountEgress) + ").";
        std::cerr << ERROR_MESSAGE << std::endl << std::flush;
        return false;
    }
    if (bundlesSentBpgen != totalBundlesBpsink) {
        ERROR_MESSAGE = "Bundles sent by BPGEN (" + std::to_string(bundlesSentBpgen) + ") != bundles received by BPSINK "
                + std::to_string(totalBundlesBpsink) + ").";
        std::cerr << ERROR_MESSAGE << std::endl << std::flush;
        return false;
    }
    return true;
}




BOOST_GLOBAL_FIXTURE(BoostIntegratedTestsFixture);

//BOOST_AUTO_TEST_CASE(it_TestCutThroughTcpcl) {
//    bool result = TestCutThroughTcpcl();
//    BOOST_CHECK(result == true);
//}

//BOOST_AUTO_TEST_CASE(it_TestCutThroughUdp) {
//    bool result = TestCutThroughUdp();
//    BOOST_CHECK(result == true);
//}

//BOOST_AUTO_TEST_CASE(it_TestCutThroughStcp) {
//    bool result = TestCutThroughStcp();
//    BOOST_CHECK(result == true);
//}

BOOST_AUTO_TEST_CASE(it_TestStorage) {
    bool result = TestStorage();
    BOOST_CHECK(result == true);
}


