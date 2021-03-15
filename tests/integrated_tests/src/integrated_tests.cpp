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
#include <boost/test/results_reporter.hpp>
#include <boost/test/unit_test_parameters.hpp>
#include "Environment.h"
#include "BpGenAsyncRunner.h"
#include "BpSinkAsyncRunner.h"
#include "IngressAsyncRunner.h"
#include "EgressAsyncRunner.h"




enum Protocol { Tcpcl, Stcp, Udp};

// Prototypes
static void DurationEndedThreadFunction(const boost::system::error_code& e);
int RunBpgenAsync(bool useTcpcl, bool useStcp, uint32_t bundleRate, uint64_t* ptrBundleCount);
int RunIngress(bool useStcp, bool alwaysSendToStorage, uint64_t* ptrBundleCount, uint64_t* ptrBundleData);
int RunEgressAsync(bool useTcpcl, bool useStcp, uint64_t* ptrBundleCountEgress, uint64_t* ptrBundleDataEgress);
int RunBpsinkAsync(bool useTcpcl, bool useStcp, uint64_t* ptrTotalBundlesBpsink, uint64_t* ptrDuplicateBundlesBpsink,
                   uint64_t* ptrTotalBytesBpsink);
int RunReleaseMessageSender();
bool TestStorage();
bool TestCutThrough(Protocol protocol);

int RunBpgenAsync2(const char * argv[], int argc, bool & running, uint64_t* ptrBundleCount);
int RunEgressAsync2(const char * argv[], int argc, bool & running, uint64_t* ptrBundleCount);
int RunBpsinkAsync2(const char * argv[], int argc, bool & running, uint64_t* ptrBundleCount);
int RunIngress2(const char * argv[], int argc, bool & running, uint64_t* ptrBundleCount);
bool TestCutThrough2();

// Global flags used to control thread execution
volatile bool RUN_BPGEN = true;
volatile bool RUN_BPSINK = true;
volatile bool RUN_INGRESS = true;
volatile bool RUN_EGRESS = true;
volatile bool RUN_RELEASE_MESSAGE_SENDER = true;
volatile bool RUN_STORAGE = true;

// Global Test Fixture.  Used to setup Python Registration server.
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
    boost::unit_test::results_reporter::set_level(boost::unit_test::report_level::DETAILED_REPORT);
    boost::unit_test::unit_test_log.set_threshold_level( boost::unit_test::log_messages );
    m_ptrThreadPython = new std::thread(&BoostIntegratedTestsFixture::StartPythonServer,this);
}

BoostIntegratedTestsFixture::~BoostIntegratedTestsFixture() {
    this->StopPythonServer();
}

void BoostIntegratedTestsFixture::StopPythonServer() {
    m_runningPythonServer = false;
    if (m_ptrChild) {
        m_ptrChild->terminate();
        m_ptrChild->wait();
        int result = m_ptrChild->exit_code();
    }
}

void BoostIntegratedTestsFixture::StartPythonServer() {
    m_runningPythonServer = true;
    const boost::filesystem::path commandArg = Environment::GetPathHdtnSourceRoot() / "common" / "regsvr" / "main.py";
#ifdef _WIN32
    const std::string pythonExe = "python";
#else
    const std::string pythonExe = "python3";
#endif
    m_ptrChild = new boost::process::child(boost::process::search_path(pythonExe),commandArg);
    while(m_ptrChild->running()) {
        while(m_runningPythonServer) {
            boost::this_thread::sleep(boost::posix_time::milliseconds(250));
            //std::cout << "StartPythonServer is running. " << std::endl << std::flush;
        }
    }
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

int RunBpgenAsync2(const char * argv[], int argc, bool & running, uint64_t* ptrBundleCount) {
    std::cout << ">>>>>> Entering RunBpgenAsync2 ... " << std::endl << std::flush;
    {
        BpGenAsyncRunner runner;
        runner.Run(argc, argv, running, false);
        *ptrBundleCount = runner.m_bundleCount;
    }
    std::cout << ">>>>>> Leaving RunBpgenAsync2.  *ptrBundleCount = " << *ptrBundleCount << std::endl << std::flush;
    return 0;
}

int RunEgressAsync2(const char * argv[], int argc, bool & running, uint64_t* ptrBundleCount) {
    std::cout << ">>>>>> Entering RunEgressAsync2 ... " << std::endl << std::flush;
    {
        EgressAsyncRunner runner;
        runner.Run(argc, argv, running, false);
        *ptrBundleCount = runner.m_bundleCount;
    }
    std::cout << ">>>>>> Leaving RunEgressAsync2.  *ptrBundleCount = " << *ptrBundleCount << std::endl << std::flush;
    return 0;
}

int RunBpsinkAsync2(const char * argv[], int argc, bool & running, uint64_t* ptrBundleCount) {
    std::cout << ">>>>>> Entering RunBpsinkAsync2 ... " << std::endl << std::flush;
    {
        BpSinkAsyncRunner runner;
        runner.Run(argc, argv, running, false);
        *ptrBundleCount = runner.m_receivedCount;
    }
    std::cout << ">>>>>> Leaving RunBpsinkAsync2.  *ptrBundleCount = " << *ptrBundleCount << std::endl << std::flush;
    return 0;
}

int RunIngress2(const char * argv[], int argc, bool & running, uint64_t* ptrBundleCount) {
    std::cout << ">>>>>> Entering RunIngress2 ... " << std::endl << std::flush;
    {
        IngressAsyncRunner runner;
        runner.Run(argc, argv, running, false);
        *ptrBundleCount = runner.m_bundleCount;
    }
    std::cout << ">>>>>> Leaving RunIngress2.  *ptrBundleCount = " << *ptrBundleCount << std::endl << std::flush;
    return 0;
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
        BpGenAsync bpGen;
        bpGen.Start(destinationAddress, boost::lexical_cast<std::string>(port), useTcpcl, useStcp, bundleSizeBytes,
                    bundleRate, tcpclFragmentSize, thisLocalEidString);
        boost::asio::io_service ioService;
        boost::asio::deadline_timer deadlineTimer(ioService, boost::posix_time::seconds(durationSeconds));
        if (durationSeconds) {
            deadlineTimer.async_wait(boost::bind(&DurationEndedThreadFunction, boost::asio::placeholders::error));
        }
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
        regsvr.Init(HDTN_REG_SERVER_PATH, "egress", 10100, "PULL");
        regsvr.Reg();
        if(hdtn::HdtnEntries_ptr res = regsvr.Query()) {
            const hdtn::HdtnEntryList_t & entryList = res->m_hdtnEntryList;
            for (hdtn::HdtnEntryList_t::const_iterator it = entryList.cbegin(); it != entryList.cend(); ++it) {
                const hdtn::HdtnEntry & entry = *it;
//                std::cout << entry.address << ":" << entry.port << ":" << entry.mode << std::endl;
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
        unsigned int flowId = 2;
        zmq::context_t ctx;
        zmq::socket_t socket(ctx, zmq::socket_type::pub);
        socket.bind(HDTN_BOUND_SCHEDULER_PUBSUB_PATH);
        while (RUN_RELEASE_MESSAGE_SENDER) {
            boost::this_thread::sleep(boost::posix_time::millisec(250));
        }
        // Start
        hdtn::IreleaseStartHdr releaseMsg;
        memset(&releaseMsg, 0, sizeof(hdtn::IreleaseStartHdr));
        releaseMsg.base.type = HDTN_MSGTYPE_IRELSTART;
        releaseMsg.flowId = flowId;
        releaseMsg.rate = 0;  //not implemented
        releaseMsg.duration = 20;//not implemented
        socket.send(zmq::const_buffer(&releaseMsg, sizeof(hdtn::IreleaseStartHdr)), zmq::send_flags::none);
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
        if (!store.init(config)) {
            return -1;
        }
        while (RUN_STORAGE) {
            store.update(); // also delays 250 milliseconds
        }
    }
    return 0;
}

bool TestCutThrough2() {
    bool runningBpgen = true;
    bool runningBpsink = true;
    bool runningIngress = true;
    bool runningEgress = true;

    uint64_t bundlesSentBpgen = 0;
    uint64_t totalBundlesBpsink = 0;
    uint64_t bundleCountEgress = 0;
    uint64_t bundleCountIngress = 0;

    // Start threads
    boost::this_thread::sleep(boost::posix_time::seconds(3));
    std::thread threadBpsink(RunBpsinkAsync2,
                             (const char * []){ "bpsink", "--use-tcpcl", "--port=4558", NULL }, 3,
                             std::ref(runningBpsink),&totalBundlesBpsink);
    boost::this_thread::sleep(boost::posix_time::seconds(3));
    std::thread threadEgress(RunEgressAsync2,
                             (const char * []){ "egress", "--use-tcpcl", "--port1=0", "--port2=4558", NULL }, 4,
                             std::ref(runningEgress),&bundleCountEgress);
    boost::this_thread::sleep(boost::posix_time::seconds(3));
    std::thread threadIngress(RunIngress2,
                              (const char * []){ "ingress", NULL }, 1,
                              std::ref(runningIngress),&bundleCountIngress);
    boost::this_thread::sleep(boost::posix_time::seconds(3));
    std::thread threadBpgen(RunBpgenAsync2,
                             (const char * []){ "bpgen", "--bundle-rate=100", "--use-tcpcl", "--flow-id=2", NULL },4,
                             std::ref(runningBpgen),&bundlesSentBpgen);
    // Allow time for data to flow
    boost::this_thread::sleep(boost::posix_time::seconds(10));
    // Stop threads
    runningBpgen = false;
    threadBpgen.join();
    runningIngress = false;
    threadIngress.join();
    runningEgress = false;
    threadEgress.join();
    runningBpsink = false;
    threadBpsink.join();
    // Verify results
    if (bundlesSentBpgen != bundleCountIngress) {
        BOOST_ERROR("Bundles sent by BPGEN (" + std::to_string(bundlesSentBpgen) + ") !=  bundles received by ingress "
                + std::to_string(bundleCountIngress) + ").");
        return false;
    }
    if (bundlesSentBpgen != bundleCountEgress) {
        BOOST_ERROR("Bundles sent by BPGEN (" + std::to_string(bundlesSentBpgen) + ") != bundles received by egress "
                + std::to_string(bundleCountEgress) + ").");
        return false;
    }
    if (bundlesSentBpgen != totalBundlesBpsink) {
        BOOST_ERROR("Bundles sent by BPGEN (" + std::to_string(bundlesSentBpgen) + ") != bundles received by BPSINK "
                + std::to_string(totalBundlesBpsink) + ").");
        return false;
    }
    return true;
}


bool TestCutThrough3() {
    bool runningBpgen = true;
    bool runningBpsink = true;
    bool runningIngress = true;
    bool runningEgress = true;

    uint64_t bundlesSentBpgen = 0;
    uint64_t totalBundlesBpsink = 0;
    uint64_t bundleCountEgress = 0;
    uint64_t bundleCountIngress = 0;

    // Start threads
    boost::this_thread::sleep(boost::posix_time::seconds(3));
    std::thread threadBpsink(RunBpsinkAsync2,
                             (const char * []){ "bpsink", "--use-tcpcl", "--port=4558", NULL }, 3,
                             std::ref(runningBpsink),&totalBundlesBpsink);
    boost::this_thread::sleep(boost::posix_time::seconds(3));
    std::thread threadEgress(RunEgressAsync2,
                             (const char * []){ "egress", "--use-tcpcl", "--port1=0", "--port2=4558", NULL }, 4,
                             std::ref(runningEgress),&bundleCountEgress);
    boost::this_thread::sleep(boost::posix_time::seconds(3));
    std::thread threadIngress(RunIngress2,
                              (const char * []){ "ingress", NULL }, 1,
                              std::ref(runningIngress),&bundleCountIngress);
    boost::this_thread::sleep(boost::posix_time::seconds(3));
    std::thread threadBpgen(RunBpgenAsync2,
                             (const char * []){ "bpgen", "--bundle-rate=0", "--use-tcpcl", "--flow-id=2",
                                                "--duration=10", NULL },5,
                             std::ref(runningBpgen),&bundlesSentBpgen);
    // Allow time for data to flow
    //boost::this_thread::sleep(boost::posix_time::seconds(10));  // Probably not needed due to duration parameter
    // Stop threads
    //runningBpgen = false; // Do not set this for multi case due to the duration parameter.
    threadBpgen.join();
    runningIngress = false;
    threadIngress.join();
    runningEgress = false;
    threadEgress.join();
    runningBpsink = false;
    threadBpsink.join();
    // Verify results
    if (bundlesSentBpgen != bundleCountIngress) {
        BOOST_ERROR("Bundles sent by BPGEN (" + std::to_string(bundlesSentBpgen) + ") !=  bundles received by ingress "
                + std::to_string(bundleCountIngress) + ").");
        return false;
    }
    if (bundlesSentBpgen != bundleCountEgress) {
        BOOST_ERROR("Bundles sent by BPGEN (" + std::to_string(bundlesSentBpgen) + ") != bundles received by egress "
                + std::to_string(bundleCountEgress) + ").");
        return false;
    }
    if (bundlesSentBpgen != totalBundlesBpsink) {
        BOOST_ERROR("Bundles sent by BPGEN (" + std::to_string(bundlesSentBpgen) + ") != bundles received by BPSINK "
                + std::to_string(totalBundlesBpsink) + ").");
        return false;
    }
    return true;
}



bool TestCutThrough4() {
    bool runningBpgen[2] = {true,true};
    bool runningBpsink[2] = {true,true};
    bool runningIngress = true;
    bool runningEgress = true;

    uint64_t bundlesSentBpgen[2] = {0,0};
    uint64_t bundlesReceivedBpsink[2] = {0,0};
    uint64_t bundleCountEgress = 0;
    uint64_t bundleCountIngress = 0;

    // Start threads
    boost::this_thread::sleep(boost::posix_time::seconds(3));
    std::thread threadBpsink0(RunBpsinkAsync2,
                             (const char * []){ "bpsink0", "--use-tcpcl", "--port=4557", NULL }, 3,
                             std::ref(runningBpsink[0]),&bundlesReceivedBpsink[0]);
    boost::this_thread::sleep(boost::posix_time::seconds(3));
    std::thread threadBpsink1(RunBpsinkAsync2,
                             (const char * []){ "bpsink1", "--use-tcpcl", "--port=4558", NULL }, 3,
                             std::ref(runningBpsink[1]),&bundlesReceivedBpsink[1]);
    boost::this_thread::sleep(boost::posix_time::seconds(3));
    std::thread threadEgress(RunEgressAsync2,
                             (const char * []){ "egress", "--use-tcpcl", "--port1=4557", "--port2=4558", NULL }, 4,
                             std::ref(runningEgress),&bundleCountEgress);
    boost::this_thread::sleep(boost::posix_time::seconds(3));
    std::thread threadIngress(RunIngress2,
                              (const char * []){ "ingress", NULL }, 1,
                              std::ref(runningIngress),&bundleCountIngress);
    boost::this_thread::sleep(boost::posix_time::seconds(3));
    std::thread threadBpgen0(RunBpgenAsync2,
                             (const char * []){ "bpgen0", "--bundle-rate=0", "--use-tcpcl", "--flow-id=2",
                                                "--duration=10" , NULL },5,
                             std::ref(runningBpgen[0]),&bundlesSentBpgen[0]);
    boost::this_thread::sleep(boost::posix_time::seconds(1));
    std::thread threadBpgen1(RunBpgenAsync2,
                             (const char * []){ "bpgen1", "--bundle-rate=0", "--use-tcpcl", "--flow-id=1",
                                                "--duration=10" ,NULL },5,
                             std::ref(runningBpgen[1]),&bundlesSentBpgen[1]);
    // Allow time for data to flow
//    boost::this_thread::sleep(boost::posix_time::seconds(10)); // Probably not needed due to duration parameter
    // Stop threads
//    runningBpgen[1] = false; // Do not set this for multi case due to the duration parameter.
    threadBpgen1.join();
//    runningBpgen[0] = false; // Do not set this for multi case due to the duration parameter.
    threadBpgen0.join();
    runningIngress = false;
    threadIngress.join();
    runningEgress = false;
    threadEgress.join();
    runningBpsink[1] = false;
    threadBpsink1.join();
    runningBpsink[0] = false;
    threadBpsink0.join();
    // Verify results
    uint64_t totalBundlesBpgen = 0;
    for(int i=0; i<2; i++) {
        totalBundlesBpgen += bundlesSentBpgen[i];
    }
    uint64_t totalBundlesBpsink = 0;
    for(int i=0; i<2; i++) {
        totalBundlesBpsink += bundlesReceivedBpsink[i];
    }
    if (totalBundlesBpgen != bundleCountIngress) {
        BOOST_ERROR("Bundles sent by BPGEN (" + std::to_string(totalBundlesBpgen) + ") !=  bundles received by ingress "
                + std::to_string(bundleCountIngress) + ").");
        return false;
    }
    if (totalBundlesBpgen != bundleCountEgress) {
        BOOST_ERROR("Bundles sent by BPGEN (" + std::to_string(totalBundlesBpgen) + ") != bundles received by egress "
                + std::to_string(bundleCountEgress) + ").");
        return false;
    }
    if (totalBundlesBpgen != totalBundlesBpsink) {
        BOOST_ERROR("Bundles sent by BPGEN (" + std::to_string(totalBundlesBpgen) + ") != bundles received by BPSINK "
                + std::to_string(totalBundlesBpsink) + ").");
        return false;
    }
    return true;
}


bool TestUdp1() {
    bool runningBpgen[1] = {true};
    bool runningBpsink[1] = {true};
    bool runningIngress = true;
    bool runningEgress = true;

    uint64_t bundlesSentBpgen[1] = {0};
    uint64_t bundlesReceivedBpsink[1] = {0};
    uint64_t bundleCountEgress = 0;
    uint64_t bundleCountIngress = 0;

    // Start threads
    boost::this_thread::sleep(boost::posix_time::seconds(3));
    std::thread threadBpsink0(RunBpsinkAsync2,
                             (const char * []){ "bpsink", "--port=4558",  NULL }, 2,
                             std::ref(runningBpsink[0]),&bundlesReceivedBpsink[0]);
    boost::this_thread::sleep(boost::posix_time::seconds(3));
    std::thread threadEgress(RunEgressAsync2,
                             (const char * []){ "egress", "--port1=0", "--port2=4558",
                                                "--stcp-rate-bits-per-sec=500000", NULL }, 4,
                             std::ref(runningEgress),&bundleCountEgress);
    boost::this_thread::sleep(boost::posix_time::seconds(3));
    std::thread threadIngress(RunIngress2,
                              (const char * []){ "ingress", NULL }, 1,
                              std::ref(runningIngress),&bundleCountIngress);
    boost::this_thread::sleep(boost::posix_time::seconds(3));
    std::thread threadBpgen0(RunBpgenAsync2,
                             (const char * []){ "bpgen", "--bundle-rate=50", "--flow-id=2",
                                                "--stcp-rate-bits-per-sec=500000", NULL },4,
                             std::ref(runningBpgen[0]),&bundlesSentBpgen[1]);
    // Allow time for data to flow
    boost::this_thread::sleep(boost::posix_time::seconds(10));
    // Stop threads
    runningBpgen[0] = false;
    threadBpgen0.join();
    runningIngress = false;
    threadIngress.join();
    runningEgress = false;
    threadEgress.join();
    runningBpsink[0] = false;
    threadBpsink0.join();
    // Verify results
    uint64_t totalBundlesBpgen = 0;
    for(int i=0; i<1; i++) {
        totalBundlesBpgen += bundlesSentBpgen[i];
    }
    uint64_t totalBundlesBpsink = 0;
    for(int i=0; i<1; i++) {
        totalBundlesBpsink += bundlesReceivedBpsink[i];
    }
    if (totalBundlesBpgen != bundleCountIngress) {
        BOOST_ERROR("Bundles sent by BPGEN (" + std::to_string(totalBundlesBpgen) + ") !=  bundles received by ingress "
                + std::to_string(bundleCountIngress) + ").");
        return false;
    }
    if (totalBundlesBpgen != bundleCountEgress) {
        BOOST_ERROR("Bundles sent by BPGEN (" + std::to_string(totalBundlesBpgen) + ") != bundles received by egress "
                + std::to_string(bundleCountEgress) + ").");
        return false;
    }
    if (totalBundlesBpgen != totalBundlesBpsink) {
        BOOST_ERROR("Bundles sent by BPGEN (" + std::to_string(totalBundlesBpgen) + ") != bundles received by BPSINK "
                + std::to_string(totalBundlesBpsink) + ").");
        return false;
    }
    return true;
}



bool TestCutThrough(Protocol protocol) {
    RUN_BPGEN = true;
    RUN_BPSINK = true;
    RUN_INGRESS = true;
    RUN_EGRESS = true;
    uint64_t bundlesSentBpgen = 0;
    uint64_t totalBundlesBpsink = 0;
    uint64_t duplicateBundlesBpsink = 0;
    uint64_t totalBytesBpsink = 0;
    uint64_t bundleDataEgress = 0;
    uint64_t bundleCountEgress = 0;
    uint64_t bundleCountIngress = 0;
    uint64_t bundleDataIngress = 0;
    bool alwaysSendToStorage = false;
    uint32_t bundleRate = 50;
    bool useTcpcl = false;
    bool useStcp = false;
    if (protocol == Tcpcl) {
        bool useTcpcl = true;
        bool useStcp = false;
    }
    else if (protocol == Stcp) {
        bool useTcpcl = false;
        bool useStcp = true;
    }
    else if (protocol == Udp) {
        bool useTcpcl = false;
        bool useStcp = false;
    }
    else {
        std::cerr << "Unsupported protocol (" << protocol << ") passed to TestCutThrough." << std::endl << std::flush;
        return false;
    }
    // Start threads
    boost::this_thread::sleep(boost::posix_time::seconds(3));
    std::thread threadBpsink(RunBpsinkAsync,useTcpcl,useStcp,&totalBundlesBpsink,&duplicateBundlesBpsink,&totalBytesBpsink);
    boost::this_thread::sleep(boost::posix_time::seconds(3));
    std::thread threadEgress(RunEgressAsync,useTcpcl,useStcp,&bundleCountEgress, &bundleDataEgress);
    boost::this_thread::sleep(boost::posix_time::seconds(3));
    std::thread threadIngress(RunIngress,useStcp,alwaysSendToStorage,&bundleCountIngress, &bundleDataIngress);
    boost::this_thread::sleep(boost::posix_time::seconds(3));
    std::thread threadBpgen(RunBpgenAsync,useTcpcl,useStcp,bundleRate,&bundlesSentBpgen);
    // Allow time for data to flow
    boost::this_thread::sleep(boost::posix_time::seconds(10));
    // Stop threads
    RUN_BPGEN = false;
    threadBpgen.join();
    RUN_INGRESS = false;
    threadIngress.join();
    RUN_EGRESS = false;
    threadEgress.join();
    RUN_BPSINK = false;
    threadBpsink.join();
    // Verify results
    if (bundlesSentBpgen != bundleCountIngress) {
        BOOST_ERROR("Bundles sent by BPGEN (" + std::to_string(bundlesSentBpgen) + ") !=  bundles received by ingress "
                + std::to_string(bundleCountIngress) + ").");
        return false;
    }
    if (bundlesSentBpgen != bundleCountEgress) {
        BOOST_ERROR("Bundles sent by BPGEN (" + std::to_string(bundlesSentBpgen) + ") != bundles received by egress "
                + std::to_string(bundleCountEgress) + ").");
        return false;
    }
    if (bundlesSentBpgen != totalBundlesBpsink) {
        BOOST_ERROR("Bundles sent by BPGEN (" + std::to_string(bundlesSentBpgen) + ") != bundles received by BPSINK "
                + std::to_string(totalBundlesBpsink) + ").");
        return false;
    }
    return true;
}

bool TestStorage() {
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
    uint32_t bundleRate = 50;
    // Start threads
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
    // Allow time for data to flow
    boost::this_thread::sleep(boost::posix_time::seconds(10));
    // Stop generating bundles
    RUN_BPGEN = false;
    threadBpgen.join();
    // Allow time for storage to process bundles
    boost::this_thread::sleep(boost::posix_time::seconds(3));
    // Send message to allow data to flow
    RUN_RELEASE_MESSAGE_SENDER = false;
    threadRunReleaseMessageSender.join();
    // Allow time for data to flow
    boost::this_thread::sleep(boost::posix_time::seconds(10));
    // Stop threads
    RUN_STORAGE = false;
    threadRunStorage.join();
    RUN_INGRESS = false;
    threadIngress.join();
    RUN_EGRESS = false;
    threadEgress.join();
    RUN_BPSINK = false;
    threadBpsink.join();
    // Verify results
    if (bundlesSentBpgen != bundleCountIngress) {
        BOOST_ERROR("Bundles sent by BPGEN (" + std::to_string(bundlesSentBpgen) + ") !=  bundles received by ingress "
                + std::to_string(bundleCountIngress) + ").");
        return false;
    }
    if (bundlesSentBpgen != bundleCountEgress) {
        BOOST_ERROR("Bundles sent by BPGEN (" + std::to_string(bundlesSentBpgen) + ") != bundles received by egress "
                + std::to_string(bundleCountEgress) + ").");
        return false;
    }
    if (bundlesSentBpgen != totalBundlesBpsink) {
        BOOST_ERROR("Bundles sent by BPGEN (" + std::to_string(bundlesSentBpgen) + ") != bundles received by BPSINK "
                + std::to_string(totalBundlesBpsink) + ").");
        return false;
    }
    return true;
}

BOOST_GLOBAL_FIXTURE(BoostIntegratedTestsFixture);

BOOST_AUTO_TEST_CASE(it_TestCutThroughTcpcl, * boost::unit_test::disabled()) {
    bool result = TestCutThrough(Tcpcl);
    BOOST_CHECK(result == true);
}

BOOST_AUTO_TEST_CASE(it_TestCutThroughUdp, * boost::unit_test::disabled()) {
    bool result = TestCutThrough(Udp);
    BOOST_CHECK(result == true);
}

BOOST_AUTO_TEST_CASE(it_TestCutThroughStcp, * boost::unit_test::disabled()) {
    bool result = TestCutThrough(Stcp);
    BOOST_CHECK(result == true);
}

BOOST_AUTO_TEST_CASE(it_TestStorage, * boost::unit_test::disabled()) {
    bool result = TestStorage();
    BOOST_CHECK(result == true);
}

BOOST_AUTO_TEST_CASE(it_TestCutThroughTcpcl2, * boost::unit_test::disabled()) {
    bool result = TestCutThrough2();
    BOOST_CHECK(result == true);
}

BOOST_AUTO_TEST_CASE(it_TestTcpclFastCutThrough, * boost::unit_test::disabled()) {
    bool result = TestCutThrough3();
    BOOST_CHECK(result == true);
}


BOOST_AUTO_TEST_CASE(it_TestTcpclMultiFastCutThrough, * boost::unit_test::disabled()) {
    bool result = TestCutThrough4();
    BOOST_CHECK(result == true);
}

BOOST_AUTO_TEST_CASE(it_TestUdp, * boost::unit_test::enabled()) {
    bool result = TestUdp1();
    BOOST_CHECK(result == true);
}
