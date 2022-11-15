#include "LtpFileTransferRunner.h"
#include "Logger.h"
#include "SignalHandler.h"
#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/uuid/detail/sha1.hpp>
#include <boost/endian/conversion.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/format.hpp>
#include "LtpUdpEngineManager.h"
#include <memory>
#ifndef _WIN32
#include <sys/socket.h> //for maxUdpPacketsToSendPerSystemCall checks
#endif

static constexpr hdtn::Logger::SubProcess subprocess = hdtn::Logger::SubProcess::none;

static void GetSha1(const uint8_t * data, const std::size_t size, std::string & sha1Str) {

    sha1Str.resize(40);
    char * strPtr = &sha1Str[0];

    boost::uuids::detail::sha1 s;
    s.process_bytes(data, size);
    boost::uint32_t digest[5];
    s.get_digest(digest);
    for (int i = 0; i < 5; ++i) {
        //const uint32_t digestBe = boost::endian::native_to_big(digest[i]);
        sprintf(strPtr, "%08x", digest[i]);// digestBe);
        strPtr += 8;
    }
}



void LtpFileTransferRunner::MonitorExitKeypressThreadFunction() {
    LOG_INFO(subprocess) << "Keyboard Interrupt.. exiting";
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
        bool dontSaveFile = false;
        bool force32BitRandomNumbers;
        std::string remoteUdpHostname;
        uint16_t remoteUdpPort;
        uint16_t myBoundUdpPort;
        uint64_t thisLtpEngineId;
        uint64_t remoteLtpEngineId;
        uint64_t ltpDataSegmentMtu;
        uint64_t ltpReportSegmentMtu;
        uint64_t oneWayLightTimeMs;
        uint64_t oneWayMarginTimeMs;
        uint64_t clientServiceId;
        uint64_t estimatedFileSizeToReceive;
        uint32_t checkpointEveryNthTxPacket;
        uint32_t maxRetriesPerSerialNumber;
        uint64_t maxSendRateBitsPerSecOrZeroToDisable;
        uint64_t maxUdpPacketsToSendPerSystemCall;
        unsigned int numUdpRxPacketsCircularBufferSize;
        unsigned int maxRxUdpPacketSizeBytes;

        boost::program_options::options_description desc("Allowed options");
        try {
            desc.add_options()
                ("help", "Produce help message.")
                ("receive-file", boost::program_options::value<std::string>(), "Receive a file to this file name.")
                ("send-file", boost::program_options::value<std::string>(), "Send this file name.")
                ("dont-save-file", "When receiving, don't write file to disk.")
                ("remote-udp-hostname", boost::program_options::value<std::string>()->default_value("localhost"), "Ltp destination UDP hostname. (receivers when remote port !=0)")
                ("remote-udp-port", boost::program_options::value<uint16_t>()->default_value(1113), "Remote UDP port.")
                ("my-bound-udp-port", boost::program_options::value<uint16_t>()->default_value(1113), "My bound UDP port. (default 1113 for senders)")
                ("random-number-size-bits", boost::program_options::value<uint32_t>()->default_value(32), "LTP can use either 32-bit or 64-bit random numbers (only 32-bit supported by ion).")

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
                ("checkpoint-every-nth-tx-packet", boost::program_options::value<uint32_t>()->default_value(0), "Make every nth packet a checkpoint. (default 0 = disabled).")
                ("max-retries-per-serial-number", boost::program_options::value<uint32_t>()->default_value(5), "Try to resend a serial number up to this many times. (default 5).")
                ("max-send-rate-bits-per-sec", boost::program_options::value<uint64_t>()->default_value(0), "Send rate in bits-per-second FOR SENDERS ONLY (zero disables). (default 0)")
                ("max-udp-packets-to-send-per-system-call", boost::program_options::value<uint64_t>()->default_value(1), "Max udp packets to send per system call (senders and receivers). (default 1)")
                ;

            boost::program_options::variables_map vm;
            boost::program_options::store(boost::program_options::parse_command_line(argc, argv, desc, boost::program_options::command_line_style::unix_style | boost::program_options::command_line_style::case_insensitive), vm);
            boost::program_options::notify(vm);

            if (vm.count("help")) {
                LOG_INFO(subprocess) << desc;
                return false;
            }
            const uint32_t randomNumberSizeBits = vm["random-number-size-bits"].as<uint32_t>();
            if ((randomNumberSizeBits != 32) && (randomNumberSizeBits != 64)) {
                LOG_ERROR(subprocess) << "randomNumberSizeBits (" << randomNumberSizeBits << ") must be either 32 or 64";
                return false;
            }
            force32BitRandomNumbers = (randomNumberSizeBits == 32);

            if (!(vm.count("receive-file") ^ vm.count("send-file"))) {
                LOG_ERROR(subprocess) << "receive-file or send-file must be specified, but not both";
                return false;
            }
            if (vm.count("receive-file")) {
                useReceiveFile = true;
                receiveFilePath = vm["receive-file"].as<std::string>();
                dontSaveFile = (vm.count("dont-save-file") != 0);
            }
            else {
                useSendFile = true;
                sendFilePath = vm["send-file"].as<std::string>();
            }
            remoteUdpHostname = vm["remote-udp-hostname"].as<std::string>();
            remoteUdpPort = vm["remote-udp-port"].as<boost::uint16_t>();
            myBoundUdpPort = vm["my-bound-udp-port"].as<boost::uint16_t>();
            thisLtpEngineId = vm["this-ltp-engine-id"].as<uint64_t>();
            remoteLtpEngineId = vm["remote-ltp-engine-id"].as<uint64_t>();
            ltpDataSegmentMtu = vm["ltp-data-segment-mtu"].as<uint64_t>();
            ltpReportSegmentMtu = vm["ltp-report-segment-mtu"].as<uint64_t>();
            oneWayLightTimeMs = vm["one-way-light-time-ms"].as<uint64_t>();
            oneWayMarginTimeMs = vm["one-way-margin-time-ms"].as<uint64_t>();
            clientServiceId = vm["client-service-id"].as<uint64_t>();
            estimatedFileSizeToReceive = vm["estimated-rx-filesize"].as<uint64_t>();
            checkpointEveryNthTxPacket = vm["checkpoint-every-nth-tx-packet"].as<uint32_t>();
            maxRetriesPerSerialNumber = vm["max-retries-per-serial-number"].as<uint32_t>();
            maxSendRateBitsPerSecOrZeroToDisable = vm["max-send-rate-bits-per-sec"].as<uint64_t>();
            if (useReceiveFile && maxSendRateBitsPerSecOrZeroToDisable) {
                LOG_ERROR(subprocess) << "maxSendRateBitsPerSecOrZeroToDisable was specified for a receiver";
                return false;
            }
            maxUdpPacketsToSendPerSystemCall = vm["max-udp-packets-to-send-per-system-call"].as<uint64_t>();
            if (maxUdpPacketsToSendPerSystemCall == 0) {
                LOG_ERROR(subprocess) << "max-udp-packets-to-send-per-system-call ("
                    << maxUdpPacketsToSendPerSystemCall << ") must be non-zero.";
                return false;
            }
#ifdef UIO_MAXIOV
            //sendmmsg() is Linux-specific. NOTES The value specified in vlen is capped to UIO_MAXIOV (1024).
            if (maxUdpPacketsToSendPerSystemCall > UIO_MAXIOV) {
                LOG_ERROR(subprocess) << "max-udp-packets-to-send-per-system-call ("
                    << maxUdpPacketsToSendPerSystemCall << ") must be <= UIO_MAXIOV (" << UIO_MAXIOV << ").";
                return false;
            }
#endif //UIO_MAXIOV
            numUdpRxPacketsCircularBufferSize = vm["num-rx-udp-packets-buffer-size"].as<unsigned int>();
            maxRxUdpPacketSizeBytes = vm["max-rx-udp-packet-size-bytes"].as<unsigned int>();
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

        LtpUdpEngineManager::SetMaxUdpRxPacketSizeBytesForAllLtp(maxRxUdpPacketSizeBytes);
        const boost::posix_time::time_duration ONE_WAY_LIGHT_TIME = (boost::posix_time::milliseconds(oneWayLightTimeMs));
        const boost::posix_time::time_duration ONE_WAY_MARGIN_TIME = (boost::posix_time::milliseconds(oneWayMarginTimeMs));
        if (useSendFile) {
            LOG_INFO(subprocess) << "loading file " << sendFilePath;
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

                LOG_INFO(subprocess) << "computing sha1..";
                std::string sha1Str;
                GetSha1(fileContentsInMemory.data(), fileContentsInMemory.size(), sha1Str);
                LOG_INFO(subprocess) << "SHA1: " << sha1Str;
            }
            else {
                LOG_ERROR(subprocess) << "error opening file: " << sendFilePath;
                return false;
            }

            struct SenderHelper {
                SenderHelper() : finished(false), cancelled(false) {}
                void TransmissionSessionCompletedCallback(const Ltp::session_id_t & sessionId) {
                    finishedTime = boost::posix_time::microsec_clock::universal_time();
                    cvMutex.lock();
                    finished = true;
                    cvMutex.unlock();
                    cv.notify_one();
                }
                void InitialTransmissionCompletedCallback(const Ltp::session_id_t & sessionId) {
                    LOG_INFO(subprocess) << "first pass of all data sent";
                }
                void TransmissionSessionCancelledCallback(const Ltp::session_id_t & sessionId, CANCEL_SEGMENT_REASON_CODES reasonCode) {
                    cvMutex.lock();
                    cancelled = true;
                    cvMutex.unlock();
                    LOG_INFO(subprocess) << "remote cancelled session with reason code " << (int)reasonCode;
                    cv.notify_one();
                }
                void WaitUntilFinishedOrCancelledOr200MsTimeout() {
                    boost::mutex::scoped_lock cvLock(cvMutex);
                    if ((!cancelled) && (!finished)) { //lock mutex (above) before checking condition
                        cv.timed_wait(cvLock, boost::posix_time::milliseconds(200));
                    }
                }

                boost::posix_time::ptime finishedTime;
                boost::mutex cvMutex;
                boost::condition_variable cv;
                bool finished;
                bool cancelled;

            };
            SenderHelper senderHelper;
            std::shared_ptr<LtpUdpEngineManager> ltpUdpEngineManagerSrcPtr = LtpUdpEngineManager::GetOrCreateInstance(myBoundUdpPort, true);
            LtpUdpEngine * ltpUdpEngineSrcPtr = ltpUdpEngineManagerSrcPtr->GetLtpUdpEnginePtrByRemoteEngineId(remoteLtpEngineId, false);
            if (ltpUdpEngineSrcPtr == NULL) {
                ltpUdpEngineManagerSrcPtr->AddLtpUdpEngine(thisLtpEngineId, remoteLtpEngineId, false, ltpDataSegmentMtu, 80, ONE_WAY_LIGHT_TIME, ONE_WAY_MARGIN_TIME, //1=> MTU NOT USED AT THIS TIME, UINT64_MAX=> unlimited report segment size
                    remoteUdpHostname, remoteUdpPort, numUdpRxPacketsCircularBufferSize, 0, 0, checkpointEveryNthTxPacket, maxRetriesPerSerialNumber,
                    force32BitRandomNumbers, maxSendRateBitsPerSecOrZeroToDisable, 5, 0, maxUdpPacketsToSendPerSystemCall, 0, 0, 0);
                ltpUdpEngineSrcPtr = ltpUdpEngineManagerSrcPtr->GetLtpUdpEnginePtrByRemoteEngineId(remoteLtpEngineId, false);
            }

            ltpUdpEngineSrcPtr->SetTransmissionSessionCompletedCallback(boost::bind(&SenderHelper::TransmissionSessionCompletedCallback, &senderHelper, boost::placeholders::_1));
            ltpUdpEngineSrcPtr->SetInitialTransmissionCompletedCallback(boost::bind(&SenderHelper::InitialTransmissionCompletedCallback, &senderHelper, boost::placeholders::_1));
            ltpUdpEngineSrcPtr->SetTransmissionSessionCancelledCallback(boost::bind(&SenderHelper::TransmissionSessionCancelledCallback, &senderHelper, boost::placeholders::_1, boost::placeholders::_2));
            
            
            const uint64_t totalBytesToSend = fileContentsInMemory.size();
            const double totalBitsToSend = totalBytesToSend * 8.0;


            std::shared_ptr<LtpEngine::transmission_request_t> tReq = std::make_shared<LtpEngine::transmission_request_t>();
            tReq->destinationClientServiceId = clientServiceId;
            tReq->destinationLtpEngineId = remoteLtpEngineId;
            tReq->clientServiceDataToSend = std::move(fileContentsInMemory);
            tReq->lengthOfRedPart = tReq->clientServiceDataToSend.size();

            ltpUdpEngineSrcPtr->TransmissionRequest_ThreadSafe(std::move(tReq));
            boost::posix_time::ptime startTime = boost::posix_time::microsec_clock::universal_time();
            
            
            if (useSignalHandler) {
                sigHandler.Start(false);
            }
            while (running && m_runningFromSigHandler && (!senderHelper.cancelled) && (!senderHelper.finished)) {
                senderHelper.WaitUntilFinishedOrCancelledOr200MsTimeout();
                if (useSignalHandler) {
                    sigHandler.PollOnce();
                }
            }
            boost::this_thread::sleep(boost::posix_time::seconds(2));
            boost::posix_time::time_duration diff = senderHelper.finishedTime - startTime;
            const double rateMbps = totalBitsToSend / (diff.total_microseconds());
            const double rateBps = rateMbps * 1e6;

            static const boost::format fmtTemplate("Sent data at %0.4f Mbits/sec");
            boost::format fmt(fmtTemplate);
            fmt % rateMbps;
            LOG_INFO(subprocess) << fmt.str();

            LOG_INFO(subprocess) << "udp packets sent: " << (ltpUdpEngineSrcPtr->m_countAsyncSendCallbackCalls + ltpUdpEngineSrcPtr->m_countBatchUdpPacketsSent);
            LOG_INFO(subprocess) << "system calls for send: " << (ltpUdpEngineSrcPtr->m_countAsyncSendCallbackCalls + ltpUdpEngineSrcPtr->m_countBatchSendCallbackCalls);
        }
        else { //receive file
            struct ReceiverHelper {
                ReceiverHelper() : finished(false), cancelled(false) {}
                void RedPartReceptionCallback(const Ltp::session_id_t & sessionId, padded_vector_uint8_t & movableClientServiceDataVec, uint64_t lengthOfRedPart, uint64_t clientServiceId, bool isEndOfBlock) {
                    finishedTime = boost::posix_time::microsec_clock::universal_time();
                    receivedFileContents = std::move(movableClientServiceDataVec);
                    cvMutex.lock();
                    finished = true;
                    cvMutex.unlock();
                    cv.notify_one();
                }
                void ReceptionSessionCancelledCallback(const Ltp::session_id_t & sessionId, CANCEL_SEGMENT_REASON_CODES reasonCode) {
                    cvMutex.lock();
                    cancelled = true;
                    cvMutex.unlock();
                    LOG_INFO(subprocess) << "remote cancelled session with reason code " << (int)reasonCode;
                    cv.notify_one();
                }
                void WaitUntilFinishedOrCancelledOr200MsTimeout() {
                    boost::mutex::scoped_lock cvLock(cvMutex);
                    if ((!cancelled) && (!finished)) { //lock mutex (above) before checking condition
                        cv.timed_wait(cvLock, boost::posix_time::milliseconds(200));
                    }
                }
                boost::posix_time::ptime finishedTime;
                boost::mutex cvMutex;
                boost::condition_variable cv;
                bool finished;
                bool cancelled;
                padded_vector_uint8_t receivedFileContents;

            };
            ReceiverHelper receiverHelper;

            LOG_INFO(subprocess) << "expecting approximately " << estimatedFileSizeToReceive << " bytes to receive";
            std::shared_ptr<LtpUdpEngineManager> ltpUdpEngineManagerDestPtr = LtpUdpEngineManager::GetOrCreateInstance(myBoundUdpPort, true);
            LtpUdpEngine * ltpUdpEngineDestPtr = ltpUdpEngineManagerDestPtr->GetLtpUdpEnginePtrByRemoteEngineId(remoteLtpEngineId, true);
            if (ltpUdpEngineDestPtr == NULL) {
                ltpUdpEngineManagerDestPtr->AddLtpUdpEngine(thisLtpEngineId, remoteLtpEngineId, true, 1, ltpReportSegmentMtu, ONE_WAY_LIGHT_TIME, ONE_WAY_MARGIN_TIME, //1=> MTU NOT USED AT THIS TIME, UINT64_MAX=> unlimited report segment size
                    remoteUdpHostname, remoteUdpPort, numUdpRxPacketsCircularBufferSize, estimatedFileSizeToReceive, estimatedFileSizeToReceive,
                    0, maxRetriesPerSerialNumber, force32BitRandomNumbers, 0, 5, 10, maxUdpPacketsToSendPerSystemCall, 0, 0, 0);
                ltpUdpEngineDestPtr = ltpUdpEngineManagerDestPtr->GetLtpUdpEnginePtrByRemoteEngineId(remoteLtpEngineId, true); //remote is expectedSessionOriginatorEngineId
            }
            
            ltpUdpEngineDestPtr->SetRedPartReceptionCallback(boost::bind(&ReceiverHelper::RedPartReceptionCallback, &receiverHelper, boost::placeholders::_1, boost::placeholders::_2, boost::placeholders::_3,
                boost::placeholders::_4, boost::placeholders::_5));
            ltpUdpEngineDestPtr->SetReceptionSessionCancelledCallback(boost::bind(&ReceiverHelper::ReceptionSessionCancelledCallback, &receiverHelper, boost::placeholders::_1, boost::placeholders::_2));
            
            LOG_INFO(subprocess) << "this ltp receiver/server for engine ID " << thisLtpEngineId << " will receive on port "
                << myBoundUdpPort << " and send report segments to " << remoteUdpHostname << ":" << remoteUdpPort;
            
            
            
            
            if (useSignalHandler) {
                sigHandler.Start(false);
            }
            while (running && m_runningFromSigHandler && (!receiverHelper.cancelled) && (!receiverHelper.finished)) {
                receiverHelper.WaitUntilFinishedOrCancelledOr200MsTimeout();
                if (useSignalHandler) {
                    sigHandler.PollOnce();
                }
            }
            if (receiverHelper.finished) {
                LOG_INFO(subprocess) << "received file of size " << receiverHelper.receivedFileContents.size();
                LOG_INFO(subprocess) << "computing sha1..";
                std::string sha1Str;
                GetSha1(receiverHelper.receivedFileContents.data(), receiverHelper.receivedFileContents.size(), sha1Str);
                LOG_INFO(subprocess) << "SHA1: " << sha1Str;
                if (!dontSaveFile) {
                    std::ofstream ofs(receiveFilePath, std::ofstream::out | std::ofstream::binary);
                    if (!ofs.good()) {
                        LOG_ERROR(subprocess) << "unable to open file " << receiveFilePath << " for writing";
                        return false;
                    }
                    ofs.write((char*)receiverHelper.receivedFileContents.data(), receiverHelper.receivedFileContents.size());
                    ofs.close();
                    LOG_INFO(subprocess) << "wrote " << receiveFilePath;
                }
                
            }
            boost::this_thread::sleep(boost::posix_time::seconds(2));
            LOG_INFO(subprocess) << "udp packets sent: " << (ltpUdpEngineDestPtr->m_countAsyncSendCallbackCalls + ltpUdpEngineDestPtr->m_countBatchUdpPacketsSent);
            LOG_INFO(subprocess) << "system calls for send: " << (ltpUdpEngineDestPtr->m_countAsyncSendCallbackCalls + ltpUdpEngineDestPtr->m_countBatchSendCallbackCalls);
        }

        LOG_INFO(subprocess) << "LtpFileTransferRunner::Run: exiting cleanly..";
        //bpGen.Stop();
        //m_bundleCount = bpGen.m_bundleCount;
        //m_FinalStats = bpGen.m_FinalStats;
    }
    LOG_INFO(subprocess) << "LtpFileTransferRunner::Run: exited cleanly";
    return true;

}
