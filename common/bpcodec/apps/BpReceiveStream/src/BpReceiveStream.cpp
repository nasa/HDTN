/**
 * @file BpReceiveStream.cpp
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

#include "BpReceiveStream.h"
#include "Logger.h"
#include <boost/process.hpp>

static constexpr hdtn::Logger::SubProcess subprocess = hdtn::Logger::SubProcess::none;

#define FFMPEG_SDP_HEADER "data:application/sdp;,"

BpReceiveStream::BpReceiveStream(size_t numCircularBufferVectors, bp_recv_stream_params_t params) 
    : BpSinkPattern(), 
    m_running(true),
    m_numCircularBufferVectors(numCircularBufferVectors),
    m_outgoingRtpHostname(params.rtpDestHostname),
    m_outgoingRtpPort(params.rtpDestPort),
    m_maxOutgoingRtpPacketSizeBytes(params.maxOutgoingRtpPacketSizeBytes),
    m_maxOutgoingRtpPayloadSizeBytes(params.maxOutgoingRtpPacketSizeBytes - sizeof(rtp_header)),
    socket(m_ioService),
    //m_sentPacketsSuccess(false),
    m_outductType(params.outductType),
    m_totalRtpPacketsReceived(0),
    m_totalRtpPacketsSent(0),
    m_totalRtpPacketsFailedToSend(0),
    m_totalRtpBytesSent(0)
{
    m_processingThread = boost::make_unique<boost::thread>(boost::bind(&BpReceiveStream::ProcessIncomingBundlesThread, this)); 
    
    m_incomingBundleQueue.set_capacity(m_numCircularBufferVectors);
    
    if (m_outductType == UDP_OUTDUCT)  {
        static const boost::asio::ip::resolver_query_base::flags UDP_RESOLVER_FLAGS = boost::asio::ip::resolver_query_base::canonical_name; //boost resolver flags
        boost::asio::ip::udp::resolver resolver(m_ioService);
        try {
            m_udpEndpoint = *resolver.resolve(boost::asio::ip::udp::resolver::query(boost::asio::ip::udp::v4(), m_outgoingRtpHostname, boost::lexical_cast<std::string>(m_outgoingRtpPort), UDP_RESOLVER_FLAGS));
        }
        catch (const boost::system::system_error& e) {
            LOG_ERROR(subprocess) << "Failed to resolve UDP hostname: " << e.what() << "  code=" << e.code();
            return;
        }
        try {
            socket.open(boost::asio::ip::udp::v4());
        }
        catch (const boost::system::system_error& e) {
            LOG_ERROR(subprocess) << "Failed to open UDP Socket: " << e.what() << "  code=" << e.code();
            return;
        }
    } else if (m_outductType == GSTREAMER_APPSRC_OUTDUCT) {
        m_gstreamerAppSrcOutductPtr = boost::make_unique<GStreamerAppSrcOutduct>(params.shmSocketPath, params.gstCaps);
        GStreamerAppSrcOutduct::SetGStreamerAppSrcOutductInstance(m_gstreamerAppSrcOutductPtr.get());
    }

}

BpReceiveStream::~BpReceiveStream()
{
    LOG_INFO(subprocess) << "Calling BpReceiveStream deconstructor";
    m_running = false;
    
    m_gstreamerAppSrcOutductPtr.reset();

    Stop();

    LOG_INFO(subprocess) << "m_totalRtpPacketsReceived: " << m_totalRtpPacketsReceived;
    LOG_INFO(subprocess) << "m_totalRtpPacketsSent: " << m_totalRtpPacketsSent;
    LOG_INFO(subprocess) << "m_totalRtpBytesSent: " << m_totalRtpBytesSent; 
    LOG_INFO(subprocess) << "m_totalRtpPacketFailedToSend: " << m_totalRtpPacketsFailedToSend;
    LOG_INFO(subprocess) << "m_incomingBundleQueue.size(): " << m_incomingBundleQueue.size();
}

void BpReceiveStream::ProcessIncomingBundlesThread()
{
    static const boost::posix_time::time_duration timeout(boost::posix_time::milliseconds(250));

    padded_vector_uint8_t rtpFrame;
    rtpFrame.reserve(m_maxOutgoingRtpPacketSizeBytes);

    while (m_running.load(std::memory_order_acquire)) 
    {
        bool notInWaitForNewPacketState = TryWaitForIncomingDataAvailable(timeout);
        if (notInWaitForNewPacketState) 
        {
            padded_vector_uint8_t incomingBundle;
            {
                boost::mutex::scoped_lock lock(m_incomingQueueMutex);
                incomingBundle = std::move(m_incomingBundleQueue.front()); // process front of queue
                m_incomingBundleQueue.pop_front();
            }
            // LOG_DEBUG(subprocess) << "bundle is size " << incomingBundle.size();

            size_t offset = 0; 
            while (1)
            {
                size_t rtpPacketLength;
                memcpy(&rtpPacketLength, incomingBundle.data() + offset, sizeof(size_t)); // get length of next rtp packet
                offset += sizeof(size_t);
                
                uint8_t * rtpPacketLocation = incomingBundle.data() + offset;

                // size our frame so we can insert the packet
                rtpFrame.resize(rtpPacketLength);

                // copy payload into outbound frame
                memcpy(rtpFrame.data(), rtpPacketLocation, rtpPacketLength); 
                offset += rtpPacketLength;

                // rtp_frame * frame = (rtp_frame *)  rtpPacketLocation;
                // frame->print_header();

                if (m_outductType == UDP_OUTDUCT) {
                    SendUdpPacket(rtpFrame);
                } else if (m_outductType == GSTREAMER_APPSRC_OUTDUCT) {
                    m_gstreamerAppSrcOutductPtr->PushRtpPacketToGStreamerOutduct(rtpFrame); // gets taken
                }

                m_totalRtpPacketsReceived++;

                if (offset == incomingBundle.size())
                    break;
            }
            
        }
    }
}

// Data from BpSourcePattern comes in through here
bool BpReceiveStream::ProcessPayload(const uint8_t *data, const uint64_t size)
{
    padded_vector_uint8_t vec(size);
    memcpy(vec.data(), data, size);

    {
        boost::mutex::scoped_lock lock(m_incomingQueueMutex); // lock mutex 
        m_incomingBundleQueue.push_back(std::move(vec));
    }

    m_incomingQueueCv.notify_one();
    return true;
}

bool BpReceiveStream::TryWaitForIncomingDataAvailable(const boost::posix_time::time_duration &timeout)
{
    boost::mutex::scoped_lock lock(m_incomingQueueMutex);
    if (!m_incomingBundleQueue.empty()) {
        return true;
    }
    m_incomingQueueCv.timed_wait(lock, timeout);
    return !m_incomingBundleQueue.empty(); //true => data available to send, false => data not avaiable to send yet
}

int BpReceiveStream::SendUdpPacket(padded_vector_uint8_t& message) 
{
    m_totalRtpBytesSent += socket.send_to(boost::asio::buffer(message), m_udpEndpoint);
    //std::cout << "sent " << m_udpEndpoint.address().to_string() << " port " << m_udpEndpoint.port() << "\n";
    m_totalRtpPacketsSent++;
	return 0;
}
