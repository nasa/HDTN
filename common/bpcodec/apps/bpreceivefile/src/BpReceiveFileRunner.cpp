#include <iostream>
#include "BpReceiveFile.h"
#include "BpReceiveFileRunner.h"
#include "SignalHandler.h"
#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <boost/lexical_cast.hpp>
#include "Uri.h"

void BpReceiveFileRunner::MonitorExitKeypressThreadFunction() {
    std::cout << "Keyboard Interrupt.. exiting\n";
    m_runningFromSigHandler = false; //do this first
}


BpReceiveFileRunner::BpReceiveFileRunner() {}
BpReceiveFileRunner::~BpReceiveFileRunner() {}


bool BpReceiveFileRunner::Run(int argc, const char* const argv[], volatile bool & running, bool useSignalHandler) {
    //scope to ensure clean exit before return 0
    {
        running = true;
        m_runningFromSigHandler = true;
        SignalHandler sigHandler(boost::bind(&BpReceiveFileRunner::MonitorExitKeypressThreadFunction, this));
        InductsConfig_ptr inductsConfigPtr;
        OutductsConfig_ptr outductsConfigPtr;
        cbhe_eid_t myEid;
        bool isAcsAware;
        boost::filesystem::path saveDirectory;
        uint64_t maxBundleSizeBytes;

        boost::program_options::options_description desc("Allowed options");
        try {
            desc.add_options()
                ("help", "Produce help message.")
                ("save-directory", boost::program_options::value<boost::filesystem::path>()->default_value(""), "Directory to save file(s) to.  Empty=>DoNotSaveToDisk")
                ("inducts-config-file", boost::program_options::value<std::string>()->default_value(""), "Inducts Configuration File.")
                ("my-uri-eid", boost::program_options::value<std::string>()->default_value("ipn:2.1"), "BpReceiveFile Eid.")
                ("custody-transfer-outducts-config-file", boost::program_options::value<std::string>()->default_value(""), "Outducts Configuration File for custody transfer (use custody if present).")
                ("acs-aware-bundle-agent", "Custody transfer should support Aggregate Custody Signals if valid CTEB present.")
                ("max-rx-bundle-size-bytes", boost::program_options::value<uint64_t>()->default_value(10000000), "Max bundle size bytes to receive (default=10MB).")
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
                std::cerr << "error: bad BpReceiveFile uri string: " << myUriEid << std::endl;
                return false;
            }

            const std::string configFileNameInducts = vm["inducts-config-file"].as<std::string>();
            if (configFileNameInducts.length()) {
                inductsConfigPtr = InductsConfig::CreateFromJsonFile(configFileNameInducts);
                if (!inductsConfigPtr) {
                    std::cerr << "error loading config file: " << configFileNameInducts << std::endl;
                    return false;
                }
                std::size_t numInducts = inductsConfigPtr->m_inductElementConfigVector.size();
                if (numInducts != 1) {
                    std::cerr << "error: number of BpReceiveFile inducts is not 1: got " << numInducts << std::endl;
                }
            }
            else {
                std::cout << "notice: BpReceiveFile has no induct... bundle data will have to flow in through a bidirectional tcpcl outduct\n";
            }

            //create outduct for custody signals
            const std::string outductsConfigFileName = vm["custody-transfer-outducts-config-file"].as<std::string>();
            if (outductsConfigFileName.length()) {
                outductsConfigPtr = OutductsConfig::CreateFromJsonFile(outductsConfigFileName);
                if (!outductsConfigPtr) {
                    std::cerr << "error loading config file: " << outductsConfigFileName << std::endl;
                    return false;
                }
                std::size_t numOutducts = outductsConfigPtr->m_outductElementConfigVector.size();
                if (numOutducts != 1) {
                    std::cerr << "error: number of BpReceiveFile outducts is not 1: got " << numOutducts << std::endl;
                }
            }
            isAcsAware = (vm.count("acs-aware-bundle-agent"));
            saveDirectory = vm["save-directory"].as<boost::filesystem::path>();
            maxBundleSizeBytes = vm["max-rx-bundle-size-bytes"].as<uint64_t>();
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


        std::cout << "starting BpReceiveFile.." << std::endl;
        BpReceiveFile bpReceiveFile(saveDirectory);
        bpReceiveFile.Init(inductsConfigPtr, outductsConfigPtr, isAcsAware, myEid, 0, maxBundleSizeBytes);


        if (useSignalHandler) {
            sigHandler.Start(false);
        }
        std::cout << "BpReceiveFileRunner up and running" << std::endl;
        while (running && m_runningFromSigHandler) {
            boost::this_thread::sleep(boost::posix_time::millisec(250));
            if (useSignalHandler) {
                sigHandler.PollOnce();
            }
        }


        std::cout << "BpReceiveFileRunner: exiting cleanly..\n";
        bpReceiveFile.Stop();
        //safe to get any stats now if needed
    }
    std::cout << "BpReceiveFileRunner: exited cleanly\n";
    return true;

}
