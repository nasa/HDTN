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
        uint32_t processingLagMs;
        InductsConfig_ptr inductsConfig;

        boost::program_options::options_description desc("Allowed options");
        try {
            desc.add_options()
                ("help", "Produce help message.")
                ("simulate-processing-lag-ms", boost::program_options::value<boost::uint32_t>()->default_value(0), "Extra milliseconds to process bundle (testing purposes).")
                ("inducts-config-file", boost::program_options::value<std::string>()->default_value("inducts.json"), "Inducts Configuration File.")
                ;

            boost::program_options::variables_map vm;
            boost::program_options::store(boost::program_options::parse_command_line(argc, argv, desc, boost::program_options::command_line_style::unix_style | boost::program_options::command_line_style::case_insensitive), vm);
            boost::program_options::notify(vm);

            if (vm.count("help")) {
                std::cout << desc << "\n";
                return false;
            }
            const std::string configFileName = vm["inducts-config-file"].as<std::string>();

            inductsConfig = InductsConfig::CreateFromJsonFile(configFileName);
            if (!inductsConfig) {
                std::cerr << "error loading config file: " << configFileName << std::endl;
                return false;
            }
            std::size_t numBpSinkInducts = inductsConfig->m_inductElementConfigVector.size();
            if (numBpSinkInducts != 1) {
                std::cerr << "error: number of bp sink inducts is not 1: got " << numBpSinkInducts << std::endl;
            }

            processingLagMs = vm["simulate-processing-lag-ms"].as<boost::uint32_t>();
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
        hdtn::BpSinkAsync bpSink;
        bpSink.Init(*inductsConfig, processingLagMs);


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
