/**
 * @file DtnRtp.cpp
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

#include "DtnRtp.h"
#include "DtnUtil.h"
#include <random>
#include <cstring>
#include "Logger.h"


#define INVALID_TS UINT32_MAX
#define INVALID_SEQ UINT16_MAX

#define DEFAULT_MAX_PAYLOAD 1440


static constexpr hdtn::Logger::SubProcess subprocess = hdtn::Logger::SubProcess::none;

DtnRtp::DtnRtp(size_t maximumTransmissionUnit):
    m_clockRate(0),
    m_sentPackets(0),
    m_maximumTransmissionUnit(maximumTransmissionUnit),
    m_numConcatenated(0)
{
    m_prevHeader.timestamp = INVALID_TS;
    m_prevHeader.seq = INVALID_SEQ;
}

DtnRtp::~DtnRtp()
{
}

// some helpful getters
uint32_t DtnRtp::GetSsrc()  const
{
    return *m_ssrc.get();
}


uint16_t DtnRtp::GetSequence()      const
{
    return m_prevHeader.seq;
}

uint32_t DtnRtp::GetTimestamp() const
{
    return m_prevHeader.timestamp;
}

uint32_t DtnRtp::GetClockRate()     const
{
    return m_clockRate;
}

rtp_header * DtnRtp::GetHeader() {
    return &m_prevHeader;
}

/**
 * This returns the number of times the current frame has been added to. 
 * A frame with no data is zero. A frame that has been added to once is 1. 
 * A frame that has been added two twice is 2. and so on.
*/
uint16_t DtnRtp::GetNumConcatenated()
{
    return m_numConcatenated;
}


// handles rtp paremeters
void DtnRtp::IncSentPkts()
{
    m_sentPackets++;
}
void DtnRtp::IncSequence()
{
    m_prevHeader.seq = htons(ntohs(m_prevHeader.seq) + 1) ;
}

void DtnRtp::IncNumConcatenated()
{
    m_numConcatenated++;
}

void DtnRtp::ResetNumConcatenated()
{
    m_numConcatenated = 0;
}

void DtnRtp::SetSequence(uint16_t host_sequence)
{
    m_prevHeader.seq = htons(host_sequence);
}

//void DtnRtp::SetMarkerBit(uint8_t marker_bit)
//{
//    m_prevHeader.marker =  (m_prevHeader.marker | RTP_MARKER_FLAG);
//}

// setters for the rtp packet configuration
void DtnRtp::SetClockRate(rtp_format_t fmt)
{
    switch (fmt) {
        case RTP_FORMAT_H264:
        case RTP_FORMAT_H265:
        case RTP_FORMAT_DYNAMICRTP:
            m_clockRate = 90000;
            break;
        default:
            printf("Unknown RTP format, setting clock rate to 8000");
            m_clockRate = 8000;
            break;
    }
}

void DtnRtp::SetTimestamp(uint32_t timestamp)
{
    m_prevHeader.timestamp = timestamp;
}

rtp_packet_status_t DtnRtp::
PacketHandler(padded_vector_uint8_t &wholeBundleVec)
{    
    if (wholeBundleVec.size() < 12) {       
        LOG_ERROR(subprocess) << "Received UDP packet is too small to contain RTP header, discarding...";
        return RTP_INVALID_HEADER;
    }
    // LOG_DEBUG(subprocess) << " In handler";

    rtp_frame * incomingFramePtr = (rtp_frame *) wholeBundleVec.data();
    rtp_header * incomingHeaderPtr = &incomingFramePtr->header;

    rtp_header_union_t currentHeaderFlags;
    memcpy(&currentHeaderFlags.flags, incomingHeaderPtr, sizeof(uint16_t));
    currentHeaderFlags.flags = htons(currentHeaderFlags.flags);

    // incomingFramePtr->print_header();

    if (!(RTP_VERSION_TWO_FLAG & currentHeaderFlags.flags)) {
        LOG_ERROR(subprocess) << "Unsupported RTP version. Use RTP Version 2";
        return RTP_INVALID_VERSION;
    }

    // This is indicative that we have received the first message, handle correspondingly
    if (!m_ssrc) {
        rtp_format_t fmt = (rtp_format_t) (RTP_PAYLOAD_MASK & currentHeaderFlags.flags); // use mask to find out our payload type

        LOG_INFO(subprocess) << "No active session. Creating active session with SSRC = " << incomingHeaderPtr->ssrc << "\n" \
                << "RTP Format: " << fmt << "\n" \
                << "Initial TS: " << ntohl(incomingHeaderPtr->timestamp) << "\n" \
                << "Initial Seq: " << ntohs(incomingHeaderPtr->seq);

        uint32_t ssrc = incomingHeaderPtr->ssrc; // extracting from packed struct
        m_ssrc = std::make_shared<std::atomic<uint32_t>>(ssrc); // assign ssrc 
        
        SetClockRate(fmt); // assign our payload's clock rate 

        return RTP_FIRST_FRAME; // no need to check for all the things below, just start new frame
    }
    
    return RTP_PUSH_PREVIOUS_FRAME;
}
