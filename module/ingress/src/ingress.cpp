#include "ingress.h"

#include <assert.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include <fstream>
#include <iostream>

#include "logging.hpp"
#include "message.hpp"
#include "reg.hpp"
#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <boost/lexical_cast.hpp>

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
        bool useStcp = false;

        boost::program_options::options_description desc("Allowed options");
        try {
            desc.add_options()
                ("help", "Produce help message.")
                //("port", boost::program_options::value<boost::uint16_t>()->default_value(4557), "Listen on this TCP or UDP port.")
                ("use-stcp", "Use STCP instead of TCPCL.")
                ;

            boost::program_options::variables_map vm;
            boost::program_options::store(boost::program_options::parse_command_line(argc, argv, desc, boost::program_options::command_line_style::unix_style | boost::program_options::command_line_style::case_insensitive), vm);
            boost::program_options::notify(vm);

            if (vm.count("help")) {
                std::cout << desc << "\n";
                return 1;
            }

            
            if (vm.count("use-stcp")) {
                useStcp = true;
            }

            //port = vm["port"].as<boost::uint16_t>();
        }
        catch (boost::bad_any_cast & e) {
            std::cout << "invalid data error: " << e.what() << "\n\n";
            std::cout << desc << "\n";
            return 1;
        }
        catch (std::exception& e) {
            std::cerr << "error: " << e.what() << "\n";
            return 1;
        }
        catch (...) {
            std::cerr << "Exception of unknown type!\n";
            return 1;
        }


        std::cout << "starting ingress.." << std::endl;
        hdtn::BpIngress ingress;
        ingress.Init(BP_INGRESS_TYPE_UDP);

        // finish registration stuff -ingress will find out what egress services have
        // registered
        hdtn::HdtnRegsvr regsvr;
        regsvr.Init(HDTN_REG_SERVER_PATH, "ingress", 10100, "PUSH");
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


        printf("Announcing presence of ingress engine ...\n");

        ingress.Netstart(INGRESS_PORT, !useStcp, useStcp);

        g_sigHandler.Start(false);
        std::cout << "ingress up and running" << std::endl;
        while (g_running) {
            boost::this_thread::sleep(boost::posix_time::millisec(250));
            ingress.RemoveInactiveTcpConnections();
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
