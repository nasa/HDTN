/**
 * @file BpReceiveStream.h
 *
 * @copyright Copyright (c) 2026 United States Government as represented by
 * the National Aeronautics and Space Administration.
 * No copyright is claimed in the United States under Title 17, U.S.Code.
 * All Other Rights Reserved.
 *
 * @section LICENSE
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include "app_patterns/BpSinkPattern.h"

#include "DtnRtp.h"

#include "GStreamerAppSrcOutduct.h"

typedef enum {
    UDP_OUTDUCT = 0,
    GSTREAMER_APPSRC_OUTDUCT = 1
} BpRecvStreamOutductTypes;


struct bp_recv_stream_params_t {
    std::string rtpDestHostname;
    uint16_t rtpDestPort;
    uint16_t maxOutgoingRtpPacketSizeBytes;
    uint8_t outductType;
    std::string shmSocketPath;
    std::string gstCaps;
};

class BpReceiveStream : public BpSinkPattern {
public:
    BpReceiveStream(size_t numCircularBufferVectors, bp_recv_stream_params_t params);
    virtual ~BpReceiveStream() override;

protected:
    virtual bool ProcessPayload(const uint8_t * data, const uint64_t size) override;

private:
    void ProcessIncomingBundlesThread(); // worker thread 

    // int TranslateBpSdpToInSdp(std::string sdp);

    bool TryWaitForIncomingDataAvailable(const boost::posix_time::time_duration& timeout);


    int SendUdpPacket(padded_vector_uint8_t & message);

    std::atomic<bool> m_running; // exit condition
    
    // inbound config
    boost::circular_buffer<padded_vector_uint8_t> m_incomingBundleQueue; // incoming rtp frames from HDTN put here
    size_t m_numCircularBufferVectors;

    // outbound config
    std::string m_outgoingRtpHostname;
    uint16_t m_outgoingRtpPort;
    uint16_t m_maxOutgoingRtpPacketSizeBytes;
    uint16_t m_maxOutgoingRtpPayloadSizeBytes;

    // outbound udp outduct
	boost::asio::io_service m_ioService;
    boost::asio::ip::udp::socket socket;
    boost::asio::ip::udp::endpoint m_udpEndpoint;
    boost::mutex m_sentPacketsMutex;
    boost::condition_variable m_cvSentPacket;

    // outbound gstreamer outduct
    uint8_t m_outductType;
    std::unique_ptr<GStreamerAppSrcOutduct> m_gstreamerAppSrcOutductPtr;

    // multithreading 
    boost::condition_variable m_incomingQueueCv;
    boost::mutex m_incomingQueueMutex;     
    std::unique_ptr<boost::thread> m_processingThread;

    // book keeping
    uint64_t m_totalRtpPacketsReceived; 
    uint64_t m_totalRtpPacketsSent; 
    uint64_t m_totalRtpPacketsFailedToSend;
    uint64_t m_totalRtpBytesSent;
};
