#include "BpSendFileRunner.h"
#include <iostream>
#include "SignalHandler.h"
#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <boost/lexical_cast.hpp>
#include "Uri.h"

void BpSendFileRunner::MonitorExitKeypressThreadFunction() {
    std::cout << "Keyboard Interrupt.. exiting\n";
    m_runningFromSigHandler = false; //do this first
}




BpSendFileRunner::BpSendFileRunner() {}
BpSendFileRunner::~BpSendFileRunner() {}


bool BpSendFileRunner::Run(int argc, const char* const argv[], volatile bool & running, bool useSignalHandler) {
    //scope to ensure clean exit before return 0
    {
        running = true;
        m_runningFromSigHandler = true;
        SignalHandler sigHandler(boost::bind(&BpSendFileRunner::MonitorExitKeypressThreadFunction, this));
        uint64_t maxBundleSizeBytes;
        std::string fileOrFolderPath;
        cbhe_eid_t myEid;
        cbhe_eid_t finalDestEid;
        uint64_t myCustodianServiceId;
        OutductsConfig_ptr outductsConfig;
        InductsConfig_ptr inductsConfig;
        bool custodyTransferUseAcs;

        boost::program_options::options_description desc("Allowed options");
        try {
                desc.add_options()
                    ("help", "Produce help message.")
                    ("max-bundle-size-bytes", boost::program_options::value<uint64_t>()->default_value(4000000), "Max size bundle for file fragments (default 4MB).")
                    ("file-or-folder-path", boost::program_options::value<std::string>()->default_value(""), "File or folder paths. Folders are recursive.")
                    ("my-uri-eid", boost::program_options::value<std::string>()->default_value("ipn:1.1"), "BpGen Source Node Id.")
                    ("dest-uri-eid", boost::program_options::value<std::string>()->default_value("ipn:2.1"), "BpGen sends to this final destination Eid.")
                    ("my-custodian-service-id", boost::program_options::value<uint64_t>()->default_value(0), "Custodian service ID is always 0.")
                    ("outducts-config-file", boost::program_options::value<std::string>()->default_value("outducts.json"), "Outducts Configuration File.")
                    ("custody-transfer-inducts-config-file", boost::program_options::value<std::string>()->default_value(""), "Inducts Configuration File for custody transfer (use custody if present).")
                    ("custody-transfer-use-acs", "Custody transfer should use Aggregate Custody Signals instead of RFC5050.")
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

                const std::string myFinalDestUriEid = vm["dest-uri-eid"].as<std::string>();
                if (!Uri::ParseIpnUriString(myFinalDestUriEid, finalDestEid.nodeId, finalDestEid.serviceId)) {
                    std::cerr << "error: bad bpsink uri string: " << myFinalDestUriEid << std::endl;
                    return false;
                }

                const std::string configFileName = vm["outducts-config-file"].as<std::string>();

                outductsConfig = OutductsConfig::CreateFromJsonFile(configFileName);
                if (!outductsConfig) {
                    std::cerr << "error loading config file: " << configFileName << std::endl;
                    return false;
                }
                std::size_t numBpGenOutducts = outductsConfig->m_outductElementConfigVector.size();
                if (numBpGenOutducts != 1) {
                    std::cerr << "error: number of bpgen outducts is not 1: got " << numBpGenOutducts << std::endl;
                }

                //create induct for custody signals
                const std::string inductsConfigFileName = vm["custody-transfer-inducts-config-file"].as<std::string>();
                if (inductsConfigFileName.length()) {
                    inductsConfig = InductsConfig::CreateFromJsonFile(inductsConfigFileName);
                    if (!inductsConfig) {
                        std::cerr << "error loading induct config file: " << inductsConfigFileName << std::endl;
                        return false;
                    }
                    std::size_t numBpGenInducts = inductsConfig->m_inductElementConfigVector.size();
                    if (numBpGenInducts != 1) {
                        std::cerr << "error: number of bp gen inducts for custody signals is not 1: got " << numBpGenInducts << std::endl;
                    }
                }
                custodyTransferUseAcs = (vm.count("custody-transfer-use-acs"));
                fileOrFolderPath = vm["file-or-folder-path"].as<std::string>();
                maxBundleSizeBytes = vm["max-bundle-size-bytes"].as<uint64_t>();
                myCustodianServiceId = vm["my-custodian-service-id"].as<uint64_t>();
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


        std::cout << "starting BpSendFile.." << std::endl;

        BpSendFile bpSendFile(fileOrFolderPath, maxBundleSizeBytes);
        if (bpSendFile.GetNumberOfFilesToSend() == 0) {
            return false;
        }
        bpSendFile.Start(*outductsConfig, inductsConfig, custodyTransferUseAcs, myEid, 0, finalDestEid, myCustodianServiceId);

        std::cout << "running BpSendFile\n";
        
        bool startedTimer = false;
        

        if (useSignalHandler) {
            sigHandler.Start(false);
        }
        std::cout << "BpSendFile up and running" << std::endl;
        while (running && m_runningFromSigHandler) {
            boost::this_thread::sleep(boost::posix_time::millisec(250));
            if (useSignalHandler) {
                sigHandler.PollOnce();
            }
        }

        std::cout<< "BpSendFileRunner::Run: exiting cleanly..\n";
        bpSendFile.Stop();
        m_bundleCount = bpSendFile.m_bundleCount;
        m_outductFinalStats = bpSendFile.m_outductFinalStats;
    }
    std::cout<< "BpSendFileRunner::Run: exited cleanly\n";
    return true;

}
