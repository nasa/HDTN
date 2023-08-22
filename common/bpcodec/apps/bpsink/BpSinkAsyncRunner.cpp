/**
 * @file BpSinkAsyncRunner.cpp
 * @author  Brian Tomko <brian.j.tomko@nasa.gov>
 *
 * @copyright Copyright © 2021 United States Government as represented by
 * the National Aeronautics and Space Administration.
 * No copyright is claimed in the United States under Title 17, U.S.Code.
 * All Other Rights Reserved.
 *
 * @section LICENSE
 * Released under the NASA Open Source Agreement (NOSA)
 * See LICENSE.md in the source root directory for more information.
 */

#include "BpSinkAsync.h"
#include "BpSinkAsyncRunner.h"
#include "SignalHandler.h"
#include "Logger.h"
#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <boost/lexical_cast.hpp>
#include "Uri.h"

static constexpr hdtn::Logger::SubProcess subprocess = hdtn::Logger::SubProcess::none;

void BpSinkAsyncRunner::MonitorExitKeypressThreadFunction() {
    LOG_INFO(subprocess) << "Keyboard Interrupt.. exiting";
    m_runningFromSigHandler = false; //do this first
}


BpSinkAsyncRunner::BpSinkAsyncRunner() {}
BpSinkAsyncRunner::~BpSinkAsyncRunner() {}


bool BpSinkAsyncRunner::Run(int argc, const char* const argv[], std::atomic<bool>& running, bool useSignalHandler) {
    //scope to ensure clean exit before return 0
    {
        running = true;
        m_runningFromSigHandler = true;
        SignalHandler sigHandler(boost::bind(&BpSinkAsyncRunner::MonitorExitKeypressThreadFunction, this));
        uint32_t processingLagMs;
        uint64_t maxBundleSizeBytes;
        InductsConfig_ptr inductsConfigPtr;
        OutductsConfig_ptr outductsConfigPtr;
        cbhe_eid_t myEid;
        bool isAcsAware;
        boost::filesystem::path bpSecConfigFilePath;

        boost::program_options::options_description desc("Allowed options");
        try {
            desc.add_options()
                ("help", "Produce help message.")
                ("simulate-processing-lag-ms", boost::program_options::value<uint32_t>()->default_value(0), "Extra milliseconds to process bundle (testing purposes).")
                ("inducts-config-file", boost::program_options::value<boost::filesystem::path>()->default_value(""), "Inducts Configuration File.")
                ("my-uri-eid", boost::program_options::value<std::string>()->default_value("ipn:2.1"), "BpSink Eid.")
                ("custody-transfer-outducts-config-file", boost::program_options::value<boost::filesystem::path>()->default_value(""), "Outducts Configuration File for custody transfer (use custody if present).")
                ("acs-aware-bundle-agent", "Custody transfer should support Aggregate Custody Signals if valid CTEB present.")
                ("bpsec-config-file", boost::program_options::value<boost::filesystem::path>()->default_value(""), "BpSec Configuration File.")
                ("max-rx-bundle-size-bytes", boost::program_options::value<uint64_t>()->default_value(10000000), "Max bundle size bytes to receive (default=10MB).")
                ;

            boost::program_options::variables_map vm;
            boost::program_options::store(boost::program_options::parse_command_line(argc, argv, desc, boost::program_options::command_line_style::unix_style | boost::program_options::command_line_style::case_insensitive), vm);
            boost::program_options::notify(vm);

            if (vm.count("help")) {
                LOG_INFO(subprocess) << desc;
                return false;
            }
            const std::string myUriEid = vm["my-uri-eid"].as<std::string>();
            if (!Uri::ParseIpnUriString(myUriEid, myEid.nodeId, myEid.serviceId)) {
                LOG_ERROR(subprocess) << "error: bad bpsink uri string: " << myUriEid;
                return false;
            }

            bpSecConfigFilePath = vm["bpsec-config-file"].as<boost::filesystem::path>();

            const boost::filesystem::path configFileNameInducts = vm["inducts-config-file"].as<boost::filesystem::path>();
            if (!configFileNameInducts.empty()) {
                inductsConfigPtr = InductsConfig::CreateFromJsonFilePath(configFileNameInducts);
                if (!inductsConfigPtr) {
                    LOG_ERROR(subprocess) << "error loading config file: " << configFileNameInducts;
                    return false;
                }
                std::size_t numBpSinkInducts = inductsConfigPtr->m_inductElementConfigVector.size();
                if (numBpSinkInducts != 1) {
                    LOG_ERROR(subprocess) << "number of bp sink inducts is not 1: got " << numBpSinkInducts;
                }
            }
            else {
                LOG_WARNING(subprocess) << "notice: bpsink has no induct... bundle data will have to flow in through a bidirectional tcpcl outduct";
            }

            //create outduct for custody signals
            const boost::filesystem::path outductsConfigFileName = vm["custody-transfer-outducts-config-file"].as<boost::filesystem::path>();
            if (!outductsConfigFileName.empty()) {
                outductsConfigPtr = OutductsConfig::CreateFromJsonFilePath(outductsConfigFileName);
                if (!outductsConfigPtr) {
                    LOG_ERROR(subprocess) << "error loading config file: " << outductsConfigFileName;
                    return false;
                }
                std::size_t numBpSinkOutducts = outductsConfigPtr->m_outductElementConfigVector.size();
                if (numBpSinkOutducts != 1) {
                    LOG_ERROR(subprocess) << "number of bpsink outducts is not 1: got " << numBpSinkOutducts;
                }
            }
            isAcsAware = (vm.count("acs-aware-bundle-agent"));

            processingLagMs = vm["simulate-processing-lag-ms"].as<uint32_t>();
            maxBundleSizeBytes = vm["max-rx-bundle-size-bytes"].as<uint64_t>();
        }
        catch (boost::bad_any_cast & e) {
            LOG_ERROR(subprocess) << "invalid data error: " << e.what();
            LOG_ERROR(subprocess) << desc;
            return false;
        }
        catch (std::exception& e) {
            LOG_ERROR(subprocess) << e.what();
            return false;
        }
        catch (...) {
            LOG_ERROR(subprocess) << "Exception of unknown type!";
            return false;
        }


        LOG_INFO(subprocess) << "starting..";
        BpSinkAsync bpSink;
        if (!bpSink.Init(inductsConfigPtr, outductsConfigPtr, bpSecConfigFilePath, isAcsAware, myEid, processingLagMs, maxBundleSizeBytes)) {
            LOG_FATAL(subprocess) << "Cannot Init BpSink!";
            return false;
        }


        if (useSignalHandler) {
            sigHandler.Start(false);
        }
        LOG_INFO(subprocess) << "Up and running";
        while (running && m_runningFromSigHandler) {
            boost::this_thread::sleep(boost::posix_time::millisec(250));
            if (useSignalHandler) {
                sigHandler.PollOnce();
            }
        }


        LOG_INFO(subprocess) << "Exiting cleanly..";
        bpSink.Stop();
        m_totalBytesRx = bpSink.m_FinalStatsBpSink.m_totalBytesRx;
        m_receivedCount = bpSink.m_FinalStatsBpSink.m_receivedCount;
        m_duplicateCount = bpSink.m_FinalStatsBpSink.m_duplicateCount;
        this->m_FinalStatsBpSink = bpSink.m_FinalStatsBpSink;
    }
    LOG_INFO(subprocess) << "Exited cleanly";
    return true;

}
