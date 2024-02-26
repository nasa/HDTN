#include "BpSendStream.h"
#include "Logger.h"
#include "ThreadNamer.h"
#include <boost/process.hpp>

static constexpr hdtn::Logger::SubProcess subprocess = hdtn::Logger::SubProcess::none;

BpSendStream::BpSendStream(uint8_t intakeType, size_t maxIncomingUdpPacketSizeBytes, uint16_t incomingRtpStreamPort, size_t numCircularBufferVectors, 
        size_t maxOutgoingBundleSizeBytes, uint16_t numRtpPacketsPerBundle, std::string fileToStream) : BpSourcePattern(),
    m_intakeType(intakeType),
    m_running(true),
    m_numCircularBufferVectors(numCircularBufferVectors),
    m_maxIncomingUdpPacketSizeBytes(maxIncomingUdpPacketSizeBytes),
    m_incomingRtpStreamPort(incomingRtpStreamPort),
    m_maxOutgoingBundleSizeBytes(maxOutgoingBundleSizeBytes),
    m_numRtpPacketsPerBundle(numRtpPacketsPerBundle),
    m_fileToStream(fileToStream)
{
    m_currentFrame.reserve(m_maxOutgoingBundleSizeBytes);

    /**
     * DtnRtp objects keep track of the RTP related paremeters such as sequence number and stream identifiers. 
     * The information in the header can be used to enhance audio/video (AV) playback.
     * Here, we have a queue for the incoming and outgoing RTP streams. 
     * BpSendStream has the ability to reduce RTP related overhead by concatenating RTP 
     * packets. 
    */
    m_incomingDtnRtpPtr = std::make_shared<DtnRtp>(m_maxIncomingUdpPacketSizeBytes);

    m_processingThread = boost::make_unique<boost::thread>(boost::bind(&BpSendStream::ProcessIncomingBundlesThread, this)); 


    m_incomingCircularPacketQueue.set_capacity(numCircularBufferVectors);
    m_outgoingCircularBundleQueue.set_capacity(numCircularBufferVectors);   

    if (m_intakeType == HDTN_APPSINK_INTAKE) 
    {
        SetCallbackFunction(boost::bind(&BpSendStream::WholeBundleReadyCallback, this, boost::placeholders::_1));
        m_GStreamerAppSinkInductPtr = boost::make_unique<GStreamerAppSinkInduct>(m_fileToStream);
    } else if (m_intakeType == HDTN_SHM_INTAKE) 
    {
        SetShmInductCallbackFunction(boost::bind(&BpSendStream::WholeBundleReadyCallback, this, boost::placeholders::_1));
        m_GStreamerShmInductPtr = boost::make_unique<GStreamerShmInduct>(m_fileToStream);
    }  else if (m_intakeType == HDTN_UDP_INTAKE) 
    {
        m_bundleSinkPtr = std::make_shared<UdpBundleSink>(m_ioService, m_incomingRtpStreamPort, 
        boost::bind(&BpSendStream::WholeBundleReadyCallback, this, boost::placeholders::_1),
        numCircularBufferVectors, 
        maxIncomingUdpPacketSizeBytes, 
        boost::bind(&BpSendStream::DeleteCallback, this));
        m_ioServiceThreadPtr = boost::make_unique<boost::thread>(boost::bind(&boost::asio::io_service::run, &m_ioService));
        ThreadNamer::SetIoServiceThreadName(m_ioService, "ioServiceBpUdpSink");
    } else {
        LOG_ERROR(subprocess) << "Unrecognized intake option";
    }
}

BpSendStream::~BpSendStream()
{
    m_running = false;

    LOG_INFO(subprocess) << "m_incomingCircularPacketQueue.size(): " << m_incomingCircularPacketQueue.size();
    LOG_INFO(subprocess) << "m_outgoingCircularBundleQueue.size(): " << m_outgoingCircularBundleQueue.size();
    LOG_INFO(subprocess) << "m_totalRtpPacketsReceived: " << m_totalRtpPacketsReceived;
    LOG_INFO(subprocess) << "m_totalRtpPacketsSent: " << m_totalRtpPacketsSent ;
    LOG_INFO(subprocess) << "m_totalRtpPacketsQueued: " << m_totalRtpPacketsQueued;
    LOG_INFO(subprocess) << "m_totalIncomingCbOverruns: " << m_totalIncomingCbOverruns;
    LOG_INFO(subprocess) << "m_totalOutgoingCbOverruns: " << m_totalOutgoingCbOverruns;

    /* shut down whatever sink is running */
    m_GStreamerAppSinkInductPtr.reset();
    m_GStreamerShmInductPtr.reset();
    m_bundleSinkPtr.reset();

    if (m_ioServiceThreadPtr) {
        m_ioServiceThreadPtr->join();   
        m_ioServiceThreadPtr.reset(); 
    }

    Stop();
}

void BpSendStream::WholeBundleReadyCallback(padded_vector_uint8_t &wholeBundleVec)
{
    {
        boost::mutex::scoped_lock lock(m_incomingQueueMutex);
        if (m_incomingCircularPacketQueue.full())
            m_totalIncomingCbOverruns++;

        m_incomingCircularPacketQueue.push_back(std::move(wholeBundleVec));  // copy out bundle to local queue for processing
    }
    m_incomingQueueCv.notify_one();
}


/**
 * RTP UDP packets are delivered, not bundles. The UDP bundle sink name scheme is inherited, but we are really receiving UDP packets, not bundles.
*/
void BpSendStream::ProcessIncomingBundlesThread()
{
    static const boost::posix_time::time_duration timeout(boost::posix_time::milliseconds(250));

    while (m_running) {
        bool notInWaitForNewBundlesState = TryWaitForIncomingDataAvailable(timeout);

        if (notInWaitForNewBundlesState) {
            m_incomingQueueMutex.lock();
            padded_vector_uint8_t incomingRtpFrame(std::move(m_incomingCircularPacketQueue.front()));
            m_incomingCircularPacketQueue.pop_front();
            m_incomingQueueMutex.unlock();

            rtp_packet_status_t packetStatus = m_incomingDtnRtpPtr->PacketHandler(incomingRtpFrame, (rtp_header *) m_currentFrame.data());
                
                switch(packetStatus) {
                    /* For the first valid frame we receive assign the CSRC, sequence number, and generic status bits by copying in the first header */
                    case RTP_FIRST_FRAME:
                        CreateFrame(incomingRtpFrame);
                        break;                       
                    case RTP_PUSH_PREVIOUS_FRAME: // push current frame and make incoming frame the current frame
                        PushFrame();
                        CreateFrame(incomingRtpFrame);
                        break;
                    default:
                        LOG_ERROR(subprocess) << "Unknown return type " << packetStatus;
                }

            m_totalRtpPacketsReceived++;
        }
    }
}
/* Copy in our outgoing Rtp header and the next rtp frame payload */
void BpSendStream::CreateFrame(padded_vector_uint8_t &incomingRtpFrame)
{
    m_currentFrame = std::move(incomingRtpFrame);
}

/* Add current frame to the outgoing queue to be bundled and sent */
void BpSendStream::PushFrame()
{
    /* prepend the payload size to the payload here */
    size_t rtpFrameSize = m_currentFrame.size();
    padded_vector_uint8_t rtpPacketSizeAndPacket(m_currentFrame.size() + sizeof(size_t));
    memcpy(rtpPacketSizeAndPacket.data(), &rtpFrameSize, sizeof(size_t));
    memcpy(rtpPacketSizeAndPacket.data() + sizeof(size_t), m_currentFrame.data(), m_currentFrame.size());
    
    m_currentFrame.resize(0);
    m_offset = 0;

    /* move into rtp packet group */
    m_outgoingRtpPacketQueue.push(std::move(rtpPacketSizeAndPacket)); // new element at end 
    m_totalRtpPacketsQueued++;
    m_rtpBytesInQueue += rtpFrameSize + sizeof(size_t);

    if (m_outgoingRtpPacketQueue.size() == m_numRtpPacketsPerBundle) // we should move this packet group into the bundle queue
        PushBundle();
}

void BpSendStream::PushBundle()
{
    size_t bundleSize = m_rtpBytesInQueue;
    std::vector<uint8_t> outgoingBundle(bundleSize);
    size_t offset = 0;
    
    /* copy all packets to an outgoing bundle */
    while (m_outgoingRtpPacketQueue.size() != 0) 
    {
        /* copy the packet size and rtp packet into our bundle */
        memcpy(outgoingBundle.data() + offset, m_outgoingRtpPacketQueue.front().data(), m_outgoingRtpPacketQueue.front().size());
        offset +=  m_outgoingRtpPacketQueue.front().size();

        m_outgoingRtpPacketQueue.pop();
    }
    
    m_rtpBytesInQueue = 0;

    /* Push outgoing bundle to outgoing bundle queue */
    {
        boost::mutex::scoped_lock lock(m_outgoingQueueMutex);    // lock mutex 
        if (m_outgoingCircularBundleQueue.full())
            m_totalOutgoingCbOverruns++;
        m_outgoingCircularBundleQueue.push_back(std::move(outgoingBundle)); 
    }
    m_outgoingQueueCv.notify_one();
}

void BpSendStream::DeleteCallback()
{
}

uint64_t BpSendStream::GetNextPayloadLength_Step1() 
{
    m_outgoingQueueMutex.lock();
    if (m_outgoingCircularBundleQueue.size() == 0) {
        m_outgoingQueueMutex.unlock();
        return UINT64_MAX; // wait for data
    } 
    return (uint64_t) m_outgoingCircularBundleQueue.front().size();
}

bool BpSendStream::CopyPayload_Step2(uint8_t * destinationBuffer) 
{
    memcpy(destinationBuffer, m_outgoingCircularBundleQueue.front().data(), m_outgoingCircularBundleQueue.front().size());
    m_outgoingCircularBundleQueue.pop_front(); 
    m_outgoingQueueMutex.unlock();
    m_outgoingQueueCv.notify_one();
    m_totalRtpPacketsSent++; 

    return true;
}

/**
 * If TryWaitForDataAvailable returns true, BpSourcePattern will move to export data (Step1 and Step2). 
 * If TryWaitForDataAvailable returns false, BpSourcePattern will recall this function after a timeout
*/
bool BpSendStream::TryWaitForDataAvailable(const boost::posix_time::time_duration& timeout) {
    if (m_outgoingCircularBundleQueue.size()==0) {
        return GetNextOutgoingPacketTimeout(timeout);
    }

    return true; 
}
bool BpSendStream::GetNextOutgoingPacketTimeout(const boost::posix_time::time_duration& timeout)
{
    boost::mutex::scoped_lock lock(m_outgoingQueueMutex);
    bool inWaitForPacketState = (m_outgoingCircularBundleQueue.size() == 0);

    if (inWaitForPacketState) {
        m_outgoingQueueCv.timed_wait(lock, timeout); //lock mutex (above) before checking condition
        return false;
    }

    return true;
}


/**
 * If return true, we have data
 * If return false, we do not have data
*/
bool BpSendStream::TryWaitForIncomingDataAvailable(const boost::posix_time::time_duration& timeout) {
    if (m_incomingCircularPacketQueue.size() == 0) { // if empty, we wait
        return GetNextIncomingPacketTimeout(timeout);
    }
    return true; 
}


bool BpSendStream::GetNextIncomingPacketTimeout(const boost::posix_time::time_duration &timeout)
{
    boost::mutex::scoped_lock lock(m_incomingQueueMutex);
    if ((m_incomingCircularPacketQueue.size() == 0)) {
        m_incomingQueueCv.timed_wait(lock, timeout); //lock mutex (above) before checking condition
        return false;
    }
    
    return true;
}
