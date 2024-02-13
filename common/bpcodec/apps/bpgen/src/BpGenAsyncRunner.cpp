/**
 * @file BpGenAsyncRunner.cpp
 * @author  Brian Tomko <brian.j.tomko@nasa.gov>
 *
 * @copyright Copyright (c) 2021 United States Government as represented by
 * the National Aeronautics and Space Administration.
 * No copyright is claimed in the United States under Title 17, U.S.Code.
 * All Other Rights Reserved.
 *
 * @section LICENSE
 * Released under the NASA Open Source Agreement (NOSA)
 * See LICENSE.md in the source root directory for more information.
 */

#include "BpGenAsyncRunner.h"
#include "SignalHandler.h"
#include <boost/filesystem/path.hpp>
#include <boost/program_options.hpp>
#include <boost/lexical_cast.hpp>
#include "Uri.h"
#include "Logger.h"

static constexpr hdtn::Logger::SubProcess subprocess = hdtn::Logger::SubProcess::none;

void BpGenAsyncRunner::MonitorExitKeypressThreadFunction() {
    LOG_INFO(subprocess) << "Keyboard Interrupt.. exiting";
    m_runningFromSigHandler = false; //do this first
}

static void DurationEndedThreadFunction(const boost::system::error_code& e, std::atomic<bool>* running) {
    if (e != boost::asio::error::operation_aborted) {
        // Timer was not cancelled, take necessary action.
        LOG_INFO(subprocess) << "Reached duration.. exiting";
    }
    else {
        LOG_ERROR(subprocess) << "Unknown error occurred in DurationEndedThreadFunction " << e.message();
    }
    *running = false;
}

BpGenAsyncRunner::BpGenAsyncRunner(): m_totalBundlesAcked(0), m_runningFromSigHandler(false) {}
BpGenAsyncRunner::~BpGenAsyncRunner() {}


bool BpGenAsyncRunner::Run(int argc, const char* const argv[], std::atomic<bool>& running, bool useSignalHandler) {
    //scope to ensure clean exit before return 0
    {
        running = true;
        m_runningFromSigHandler = true;
        SignalHandler sigHandler(boost::bind(&BpGenAsyncRunner::MonitorExitKeypressThreadFunction, this));
        uint32_t bundleSizeBytes;
        double bundleRate;
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
        uint64_t bundleLifetimeMilliseconds;
        uint64_t bundlePriority;
        uint64_t claRate;
        boost::filesystem::path bpSecConfigFilePath;

        boost::program_options::options_description desc("Allowed options");
        try {
            desc.add_options()
                ("help", "Produce help message.")
                ("bundle-size", boost::program_options::value<uint32_t>()->default_value(100), "Bundle size bytes.")
                ("bundle-rate", boost::program_options::value<double>()->default_value(1500), "Bundle rate. (0=>as fast as possible)")
                ("duration", boost::program_options::value<uint32_t>()->default_value(0), "Seconds to send bundles for (0=>infinity).")
                ("my-uri-eid", boost::program_options::value<std::string>()->default_value("ipn:1.1"), "BpGen Source Node Id.")
                ("dest-uri-eid", boost::program_options::value<std::string>()->default_value("ipn:2.1"), "BpGen sends to this final destination Eid.")
                ("my-custodian-service-id", boost::program_options::value<uint64_t>()->default_value(0), "Custodian service ID is always 0.")
                ("outducts-config-file", boost::program_options::value<boost::filesystem::path>()->default_value(""), "Outducts Configuration File.")
                ("custody-transfer-inducts-config-file", boost::program_options::value<boost::filesystem::path>()->default_value(""), "Inducts Configuration File for custody transfer (use custody if present).")
                ("bpsec-config-file", boost::program_options::value<boost::filesystem::path>()->default_value(""), "BpSec Configuration File.")
                ("custody-transfer-use-acs", "Custody transfer should use Aggregate Custody Signals instead of RFC5050.")
                ("force-disable-custody", "Custody transfer turned off regardless of link bidirectionality.")
                ("use-bp-version-7", "Send bundles using bundle protocol version 7.")
                ("bundle-send-timeout-seconds", boost::program_options::value<unsigned int>()->default_value(3), "Max time to send a bundle and get acknowledgement.")
                ("bundle-lifetime-milliseconds", boost::program_options::value<uint64_t>()->default_value(1000000), "Bundle lifetime in milliseconds.")
                ("bundle-priority", boost::program_options::value<uint64_t>()->default_value(2), "Bundle priority. 0 = Bulk 1 = Normal 2 = Expedited")
                ("cla-rate", boost::program_options::value<uint64_t>()->default_value(0), "Convergence layer adapter send rate. 0 = unlimited")
                ;

            boost::program_options::variables_map vm;
            boost::program_options::store(boost::program_options::parse_command_line(argc, argv, desc, boost::program_options::command_line_style::unix_style | boost::program_options::command_line_style::case_insensitive), vm);
            boost::program_options::notify(vm);

            if (vm.count("help")) {
                    LOG_INFO(subprocess) << desc;
                    return false;
            }
            forceDisableCustody = (vm.count("force-disable-custody") != 0);
            useBpVersion7 = (vm.count("use-bp-version-7") != 0);

            const std::string myUriEid = vm["my-uri-eid"].as<std::string>();
            if (!Uri::ParseIpnUriString(myUriEid, myEid.nodeId, myEid.serviceId)) {
                LOG_ERROR(subprocess) << "error: bad bpsink uri string: " << myUriEid;
                return false;
            }

            const std::string myFinalDestUriEid = vm["dest-uri-eid"].as<std::string>();
            if (!Uri::ParseIpnUriString(myFinalDestUriEid, finalDestEid.nodeId, finalDestEid.serviceId)) {
                LOG_ERROR(subprocess) << "error: bad bpsink uri string: " << myFinalDestUriEid;
                return false;
            }

            bpSecConfigFilePath = vm["bpsec-config-file"].as<boost::filesystem::path>();

            const boost::filesystem::path outductsConfigFileName = vm["outducts-config-file"].as<boost::filesystem::path>();

            if (!outductsConfigFileName.empty()) {
                outductsConfigPtr = OutductsConfig::CreateFromJsonFilePath(outductsConfigFileName);
                if (!outductsConfigPtr) {
                    LOG_ERROR(subprocess) << "error loading outducts config file: " << outductsConfigFileName;
                    return false;
                }
                std::size_t numBpGenOutducts = outductsConfigPtr->m_outductElementConfigVector.size();
                if (numBpGenOutducts != 1) {
                    LOG_ERROR(subprocess) << "error: number of bpgen outducts is not 1: got " << numBpGenOutducts;
                }
            }
            else {
                LOG_WARNING(subprocess) << "notice: bpgen has no outduct... bundle data will have to flow out through a bidirectional tcpcl induct";
            }

            //create induct for custody signals
            const boost::filesystem::path inductsConfigFileName = vm["custody-transfer-inducts-config-file"].as<boost::filesystem::path>();
            if (!inductsConfigFileName.empty()) {
                inductsConfigPtr = InductsConfig::CreateFromJsonFilePath(inductsConfigFileName);
                if (!inductsConfigPtr) {
                    LOG_ERROR(subprocess) << "error loading induct config file: " << inductsConfigFileName;
                    return false;
                }
                std::size_t numBpGenInducts = inductsConfigPtr->m_inductElementConfigVector.size();
                if (numBpGenInducts != 1) {
                    LOG_ERROR(subprocess) << "error: number of bp gen inducts for custody signals is not 1: got " << numBpGenInducts;
                }
            }
            custodyTransferUseAcs = (vm.count("custody-transfer-use-acs"));

            bundlePriority = vm["bundle-priority"].as<uint64_t>();
            if (bundlePriority > 2) {
                std::cerr << "Priority must be 0, 1, or 2." << std::endl;
                return false;
            }

            bundleSizeBytes = vm["bundle-size"].as<uint32_t>();
            bundleRate = vm["bundle-rate"].as<double>();
            durationSeconds = vm["duration"].as<uint32_t>();
            myCustodianServiceId = vm["my-custodian-service-id"].as<uint64_t>();
            bundleSendTimeoutSeconds = vm["bundle-send-timeout-seconds"].as<unsigned int>();
            bundleLifetimeMilliseconds = vm["bundle-lifetime-milliseconds"].as<uint64_t>();
            claRate = vm["cla-rate"].as<uint64_t>();
        }
        catch (boost::bad_any_cast & e) {
                LOG_ERROR(subprocess) << "invalid data error: " << e.what();
                LOG_ERROR(subprocess) << desc;
                return false;
        }
        catch (std::exception& e) {
                LOG_ERROR(subprocess) << "error: " << e.what();
                return false;
        }
        catch (...) {
                LOG_ERROR(subprocess) << "Exception of unknown type!";
                return false;
        }


        LOG_INFO(subprocess) << "starting BpGenAsync..";
        LOG_INFO(subprocess) << "Sending Bundles from BPGen Node " << myEid.nodeId << " to final Destination Node " << finalDestEid.nodeId; 
        BpGenAsync bpGen(bundleSizeBytes);
        bpGen.Start(
            outductsConfigPtr,
            inductsConfigPtr,
            bpSecConfigFilePath,
            custodyTransferUseAcs,
            myEid,
            bundleRate,
            finalDestEid,
            myCustodianServiceId,
            bundleSendTimeoutSeconds,
            bundleLifetimeMilliseconds,
            bundlePriority,
            false,
            forceDisableCustody,
            useBpVersion7,
            claRate
        );

        boost::asio::io_service ioService;
        boost::asio::deadline_timer deadlineTimer(ioService);
        LOG_INFO(subprocess) << "running bpgen for " << durationSeconds << " seconds";
        
        bool startedTimer = false;
        

        if (useSignalHandler) {
            sigHandler.Start(false);
        }
        LOG_INFO(subprocess) << "BpGenAsync up and running";
        while (running && m_runningFromSigHandler) {
            boost::this_thread::sleep(boost::posix_time::millisec(250));
            if (durationSeconds) {
                if ((!startedTimer) && bpGen.m_allOutductsReady) {
                    startedTimer = true;
                    deadlineTimer.expires_from_now(boost::posix_time::seconds(durationSeconds));
                    deadlineTimer.async_wait(boost::bind(&DurationEndedThreadFunction, boost::asio::placeholders::error, &running));
                }
                else if (startedTimer && running) { //don't call poll_one until there is work
                    ioService.poll_one();
                }
            }
            if (useSignalHandler) {
                sigHandler.PollOnce();
            }
        }


        LOG_INFO(subprocess) << "BpGenAsyncRunner::Run: exiting cleanly..";
        bpGen.Stop();
        m_bundleCount = bpGen.m_bundleCount;
        m_outductFinalStats = bpGen.m_outductFinalStats;
    }
    LOG_INFO(subprocess) << "BpGenAsyncRunner::Run: exited cleanly";
    return true;

}
