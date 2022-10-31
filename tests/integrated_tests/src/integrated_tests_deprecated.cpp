/***************************************************************************
 * NASA Glenn Research Center, Cleveland, OH
 * Released under the NASA Open Source Agreement (NOSA)
 * May  2021
 ****************************************************************************
 */


#include <codec/bpv6.h>
#include <ingress.h>
#include <fstream>
#include <iostream>
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

#define DELAY_THREAD 3
#define DELAY_TEST 3
#define MAX_RATE "--stcp-rate-bits-per-sec=30000"
#define MAX_RATE_DIV_3 "--stcp-rate-bits-per-sec=10000"
#define MAX_RATE_DIV_6 "--stcp-rate-bits-per-sec=5000"

// Prototypes

int RunBpgenAsync(const char * argv[], int argc, bool & running, uint64_t* ptrBundleCount, OutductFinalStats * ptrFinalStats);
int RunEgressAsync(const char * argv[], int argc, bool & running, uint64_t* ptrBundleCount);
int RunBpsinkAsync(const char * argv[], int argc, bool & running, uint64_t* ptrBundleCount, hdtn::FinalStatsBpSink * ptrFinalStatsBpSink);
int RunIngress(const char * argv[], int argc, bool & running, uint64_t* ptrBundleCount);
int RunStorage(const char * argv[], int argc, bool & running, uint64_t* ptrBundleCount);
void Delay(uint64_t seconds);

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
    std::unique_ptr<boost::process::child> m_childPtr;
    std::unique_ptr<boost::thread> m_threadPythonPtr;
};

BoostIntegratedTestsFixture::BoostIntegratedTestsFixture() {
    boost::unit_test::results_reporter::set_level(boost::unit_test::report_level::DETAILED_REPORT);
    boost::unit_test::unit_test_log.set_threshold_level( boost::unit_test::log_messages );
    m_threadPythonPtr = boost::make_unique<boost::thread>(boost::bind(&BoostIntegratedTestsFixture::StartPythonServer,
                                                                      this));
}

BoostIntegratedTestsFixture::~BoostIntegratedTestsFixture() {
    this->StopPythonServer();
}

void BoostIntegratedTestsFixture::StopPythonServer() {
    m_runningPythonServer = false;
    if (m_childPtr) {
        m_childPtr->terminate();
        m_childPtr->wait();
        int result = m_childPtr->exit_code();
        m_childPtr.reset();
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
    m_childPtr = boost::make_unique<boost::process::child>(boost::process::search_path(pythonExe),commandArg);
    while(m_childPtr->running()) {
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

void Delay(uint64_t seconds) {
    boost::this_thread::sleep(boost::posix_time::seconds(seconds));
}

int RunBpgenAsync(const char * argv[], int argc, bool & running, uint64_t* ptrBundleCount,
    OutductFinalStats * ptrFinalStats) {
    {
        BpGenAsyncRunner runner;
        runner.Run(argc, argv, running, false);
        *ptrBundleCount = runner.m_bundleCount;
        *ptrFinalStats = runner.m_outductFinalStats;
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

int RunBpsinkAsync(const char * argv[], int argc, bool & running, uint64_t* ptrBundleCount, hdtn::FinalStatsBpSink * ptrFinalStatsBpSink) {
    {
        BpSinkAsyncRunner runner;
        runner.Run(argc, argv, running, false);
        *ptrBundleCount = runner.m_receivedCount;
        *ptrFinalStatsBpSink = runner.m_FinalStatsBpSink;
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

    Delay(DELAY_TEST);

    bool runningBpgen = true;
    bool runningBpsink = true;
    bool runningIngress = true;
    bool runningEgress = true;
    uint64_t bundlesSentBpgen[1] = {0};
    OutductFinalStats finalStats[1];
    hdtn::FinalStatsBpSink finalStatsBpSink[1];
    uint64_t totalBundlesBpsink = 0;
    uint64_t bundleCountEgress = 0;
    uint64_t bundleCountIngress = 0;

    // Start threads
    Delay(DELAY_THREAD);
    static const std::string bpsinkConfigArg = "--inducts-config-file=" + (Environment::GetPathHdtnSourceRoot() / "config_files" / "inducts" / "bpsink_one_tcpcl_port4558.json").string();
    static const char * argsBpsink[] = { "storage",bpsinkConfigArg.c_str(), NULL };
    std::thread threadBpsink(RunBpsinkAsync,argsBpsink,2,std::ref(runningBpsink),&totalBundlesBpsink,
                             &finalStatsBpSink[0]);
    Delay(DELAY_THREAD);
    static const std::string hdtnConfigArg = "--hdtn-config-file=" + (Environment::GetPathHdtnSourceRoot() / "config_files" / "hdtn" / "hdtn_ingress1tcpcl_port4556_egress1tcpcl_port4558flowid2.json").string();
    static const char * argsEgress[] = { "egress",hdtnConfigArg.c_str(), NULL };
    std::thread threadEgress(RunEgressAsync,argsEgress,2,std::ref(runningEgress),&bundleCountEgress);
    Delay(DELAY_THREAD);
    static const char * argsIngress[] = { "ingress",hdtnConfigArg.c_str(), NULL };
    std::thread threadIngress(RunIngress,argsIngress,2,std::ref(runningIngress),&bundleCountIngress);
    Delay(DELAY_THREAD);
    static const std::string bpgenConfigArg = "--outducts-config-file=" + (Environment::GetPathHdtnSourceRoot() / "config_files" / "outducts" / "bpgen_one_tcpcl_port4556.json").string();
    static const char * argsBpgen[] = { "bpgen", "--bundle-rate=100","--flow-id=2",bpgenConfigArg.c_str(), NULL };
    std::thread threadBpgen(RunBpgenAsync,argsBpgen,4,std::ref(runningBpgen),&bundlesSentBpgen[0],&finalStats[0]);

    // Allow time for data to flow
    boost::this_thread::sleep(boost::posix_time::seconds(5));

    // Stop threads
    runningBpgen = false;
    threadBpgen.join();
    runningIngress = false;
    threadIngress.join();
    runningEgress = false;
    threadEgress.join();
    runningBpsink = false;
    threadBpsink.join();

    // Get stats
    uint64_t bundlesAckedBpgen[1] = {0};
    for(int i=0; i<1; i++) {
        bundlesAckedBpgen[i] = finalStats[i].m_totalDataSegmentsOrPacketsAcked;
    }
    uint64_t bundlesAckedBpsink[1] = {0};
    for(int i=0; i<1; i++) {
        bundlesAckedBpsink[i] = finalStatsBpSink[i].m_receivedCount;
    }

    // Verify results
    uint64_t totalBundlesBpgen = 0;
    for(int i=0; i<1; i++) {
        totalBundlesBpgen += bundlesSentBpgen[i];
    }
    uint64_t totalBundlesAckedBpgen = 0;
    for(int i=0; i<1; i++) {
        totalBundlesAckedBpgen += bundlesAckedBpgen[i];
    }
    uint64_t totalBundlesAckedBpsink = 0;
    for(int i=0; i<1; i++) {
        totalBundlesAckedBpsink += bundlesAckedBpsink[i];
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
    if (totalBundlesBpgen != totalBundlesAckedBpsink) {
        BOOST_ERROR("Bundles sent by BPGEN (" + std::to_string(totalBundlesBpgen) + ") != bundles acked by BPSINK "
                + std::to_string(totalBundlesAckedBpsink) + ").");
        return false;
    }
    return true;
}

bool TestTcpclFastCutThrough() {

    Delay(DELAY_TEST);

    bool runningBpgen = true;
    bool runningBpsink = true;
    bool runningIngress = true;
    bool runningEgress = true;
    uint64_t bundlesSentBpgen[1] = {0};
    OutductFinalStats finalStats[1];
    hdtn::FinalStatsBpSink finalStatsBpSink[1];
    uint64_t totalBundlesBpsink = 0;
    uint64_t bundleCountEgress = 0;
    uint64_t bundleCountIngress = 0;

    // Start threads
    Delay(DELAY_THREAD);
    static const std::string bpsinkConfigArg = "--inducts-config-file=" + (Environment::GetPathHdtnSourceRoot() / "config_files" / "inducts" / "bpsink_one_tcpcl_port4558.json").string();
    static const char * argsBpsink[] = { "storage",bpsinkConfigArg.c_str(), NULL };
    std::thread threadBpsink(RunBpsinkAsync, argsBpsink, 2, std::ref(runningBpsink), &totalBundlesBpsink,
        &finalStatsBpSink[0]);
    Delay(DELAY_THREAD);
    static const std::string hdtnConfigArg = "--hdtn-config-file=" + (Environment::GetPathHdtnSourceRoot() / "config_files" / "hdtn" / "hdtn_ingress1tcpcl_port4556_egress1tcpcl_port4558flowid2.json").string();
    static const char * argsEgress[] = { "egress",hdtnConfigArg.c_str(), NULL };
    std::thread threadEgress(RunEgressAsync, argsEgress, 2, std::ref(runningEgress), &bundleCountEgress);
    Delay(DELAY_THREAD);
    static const char * argsIngress[] = { "ingress",hdtnConfigArg.c_str(), NULL };
    std::thread threadIngress(RunIngress, argsIngress, 2, std::ref(runningIngress), &bundleCountIngress);
    Delay(DELAY_THREAD);
    static const std::string bpgenConfigArg = "--outducts-config-file=" + (Environment::GetPathHdtnSourceRoot() / "config_files" / "outducts" / "bpgen_one_tcpcl_port4556.json").string();
    static const char * argsBpgen[] = { "bpgen", "--bundle-rate=0","--flow-id=2","--bundle-size=100000","--duration=5",bpgenConfigArg.c_str(), NULL };
    std::thread threadBpgen(RunBpgenAsync, argsBpgen, 6, std::ref(runningBpgen), &bundlesSentBpgen[0], &finalStats[0]);

    // Stop threads
    //runningBpgen = false; // Do not set this for multi case due to the duration parameter.
    threadBpgen.join();
    runningIngress = false;
    threadIngress.join();
    runningEgress = false;
    threadEgress.join();
    runningBpsink = false;
    threadBpsink.join();

    // Get stats
    uint64_t bundlesAckedBpgen[1] = {0};
    for(int i=0; i<1; i++) {
        bundlesAckedBpgen[i] = finalStats[i].m_totalDataSegmentsOrPacketsAcked;
    }
    uint64_t bundlesAckedBpsink[1] = {0};
    for(int i=0; i<1; i++) {
        bundlesAckedBpsink[i] = finalStatsBpSink[i].m_receivedCount;
    }

    // Verify results
    uint64_t totalBundlesBpgen = 0;
    for(int i=0; i<1; i++) {
        totalBundlesBpgen += bundlesSentBpgen[i];
    }
    uint64_t totalBundlesAckedBpgen = 0;
    for(int i=0; i<1; i++) {
        totalBundlesAckedBpgen += bundlesAckedBpgen[i];
    }
    uint64_t totalBundlesAckedBpsink = 0;
    for(int i=0; i<1; i++) {
        totalBundlesAckedBpsink += bundlesAckedBpsink[i];
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
    if (totalBundlesBpgen != totalBundlesAckedBpsink) {
        BOOST_ERROR("Bundles sent by BPGEN (" + std::to_string(totalBundlesBpgen) + ") != bundles acked by BPSINK "
                + std::to_string(totalBundlesAckedBpsink) + ").");
        return false;
    }
    return true;
}

bool TestTcpclMultiFastCutThrough() {

    Delay(DELAY_TEST);

    bool runningBpgen[2] = {true,true};
    bool runningBpsink[2] = {true,true};
    bool runningIngress = true;
    bool runningEgress = true;
    uint64_t bundlesSentBpgen[2] = {0,0};
    OutductFinalStats finalStats[2];
    hdtn::FinalStatsBpSink finalStatsBpSink[2];
    uint64_t bundlesReceivedBpsink[2] = {0,0};
    uint64_t bundleCountEgress = 0;
    uint64_t bundleCountIngress = 0;


    // Start threads
    Delay(DELAY_THREAD);
    static const std::string bpsink0ConfigArg = "--inducts-config-file=" + (Environment::GetPathHdtnSourceRoot() / "config_files" / "inducts" / "bpsink_one_tcpcl_port4557.json").string();
    static const char * argsBpsink0[] = { "bpsink0",bpsink0ConfigArg.c_str(), NULL };
    
    std::thread threadBpsink0(RunBpsinkAsync,argsBpsink0, 2,std::ref(runningBpsink[0]),&bundlesReceivedBpsink[0],
            &finalStatsBpSink[0]);
    Delay(DELAY_THREAD);
    static const std::string bpsink1ConfigArg = "--inducts-config-file=" + (Environment::GetPathHdtnSourceRoot() / "config_files" / "inducts" / "bpsink_one_tcpcl_port4558.json").string();
    static const char * argsBpsink1[] = { "bpsink1",bpsink1ConfigArg.c_str(), NULL };
    std::thread threadBpsink1(RunBpsinkAsync,argsBpsink1, 2, std::ref(runningBpsink[1]),&bundlesReceivedBpsink[1],
            &finalStatsBpSink[1]);
    Delay(DELAY_THREAD);
    static const std::string hdtnConfigArg = "--hdtn-config-file=" + (Environment::GetPathHdtnSourceRoot() / "config_files" / "hdtn" / "hdtn_ingress1tcpcl_port4556_egress2tcpcl_port4557flowid1_port4558flowid2.json").string();
    static const char * argsEgress[] = { "egress",hdtnConfigArg.c_str(), NULL };
    std::thread threadEgress(RunEgressAsync,argsEgress, 2,std::ref(runningEgress),&bundleCountEgress);
    Delay(DELAY_THREAD);
    static const char * argsIngress[] = { "ingress",hdtnConfigArg.c_str(), NULL };
    std::thread threadIngress(RunIngress,argsIngress, 2,std::ref(runningIngress),&bundleCountIngress);
    Delay(DELAY_THREAD);
    static const std::string bpgenConfigArg = "--outducts-config-file=" + (Environment::GetPathHdtnSourceRoot() / "config_files" / "outducts" / "bpgen_one_tcpcl_port4556.json").string();
    static const char * argsBpgen1[] = { "bpgen1", "--bundle-rate=0","--flow-id=2","--bundle-size=100000","--duration=5",bpgenConfigArg.c_str(), NULL };
    std::thread threadBpgen0(RunBpgenAsync,argsBpgen1,6,std::ref(runningBpgen[0]),&bundlesSentBpgen[0],&finalStats[0]);
    Delay(DELAY_THREAD);
    static const char * argsBpgen0[] = { "bpgen0", "--bundle-rate=0", "--flow-id=1","--bundle-size=100000","--duration=3",bpgenConfigArg.c_str(),NULL};
    std::thread threadBpgen1(RunBpgenAsync,argsBpgen0,6,std::ref(runningBpgen[1]),&bundlesSentBpgen[1],&finalStats[1]);
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

    // Get stats
    uint64_t bundlesAckedBpgen[2] = {0,0};
    for(int i=0; i<2; i++) {
        bundlesAckedBpgen[i] = finalStats[i].m_totalDataSegmentsOrPacketsAcked;
    }
    uint64_t bundlesAckedBpsink[2] = {0,0};
    for(int i=0; i<2; i++) {
        bundlesAckedBpsink[i] = finalStatsBpSink[i].m_receivedCount;
    }

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
    uint64_t totalBundlesAckedBpsink = 0;
    for(int i=0; i<2; i++) {
        totalBundlesAckedBpsink += bundlesAckedBpsink[i];
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
    if (totalBundlesBpgen != totalBundlesAckedBpsink) {
        BOOST_ERROR("Bundles sent by BPGEN (" + std::to_string(totalBundlesBpgen) + ") != bundles acked by BPSINK "
                + std::to_string(totalBundlesAckedBpsink) + ").");
        return false;
    }
    return true;
}


bool TestUdp() {

    Delay(DELAY_TEST);

    bool runningBpgen[1] = {true};
    bool runningBpsink[1] = {true};
    bool runningIngress = true;
    bool runningEgress = true;
    uint64_t bundlesSentBpgen[1] = {0};
    OutductFinalStats finalStats[1];
    hdtn::FinalStatsBpSink finalStatsBpSink[1];
    uint64_t bundlesReceivedBpsink[1] = {0};
    uint64_t bundleCountEgress = 0;
    uint64_t bundleCountIngress = 0;

    // Start threads
    Delay(DELAY_THREAD);
    static const std::string bpsinkConfigArg = "--inducts-config-file=" + (Environment::GetPathHdtnSourceRoot() / "config_files" / "inducts" / "bpsink_one_udp_port4558.json").string();
    static const char * argsBpsink0[] = { "bpsink",bpsinkConfigArg.c_str(), NULL };
    std::thread threadBpsink0(RunBpsinkAsync,argsBpsink0,2, std::ref(runningBpsink[0]),&bundlesReceivedBpsink[0],
            &finalStatsBpSink[0]);
    Delay(DELAY_THREAD);
    static const std::string hdtnConfigArg = "--hdtn-config-file=" + (Environment::GetPathHdtnSourceRoot() / "config_files" / "hdtn" / "hdtn_ingress1udp_port4556_egress1udp_port4558flowid2_0.8Mbps.json").string();
    static const char * argsEgress[] = { "egress",hdtnConfigArg.c_str(), NULL };
    std::thread threadEgress(RunEgressAsync,argsEgress,2,std::ref(runningEgress),&bundleCountEgress);
    Delay(DELAY_THREAD);
    static const char * argsIngress[] = { "ingress",hdtnConfigArg.c_str(), NULL };
    std::thread threadIngress(RunIngress,argsIngress,2,std::ref(runningIngress),&bundleCountIngress);
    Delay(DELAY_THREAD);
    static const std::string bpgenConfigArg = "--outducts-config-file=" + (Environment::GetPathHdtnSourceRoot() / "config_files" / "outducts" / "bpgen_one_udp_port4556_0.5Mbps.json").string();
    static const char * argsBpgen0[] = { "bpgen", "--bundle-rate=100","--flow-id=2","--bundle-size=1000",bpgenConfigArg.c_str(), NULL };
    std::thread threadBpgen0(RunBpgenAsync,argsBpgen0,5,std::ref(runningBpgen[0]),&bundlesSentBpgen[0],&finalStats[0]);

    // Allow time for data to flow
    Delay(5);
    // Stop threads
    runningBpgen[0] = false;
    threadBpgen0.join();
    runningIngress = false;
    threadIngress.join();
    runningEgress = false;
    threadEgress.join();
    runningBpsink[0] = false;
    threadBpsink0.join();

    // Get stats
    uint64_t bundlesAckedBpgen[1] = {0};
    for(int i=0; i<1; i++) {
        bundlesAckedBpgen[i] = finalStats[i].m_totalDataSegmentsOrPacketsAcked;
    }
    uint64_t bundlesAckedBpsink[1] = {0};
    for(int i=0; i<1; i++) {
        bundlesAckedBpsink[i] = finalStatsBpSink[i].m_receivedCount;
    }

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
    uint64_t totalBundlesAckedBpsink = 0;
    for(int i=0; i<1; i++) {
        totalBundlesAckedBpsink += bundlesAckedBpsink[i];
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
    if (totalBundlesBpgen != totalBundlesAckedBpsink) {
        BOOST_ERROR("Bundles sent by BPGEN (" + std::to_string(totalBundlesBpgen) + ") != bundles acked by BPSINK "
                + std::to_string(totalBundlesAckedBpsink) + ").");
        return false;
    }
    return true;
}

bool TestUdpFastCutthrough() {

    Delay(DELAY_TEST);

    bool runningBpgen[1] = {true};
    bool runningBpsink[1] = {true};
    bool runningIngress = true;
    bool runningEgress = true;
    uint64_t bundlesSentBpgen[1] = {0};
    OutductFinalStats finalStats[1];
    hdtn::FinalStatsBpSink finalStatsBpSink[1];
    uint64_t bundlesReceivedBpsink[1] = {0};
    uint64_t bundleCountEgress = 0;
    uint64_t bundleCountIngress = 0;

    // Start threads
    Delay(DELAY_THREAD);
    static const std::string bpsinkConfigArg = "--inducts-config-file=" + (Environment::GetPathHdtnSourceRoot() / "config_files" / "inducts" / "bpsink_one_udp_port4558.json").string();
    static const char * argsBpsink0[] = { "bpsink",bpsinkConfigArg.c_str(), NULL };
    std::thread threadBpsink0(RunBpsinkAsync, argsBpsink0, 2, std::ref(runningBpsink[0]), &bundlesReceivedBpsink[0],
        &finalStatsBpSink[0]);
    Delay(DELAY_THREAD);
    static const std::string hdtnConfigArg = "--hdtn-config-file=" + (Environment::GetPathHdtnSourceRoot() / "config_files" / "hdtn" / "hdtn_ingress1udp_port4556_egress1udp_port4558flowid2_0.8Mbps.json").string();
    static const char * argsEgress[] = { "egress",hdtnConfigArg.c_str(), NULL };
    std::thread threadEgress(RunEgressAsync, argsEgress, 2, std::ref(runningEgress), &bundleCountEgress);
    Delay(DELAY_THREAD);
    static const char * argsIngress[] = { "ingress",hdtnConfigArg.c_str(), NULL };
    std::thread threadIngress(RunIngress, argsIngress, 2, std::ref(runningIngress), &bundleCountIngress);
    Delay(DELAY_THREAD);
    static const std::string bpgenConfigArg = "--outducts-config-file=" + (Environment::GetPathHdtnSourceRoot() / "config_files" / "outducts" / "bpgen_one_udp_port4556_0.05Mbps.json").string();
    static const char * argsBpgen0[] = { "bpgen", "--bundle-rate=0","--flow-id=2","--bundle-size=1000","--duration=5",bpgenConfigArg.c_str(), NULL };
    std::thread threadBpgen0(RunBpgenAsync, argsBpgen0, 6, std::ref(runningBpgen[0]), &bundlesSentBpgen[0], &finalStats[0]);

    // Stop threads
    //    runningBpgen[0] = false; // Do not set this for multi case due to the duration parameter.
    threadBpgen0.join();
    runningIngress = false;
    threadIngress.join();
    runningEgress = false;
    threadEgress.join();
    runningBpsink[0] = false;
    threadBpsink0.join();

    // Get stats
    uint64_t bundlesAckedBpgen[1] = {0};
    for(int i=0; i<1; i++) {
        bundlesAckedBpgen[i] = finalStats[i].m_totalDataSegmentsOrPacketsAcked;
    }
    uint64_t bundlesAckedBpsink[1] = {0};
    for(int i=0; i<1; i++) {
        bundlesAckedBpsink[i] = finalStatsBpSink[i].m_receivedCount;
    }

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
    uint64_t totalBundlesAckedBpsink = 0;
    for(int i=0; i<1; i++) {
        totalBundlesAckedBpsink += bundlesAckedBpsink[i];
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
    if (totalBundlesBpgen != totalBundlesAckedBpsink) {
        BOOST_ERROR("Bundles sent by BPGEN (" + std::to_string(totalBundlesBpgen) + ") != bundles acked by BPSINK "
                + std::to_string(totalBundlesAckedBpsink) + ").");
        return false;
    }
    return true;
}

bool TestUdpMultiFastCutthrough() {

    Delay(DELAY_TEST);

    bool runningBpgen[2] = {true,true};
    bool runningBpsink[2] = {true,true};
    bool runningIngress = true;
    bool runningEgress = true;
    uint64_t bundlesSentBpgen[2] = {0,0};
    OutductFinalStats finalStats[2];
    hdtn::FinalStatsBpSink finalStatsBpSink[2];
    uint64_t bundlesReceivedBpsink[2] = {0,0};
    uint64_t bundleCountEgress = 0;
    uint64_t bundleCountIngress = 0;

    // Start threads
    Delay(DELAY_THREAD);
    static const std::string bpsink0ConfigArg = "--inducts-config-file=" + (Environment::GetPathHdtnSourceRoot() / "config_files" / "inducts" / "bpsink_one_udp_port4557.json").string();
    static const char * argsBpsink0[] = { "bpsink0",bpsink0ConfigArg.c_str(), NULL };

    std::thread threadBpsink0(RunBpsinkAsync, argsBpsink0, 2, std::ref(runningBpsink[0]), &bundlesReceivedBpsink[0],
        &finalStatsBpSink[0]);
    Delay(DELAY_THREAD);
    static const std::string bpsink1ConfigArg = "--inducts-config-file=" + (Environment::GetPathHdtnSourceRoot() / "config_files" / "inducts" / "bpsink_one_udp_port4558.json").string();
    static const char * argsBpsink1[] = { "bpsink1",bpsink1ConfigArg.c_str(), NULL };
    std::thread threadBpsink1(RunBpsinkAsync, argsBpsink1, 2, std::ref(runningBpsink[1]), &bundlesReceivedBpsink[1],
        &finalStatsBpSink[1]);
    Delay(DELAY_THREAD);
    static const std::string hdtnConfigArg = "--hdtn-config-file=" + (Environment::GetPathHdtnSourceRoot() / "config_files" / "hdtn" / "hdtn_ingress1udp_port4556_egress2udp_port4557flowid1_port4558flowid2_0.8Mbps.json").string();
    static const char * argsEgress[] = { "egress",hdtnConfigArg.c_str(), NULL };
    std::thread threadEgress(RunEgressAsync, argsEgress, 2, std::ref(runningEgress), &bundleCountEgress);
    Delay(DELAY_THREAD);
    static const char * argsIngress[] = { "ingress",hdtnConfigArg.c_str(), NULL };
    std::thread threadIngress(RunIngress, argsIngress, 2, std::ref(runningIngress), &bundleCountIngress);
    Delay(DELAY_THREAD);
    static const std::string bpgenConfigArg = "--outducts-config-file=" + (Environment::GetPathHdtnSourceRoot() / "config_files" / "outducts" / "bpgen_one_udp_port4556_0.05Mbps.json").string();
    static const char * argsBpgen1[] = { "bpgen1", "--bundle-rate=0","--flow-id=2","--duration=5",bpgenConfigArg.c_str(), NULL };
    std::thread threadBpgen0(RunBpgenAsync, argsBpgen1, 5, std::ref(runningBpgen[0]), &bundlesSentBpgen[0], &finalStats[0]);
    Delay(DELAY_THREAD);
    static const char * argsBpgen0[] = { "bpgen0", "--bundle-rate=0", "--flow-id=1","--duration=3",bpgenConfigArg.c_str(),NULL };
    std::thread threadBpgen1(RunBpgenAsync, argsBpgen0, 5, std::ref(runningBpgen[1]), &bundlesSentBpgen[1], &finalStats[1]);

   
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

    // Get stats
    uint64_t bundlesAckedBpgen[2] = {0,0};
    for(int i=0; i<2; i++) {
        bundlesAckedBpgen[i] = finalStats[i].m_totalDataSegmentsOrPacketsAcked;
    }
    uint64_t bundlesAckedBpsink[2] = {0,0};
    for(int i=0; i<2; i++) {
        bundlesAckedBpsink[i] = finalStatsBpSink[i].m_receivedCount;
    }

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
    uint64_t totalBundlesAckedBpsink = 0;
    for(int i=0; i<2; i++) {
        totalBundlesAckedBpsink += bundlesAckedBpsink[i];
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
    if (totalBundlesBpgen != totalBundlesAckedBpsink) {
        BOOST_ERROR("Bundles sent by BPGEN (" + std::to_string(totalBundlesBpgen) + ") != bundles acked by BPSINK "
                + std::to_string(totalBundlesAckedBpsink) + ").");
        return false;
    }
    return true;
}

bool TestStcp() {

    Delay(DELAY_TEST);

    bool runningBpgen = true;
    bool runningBpsink = {true};
    bool runningIngress = true;
    bool runningEgress = true;
    uint64_t bundlesSentBpgen[1] = {0};
    OutductFinalStats finalStats[1];
    hdtn::FinalStatsBpSink finalStatsBpSink[1];
    uint64_t bundlesReceivedBpsink[1] = {0};
    uint64_t bundleCountEgress = 0;
    uint64_t bundleCountIngress = 0;

    // Start threads
    Delay(DELAY_THREAD);
    static const std::string bpsinkConfigArg = "--inducts-config-file=" + (Environment::GetPathHdtnSourceRoot() / "config_files" / "inducts" / "bpsink_one_stcp_port4558.json").string();
    static const char * argsBpsink[] = { "storage",bpsinkConfigArg.c_str(), NULL };
    std::thread threadBpsink0(RunBpsinkAsync, argsBpsink, 2, std::ref(runningBpsink), &bundlesReceivedBpsink[0],
        &finalStatsBpSink[0]);
    Delay(DELAY_THREAD);
    static const std::string hdtnConfigArg = "--hdtn-config-file=" + (Environment::GetPathHdtnSourceRoot() / "config_files" / "hdtn" / "hdtn_ingress1stcp_port4556_egress1stcp_port4558flowid2.json").string();
    static const char * argsEgress[] = { "egress",hdtnConfigArg.c_str(), NULL };
    std::thread threadEgress(RunEgressAsync, argsEgress, 2, std::ref(runningEgress), &bundleCountEgress);
    Delay(DELAY_THREAD);
    static const char * argsIngress[] = { "ingress",hdtnConfigArg.c_str(), NULL };
    std::thread threadIngress(RunIngress, argsIngress, 2, std::ref(runningIngress), &bundleCountIngress);
    Delay(DELAY_THREAD);
    static const std::string bpgenConfigArg = "--outducts-config-file=" + (Environment::GetPathHdtnSourceRoot() / "config_files" / "outducts" / "bpgen_one_stcp_port4556.json").string();
    static const char * argsBpgen[] = { "bpgen", "--bundle-rate=100","--flow-id=2",bpgenConfigArg.c_str(), NULL };
    std::thread threadBpgen0(RunBpgenAsync, argsBpgen, 4, std::ref(runningBpgen), &bundlesSentBpgen[0], &finalStats[0]);
    // Allow time for data to flow
    Delay(5);
    // Stop threads
    runningBpgen = false;
    threadBpgen0.join();
    runningIngress = false;
    threadIngress.join();
    runningEgress = false;
    threadEgress.join();
    runningBpsink = false;
    threadBpsink0.join();

    // Get stats
    uint64_t bundlesAckedBpgen[1] = {0};
    for(int i=0; i<1; i++) {
        bundlesAckedBpgen[i] = finalStats[i].m_totalDataSegmentsOrPacketsAcked;
    }
    uint64_t bundlesAckedBpsink[1] = {0};
    for(int i=0; i<1; i++) {
        bundlesAckedBpsink[i] = finalStatsBpSink[i].m_receivedCount;
    }

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
    uint64_t totalBundlesAckedBpsink = 0;
    for(int i=0; i<1; i++) {
        totalBundlesAckedBpsink += bundlesAckedBpsink[i];
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
    if (totalBundlesBpgen != totalBundlesAckedBpsink) {
        BOOST_ERROR("Bundles sent by BPGEN (" + std::to_string(totalBundlesBpgen) + ") != bundles acked by BPSINK "
                + std::to_string(totalBundlesAckedBpsink) + ").");
        return false;
    }
    return true;
}

bool TestStcpFastCutthrough() {

    Delay(DELAY_TEST);

    bool runningBpgen = true;
    bool runningBpsink = true;
    bool runningIngress = true;
    bool runningEgress = true;
    uint64_t bundlesSentBpgen[1] = {0};
    OutductFinalStats finalStats[1];
    hdtn::FinalStatsBpSink finalStatsBpSink[1];
    uint64_t bundlesReceivedBpsink[1] = {0};
    uint64_t bundleCountEgress = 0;
    uint64_t bundleCountIngress = 0;

    // Start threads
    Delay(DELAY_THREAD);
    static const std::string bpsinkConfigArg = "--inducts-config-file=" + (Environment::GetPathHdtnSourceRoot() / "config_files" / "inducts" / "bpsink_one_stcp_port4558.json").string();
    static const char * argsBpsink[] = { "storage",bpsinkConfigArg.c_str(), NULL };
    std::thread threadBpsink0(RunBpsinkAsync, argsBpsink, 2, std::ref(runningBpsink), &bundlesReceivedBpsink[0],
        &finalStatsBpSink[0]);
    Delay(DELAY_THREAD);
    static const std::string hdtnConfigArg = "--hdtn-config-file=" + (Environment::GetPathHdtnSourceRoot() / "config_files" / "hdtn" / "hdtn_ingress1stcp_port4556_egress1stcp_port4558flowid2.json").string();
    static const char * argsEgress[] = { "egress",hdtnConfigArg.c_str(), NULL };
    std::thread threadEgress(RunEgressAsync, argsEgress, 2, std::ref(runningEgress), &bundleCountEgress);
    Delay(DELAY_THREAD);
    static const char * argsIngress[] = { "ingress",hdtnConfigArg.c_str(), NULL };
    std::thread threadIngress(RunIngress, argsIngress, 2, std::ref(runningIngress), &bundleCountIngress);
    Delay(DELAY_THREAD);
    static const std::string bpgenConfigArg = "--outducts-config-file=" + (Environment::GetPathHdtnSourceRoot() / "config_files" / "outducts" / "bpgen_one_stcp_port4556.json").string();
    static const char * argsBpgen[] = { "bpgen", "--bundle-rate=0","--flow-id=2","--bundle-size=100000","--duration=5",bpgenConfigArg.c_str(), NULL };
    std::thread threadBpgen0(RunBpgenAsync, argsBpgen, 6, std::ref(runningBpgen), &bundlesSentBpgen[0], &finalStats[0]);
    // Stop threads
    //    runningBpgen[0] = false; // Do not set this for multi case due to the duration parameter.
    threadBpgen0.join();
    runningIngress = false;
    threadIngress.join();
    runningEgress = false;
    threadEgress.join();
    runningBpsink = false;
    threadBpsink0.join();

    // Get stats
    uint64_t bundlesAckedBpgen[1] = {0};
    for(int i=0; i<1; i++) {
        bundlesAckedBpgen[i] = finalStats[i].m_totalDataSegmentsOrPacketsAcked;
    }
    uint64_t bundlesAckedBpsink[1] = {0};
    for(int i=0; i<1; i++) {
        bundlesAckedBpsink[i] = finalStatsBpSink[i].m_receivedCount;
    }

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
    uint64_t totalBundlesAckedBpsink = 0;
    for(int i=0; i<1; i++) {
        totalBundlesAckedBpsink += bundlesAckedBpsink[i];
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
    if (totalBundlesBpgen != totalBundlesAckedBpsink) {
        BOOST_ERROR("Bundles sent by BPGEN (" + std::to_string(totalBundlesBpgen) + ") != bundles acked by BPSINK "
                + std::to_string(totalBundlesAckedBpsink) + ").");
        return false;
    }
    return true;
}

bool TestStcpMultiFastCutthrough() {

    Delay(DELAY_TEST);

    bool runningBpgen[2] = {true,true};
    bool runningBpsink[2] = {true,true};
    bool runningIngress = true;
    bool runningEgress = true;
    uint64_t bundlesSentBpgen[2] = {0,0};
    OutductFinalStats finalStats[2];
    hdtn::FinalStatsBpSink finalStatsBpSink[2];
    uint64_t bundlesReceivedBpsink[2] = {0,0};
    uint64_t bundleCountEgress = 0;
    uint64_t bundleCountIngress = 0;

    // Start threads
    Delay(DELAY_THREAD);
    static const std::string bpsink0ConfigArg = "--inducts-config-file=" + (Environment::GetPathHdtnSourceRoot() / "config_files" / "inducts" / "bpsink_one_stcp_port4557.json").string();
    static const char * argsBpsink0[] = { "bpsink0",bpsink0ConfigArg.c_str(), NULL };

    std::thread threadBpsink0(RunBpsinkAsync, argsBpsink0, 2, std::ref(runningBpsink[0]), &bundlesReceivedBpsink[0],
        &finalStatsBpSink[0]);
    Delay(DELAY_THREAD);
    static const std::string bpsink1ConfigArg = "--inducts-config-file=" + (Environment::GetPathHdtnSourceRoot() / "config_files" / "inducts" / "bpsink_one_stcp_port4558.json").string();
    static const char * argsBpsink1[] = { "bpsink1",bpsink1ConfigArg.c_str(), NULL };
    std::thread threadBpsink1(RunBpsinkAsync, argsBpsink1, 2, std::ref(runningBpsink[1]), &bundlesReceivedBpsink[1],
        &finalStatsBpSink[1]);
    Delay(DELAY_THREAD);
    static const std::string hdtnConfigArg = "--hdtn-config-file=" + (Environment::GetPathHdtnSourceRoot() / "config_files" / "hdtn" / "hdtn_ingress1stcp_port4556_egress2stcp_port4557flowid1_port4558flowid2.json").string();
    static const char * argsEgress[] = { "egress",hdtnConfigArg.c_str(), NULL };
    std::thread threadEgress(RunEgressAsync, argsEgress, 2, std::ref(runningEgress), &bundleCountEgress);
    Delay(DELAY_THREAD);
    static const char * argsIngress[] = { "ingress",hdtnConfigArg.c_str(), NULL };
    std::thread threadIngress(RunIngress, argsIngress, 2, std::ref(runningIngress), &bundleCountIngress);
    Delay(DELAY_THREAD);
    static const std::string bpgenConfigArg = "--outducts-config-file=" + (Environment::GetPathHdtnSourceRoot() / "config_files" / "outducts" / "bpgen_one_stcp_port4556.json").string();
    static const char * argsBpgen1[] = { "bpgen1", "--bundle-rate=0","--flow-id=2","--bundle-size=100000","--duration=5",bpgenConfigArg.c_str(), NULL };
    std::thread threadBpgen0(RunBpgenAsync, argsBpgen1, 6, std::ref(runningBpgen[0]), &bundlesSentBpgen[0], &finalStats[0]);
    Delay(DELAY_THREAD);
    static const char * argsBpgen0[] = { "bpgen0", "--bundle-rate=0", "--flow-id=1","--bundle-size=100000","--duration=3",bpgenConfigArg.c_str(),NULL };
    std::thread threadBpgen1(RunBpgenAsync, argsBpgen0, 6, std::ref(runningBpgen[1]), &bundlesSentBpgen[1], &finalStats[1]);
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

    // Get stats
    uint64_t bundlesAckedBpgen[2] = {0,0};
    for(int i=0; i<2; i++) {
        bundlesAckedBpgen[i] = finalStats[i].m_totalDataSegmentsOrPacketsAcked;
    }
    uint64_t bundlesAckedBpsink[2] = {0,0};
    for(int i=0; i<2; i++) {
        bundlesAckedBpsink[i] = finalStatsBpSink[i].m_receivedCount;
    }

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
    uint64_t totalBundlesAckedBpsink = 0;
    for(int i=0; i<2; i++) {
        totalBundlesAckedBpsink += bundlesAckedBpsink[i];
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
    if (totalBundlesBpgen != totalBundlesAckedBpsink) {
        BOOST_ERROR("Bundles sent by BPGEN (" + std::to_string(totalBundlesBpgen) + ") != bundles acked by BPSINK "
                + std::to_string(totalBundlesAckedBpsink) + ").");
        return false;
    }
    return true;
}

bool TestStorage() {

    Delay(DELAY_TEST);

    bool runningBpgen[1] = {true};
    bool runningBpsink[1] = {true};
    bool runningIngress = true;
    bool runningEgress = true;
    bool runningStorage = true;
    uint64_t bundlesSentBpgen[1] = {0};
    OutductFinalStats finalStats[1];
    hdtn::FinalStatsBpSink finalStatsBpSink[1];
    uint64_t bundlesReceivedBpsink[1] = {0};
    uint64_t bundleCountEgress = 0;
    uint64_t bundleCountIngress = 0;
    uint64_t bundleCountStorage = 0;

    // Start threads
    Delay(DELAY_THREAD);
    static const std::string bpsinkConfigArg = "--inducts-config-file=" + (Environment::GetPathHdtnSourceRoot() / "config_files" / "inducts" / "bpsink_one_tcpcl_port4558.json").string();
    static const char * argsBpsink0[] = { "bpsink",bpsinkConfigArg.c_str(), NULL };
    std::thread threadBpsink0(RunBpsinkAsync,argsBpsink0,2,std::ref(runningBpsink[0]),&bundlesReceivedBpsink[0],
            &finalStatsBpSink[0]);

    Delay(DELAY_THREAD);
    static const std::string hdtnConfigArg = "--hdtn-config-file=" + (Environment::GetPathHdtnSourceRoot() / "config_files" / "hdtn" / "hdtn_ingress1tcpcl_port4556_egress1tcpcl_port4558flowid2.json").string();
    static const char * argsEgress[] = { "egress",hdtnConfigArg.c_str(), NULL };
    std::thread threadEgress(RunEgressAsync,argsEgress,2,std::ref(runningEgress),&bundleCountEgress);

    Delay(DELAY_THREAD);
    static const char * argsIngress[] = { "ingress", hdtnConfigArg.c_str(), NULL };
    std::thread threadIngress(RunIngress,argsIngress,3,std::ref(runningIngress),&bundleCountIngress);

    // Run Release Message Sender
    Delay(DELAY_THREAD);
    ReleaseSender releaseSender;
    std::string eventFile = ReleaseSender::GetFullyQualifiedFilename("releaseMessagesIntegratedTest1.json");
    static const char * argsRelease[] = { "release",hdtnConfigArg.c_str(), NULL };
    std::string junk;
    releaseSender.ProcessComandLine(2, argsRelease, junk); //just to set m_hdtnConfig
    std::thread threadReleaseSender(&ReleaseSender::ProcessEventFile,&releaseSender,eventFile);

    // Run Storage
    Delay(DELAY_THREAD);
    static const char * argsStorage[] = {"storage",hdtnConfigArg.c_str(),NULL};
    StorageRunner storageRunner;
    std::thread threadStorage(&StorageRunner::Run,&storageRunner,2,argsStorage,std::ref(runningStorage),false);

    Delay(DELAY_THREAD);
    static const std::string bpgenConfigArg = "--outducts-config-file=" + (Environment::GetPathHdtnSourceRoot() / "config_files" / "outducts" / "bpgen_one_tcpcl_port4556.json").string();
    static const char * argsBpgen0[] = { "bpgen", "--bundle-rate=100","--flow-id=2","--duration=5",bpgenConfigArg.c_str(), NULL };
    std::thread threadBpgen0(RunBpgenAsync,argsBpgen0, 5,std::ref(runningBpgen[0]),&bundlesSentBpgen[0],&finalStats[0]);

    // Stop threads
//    runningBpgen[0] = false;  // Do not set due to the duration parameter
    threadBpgen0.join();

    // Storage should not be stopped until after release messages has finished.
    while (! releaseSender.m_timersFinished) {
        Delay(1);
    }

    // Do not stop storage until the bundles deleted equal number generated
    uint64_t totalBundlesBpgen = 0;
    for(int i=0; i<1; i++) {
        totalBundlesBpgen += bundlesSentBpgen[i];
    }
    for(int i=0; i<30; i++) {
        uint64_t bundlesDeletedFromStorage = storageRunner.GetCurrentNumberOfBundlesDeletedFromStorage();
        Delay(1);
	std::cout << std::endl << " bundlesDeletedFromStorage: " << bundlesDeletedFromStorage << "totalBundlesBpgen"  << totalBundlesBpgen << std::endl << std::flush;

        if (bundlesDeletedFromStorage == totalBundlesBpgen) {
	    	std::cout << "Exiting!" << std::endl;
		break;
        }
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

    // Get stats
    uint64_t bundlesAckedBpgen[1] = {0};
    for(int i=0; i<1; i++) {
      bundlesAckedBpgen[i] = finalStats[i].m_totalDataSegmentsOrPacketsAcked;
    }
    uint64_t bundlesAckedBpsink[1] = {0};
    for(int i=0; i<1; i++) {
      bundlesAckedBpsink[i] = finalStatsBpSink[i].m_receivedCount;
    }
    uint64_t totalBundlesBpsink = 0;
    for(int i=0; i<1; i++) {
        totalBundlesBpsink += bundlesReceivedBpsink[i];
    }
    uint64_t totalBundlesAckedBpgen = 0;
    for(int i=0; i<1; i++) {
        totalBundlesAckedBpgen += bundlesAckedBpgen[i];
    }
    uint64_t totalBundlesAckedBpsink = 0;
    for(int i=0; i<1; i++) {
        totalBundlesAckedBpsink += bundlesAckedBpsink[i];
    }

    // Verify results
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
    if (totalBundlesBpgen != totalBundlesAckedBpgen) {
        BOOST_ERROR("Bundles sent by BPGEN (" + std::to_string(totalBundlesBpgen) + ") != bundles acked by BPGEN "
                + std::to_string(totalBundlesAckedBpgen) + ").");
        return false;
    }
    if (totalBundlesBpgen != totalBundlesAckedBpsink) {
        BOOST_ERROR("Bundles sent by BPGEN (" + std::to_string(totalBundlesBpgen) + ") != bundles acked by BPSINK "
                + std::to_string(totalBundlesAckedBpsink) + ").");
        return false;
    }
    return true;
}

bool TestStorageSlowBpSink() {

    Delay(DELAY_TEST);

    bool runningBpgen[1] = {true};
    bool runningBpsink[1] = {true};
    bool runningIngress = true;
    bool runningEgress = true;
    bool runningStorage = true;
    uint64_t bundlesSentBpgen[1] = {0};
    OutductFinalStats finalStats[1];
    hdtn::FinalStatsBpSink finalStatsBpSink[1];
    uint64_t bundlesReceivedBpsink[1] = {0};
    uint64_t bundleCountEgress = 0;
    uint64_t bundleCountIngress = 0;
    uint64_t bundleCountStorage = 0;
    //
    // Start threads
    Delay(DELAY_THREAD);
    static const std::string bpsinkConfigArg = "--inducts-config-file=" + (Environment::GetPathHdtnSourceRoot() / "config_files" / "inducts" / "bpsink_one_tcpcl_port4558.json").string();
    static const char * argsBpsink0[] = { "bpsink","--simulate-processing-lag-ms=10", bpsinkConfigArg.c_str(), NULL };
    std::thread threadBpsink0(RunBpsinkAsync, argsBpsink0, 3, std::ref(runningBpsink[0]), &bundlesReceivedBpsink[0],
        &finalStatsBpSink[0]);

    Delay(DELAY_THREAD);
    static const std::string hdtnConfigArg = "--hdtn-config-file=" + (Environment::GetPathHdtnSourceRoot() / "config_files" / "hdtn" / "hdtn_ingress1tcpcl_port4556_egress1tcpcl_port4558flowid2.json").string();
    static const char * argsEgress[] = { "egress",hdtnConfigArg.c_str(), NULL };
    std::thread threadEgress(RunEgressAsync, argsEgress, 2, std::ref(runningEgress), &bundleCountEgress);

    Delay(DELAY_THREAD);
    static const char * argsIngress[] = { "ingress","--always-send-to-storage",hdtnConfigArg.c_str(), NULL };
    std::thread threadIngress(RunIngress, argsIngress, 3, std::ref(runningIngress), &bundleCountIngress);

    // Run Release Message Sender
    Delay(DELAY_THREAD);
    ReleaseSender releaseSender;
    std::string eventFile = ReleaseSender::GetFullyQualifiedFilename("releaseMessagesIntegratedTest1.json");
    static const char * argsRelease[] = { "release",hdtnConfigArg.c_str(), NULL };
    std::string junk;
    releaseSender.ProcessComandLine(2, argsRelease, junk); //just to set m_hdtnConfig
    std::thread threadReleaseSender(&ReleaseSender::ProcessEventFile, &releaseSender, eventFile);

    // Run Storage
    Delay(DELAY_THREAD);
    static const char * argsStorage[] = { "storage",hdtnConfigArg.c_str(),NULL };
    StorageRunner storageRunner;
    std::thread threadStorage(&StorageRunner::Run, &storageRunner, 2, argsStorage, std::ref(runningStorage), false);

    Delay(DELAY_THREAD);
    static const std::string bpgenConfigArg = "--outducts-config-file=" + (Environment::GetPathHdtnSourceRoot() / "config_files" / "outducts" / "bpgen_one_tcpcl_port4556.json").string();
    static const char * argsBpgen0[] = { "bpgen", "--bundle-rate=100","--flow-id=2","--duration=5",bpgenConfigArg.c_str(), NULL };
    std::thread threadBpgen0(RunBpgenAsync, argsBpgen0, 5, std::ref(runningBpgen[0]), &bundlesSentBpgen[0], &finalStats[0]);

    // Stop threads
    //runningBpgen[0] = false;  // Do not set due to the duration parameter
    threadBpgen0.join();

    // Storage should not be stopped until after release messages has finished.
    while (! releaseSender.m_timersFinished) {
        Delay(1);
    }

    // Do not stop storage until the bundles deleted equal number generated
    uint64_t totalBundlesBpgen = 0;
    for(int i=0; i<1; i++) {
        totalBundlesBpgen += bundlesSentBpgen[i];
    }
    for(int i=0; i<30; i++) {
        uint64_t bundlesDeletedFromStorage = storageRunner.GetCurrentNumberOfBundlesDeletedFromStorage();
        Delay(1);
        if (bundlesDeletedFromStorage == totalBundlesBpgen) {
            break;
        }
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

    // Get stats
    uint64_t bundlesAckedBpgen[1] = {0};
    for(int i=0; i<1; i++) {
        bundlesAckedBpgen[i] = finalStats[i].m_totalDataSegmentsOrPacketsAcked;
    }
    uint64_t bundlesAckedBpsink[1] = {0};
    for(int i=0; i<1; i++) {
      bundlesAckedBpsink[i] = finalStatsBpSink[i].m_receivedCount;
    }
    uint64_t totalBundlesAckedBpgen = 0;
    for(int i=0; i<1; i++) {
        totalBundlesAckedBpgen += bundlesAckedBpgen[i];
    }
    uint64_t totalBundlesBpsink = 0;
    for(int i=0; i<1; i++) {
        totalBundlesBpsink += bundlesReceivedBpsink[i];
    }
    uint64_t totalBundlesAckedBpsink = 0;
    for(int i=0; i<1; i++) {
        totalBundlesAckedBpsink += bundlesAckedBpsink[i];
    }

    // Verify results
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
    if (totalBundlesBpgen != totalBundlesAckedBpgen) {
        BOOST_ERROR("Bundles sent by BPGEN (" + std::to_string(totalBundlesBpgen) + ") != bundles acked by BPGEN "
                + std::to_string(totalBundlesAckedBpgen) + ").");
        return false;
    }
    if (totalBundlesBpgen != totalBundlesAckedBpsink) {
        BOOST_ERROR("Bundles sent by BPGEN (" + std::to_string(totalBundlesBpgen) + ") != bundles acked by BPSINK "
                + std::to_string(totalBundlesAckedBpsink) + ").");
        return false;
    }
    return true;
}

bool TestStorageMulti() {

    Delay(DELAY_TEST);

    bool runningBpgen[2] = {true,true};
    bool runningBpsink[2] = {true,true};
    bool runningIngress = true;
    bool runningEgress = true;
    bool runningStorage = true;
    uint64_t bundlesSentBpgen[2] = {0,0};
    OutductFinalStats finalStats[2];
    hdtn::FinalStatsBpSink finalStatsBpSink[2];
    uint64_t bundlesReceivedBpsink[2] = {0,0};
    uint64_t bundleCountEgress = 0;
    uint64_t bundleCountIngress = 0;
    uint64_t bundleCountStorage = 0;

    
    // Start threads

    Delay(DELAY_THREAD);
    static const std::string bpsinkConfigArg0 = "--inducts-config-file=" + (Environment::GetPathHdtnSourceRoot() / "config_files" / "inducts" / "bpsink_one_tcpcl_port4557.json").string();
    static const char * argsBpsink0[] = { "bpsink",bpsinkConfigArg0.c_str(), NULL };
    std::thread threadBpsink0(RunBpsinkAsync, argsBpsink0, 2, std::ref(runningBpsink[0]), &bundlesReceivedBpsink[0],
        &finalStatsBpSink[0]);

    Delay(DELAY_THREAD);
    static const std::string bpsinkConfigArg1 = "--inducts-config-file=" + (Environment::GetPathHdtnSourceRoot() / "config_files" / "inducts" / "bpsink_one_tcpcl_port4558.json").string();
    static const char * argsBpsink1[] = { "bpsink",bpsinkConfigArg1.c_str(), NULL };
    std::thread threadBpsink1(RunBpsinkAsync, argsBpsink1, 2, std::ref(runningBpsink[1]), &bundlesReceivedBpsink[1],
        &finalStatsBpSink[1]);

    Delay(DELAY_THREAD);
    static const std::string hdtnConfigArg = "--hdtn-config-file=" + (Environment::GetPathHdtnSourceRoot() / "config_files" / "hdtn" / "hdtn_ingress1tcpcl_port4556_egress2tcpcl_port4557flowid1_port4558flowid2.json").string();
    static const char * argsEgress[] = { "egress",hdtnConfigArg.c_str(), NULL };
    std::thread threadEgress(RunEgressAsync, argsEgress, 2, std::ref(runningEgress), &bundleCountEgress);

    Delay(DELAY_THREAD);
    static const char * argsIngress[] = { "ingress","--always-send-to-storage",hdtnConfigArg.c_str(), NULL };
    std::thread threadIngress(RunIngress, argsIngress, 3, std::ref(runningIngress), &bundleCountIngress);

    // Run Release Message Sender
    Delay(DELAY_THREAD);
    ReleaseSender releaseSender;
    std::string eventFile = ReleaseSender::GetFullyQualifiedFilename("releaseMessagesIntegratedTest2.json");
    static const char * argsRelease[] = { "release",hdtnConfigArg.c_str(), NULL };
    std::string junk;
    releaseSender.ProcessComandLine(2, argsRelease, junk); //just to set m_hdtnConfig
    std::thread threadReleaseSender(&ReleaseSender::ProcessEventFile, &releaseSender, eventFile);

    // Run Storage
    Delay(1);
    // Run Storage
    Delay(DELAY_THREAD);
    static const char * argsStorage[] = { "storage",hdtnConfigArg.c_str(),NULL };
    StorageRunner storageRunner;
    std::thread threadStorage(&StorageRunner::Run, &storageRunner, 2, argsStorage, std::ref(runningStorage), false);

    Delay(DELAY_THREAD);
    static const std::string bpgenConfigArg = "--outducts-config-file=" + (Environment::GetPathHdtnSourceRoot() / "config_files" / "outducts" / "bpgen_one_tcpcl_port4556.json").string();
    static const char * argsBpgen1[] = { "bpgen", "--bundle-rate=100","--flow-id=2","--duration=5",bpgenConfigArg.c_str(), NULL };
    std::thread threadBpgen1(RunBpgenAsync,argsBpgen1, 5,std::ref(runningBpgen[1]),&bundlesSentBpgen[1],&finalStats[1]);

    Delay(1);
    static const char * argsBpgen0[] = { "bpgen", "--bundle-rate=100","--flow-id=1","--duration=5",bpgenConfigArg.c_str(), NULL };
    std::thread threadBpgen0(RunBpgenAsync,argsBpgen0, 5,std::ref(runningBpgen[0]),&bundlesSentBpgen[0],&finalStats[0]);

    // Stop threads
    //    runningBpgen[0] = false;  // Do not set due to the duration parameter
    threadBpgen0.join();
    //    runningBpgen[1] = false;  // Do not set due to the duration parameter
    threadBpgen1.join();

    // Storage should not be stopped until after release messages has finished.
    while (! releaseSender.m_timersFinished) {
        Delay(1);
    }

    // Do not stop storage until the bundles deleted equal number generated
    uint64_t totalBundlesBpgen = 0;
    for(int i=0; i<2; i++) {
        totalBundlesBpgen += bundlesSentBpgen[i];
    }
    int maxWait = 30;
    for(int i=0; i<maxWait; i++) {
        uint64_t bundlesDeletedFromStorage = storageRunner.GetCurrentNumberOfBundlesDeletedFromStorage();
        Delay(1);
        if (bundlesDeletedFromStorage == totalBundlesBpgen) {
            break;
        }
        if (i == maxWait) {
            std::cerr << "ERROR in TestStorageMulti: " << " bundlesDeletedFromStorage(" << bundlesDeletedFromStorage
                      << ") != totalBundlesBpgen(" << totalBundlesBpgen << ")" << std::endl;
        }
    }

    runningStorage = false;
    threadStorage.join();
    bundleCountStorage = storageRunner.m_totalBundlesSentToEgressFromStorage;

    // Still getting spurious error where bundle lost from BPGEN to ingress
    Delay(5);

    runningIngress = false;
    threadIngress.join();
    runningEgress = false;
    threadEgress.join();
    runningBpsink[1] = false;
    threadBpsink1.join();
    runningBpsink[0] = false;
    threadBpsink0.join();
    threadReleaseSender.join();

    // Get stats
    uint64_t bundlesAckedBpgen[2] = {0,0};
    for(int i=0; i<2; i++) {
        bundlesAckedBpgen[i] = finalStats[i].m_totalDataSegmentsOrPacketsAcked;
    }
    uint64_t bundlesAckedBpsink[2] = {0,0};
    for(int i=0; i<2; i++) {
      bundlesAckedBpsink[i] = finalStatsBpSink[i].m_receivedCount;
    }
    uint64_t totalBundlesAckedBpgen = 0;
    for(int i=0; i<2; i++) {
        totalBundlesAckedBpgen += bundlesAckedBpgen[i];
    }
    uint64_t totalBundlesBpsink = 0;
    for(int i=0; i<2; i++) {
        totalBundlesBpsink += bundlesReceivedBpsink[i];
    }
    uint64_t totalBundlesAckedBpsink = 0;
    for(int i=0; i<2; i++) {
        totalBundlesAckedBpsink += bundlesAckedBpsink[i];
    }

    // Verify results
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
    if (totalBundlesBpgen != totalBundlesAckedBpgen) {
        BOOST_ERROR("Bundles sent by BPGEN (" + std::to_string(totalBundlesBpgen) + ") != bundles acked by BPGEN "
                + std::to_string(totalBundlesAckedBpgen) + ").");
        return false;
    }
    if (totalBundlesBpgen != totalBundlesAckedBpsink) {
        BOOST_ERROR("Bundles sent by BPGEN (" + std::to_string(totalBundlesBpgen) + ") != bundles acked by BPSINK "
                + std::to_string(totalBundlesAckedBpsink) + ").");
        return false;
    }
    return true;
}

BOOST_GLOBAL_FIXTURE(BoostIntegratedTestsFixture);

//  Passes test_tcpl_cutthrough.bat
BOOST_AUTO_TEST_CASE(it_TestCutThroughTcpcl, * boost::unit_test::enabled()) {
    std::cout << std::endl << ">>>>>> Running: " << "it_TestCutThroughTcpcl" << std::endl << std::flush;
    bool result = TestCutThroughTcpcl();
    BOOST_CHECK(result == true);
}

//  Fails ACK test-- test_tcpl_fast_cutthrough.bat
BOOST_AUTO_TEST_CASE(it_TestTcpclFastCutThrough, * boost::unit_test::enabled()) {
    std::cout << std::endl << ">>>>>> Running: " << "it_TestTcpclFastCutThrough" << std::endl << std::flush;
    bool result = TestTcpclFastCutThrough();
    BOOST_CHECK(result == true);
}

//  Fails ACK test -- test_tcpl_multi_fast_cutthrough.bat
BOOST_AUTO_TEST_CASE(it_TestTcpclMultiFastCutThrough, * boost::unit_test::enabled()) {
    std::cout << std::endl << ">>>>>> Running: " << "it_TestTcpclMultiFastCutThrough" << std::endl << std::flush;
    bool result = TestTcpclMultiFastCutThrough();
    BOOST_CHECK(result == true);
}



//  Passes ACK test -- test_udp.bat
BOOST_AUTO_TEST_CASE(it_TestUdp, * boost::unit_test::enabled()) {
    std::cout << std::endl << ">>>>>> Running: "<< "it_TestUdp" << std::endl << std::flush;
    bool result = TestUdp();
    BOOST_CHECK(result == true);
}

//   Passes ACK test -- test_udp_fast_cutthrough.bat
BOOST_AUTO_TEST_CASE(it_TestUdpFastCutthrough, * boost::unit_test::enabled()) {
    std::cout << std::endl << ">>>>>> Running: " << "it_TestUdpFastCutthrough" << std::endl << std::flush;
    bool result = TestUdpFastCutthrough();
    BOOST_CHECK(result == true);
}

//   Passes ACK test -- test_udp_multi_fast_cutthrough.bat
BOOST_AUTO_TEST_CASE(it_TestUdpMultiFastCutthrough, * boost::unit_test::enabled()) {
    std::cout << std::endl << ">>>>>> Running: " "it_TestUdpMultiFastCutthrough" << std::endl << std::flush;
    bool result = TestUdpMultiFastCutthrough();
    BOOST_CHECK(result == true);
}

//  Passes ACK test -- test_stcp.bat
BOOST_AUTO_TEST_CASE(it_TestStcp, * boost::unit_test::enabled()) {
    std::cout << std::endl << ">>>>>> Running: " << "it_TestStcp" << std::endl << std::flush;
    bool result = TestStcp();
    BOOST_CHECK(result == true);
}

//  Passes ACK test -- test_stcp_fast_cutthrough.bat
BOOST_AUTO_TEST_CASE(it_TestStcpFastCutthrough, * boost::unit_test::enabled()) {
    std::cout << std::endl << ">>>>>> Running: " "it_TestStcpFastCutthrough" << std::endl << std::flush;
    bool result = TestStcpFastCutthrough();
    BOOST_CHECK(result == true);
}

//  Passes ACK test -- test_stcp_multi_fast_cutthrough.bat
BOOST_AUTO_TEST_CASE(it_TestStcpMuliFastCutthrough, * boost::unit_test::enabled()) {
    std::cout << std::endl << ">>>>>> Running: " << "it_TestStcpMuliFastCutthrough" << std::endl << std::flush;
    bool result = TestStcpMultiFastCutthrough();
    BOOST_CHECK(result == true);
}

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

