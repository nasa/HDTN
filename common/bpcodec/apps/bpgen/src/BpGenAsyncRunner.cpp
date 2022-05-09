#include "BpGenAsyncRunner.h"
#include <iostream>
#include "SignalHandler.h"
#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <boost/lexical_cast.hpp>
#include "Uri.h"

void BpGenAsyncRunner::MonitorExitKeypressThreadFunction() {
    std::cout << "Keyboard Interrupt.. exiting\n";
    m_runningFromSigHandler = false; //do this first
}



static void DurationEndedThreadFunction(const boost::system::error_code& e, volatile bool * running) {
    if (e != boost::asio::error::operation_aborted) {
        // Timer was not cancelled, take necessary action.
        std::cout << "BpGen reached duration.. exiting\n";
    }
    else {
        std::cout << "Unknown error occurred in DurationEndedThreadFunction " << e.message() << std::endl;
    }
    *running = false;
}

BpGenAsyncRunner::BpGenAsyncRunner() {}
BpGenAsyncRunner::~BpGenAsyncRunner() {}


bool BpGenAsyncRunner::Run(int argc, const char* const argv[], volatile bool & running, bool useSignalHandler) {
    //scope to ensure clean exit before return 0
    {
        running = true;
        m_runningFromSigHandler = true;
        SignalHandler sigHandler(boost::bind(&BpGenAsyncRunner::MonitorExitKeypressThreadFunction, this));
        uint32_t bundleSizeBytes;
        uint32_t bundleRate;
        //uint32_t tcpclFragmentSize;
        uint32_t durationSeconds;
        cbhe_eid_t myEid;
        cbhe_eid_t finalDestEid;
        uint64_t myCustodianServiceId;
        OutductsConfig_ptr outductsConfigPtr;
        InductsConfig_ptr inductsConfigPtr;
        bool custodyTransferUseAcs;
        bool forceDisableCustody;
        bool useBpVersion7;
        unsigned int bundleSendTimeoutSeconds;

        boost::program_options::options_description desc("Allowed options");
        try {
                desc.add_options()
                    ("help", "Produce help message.")
                    ("bundle-size", boost::program_options::value<uint32_t>()->default_value(100), "Bundle size bytes.")
                    ("bundle-rate", boost::program_options::value<uint32_t>()->default_value(1500), "Bundle rate. (0=>as fast as possible)")
                    ("duration", boost::program_options::value<uint32_t>()->default_value(0), "Seconds to send bundles for (0=>infinity).")
                    ("my-uri-eid", boost::program_options::value<std::string>()->default_value("ipn:1.1"), "BpGen Source Node Id.")
                    ("dest-uri-eid", boost::program_options::value<std::string>()->default_value("ipn:2.1"), "BpGen sends to this final destination Eid.")
                    ("my-custodian-service-id", boost::program_options::value<uint64_t>()->default_value(0), "Custodian service ID is always 0.")
                    ("outducts-config-file", boost::program_options::value<std::string>()->default_value(""), "Outducts Configuration File.")
                    ("custody-transfer-inducts-config-file", boost::program_options::value<std::string>()->default_value(""), "Inducts Configuration File for custody transfer (use custody if present).")
                    ("custody-transfer-use-acs", "Custody transfer should use Aggregate Custody Signals instead of RFC5050.")
                    ("force-disable-custody", "Custody transfer turned off regardless of link bidirectionality.")
                    ("use-bp-version-7", "Send bundles using bundle protocol version 7.")
                    ("bundle-send-timeout-seconds", boost::program_options::value<unsigned int>()->default_value(3), "Max time to send a bundle and get acknowledgement.")
                    ;

                boost::program_options::variables_map vm;
                boost::program_options::store(boost::program_options::parse_command_line(argc, argv, desc, boost::program_options::command_line_style::unix_style | boost::program_options::command_line_style::case_insensitive), vm);
                boost::program_options::notify(vm);

                if (vm.count("help")) {
                        std::cout << desc << "\n";
                        return false;
                }
                forceDisableCustody = (vm.count("force-disable-custody") != 0);
                useBpVersion7 = (vm.count("use-bp-version-7") != 0);

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

                const std::string outductsConfigFileName = vm["outducts-config-file"].as<std::string>();

                if (outductsConfigFileName.length()) {
                    outductsConfigPtr = OutductsConfig::CreateFromJsonFile(outductsConfigFileName);
                    if (!outductsConfigPtr) {
                        std::cerr << "error loading outducts config file: " << outductsConfigFileName << std::endl;
                        return false;
                    }
                    std::size_t numBpGenOutducts = outductsConfigPtr->m_outductElementConfigVector.size();
                    if (numBpGenOutducts != 1) {
                        std::cerr << "error: number of bpgen outducts is not 1: got " << numBpGenOutducts << std::endl;
                    }
                }
                else {
                    std::cout << "notice: bpgen has no outduct... bundle data will have to flow out through a bidirectional tcpcl induct\n";
                }

                //create induct for custody signals
                const std::string inductsConfigFileName = vm["custody-transfer-inducts-config-file"].as<std::string>();
                if (inductsConfigFileName.length()) {
                    inductsConfigPtr = InductsConfig::CreateFromJsonFile(inductsConfigFileName);
                    if (!inductsConfigPtr) {
                        std::cerr << "error loading induct config file: " << inductsConfigFileName << std::endl;
                        return false;
                    }
                    std::size_t numBpGenInducts = inductsConfigPtr->m_inductElementConfigVector.size();
                    if (numBpGenInducts != 1) {
                        std::cerr << "error: number of bp gen inducts for custody signals is not 1: got " << numBpGenInducts << std::endl;
                    }
                }
                custodyTransferUseAcs = (vm.count("custody-transfer-use-acs"));

                bundleSizeBytes = vm["bundle-size"].as<uint32_t>();
                bundleRate = vm["bundle-rate"].as<uint32_t>();
                durationSeconds = vm["duration"].as<uint32_t>();
                myCustodianServiceId = vm["my-custodian-service-id"].as<uint64_t>();
                bundleSendTimeoutSeconds = vm["bundle-send-timeout-seconds"].as<unsigned int>();
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


        std::cout << "starting BpGenAsync.." << std::endl;
        std::cout << "Sending Bundles from BPGen Node " << myEid.nodeId << " to final Destination Node " << finalDestEid.nodeId << std::endl; 
        BpGenAsync bpGen(bundleSizeBytes);
        bpGen.Start(outductsConfigPtr, inductsConfigPtr, custodyTransferUseAcs, myEid, bundleRate, finalDestEid, myCustodianServiceId, bundleSendTimeoutSeconds, false, forceDisableCustody, useBpVersion7);

        boost::asio::io_service ioService;
        boost::asio::deadline_timer deadlineTimer(ioService);
        std::cout << "running bpgen for " << durationSeconds << " seconds\n";
        
        bool startedTimer = false;
        

        if (useSignalHandler) {
            sigHandler.Start(false);
        }
        std::cout << "BpGenAsync up and running" << std::endl;
        while (running && m_runningFromSigHandler) {
            boost::this_thread::sleep(boost::posix_time::millisec(250));
            if (durationSeconds) {
                if ((!startedTimer) && bpGen.m_allOutductsReady) {
                    startedTimer = true;
                    deadlineTimer.expires_from_now(boost::posix_time::seconds(durationSeconds));
                    deadlineTimer.async_wait(boost::bind(&DurationEndedThreadFunction, boost::asio::placeholders::error, &running));
                }
                else {
                    ioService.poll_one();
                }
            }
            if (useSignalHandler) {
                sigHandler.PollOnce();
            }
        }

       //std::cout << "Msg Count, Bundle Count, Bundle data bytes\n";

        //std::cout << egress.m_messageCount << "," << egress.m_bundleCount << "," << egress.m_bundleData << "\n";


        std::cout<< "BpGenAsyncRunner::Run: exiting cleanly..\n";
        bpGen.Stop();
        m_bundleCount = bpGen.m_bundleCount;
        m_outductFinalStats = bpGen.m_outductFinalStats;
    }
    std::cout<< "BpGenAsyncRunner::Run: exited cleanly\n";
    return true;

}
