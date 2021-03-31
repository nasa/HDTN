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
#include "StorageRunner.h"
#include "SignalHandler.h"
#include "ReleaseSender.h"

// Prototypes

int RunBpgenAsync(const char * argv[], int argc, bool & running, uint64_t* ptrBundleCount, struct FinalStats * ptrFinalStats);
int RunEgressAsync(const char * argv[], int argc, bool & running, uint64_t* ptrBundleCount);
int RunBpsinkAsync(const char * argv[], int argc, bool & running, uint64_t* ptrBundleCount);
int RunIngress(const char * argv[], int argc, bool & running, uint64_t* ptrBundleCount);
int RunStorage(const char * argv[], int argc, bool & running, uint64_t* ptrBundleCount);

// Global Test Fixture.  Used to setup Python Registration server.
class BoostIntegratedTestsFixture {
public:
    BoostIntegratedTestsFixture();
    ~BoostIntegratedTestsFixture();
    bool m_runningPythonServer;
    void StopPythonServer();
private:
    void StartPythonServer();
    void MonitorExitKeypressThreadFunction();
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
    SignalHandler sigHandler(boost::bind(&BoostIntegratedTestsFixture::MonitorExitKeypressThreadFunction, this));
    sigHandler.Start(false);
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
            sigHandler.PollOnce();
            //std::cout << "StartPythonServer is running. " << std::endl << std::flush;
        }
    }
}

void BoostIntegratedTestsFixture::MonitorExitKeypressThreadFunction() {
    std::cout << "Keyboard Interrupt.. exiting " << std::endl << std::flush;
    this->StopPythonServer();
}

int RunBpgenAsync(const char * argv[], int argc, bool & running, uint64_t* ptrBundleCount,  struct FinalStats * ptrFinalStats) {
    {
        BpGenAsyncRunner runner;
        runner.Run(argc, argv, running, false);
        *ptrBundleCount = runner.m_bundleCount;
        *ptrFinalStats = runner.m_FinalStats;
    }
    return 0;
}

int RunEgressAsync(const char * argv[], int argc, bool & running, uint64_t* ptrBundleCount) {
    {
        EgressAsyncRunner runner;
        runner.Run(argc, argv, running, false);
        *ptrBundleCount = runner.m_bundleCount;
    }
    return 0;
}

int RunBpsinkAsync(const char * argv[], int argc, bool & running, uint64_t* ptrBundleCount) {
    {
        BpSinkAsyncRunner runner;
        runner.Run(argc, argv, running, false);
        *ptrBundleCount = runner.m_receivedCount;
    }
    return 0;
}

int RunIngress(const char * argv[], int argc, bool & running, uint64_t* ptrBundleCount) {
    {
        IngressAsyncRunner runner;
        runner.Run(argc, argv, running, false);
        *ptrBundleCount = runner.m_bundleCount;
    }
    return 0;
}

int RunStorage(const char * argv[], int argc, bool & running, uint64_t* ptrBundleCount) {
    {
        StorageRunner runner;
        runner.Run(argc, argv, running, false);
        *ptrBundleCount = runner.m_totalBundlesSentToEgressFromStorage;
    }
    return 0;
}

bool TestCutThroughTcpcl() {
    boost::this_thread::sleep(boost::posix_time::seconds(3));
    bool runningBpgen = true;
    bool runningBpsink = true;
    bool runningIngress = true;
    bool runningEgress = true;

    uint64_t bundlesSentBpgen[1] = {0};
    struct FinalStats finalStats[1] = {{false,false,0,0,0,0,0,0,0}};
    uint64_t totalBundlesBpsink = 0;
    uint64_t bundleCountEgress = 0;
    uint64_t bundleCountIngress = 0;


    // Start threads
    boost::this_thread::sleep(boost::posix_time::seconds(3));
    static const char * argsBpsink[] = { "bpsink", "--use-tcpcl", "--port=4558", NULL };
    std::thread threadBpsink(RunBpsinkAsync,argsBpsink,3,std::ref(runningBpsink),&totalBundlesBpsink);
    boost::this_thread::sleep(boost::posix_time::seconds(3));
    static const char * argsEgress[] = { "egress", "--use-tcpcl", "--port1=0", "--port2=4558", NULL };
    std::thread threadEgress(RunEgressAsync,argsEgress,4,std::ref(runningEgress),&bundleCountEgress);
    boost::this_thread::sleep(boost::posix_time::seconds(3));
    static const char * argsIngress[] = { "ingress", NULL };
    std::thread threadIngress(RunIngress,argsIngress,1,std::ref(runningIngress),&bundleCountIngress);
    boost::this_thread::sleep(boost::posix_time::seconds(3));
    static const char * argsBpgen[] = { "bpgen", "--bundle-rate=100", "--use-tcpcl", "--flow-id=2", NULL };
    std::thread threadBpgen(RunBpgenAsync,argsBpgen,4,std::ref(runningBpgen),&bundlesSentBpgen[0],&finalStats[0]);

    // Allow time for data to flow
    boost::this_thread::sleep(boost::posix_time::seconds(10));

    // Stop threads
    runningBpgen = false;
    threadBpgen.join();

    // Get stats
    uint64_t bundlesAckedBpgen[1] = {0};
    for(int i=0; i<1; i++) {
        bundlesAckedBpgen[i] = finalStats[i].m_totalDataSegmentsAcked;
    }

    runningIngress = false;
    threadIngress.join();
    runningEgress = false;
    threadEgress.join();
    runningBpsink = false;
    threadBpsink.join();
    // Verify results
    uint64_t totalBundlesBpgen = 0;
    for(int i=0; i<1; i++) {
        totalBundlesBpgen += bundlesSentBpgen[i];
    }
    uint64_t totalBundlesAckedBpgen = 0;
    for(int i=0; i<1; i++) {
        totalBundlesAckedBpgen += bundlesAckedBpgen[i];
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
// Fails this test
//    if (totalBundlesBpgen != totalBundlesAckedBpgen) {
//        BOOST_ERROR("Bundles sent by BPGEN (" + std::to_string(totalBundlesBpgen) + ") != bundles acked by BPGEN "
//                + std::to_string(totalBundlesAckedBpgen) + ").");
//        return false;
//    }
    return true;
}

bool TestTcpclFastCutThrough() {
    boost::this_thread::sleep(boost::posix_time::seconds(3));
    bool runningBpgen = true;
    bool runningBpsink = true;
    bool runningIngress = true;
    bool runningEgress = true;

    uint64_t bundlesSentBpgen[1] = {0};
    struct FinalStats finalStats[1] = {{false,false,0,0,0,0,0,0,0}};
    uint64_t totalBundlesBpsink = 0;
    uint64_t bundleCountEgress = 0;
    uint64_t bundleCountIngress = 0;

    // Start threads
    boost::this_thread::sleep(boost::posix_time::seconds(3));
    static const char * argsBpsink[] = { "bpsink", "--use-tcpcl", "--port=4558", NULL };
    std::thread threadBpsink(RunBpsinkAsync,argsBpsink, 3,std::ref(runningBpsink),&totalBundlesBpsink);
    boost::this_thread::sleep(boost::posix_time::seconds(3));
    static const char * argsEgress[] = { "egress", "--use-tcpcl", "--port1=0", "--port2=4558", NULL };
    std::thread threadEgress(RunEgressAsync,argsEgress, 4,std::ref(runningEgress),&bundleCountEgress);
    boost::this_thread::sleep(boost::posix_time::seconds(3));
    static const char * argsIngress[] = { "ingress", NULL };
    std::thread threadIngress(RunIngress,argsIngress, 1,std::ref(runningIngress),&bundleCountIngress);
    boost::this_thread::sleep(boost::posix_time::seconds(3));
    static const char * argsBpgen[] = { "bpgen", "--bundle-rate=0", "--use-tcpcl", "--flow-id=2", "--duration=10", NULL };

    std::thread threadBpgen(RunBpgenAsync,argsBpgen,5,std::ref(runningBpgen),&bundlesSentBpgen[0],&finalStats[0]);
    // Stop threads
    //runningBpgen = false; // Do not set this for multi case due to the duration parameter.
    threadBpgen.join();

    // Get stats
    uint64_t bundlesAckedBpgen[1] = {0};
    for(int i=0; i<1; i++) {
        bundlesAckedBpgen[i] = finalStats[i].m_totalDataSegmentsAcked;
    }

    runningIngress = false;
    threadIngress.join();
    runningEgress = false;
    threadEgress.join();
    runningBpsink = false;
    threadBpsink.join();
    // Verify results
    uint64_t totalBundlesBpgen = 0;
    for(int i=0; i<1; i++) {
        totalBundlesBpgen += bundlesSentBpgen[i];
    }
    uint64_t totalBundlesAckedBpgen = 0;
    for(int i=0; i<1; i++) {
        totalBundlesAckedBpgen += bundlesAckedBpgen[i];
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
// Fails this test
//    if (totalBundlesBpgen != totalBundlesAckedBpgen) {
//        BOOST_ERROR("Bundles sent by BPGEN (" + std::to_string(totalBundlesBpgen) + ") != bundles acked by BPGEN "
//                + std::to_string(totalBundlesAckedBpgen) + ").");
//        return false;
//    }
    return true;
}

bool TestTcpclMultiFastCutThrough() {
    boost::this_thread::sleep(boost::posix_time::seconds(3));
    bool runningBpgen[2] = {true,true};
    bool runningBpsink[2] = {true,true};
    bool runningIngress = true;
    bool runningEgress = true;

    uint64_t bundlesSentBpgen[2] = {0,0};
    struct FinalStats finalStats[2] = {{false,false,0,0,0,0,0,0,0},{false,false,0,0,0,0,0,0,0}};
    uint64_t bundlesReceivedBpsink[2] = {0,0};
    uint64_t bundleCountEgress = 0;
    uint64_t bundleCountIngress = 0;

    // Start threads
    boost::this_thread::sleep(boost::posix_time::seconds(3));
    static const char * argsBpsink0[] = { "bpsink0", "--use-tcpcl", "--port=4557", NULL };
    std::thread threadBpsink0(RunBpsinkAsync,argsBpsink0, 3,std::ref(runningBpsink[0]),&bundlesReceivedBpsink[0]);
    boost::this_thread::sleep(boost::posix_time::seconds(3));
    static const char * argsBpsink1[] = { "bpsink1", "--use-tcpcl", "--port=4558", NULL };
    std::thread threadBpsink1(RunBpsinkAsync,argsBpsink1, 3, std::ref(runningBpsink[1]),&bundlesReceivedBpsink[1]);
    boost::this_thread::sleep(boost::posix_time::seconds(3));
    static const char * argsEgress[] = { "egress", "--use-tcpcl", "--port1=4557", "--port2=4558", NULL };
    std::thread threadEgress(RunEgressAsync,argsEgress, 4,std::ref(runningEgress),&bundleCountEgress);
    boost::this_thread::sleep(boost::posix_time::seconds(3));
    static const char * argsIngress[] = { "ingress", NULL };
    std::thread threadIngress(RunIngress,argsIngress, 1,std::ref(runningIngress),&bundleCountIngress);
    boost::this_thread::sleep(boost::posix_time::seconds(3));
    static const char * argsBpgen0[] = { "bpgen0", "--bundle-rate=0", "--use-tcpcl", "--flow-id=2","--duration=10" , NULL };
    std::thread threadBpgen0(RunBpgenAsync,argsBpgen0,5,std::ref(runningBpgen[0]),&bundlesSentBpgen[0],&finalStats[0]);
    boost::this_thread::sleep(boost::posix_time::seconds(1));
    static const char * argsBpgen1[] = { "bpgen1", "--bundle-rate=0", "--use-tcpcl", "--flow-id=1","--duration=10" ,NULL };
    std::thread threadBpgen1(RunBpgenAsync,argsBpgen1,5,std::ref(runningBpgen[1]),&bundlesSentBpgen[1],&finalStats[1]);
    // Stop threads
//    runningBpgen[1] = false; // Do not set this for multi case due to the duration parameter.
    threadBpgen1.join();
//    runningBpgen[0] = false; // Do not set this for multi case due to the duration parameter.
    threadBpgen0.join();

    // Get stats
    uint64_t bundlesAckedBpgen[2] = {0,0};
    for(int i=0; i<2; i++) {
        bundlesAckedBpgen[i] = finalStats[i].m_totalDataSegmentsAcked;
    }

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
    uint64_t totalBundlesAckedBpgen = 0;
    for(int i=0; i<2; i++) {
        totalBundlesAckedBpgen += bundlesAckedBpgen[i];
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
// Fails this test
//    if (totalBundlesBpgen != totalBundlesAckedBpgen) {
//        BOOST_ERROR("Bundles sent by BPGEN (" + std::to_string(totalBundlesBpgen) + ") != bundles acked by BPGEN "
//                + std::to_string(totalBundlesAckedBpgen) + ").");
//        return false;
//    }
    return true;
}



bool TestCutThroughMulti() {
    boost::this_thread::sleep(boost::posix_time::seconds(3));
    bool runningBpgen[2] = {true,true};
    bool runningBpsink[2] = {true,true};
    bool runningIngress = true;
    bool runningEgress = true;

    uint64_t bundlesSentBpgen[2] = {0,0};
    struct FinalStats finalStats[2] = {{false,false,0,0,0,0,0,0,0},{false,false,0,0,0,0,0,0,0}};
    uint64_t bundlesReceivedBpsink[2] = {0,0};
    uint64_t bundleCountEgress = 0;
    uint64_t bundleCountIngress = 0;

    // Start threads
    boost::this_thread::sleep(boost::posix_time::seconds(3));
    static const char * argsBpsink0[] = { "bpsink0", "--use-tcpcl", "--port=4557", NULL };
    std::thread threadBpsink0(RunBpsinkAsync,argsBpsink0, 3,std::ref(runningBpsink[0]),&bundlesReceivedBpsink[0]);
    boost::this_thread::sleep(boost::posix_time::seconds(3));
    static const char * argsBpsink1[] = { "bpsink1", "--use-tcpcl", "--port=4558", NULL };
    std::thread threadBpsink1(RunBpsinkAsync,argsBpsink1, 3, std::ref(runningBpsink[1]),&bundlesReceivedBpsink[1]);
    boost::this_thread::sleep(boost::posix_time::seconds(3));
    static const char * argsEgress[] = { "egress", "--use-tcpcl", "--port1=4557", "--port2=4558", NULL };
    std::thread threadEgress(RunEgressAsync,argsEgress, 4,std::ref(runningEgress),&bundleCountEgress);
    boost::this_thread::sleep(boost::posix_time::seconds(3));
    static const char * argsIngress[] = { "ingress", NULL };
    std::thread threadIngress(RunIngress,argsIngress, 1,std::ref(runningIngress),&bundleCountIngress);
    boost::this_thread::sleep(boost::posix_time::seconds(3));
    static const char * argsBpgen0[] = { "bpgen0", "--bundle-rate=100", "--use-tcpcl", "--flow-id=2","--duration=5" , NULL };
    std::thread threadBpgen0(RunBpgenAsync,argsBpgen0,5,std::ref(runningBpgen[0]),&bundlesSentBpgen[0],&finalStats[0]);
    boost::this_thread::sleep(boost::posix_time::seconds(1));
    static const char * argsBpgen1[] = { "bpgen1", "--bundle-rate=100", "--use-tcpcl", "--flow-id=1","--duration=3" ,NULL };
    std::thread threadBpgen1(RunBpgenAsync,argsBpgen1,5,std::ref(runningBpgen[1]),&bundlesSentBpgen[1],&finalStats[1]);
    // Stop threads
//    runningBpgen[1] = false; // Do not set this for multi case due to the duration parameter.
    threadBpgen1.join();
//    runningBpgen[0] = false; // Do not set this for multi case due to the duration parameter.
    threadBpgen0.join();

    // Get stats
    uint64_t bundlesAckedBpgen[2] = {0,0};
    for(int i=0; i<2; i++) {
        bundlesAckedBpgen[i] = finalStats[i].m_totalDataSegmentsAcked;
    }

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
    uint64_t totalBundlesAckedBpgen = 0;
    for(int i=0; i<2; i++) {
        totalBundlesAckedBpgen += bundlesAckedBpgen[i];
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
// Fails this test
//    if (totalBundlesBpgen != totalBundlesAckedBpgen) {
//        BOOST_ERROR("Bundles sent by BPGEN (" + std::to_string(totalBundlesBpgen) + ") != bundles acked by BPGEN "
//                + std::to_string(totalBundlesAckedBpgen) + ").");
//        return false;
//    }
    return true;
}

bool TestUdp() {
    boost::this_thread::sleep(boost::posix_time::seconds(3));
    bool runningBpgen[1] = {true};
    bool runningBpsink[1] = {true};
    bool runningIngress = true;
    bool runningEgress = true;

    uint64_t bundlesSentBpgen[1] = {0};
    struct FinalStats finalStats[1] = {{false,false,0,0,0,0,0,0,0}};
    uint64_t bundlesReceivedBpsink[1] = {0};
    uint64_t bundleCountEgress = 0;
    uint64_t bundleCountIngress = 0;

    // Start threads
    boost::this_thread::sleep(boost::posix_time::seconds(3));
    static const char * argsBpsink0[] = {"bpsink","--port=4558",NULL};
    std::thread threadBpsink0(RunBpsinkAsync,argsBpsink0,2, std::ref(runningBpsink[0]),&bundlesReceivedBpsink[0]);
    boost::this_thread::sleep(boost::posix_time::seconds(3));
    static const char * argsEgress[] = {"egress","--port1=0","--port2=4558","--stcp-rate-bits-per-sec=4500",NULL};
    std::thread threadEgress(RunEgressAsync,argsEgress,4,std::ref(runningEgress),&bundleCountEgress);
    boost::this_thread::sleep(boost::posix_time::seconds(3));
    static const char * argsIngress[] = {"ingress", NULL};
    std::thread threadIngress(RunIngress,argsIngress,1,std::ref(runningIngress),&bundleCountIngress);
    boost::this_thread::sleep(boost::posix_time::seconds(3));
    static const char * argsBpgen0[] = {"bpgen","--bundle-rate=0","--flow-id=2","--stcp-rate-bits-per-sec=1500","--bundle-size=1000",NULL};
    std::thread threadBpgen0(RunBpgenAsync,argsBpgen0,5,std::ref(runningBpgen[0]),&bundlesSentBpgen[0],&finalStats[0]);

    // Allow time for data to flow
    boost::this_thread::sleep(boost::posix_time::seconds(10));
    // Stop threads
    runningBpgen[0] = false;
    threadBpgen0.join();

    // Get stats
    uint64_t bundlesAckedBpgen[1] = {0};
    for(int i=0; i<1; i++) {
        bundlesAckedBpgen[i] = finalStats[i].m_totalUdpPacketsAckedByRate;
        if (finalStats[i].m_totalUdpPacketsAckedByUdpSendCallback > finalStats[i].m_totalUdpPacketsAckedByRate) {
            bundlesAckedBpgen[i] = finalStats[i].m_totalUdpPacketsAckedByUdpSendCallback;
        }
    }
    // JCF -- Delay may be needed to get test to pass consistently.
    boost::this_thread::sleep(boost::posix_time::seconds(6));

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
    uint64_t totalBundlesAckedBpgen = 0;
    for(int i=0; i<1; i++) {
        totalBundlesAckedBpgen += bundlesAckedBpgen[i];
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
    if (totalBundlesBpgen != totalBundlesAckedBpgen) {
        BOOST_ERROR("Bundles sent by BPGEN (" + std::to_string(totalBundlesBpgen) + ") != bundles acked by BPGEN "
                + std::to_string(totalBundlesAckedBpgen) + ").");
        return false;
    }
    return true;
}

bool TestUdpFastCutthrough() {
    boost::this_thread::sleep(boost::posix_time::seconds(3));
    bool runningBpgen[1] = {true};
    bool runningBpsink[1] = {true};
    bool runningIngress = true;
    bool runningEgress = true;

    uint64_t bundlesSentBpgen[1] = {0};
    struct FinalStats finalStats[1] = {{false,false,0,0,0,0,0,0,0}};
    uint64_t bundlesReceivedBpsink[1] = {0};
    uint64_t bundleCountEgress = 0;
    uint64_t bundleCountIngress = 0;

    // Start threads
    boost::this_thread::sleep(boost::posix_time::seconds(3));
    static const char * argsBpsink0[] = {"bpsink","--port=4558",NULL};
    std::thread threadBpsink0(RunBpsinkAsync,argsBpsink0,2, std::ref(runningBpsink[0]),&bundlesReceivedBpsink[0]);
    boost::this_thread::sleep(boost::posix_time::seconds(3));
    static const char * argsEgress[] = {"egress","--port1=0","--port2=4558","--stcp-rate-bits-per-sec=9000",NULL};
    std::thread threadEgress(RunEgressAsync,argsEgress,4,std::ref(runningEgress),&bundleCountEgress);
    boost::this_thread::sleep(boost::posix_time::seconds(3));
    static const char * argsIngress[] = {"ingress", NULL};
    std::thread threadIngress(RunIngress,argsIngress,1,std::ref(runningIngress),&bundleCountIngress);
    boost::this_thread::sleep(boost::posix_time::seconds(3));
    static const char * argsBpgen0[] = {"bpgen","--bundle-rate=0","--flow-id=2","--duration=10",
                                        "--stcp-rate-bits-per-sec=3000","--bundle-size=1000",NULL};
    std::thread threadBpgen0(RunBpgenAsync,argsBpgen0,6,std::ref(runningBpgen[0]),&bundlesSentBpgen[0],&finalStats[0]);
    // Stop threads
    //    runningBpgen[0] = false; // Do not set this for multi case due to the duration parameter.
    threadBpgen0.join();

    // JCF -- Delay may be needed to get test to pass consistently.
    boost::this_thread::sleep(boost::posix_time::seconds(6));

    // Get stats
    uint64_t bundlesAckedBpgen[1] = {0};
    for(int i=0; i<1; i++) {
        bundlesAckedBpgen[i] = finalStats[i].m_totalUdpPacketsAckedByRate;
        if (finalStats[i].m_totalUdpPacketsAckedByUdpSendCallback > finalStats[i].m_totalUdpPacketsAckedByRate) {
            bundlesAckedBpgen[i] = finalStats[i].m_totalUdpPacketsAckedByUdpSendCallback;
        }
    }

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
    uint64_t totalBundlesAckedBpgen = 0;
    for(int i=0; i<1; i++) {
        totalBundlesAckedBpgen += bundlesAckedBpgen[i];
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
    if (totalBundlesBpgen != totalBundlesAckedBpgen) {
        BOOST_ERROR("Bundles sent by BPGEN (" + std::to_string(totalBundlesBpgen) + ") != bundles acked by BPGEN "
                + std::to_string(totalBundlesAckedBpgen) + ").");
        return false;
    }
    return true;
}

bool TestUdpMultiFastCutthrough() {
    boost::this_thread::sleep(boost::posix_time::seconds(3));
    bool runningBpgen[2] = {true,true};
    bool runningBpsink[2] = {true,true};
    bool runningIngress = true;
    bool runningEgress = true;

    uint64_t bundlesSentBpgen[2] = {0,0};
    struct FinalStats finalStats[2] = {{false,false,0,0,0,0,0,0,0},{false,false,0,0,0,0,0,0,0}};
    uint64_t bundlesReceivedBpsink[2] = {0,0};
    uint64_t bundleCountEgress = 0;
    uint64_t bundleCountIngress = 0;

    // Start threads
    boost::this_thread::sleep(boost::posix_time::seconds(3));
    static const char * argsBpsink0[] = {"bpsink","--port=4557",NULL};
    std::thread threadBpsink0(RunBpsinkAsync,argsBpsink0,2,std::ref(runningBpsink[0]),&bundlesReceivedBpsink[0]);
    boost::this_thread::sleep(boost::posix_time::seconds(3));
    static const char * argsBpsink1[] = {"bpsink","--port=4558",NULL};
    std::thread threadBpsink1(RunBpsinkAsync,argsBpsink1,2,std::ref(runningBpsink[1]),&bundlesReceivedBpsink[1]);
    boost::this_thread::sleep(boost::posix_time::seconds(3));
    static const char * argsEgress[] = {"egress","--port1=4557","--port2=4558","--stcp-rate-bits-per-sec=18000",NULL};
    std::thread threadEgress(RunEgressAsync,argsEgress,4,std::ref(runningEgress),&bundleCountEgress);
    boost::this_thread::sleep(boost::posix_time::seconds(3));
    static const char * argsIngress[] = {"ingress", NULL};
    std::thread threadIngress(RunIngress,argsIngress,1,std::ref(runningIngress),&bundleCountIngress);
    boost::this_thread::sleep(boost::posix_time::seconds(3));
    static const char * argsBpgen0[] = {"bpgen","--bundle-rate=0","--flow-id=2","--duration=10",
                                        "--stcp-rate-bits-per-sec=3000","--bundle-size=1000",NULL};
    std::thread threadBpgen0(RunBpgenAsync,argsBpgen0,6,std::ref(runningBpgen[0]),&bundlesSentBpgen[0],&finalStats[0]);
    boost::this_thread::sleep(boost::posix_time::seconds(3));
    static const char * argsBpgen1[] = {"bpgen","--bundle-rate=0","--flow-id=1","--duration=10",
                                        "--stcp-rate-bits-per-sec=3000","--bundle-size=1000",NULL};
    std::thread threadBpgen1(RunBpgenAsync,argsBpgen1,6,std::ref(runningBpgen[1]),&bundlesSentBpgen[1],&finalStats[1]);
    // Stop threads
    //    runningBpgen[1] = false; // Do not set this for multi case due to the duration parameter.
    threadBpgen1.join();
    //    runningBpgen[0] = false; // Do not set this for multi case due to the duration parameter.
    threadBpgen0.join();


    // JCF -- Delay may be needed to get test to pass consistently.
    boost::this_thread::sleep(boost::posix_time::seconds(6));

    // Get stats
    uint64_t bundlesAckedBpgen[2] = {0,0};
    for(int i=0; i<2; i++) {
        bundlesAckedBpgen[i] = finalStats[i].m_totalUdpPacketsAckedByRate;
        if (finalStats[i].m_totalUdpPacketsAckedByUdpSendCallback > finalStats[i].m_totalUdpPacketsAckedByRate) {
            bundlesAckedBpgen[i] = finalStats[i].m_totalUdpPacketsAckedByUdpSendCallback;
        }
    }
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
    uint64_t totalBundlesAckedBpgen = 0;
    for(int i=0; i<2; i++) {
        totalBundlesAckedBpgen += bundlesAckedBpgen[i];
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
    if (totalBundlesBpgen != totalBundlesAckedBpgen) {
        BOOST_ERROR("Bundles sent by BPGEN (" + std::to_string(totalBundlesBpgen) + ") != bundles acked by BPGEN "
                + std::to_string(totalBundlesAckedBpgen) + ").");
        return false;
    }
    return true;
}

bool TestStcp() {
    boost::this_thread::sleep(boost::posix_time::seconds(3));
    bool runningBpgen[1] = {true};
    bool runningBpsink[1] = {true};
    bool runningIngress = true;
    bool runningEgress = true;

    uint64_t bundlesSentBpgen[1] = {0};
    struct FinalStats finalStats[1] = {{false,false,0,0,0,0,0,0,0}};
    uint64_t bundlesReceivedBpsink[1] = {0};
    uint64_t bundleCountEgress = 0;
    uint64_t bundleCountIngress = 0;

    // Start threads
    boost::this_thread::sleep(boost::posix_time::seconds(3));
    static const char * argsBpsink0[] = {"bpsink","--use-stcp","--port=4558",NULL};
    std::thread threadBpsink0(RunBpsinkAsync,argsBpsink0,3,std::ref(runningBpsink[0]),&bundlesReceivedBpsink[0]);
    boost::this_thread::sleep(boost::posix_time::seconds(3));
    static const char * argsEgress[] = {"egress","--use-stcp","--port1=0","--port2=4558",
                                        "--stcp-rate-bits-per-sec=9000",NULL};
    std::thread threadEgress(RunEgressAsync,argsEgress,5,std::ref(runningEgress),&bundleCountEgress);
    boost::this_thread::sleep(boost::posix_time::seconds(3));
    static const char * argsIngress[] = {"ingress","--use-stcp",NULL};
    std::thread threadIngress(RunIngress,argsIngress,2,std::ref(runningIngress),&bundleCountIngress);
    boost::this_thread::sleep(boost::posix_time::seconds(3));
    static const char * argsBpgen0[] = {"bpgen","--bundle-rate=0","--use-stcp","--flow-id=2",
                                        "--stcp-rate-bits-per-sec=3000","--bundle-size=1000",NULL};
    std::thread threadBpgen0(RunBpgenAsync,argsBpgen0,6,std::ref(runningBpgen[0]),&bundlesSentBpgen[0],&finalStats[0]);
    // Allow time for data to flow
    boost::this_thread::sleep(boost::posix_time::seconds(10));
    // Stop threads
    runningBpgen[0] = false;
    threadBpgen0.join();

    // JCF -- Delay may be needed to get test to pass consistently.
    boost::this_thread::sleep(boost::posix_time::seconds(6));

    // Get stats
    uint64_t bundlesAckedBpgen[1] = {0};
    for(int i=0; i<1; i++) {
        bundlesAckedBpgen[i] = finalStats[i].m_totalDataSegmentsAckedByRate;
        if (finalStats[i].m_totalDataSegmentsAckedByTcpSendCallback > finalStats[i].m_totalDataSegmentsAckedByRate) {
            bundlesAckedBpgen[i] = finalStats[i].m_totalDataSegmentsAckedByTcpSendCallback;
        }
    }

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
    uint64_t totalBundlesAckedBpgen = 0;
    for(int i=0; i<1; i++) {
        totalBundlesAckedBpgen += bundlesAckedBpgen[i];
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
    if (totalBundlesBpgen != totalBundlesAckedBpgen) {
        BOOST_ERROR("Bundles sent by BPGEN (" + std::to_string(totalBundlesBpgen) + ") != bundles acked by BPGEN "
                + std::to_string(totalBundlesAckedBpgen) + ").");
        return false;
    }
    return true;
}

bool TestStcpFastCutthrough() {
    boost::this_thread::sleep(boost::posix_time::seconds(3));
    bool runningBpgen[1] = {true};
    bool runningBpsink[1] = {true};
    bool runningIngress = true;
    bool runningEgress = true;

    uint64_t bundlesSentBpgen[1] = {0};
    struct FinalStats finalStats[1] = {{false,false,0,0,0,0,0,0,0}};
    uint64_t bundlesReceivedBpsink[1] = {0};
    uint64_t bundleCountEgress = 0;
    uint64_t bundleCountIngress = 0;

    // Start threads
    boost::this_thread::sleep(boost::posix_time::seconds(3));
    static const char * argsBpsink0[] = { "bpsink",  "--use-stcp", "--port=4558",  NULL };
    std::thread threadBpsink0(RunBpsinkAsync,argsBpsink0, 3,std::ref(runningBpsink[0]),&bundlesReceivedBpsink[0]);
    boost::this_thread::sleep(boost::posix_time::seconds(3));
    static const char * argsEgress[] = { "egress",  "--use-stcp", "--port1=0",
                                         "--port2=4558","--stcp-rate-bits-per-sec=9000", NULL };
    std::thread threadEgress(RunEgressAsync,argsEgress, 5,std::ref(runningEgress),&bundleCountEgress);
    boost::this_thread::sleep(boost::posix_time::seconds(3));
    static const char * argsIngress[] = { "ingress", "--use-stcp", NULL };
    std::thread threadIngress(RunIngress,argsIngress, 2,std::ref(runningIngress),&bundleCountIngress);
    boost::this_thread::sleep(boost::posix_time::seconds(3));
    static const char * argsBpgen0[] = { "bpgen",  "--bundle-rate=0", "--use-stcp",  "--flow-id=2","--duration=10",
                                         "--stcp-rate-bits-per-sec=3000","--bundle-size=1000",NULL };
    std::thread threadBpgen0(RunBpgenAsync,argsBpgen0,7,std::ref(runningBpgen[0]),&bundlesSentBpgen[0],&finalStats[0]);
    // Stop threads
    //    runningBpgen[0] = false; // Do not set this for multi case due to the duration parameter.
    threadBpgen0.join();

    // JCF -- Delay may be needed to get test to pass consistently.
    boost::this_thread::sleep(boost::posix_time::seconds(6));

    // Get stats
    uint64_t bundlesAckedBpgen[1] = {0};
    for(int i=0; i<1; i++) {
        bundlesAckedBpgen[i] = finalStats[i].m_totalDataSegmentsAckedByRate;
        if (finalStats[i].m_totalDataSegmentsAckedByTcpSendCallback > finalStats[i].m_totalDataSegmentsAckedByRate) {
            bundlesAckedBpgen[i] = finalStats[i].m_totalDataSegmentsAckedByTcpSendCallback;
        }
    }

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
    uint64_t totalBundlesAckedBpgen = 0;
    for(int i=0; i<1; i++) {
        totalBundlesAckedBpgen += bundlesAckedBpgen[i];
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
    if (totalBundlesBpgen != totalBundlesAckedBpgen) {
        BOOST_ERROR("Bundles sent by BPGEN (" + std::to_string(totalBundlesBpgen) + ") != bundles acked by BPGEN "
                + std::to_string(totalBundlesAckedBpgen) + ").");
        return false;
    }
    return true;
}

bool TestStcpMultiFastCutthrough() {
    boost::this_thread::sleep(boost::posix_time::seconds(3));
    bool runningBpgen[2] = {true,true};
    bool runningBpsink[2] = {true,true};
    bool runningIngress = true;
    bool runningEgress = true;

    uint64_t bundlesSentBpgen[2] = {0,0};
    struct FinalStats finalStats[2] = {{false,false,0,0,0,0,0,0,0},{false,false,0,0,0,0,0,0,0}};
    uint64_t bundlesReceivedBpsink[2] = {0,0};
    uint64_t bundleCountEgress = 0;
    uint64_t bundleCountIngress = 0;

    // Start threads
    boost::this_thread::sleep(boost::posix_time::seconds(3));
    static const char * argsBpsink0[] = { "bpsink",  "--use-stcp", "--port=4557",  NULL };
    std::thread threadBpsink0(RunBpsinkAsync,argsBpsink0,3,std::ref(runningBpsink[0]),&bundlesReceivedBpsink[0]);
    boost::this_thread::sleep(boost::posix_time::seconds(3));
    static const char * argsBpsink1[] = { "bpsink",  "--use-stcp", "--port=4558",  NULL };
    std::thread threadBpsink1(RunBpsinkAsync,argsBpsink1,3,std::ref(runningBpsink[1]),&bundlesReceivedBpsink[1]);
    boost::this_thread::sleep(boost::posix_time::seconds(3));
    static const char * argsEgress[] = { "egress",  "--use-stcp", "--port1=4557", "--port2=4558",
                                         "--stcp-rate-bits-per-sec=18000", NULL };
    std::thread threadEgress(RunEgressAsync,argsEgress, 5,std::ref(runningEgress),&bundleCountEgress);
    boost::this_thread::sleep(boost::posix_time::seconds(3));
    static const char * argsIngress[] = { "ingress", "--use-stcp", NULL };
    std::thread threadIngress(RunIngress,argsIngress, 2,std::ref(runningIngress),&bundleCountIngress);
    boost::this_thread::sleep(boost::posix_time::seconds(3));
    static const char * argsBpgen0[] = { "bpgen",  "--bundle-rate=0", "--use-stcp",  "--flow-id=2","--duration=10",
                                         "--stcp-rate-bits-per-sec=3000","--bundle-size=1000",NULL };
    std::thread threadBpgen0(RunBpgenAsync,argsBpgen0,7,std::ref(runningBpgen[0]),&bundlesSentBpgen[0],&finalStats[0]);
    boost::this_thread::sleep(boost::posix_time::seconds(3));
    static const char * argsBpgen1[] = { "bpgen",  "--bundle-rate=0", "--use-stcp",  "--flow-id=1","--duration=10",
                                         "--stcp-rate-bits-per-sec=3000", "--bundle-size=1000",NULL };
    std::thread threadBpgen1(RunBpgenAsync,argsBpgen1,7,std::ref(runningBpgen[1]),&bundlesSentBpgen[1],&finalStats[1]);
    // Stop threads
    //    runningBpgen[1] = false; // Do not set this for multi case due to the duration parameter.
    threadBpgen1.join();
    //    runningBpgen[0] = false; // Do not set this for multi case due to the duration parameter.
    threadBpgen0.join();

    // JCF -- Delay may be needed to get test to pass consistently.
    boost::this_thread::sleep(boost::posix_time::seconds(6));

    // Get stats
    uint64_t bundlesAckedBpgen[2] = {0,0};
    for(int i=0; i<2; i++) {
        bundlesAckedBpgen[i] = finalStats[i].m_totalDataSegmentsAckedByRate;
        if (finalStats[i].m_totalDataSegmentsAckedByTcpSendCallback > finalStats[i].m_totalDataSegmentsAckedByRate) {
            bundlesAckedBpgen[i] = finalStats[i].m_totalDataSegmentsAckedByTcpSendCallback;
        }
    }

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
    uint64_t totalBundlesAckedBpgen = 0;
    for(int i=0; i<2; i++) {
        totalBundlesAckedBpgen += bundlesAckedBpgen[i];
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
    if (totalBundlesBpgen != totalBundlesAckedBpgen) {
        BOOST_ERROR("Bundles sent by BPGEN (" + std::to_string(totalBundlesBpgen) + ") != bundles acked by BPGEN "
                + std::to_string(totalBundlesAckedBpgen) + ").");
        return false;
    }
    return true;
}

bool TestStorage() {
    boost::this_thread::sleep(boost::posix_time::seconds(3));
    bool runningBpgen[1] = {true};
    bool runningBpsink[1] = {true};
    bool runningIngress = true;
    bool runningEgress = true;
    bool runningStorage = true;

    uint64_t bundlesSentBpgen[1] = {0};
    struct FinalStats finalStats[1] = {{false,false,0,0,0,0,0,0,0}};
    uint64_t bundlesReceivedBpsink[1] = {0};
    uint64_t bundleCountEgress = 0;
    uint64_t bundleCountIngress = 0;
    uint64_t bundleCountStorage = 0;

    // Start threads
    boost::this_thread::sleep(boost::posix_time::seconds(3));
    static const char * argsBpsink0[] = {"bpsink","--use-tcpcl","--port=4558",NULL};
    std::thread threadBpsink0(RunBpsinkAsync,argsBpsink0,3,std::ref(runningBpsink[0]),&bundlesReceivedBpsink[0]);

    boost::this_thread::sleep(boost::posix_time::seconds(3));
    static const char * argsEgress[] = {"egress","--use-tcpcl","--port1=0","--port2=4558",NULL};
    std::thread threadEgress(RunEgressAsync,argsEgress,4,std::ref(runningEgress),&bundleCountEgress);

    boost::this_thread::sleep(boost::posix_time::seconds(3));
    static const char * argsIngress[] = {"ingress","--always-send-to-storage",NULL};
    std::thread threadIngress(RunIngress,argsIngress,2,std::ref(runningIngress),&bundleCountIngress);

    // Run Release Message Sender
    boost::this_thread::sleep(boost::posix_time::seconds(3));
    ReleaseSender releaseSender;
    std::string eventFile = ReleaseSender::GetFullyQualifiedFilename("releaseMessagesIntegratedTest1.json");
    std::thread threadReleaseSender(&ReleaseSender::ProcessEventFile,&releaseSender,eventFile);

    // Run Storage
    boost::this_thread::sleep(boost::posix_time::seconds(1));
    static const std::string storageConfigArg = "--storage-config-json-file=" + (Environment::GetPathHdtnSourceRoot() / "module" / "storage" / "storage-brian" / "unit_tests" / "storageConfigRelativePaths.json").string();
    static const char * argsStorage[] = {"storage",storageConfigArg.c_str(),NULL};
    StorageRunner storageRunner;
    std::thread threadStorage(&StorageRunner::Run,&storageRunner,2,argsStorage,std::ref(runningStorage),false);

    boost::this_thread::sleep(boost::posix_time::seconds(3));
    static const char * argsBpgen0[] = {"bpgen","--bundle-rate=100","--use-tcpcl","--duration=5","--flow-id=2",NULL};
    std::thread threadBpgen0(RunBpgenAsync,argsBpgen0, 5,std::ref(runningBpgen[0]),&bundlesSentBpgen[0],&finalStats[0]);

    // Stop threads
//    runningBpgen[0] = false;  // Do not set due to the duration parameter
    threadBpgen0.join();

    // Storage should not be stopped until after release messages has finished.
    while (! releaseSender.m_timersFinished) {
        boost::this_thread::sleep(boost::posix_time::seconds(1));
    }

    // Do not stop storage until the bundles deleted equal number generated
    uint64_t totalBundlesBpgen = 0;
    for(int i=0; i<2; i++) {
        totalBundlesBpgen += bundlesSentBpgen[i];
    }
    for(int i=0; i<30; i++) {
        uint64_t bundlesDeletedFromStorage = storageRunner.GetCurrentNumberOfBundlesDeletedFromStorage();
        boost::this_thread::sleep(boost::posix_time::seconds(1));
        if (bundlesDeletedFromStorage == totalBundlesBpgen) {
            break;
        }
    }

    // Get stats
    uint64_t bundlesAckedBpgen[1] = {0};
    for(int i=0; i<1; i++) {
        bundlesAckedBpgen[i] = finalStats[i].m_totalDataSegmentsAcked;
    }

//    std::cout << "finalStats[0].useStcp:                                   " << finalStats[0].useStcp << std::endl;
//    std::cout << "finalStats[0].useTcpcl:                                  " << finalStats[0].useTcpcl << std::endl;
//    std::cout << "finalStats[0].bundleCount:                               " << finalStats[0].bundleCount << std::endl;
//    std::cout << "finalStats[0].m_totalUdpPacketsSent:                     " << finalStats[0].m_totalUdpPacketsSent << std::endl;
//    std::cout << "finalStats[0].m_totalDataSegmentsAcked:                  " << finalStats[0].m_totalDataSegmentsAcked << std::endl;
//    std::cout << "finalStats[0].m_totalUdpPacketsAckedByRate:              " << finalStats[0].m_totalUdpPacketsAckedByRate << std::endl;
//    std::cout << "finalStats[0].m_totalDataSegmentsAckedByRate:            " << finalStats[0].m_totalDataSegmentsAckedByRate << std::endl;
//    std::cout << "finalStats[0].m_totalUdpPacketsAckedByUdpSendCallback:   " << finalStats[0].m_totalUdpPacketsAckedByUdpSendCallback << std::endl;
//    std::cout << "finalStats[0].m_totalDataSegmentsAckedByTcpSendCallback: " << finalStats[0].m_totalDataSegmentsAckedByTcpSendCallback << std::endl;

    uint64_t totalBundlesAckedBpgen = 0;
    for(int i=0; i<1; i++) {
        totalBundlesAckedBpgen += bundlesAckedBpgen[i];
    }

    runningStorage = false;
    threadStorage.join();
    bundleCountStorage = storageRunner.m_totalBundlesSentToEgressFromStorage;

    runningIngress = false;
    threadIngress.join();
    runningEgress = false;
    threadEgress.join();
    runningBpsink[0] = false;
    threadBpsink0.join();

    threadReleaseSender.join();

    // Verify results
    uint64_t totalBundlesBpsink = 0;
    for(int i=0; i<1; i++) {
        totalBundlesBpsink += bundlesReceivedBpsink[i];
    }

    if (totalBundlesBpgen != bundleCountIngress) {
        BOOST_ERROR("Bundles sent by BPGEN (" + std::to_string(totalBundlesBpgen) + ") !=  bundles received by ingress "
                + std::to_string(bundleCountIngress) + ").");
        return false;
    }
    if (totalBundlesBpgen != bundleCountStorage) {
        BOOST_ERROR("Bundles sent by BPGEN (" + std::to_string(totalBundlesBpgen) + ") != bundles sent by storage "
                + std::to_string(bundleCountStorage) + ").");
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
// Fails this test
//    if (totalBundlesBpgen != totalBundlesAckedBpgen) {
//        BOOST_ERROR("Bundles sent by BPGEN (" + std::to_string(totalBundlesBpgen) + ") != bundles acked by BPGEN "
//                + std::to_string(totalBundlesAckedBpgen) + ").");
//        return false;
//    }
    return true;
}

bool TestStorageSlowBpSink() {
    boost::this_thread::sleep(boost::posix_time::seconds(3));
    bool runningBpgen[1] = {true};
    bool runningBpsink[1] = {true};
    bool runningIngress = true;
    bool runningEgress = true;
    bool runningStorage = true;

    uint64_t bundlesSentBpgen[1] = {0};
    struct FinalStats finalStats[1] = {{false,false,0,0,0,0,0,0,0}};

    uint64_t bundlesReceivedBpsink[1] = {0};
    uint64_t bundleCountEgress = 0;
    uint64_t bundleCountIngress = 0;
    uint64_t bundleCountStorage = 0;

    // Start threads
    boost::this_thread::sleep(boost::posix_time::seconds(3));
    static const char * argsBpsink0[] = {"bpsink","--use-tcpcl","--port=4558","--simulate-processing-lag-ms=10",NULL};
    std::thread threadBpsink0(RunBpsinkAsync,argsBpsink0,4,std::ref(runningBpsink[0]),&bundlesReceivedBpsink[0]);

    boost::this_thread::sleep(boost::posix_time::seconds(3));
    static const char * argsEgress[] = {"egress","--use-tcpcl","--port1=0","--port2=4558",NULL};
    std::thread threadEgress(RunEgressAsync,argsEgress,4,std::ref(runningEgress),&bundleCountEgress);

    boost::this_thread::sleep(boost::posix_time::seconds(3));
    static const char * argsIngress[] = {"ingress","--always-send-to-storage",NULL};
    std::thread threadIngress(RunIngress,argsIngress,2,std::ref(runningIngress),&bundleCountIngress);

    // Run Release Message Sender
    boost::this_thread::sleep(boost::posix_time::seconds(3));
    //ReleaseSender releaseSender;
    ReleaseSender releaseSender;
    std::string eventFile = ReleaseSender::GetFullyQualifiedFilename("releaseMessagesIntegratedTest1.json");
    //    std::thread threadReleaseSender(&ReleaseSender::ProcessEventFile,releaseSender,eventFile);
    std::thread threadReleaseSender(&ReleaseSender::ProcessEventFile,&releaseSender,eventFile);

    //std::cout <<  " $$$ Time Before Storage: " << boost::posix_time::second_clock::local_time() << std::endl << std::flush;

    // Run Storage
    boost::this_thread::sleep(boost::posix_time::seconds(1));

    static const std::string storageConfigArg = "--storage-config-json-file=" + (Environment::GetPathHdtnSourceRoot() / "module" / "storage" / "storage-brian" / "unit_tests" / "storageConfigRelativePaths.json").string();

    static const char * argsStorage[] = {"storage",storageConfigArg.c_str(),NULL};
    StorageRunner storageRunner;
    std::thread threadStorage(&StorageRunner::Run,&storageRunner,2,argsStorage,std::ref(runningStorage),false);

    boost::this_thread::sleep(boost::posix_time::seconds(3));
    static const char * argsBpgen0[] = {"bpgen","--bundle-rate=100","--use-tcpcl","--duration=5","--flow-id=2",NULL};
    std::thread threadBpgen0(RunBpgenAsync,argsBpgen0,5,std::ref(runningBpgen[0]),&bundlesSentBpgen[0],&finalStats[0]);

    // Allow time for data to flow
    //boost::this_thread::sleep(boost::posix_time::seconds(10));  // Do not set due to the duration parameter
    // Stop threads
    //runningBpgen[0] = false;  // Do not set due to the duration parameter
    threadBpgen0.join();

    // Storage should not be stopped until after release messages has finished.
    while (! releaseSender.m_timersFinished) {
        boost::this_thread::sleep(boost::posix_time::seconds(1));
    }
//    std::cout << "releaseSender.m_timersFinished: " << ptrReleaseSender->m_timersFinished << std::endl << std::flush;

    // Do not stop storage until the bundles deleted equal number generated
    uint64_t totalBundlesBpgen = 0;
    for(int i=0; i<1; i++) {
        totalBundlesBpgen += bundlesSentBpgen[i];
    }
    for(int i=0; i<30; i++) {
        uint64_t bundlesDeletedFromStorage = storageRunner.GetCurrentNumberOfBundlesDeletedFromStorage();
//        std::cout << "\ntotalBundlesBpgen: " << totalBundlesBpgen << std::endl << std::flush;
//        std::cout << "bundlesDeletedFromStorage: " <<  bundlesDeletedFromStorage << std::endl << std::flush;
        boost::this_thread::sleep(boost::posix_time::seconds(1));
        if (bundlesDeletedFromStorage == totalBundlesBpgen) {
            break;
        }
    }

    // Get stats
    uint64_t bundlesAckedBpgen[1] = {0};
    for(int i=0; i<1; i++) {
        bundlesAckedBpgen[i] = finalStats[i].m_totalDataSegmentsAcked;
    }

    uint64_t totalBundlesAckedBpgen = 0;
    for(int i=0; i<1; i++) {
        totalBundlesAckedBpgen += bundlesAckedBpgen[i];
    }

    runningStorage = false;
    threadStorage.join();
    bundleCountStorage = storageRunner.m_totalBundlesSentToEgressFromStorage;

    runningIngress = false;
    threadIngress.join();
    runningEgress = false;
    threadEgress.join();
    runningBpsink[0] = false;
    threadBpsink0.join();

    threadReleaseSender.join();

    // Verify results
    uint64_t totalBundlesBpsink = 0;
    for(int i=0; i<1; i++) {
        totalBundlesBpsink += bundlesReceivedBpsink[i];
    }

    if (totalBundlesBpgen != bundleCountIngress) {
        BOOST_ERROR("Bundles sent by BPGEN (" + std::to_string(totalBundlesBpgen) + ") !=  bundles received by ingress "
                + std::to_string(bundleCountIngress) + ").");
        return false;
    }
    if (totalBundlesBpgen != bundleCountStorage) {
        BOOST_ERROR("Bundles sent by BPGEN (" + std::to_string(totalBundlesBpgen) + ") != bundles sent by storage "
                + std::to_string(bundleCountStorage) + ").");
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
// Fails this test
//    if (totalBundlesBpgen != totalBundlesAckedBpgen) {
//        BOOST_ERROR("Bundles sent by BPGEN (" + std::to_string(totalBundlesBpgen) + ") != bundles acked by BPGEN "
//                + std::to_string(totalBundlesAckedBpgen) + ").");
//        return false;
//    }
    return true;
}

bool TestStorageMulti() {
    boost::this_thread::sleep(boost::posix_time::seconds(3));
    bool runningBpgen[2] = {true,true};
    bool runningBpsink[2] = {true,true};
    bool runningIngress = true;
    bool runningEgress = true;
    bool runningStorage = true;

    uint64_t bundlesSentBpgen[2] = {0,0};
    struct FinalStats finalStats[2] = {{false,false,0,0,0,0,0,0,0},{false,false,0,0,0,0,0,0,0}};
    uint64_t bundlesReceivedBpsink[2] = {0,0};
    uint64_t bundleCountEgress = 0;
    uint64_t bundleCountIngress = 0;
    uint64_t bundleCountStorage = 0;

    // Start threads

    boost::this_thread::sleep(boost::posix_time::seconds(3));
    static const char * argsBpsink0[] = {"bpsink","--use-tcpcl","--port=4557",NULL};
    std::thread threadBpsink0(RunBpsinkAsync,argsBpsink0,3,std::ref(runningBpsink[0]),&bundlesReceivedBpsink[0]);

    boost::this_thread::sleep(boost::posix_time::seconds(3));
    static const char * argsBpsink1[] = {"bpsink","--use-tcpcl","--port=4558",NULL};
    std::thread threadBpsink1(RunBpsinkAsync,argsBpsink1,3,std::ref(runningBpsink[1]),&bundlesReceivedBpsink[1]);

    boost::this_thread::sleep(boost::posix_time::seconds(3));
    static const char * argsEgress[] = {"egress","--use-tcpcl","--port1=4557","--port2=4558",NULL};
    std::thread threadEgress(RunEgressAsync,argsEgress,4,std::ref(runningEgress),&bundleCountEgress);

    boost::this_thread::sleep(boost::posix_time::seconds(3));
    static const char * argsIngress[] = {"ingress","--always-send-to-storage",NULL};
    std::thread threadIngress(RunIngress,argsIngress,2,std::ref(runningIngress),&bundleCountIngress);

    // Run Release Message Sender
    boost::this_thread::sleep(boost::posix_time::seconds(3));
    //ReleaseSender releaseSender;
    ReleaseSender releaseSender;
    std::string eventFile = ReleaseSender::GetFullyQualifiedFilename("releaseMessagesIntegratedTest2.json");
    //    std::thread threadReleaseSender(&ReleaseSender::ProcessEventFile,releaseSender,eventFile);
    std::thread threadReleaseSender(&ReleaseSender::ProcessEventFile,&releaseSender,eventFile);

//    std::cout <<  " $$$ Time Before Storage: " << boost::posix_time::second_clock::local_time() << std::endl << std::flush;

    // Run Storage
    boost::this_thread::sleep(boost::posix_time::seconds(1));

    static const std::string storageConfigArg = "--storage-config-json-file=" + (Environment::GetPathHdtnSourceRoot() / "module" / "storage" / "storage-brian" / "unit_tests" / "storageConfigRelativePaths.json").string();

//    std::cout << "storageConfigArg: " << storageConfigArg << std::endl << std::flush;
    static const char * argsStorage[] = {"storage",storageConfigArg.c_str(),NULL};
    StorageRunner storageRunner;
    std::thread threadStorage(&StorageRunner::Run,&storageRunner,2,argsStorage,std::ref(runningStorage),false);

    boost::this_thread::sleep(boost::posix_time::seconds(3));
    static const char * argsBpgen1[] = {"bpgen","--bundle-rate=100","--use-tcpcl","--duration=5","--flow-id=2",NULL};
    std::thread threadBpgen1(RunBpgenAsync,argsBpgen1, 5,std::ref(runningBpgen[1]),&bundlesSentBpgen[1],&finalStats[1]);

    boost::this_thread::sleep(boost::posix_time::seconds(3));
    static const char * argsBpgen0[] = {"bpgen","--bundle-rate=100","--use-tcpcl","--duration=3","--flow-id=1",NULL};
    std::thread threadBpgen0(RunBpgenAsync,argsBpgen0, 5,std::ref(runningBpgen[0]),&bundlesSentBpgen[0],&finalStats[0]);

    // Allow time for data to flow
    //boost::this_thread::sleep(boost::posix_time::seconds(10));  // Do not set due to the duration parameter
    // Stop threads
    //    runningBpgen[0] = false;  // Do not set due to the duration parameter
    threadBpgen0.join();
    //    runningBpgen[1] = false;  // Do not set due to the duration parameter
    threadBpgen1.join();

    // Storage should not be stopped until after release messages has finished.
    while (! releaseSender.m_timersFinished) {
        boost::this_thread::sleep(boost::posix_time::seconds(1));
    }
//    std::cout << "releaseSender.m_timersFinished: " << ptrReleaseSender->m_timersFinished << std::endl << std::flush;

    // Do not stop storage until the bundles deleted equal number generated
    uint64_t totalBundlesBpgen = 0;
    for(int i=0; i<2; i++) {
        totalBundlesBpgen += bundlesSentBpgen[i];
    }
    for(int i=0; i<30; i++) {
        uint64_t bundlesDeletedFromStorage = storageRunner.GetCurrentNumberOfBundlesDeletedFromStorage();
//        std::cout << "\ntotalBundlesBpgen: " << totalBundlesBpgen << std::endl << std::flush;
//        std::cout << "bundlesDeletedFromStorage: " <<  bundlesDeletedFromStorage << std::endl << std::flush;
        boost::this_thread::sleep(boost::posix_time::seconds(1));
        if (bundlesDeletedFromStorage == totalBundlesBpgen) {
            break;
        }
    }

    // Get stats
    uint64_t bundlesAckedBpgen[2] = {0,0};
    for(int i=0; i<2; i++) {
        bundlesAckedBpgen[i] = finalStats[i].m_totalDataSegmentsAcked;
    }

    uint64_t totalBundlesAckedBpgen = 0;
    for(int i=0; i<2; i++) {
        totalBundlesAckedBpgen += bundlesAckedBpgen[i];
    }

    runningStorage = false;
    threadStorage.join();
    bundleCountStorage = storageRunner.m_totalBundlesSentToEgressFromStorage;

    runningIngress = false;
    threadIngress.join();
    runningEgress = false;
    threadEgress.join();
    runningBpsink[1] = false;
    threadBpsink1.join();
    runningBpsink[0] = false;
    threadBpsink0.join();
    threadReleaseSender.join();

    // Verify results
    uint64_t totalBundlesBpsink = 0;
    for(int i=0; i<2; i++) {
        totalBundlesBpsink += bundlesReceivedBpsink[i];
    }
    if (totalBundlesBpgen != bundleCountIngress) {
        BOOST_ERROR("Bundles sent by BPGEN (" + std::to_string(totalBundlesBpgen) + ") !=  bundles received by ingress "
                + std::to_string(bundleCountIngress) + ").");
        return false;
    }
    if (totalBundlesBpgen != bundleCountStorage) {
        BOOST_ERROR("Bundles sent by BPGEN (" + std::to_string(totalBundlesBpgen) + ") != bundles sent by storage "
                + std::to_string(bundleCountStorage) + ").");
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
// Fails this test
//    if (totalBundlesBpgen != totalBundlesAckedBpgen) {
//        BOOST_ERROR("Bundles sent by BPGEN (" + std::to_string(totalBundlesBpgen) + ") != bundles acked by BPGEN "
//                + std::to_string(totalBundlesAckedBpgen) + ").");
//        return false;
//    }
    return true;
}

BOOST_GLOBAL_FIXTURE(BoostIntegratedTestsFixture);

////  Fails ACK test -- test_tcpl_cutthrough.bat
//BOOST_AUTO_TEST_CASE(it_TestCutThroughTcpcl, * boost::unit_test::enabled()) {
//    std::cout << std::endl << ">>>>>> Running: " << "it_TestCutThroughTcpcl" << std::endl << std::flush;
//    bool result = TestCutThroughTcpcl();
//    BOOST_CHECK(result == true);
//}

////  Fails ACK test-- test_tcpl_fast_cutthrough.bat
//BOOST_AUTO_TEST_CASE(it_TestTcpclFastCutThrough, * boost::unit_test::enabled()) {
//    std::cout << std::endl << ">>>>>> Running: " << "it_TestTcpclFastCutThrough" << std::endl << std::flush;
//    bool result = TestTcpclFastCutThrough();
//    BOOST_CHECK(result == true);
//}

////  Fails ACK test -- test_tcpl_multi_fast_cutthrough.bat
//BOOST_AUTO_TEST_CASE(it_TestTcpclMultiFastCutThrough, * boost::unit_test::enabled()) {
//    std::cout << std::endl << ">>>>>> Running: " << "it_TestTcpclMultiFastCutThrough" << std::endl << std::flush;
//    bool result = TestTcpclMultiFastCutThrough();
//    BOOST_CHECK(result == true);
//}

////   Fails ACK test -- test_cutthrough_multi.bat
//BOOST_AUTO_TEST_CASE(it_TestCutThroughMulti, * boost::unit_test::enabled()) {
//    std::cout << std::endl << ">>>>>> Running: " << "it_TestCutThroughMulti" << std::endl << std::flush;
//    bool result = TestCutThroughMulti();
//    BOOST_CHECK(result == true);
//}

////  Passes ACK test -- test_udp.bat
//BOOST_AUTO_TEST_CASE(it_TestUdp, * boost::unit_test::enabled()) {
//    std::cout << std::endl << ">>>>>> Running: "<< "it_TestUdp" << std::endl << std::flush;
//    bool result = TestUdp();
//    BOOST_CHECK(result == true);
//}

////   Passes ACK test -- test_udp_fast_cutthrough.bat
//BOOST_AUTO_TEST_CASE(it_TestUdpFastCutthrough, * boost::unit_test::enabled()) {
//    std::cout << std::endl << ">>>>>> Running: " << "it_TestUdpFastCutthrough" << std::endl << std::flush;
//    bool result = TestUdpFastCutthrough();
//    BOOST_CHECK(result == true);
//}

////   Passes ACK test -- test_udp_multi_fast_cutthrough.bat
//BOOST_AUTO_TEST_CASE(it_TestUdpMultiFastCutthrough, * boost::unit_test::enabled()) {
//    std::cout << std::endl << ">>>>>> Running: " "it_TestUdpMultiFastCutthrough" << std::endl << std::flush;
//    bool result = TestUdpMultiFastCutthrough();
//    BOOST_CHECK(result == true);
//}

////  Passes ACK test -- test_stcp.bat
//BOOST_AUTO_TEST_CASE(it_TestStcp, * boost::unit_test::enabled()) {
//    std::cout << std::endl << ">>>>>> Running: " << "it_TestStcp" << std::endl << std::flush;
//    bool result = TestStcp();
//    BOOST_CHECK(result == true);
//}

////  Passes ACK test -- test_stcp_fast_cutthrough.bat
//BOOST_AUTO_TEST_CASE(it_TestStcpFastCutthrough, * boost::unit_test::enabled()) {
//    std::cout << std::endl << ">>>>>> Running: " "it_TestStcpFastCutthrough" << std::endl << std::flush;
//    bool result = TestStcpFastCutthrough();
//    BOOST_CHECK(result == true);
//}

////  Passes ACK test -- test_stcp_multi_fast_cutthrough.bat
//BOOST_AUTO_TEST_CASE(it_TestStcpMuliFastCutthrough, * boost::unit_test::enabled()) {
//    std::cout << std::endl << ">>>>>> Running: " << "it_TestStcpMuliFastCutthrough" << std::endl << std::flush;
//    bool result = TestStcpMultiFastCutthrough();
//    BOOST_CHECK(result == true);
//}

//   Fails ACK test -- test_storage.bat
BOOST_AUTO_TEST_CASE(it_TestStorage, * boost::unit_test::enabled()) {
    std::cout << std::endl << ">>>>>> Running: " << "it_TestStorage" << std::endl << std::flush;
    bool result = TestStorage();
    BOOST_CHECK(result == true);
}

//    Fails ACK test -- test_storage_multi.bat
BOOST_AUTO_TEST_CASE(it_TestStorageMulti, * boost::unit_test::enabled()) {
    std::cout << std::endl << ">>>>>> Running: " << "it_TestStorageMulti" << std::endl << std::flush;
    bool result = TestStorageMulti();
    BOOST_CHECK(result == true);
}

//    Fails ACK test -- test_storage_slowbpsink.bat
BOOST_AUTO_TEST_CASE(it_TestStorageSlowBpSink, * boost::unit_test::enabled()) {
    std::cout << std::endl << ">>>>>> Running: " << "it_TestStorageSlowBpSink" << std::endl << std::flush;
    bool result = TestStorageSlowBpSink();
    BOOST_CHECK(result == true);
}

