/**
 * @file LtpEncap.h
 * @author  Brian Tomko <brian.j.tomko@nasa.gov>
 * @author  Patrick Zhong <patrick.zhong@nasa.gov>
 *
 * @copyright Copyright © 2021 United States Government as represented by
 * the National Aeronautics and Space Administration.
 * No copyright is claimed in the United States under Title 17, U.S.Code.
 * All Other Rights Reserved.
 *
 * @section LICENSE
 * Released under the NASA Open Source Agreement (NOSA)
 * See LICENSE.md in the source root directory for more information.
 *
 * @section DESCRIPTION
 *
 * This is a header-only library for LTP encapsulation and decapsulation functions.
 * Based on: Encapsulation Packet Protocol: https://public.ccsds.org/Pubs/133x1b3e1.pdf
 */

#ifndef _LTP_ENCAP_H
#define _LTP_ENCAP_H 1

#include <cstdint>
#include <boost/endian/conversion.hpp>
#include <boost/config/detail/suffix.hpp>

// Encapsulation parameters
#define PACKET_VERSION_NUMBER 7  // 0b111 for encapsulation packet: https://sanaregistry.org/r/packet_version_number/
#define LTP_ENCAP_PROTOCOL_ID 1  // 0b001 for LTP Protocol Extension: https://sanaregistry.org/r/protocol_id/
#define USER_DEFINED_FIELD 0
#define ENCAP_PROTOCOL_ID_EXT 0
#define CCSDS_DEFINED_FIELD 0



/*

 Encapsulate an LTP packet
 
 Encapsulation Packet Protocol: https://public.ccsds.org/Pubs/133x1b3e1.pdf
 Protocol Id (0b001) for LTP Encap: https://sanaregistry.org/r/protocol_id/
    
    Payload length <= 255-2: 1 octet length field
    2 byte header
    ┏━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┓   
    ┃                             ENCAPSULATION                     ┃
    ┃                             PACKET                            ┃
    ┃                             HEADER                            ┃
    ┠───────────┬───────────┬───────┬───────────────────────────────┨
    ┃           │    LTP    │       │                               ┃
    ┃  PACKET   │   ENCAP   │  LEN  │             PACKET            ┃
    ┃  VERSION  │  PROTOCOL │  OF   │             LENGTH            ┃     LTP PDU     ┃
    ┃  NUMBER   │     ID    │  LEN  │                               ┃
    ┃  (0b111)  │  (0b001)  │ (0b01)│                               ┃
    ┣━━━━━━━━━━━┿━━━━━━━━━━━┿━━━━━━━┿━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
    ┃ 7 │ 6 │ 5 │ 4 │ 3 │ 2 │ 1 │ 0 │ 7 │ 6 │ 5 │ 4 │ 3 │ 2 │ 1 │ 0 ┃
    ┃             data[0]           │            data[1]            ┃
    ┗━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┷━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┛

    Payload length <= 65535-4: 2 octet length field
    4 byte header
    ┏━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┓   
    ┃                                                         ENCAPSULATION                                                         ┃
    ┃                                                            PACKET                                                             ┃
    ┃                                                            HEADER                                                             ┃
    ┠───────────┬───────────┬───────┬───────────────┬───────────────┬───────────────────────────────────────────────────────────────┨
    ┃           │    LTP    │       │               │               │                                                               ┃
    ┃  PACKET   │   ENCAP   │  LEN  │     USER      │ ENCAPSULATION │                            PACKET                             ┃
    ┃  VERSION  │  PROTOCOL │  OF   │    DEFINED    │  PROTOCOL ID  │                            LENGTH                             ┃     LTP PDU     ┃
    ┃  NUMBER   │     ID    │  LEN  │     FIELD     │   EXTENSION   │                         (big endian)                          ┃
    ┃  (0b111)  │  (0b001)  │ (0b10)│    (zeros)    │    (zeros)    │                                                               ┃
    ┣━━━━━━━━━━━┿━━━━━━━━━━━┿━━━━━━━┿━━━━━━━━━━━━━━━┿━━━━━━━━━━━━━━━┿━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
    ┃ 7 │ 6 │ 5 │ 4 │ 3 │ 2 │ 1 │ 0 │ 7 │ 6 │ 5 │ 4 │ 3 │ 2 │ 1 │ 0 │ 7 │ 6 │ 5 │ 4 │ 3 │ 2 │ 1 │ 0 │ 7 │ 6 │ 5 │ 4 │ 3 │ 2 │ 1 │ 0 ┃
    ┃             data[0]           │            data[1]            │            data[2]            │            data[3]            ┃
    ┗━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┷━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┷━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┷━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┛
    
    Payload length <= 4,294,967,295-8: 4 octet length field
    8 byte header
    ┏━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┓
    ┃                                                         ENCAPSULATION                                                                     ┃
    ┃                                                            PACKET                                                                         ┃
    ┃                                                            HEADER                                                                         ┃
    ┠───────────┬───────────┬───────┬───────────────┬───────────────┬───────────────────┬───────────────────────────────────────────────────────┨
    ┃           │    LTP    │       │               │               │                   │                                                       ┃
    ┃  PACKET   │   ENCAP   │  LEN  │     USER      │ ENCAPSULATION │       CCSDS       │                        PACKET                         ┃
    ┃  VERSION  │  PROTOCOL │  OF   │    DEFINED    │  PROTOCOL ID  │      DEFINED      │                        LENGTH                         ┃     LTP PDU     ┃
    ┃  NUMBER   │     ID    │  LEN  │     FIELD     │   EXTENSION   │       FIELD       │                     (big endian)                      ┃
    ┃  (0b111)  │  (0b001)  │ (0b11)│    (zeros)    │    (zeros)    │      (zeros)      │                                                       ┃
    ┣━━━━━━━━━━━┿━━━━━━━━━━━┿━━━━━━━┿━━━━━━━━━━━━━━━┿━━━━━━━━━━━━━━━┿━━━━━━━━━┯━━━━━━━━━┿━━━━━━━━━━━━━┯━━━━━━━━━━━━━┯━━━━━━━━━━━━━┯━━━━━━━━━━━━━┫
    ┃ 7 │ 6 │ 5 │ 4 │ 3 │ 2 │ 1 │ 0 │ 7 │ 6 │ 5 │ 4 │ 3 │ 2 │ 1 │ 0 │ 7 ... 0 │ 7 ... 0 │ 7 │ ... │ 0 │ 7 │ ... │ 0 │ 7 │ ... │ 0 │ 7 │ ... │ 0 ┃
    ┃             data[0]           │            data[1]            │ data[2] │ data[3] │   data[4]   │   data[5]   │   data[6]   │   data[7]   ┃
    ┗━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┷━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┷━━━━━━━━━┷━━━━━━━━━┷━━━━━━━━━━━━━┷━━━━━━━━━━━━━┻━━━━━━━━━━━━━━━━━━━━━━━━━━━┛

 */
static bool GetCcsdsLtpEncapHeader(uint8_t* outHeader8Byte, const uint32_t encappedPayloadSize, uint8_t& outHeaderSize) {

    // Need larger packet length field than 2 octets, since IP is 2 bytes and we have an additional 5 byte header
    // We can dynamically switch headers depending on packet size, in the interest of lower header overhead,
    //      but that adds an additional dependency on length-of-length field.

    uint8_t lengthOfLength;

    // different packet-length field sizes have different header sections
    if (encappedPayloadSize > (0xffffffffu - 8u)) { //max size
        outHeaderSize = 0;
        return false;
    }
    else if (encappedPayloadSize == 0) { //keep alive
        lengthOfLength = 0; // outHeaderSize = 1;
    }
    else if(encappedPayloadSize <= (0xff - 2)) {
        lengthOfLength = 1; // outHeaderSize = 2;
    }
    else if(encappedPayloadSize <= (0xffffu - 4)) {
        lengthOfLength = 2; // outHeaderSize = 4;
    }
    else {
        lengthOfLength = 3; // outHeaderSize = 8;
    }
    outHeaderSize = 1u << lengthOfLength;

    const uint32_t encapLen = encappedPayloadSize + outHeaderSize; // total size of encapsulation packet

    *outHeader8Byte++ = ((PACKET_VERSION_NUMBER << 5) | (LTP_ENCAP_PROTOCOL_ID << 2)) | lengthOfLength;

    if(lengthOfLength >= 2) {
        *outHeader8Byte++ = (USER_DEFINED_FIELD << 4) | (ENCAP_PROTOCOL_ID_EXT);
        
        if(lengthOfLength == 3) {
            *outHeader8Byte++ = CCSDS_DEFINED_FIELD >> 8;
            *outHeader8Byte++ = CCSDS_DEFINED_FIELD;
        }
    }

    if (lengthOfLength == 0) { // length field absent
        
    }
    else if(lengthOfLength == 1) { // 1 octet length field
        *outHeader8Byte = static_cast<uint8_t>(encapLen);
    }
    else if(lengthOfLength == 2) { // 2 octet length field
        const uint16_t encapLenBe = boost::endian::native_to_big(static_cast<uint16_t>(encapLen));
        memcpy(outHeader8Byte, &encapLenBe, sizeof(encapLenBe));
    }
    else { // 4 octet length field
        const uint32_t encapLenBe = boost::endian::native_to_big(encapLen);
        memcpy(outHeader8Byte, &encapLenBe, sizeof(encapLenBe));
    }

    return true;
}


/*
  
  Decapsulate a CCSDS encapsulation packet into an IP packet

 */
BOOST_FORCEINLINE static uint8_t DecodeCcsdsLtpEncapHeaderSizeFromFirstByte(const uint8_t firstByte) {
    const uint8_t lengthOfLength = firstByte & 0x3u;
    const uint8_t headerLength = 1u << lengthOfLength;
    const uint8_t expectedHeaderFirstByte = ((PACKET_VERSION_NUMBER << 5) | (LTP_ENCAP_PROTOCOL_ID << 2)) | lengthOfLength;
    const bool valid = (firstByte == expectedHeaderFirstByte);
    return valid * headerLength;
}

static bool DecodeCcsdsLtpEncapPayloadSizeFromSecondToRemainingBytes(const uint8_t encapHeaderLength,
    const uint8_t* secondByte,
    uint8_t& userDefinedField,
    uint32_t& payloadSize)
{
    userDefinedField = 0;
    payloadSize = 0;
    if (encapHeaderLength == 1) { // length field absent
        return true;
    }
    else if (encapHeaderLength == 2) { // 1 octet length field
        payloadSize = *secondByte - 2;
        return true;
    }
    else if ((encapHeaderLength == 4) || (encapHeaderLength == 8)) {
        const uint8_t udfPlusExt = *secondByte++;
        if ((udfPlusExt & 0x0fu) != ENCAP_PROTOCOL_ID_EXT) {
            return false; //failure
        }
        userDefinedField = (udfPlusExt >> 4);

        if (encapHeaderLength == 8) {
            if ((*secondByte++) != (static_cast<uint8_t>(CCSDS_DEFINED_FIELD >> 8))) {
                return false; //failure
            }
            if ((*secondByte++) != (static_cast<uint8_t>(CCSDS_DEFINED_FIELD))) {
                return false; //failure
            }
            // 4 octet length field
            uint32_t encapLenBe;
            memcpy(&encapLenBe, secondByte, sizeof(encapLenBe));
            payloadSize = boost::endian::big_to_native(encapLenBe) - 8;
            return true;
        }
        else {
            // 2 octet length field
            uint16_t encapLenBe;
            memcpy(&encapLenBe, secondByte, sizeof(encapLenBe));
            payloadSize = boost::endian::big_to_native(encapLenBe) - 4;
            return true;
        }
    }
    else {
        return false; //failure (invalid encapHeaderLength)
    }
}
#endif // _LTP_ENCAP_H
