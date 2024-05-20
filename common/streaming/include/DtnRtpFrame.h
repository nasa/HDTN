/**
 * @file DtnRtpFrame.h
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

#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#include <ws2def.h>
#else
#include <netinet/in.h>
#endif

#include <string>
#include <vector>
#include <iostream>

/* https://stackoverflow.com/questions/1537964/visual-c-equivalent-of-gccs-attribute-packed  */
#if defined(__MINGW32__) || defined(__MINGW64__) || defined(__GNUC__) || defined(__linux__)
#define PACK(__Declaration__) __Declaration__ __attribute__((__packed__))
#else
#define PACK(__Declaration__) __pragma(pack(push, 1)) __Declaration__ __pragma(pack(pop))
#endif

#include "DtnUtil.h"


typedef enum RTP_ERROR_CODES {
    RTP_CONCATENATE = 0,
    RTP_INVALID_HEADER = 1,
    RTP_MISMATCH_SSRC = 2,
    RTP_INVALID_VERSION = 3,
    RTP_PUSH_PREVIOUS_FRAME = 4, // push previous rtp frame and add current packet to next frame
    RTP_OUT_OF_SEQ = 5,
    RTP_FIRST_FRAME = 6
} rtp_packet_status_t;


typedef enum RTP_MODES {
    RTP_RECV_ONLY = 1,
    RTP_SEND_ONLY = 2,
    RTP_SEND_RECV = 3
} rtp_modes_t;

#define RTP_VERSION_TWO_FLAG (3 << 15) 
#define RTP_PADDING_FLAG (1 << 13) 
#define RTP_EXT_FLAG (1 << 12) 
#define RTP_CSRC_FMASK (15 << 11) // 1111 << 11 
#define RTP_MARKER_FLAG (1 << 7) 
#define RTP_PAYLOAD_MASK (127 << 0) // 1111111 << 0 

enum RTCP_FRAME_TYPE {
    RTCP_FT_SR   = 200, /* Sender report */
    RTCP_FT_RR   = 201, /* Receiver report */
    RTCP_FT_SDES = 202, /* Source description */
    RTCP_FT_BYE  = 203, /* Goodbye */
    RTCP_FT_APP  = 204  /* Application-specific message */
};

PACK(struct rtp_header {
    uint8_t version:2;
    uint8_t padding:1;
    uint8_t ext:1;
    uint8_t cc:4;
    uint8_t marker:1;
    uint8_t payload:7; 
    uint16_t seq = 0;
    uint32_t timestamp = 0;
    uint32_t ssrc = 0;
});

typedef union rtp_header_union {
    uint16_t flags;

    struct rtp_header;
    
} rtp_header_union_t; 

PACK(struct ext_header {
    uint16_t type = 0;
    uint16_t len = 0;
    uint8_t *data = nullptr;
});

/** \brief See <a href="https://www.rfc-editor.org/rfc/rfc3550#section-5" target="_blank">RFC 3550 section 5</a> */
struct rtp_frame {
    struct rtp_header header;
    buffer payload;

    void print_header()
    {
        rtp_header_union_t flags;
        memcpy(&flags.flags, &header, sizeof(uint16_t));
        flags.flags = htons(flags.flags);

        std::cout 
        // << "\n version: "   <<   ((RTP_VERSION_TWO_FLAG & flags.flags) >> 14)
        // << "\n padding: "   <<   ((RTP_PADDING_FLAG & flags.flags) << 13)
        // << "\n ext: "       <<   ((RTP_EXT_FLAG & flags.flags) >> 12)
        // << "\n cc: "        <<   (RTP_CSRC_FMASK & flags.flags)
        << "\n marker: "    <<   ((RTP_MARKER_FLAG &flags.flags)  >> 7)
        // << "\n payload: "   <<   (rtp_format_t) (RTP_PAYLOAD_MASK & flags.flags)
        << "\n seq: "       <<   (unsigned int) (ntohs(header.seq)) << " (network " << header.seq << ")"
        << "\n timestamp: " <<   (unsigned int) (ntohl(header.timestamp)) << " (network " << header.timestamp << ")"
        // << "\n ssrc: "      <<   (unsigned int) (header.ssrc )
        << std::endl;
    }

};


/** \brief Header of for all RTCP packets defined in <a href="https://www.rfc-editor.org/rfc/rfc3550#section-6" target="_blank">RFC 3550 section 6</a> */
struct rtcp_header {
    /** \brief  This field identifies the version of RTP. The version defined by
     * RFC 3550 is two (2).  */
    uint8_t version = 0;
    /** \brief Does this packet contain padding at the end */
    uint8_t padding = 0;
    union {
        /** \brief Source count or report count. Alternative to pkt_subtype. */
        uint8_t count = 0;   
        /** \brief Subtype in APP packets. Alternative to count */
        uint8_t pkt_subtype; 
    };
    /** \brief Identifies the RTCP packet type */
    uint8_t pkt_type = 0;
    /** \brief Length of the whole message measured in 32-bit words */
    uint16_t length = 0;
};

/** \brief See <a href="https://www.rfc-editor.org/rfc/rfc3550#section-6.4.1" target="_blank">RFC 3550 section 6.4.1</a> */
struct rtcp_sender_info {
    /** \brief NTP timestamp, most significant word */
    uint32_t ntp_msw = 0;
    /** \brief NTP timestamp, least significant word */
    uint32_t ntp_lsw = 0;
    /** \brief RTP timestamp corresponding to this NTP timestamp */
    uint32_t rtp_ts = 0;
    uint32_t pkt_cnt = 0;
    /** \brief Also known as octet count*/
    uint32_t byte_cnt = 0;
};

/** \brief See <a href="https://www.rfc-editor.org/rfc/rfc3550#section-6.4.1" target="_blank">RFC 3550 section 6.4.1</a> */
struct rtcp_report_block {
    uint32_t ssrc = 0;
    uint8_t  fraction = 0;
    int32_t  lost = 0;
    uint32_t last_seq = 0;
    uint32_t jitter = 0;
    uint32_t lsr = 0;  /* last Sender Report */
    uint32_t dlsr = 0; /* delay since last Sender Report */
};

/** \brief See <a href="https://www.rfc-editor.org/rfc/rfc3550#section-6.4.2" target="_blank">RFC 3550 section 6.4.2</a> */
struct rtcp_receiver_report {
    struct rtcp_header header;
    uint32_t ssrc = 0;
    std::vector<rtcp_report_block> report_blocks;
};

/** \brief See <a href="https://www.rfc-editor.org/rfc/rfc3550#section-6.4.1" target="_blank">RFC 3550 section 6.4.1</a> */
struct rtcp_sender_report {
    struct rtcp_header header;
    uint32_t ssrc = 0;
    struct rtcp_sender_info sender_info;
    std::vector<rtcp_report_block> report_blocks;
};

/** \brief See <a href="https://www.rfc-editor.org/rfc/rfc3550#section-6.5" target="_blank">RFC 3550 section 6.5</a> */
struct rtcp_sdes_item {
    uint8_t type = 0;
    uint8_t length = 0;
    uint8_t *data = nullptr;
};

/** \brief See <a href="https://www.rfc-editor.org/rfc/rfc3550#section-6.5" target="_blank">RFC 3550 section 6.5</a> */
struct rtcp_sdes_chunk {
    uint32_t ssrc = 0;
    std::vector<rtcp_sdes_item> items;
};

/** \brief See <a href="https://www.rfc-editor.org/rfc/rfc3550#section-6.5" target="_blank">RFC 3550 section 6.5</a> */
struct rtcp_sdes_packet {
    struct rtcp_header header;
    std::vector<rtcp_sdes_chunk> chunks;
};

/** \brief See <a href="https://www.rfc-editor.org/rfc/rfc3550#section-6.7" target="_blank">RFC 3550 section 6.7</a> */
struct rtcp_app_packet {
    struct rtcp_header header;
    uint32_t ssrc = 0;
    uint8_t name[4] = {0};
    uint8_t *payload = nullptr;
    /** \brief Size of the payload in bytes. Added by uvgRTP to help process the payload. */
    size_t payload_len = 0;
};

