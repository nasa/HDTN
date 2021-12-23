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
#include "IngressAsyncRunner.h"
#include "EgressAsyncRunner.h"
#include "StorageRunner.h"
#include "SignalHandler.h"
#include "ReleaseSender.h"
#include "scheduler.h"

#define DELAY_THREAD 3
#define DELAY_TEST 3
#define MAX_RATE "--stcp-rate-bits-per-sec=30000"
#define MAX_RATE_DIV_3 "--stcp-rate-bits-per-sec=10000"
#define MAX_RATE_DIV_6 "--stcp-rate-bits-per-sec=5000"

// Prototypes

int RunBpgenAsync(const char * argv[], int argc, bool & running, uint64_t* ptrBundleCount, OutductFinalStats * ptrFinalStats);
int RunEgressAsync(const char * argv[], int argc, bool & running, uint64_t* ptrBundleCount);
int RunBpsinkAsync(const char * argv[], int argc, bool & running, uint64_t* ptrBundleCount, FinalStatsBpSink * ptrFinalStatsBpSink);
int RunIngress(const char * argv[], int argc, bool & running, uint64_t* ptrBundleCount);
int RunStorage(const char * argv[], int argc, bool & running, uint64_t* ptrBundleCount);
int RunScheduler(const char * argv[], int argc, bool & running, std::string jsonFile);
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

int RunEgressAsync(const char * argv[], int argc, bool & running, uint64_t* ptrBundleCount) {
    {
        EgressAsyncRunner runner;
        runner.Run(argc, argv, running, false);
        *ptrBundleCount = runner.m_bundleCount;
    }
    return 0;
}

int RunBpsinkAsync(const char * argv[], int argc, bool & running, uint64_t* ptrBundleCount, FinalStatsBpSink * ptrFinalStatsBpSink) {
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

int RunScheduler(const char * argv[], int argc, bool & running, std::string jsonFileName) {
    {
        Scheduler runner;
        runner.Run(argc, argv, running, jsonFileName, true);
    }
    return 0;
}

bool TestSchedulerTcpcl() {

    Delay(DELAY_TEST);

    bool runningBpgen[2] = {true,true};
    bool runningBpsink[2] = {true,true};
    bool runningIngress = true;
    bool runningEgress = true;
    bool runningStorage = true;
    bool runningScheduler = true; 
    uint64_t bundlesSentBpgen[2] = {0,0};
    OutductFinalStats finalStats[2];
    FinalStatsBpSink finalStatsBpSink[2];
    uint64_t bundlesReceivedBpsink[2] = {0,0};
    uint64_t bundleCountEgress = 0;
    uint64_t bundleCountIngress = 0;
    uint64_t bundleCountStorage = 0;


    // Start threads
    Delay(DELAY_THREAD);

    //bpsink1
    static const std::string bpsinkConfigArg0 = "--inducts-config-file=" + (Environment::GetPathHdtnSourceRoot() / "tests" / "config_files" / "inducts" / "bpsink_one_tcpcl_port4557.json").string();
    static const char * argsBpsink0[] = { "bpsink",  "--my-uri-eid=ipn:1.1", bpsinkConfigArg0.c_str(), NULL };
    std::thread threadBpsink0(RunBpsinkAsync, argsBpsink0, 3, std::ref(runningBpsink[0]), &bundlesReceivedBpsink[0],
        &finalStatsBpSink[0]);
    Delay(DELAY_THREAD);

    //bpsink2
    static const std::string bpsinkConfigArg1 = "--inducts-config-file=" + (Environment::GetPathHdtnSourceRoot() / "tests" / "config_files" / "inducts" / "bpsink_one_tcpcl_port4558.json").string();
    static const char * argsBpsink1[] = { "bpsink", "--my-uri-eid=ipn:2.1", bpsinkConfigArg1.c_str(), NULL };
    std::thread threadBpsink1(RunBpsinkAsync, argsBpsink1, 3, std::ref(runningBpsink[1]), &bundlesReceivedBpsink[1],
        &finalStatsBpSink[1]);
    Delay(DELAY_THREAD);

    //Egress
    static const std::string hdtnConfigArg = "--hdtn-config-file=" + (Environment::GetPathHdtnSourceRoot() / "tests" / "config_files" / "hdtn" / "hdtn_ingress1tcpcl_port4556_egress2tcpcl_port4557flowid1_port4558flowid2.json").string();
    static const char * argsEgress[] = { "egress",hdtnConfigArg.c_str(), NULL };
    std::thread threadEgress(RunEgressAsync,argsEgress,2,std::ref(runningEgress),&bundleCountEgress);
    Delay(DELAY_THREAD);

    //Ingress
    static const char * argsIngress[] = { "ingress", hdtnConfigArg.c_str(), NULL };
    std::thread threadIngress(RunIngress,argsIngress,2,std::ref(runningIngress),&bundleCountIngress);
    Delay(DELAY_THREAD);


    //Storage
    static const char * argsStorage[] = { "storage",hdtnConfigArg.c_str(),NULL };
    StorageRunner storageRunner;
    std::thread threadStorage(&StorageRunner::Run, &storageRunner, 2, argsStorage, std::ref(runningStorage), false);
    Delay(DELAY_THREAD);

    //scheduler
    Scheduler scheduler;
    std::string contactsFile = "contactPlan.json";
    static const std::string eventFileArg = "--contact-plan-file=" + contactsFile;

    std::string jsonFileName =  Scheduler::GetFullyQualifiedFilename(contactsFile);
    if ( !boost::filesystem::exists( jsonFileName ) ) {
        std::cerr << "ContactPlan File not found: " << jsonFileName << std::endl << std::flush;
        return false;
    }

    static const char * argsScheduler[] = { "scheduler", eventFileArg.c_str(), hdtnConfigArg.c_str(), NULL };
    std::thread threadScheduler(&Scheduler::Run, &scheduler, 3, argsScheduler, std::ref(runningScheduler), jsonFileName, true);
    Delay(1);

    
    //Bpgen1
   static const std::string bpgenConfigArg = "--outducts-config-file=" + (Environment::GetPathHdtnSourceRoot() / "tests" / "config_files" / "outducts" / "bpgen_one_tcpcl_port4556.json").string();
    static const char * argsBpgen1[] = { "bpgen", "--bundle-rate=100", "--my-uri-eid=ipn:101.1", "--dest-uri-eid=ipn:1.1","--duration=40", bpgenConfigArg.c_str(), NULL };
    std::thread threadBpgen1(RunBpgenAsync,argsBpgen1, 6,std::ref(runningBpgen[1]),&bundlesSentBpgen[1],&finalStats[1]);
    Delay(1);

    //Bpgen2
    static const char * argsBpgen0[] = { "bpgen", "--bundle-rate=100", "--my-uri-eid=ipn:102.1", "--dest-uri-eid=ipn:2.1","--duration=40", bpgenConfigArg.c_str(), NULL };
    std::thread threadBpgen0(RunBpgenAsync,argsBpgen0, 6,std::ref(runningBpgen[0]),&bundlesSentBpgen[0],&finalStats[0]);


    // Allow time for data to flow
    boost::this_thread::sleep(boost::posix_time::seconds(8));

    // Stop threads
    runningBpgen[0] = false;
    runningBpgen[1] = false;
    threadBpgen0.join();
    threadBpgen1.join();

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
    runningScheduler = false;
    threadScheduler.join();

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

   if (totalBundlesBpgen != (bundleCountEgress)) {
        BOOST_ERROR("Bundles sent by BPGEN (" + std::to_string(totalBundlesBpgen) + ") != bundles sent by egress "
                     + std::to_string(bundleCountStorage) + ").");
       return false;
     }
   //  if (totalBundlesBpgen != totalBundlesBpsink) {
     //    BOOST_ERROR("Bundles sent by BPGEN (" + std::to_string(totalBundlesBpgen) + ") != bundles received by BPSINK "
       //         + std::to_string(totalBundlesBpsink) + ").");
         // return false;
    //}
    if (totalBundlesBpgen != totalBundlesAckedBpgen) {
        BOOST_ERROR("Bundles sent by BPGEN (" + std::to_string(totalBundlesBpgen) + ") != bundles acked by BPGEN "
                + std::to_string(totalBundlesAckedBpgen) + ").");
        return false;
    }
    //if (totalBundlesBpgen != totalBundlesAckedBpsink) {
      //  BOOST_ERROR("Bundles sent by BPGEN (" + std::to_string(totalBundlesBpgen) + ") != bundles acked by BPSINK "
        //       + std::to_string(totalBundlesAckedBpsink) + ").");
        //return false;
   //}
    return true;
}

BOOST_GLOBAL_FIXTURE(BoostIntegratedTestsFixture);

BOOST_AUTO_TEST_CASE(it_TestSchedulerTcpcl, * boost::unit_test::enabled()) {
    std::cout << std::endl << ">>>>>> Running: " << "it_TestSchedulerTcpcl" << std::endl << std::flush;
    bool result = TestSchedulerTcpcl();
    BOOST_CHECK(result == true);
}
