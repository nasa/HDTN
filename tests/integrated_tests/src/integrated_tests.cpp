/***************************************************************************
 * NASA Glenn Research Center, Cleveland, OH
 * Released under the NASA Open Source Agreement (NOSA)
 * May  2021
 ****************************************************************************
 */

 //todo: better cmake solution to detect if we are using boost static or shared libs... assume for now
 //      that shared libs will be used on linux and static libs will be used on windows.
#ifndef _WIN32
#define BOOST_TEST_DYN_LINK
#endif

//#define BOOST_TEST_NO_MAIN 1

#include <codec/bpv6.h>
#include <ingress.h>
#include <fstream>
#include <iostream>
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
#include "HdtnOneProcessRunner.h"
#include "EgressAsyncRunner.h"
#include "StorageRunner.h"
#include "SignalHandler.h"
#include "SchedulerRunner.h"

#define DELAY_THREAD 3
#define DELAY_TEST 3
#define MAX_RATE "--stcp-rate-bits-per-sec=30000"
#define MAX_RATE_DIV_3 "--stcp-rate-bits-per-sec=10000"
#define MAX_RATE_DIV_6 "--stcp-rate-bits-per-sec=5000"

// Prototypes

int RunBpgenAsync(const char * argv[], int argc, bool & running, uint64_t* ptrBundleCount, OutductFinalStats * ptrFinalStats);
int RunBpsinkAsync(const char * argv[], int argc, bool & running, uint64_t* ptrBundleCount, FinalStatsBpSink * ptrFinalStatsBpSink);
int RunHdtnOneProcess(const char * argv[], int argc, bool & running, uint64_t* ptrBundleCountStorage,
                      uint64_t* ptrBundleCountEgress, uint64_t* ptrBundleCountIngress);
void Delay(uint64_t seconds);

// Global Test Fixture.  Used to setup Python Registration server.
class BoostIntegratedTestsFixture {
public:
    BoostIntegratedTestsFixture();
    ~BoostIntegratedTestsFixture();
private:
    void MonitorExitKeypressThreadFunction();
};

BoostIntegratedTestsFixture::BoostIntegratedTestsFixture() {
    boost::unit_test::results_reporter::set_level(boost::unit_test::report_level::DETAILED_REPORT);
    boost::unit_test::unit_test_log.set_threshold_level( boost::unit_test::log_messages );
}

BoostIntegratedTestsFixture::~BoostIntegratedTestsFixture() {
}

void BoostIntegratedTestsFixture::MonitorExitKeypressThreadFunction() {
    std::cout << "Keyboard Interrupt.. exiting " << std::endl << std::flush;
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

int RunBpsinkAsync(const char * argv[], int argc, bool & running, uint64_t* ptrBundleCount, 
		FinalStatsBpSink * ptrFinalStatsBpSink) {
    {
        BpSinkAsyncRunner runner;
        runner.Run(argc, argv, running, false);
        *ptrBundleCount = runner.m_receivedCount + runner.m_duplicateCount;
        *ptrFinalStatsBpSink = runner.m_FinalStatsBpSink;
    }
    return 0;
}

int RunHdtnOneProcess(const char * argv[], int argc, bool & running, uint64_t* ptrBundleCountStorage, 
		      uint64_t* ptrBundleCountEgress, uint64_t* ptrBundleCountIngress) {
    {
        HdtnOneProcessRunner runner;
        runner.Run(argc, argv, running, false);
	*ptrBundleCountStorage = runner.m_ingressBundleCountStorage;
	*ptrBundleCountEgress = runner.m_ingressBundleCountEgress;
        *ptrBundleCountIngress = runner.m_ingressBundleCount;
    }
    return 0;
}

bool TestHDTNCutThroughModeLTP() {

    Delay(DELAY_TEST);

    bool runningBpgen = true;
    bool runningBpsink = true;
    bool runningHdtnOneProcess = true; 

    uint64_t bundlesSentBpgen[1] = {0};
    OutductFinalStats finalStats[1];
    FinalStatsBpSink finalStatsBpSink[1];
    uint64_t bundlesReceivedBpsink[1]= {0};
    uint64_t bundleCountStorage = 0;
    uint64_t bundleCountEgress = 0;
    uint64_t bundleCountIngress = 0;

    // Start threads
    Delay(DELAY_THREAD);

    //bpsink
    static const std::string bpsinkConfigArg = "--inducts-config-file=" + (Environment::GetPathHdtnSourceRoot() / "config_files" / "inducts" / "bpsink_one_ltp_port4558.json").string();
    static const char * argsBpsink[] = { "bpsink",  "--my-uri-eid=ipn:2.1", bpsinkConfigArg.c_str(), NULL };
    std::thread threadBpsink(RunBpsinkAsync, argsBpsink, 3, std::ref(runningBpsink), &bundlesReceivedBpsink[0],
        &finalStatsBpSink[0]);

    Delay(DELAY_THREAD);

    //HDTN One Process
    //HdtnOneProcessRunner hdtn;
    static const std::string hdtnConfigArg = "--hdtn-config-file=" + (Environment::GetPathHdtnSourceRoot() / "config_files" / "hdtn" / "hdtn_ingress1ltp_port4556_egress1ltp_port4558flowid2.json").string();
    const boost::filesystem::path contactsFile = "contactPlanCutThroughMode.json";
    const std::string eventFileArg = "--contact-plan-file=" + contactsFile.string();
    const char * argsHdtnOneProcess[] = { "HdtnOneProcess", eventFileArg.c_str(), hdtnConfigArg.c_str(), NULL };
    //std::thread threadHdtn(&HdtnOneProcessRunner::Run, &hdtn, 3,  argsHdtnOneProcess, std::ref(runningHdtnOneProcess), true);
    std::thread threadHdtn(RunHdtnOneProcess, argsHdtnOneProcess, 3, std::ref(runningHdtnOneProcess), &bundleCountStorage,
		             &bundleCountEgress, &bundleCountIngress);

    Delay(10);

    //Bpgen
    static const std::string bpgenConfigArg = 
	"--outducts-config-file=" + (Environment::GetPathHdtnSourceRoot() / "config_files" / "outducts" / "bpgen_one_ltp_port4556_thisengineid200.json").string();
    static const char * argsBpgen[] = { "bpgen", "--bundle-rate=100", "--my-uri-eid=ipn:1.1", "--dest-uri-eid=ipn:2.1","--duration=40", bpgenConfigArg.c_str(), NULL };
    std::thread threadBpgen(RunBpgenAsync,argsBpgen, 6, std::ref(runningBpgen), &bundlesSentBpgen[0], &finalStats[0]);

    // Allow time for data to flow
    boost::this_thread::sleep(boost::posix_time::seconds(8));

    // Stop threads
    runningBpgen = false;
    threadBpgen.join();

    runningHdtnOneProcess = false;
    threadHdtn.join();

    runningBpsink = false;
    threadBpsink.join();

    // Verify results
    uint64_t totalBundlesBpgen = 0;
    for (int i=0; i<1; i++) {
        totalBundlesBpgen += bundlesSentBpgen[i];
    }
 
    uint64_t totalBundlesBpsink = 0;
    for(int i=0; i<1; i++) {
        totalBundlesBpsink += bundlesReceivedBpsink[i];
    }

    if (bundleCountIngress != bundleCountEgress) {
        BOOST_ERROR("Total Bundles received by Ingress (" + std::to_string(bundleCountIngress) + ") != Total bundles received by Egress in Cuthrough Mode "
                    + std::to_string(bundleCountEgress) + ").");
        return false;
    }

    if (totalBundlesBpgen != totalBundlesBpsink) {
        BOOST_ERROR("Bundles sent by BpGen (" + std::to_string(totalBundlesBpgen) + ") != bundles received by BpSink "
                    + std::to_string(totalBundlesBpsink) + ").");
	return false;
    }

    if (totalBundlesBpgen != bundleCountIngress) {
        BOOST_ERROR("Bundles sent by BpGen (" + std::to_string(totalBundlesBpgen) + ") !=  bundles received by Ingress "
                + std::to_string(bundleCountIngress) + ").");
        return false;
    }

    if (totalBundlesBpgen != bundleCountEgress) {
        BOOST_ERROR("Bundles sent by BpGen (" + std::to_string(totalBundlesBpgen) + ") != bundles received by Egress "
                + std::to_string(bundleCountEgress) + ").");
        return false;
    }
    
    return true;
}

bool TestHDTNCutThroughModeLTPv7() {

    Delay(DELAY_TEST);

    bool runningBpgen = true;
    bool runningBpsink = true;
    bool runningHdtnOneProcess = true; 

    uint64_t bundlesSentBpgen[1] = {0};
    OutductFinalStats finalStats[1];
    FinalStatsBpSink finalStatsBpSink[1];
    uint64_t bundlesReceivedBpsink[1]= {0};
    uint64_t bundleCountStorage = 0;
    uint64_t bundleCountEgress = 0;
    uint64_t bundleCountIngress = 0;

    // Start threads
    Delay(DELAY_THREAD);

    //bpsink
    static const std::string bpsinkConfigArg = "--inducts-config-file=" + (Environment::GetPathHdtnSourceRoot() / "config_files" / "inducts" / "bpsink_one_ltp_port4558.json").string();
    static const char * argsBpsink[] = { "bpsink",  "--my-uri-eid=ipn:2.1", bpsinkConfigArg.c_str(), NULL };
    std::thread threadBpsink(RunBpsinkAsync, argsBpsink, 3, std::ref(runningBpsink), &bundlesReceivedBpsink[0],
        &finalStatsBpSink[0]);

    Delay(DELAY_THREAD);

    //HDTN One Process
    //HdtnOneProcessRunner hdtn;
    static const std::string hdtnConfigArg = "--hdtn-config-file=" + (Environment::GetPathHdtnSourceRoot() / "config_files" / "hdtn" / "hdtn_ingress1ltp_port4556_egress1ltp_port4558flowid2.json").string();
    const boost::filesystem::path contactsFile = "contactPlanCutThroughMode.json";
    const std::string eventFileArg = "--contact-plan-file=" + contactsFile.string();
    const char * argsHdtnOneProcess[] = { "HdtnOneProcess", eventFileArg.c_str(), hdtnConfigArg.c_str(), NULL };
    //std::thread threadHdtn(&HdtnOneProcessRunner::Run, &hdtn, 3,  argsHdtnOneProcess, std::ref(runningHdtnOneProcess), true);
    std::thread threadHdtn(RunHdtnOneProcess, argsHdtnOneProcess, 3, std::ref(runningHdtnOneProcess), &bundleCountStorage,
		             &bundleCountEgress, &bundleCountIngress);

    Delay(10);

    //Bpgen
    static const std::string bpgenConfigArg = 
	"--outducts-config-file=" + (Environment::GetPathHdtnSourceRoot() / "config_files" / "outducts" / "bpgen_one_ltp_port4556_thisengineid200.json").string();
    static const char * argsBpgen[] = { "bpgen", "--bundle-rate=100", "--my-uri-eid=ipn:1.1", "--dest-uri-eid=ipn:2.1","--duration=40", bpgenConfigArg.c_str(), NULL, "--use-bp-version-7"};
    std::thread threadBpgen(RunBpgenAsync,argsBpgen, 6, std::ref(runningBpgen), &bundlesSentBpgen[0], &finalStats[0]);

    // Allow time for data to flow
    boost::this_thread::sleep(boost::posix_time::seconds(8));

    // Stop threads
    runningBpgen = false;
    threadBpgen.join();

    runningHdtnOneProcess = false;
    threadHdtn.join();

    runningBpsink = false;
    threadBpsink.join();

    // Verify results
    uint64_t totalBundlesBpgen = 0;
    for (int i=0; i<1; i++) {
        totalBundlesBpgen += bundlesSentBpgen[i];
    }
 
    uint64_t totalBundlesBpsink = 0;
    for(int i=0; i<1; i++) {
        totalBundlesBpsink += bundlesReceivedBpsink[i];
    }

    if (bundleCountIngress != bundleCountEgress) {
        BOOST_ERROR("Total Bundles received by Ingress (" + std::to_string(bundleCountIngress) + ") != Total bundles received by Egress in Cuthrough Mode "
                    + std::to_string(bundleCountEgress) + ").");
        return false;
    }

    if (totalBundlesBpgen != totalBundlesBpsink) {
        BOOST_ERROR("Bundles sent by BpGen (" + std::to_string(totalBundlesBpgen) + ") != bundles received by BpSink "
                    + std::to_string(totalBundlesBpsink) + ").");
	return false;
    }

    if (totalBundlesBpgen != bundleCountIngress) {
        BOOST_ERROR("Bundles sent by BpGen (" + std::to_string(totalBundlesBpgen) + ") !=  bundles received by Ingress "
                + std::to_string(bundleCountIngress) + ").");
        return false;
    }

    if (totalBundlesBpgen != bundleCountEgress) {
        BOOST_ERROR("Bundles sent by BpGen (" + std::to_string(totalBundlesBpgen) + ") != bundles received by Egress "
                + std::to_string(bundleCountEgress) + ").");
        return false;
    }
    
    return true;
}



bool TestHDTNStorageModeLTP() {

    Delay(DELAY_TEST);

    bool runningBpgen = true;
    bool runningBpsink = true;
    bool runningHdtnOneProcess = true;

    uint64_t bundlesSentBpgen[1] = {0};
    OutductFinalStats finalStats[1];
    FinalStatsBpSink finalStatsBpSink[1];
    uint64_t bundlesReceivedBpsink[1]= {0};
    uint64_t bundleCountStorage = 0;
    uint64_t bundleCountEgress = 0;
    uint64_t bundleCountIngress = 0;

    // Start threads
    Delay(DELAY_THREAD);

    //bpsink
    static const std::string bpsinkConfigArg = "--inducts-config-file=" + (Environment::GetPathHdtnSourceRoot() / "config_files" / "inducts" / "bpsink_one_ltp_port4558.json").string();
    static const char * argsBpsink[] = { "bpsink",  "--my-uri-eid=ipn:2.1", bpsinkConfigArg.c_str(), NULL };
    std::thread threadBpsink(RunBpsinkAsync, argsBpsink, 3, std::ref(runningBpsink), &bundlesReceivedBpsink[0],
        &finalStatsBpSink[0]);

    Delay(DELAY_THREAD);

    //HDTN One Process
    static const std::string hdtnConfigArg = "--hdtn-config-file=" + (Environment::GetPathHdtnSourceRoot() / "config_files" / "hdtn" / "hdtn_ingress1ltp_port4556_egress1ltp_port4558flowid2.json").string();
    const boost::filesystem::path contactsFile = "contactPlanStorageMode.json";
    const std::string eventFileArg = "--contact-plan-file=" + contactsFile.string();
    const char * argsHdtnOneProcess[] = { "HdtnOneProcess", eventFileArg.c_str(), hdtnConfigArg.c_str(), NULL };
    std::thread threadHdtn(RunHdtnOneProcess, argsHdtnOneProcess, 3, std::ref(runningHdtnOneProcess), &bundleCountStorage,
                           &bundleCountEgress, &bundleCountIngress);

    Delay(10);

    //Bpgen
    static const std::string bpgenConfigArg =
        "--outducts-config-file=" + (Environment::GetPathHdtnSourceRoot() / "config_files" / "outducts" / "bpgen_one_ltp_port4556_thisengineid200.json").string();
    static const char * argsBpgen[] = { "bpgen", "--bundle-rate=100", "--my-uri-eid=ipn:1.1", "--dest-uri-eid=ipn:2.1","--duration=40", bpgenConfigArg.c_str(), NULL };
    std::thread threadBpgen(RunBpgenAsync,argsBpgen, 6, std::ref(runningBpgen), &bundlesSentBpgen[0], &finalStats[0]);

    // Allow time for data to flow
    boost::this_thread::sleep(boost::posix_time::seconds(8));

    // Stop threads
    runningBpgen = false;
    threadBpgen.join();

    runningHdtnOneProcess = false;
    threadHdtn.join();

    runningBpsink = false;
    threadBpsink.join();

    // Verify results
    uint64_t totalBundlesBpgen = 0;
    for (int i=0; i<1; i++) {
        totalBundlesBpgen += bundlesSentBpgen[i];
    }

    uint64_t totalBundlesBpsink = 0;
    for(int i=0; i<1; i++) {
        totalBundlesBpsink += bundlesReceivedBpsink[i];
    }


    uint64_t totalBundlesCount = bundleCountEgress + bundleCountStorage;

    if (bundleCountIngress != (totalBundlesCount)) {
        BOOST_ERROR("Total Bundles received by Ingress (" + std::to_string(bundleCountIngress) + 
	            ") != Total bundles received by Egress and Storage in Storage Mode ("
                    + std::to_string(totalBundlesCount) + ").");
        return false;
    }

    if (totalBundlesBpgen != totalBundlesBpsink) {
        BOOST_ERROR("Bundles sent by BpGen (" + std::to_string(totalBundlesBpgen) + ") != bundles received by BpSink "
                    + std::to_string(totalBundlesBpsink) + ").");
        return false;
    }

    if (totalBundlesBpgen != bundleCountIngress) {
        BOOST_ERROR("Bundles sent by BpGen (" + std::to_string(totalBundlesBpgen) + ") !=  bundles received by Ingress "
                + std::to_string(bundleCountIngress) + ").");
        return false;
    }

    if (totalBundlesBpgen != bundleCountEgress) {
        BOOST_ERROR("Bundles sent by BpGen (" + std::to_string(totalBundlesBpgen) + ") != bundles received by Egress "
                + std::to_string(bundleCountEgress) + ").");
        return false;
    }

    return true;
}

bool TestHDTNStorageModeLTPv7() {

    Delay(DELAY_TEST);

    bool runningBpgen = true;
    bool runningBpsink = true;
    bool runningHdtnOneProcess = true;

    uint64_t bundlesSentBpgen[1] = {0};
    OutductFinalStats finalStats[1];
    FinalStatsBpSink finalStatsBpSink[1];
    uint64_t bundlesReceivedBpsink[1]= {0};
    uint64_t bundleCountStorage = 0;
    uint64_t bundleCountEgress = 0;
    uint64_t bundleCountIngress = 0;

    // Start threads
    Delay(DELAY_THREAD);

    //bpsink
    static const std::string bpsinkConfigArg = "--inducts-config-file=" + (Environment::GetPathHdtnSourceRoot() / "config_files" / "inducts" / "bpsink_one_ltp_port4558.json").string();
    static const char * argsBpsink[] = { "bpsink",  "--my-uri-eid=ipn:2.1", bpsinkConfigArg.c_str(), NULL };
    std::thread threadBpsink(RunBpsinkAsync, argsBpsink, 3, std::ref(runningBpsink), &bundlesReceivedBpsink[0],
        &finalStatsBpSink[0]);

    Delay(DELAY_THREAD);

    //HDTN One Process
    static const std::string hdtnConfigArg = "--hdtn-config-file=" + (Environment::GetPathHdtnSourceRoot() / "config_files" / "hdtn" / "hdtn_ingress1ltp_port4556_egress1ltp_port4558flowid2.json").string();
    const boost::filesystem::path contactsFile = "contactPlanStorageMode.json";
    const std::string eventFileArg = "--contact-plan-file=" + contactsFile.string();
    const char * argsHdtnOneProcess[] = { "HdtnOneProcess", eventFileArg.c_str(), hdtnConfigArg.c_str(), NULL };
    std::thread threadHdtn(RunHdtnOneProcess, argsHdtnOneProcess, 3, std::ref(runningHdtnOneProcess), &bundleCountStorage,
                           &bundleCountEgress, &bundleCountIngress);

    Delay(10);

    //Bpgen
    static const std::string bpgenConfigArg =
        "--outducts-config-file=" + (Environment::GetPathHdtnSourceRoot() / "config_files" / "outducts" / "bpgen_one_ltp_port4556_thisengineid200.json").string();
    static const char * argsBpgen[] = { "bpgen", "--bundle-rate=100", "--my-uri-eid=ipn:1.1", "--dest-uri-eid=ipn:2.1","--duration=40", bpgenConfigArg.c_str(), NULL, "--use-bp-version-7"};
    std::thread threadBpgen(RunBpgenAsync,argsBpgen, 6, std::ref(runningBpgen), &bundlesSentBpgen[0], &finalStats[0]);

    // Allow time for data to flow
    boost::this_thread::sleep(boost::posix_time::seconds(8));

    // Stop threads
    runningBpgen = false;
    threadBpgen.join();

    runningHdtnOneProcess = false;
    threadHdtn.join();

    runningBpsink = false;
    threadBpsink.join();

    // Verify results
    uint64_t totalBundlesBpgen = 0;
    for (int i=0; i<1; i++) {
        totalBundlesBpgen += bundlesSentBpgen[i];
    }

    uint64_t totalBundlesBpsink = 0;
    for(int i=0; i<1; i++) {
        totalBundlesBpsink += bundlesReceivedBpsink[i];
    }


    uint64_t totalBundlesCount = bundleCountEgress + bundleCountStorage;

    if (bundleCountIngress != (totalBundlesCount)) {
        BOOST_ERROR("Total Bundles received by Ingress (" + std::to_string(bundleCountIngress) + 
	            ") != Total bundles received by Egress and Storage in Storage Mode ("
                    + std::to_string(totalBundlesCount) + ").");
        return false;
    }

    if (totalBundlesBpgen != totalBundlesBpsink) {
        BOOST_ERROR("Bundles sent by BpGen (" + std::to_string(totalBundlesBpgen) + ") != bundles received by BpSink "
                    + std::to_string(totalBundlesBpsink) + ").");
        return false;
    }

    if (totalBundlesBpgen != bundleCountIngress) {
        BOOST_ERROR("Bundles sent by BpGen (" + std::to_string(totalBundlesBpgen) + ") !=  bundles received by Ingress "
                + std::to_string(bundleCountIngress) + ").");
        return false;
    }

    if (totalBundlesBpgen != bundleCountEgress) {
        BOOST_ERROR("Bundles sent by BpGen (" + std::to_string(totalBundlesBpgen) + ") != bundles received by Egress "
                + std::to_string(bundleCountEgress) + ").");
        return false;
    }

    return true;
}


bool TestHDTNCutThroughModeTCPCL() {

    Delay(DELAY_TEST);

    bool runningBpgen = true;
    bool runningBpsink = true;
    bool runningHdtnOneProcess = true; 

    uint64_t bundlesSentBpgen[1] = {0};
    OutductFinalStats finalStats[1];
    FinalStatsBpSink finalStatsBpSink[1];
    uint64_t bundlesReceivedBpsink[1]= {0};
    uint64_t bundleCountStorage = 0;
    uint64_t bundleCountEgress = 0;
    uint64_t bundleCountIngress = 0;

    // Start threads
    Delay(DELAY_THREAD);


    //bpsink
    static const std::string bpsinkConfigArg = "--inducts-config-file=" + (Environment::GetPathHdtnSourceRoot() / "config_files" / "inducts" / "bpsink_one_tcpclv4_port4558.json").string();
    static const char * argsBpsink[] = { "bpsink",  "--my-uri-eid=ipn:2.1", bpsinkConfigArg.c_str(), NULL };
    std::thread threadBpsink(RunBpsinkAsync, argsBpsink, 3, std::ref(runningBpsink), &bundlesReceivedBpsink[0],
        &finalStatsBpSink[0]);

    Delay(DELAY_THREAD);

    //HDTN One Process
    static const std::string hdtnConfigArg = "--hdtn-config-file=" + (Environment::GetPathHdtnSourceRoot() / "config_files" /
 "hdtn" / "hdtn_ingress1tcpclv4_port4556_egress1tcpclv4_port4558flowid2.json").string();
    const boost::filesystem::path contactsFile = "contactPlanCutThroughMode.json";
    const std::string eventFileArg = "--contact-plan-file=" + contactsFile.string();
    const char * argsHdtnOneProcess[] = { "HdtnOneProcess", eventFileArg.c_str(), hdtnConfigArg.c_str(), NULL };
    std::thread threadHdtn(RunHdtnOneProcess, argsHdtnOneProcess, 3, std::ref(runningHdtnOneProcess), &bundleCountStorage,
		             &bundleCountEgress, &bundleCountIngress);

    Delay(10);

    //Bpgen
    static const std::string bpgenConfigArg = 
	"--outducts-config-file=" + (Environment::GetPathHdtnSourceRoot() / "config_files" / "outducts" / "bpgen_one_tcpclv4_port4556.json").string();
    static const char * argsBpgen[] = { "bpgen", "--bundle-rate=100", "--my-uri-eid=ipn:1.1", "--dest-uri-eid=ipn:2.1","--duration=40", bpgenConfigArg.c_str(), NULL };
    std::thread threadBpgen(RunBpgenAsync,argsBpgen, 6, std::ref(runningBpgen), &bundlesSentBpgen[0], &finalStats[0]);

    // Allow time for data to flow
    boost::this_thread::sleep(boost::posix_time::seconds(8));

    // Stop threads
    runningBpgen = false;
    threadBpgen.join();

    runningHdtnOneProcess = false;
    threadHdtn.join();

    runningBpsink = false;
    threadBpsink.join();

    // Verify results
    uint64_t totalBundlesBpgen = 0;
    for (int i=0; i<1; i++) {
        totalBundlesBpgen += bundlesSentBpgen[i];
    }
 
    uint64_t totalBundlesBpsink = 0;
    for(int i=0; i<1; i++) {
        totalBundlesBpsink += bundlesReceivedBpsink[i];
    }

    if (bundleCountIngress != bundleCountEgress) {
        BOOST_ERROR("Total Bundles received by Ingress (" + std::to_string(bundleCountIngress) + ") != Total bundles received by Egress in Cuthrough Mode "
                    + std::to_string(bundleCountEgress) + ").");
        return false;
    }

    if (totalBundlesBpgen != totalBundlesBpsink) {
        BOOST_ERROR("Bundles sent by BpGen (" + std::to_string(totalBundlesBpgen) + ") != bundles received by BpSink "
                    + std::to_string(totalBundlesBpsink) + ").");
	return false;
    }

    if (totalBundlesBpgen != bundleCountIngress) {
        BOOST_ERROR("Bundles sent by BpGen (" + std::to_string(totalBundlesBpgen) + ") !=  bundles received by Ingress "
                + std::to_string(bundleCountIngress) + ").");
        return false;
    }

    if (totalBundlesBpgen != bundleCountEgress) {
        BOOST_ERROR("Bundles sent by BpGen (" + std::to_string(totalBundlesBpgen) + ") != bundles received by Egress "
                + std::to_string(bundleCountEgress) + ").");
        return false;
    }
    
    return true;
}


bool TestHDTNStorageModeTCPCL() {

    Delay(DELAY_TEST);

    bool runningBpgen = true;
    bool runningBpsink = true;
    bool runningHdtnOneProcess = true;

    uint64_t bundlesSentBpgen[1] = {0};
    OutductFinalStats finalStats[1];
    FinalStatsBpSink finalStatsBpSink[1];
    uint64_t bundlesReceivedBpsink[1]= {0};
    uint64_t bundleCountStorage = 0;
    uint64_t bundleCountEgress = 0;
    uint64_t bundleCountIngress = 0;

    // Start threads
    Delay(DELAY_THREAD);

    //bpsink
    static const std::string bpsinkConfigArg = "--inducts-config-file=" + (Environment::GetPathHdtnSourceRoot() / "config_files" / "inducts" / "bpsink_one_tcpclv4_port4558.json").string();
    static const char * argsBpsink[] = { "bpsink",  "--my-uri-eid=ipn:2.1", bpsinkConfigArg.c_str(), NULL };
    std::thread threadBpsink(RunBpsinkAsync, argsBpsink, 3, std::ref(runningBpsink), &bundlesReceivedBpsink[0],
        &finalStatsBpSink[0]);

    Delay(DELAY_THREAD);

    //HDTN One Process
    static const std::string hdtnConfigArg = "--hdtn-config-file=" + (Environment::GetPathHdtnSourceRoot() / "config_files" /
 "hdtn" / "hdtn_ingress1tcpclv4_port4556_egress1tcpclv4_port4558flowid2.json").string();
    const boost::filesystem::path contactsFile = "contactPlanStorageMode.json";
    const std::string eventFileArg = "--contact-plan-file=" + contactsFile.string();
    const char * argsHdtnOneProcess[] = { "HdtnOneProcess", eventFileArg.c_str(), hdtnConfigArg.c_str(), NULL };
    std::thread threadHdtn(RunHdtnOneProcess, argsHdtnOneProcess, 3, std::ref(runningHdtnOneProcess), &bundleCountStorage,
                           &bundleCountEgress, &bundleCountIngress);

    Delay(10);

    //Bpgen
    static const std::string bpgenConfigArg =
        "--outducts-config-file=" + (Environment::GetPathHdtnSourceRoot() / "config_files" / "outducts" / "bpgen_one_tcpclv4_port4556.json").string();
    static const char * argsBpgen[] = { "bpgen", "--bundle-rate=100", "--my-uri-eid=ipn:1.1", "--dest-uri-eid=ipn:2.1","--duration=40", bpgenConfigArg.c_str(), NULL };
    std::thread threadBpgen(RunBpgenAsync,argsBpgen, 6, std::ref(runningBpgen), &bundlesSentBpgen[0], &finalStats[0]);

    // Allow time for data to flow
    boost::this_thread::sleep(boost::posix_time::seconds(8));

    // Stop threads
    runningBpgen = false;
    threadBpgen.join();

    runningHdtnOneProcess = false;
    threadHdtn.join();

    runningBpsink = false;
    threadBpsink.join();

    // Verify results
    uint64_t totalBundlesBpgen = 0;
    for (int i=0; i<1; i++) {
        totalBundlesBpgen += bundlesSentBpgen[i];
    }

    uint64_t totalBundlesBpsink = 0;
    for(int i=0; i<1; i++) {
        totalBundlesBpsink += bundlesReceivedBpsink[i];
    }

    uint64_t totalBundlesCount = bundleCountEgress + bundleCountStorage;

    if (bundleCountIngress != (totalBundlesCount)) {
        BOOST_ERROR("Total Bundles received by Ingress (" + std::to_string(bundleCountIngress) + 
	            ") != Total bundles received by Egress and Storage in Storage Mode ("
                    + std::to_string(totalBundlesCount) + ").");
        return false;
    }

    if (totalBundlesBpgen != totalBundlesBpsink) {
        BOOST_ERROR("Bundles sent by BpGen (" + std::to_string(totalBundlesBpgen) + ") != bundles received by BpSink "
                    + std::to_string(totalBundlesBpsink) + ").");
        return false;
    }

    if (totalBundlesBpgen != bundleCountIngress) {
        BOOST_ERROR("Bundles sent by BpGen (" + std::to_string(totalBundlesBpgen) + ") !=  bundles received by Ingress "
                + std::to_string(bundleCountIngress) + ").");
        return false;
    }

    if (totalBundlesBpgen != bundleCountEgress) {
        BOOST_ERROR("Bundles sent by BpGen (" + std::to_string(totalBundlesBpgen) + ") != bundles received by Egress "
                + std::to_string(bundleCountEgress) + ").");
        return false;
    }

    return true;
}

BOOST_GLOBAL_FIXTURE(BoostIntegratedTestsFixture);

BOOST_AUTO_TEST_CASE(it_TestHDTNCutThroughModeLTP, * boost::unit_test::enabled()) {
    std::cout << std::endl << ">>>>>> Running: " << "it_TestHDTNCutThroughModeLTP" << std::endl << std::flush;
    bool result = TestHDTNCutThroughModeLTP();
    BOOST_CHECK(result == true);
}

BOOST_AUTO_TEST_CASE(it_TestHDTNStorageModeLTP, * boost::unit_test::enabled()) {
    std::cout << std::endl << ">>>>>> Running: " << "it_TestHDTNStorageModeLTP" << std::endl << std::flush;
    bool result = TestHDTNStorageModeLTP();
    BOOST_CHECK(result == true);
}

BOOST_AUTO_TEST_CASE(it_TestHDTNCutThroughModeLTPv7, * boost::unit_test::enabled()) {
    std::cout << std::endl << ">>>>>> Running: " << "it_TestHDTNCutThroughModeLTP for version 7" << std::endl << std::flush;
    bool result = TestHDTNCutThroughModeLTP();
    BOOST_CHECK(result == true);
}

BOOST_AUTO_TEST_CASE(it_TestHDTNStorageModeLTPv7, * boost::unit_test::enabled()) {
    std::cout << std::endl << ">>>>>> Running: " << "it_TestHDTNStorageModeLTP for version 7" << std::endl << std::flush;
    bool result = TestHDTNStorageModeLTP();
    BOOST_CHECK(result == true);
}

/*
BOOST_AUTO_TEST_CASE(it_TestHDTNCutThroughModeTCPCL, * boost::unit_test::enabled()) {
    std::cout << std::endl << ">>>>>> Running: " << "it_TestHDTNCutThroughModeTCPCL" << std::endl << std::flush;
    bool result = TestHDTNCutThroughModeTCPCL();
    BOOST_CHECK(result == true);
}

BOOST_AUTO_TEST_CASE(it_TestHDTNStorageModeTCPCL, * boost::unit_test::enabled()) {
    std::cout << std::endl << ">>>>>> Running: " << "it_TestHDTNStorageModeTCPCL" << std::endl << std::flush;
    bool result = TestHDTNStorageModeTCPCL();
    BOOST_CHECK(result == true);
}*/
