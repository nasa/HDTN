#pragma once

#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#else
#include <sys/time.h>
#endif

#if defined(_MSC_VER)
typedef SSIZE_T ssize_t;
#else
#include <sys/types.h>
#endif

#include <stdint.h>
#include <cstring> 
#include <random>

#include <boost/circular_buffer.hpp>

struct buffer {
        void   *start;
        size_t  length;
        
        void allocate(size_t new_length)
        {
            start = malloc(new_length);
            length = new_length;
        }

        void unallocate()
        {
            free(start);
        }

        void copy(void * src)
        {
            if (!start)
                return;

            memcpy(start, src, length);
        }
};

typedef enum RTP_FORMAT {
    // See RFC 3551 for more details

    // static audio profiles
    RTP_FORMAT_GENERIC    = 0,   ///< Same as PCMU
    RTP_FORMAT_PCMU       = 0,   ///< PCMU, ITU-T G.711
    // 1 is reserved in RFC 3551 
    // 2 is reserved in RFC 3551
    RTP_FORMAT_GSM        = 3,   ///< GSM (Group Speciale Mobile)
    RTP_FORMAT_G723       = 4,   ///< G723
    RTP_FORMAT_DVI4_32    = 5,   ///< DVI 32 kbit/s
    RTP_FORMAT_DVI4_64    = 6,   ///< DVI 64 kbit/s
    RTP_FORMAT_LPC        = 7,   ///< LPC
    RTP_FORMAT_PCMA       = 8,   ///< PCMA
    RTP_FORMAT_G722       = 9,   ///< G722
    RTP_FORMAT_L16_STEREO = 10,  ///< L16 Stereo
    RTP_FORMAT_L16_MONO   = 11,  ///< L16 Mono
    // 12 QCELP is unsupported in uvgRTP
    // 13 CN is unsupported in uvgRTP
    // 14 MPA is unsupported in uvgRTP
    RTP_FORMAT_G728       = 15,  ///< G728
    RTP_FORMAT_DVI4_441   = 16,  ///< DVI 44.1 kbit/s
    RTP_FORMAT_DVI4_882   = 17,  ///< DVI 88.2 kbit/s
    RTP_FORMAT_G729       = 18,  ///< G729, 8 kbit/s
    // 19 is reserved in RFC 3551
    // 20 - 23 are unassigned in RFC 3551

    /* static video profiles, unsupported in uvgRTP
    * 24 is unassigned
    * 25 is CelB, 
    * 26 is JPEG
    * 27 is unassigned
    * 28 is nv
    * 29 is unassigned
    * 30 is unassigned
    * 31 is H261
    * 32 is MPV
    * 33 is MP2T
    * 32 is H263
    */

    /* Rest of static numbers
    * 35 - 71 are unassigned
    * 72 - 76 are reserved
    * 77 - 95 are unassigned
    */
    
    /* Formats with dynamic payload numbers 96 - 127, including default values.
    * Use RCC_DYN_PAYLOAD_TYPE flag to change the number if desired. */

    RTP_FORMAT_DYNAMICRTP   = 96,  // used by ffmpeg

    RTP_FORMAT_G726_32   = 97,  ///< G726, 32 kbit/s
    RTP_FORMAT_G726_24   = 98,  ///< G726, 24 kbit/s
    RTP_FORMAT_G726_16   = 99,  ///< G726, 16 kbit/s
    RTP_FORMAT_G729D     = 100, ///< G729D, 6.4 kbit/s
    RTP_FORMAT_G729E     = 101, ///< G729E, 11.8 kbit/s
    RTP_FORMAT_GSM_EFR   = 102, ///< GSM enhanced full rate speech transcoding
    RTP_FORMAT_L8        = 103, ///< L8, linear audio data samples
    // RED is unsupported in uvgRTP
    RTP_FORMAT_VDVI      = 104, ///< VDVI, variable-rate DVI4
    RTP_FORMAT_OPUS      = 105, ///< Opus, see RFC 7587
    // H263-1998 is unsupported in uvgRTP
    RTP_FORMAT_H264      = 106, ///< H.264/AVC, see RFC 6184
    RTP_FORMAT_H265      = 107, ///< H.265/HEVC, see RFC 7798
    RTP_FORMAT_H266      = 108  ///< H.266/VVC
    
} rtp_format_t;