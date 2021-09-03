#include <iostream>
#include "BpSinkAsync.h"
#include "BpSinkAsyncRunner.h"
#include "SignalHandler.h"
#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <boost/lexical_cast.hpp>
#include "Uri.h"

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
        InductsConfig_ptr inductsConfigPtr;
        OutductsConfig_ptr outductsConfigPtr;
        cbhe_eid_t myEid;
        bool isAcsAware;

        boost::program_options::options_description desc("Allowed options");
        try {
            desc.add_options()
                ("help", "Produce help message.")
                ("simulate-processing-lag-ms", boost::program_options::value<boost::uint32_t>()->default_value(0), "Extra milliseconds to process bundle (testing purposes).")
                ("inducts-config-file", boost::program_options::value<std::string>()->default_value("inducts.json"), "Inducts Configuration File.")
                ("my-uri-eid", boost::program_options::value<std::string>()->default_value("ipn:2.1"), "BpGen Source Node Id.")
                ("custody-transfer-outducts-config-file", boost::program_options::value<std::string>()->default_value(""), "Outducts Configuration File for custody transfer (use custody if present).")
                ("acs-aware-bundle-agent", "Custody transfer should support Aggregate Custody Signals if valid CTEB present.")
                ;

            boost::program_options::variables_map vm;
            boost::program_options::store(boost::program_options::parse_command_line(argc, argv, desc, boost::program_options::command_line_style::unix_style | boost::program_options::command_line_style::case_insensitive), vm);
            boost::program_options::notify(vm);

            if (vm.count("help")) {
                std::cout << desc << "\n";
                return false;
            }
            const std::string myUriEid = vm["my-uri-eid"].as<std::string>();
            if (!Uri::ParseIpnUriString(myUriEid, myEid.nodeId, myEid.serviceId)) {
                std::cerr << "error: bad bpsink uri string: " << myUriEid << std::endl;
                return false;
            }

            const std::string configFileNameInducts = vm["inducts-config-file"].as<std::string>();
            inductsConfigPtr = InductsConfig::CreateFromJsonFile(configFileNameInducts);
            if (!inductsConfigPtr) {
                std::cerr << "error loading config file: " << configFileNameInducts << std::endl;
                return false;
            }
            std::size_t numBpSinkInducts = inductsConfigPtr->m_inductElementConfigVector.size();
            if (numBpSinkInducts != 1) {
                std::cerr << "error: number of bp sink inducts is not 1: got " << numBpSinkInducts << std::endl;
            }

            //create outduct for custody signals
            const std::string outductsConfigFileName = vm["custody-transfer-outducts-config-file"].as<std::string>();
            if (outductsConfigFileName.length()) {
                outductsConfigPtr = OutductsConfig::CreateFromJsonFile(outductsConfigFileName);
                if (!outductsConfigPtr) {
                    std::cerr << "error loading config file: " << outductsConfigFileName << std::endl;
                    return false;
                }
                std::size_t numBpSinkOutducts = outductsConfigPtr->m_outductElementConfigVector.size();
                if (numBpSinkOutducts != 1) {
                    std::cerr << "error: number of bpsink outducts is not 1: got " << numBpSinkOutducts << std::endl;
                }
            }
            isAcsAware = (vm.count("acs-aware-bundle-agent"));

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
        bpSink.Init(*inductsConfigPtr, outductsConfigPtr, isAcsAware, myEid, processingLagMs);


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
