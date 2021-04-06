#include <iostream>
#include "BpSinkAsync.h"
#include "BpSinkAsyncRunner.h"
#include "SignalHandler.h"
#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <boost/lexical_cast.hpp>


void BpSinkAsyncRunner::MonitorExitKeypressThreadFunction() {
    std::cout << "Keyboard Interrupt.. exiting\n";
    m_runningFromSigHandler = false; //do this first
}


BpSinkAsyncRunner::BpSinkAsyncRunner() {}
BpSinkAsyncRunner::~BpSinkAsyncRunner() {}


bool BpSinkAsyncRunner::Run(int argc, const char* const argv[], volatile bool & running, bool useSignalHandler) {
    //scope to ensure clean exit before return 0
    {
        running = true;
        m_runningFromSigHandler = true;
        SignalHandler sigHandler(boost::bind(&BpSinkAsyncRunner::MonitorExitKeypressThreadFunction, this));
        uint16_t port;
        uint32_t processingLagMs;
        bool useTcpcl = false;
        bool useStcp = false;
        std::string thisLocalEidString;

        boost::program_options::options_description desc("Allowed options");
        try {
            desc.add_options()
                ("help", "Produce help message.")
                ("port", boost::program_options::value<boost::uint16_t>()->default_value(4557), "Listen on this TCP or UDP port.")
                ("simulate-processing-lag-ms", boost::program_options::value<boost::uint32_t>()->default_value(0), "Extra milliseconds to process bundle (testing purposes).")
                ("use-stcp", "Use STCP instead of UDP.")
                ("use-tcpcl", "Use TCP Convergence Layer Version 3 instead of UDP.")
                ("tcpcl-eid", boost::program_options::value<std::string>()->default_value("BpSink"), "Local EID string for this program.")
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

            port = vm["port"].as<boost::uint16_t>();
            processingLagMs = vm["simulate-processing-lag-ms"].as<boost::uint32_t>();
            thisLocalEidString = vm["tcpcl-eid"].as<std::string>();
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


        std::cout << "starting BpSink.." << std::endl;
        hdtn::BpSinkAsync bpSink(port, useTcpcl, useStcp, thisLocalEidString, processingLagMs);
        bpSink.Init(0);



        bpSink.Netstart();

        if (useSignalHandler) {
            sigHandler.Start(false);
        }
        std::cout << "BpSink up and running" << std::endl;
        while (running && m_runningFromSigHandler) {
            boost::this_thread::sleep(boost::posix_time::millisec(250));
            if (useSignalHandler) {
                sigHandler.PollOnce();
            }
        }


        std::cout << "BpSinkAsyncRunner: exiting cleanly..\n";
        bpSink.Stop();
        m_totalBytesRx = bpSink.m_totalBytesRx;
        m_receivedCount = bpSink.m_receivedCount;
        m_duplicateCount = bpSink.m_duplicateCount;
        this->m_FinalStatsBpSink = bpSink.m_FinalStatsBpSink;
    }
    std::cout << "BpSinkAsyncRunner: exited cleanly\n";
    return true;

}
