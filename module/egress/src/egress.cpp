#include "egress.h"

#include <signal.h>
#include <sys/time.h>

#include <fstream>
#include <iostream>

#include "logging.hpp"
#include "message.hpp"
#include "reg.hpp"

//using namespace hdtn;
//using namespace std;

static uint64_t bundleCount = 0;
static uint64_t bundleData = 0;
static uint64_t messageCount = 0;
static double elapsed = 0;
static uint64_t start;

static void SignalHandler(int signalValue) {
    (void) signalValue; //unused parameter
    std::ofstream output;
    output.open("egress-" + hdtn::Datetime());
    output << "Elapsed, Bundle Count (M), Rate (Mbps),Bundles/sec,Message Count "
              "(M)\n";
    double rate = 8 * ((bundleData / (double)(1024 * 1024)) / elapsed);
    output << elapsed << ", " << bundleCount / 1000000.0f << ", " << rate << ", " << bundleCount / elapsed << ","
           << messageCount / 1000000.0f << "\n";
    output.close();
    exit(EXIT_SUCCESS);
}

static void CatchSignals(void) {
    struct sigaction action;
    action.sa_handler = SignalHandler;
    action.sa_flags = 0;
    sigemptyset(&action.sa_mask);
    sigaction(SIGINT, &action, NULL);
    sigaction(SIGTERM, &action, NULL);
}

int main(int argc, char *argv[]) {
    hdtn::HegrManager egress;
    bool ok = true;
    struct timeval tv;
    gettimeofday(&tv, NULL);
    double start = ((double)tv.tv_sec) + ((double)tv.tv_usec / 1000000.0);
    printf("Start: +%f\n", start);
    CatchSignals();
    // finish registration stuff - egress should register, ingress will query
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
        return 1;
    }

    egress.Init();
    int entryStatus;
    entryStatus = egress.Add(1, HEGR_FLAG_UDP, "127.0.0.1", 4557);
    if (!entryStatus) {
        return 0;  // error message prints in add function
    }
    printf("Announcing presence of egress ...\n");
    for (int i = 0; i < 8; ++i) {
        egress.Up(i);
    }
    char bundle[HMSG_MSG_MAX];
    int bundleSize = 0;
    int numFrames = 0;
    int frameIndex = 0;
    int maxFrames = 0;

    char *type;
    size_t payloadSize;
    while (true) {
        gettimeofday(&tv, NULL);
        elapsed = ((double)tv.tv_sec) + ((double)tv.tv_usec / 1000000.0);
        elapsed -= start;
        zmq::message_t hdr;
        zmq::message_t message;
        egress.m_zmqCutThroughSock->recv(hdr, zmq::recv_flags::none);
        messageCount++;
        char bundle[HMSG_MSG_MAX];
        if (hdr.size() < sizeof(hdtn::CommonHdr)) {
            std::cerr << "[dispatch] message too short: " << hdr.size() << std::endl;
            return -1;
        }
        hdtn::CommonHdr *common = (hdtn::CommonHdr *)hdr.data();
        //hdtn::BlockHdr *block = (hdtn::BlockHdr *)common;
        switch (common->type) {
            case HDTN_MSGTYPE_STORE:
                egress.m_zmqCutThroughSock->recv(message, zmq::recv_flags::none);
                bundleSize = message.size();
                memcpy(bundle, message.data(), bundleSize);
                egress.Forward(1, bundle, bundleSize);
                bundleData += bundleSize;
                bundleCount++;
                break;
        }
    }

    return 0;
}
