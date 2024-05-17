/**
 * @file DtnRtp.h
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

#include "DtnFrameQueue.h"
#include "DtnUtil.h"


#include <memory>
#include <vector>
#include <chrono>
#include <memory>
#include <atomic>

#include "UdpBundleSink.h"
#include "streaming_lib_export.h"

#define USE_INCOMING_SEQ true
#define USE_OUTGOING_SEQ false



/**
 * This class effectively acts as a tracker for all the
 * pertinent RTP frame information such as timestamp, ssrc,
 * number of packets sent, clock rate etc
*/
class DtnRtp
{
private:
    std::shared_ptr<std::atomic<uint32_t> > m_ssrc; // as seen in rtp frames


    rtp_header m_prevHeader;  // this is stored in network byte order
    uint32_t m_clockRate; // sampling clock rate, not hardware
    std::chrono::time_point<std::chrono::high_resolution_clock> m_wallClockStart; // filled upon first call to FillHeader

    size_t m_sentPackets; // number of packets sent through this object and put into rtp frames. does not necessarily equal the number of frames sent over the line
    size_t m_maximumTransmissionUnit;    

    uint16_t m_numConcatenated;

public:
    // each rtp packet must have a particular format and ssrc. get this information from the MediaStream object on creation
    STREAMING_LIB_EXPORT DtnRtp(size_t maximumTransmissionUnit);
    STREAMING_LIB_EXPORT ~DtnRtp();

    // some helpful getters
    STREAMING_LIB_EXPORT uint32_t     GetSsrc()          const;
    STREAMING_LIB_EXPORT uint16_t     GetSequence()      const;
    STREAMING_LIB_EXPORT uint32_t     GetTimestamp()      const;
    STREAMING_LIB_EXPORT uint32_t     GetClockRate()    const;
    STREAMING_LIB_EXPORT rtp_header * GetHeader();
    STREAMING_LIB_EXPORT uint16_t     GetNumConcatenated();

    // handles rtp paremeters 
    STREAMING_LIB_EXPORT void IncSentPkts();
    STREAMING_LIB_EXPORT void IncSequence();
    STREAMING_LIB_EXPORT void IncNumConcatenated();
    STREAMING_LIB_EXPORT void ResetNumConcatenated();

    // setters for the rtp packet configuration
    STREAMING_LIB_EXPORT void SetSequence(uint16_t host_sequence);
    //void SetMarkerBit(uint8_t marker_bit);
    STREAMING_LIB_EXPORT void SetFormat(rtp_format_t fmt);
    STREAMING_LIB_EXPORT void SetClockRate(rtp_format_t fmt); // this is not hardware clock. this is the sampling frequency of the given format. usually 90 kHz
    STREAMING_LIB_EXPORT void SetTimestamp(uint32_t timestamp);

    STREAMING_LIB_EXPORT void FillHeader(rtp_frame * frame);     // takes pointer to a rtp frame and fills the header with the current information about the rtp session
    STREAMING_LIB_EXPORT int PacketHandler(ssize_t size, void *packet, int rce_flags, std::shared_ptr<DtnFrameQueue> incomingFrameQueue);
    STREAMING_LIB_EXPORT rtp_packet_status_t PacketHandler(padded_vector_uint8_t &wholeBundleVec);

    STREAMING_LIB_EXPORT void UpdateSequence(rtp_frame * frame); // takes pointer to a rtp frame and updates the header with the curent sequence number
};