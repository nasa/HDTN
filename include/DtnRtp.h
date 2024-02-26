#pragma once

#include "DtnFrameQueue.h"
#include "DtnUtil.h"


#include <memory>
#include <vector>
#include <chrono>
#include <memory>
#include <atomic>
#include <arpa/inet.h>

#include "UdpBundleSink.h"


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
    rtp_format_t m_fmt;
    std::shared_ptr<std::atomic<uint32_t>> m_ssrc; // as seen in rtp frames


    rtp_header m_prevHeader;  // this is stored in network byte order
    uint32_t m_clockRate; // sampling clock rate, not hardware
    std::chrono::time_point<std::chrono::high_resolution_clock> m_wallClockStart; // filled upon first call to FillHeader

    size_t m_sentPackets = 0; // number of packets sent through this object and put into rtp frames. does not necessarily equal the number of frames sent over the line
    size_t m_maximumTransmissionUnit;    

    uint16_t m_numConcatenated;

public:
    // each rtp packet must have a particular format and ssrc. get this information from the MediaStream object on creation
    DtnRtp(size_t maximumTransmissionUnit);
    ~DtnRtp();

    // some helpful getters
    uint32_t     GetSsrc()          const;
    uint16_t     GetSequence()      const;
    uint32_t     GetTimestamp()      const;
    uint32_t     GetClockRate()    const;
    rtp_header * GetHeader();
    uint16_t     GetNumConcatenated(); 

    // handles rtp paremeters 
    void IncSentPkts();
    void IncSequence();
    void IncNumConcatenated();
    void ResetNumConcatenated();

    // setters for the rtp packet configuration
    void SetSequence(uint16_t host_sequence);
    void SetMarkerBit(uint8_t marker_bit);
    void SetFormat(rtp_format_t fmt);
    void SetClockRate(rtp_format_t fmt); // this is not hardware clock. this is the sampling frequency of the given format. usually 90 kHz
    void SetTimestamp(uint32_t timestamp); 

    void FillHeader(rtp_frame * frame);     // takes pointer to a rtp frame and fills the header with the current information about the rtp session
    int PacketHandler(ssize_t size, void *packet, int rce_flags, std::shared_ptr<DtnFrameQueue> incomingFrameQueue);
    rtp_packet_status_t PacketHandler(padded_vector_uint8_t &wholeBundleVec, const rtp_header * currentRtpFrameHeader);

    void UpdateSequence(rtp_frame * frame); // takes pointer to a rtp frame and updates the header with the curent sequence number
};