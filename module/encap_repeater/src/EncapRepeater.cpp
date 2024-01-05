/**
 * @file EncapRepeater.cpp
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

#include <iostream>
#include "../../../common/util/include/EncapAsyncDuplexLocalStream.h"
#include <boost/make_unique.hpp>
#include <boost/program_options.hpp>
#include <queue>

class EncapRepeater {
    struct StreamInfo {
        StreamInfo() : writeInProgress(false), sendErrorOccurred(false), otherStreamIndex(UINT8_MAX) {}
        std::unique_ptr<EncapAsyncDuplexLocalStream> encapAsyncDuplexLocalStream;
        std::queue<padded_vector_uint8_t> toSendQueue;
        std::atomic<bool> writeInProgress;
        std::atomic<bool> sendErrorOccurred;
        uint8_t otherStreamIndex;
    };
public:

    EncapRepeater()  : m_signals(m_ioService), m_queueSize(0)
    {
        m_signals.add(SIGINT);
        m_signals.add(SIGTERM);
#if defined(SIGQUIT)
        m_signals.add(SIGQUIT);
#endif
    }
    ~EncapRepeater() {
        Stop();
    }

    void Stop() {
        for (uint8_t i = 0; i < 2; ++i) {
            m_streamInfos[i].encapAsyncDuplexLocalStream.reset();
        }
        m_ioService.stop();
    }

    //used by signal handler which shares the same io_service (and its thread) as the streams
    void Stop_CalledFromWithinIoServiceThread() {
        for (uint8_t i = 0; i < 2; ++i) {
            m_streamInfos[i].encapAsyncDuplexLocalStream->Stop_CalledFromWithinIoServiceThread();
        }
    }

    void RunForever(const std::string& socketOrPipePath0, bool isStreamCreator0,
        const std::string& socketOrPipePath1, bool isStreamCreator1,
        ENCAP_PACKET_TYPE encapPacketType, uint32_t queueSize)
    {
        m_queueSize = queueSize;

        //create two streams
        for (uint8_t i = 0; i < 2; ++i) {
            m_streamInfos[i].otherStreamIndex = (i + 1) % 2;
            m_streamInfos[i].encapAsyncDuplexLocalStream = boost::make_unique<EncapAsyncDuplexLocalStream>(
                m_ioService,
                encapPacketType,
                1,//maxBundleSizeBytes, //initial resize (don't waste memory with potential max bundle size)
                boost::bind(&EncapRepeater::OnFullEncapPacketReceived, this,
                    boost::placeholders::_1, boost::placeholders::_2, boost::placeholders::_3, i),
                boost::bind(&EncapRepeater::OnLocalStreamConnectionStatusChanged, this,
                    boost::placeholders::_1, i),
                true);  //true => don't discard 1-8 byte encap header in OnFullEncapPacketReceived
                        //        (i.e. receivedFullEncapPacket will be the concatenation of encap header + encap payload (PDU))
                        //false => receivedFullEncapPacket will be just the encap payload (PDU)
        }
        if (!m_streamInfos[0].encapAsyncDuplexLocalStream->Init(socketOrPipePath0, isStreamCreator0)) {
            return;
        }
        if (!m_streamInfos[1].encapAsyncDuplexLocalStream->Init(socketOrPipePath1, isStreamCreator1)) {
            return;
        }
        m_signals.async_wait(boost::bind(&EncapRepeater::Stop_CalledFromWithinIoServiceThread, this));
        m_ioService.run();
    }

private:

    void OnLocalStreamConnectionStatusChanged(bool isOnConnectionEvent,
        uint8_t thisStreamIndex) //thisStreamIndex = non-standard parameter value permanently set in constructor by boost::bind
    {
        std::cout << "EncapRepeater[" << ((int)thisStreamIndex) << "] connection " << ((isOnConnectionEvent) ? "up" : "down") << "\n";

        //in case this was the second to connect, send anything that might have been queued up to be sent
        if (isOnConnectionEvent) {
            StreamInfo& txStreamInfo = m_streamInfos[thisStreamIndex];
            TrySendQueued(txStreamInfo);
        }
    }

    void OnFullEncapPacketReceived(padded_vector_uint8_t& receivedFullEncapPacket,
        uint32_t decodedEncapPayloadSize, uint8_t decodedEncapHeaderSize,
        uint8_t thisRxStreamIndex) //thisRxStreamIndex = non-standard parameter value permanently set in constructor by boost::bind
    {
        
        //note: decodedEncapHeaderSize always set even if the encap header was discarded

        StreamInfo& thisRxStreamInfo = m_streamInfos[thisRxStreamIndex];
        StreamInfo& otherTxStreamInfo = m_streamInfos[thisRxStreamInfo.otherStreamIndex];

        
        //m_inductProcessBundleCallback(receivedFullEncapPacket); //receivedFullEncapPacket is just the bundle (encap header discarded)
        otherTxStreamInfo.toSendQueue.push(std::move(receivedFullEncapPacket));
        TrySendQueued(otherTxStreamInfo);
        
        //The user must manually restart the read operation of the reader pushing to the tx repeater queue.
        //If the queue limit is reached, do not restart the read operation (i.e. pause it).
        //Pausing will cause flow control to kick in on the remote process pushing to the OS stream.
        //Unpausing will occur when the HandleSend callback function is called.
        if (otherTxStreamInfo.toSendQueue.size() < 5) {
            thisRxStreamInfo.encapAsyncDuplexLocalStream->StartReadFirstEncapHeaderByte_NotThreadSafe();
        }
    }
    

    void TrySendQueued(StreamInfo& txStreamInfo) {
        if (!txStreamInfo.toSendQueue.empty()) {
            if (txStreamInfo.encapAsyncDuplexLocalStream->ReadyToSend()) {
                if (!txStreamInfo.sendErrorOccurred.load(std::memory_order_acquire)) {
                    if (!txStreamInfo.writeInProgress.exchange(true)) {
                        boost::asio::async_write(txStreamInfo.encapAsyncDuplexLocalStream->GetStreamHandleRef(),
                            boost::asio::buffer(txStreamInfo.toSendQueue.front()),
                            boost::bind(&EncapRepeater::HandleSend, this,
                                boost::asio::placeholders::error,
                                boost::asio::placeholders::bytes_transferred,
                                boost::ref(txStreamInfo)));
                    }
                }
            }
        }
    }
    
    void HandleSend(const boost::system::error_code& error, std::size_t bytes_transferred, StreamInfo& txStreamInfo) {
        txStreamInfo.toSendQueue.pop();
        txStreamInfo.writeInProgress.store(false, std::memory_order_release);

        if (error) {
            txStreamInfo.sendErrorOccurred = true; //prevents sending from queue
            std::cout << "EncapRepeater::HandleSend: " << error.message();
        }

        TrySendQueued(txStreamInfo);

        //a pop occurred in the queue, so make sure the read operation of the reader pushing to this tx queue is not paused
        //from hitting the max queue size
        if (txStreamInfo.toSendQueue.size() < 5) {
            StreamInfo& otherRxStreamInfo = m_streamInfos[txStreamInfo.otherStreamIndex];
            otherRxStreamInfo.encapAsyncDuplexLocalStream->StartReadFirstEncapHeaderByte_NotThreadSafe();
        }
    }
    
    boost::asio::io_service m_ioService;
    StreamInfo m_streamInfos[2];
    boost::asio::signal_set m_signals; //Register keyboard interrupt signals to listen for.
    uint32_t m_queueSize;
};

int main(int argc, const char* argv[]) {
    std::string streamNames[2];
    bool isStreamCreators[2];
    uint32_t queueSize;
    ENCAP_PACKET_TYPE encapPacketType;
#ifdef STREAM_USE_WINDOWS_NAMED_PIPE
# define STREAM_NAME_HELPER_STRING "Windows path to named pipe (e.g. \\\\.\\pipe\\my_pipe_name )."
#else
# define STREAM_NAME_HELPER_STRING "Unix path to local socket (e.g. /tmp/my_socket_name )."
#endif
    boost::program_options::options_description desc("Allowed options");
    try {
        desc.add_options()
            ("help", "Produce help message.")
            ("stream-name-0", boost::program_options::value<std::string>(), STREAM_NAME_HELPER_STRING)
            ("stream-init-0", boost::program_options::value<std::string>(), "valid values are [open, create]")
            ("stream-name-1", boost::program_options::value<std::string>(), STREAM_NAME_HELPER_STRING)
            ("stream-init-1", boost::program_options::value<std::string>(), "valid values are [open, create]")
            ("queue-size", boost::program_options::value<uint32_t>()->default_value(5), "Max number of encap packets to buffer when receiver faster than sender")
            ("encap-packet-type", boost::program_options::value<std::string>(), "valid values are [bp, ltp]")
            ;

        boost::program_options::variables_map vm;
        boost::program_options::store(boost::program_options::parse_command_line(argc, argv, desc, boost::program_options::command_line_style::unix_style | boost::program_options::command_line_style::case_insensitive), vm);
        boost::program_options::notify(vm);

        if (vm.count("help")) {
            std::cout << desc;
            return 0;
        }

        streamNames[0] = vm["stream-name-0"].as<std::string>();
        streamNames[1] = vm["stream-name-1"].as<std::string>();
        std::string inits[2];
        inits[0] = vm["stream-init-0"].as<std::string>();
        inits[1] = vm["stream-init-1"].as<std::string>();
        for (unsigned int i = 0; i < 2; ++i) {
            if (inits[i] == "open") {
                isStreamCreators[i] = false;
            }
            else if (inits[i] == "create") {
                isStreamCreators[i] = true;
            }
            else {
                std::cerr << "stream-init-" << i << " must be one of [open, create].. got " << inits[i] << "\n";
                return 1;
            }
        }
        queueSize = vm["queue-size"].as<uint32_t>();
        
        std::string encapPacketTypeStr = vm["encap-packet-type"].as<std::string>();
        if (encapPacketTypeStr == "bp") {
            encapPacketType = ENCAP_PACKET_TYPE::BP;
        }
        else if (encapPacketTypeStr == "ltp") {
            encapPacketType = ENCAP_PACKET_TYPE::LTP;
        }
        else {
            std::cerr << "encap-packet-type must be one of [bp, ltp].. got " << encapPacketTypeStr << "\n";
            return 1;
        }
    }
    catch (boost::bad_any_cast& e) {
        std::cerr << "invalid data error: " << e.what() << "\n";
        std::cout << desc;
        return 1;
    }
    catch (std::exception& e) {
        std::cerr << e.what() << "\n";
        std::cout << desc;
        return 1;
    }
    catch (...) {
        std::cerr << "Exception of unknown type!\n";
        return 1;
    }

    EncapRepeater r;
    r.RunForever(streamNames[0], isStreamCreators[0], streamNames[1], isStreamCreators[1], encapPacketType, queueSize);
    return 0;
}
