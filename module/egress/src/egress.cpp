#include "egress.h"

#include <signal.h>
#include <sys/time.h>

#include <fstream>
#include <iostream>

#include "logging.hpp"
#include "message.hpp"
#include "reg.hpp"
using namespace hdtn;
using namespace std;

static uint64_t bundle_count = 0;
static uint64_t bundle_data = 0;
static uint64_t message_count = 0;
static double elapsed = 0;
static uint64_t start;

static void signal_handler(int signal_value) {
    ofstream output;
    output.open("egress-" + datetime());
    output << "Elapsed, Bundle Count (M), Rate (Mbps),Bundles/sec,Message Count (M)\n";
    double rate = 8 * ((bundle_data / (double)(1024 * 1024)) / elapsed);
    output << elapsed << ", " << bundle_count / 1000000.0f << ", " << rate << ", " << bundle_count / elapsed << "," << message_count / 1000000.0f << "\n";
    output.close();
    exit(EXIT_SUCCESS);
}

static void catch_signals(void) {
    struct sigaction action;
    action.sa_handler = signal_handler;
    action.sa_flags = 0;
    sigemptyset(&action.sa_mask);
    sigaction(SIGINT, &action, NULL);
    sigaction(SIGTERM, &action, NULL);
}

int main(int argc, char *argv[]) {
    hegr_manager egress;
    bool ok = true;
    struct timeval tv;
    gettimeofday(&tv, NULL);
    double start = ((double)tv.tv_sec) + ((double)tv.tv_usec / 1000000.0);
    printf("Start: +%f\n", start);
    catch_signals();
    //finish registration stuff - egress should register, ingress will query
    hdtn::hdtn_regsvr regsvr;
    regsvr.init(HDTN_REG_SERVER_PATH, "egress", 10100, "PULL");
    regsvr.reg();
    hdtn::hdtn_entries res = regsvr.query();
    for (auto entry : res) {
        std::cout << entry.address << ":" << entry.port << ":" << entry.mode << std::endl;
    }
    
    
    egress.init();
    int entry_status;
    entry_status = egress.add(1, HEGR_FLAG_UDP, "127.0.0.1", 4557);
    if (!entry_status) {
        return 0;  //error message prints in add function
    }
    printf("Announcing presence of egress ...\n");
    for (int i = 0; i < 8; ++i) {
        egress.up(i);
    }
    char bundle[HMSG_MSG_MAX];
    int bundle_size = 0;
    int num_frames = 0;
    int frame_index = 0;
    int max_frames = 0;

    char *type;
    size_t payload_size;
    while (true) {
        gettimeofday(&tv, NULL);
        elapsed = ((double)tv.tv_sec) + ((double)tv.tv_usec / 1000000.0);
        elapsed -= start;
        zmq::message_t hdr;
        zmq::message_t message;
        egress.zmqCutThroughSock->recv(&hdr);
        message_count++;
        char bundle[HMSG_MSG_MAX];
        if (hdr.size() < sizeof(hdtn::common_hdr)) {
            std::cerr << "[dispatch] message too short: " << hdr.size() << std::endl;
            return -1;
        }
        hdtn::common_hdr *common = (hdtn::common_hdr *)hdr.data();
        hdtn::block_hdr *block = (hdtn::block_hdr *)common;
        switch (common->type) {
            case HDTN_MSGTYPE_STORE:
                egress.zmqCutThroughSock->recv(&message);
                bundle_size = message.size();
                memcpy(bundle, message.data(), bundle_size);
                egress.forward(1, bundle, bundle_size);
                bundle_data += bundle_size;
                bundle_count++;
                break;
        }
    }

    return 0;
}