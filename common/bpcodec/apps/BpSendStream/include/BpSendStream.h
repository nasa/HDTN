/**
 * @file BpSendStream.h
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

#include "app_patterns/BpSourcePattern.h"

#include "UdpBundleSink.h"
#include "GStreamerAppSinkInduct.h"
#include "GStreamerShmInduct.h"

#include "DtnRtp.h"

#include <boost/asio.hpp>
#include <boost/process.hpp>


typedef enum {
    HDTN_APPSINK_INTAKE = 0,
    HDTN_UDP_INTAKE = 1,
    HDTN_SHM_INTAKE = 2
} BpSendStreamIntakeTypes;


class BpSendStream : public BpSourcePattern
{
public:

    BpSendStream(uint8_t intakeType, size_t maxIncomingUdpPacketSizeBytes, uint16_t incomingRtpStreamPort, 
            size_t numCircularBufferVectors, size_t maxOutgoingBundleSizeBytes, uint16_t numRtpPacketsPerBundle, std::string fileToStream);
    ~BpSendStream();

    boost::asio::io_service m_ioService; // socket uses this to grab data from incoming rtp stream
    
    std::shared_ptr<DtnRtp> m_outgoingDtnRtpPtr;
    std::shared_ptr<DtnRtp> m_incomingDtnRtpPtr;

    boost::circular_buffer<padded_vector_uint8_t> m_incomingCircularPacketQueue; // consider making this a pre allocated vector
    std::queue<padded_vector_uint8_t> m_outgoingRtpPacketQueue; // these buffers get placed into m_outgoingCircularBundleQueue when we have packed in the requested RTP packets 
    boost::circular_buffer<std::vector<uint8_t> > m_outgoingCircularBundleQueue;

protected:
    virtual bool TryWaitForDataAvailable(const boost::posix_time::time_duration& timeout) override;
    virtual uint64_t GetNextPayloadLength_Step1() override;
    virtual bool CopyPayload_Step2(uint8_t * destinationBuffer) override;

private:
    bool TryWaitForIncomingDataAvailable(const boost::posix_time::time_duration& timeout);
    bool SdpTimerThread();
    
    void ProcessIncomingBundlesThread(); // worker thread that calls RTP packet handler
    void WholeBundleReadyCallback(padded_vector_uint8_t & wholeBundleVec); // incoming udp packets come in here
    void DeleteCallback(); // gets called on socket shutdow, optional to do anything with it
    void Concatenate(padded_vector_uint8_t &incomingRtpFrame);
    void CreateFrame(padded_vector_uint8_t &incomingRtpFrame);
    void PushFrame();
    void PushBundle();

    /* Gstreamer App Sink Intake */
    std::unique_ptr<GStreamerAppSinkInduct> m_GStreamerAppSinkInductPtr;
    std::unique_ptr<GStreamerShmInduct> m_GStreamerShmInductPtr;
    /* Udp Intake*/
    std::shared_ptr<UdpBundleSink> m_bundleSinkPtr; 

    padded_vector_uint8_t m_currentFrame;  
    size_t m_offset = 0;

    uint8_t m_intakeType;
    std::atomic<bool> m_running;
    
    uint64_t m_numCircularBufferVectors;
    uint64_t m_maxIncomingUdpPacketSizeBytes; // passed in via config file, should be greater than or equal to the RTP stream source maximum packet size
    uint16_t m_incomingRtpStreamPort;
    uint64_t m_maxOutgoingBundleSizeBytes;

    uint64_t m_rtpBytesInQueue;

    boost::mutex m_outgoingQueueMutex;   
    boost::mutex m_incomingQueueMutex;     
    boost::mutex m_sdpMutex;

    boost::condition_variable m_outgoingQueueCv;
    boost::condition_variable m_incomingQueueCv;
    boost::condition_variable m_sdpCv;

    std::unique_ptr<boost::thread> m_processingThread;
    std::unique_ptr<boost::thread> m_ioServiceThreadPtr;

    uint16_t m_numRtpPacketsPerBundle;
    
    std::string m_fileToStream;

    /* stat tracking */
    uint64_t m_totalRtpPacketsReceived; // counted when received from udp sink
    uint64_t m_totalRtpPacketsSent; // counted when send to bundler
    uint64_t m_totalRtpPacketsQueued; // counted when pushed into outgoing queue
    uint64_t m_totalIncomingCbOverruns;
    uint64_t m_totalOutgoingCbOverruns;

};
