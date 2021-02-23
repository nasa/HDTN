#include <iostream>
#include "BpSinkAsync.h"
#include "SignalHandler.h"
#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <boost/lexical_cast.hpp>

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
        std::string thisLocalEidString;

        boost::program_options::options_description desc("Allowed options");
        try {
                desc.add_options()
                        ("help", "Produce help message.")
                        ("port", boost::program_options::value<boost::uint16_t>()->default_value(4557), "Listen on this TCP or UDP port.")
                        ("use-tcpcl", "Use TCP Convergence Layer Version 3 instead of UDP.")
                        ("tcpcl-eid", boost::program_options::value<std::string>()->default_value("BpSink"), "Local EID string for this program.")
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

                port = vm["port"].as<boost::uint16_t>();
                thisLocalEidString = vm["tcpcl-eid"].as<std::string>();
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


        std::cout << "starting BpSink.." << std::endl;
        hdtn::BpSinkAsync bpSink(port, useTcpcl, thisLocalEidString);
        bpSink.Init(0);



        bpSink.Netstart();

        g_sigHandler.Start(false);
        std::cout << "BpSink up and running" << std::endl;
        while (g_running) {
            boost::this_thread::sleep(boost::posix_time::millisec(250));
            g_sigHandler.PollOnce();
        }

       std::cout << "Rx Count, Duplicate Count, Total bytes Rx\n";

        std::cout << bpSink.m_receivedCount << "," << bpSink.m_duplicateCount << "," << bpSink.m_totalBytesRx << "\n";


        std::cout<< "BpSinkAsyncMain.cpp: exiting cleanly..\n";
    }
    std::cout<< "BpSinkAsyncMain: exited cleanly\n";
    return 0;

}
