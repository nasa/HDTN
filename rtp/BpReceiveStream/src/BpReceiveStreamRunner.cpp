#include "BpReceiveStreamRunner.h"
#include "Logger.h"
#include "SignalHandler.h"
#include "Uri.h"
#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <boost/lexical_cast.hpp>

static constexpr hdtn::Logger::SubProcess subprocess = hdtn::Logger::SubProcess::none;

BpReceiveStreamRunner::BpReceiveStreamRunner()
{
}

BpReceiveStreamRunner::~BpReceiveStreamRunner()
{
}

void BpReceiveStreamRunner::MonitorExitKeypressThreadFunction() {
    LOG_INFO(subprocess) << "Keyboard Interrupt.. exiting";
    m_runningFromSigHandler = false; //do this first
}

bool BpReceiveStreamRunner::Run(int argc, const char *const argv[], volatile bool &running, bool useSignalHandler)
{
    //scope to ensure clean exit before return 0
    {
        running = true;
        m_runningFromSigHandler = true;
        SignalHandler sigHandler(boost::bind(&BpReceiveStreamRunner::MonitorExitKeypressThreadFunction, this));
        InductsConfig_ptr inductsConfigPtr;
        OutductsConfig_ptr outductsConfigPtr;
        cbhe_eid_t myEid;
        bool isAcsAware;
        uint64_t maxBundleSizeBytes;

        // bp send stream
        uint16_t remotePort;
        std::string remoteHostname;
        size_t numCircularBufferVectors;
        uint16_t maxOutgoingRtpPacketSizeBytes;
        std::string shmSocketPath;
        std::string gstCaps;
        std::string outductType;
        boost::filesystem::path bpSecConfigFilePath;

        boost::program_options::options_description desc("Allowed options");
        try {
            desc.add_options()
                ("help", "Produce help message.")
                ("inducts-config-file", boost::program_options::value<boost::filesystem::path>()->default_value(""), "Inducts Configuration File.")
                ("my-uri-eid", boost::program_options::value<std::string>()->default_value("ipn:2.1"), "BpReceiveFile Eid.")
                ("custody-transfer-outducts-config-file", boost::program_options::value<boost::filesystem::path>()->default_value(""), "Outducts Configuration File for custody transfer (use custody if present).")
                ("acs-aware-bundle-agent", "Custody transfer should support Aggregate Custody Signals if valid CTEB present.")
                ("max-rx-bundle-size-bytes", boost::program_options::value<uint64_t>()->default_value(10000000), "Max bundle size bytes to receive (default=10MB).")
                ("outgoing-rtp-port", boost::program_options::value<uint16_t>()->default_value(50560), "Destination port for the created RTP stream")
                ("outgoing-rtp-hostname",  boost::program_options::value<std::string>()->default_value("127.0.0.1"), "Remote IP to forward rtp packets to")
                ("num-circular-buffer-vectors", boost::program_options::value<size_t>()->default_value(50), "Number of circular buffer vector elements for incoming bundles")
                ("max-outgoing-rtp-packet-size-bytes", boost::program_options::value<uint16_t>()->default_value(1400), "Max size in bytes of the outgoing rtp packets")
                ("shm-socket-path", boost::program_options::value<std::string>()->default_value(GST_HDTN_OUTDUCT_SOCKET_PATH), "Location of the socket for shared memory sink to gstreamer")
                ("outduct-type", boost::program_options::value<std::string>()->default_value("udp"), "Outduct type to offboard RTP stream")
                ("gst-caps",boost::program_options::value<std::string>()->default_value("application/x-rtp, media=(string)video, clock-rate=(int)90000, encoding-name=(string)H264, payload=(int)96"), "Caps to apply to GStreamer elements before shared memory interface")
                ("bpsec-config-file", boost::program_options::value<boost::filesystem::path>()->default_value(""), "BpSec Configuration File.")      
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
                LOG_ERROR(subprocess) << "bad BpReceiveStream uri string: " << myUriEid;
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
                    LOG_ERROR(subprocess) << "number of BpRecvStream inducts is not 1: got " << numInducts;
                }
            }
            else {
                LOG_WARNING(subprocess) << "notice: BpRecvStream has no induct... bundle data will have to flow in through a bidirectional tcpcl outduct";
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
                    LOG_ERROR(subprocess) << "number of BpRecvStream outducts is not 1: got " << numOutducts;
                }
            }
            isAcsAware = (vm.count("acs-aware-bundle-agent"));
            maxBundleSizeBytes = vm["max-rx-bundle-size-bytes"].as<uint64_t>();


            remotePort                    = vm["outgoing-rtp-port"].as<uint16_t>();
            remoteHostname                = vm["outgoing-rtp-hostname"].as<std::string>();
            numCircularBufferVectors      = vm["num-circular-buffer-vectors"].as<size_t>();
            maxOutgoingRtpPacketSizeBytes = vm["max-outgoing-rtp-packet-size-bytes"].as<uint16_t>();
            shmSocketPath                 = vm["shm-socket-path"].as<std::string>();
            outductType                   = vm["outduct-type"].as<std::string>();
            gstCaps                       = vm["gst-caps"].as<std::string>();
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

        uint8_t outductTypeInt = UDP_OUTDUCT; // default

        if (strcmp(outductType.c_str(), "appsrc") == 0) {
            outductTypeInt = GSTREAMER_APPSRC_OUTDUCT;
            LOG_INFO(subprocess) << "Using GStreamer appsrc outduct with path " << shmSocketPath;
        } else if (strcmp(outductType.c_str(), "udp") == 0) {
            outductTypeInt = UDP_OUTDUCT;
            LOG_INFO(subprocess) << "Using UDP outduct";
        } else {
            LOG_ERROR(subprocess) << "Unrecognized outduct type. Aborting!";
            return false;
        }

        /* Set out parameters for bpRecvStream object */
        bp_recv_stream_params_t bpRecvStreamParams;
        bpRecvStreamParams.rtpDestHostname = remoteHostname;
        bpRecvStreamParams.rtpDestPort = remotePort;
        bpRecvStreamParams.maxOutgoingRtpPacketSizeBytes = maxOutgoingRtpPacketSizeBytes;
        bpRecvStreamParams.shmSocketPath = shmSocketPath;
        bpRecvStreamParams.outductType = outductTypeInt;
        bpRecvStreamParams.gstCaps = gstCaps;

        LOG_INFO(subprocess) << "starting..";

        BpReceiveStream BpReceiveStream(numCircularBufferVectors, bpRecvStreamParams);
        BpReceiveStream.Init(inductsConfigPtr, outductsConfigPtr, 
			     bpSecConfigFilePath, isAcsAware, 
			     myEid, 0, maxBundleSizeBytes);

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
        BpReceiveStream.Stop();
        //safe to get any stats now if needed
    }
    LOG_INFO(subprocess) << "Exited cleanly";
    return true;

}
