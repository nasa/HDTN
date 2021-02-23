#include <iostream>
#include "BpGenAsync.h"
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
        std::string destinationAddress;
        std::string thisLocalEidString;
        uint16_t port;
        bool useTcpcl = false;
        uint32_t bundleSizeBytes;
        uint32_t bundleRate;
        uint32_t tcpclFragmentSize;

        boost::program_options::options_description desc("Allowed options");
        try {
                desc.add_options()
                        ("help", "Produce help message.")
                        ("dest", boost::program_options::value<std::string>()->default_value("localhost"), "Send bundles to this TCP or UDP destination address.")
                        ("port", boost::program_options::value<boost::uint16_t>()->default_value(4556), "Send bundles to this TCP or UDP destination port.")
                        ("bundle-size", boost::program_options::value<boost::uint32_t>()->default_value(100), "Bundle size bytes.")
                        ("bundle-rate", boost::program_options::value<boost::uint32_t>()->default_value(1500), "Bundle rate.")
                        ("use-tcpcl", "Use TCP Convergence Layer Version 3 instead of UDP.")
                        ("tcpcl-fragment-size", boost::program_options::value<boost::uint32_t>()->default_value(0), "Max fragment size bytes of Tcpcl bundles (0->disabled).")
                        ("tcpcl-eid", boost::program_options::value<std::string>()->default_value("BpGen"), "Local EID string for this program.")
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
                destinationAddress = vm["dest"].as<std::string>();
                port = vm["port"].as<boost::uint16_t>();
                bundleSizeBytes = vm["bundle-size"].as<boost::uint32_t>();
                bundleRate = vm["bundle-rate"].as<boost::uint32_t>();
                tcpclFragmentSize = vm["tcpcl-fragment-size"].as<boost::uint32_t>();
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


        std::cout << "starting BpGenAsync.." << std::endl;

        BpGenAsync bpGen;
        bpGen.Start(destinationAddress, boost::lexical_cast<std::string>(port), useTcpcl, bundleSizeBytes, bundleRate, tcpclFragmentSize, thisLocalEidString);

        g_sigHandler.Start(false);
        std::cout << "BpGenAsync up and running" << std::endl;
        while (g_running) {
            boost::this_thread::sleep(boost::posix_time::millisec(250));
            g_sigHandler.PollOnce();
        }

       //std::cout << "Msg Count, Bundle Count, Bundle data bytes\n";

        //std::cout << egress.m_messageCount << "," << egress.m_bundleCount << "," << egress.m_bundleData << "\n";


        std::cout<< "BpGenAsyncMain: exiting cleanly..\n";
    }
    std::cout<< "BpGenAsyncMain: exited cleanly\n";
    return 0;

}
