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

ofstream output;
static void signal_handler(int signal_value) {
    //changed from calculating stats this way. If bundles aren't received the time is
    //still counted in elapsed, with will make the rate seem lower that it actually is
    // ofstream output;
    // output.open("egress-" + datetime());
    // output << "Elapsed, Bundle Count (M), Rate (Mbps),Bundles/sec,Message Count (M)\n";
    //double rate = 8 * ((egress.bundle_data / (double)(1024 * 1024)) / egress.elapsed);
    // output << egress.elapsed << ", " << egress.bundle_count / 1000000.0f << ", " << rate << ", " << egress.bundle_count / egress.elapsed <<  "\n";
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
    double last = 0.0;
    timeval tv;
    gettimeofday(&tv, NULL);

    output.open("egress-" + datetime());
    last = (tv.tv_sec + (tv.tv_usec / 1000000.0));
    double start = ((double)tv.tv_sec) + ((double)tv.tv_usec / 1000000.0);
    output << "Elapsed, Bundle Count, Rate (Mbps),Total Bytes\n";
    printf("Start: +%f\n", start);
    catch_signals();

    //finish registration stuff - egress should register, ingress will query
    hdtn::hdtn_regsvr regsvr;
    regsvr.init(HDTN_REG_SERVER_PATH, "egress", 10120, "PULL");
    regsvr.reg();
    hdtn::hdtn_entries res = regsvr.query();
    for (auto entry : res) {
        std::cout << entry.address << ":" << entry.port << ":" << entry.mode << std::endl;
    }
    hegr_manager egress;
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
    uint64_t last_bytes = 0;
    uint64_t last_count = 0;
    uint64_t cbytes = 0;
    uint64_t ccount = 0;
    while (true) {
        egress.update();
        gettimeofday(&tv, NULL);
        double curr = (tv.tv_sec + (tv.tv_usec / 1000000.0));
        if (curr - last > 1) {
            last = curr;
            cbytes = egress.bundle_data - last_bytes;
            ccount = egress.bundle_count - last_count;
            last_bytes = egress.bundle_data;
            last_count = egress.bundle_count;
            //only writes to file if data was received
            if (ccount > 0) {
                output << egress.elapsed << ", " << ccount << ", " << (cbytes * 8) / double(1024 * 1024) << ", " << egress.bundle_data << "\n";
            }
        }
    }
    return 0;
}
