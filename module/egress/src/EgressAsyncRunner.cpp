#include <iostream>
#include "EgressAsync.h"
#include "EgressAsyncRunner.h"
#include "SignalHandler.h"
#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <boost/lexical_cast.hpp>
#include "reg.hpp"


void EgressAsyncRunner::MonitorExitKeypressThreadFunction() {
    std::cout << "Keyboard Interrupt.. exiting\n";
    m_runningFromSigHandler = false; //do this first
}


EgressAsyncRunner::EgressAsyncRunner() {}
EgressAsyncRunner::~EgressAsyncRunner() {}


bool EgressAsyncRunner::Run(int argc, const char* const argv[], volatile bool & running, bool useSignalHandler) {
    //scope to ensure clean exit before return 0
    {
        running = true;
        m_runningFromSigHandler = true;
        SignalHandler sigHandler(boost::bind(&EgressAsyncRunner::MonitorExitKeypressThreadFunction, this));
        uint16_t portLink1, portLink2;
        bool useTcpcl = false;
        bool useStcp = false;

        boost::program_options::options_description desc("Allowed options");
        try {
            desc.add_options()
                ("help", "Produce help message.")
                ("port1", boost::program_options::value<boost::uint16_t>()->default_value(4557), "Connect FlowId 1 to this TCP or UDP port (0=>disabled).")
                ("port2", boost::program_options::value<boost::uint16_t>()->default_value(4558), "Connect FlowId 2 to this TCP or UDP port (0=>disabled).")
                ("use-stcp", "Use STCP instead of UDP.")
                ("use-tcpcl", "Use TCP Convergence Layer Version 3 instead of UDP.")
                ;

            boost::program_options::variables_map vm;
            boost::program_options::store(boost::program_options::parse_command_line(argc, argv, desc, boost::program_options::command_line_style::unix_style | boost::program_options::command_line_style::case_insensitive), vm);
            boost::program_options::notify(vm);

            if (vm.count("help")) {
                std::cout << desc << "\n";
                return false;
            }

            if (vm.count("use-tcpcl")) {
                useTcpcl = true;
            }
            if (vm.count("use-stcp")) {
                useStcp = true;
            }
            if (useTcpcl && useStcp) {
                std::cerr << "ERROR: cannot use both tcpcl and stcp" << std::endl;
                return false;
            }

            portLink1 = vm["port1"].as<boost::uint16_t>();
            portLink2 = vm["port2"].as<boost::uint16_t>();
        }
        catch (boost::bad_any_cast & e) {
            std::cout << "invalid data error: " << e.what() << "\n\n";
            std::cout << desc << "\n";
            return false;
        }
        catch (std::exception& e) {
            std::cerr << "error: " << e.what() << "\n";
            return false;
        }
        catch (...) {
            std::cerr << "Exception of unknown type!\n";
            return false;
        }


        std::cout << "starting EgressAsync.." << std::endl;
        hdtn::HdtnRegsvr regsvr;
        regsvr.Init(HDTN_REG_SERVER_PATH, "egress", 10100, "PULL");
        regsvr.Reg();
        if (hdtn::HdtnEntries_ptr res = regsvr.Query()) {
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
        if (portLink1) {
            entryStatus = egress.Add(1, (useTcpcl) ? HEGR_FLAG_TCPCLv3 : (useStcp) ? HEGR_FLAG_STCPv1 : HEGR_FLAG_UDP, "127.0.0.1", portLink1);
        }
        if (portLink2) {
            entryStatus = egress.Add(2, (useTcpcl) ? HEGR_FLAG_TCPCLv3 : (useStcp) ? HEGR_FLAG_STCPv1 : HEGR_FLAG_UDP, "127.0.0.1", portLink2);
        }
        if (!entryStatus) {
            return 0;  // error message prints in add function
        }
        printf("Announcing presence of egress ...\n");
        for (int i = 0; i < 8; ++i) {
            egress.Up(i);
        }

        if (useSignalHandler) {
            sigHandler.Start(false);
        }
        std::cout << "egress up and running" << std::endl;
        while (running && m_runningFromSigHandler) {
            boost::this_thread::sleep(boost::posix_time::millisec(250));
            if (useSignalHandler) {
                sigHandler.PollOnce();
            }
        }

        


        std::cout << "EgressAsyncRunner: exiting cleanly..\n";
        egress.Stop();
        m_bundleCount = egress.m_bundleCount;
        m_bundleData = egress.m_bundleData;
        m_messageCount = egress.m_messageCount;
    }
    std::cout << "EgressAsyncRunner: exited cleanly\n";
    return true;
}
