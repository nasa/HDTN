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
#include "SignalHandler.h"

static volatile bool g_running = true;

static void MonitorExitKeypressThreadFunction() {
    std::cout << "Keyboard Interrupt.. exiting\n";
    g_running = false; //do this first
}

static SignalHandler g_sigHandler(boost::bind(&MonitorExitKeypressThreadFunction));



int main(int argc, char *argv[]) {

    //scope to ensure clean exit before return 0
    {
        std::cout << "starting ingress.." << std::endl;
        hdtn::BpIngress ingress;
        ingress.Init(BP_INGRESS_TYPE_UDP);

        // finish registration stuff -ingress will find out what egress services have
        // registered
        hdtn::HdtnRegsvr regsvr;
        regsvr.Init(HDTN_REG_SERVER_PATH, "ingress", 10100, "PUSH");
        regsvr.Reg();
        hdtn::HdtnEntries res = regsvr.Query();
        for (auto entry : res) {
            std::cout << entry.address << ":" << entry.port << ":" << entry.mode << std::endl;
        }

        printf("Announcing presence of ingress engine ...\n");

        ingress.Netstart(INGRESS_PORT);

        g_sigHandler.Start(false);
        std::cout << "ingress up and running" << std::endl;
        while (g_running) {
            boost::this_thread::sleep(boost::posix_time::millisec(250));
            g_sigHandler.PollOnce();
        }

        std::string currentDate = hdtn::Datetime();
        std::ofstream output;
        output.open("ingress-" + currentDate);

        std::ostringstream oss;
        oss << "Elapsed, Bundle Count (M),Rate (Mbps),Bundles/sec, Bundle Data "
                  "(MB)\n";
        double rate = 8 * ((ingress.m_bundleData / (double)(1024 * 1024)) / ingress.m_elapsed);
        oss << ingress.m_elapsed << "," << ingress.m_bundleCount / 1000000.0f << "," << rate << ","
               << ingress.m_bundleCount / ingress.m_elapsed << ", " << ingress.m_bundleData / (double)(1024 * 1024) << "\n";

        std::cout << oss.str();
        output << oss.str();
        output.close();

        std::cout<< "Ingress main.cpp: exiting cleanly..\n";
    }
    std::cout<< "Ingress main.cpp: exited cleanly\n";
    return 0;

}
