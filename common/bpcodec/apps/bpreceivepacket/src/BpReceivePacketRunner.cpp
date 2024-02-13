/**
 * @file BpReceivePacketRunner.cpp
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

#include "BpReceivePacket.h"
#include "BpReceivePacketRunner.h"
#include "SignalHandler.h"
#include "Logger.h"
#include <boost/filesystem/path.hpp>
#include <boost/program_options.hpp>
#include <boost/lexical_cast.hpp>
#include "Uri.h"

static constexpr hdtn::Logger::SubProcess subprocess = hdtn::Logger::SubProcess::none;

void BpReceivePacketRunner::MonitorExitKeypressThreadFunction() {
    LOG_INFO(subprocess) << "Keyboard Interrupt.. exiting";
    m_runningFromSigHandler = false; //do this first
}

BpReceivePacketRunner::BpReceivePacketRunner(): m_totalBytesRx(0), m_runningFromSigHandler(false) {}
BpReceivePacketRunner::~BpReceivePacketRunner() {}


bool BpReceivePacketRunner::Run(int argc, const char* const argv[], std::atomic<bool>& running, bool useSignalHandler) {
    //scope to ensure clean exit before return 0
    {
        running = true;
        m_runningFromSigHandler = true;
        SignalHandler sigHandler(boost::bind(&BpReceivePacketRunner::MonitorExitKeypressThreadFunction, this));
        InductsConfig_ptr inductsConfigPtr;
        OutductsConfig_ptr outductsConfigPtr;
        OutductsConfig_ptr packetOutductsConfigPtr;
        cbhe_eid_t myEid;
        bool isAcsAware;
        uint64_t maxBundleSizeBytes;
        boost::filesystem::path bpSecConfigFilePath;

        boost::program_options::options_description desc("Allowed options");
        try {
            desc.add_options()
                ("help", "Produce help message.")
                ("inducts-config-file", boost::program_options::value<boost::filesystem::path>()->default_value(""), "Inducts Configuration File.")
                ("my-uri-eid", boost::program_options::value<std::string>()->default_value("ipn:2.1"), "BpReceivePacket Eid.")
                ("custody-transfer-outducts-config-file", boost::program_options::value<boost::filesystem::path>()->default_value(""), "Outducts Configuration File for custody transfer (use custody if present).")
                ("packet-outducts-config-file", boost::program_options::value<boost::filesystem::path>()->default_value(""), "Packet Outducts Configuration File.")
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
                LOG_ERROR(subprocess) << "bad BpReceivePacket uri string: " << myUriEid;
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
                std::size_t numInducts = inductsConfigPtr->m_inductElementConfigVector.size();
                if (numInducts != 1) {
                    LOG_ERROR(subprocess) << "number of BpReceivePacket inducts is not 1: got " << numInducts;
                }
            }
            else {
                LOG_WARNING(subprocess) << "notice: BpReceivePacket has no induct... bundle data will have to flow in through a bidirectional tcpcl outduct";
            }
            
            //create outduct for packets
            const boost::filesystem::path packetOutductsConfigFileName = vm["packet-outducts-config-file"].as<boost::filesystem::path>();
            if (!packetOutductsConfigFileName.empty()) {
                packetOutductsConfigPtr = OutductsConfig::CreateFromJsonFilePath(packetOutductsConfigFileName);
                if (!packetOutductsConfigPtr) {
                    LOG_ERROR(subprocess) << "error loading config file: " << packetOutductsConfigFileName;
                    return false;
                }
            }
            else {
                LOG_ERROR(subprocess) << "notice: bpreceivepacket has no packet outduct...";
                return false;
            }
            
            //create outduct for custody signals
            const boost::filesystem::path outductsConfigFileName = vm["custody-transfer-outducts-config-file"].as<boost::filesystem::path>();
            if (!outductsConfigFileName.empty()) {
                outductsConfigPtr = OutductsConfig::CreateFromJsonFilePath(outductsConfigFileName);
                if (!outductsConfigPtr) {
                    LOG_ERROR(subprocess) << "error loading config file: " << outductsConfigFileName;
                    return false;
                }
                std::size_t numOutducts = outductsConfigPtr->m_outductElementConfigVector.size();
                if (numOutducts != 1) {
                    LOG_ERROR(subprocess) << "number of BpReceivePacket outducts is not 1: got " << numOutducts;
                }
            }
            isAcsAware = (vm.count("acs-aware-bundle-agent"));
            maxBundleSizeBytes = vm["max-rx-bundle-size-bytes"].as<uint64_t>();
        }
        catch (boost::bad_any_cast & e) {
            LOG_ERROR(subprocess) << "invalid data error: " << e.what() << "\n";
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
        BpReceivePacket bpReceivePacket;
        if (!bpReceivePacket.Init(inductsConfigPtr, outductsConfigPtr, bpSecConfigFilePath,
		             isAcsAware, myEid, 0, maxBundleSizeBytes)) {
	    LOG_FATAL(subprocess) << "Cannot Init BpReceivePacket!";
	    return false;
        }

        if (!bpReceivePacket.socketInit(packetOutductsConfigPtr, myEid, maxBundleSizeBytes)) {
	    LOG_FATAL(subprocess) << "Cannot initialize Packet outduct!";
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
        bpReceivePacket.Stop();
        //safe to get any stats now if needed
    }
    LOG_INFO(subprocess) << "Exited cleanly";
    return true;

}
