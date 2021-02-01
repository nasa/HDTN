#include "ingress.h"

#include <assert.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>

#include <fstream>
#include <iostream>

#include "logging.hpp"
#include "message.hpp"
#include "reg.hpp"

#define BP_INGRESS_TELEM_FREQ (0.10)
#define INGRESS_PORT (4556)

//using namespace hdtn;
//using namespace std;
//using namespace zmq;
static hdtn::BpIngress ingress;

static void SSignalHandler(int signalValue) {
    // s_interrupted = 1;
    std::ofstream output;
    std::string currentDate = hdtn::Datetime();
    output.open("ingress-" + currentDate);
    output << "Elapsed, Bundle Count (M),Rate (Mbps),Bundles/sec, Bundle Data "
              "(MB)\n";
    double rate = 8 * ((ingress.m_bundleData / (double)(1024 * 1024)) / ingress.m_elapsed);
    output << ingress.m_elapsed << "," << ingress.m_bundleCount / 1000000.0f << "," << rate << ","
           << ingress.m_bundleCount / ingress.m_elapsed << ", " << ingress.m_bundleData / (double)(1024 * 1024) << "\n";
    output.close();
    exit(EXIT_SUCCESS);
}

static void SCatchSignals(void) {
    struct sigaction action;
    action.sa_handler = SSignalHandler;
    action.sa_flags = 0;
    sigemptyset(&action.sa_mask);
    sigaction(SIGINT, &action, NULL);
    sigaction(SIGTERM, &action, NULL);
}

int main(int argc, char *argv[]) {
    ingress.Init(BP_INGRESS_TYPE_UDP);
    uint64_t lastTime = 0;
    uint64_t currTime = 0;
    // finish registration stuff -ingress will find out what egress services have
    // registered
    hdtn::HdtnRegsvr regsvr;
    regsvr.Init(HDTN_REG_SERVER_PATH, "ingress", 10100, "PUSH");
    regsvr.Reg();
    hdtn::HdtnEntries res = regsvr.Query();
    for (auto entry : res) {
        std::cout << entry.address << ":" << entry.port << ":" << entry.mode << std::endl;
    }
    SCatchSignals();
    printf("Announcing presence of ingress engine ...\n");

    ingress.Netstart(INGRESS_PORT);
    int count = 0;
    struct timeval tv;
    gettimeofday(&tv, NULL);
    double start = ((double)tv.tv_sec) + ((double)tv.tv_usec / 1000000.0);
    printf("Start: +%f\n", start);
    while (true) {
        currTime = time(0);
        gettimeofday(&tv, NULL);
        ingress.m_elapsed = ((double)tv.tv_sec) + ((double)tv.tv_usec / 1000000.0);
        ingress.m_elapsed -= start;
        count = ingress.Update();
        ingress.Process(count);
        lastTime = currTime;
    }

    exit(EXIT_SUCCESS);
}
