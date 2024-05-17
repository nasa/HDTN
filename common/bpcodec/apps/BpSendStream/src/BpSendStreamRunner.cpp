/**
 * @file BpSendStreamRunner.cpp
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

#include "BpSendStream.h"
#include "BpSendStreamRunner.h"

#include "Logger.h"
#include "SignalHandler.h"
#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <boost/lexical_cast.hpp>
#include "Uri.h"
#include <cstdlib>

static constexpr hdtn::Logger::SubProcess subprocess = hdtn::Logger::SubProcess::none;

void BpSendStreamRunner::MonitorExitKeypressThreadFunction() {
    LOG_INFO(subprocess) << "Keyboard Interrupt.. exiting";
    m_runningFromSigHandler = false; //do this first
}


BpSendStreamRunner::BpSendStreamRunner() {}
BpSendStreamRunner::~BpSendStreamRunner() {}


bool BpSendStreamRunner::Run(int argc, const char* const argv[], std::atomic<bool>& running, bool useSignalHandler) {
    //scope to ensure clean exit before return 0
    {
        running = true;
        m_runningFromSigHandler = true;
        SignalHandler sigHandler(boost::bind(&BpSendStreamRunner::MonitorExitKeypressThreadFunction, this));
        uint32_t bundleRate;

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

        size_t maxIncomingUdpPacketSizeBytes;
        uint16_t incomingRtpStreamPort;
        size_t numCircularBufferVectors;
        uint32_t maxBundleSizeBytes;
        uint16_t rtpPacketsPerBundle;
        std::string inductType;
        std::string fileLocation;

        boost::program_options::options_description desc("Allowed options");
        try {
            desc.add_options()
                ("help", "Produce help message.")
                ("bundle-size", boost::program_options::value<uint32_t>()->default_value(100), "Bundle size bytes.")
                ("bundle-rate", boost::program_options::value<uint32_t>()->default_value(1500), "Bundle rate. (0=>as fast as possible)")
                ("my-uri-eid", boost::program_options::value<std::string>()->default_value("ipn:1.1"), "BpGen Source Node Id.")
                ("dest-uri-eid", boost::program_options::value<std::string>()->default_value("ipn:2.1"), "BpGen sends to this final destination Eid.")
                ("my-custodian-service-id", boost::program_options::value<uint64_t>()->default_value(0), "Custodian service ID is always 0.")
                ("outducts-config-file", boost::program_options::value<boost::filesystem::path>()->default_value(""), "Outducts Configuration File.")
		 ("bpsec-config-file", boost::program_options::value<boost::filesystem::path>()->default_value(""), "BpSec Configuration File.")
                ("custody-transfer-inducts-config-file", boost::program_options::value<boost::filesystem::path>()->default_value(""), "Inducts Configuration File for custody transfer (use custody if present).")
                ("custody-transfer-use-acs", "Custody transfer should use Aggregate Custody Signals instead of RFC5050.")
                ("force-disable-custody", "Custody transfer turned off regardless of link bidirectionality.")
                ("use-bp-version-7", "Send bundles using bundle protocol version 7.")
                ("bundle-send-timeout-seconds", boost::program_options::value<unsigned int>()->default_value(3), "Max time to send a bundle and get acknowledgement.")
                ("bundle-lifetime-milliseconds", boost::program_options::value<uint64_t>()->default_value(1000000), "Bundle lifetime in milliseconds.")
                ("bundle-priority", boost::program_options::value<uint64_t>()->default_value(2), "Bundle priority. 0 = Bulk 1 = Normal 2 = Expedited")
                ("num-circular-buffer-vectors", boost::program_options::value<size_t>()->default_value(50), "Number of circular buffer vector elements in the udp sink")
                ("max-incoming-udp-packet-size-bytes", boost::program_options::value<size_t>()->default_value(2), "Max size of incoming UDP packets (from the RTP stream). Use in conjection with FFmpeg")
                ("incoming-rtp-stream-port", boost::program_options::value<uint16_t>()->default_value(50000), "Where incoming RTP stream is being delivered")
                ("rtp-packets-per-bundle", boost::program_options::value<uint16_t>()->default_value(1), "Number of RTP packets placed into a bundle before sending")
                ("induct-type", boost::program_options::value<std::string>()->default_value("udp"), "Type of induct to use. Either embedded gstreamer appsink, udp, fd, or tcp")
                ("file-to-stream", boost::program_options::value<std::string>()->default_value("file.mp4"), "Full filepath of the file to be streamed if reading from file OR socket path if reading from a shared memory induct")
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

		//bpSecConfigFilePath = vm["bpsec-config-file"].as<boost::filesystem::path>();

                const boost::filesystem::path outductsConfigFileName = vm["outducts-config-file"].as<boost::filesystem::path>();

                if (!outductsConfigFileName.empty()) {
                    outductsConfigPtr = OutductsConfig::CreateFromJsonFilePath(outductsConfigFileName);
                    if (!outductsConfigPtr) {
                        LOG_ERROR(subprocess) << "error loading outducts config file: " << outductsConfigFileName;
                        return false;
                    }
                    std::size_t numBpGenOutducts = outductsConfigPtr->m_outductElementConfigVector.size();
                    if (numBpGenOutducts != 1) {
                        LOG_ERROR(subprocess) << "number of BpSendStream outducts is not 1: got " << numBpGenOutducts;
                    }
                }
                else {
                    LOG_WARNING(subprocess) << "notice: BpSendStream has no outduct... bundle data will have to flow out through a bidirectional tcpcl induct";
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
                        LOG_ERROR(subprocess) << "number of bp gen inducts for custody signals is not 1: got " << numBpGenInducts;
                    }
                }
                custodyTransferUseAcs = (vm.count("custody-transfer-use-acs"));


                myCustodianServiceId = vm["my-custodian-service-id"].as<uint64_t>();
                bundleSendTimeoutSeconds = vm["bundle-send-timeout-seconds"].as<unsigned int>();

                bundlePriority = vm["bundle-priority"].as<uint64_t>();
                if (bundlePriority > 2) {
                    std::cerr << "Priority must be 0, 1, or 2." << std::endl;
                    return false;
                }

                bundleLifetimeMilliseconds = vm["bundle-lifetime-milliseconds"].as<uint64_t>();

                maxIncomingUdpPacketSizeBytes =  vm["max-incoming-udp-packet-size-bytes"].as<size_t>();
                incomingRtpStreamPort =  vm["incoming-rtp-stream-port"].as<uint16_t>();
                numCircularBufferVectors = vm["num-circular-buffer-vectors"].as<size_t>();
                maxBundleSizeBytes = vm["bundle-size"].as<uint32_t>();
                bundleRate = vm["bundle-rate"].as<uint32_t>();
                rtpPacketsPerBundle = vm["rtp-packets-per-bundle"].as<uint16_t>();
                inductType = vm["induct-type"].as<std::string>();
                fileLocation = vm["file-to-stream"].as<std::string>();
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
        
        uint8_t inductTypeInt = HDTN_UDP_INTAKE; // default

        if (strcmp(inductType.c_str(), "appsink") == 0) {
            inductTypeInt = HDTN_APPSINK_INTAKE;
            LOG_INFO(subprocess) << "Using appsink induct type";
        } else if (strcmp(inductType.c_str(), "udp") == 0) {
            inductTypeInt = HDTN_UDP_INTAKE;
            LOG_INFO(subprocess) << "Using udp induct type";
        } else if (strcmp(inductType.c_str(), "shm") == 0) {
            inductTypeInt = HDTN_SHM_INTAKE;
            LOG_INFO(subprocess) << "Using shared memory (shm) induct type"; 
        } else {
            LOG_ERROR(subprocess) << "Unrecognized intake type. Aborting!";
            return false;
        }

        BpSendStream bpSendStream(inductTypeInt, maxIncomingUdpPacketSizeBytes, incomingRtpStreamPort, 
                numCircularBufferVectors, maxBundleSizeBytes,  rtpPacketsPerBundle, fileLocation);
        
        bpSendStream.Start(
            outductsConfigPtr,
            inductsConfigPtr,
            "",
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
            useBpVersion7
        );

        LOG_INFO(subprocess) << "running";
            
        if (useSignalHandler) {
            sigHandler.Start(false);
        }

        LOG_INFO(subprocess) << "Up and running";
        while (running.load(std::memory_order_acquire) && m_runningFromSigHandler.load(std::memory_order_acquire)) {
            boost::this_thread::sleep(boost::posix_time::millisec(250));
            if (useSignalHandler) {
                sigHandler.PollOnce();
            }
        }

        LOG_INFO(subprocess)<< "Exiting cleanly..";
        bpSendStream.Stop();
        m_bundleCount = bpSendStream.m_bundleCount;
        m_outductFinalStats = bpSendStream.m_outductFinalStats;
    }
    LOG_INFO(subprocess)<< "Exited cleanly";
    return true;

}
