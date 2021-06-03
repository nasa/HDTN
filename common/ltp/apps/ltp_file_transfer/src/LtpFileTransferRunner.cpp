#include "LtpFileTransferRunner.h"
#include <iostream>
#include "SignalHandler.h"
#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/uuid/detail/sha1.hpp>
#include <boost/endian/conversion.hpp>
#include "LtpUdpEngine.h"

static void GetSha1(const std::vector<uint8_t> & data, std::string & sha1Str) {

    sha1Str.resize(40);
    char * strPtr = &sha1Str[0];

    boost::uuids::detail::sha1 s;
    s.process_bytes(data.data(), data.size());
    boost::uint32_t digest[5];
    s.get_digest(digest);
    for (int i = 0; i < 5; ++i) {
        //const uint32_t digestBe = boost::endian::native_to_big(digest[i]);
        sprintf(strPtr, "%08x", digest[i]);// digestBe);
        strPtr += 8;
    }
}



void LtpFileTransferRunner::MonitorExitKeypressThreadFunction() {
    std::cout << "Keyboard Interrupt.. exiting\n";
    m_runningFromSigHandler = false; //do this first
}




LtpFileTransferRunner::LtpFileTransferRunner() {}
LtpFileTransferRunner::~LtpFileTransferRunner() {}


bool LtpFileTransferRunner::Run(int argc, const char* const argv[], volatile bool & running, bool useSignalHandler) {
    //scope to ensure clean exit before return 0
    {
        running = true;
        m_runningFromSigHandler = true;
        SignalHandler sigHandler(boost::bind(&LtpFileTransferRunner::MonitorExitKeypressThreadFunction, this));
        std::string sendFilePath;
        std::string receiveFilePath;
        bool useSendFile = false;
        bool useReceiveFile = false;
        std::string destUdpHostname;
        uint16_t destUdpPort;
        uint64_t thisLtpEngineId;
        uint64_t remoteLtpEngineId;
        uint64_t ltpDataSegmentMtu;
        uint64_t ltpReportSegmentMtu;
        uint64_t oneWayLightTimeMs;
        uint64_t oneWayMarginTimeMs;
        uint64_t clientServiceId;
        uint64_t estimatedFileSizeToReceive;
        unsigned int numUdpRxPacketsCircularBufferSize;
        unsigned int maxRxUdpPacketSizeBytes;

        boost::program_options::options_description desc("Allowed options");
        try {
            desc.add_options()
                ("help", "Produce help message.")
                ("receive-file", boost::program_options::value<std::string>(), "Receive a file to this file name.")
                ("send-file", boost::program_options::value<std::string>(), "Send this file name.")
                ("dest-udp-hostname", boost::program_options::value<std::string>()->default_value("localhost"), "Ltp destination UDP hostname.")
                ("dest-udp-port", boost::program_options::value<uint16_t>()->default_value(4556), "Ltp destination UDP port.")

                ("this-ltp-engine-id", boost::program_options::value<uint64_t>()->default_value(2), "My LTP engine ID.")
                ("remote-ltp-engine-id", boost::program_options::value<uint64_t>()->default_value(2), "Remote LTP engine ID.")
                ("ltp-data-segment-mtu", boost::program_options::value<uint64_t>()->default_value(1), "Max payload size (bytes) of sender's LTP data segment")
                ("ltp-report-segment-mtu", boost::program_options::value<uint64_t>()->default_value(UINT64_MAX), "Approximate max size (bytes) of receiver's LTP report segment")
                ("num-rx-udp-packets-buffer-size", boost::program_options::value<unsigned int>()->default_value(100), "UDP max packets to receive (circular buffer size)")
                ("max-rx-udp-packet-size-bytes", boost::program_options::value<unsigned int>()->default_value(UINT16_MAX), "Maximum size (bytes) of a UDP packet to receive (65KB safest option)")
                ("one-way-light-time-ms", boost::program_options::value<uint64_t>()->default_value(1), "One way light time in milliseconds")
                ("one-way-margin-time-ms", boost::program_options::value<uint64_t>()->default_value(1), "One way light time in milliseconds")
                ("client-service-id", boost::program_options::value<uint64_t>()->default_value(2), "LTP Client Service ID.")
                ("estimated-rx-filesize", boost::program_options::value<uint64_t>()->default_value(50000000), "How many bytes to initially reserve for rx (default 50MB).")
                ;

            boost::program_options::variables_map vm;
            boost::program_options::store(boost::program_options::parse_command_line(argc, argv, desc, boost::program_options::command_line_style::unix_style | boost::program_options::command_line_style::case_insensitive), vm);
            boost::program_options::notify(vm);

            if (vm.count("help")) {
                std::cout << desc << "\n";
                return false;
            }

            if (!(vm.count("receive-file") ^ vm.count("send-file"))) {
                std::cerr << "error, receive-file or send-file must be specified, but not both\n";
                return false;
            }
            if (vm.count("receive-file")) {
                useReceiveFile = true;
                receiveFilePath = vm["receive-file"].as<std::string>();
            }
            else {
                useSendFile = true;
                sendFilePath = vm["send-file"].as<std::string>();
            }
            destUdpHostname = vm["dest-udp-hostname"].as<std::string>();
            destUdpPort = vm["dest-udp-port"].as<boost::uint16_t>();
            thisLtpEngineId = vm["this-ltp-engine-id"].as<uint64_t>();
            remoteLtpEngineId = vm["remote-ltp-engine-id"].as<uint64_t>();
            ltpDataSegmentMtu = vm["ltp-data-segment-mtu"].as<uint64_t>();
            ltpReportSegmentMtu = vm["ltp-report-segment-mtu"].as<uint64_t>();
            oneWayLightTimeMs = vm["one-way-light-time-ms"].as<uint64_t>();
            oneWayMarginTimeMs = vm["one-way-margin-time-ms"].as<uint64_t>();
            clientServiceId = vm["client-service-id"].as<uint64_t>();
            estimatedFileSizeToReceive = vm["estimated-rx-filesize"].as<uint64_t>();
            numUdpRxPacketsCircularBufferSize = vm["num-rx-udp-packets-buffer-size"].as<unsigned int>();
            maxRxUdpPacketSizeBytes = vm["max-rx-udp-packet-size-bytes"].as<unsigned int>();
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


        const boost::posix_time::time_duration ONE_WAY_LIGHT_TIME = (boost::posix_time::milliseconds(oneWayLightTimeMs));
        const boost::posix_time::time_duration ONE_WAY_MARGIN_TIME = (boost::posix_time::milliseconds(oneWayMarginTimeMs));
        if (useSendFile) {
            std::cout << "loading file " << sendFilePath << std::endl;
            std::vector<uint8_t> fileContentsInMemory;
            std::ifstream ifs(sendFilePath, std::ifstream::in | std::ifstream::binary);

            if (ifs.good()) {
                // get length of file:
                ifs.seekg(0, ifs.end);
                std::size_t length = ifs.tellg();
                ifs.seekg(0, ifs.beg);

                // allocate memory:
                fileContentsInMemory.resize(length);

                // read data as a block:
                ifs.read((char*)fileContentsInMemory.data(), length);

                ifs.close();

                std::cout << "computing sha1..\n";
                std::string sha1Str;
                GetSha1(fileContentsInMemory, sha1Str);
                std::cout << "SHA1: " << sha1Str << std::endl;
            }
            else {
                std::cerr << "error opening file: " << sendFilePath << std::endl;
                return false;
            }

            struct SenderHelper {
                SenderHelper() : finished(false), cancelled(false) {}
                void TransmissionSessionCompletedCallback(const Ltp::session_id_t & sessionId) {
                    finishedTime = boost::posix_time::microsec_clock::universal_time();
                    finished = true;
                    cv.notify_one();
                }
                void InitialTransmissionCompletedCallback(const Ltp::session_id_t & sessionId) {
                    std::cout << "first pass of all data sent\n";
                }
                void TransmissionSessionCancelledCallback(const Ltp::session_id_t & sessionId, CANCEL_SEGMENT_REASON_CODES reasonCode) {
                    cancelled = true;
                    std::cout << "remote cancelled session with reason code " << (int)reasonCode << std::endl;
                    cv.notify_one();
                }

                boost::posix_time::ptime finishedTime;
                boost::condition_variable cv;
                bool finished;
                bool cancelled;

            };
            SenderHelper senderHelper;

            LtpUdpEngine engineSrc(thisLtpEngineId, ltpDataSegmentMtu, ltpReportSegmentMtu, ONE_WAY_LIGHT_TIME, ONE_WAY_MARGIN_TIME, 0, numUdpRxPacketsCircularBufferSize, maxRxUdpPacketSizeBytes);
            engineSrc.SetTransmissionSessionCompletedCallback(boost::bind(&SenderHelper::TransmissionSessionCompletedCallback, &senderHelper, boost::placeholders::_1));
            engineSrc.SetInitialTransmissionCompletedCallback(boost::bind(&SenderHelper::InitialTransmissionCompletedCallback, &senderHelper, boost::placeholders::_1));
            engineSrc.SetTransmissionSessionCancelledCallback(boost::bind(&SenderHelper::TransmissionSessionCancelledCallback, &senderHelper, boost::placeholders::_1, boost::placeholders::_2));
            
            engineSrc.Connect(destUdpHostname, boost::lexical_cast<std::string>(destUdpPort));
            while (!engineSrc.ReadyToForward()) {
                std::cout << "connecting\n";
                boost::this_thread::sleep(boost::posix_time::milliseconds(500));
            }
            std::cout << "connected\n";

            const uint64_t totalBytesToSend = fileContentsInMemory.size();
            const double totalBitsToSend = totalBytesToSend * 8.0;


            boost::shared_ptr<LtpEngine::transmission_request_t> tReq = boost::make_shared<LtpEngine::transmission_request_t>();
            tReq->destinationClientServiceId = clientServiceId;
            tReq->destinationLtpEngineId = remoteLtpEngineId;
            tReq->clientServiceDataToSend = std::move(fileContentsInMemory);
            tReq->lengthOfRedPart = tReq->clientServiceDataToSend.size();

            engineSrc.TransmissionRequest_ThreadSafe(std::move(tReq));
            boost::posix_time::ptime startTime = boost::posix_time::microsec_clock::universal_time();
            boost::mutex cvMutex;
            boost::mutex::scoped_lock cvLock(cvMutex);
            if (useSignalHandler) {
                sigHandler.Start(false);
            }
            while (running && m_runningFromSigHandler && (!senderHelper.cancelled) && (!senderHelper.finished)) {
                senderHelper.cv.timed_wait(cvLock, boost::posix_time::milliseconds(200));
                if (useSignalHandler) {
                    sigHandler.PollOnce();
                }
            }
            boost::this_thread::sleep(boost::posix_time::seconds(2));
            boost::posix_time::time_duration diff = senderHelper.finishedTime - startTime;
            const double rateMbps = totalBitsToSend / (diff.total_microseconds());
            const double rateBps = rateMbps * 1e6;
            printf("Sent data at %0.4f Mbits/sec\n", rateMbps);
            std::cout << "udp packets sent: " << engineSrc.m_countAsyncSendCallbackCalls << std::endl;
        }
        else { //receive file
            struct ReceiverHelper {
                ReceiverHelper() : finished(false), cancelled(false) {}
                void RedPartReceptionCallback(const Ltp::session_id_t & sessionId, std::vector<uint8_t> & movableClientServiceDataVec, uint64_t lengthOfRedPart, uint64_t clientServiceId, bool isEndOfBlock) {
                    finishedTime = boost::posix_time::microsec_clock::universal_time();
                    receivedFileContents = std::move(movableClientServiceDataVec);
                    finished = true;
                    cv.notify_one();
                }
                void ReceptionSessionCancelledCallback(const Ltp::session_id_t & sessionId, CANCEL_SEGMENT_REASON_CODES reasonCode) {
                    cancelled = true;
                    std::cout << "remote cancelled session with reason code " << (int)reasonCode << std::endl;
                    cv.notify_one();
                }
                boost::posix_time::ptime finishedTime;
                boost::condition_variable cv;
                bool finished;
                bool cancelled;
                std::vector<uint8_t> receivedFileContents;

            };
            ReceiverHelper receiverHelper;

            std::cout << "expecting approximately " << estimatedFileSizeToReceive << " bytes to receive\n";
            LtpUdpEngine engineDest(thisLtpEngineId, 1, ltpReportSegmentMtu, ONE_WAY_LIGHT_TIME, ONE_WAY_MARGIN_TIME, destUdpPort, numUdpRxPacketsCircularBufferSize, maxRxUdpPacketSizeBytes, estimatedFileSizeToReceive);
            engineDest.SetRedPartReceptionCallback(boost::bind(&ReceiverHelper::RedPartReceptionCallback, &receiverHelper, boost::placeholders::_1, boost::placeholders::_2, boost::placeholders::_3,
                boost::placeholders::_4, boost::placeholders::_5));
            engineDest.SetReceptionSessionCancelledCallback(boost::bind(&ReceiverHelper::ReceptionSessionCancelledCallback, &receiverHelper, boost::placeholders::_1, boost::placeholders::_2));

            
            boost::mutex cvMutex;
            boost::mutex::scoped_lock cvLock(cvMutex);
            if (useSignalHandler) {
                sigHandler.Start(false);
            }
            while (running && m_runningFromSigHandler && (!receiverHelper.cancelled) && (!receiverHelper.finished)) {
                receiverHelper.cv.timed_wait(cvLock, boost::posix_time::milliseconds(200));
                if (useSignalHandler) {
                    sigHandler.PollOnce();
                }
            }
            if (receiverHelper.finished) {
                std::cout << "received file of size " << receiverHelper.receivedFileContents.size() << std::endl;
                std::cout << "computing sha1..\n";
                std::string sha1Str;
                GetSha1(receiverHelper.receivedFileContents, sha1Str);
                std::cout << "SHA1: " << sha1Str << std::endl;
                std::ofstream ofs(receiveFilePath, std::ofstream::out | std::ofstream::binary);
                if (!ofs.good()) {
                    std::cout << "error, unable to open file " << receiveFilePath << " for writing\n";
                    return false;
                }
                ofs.write((char*)receiverHelper.receivedFileContents.data(), receiverHelper.receivedFileContents.size());
                ofs.close();
                std::cout << "wrote " << receiveFilePath << "\n";
                
            }
            boost::this_thread::sleep(boost::posix_time::seconds(2));
            std::cout << "udp packets sent: " << engineDest.m_countAsyncSendCallbackCalls << std::endl;
        }

        std::cout << "LtpFileTransferRunner::Run: exiting cleanly..\n";
        //bpGen.Stop();
        //m_bundleCount = bpGen.m_bundleCount;
        //m_FinalStats = bpGen.m_FinalStats;
    }
    std::cout << "LtpFileTransferRunner::Run: exited cleanly\n";
    return true;

}
