/**
 * @file BpReceiveStream.h
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
