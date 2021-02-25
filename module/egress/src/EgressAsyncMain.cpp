#include <iostream>
#include "EgressAsync.h"
#include "SignalHandler.h"
#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <boost/lexical_cast.hpp>
#include "reg.hpp"

static volatile bool g_running = true;

static void MonitorExitKeypressThreadFunction() {
    std::cout << "Keyboard Interrupt.. exiting\n";
    g_running = false; //do this first
}

static SignalHandler g_sigHandler(boost::bind(&MonitorExitKeypressThreadFunction));


int main(int argc, char* argv[]) {
    //scope to ensure clean exit before return 0
    {
        uint16_t port;
        bool useTcpcl = false;
        bool useStcp = false;

        boost::program_options::options_description desc("Allowed options");
        try {
                desc.add_options()
                        ("help", "Produce help message.")
                        ("port", boost::program_options::value<boost::uint16_t>()->default_value(4557), "Listen on this TCP or UDP port.")
                        ("use-stcp", "Use STCP instead of UDP.")
                        ("use-tcpcl", "Use TCP Convergence Layer Version 3 instead of UDP.")
                        ;

                boost::program_options::variables_map vm;
                boost::program_options::store(boost::program_options::parse_command_line(argc, argv, desc, boost::program_options::command_line_style::unix_style | boost::program_options::command_line_style::case_insensitive), vm);
                boost::program_options::notify(vm);

                if (vm.count("help")) {
                        std::cout << desc << "\n";
                        return 1;
                }

                if (vm.count("use-tcpcl")) {
                    useTcpcl = true;
                }
                if (vm.count("use-stcp")) {
                    useStcp = true;
                }
                if (useTcpcl && useStcp) {
                    std::cerr << "ERROR: cannot use both tcpcl and stcp" << std::endl;
                    return 1;
                }

                port = vm["port"].as<boost::uint16_t>();
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


        std::cout << "starting EgressAsync.." << std::endl;
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
        hdtn::HegrManagerAsync egress;
        egress.Init();
        int entryStatus;
        entryStatus = egress.Add(1, (useTcpcl) ? HEGR_FLAG_TCPCLv3 : (useStcp) ? HEGR_FLAG_STCPv1 : HEGR_FLAG_UDP, "127.0.0.1", port);
        if (!entryStatus) {
            return 0;  // error message prints in add function
        }
        printf("Announcing presence of egress ...\n");
        for (int i = 0; i < 8; ++i) {
            egress.Up(i);
        }

        g_sigHandler.Start(false);
        std::cout << "egress up and running" << std::endl;
        while (g_running) {
            boost::this_thread::sleep(boost::posix_time::millisec(250));
            g_sigHandler.PollOnce();
        }

       std::cout << "Msg Count, Bundle Count, Bundle data bytes\n";

        std::cout << egress.m_messageCount << "," << egress.m_bundleCount << "," << egress.m_bundleData << "\n";


        std::cout<< "EgressAsyncMain.cpp: exiting cleanly..\n";
    }
    std::cout<< "EgressAsyncMain: exited cleanly\n";
    return 0;

}
