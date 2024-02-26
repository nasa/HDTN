#include "BpReceiveStream.h"
#include "Logger.h"
#include <boost/process.hpp>

static constexpr hdtn::Logger::SubProcess subprocess = hdtn::Logger::SubProcess::none;

#define FFMPEG_SDP_HEADER "data:application/sdp;,"

BpReceiveStream::BpReceiveStream(size_t numCircularBufferVectors, bp_recv_stream_params_t params) 
    : BpSinkPattern(), 
    m_numCircularBufferVectors(numCircularBufferVectors),
    m_outgoingRtpHostname(params.rtpDestHostname),
    m_outgoingRtpPort(params.rtpDestPort),
    m_maxOutgoingRtpPacketSizeBytes(params.maxOutgoingRtpPacketSizeBytes),
    socket(io_service),
    m_outductType(params.outductType)
{
    m_maxOutgoingRtpPayloadSizeBytes = m_maxOutgoingRtpPacketSizeBytes - sizeof(rtp_header);
    m_processingThread = boost::make_unique<boost::thread>(boost::bind(&BpReceiveStream::ProcessIncomingBundlesThread, this)); 
    
    m_incomingBundleQueue.set_capacity(m_numCircularBufferVectors);
    
    if (m_outductType == UDP_OUTDUCT)  {
        m_udpBatchSenderPtr = std::make_shared<UdpBatchSender>();
        m_udpBatchSenderPtr->SetOnSentPacketsCallback(boost::bind(&BpReceiveStream::OnSentRtpPacketCallback, this, 
                boost::placeholders::_1, boost::placeholders::_2, boost::placeholders::_3));
        m_udpBatchSenderPtr->Init(m_outgoingRtpHostname, m_outgoingRtpPort);
        m_udpEndpoint = m_udpBatchSenderPtr->GetCurrentUdpEndpoint();
        socket.open(boost::asio::ip::udp::v4());

    } else if (m_outductType == GSTREAMER_APPSRC_OUTDUCT) {
        m_gstreamerAppSrcOutductPtr = boost::make_unique<GStreamerAppSrcOutduct>(params.shmSocketPath, params.gstCaps);
        SetGStreamerAppSrcOutductInstance(m_gstreamerAppSrcOutductPtr.get());
    }

}

BpReceiveStream::~BpReceiveStream()
{
    LOG_INFO(subprocess) << "Calling BpReceiveStream deconstructor";
    m_running = false;
    
    m_gstreamerAppSrcOutductPtr.reset();
    m_udpBatchSenderPtr->Stop();

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

    while (m_running) 
    {
        bool notInWaitForNewPacketState = TryWaitForIncomingDataAvailable(timeout);
        if (notInWaitForNewPacketState) 
        {
            boost::mutex::scoped_lock lock(m_incomingQueueMutex);
        
            padded_vector_uint8_t &incomingBundle = m_incomingBundleQueue.front(); // process front of queue
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
            m_incomingBundleQueue.pop_front();
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
    if (m_incomingBundleQueue.size() == 0) { // if empty, we wait
        return GetNextIncomingPacketTimeout(timeout);
    }
    return true; 
}

bool BpReceiveStream::GetNextIncomingPacketTimeout(const boost::posix_time::time_duration &timeout)
{
    boost::mutex::scoped_lock lock(m_incomingQueueMutex);
    if ((m_incomingBundleQueue.size() == 0)) {
        m_incomingQueueCv.timed_wait(lock, timeout); //lock mutex (above) before checking condition
        return false;
    }
    
    return true;
}

bool BpReceiveStream::TryWaitForSuccessfulSend(const boost::posix_time::time_duration &timeout)
{
    if (!m_sentPacketsSuccess) { 
        return GetSuccessfulSendTimeout(timeout);
    }
    return true; 
}

bool BpReceiveStream::GetSuccessfulSendTimeout(const boost::posix_time::time_duration &timeout)
{
    boost::mutex::scoped_lock lock(m_sentPacketsMutex);
    if (!m_sentPacketsSuccess) {
        m_cvSentPacket.timed_wait(lock, timeout); //lock mutex (above) before checking condition
        return false;
    }
    
    return true;
}


void BpReceiveStream::OnSentRtpPacketCallback(bool success, std::shared_ptr<std::vector<UdpSendPacketInfo>> &udpSendPacketInfoVecSharedPtr, const std::size_t numPacketsSent)
{
    m_sentPacketsSuccess = true;
    m_cvSentPacket.notify_one();

    if (success) 
    {
        m_totalRtpPacketsSent += numPacketsSent;
        m_totalRtpBytesSent += udpSendPacketInfoVecSharedPtr->size();
        LOG_DEBUG(subprocess) << "Sent " <<  numPacketsSent << " packets. Sent " << udpSendPacketInfoVecSharedPtr->size() << " bytes";
    } else {
        LOG_ERROR(subprocess) << "Failed to send RTP packet";
    }
}

int BpReceiveStream::SendUdpPacket(padded_vector_uint8_t& message) 
{
    m_totalRtpBytesSent += socket.send_to(boost::asio::buffer(message), m_udpEndpoint);
    m_totalRtpPacketsSent++;
	return 0;
}