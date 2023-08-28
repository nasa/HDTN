/**
 * @file CcsdsEncap.h
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
 * This is a header-only library for LTP/BP/IDLE encapsulation and decapsulation functions.
 * Based on: Encapsulation Packet Protocol: https://public.ccsds.org/Pubs/133x1b3e1.pdf
 */

#ifndef _CCSDS_ENCAP_H
#define _CCSDS_ENCAP_H 1

#include <cstdint>
#include <boost/endian/conversion.hpp>
#include <boost/config/detail/suffix.hpp>

// Encapsulation parameters
#define CCSDS_ENCAP_PACKET_VERSION_NUMBER 7  // 0b111 for encapsulation packet: https://sanaregistry.org/r/packet_version_number/
#define SANA_IDLE_ENCAP_PROTOCOL_ID 0  // 0b000 for Encap Idle Packet: https://sanaregistry.org/r/protocol_id/
#define SANA_LTP_ENCAP_PROTOCOL_ID 1  // 0b001 for LTP Protocol: https://sanaregistry.org/r/protocol_id/
#define SANA_BP_ENCAP_PROTOCOL_ID 4  // 0b100 for Bundle Protocol (BP): https://sanaregistry.org/r/protocol_id/
#define CCSDS_ENCAP_USER_DEFINED_FIELD 0
#define CCSDS_ENCAP_PROTOCOL_ID_EXT 0
#define CCSDS_ENCAP_DEFINED_FIELD 0

enum class ENCAP_PACKET_TYPE : uint8_t {
    IDLE = ((CCSDS_ENCAP_PACKET_VERSION_NUMBER << 5) | (SANA_IDLE_ENCAP_PROTOCOL_ID << 2)),
    LTP =  ((CCSDS_ENCAP_PACKET_VERSION_NUMBER << 5) | (SANA_LTP_ENCAP_PROTOCOL_ID << 2)),
    BP =   ((CCSDS_ENCAP_PACKET_VERSION_NUMBER << 5) | (SANA_BP_ENCAP_PROTOCOL_ID << 2))
};

/*
Encapsulation Packet Protocol: https://public.ccsds.org/Pubs/133x1b3e1.pdf

///////////////
// Idle Packet
///////////////
4.1.2.4.4:
If the Length of Length field has the value ‘00’ then
the Protocol ID field shall have the value ‘000’,
indicating that the packet is an Encapsulation Idle Packet.
NOTE – If the Length of Length field has the value ‘00’,
then the Packet Length field and the Encapsulated Data Unit
field are both absent from the packet.
In this case, the length of the Encapsulation Packet is one octet.
    ┏━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┓
    ┃      IDLE ENCAPSULATION       ┃
    ┃           PACKET              ┃
    ┃           HEADER              ┃
    ┠───────────┬───────────┬───────┨
    ┃           │    Idle   │       ┃
    ┃  PACKET   │   ENCAP   │  LEN  ┃
    ┃  VERSION  │  PROTOCOL │  OF   ┃
    ┃  NUMBER   │     ID    │  LEN  ┃
    ┃  (0b111)  │  (0b000)  │ (0b00)┃
    ┣━━━━━━━━━━━┿━━━━━━━━━━━┿━━━━━━━┫
    ┃ 7 │ 6 │ 5 │ 4 │ 3 │ 2 │ 1 │ 0 ┃
    ┃             data[0]           ┃
    ┗━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┛


//////////////////////////////////
// Encapsulate an LTP or BP packet
//////////////////////////////////
 

    
    Payload length <= 255-2: 1 octet length field
    2 byte header
    ┏━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┓   
    ┃                             ENCAPSULATION                     ┃
    ┃                             PACKET                            ┃
    ┃                             HEADER                            ┃
    ┠───────────┬───────────┬───────┬───────────────────────────────┨
    ┃           │           │       │                               ┃
    ┃  PACKET   │   ENCAP   │  LEN  │             PACKET            ┃
    ┃  VERSION  │  PROTOCOL │  OF   │             LENGTH            ┃  LTP/BP PDU     ┃
    ┃  NUMBER   │     ID    │  LEN  │                               ┃
    ┃  (0b111)  │  (LTP/BP) │ (0b01)│                               ┃
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
    ┃           │           │       │               │               │                                                               ┃
    ┃  PACKET   │   ENCAP   │  LEN  │     USER      │ ENCAPSULATION │                            PACKET                             ┃
    ┃  VERSION  │  PROTOCOL │  OF   │    DEFINED    │  PROTOCOL ID  │                            LENGTH                             ┃  LTP/BP PDU     ┃
    ┃  NUMBER   │     ID    │  LEN  │     FIELD     │   EXTENSION   │                         (big endian)                          ┃
    ┃  (0b111)  │  (LTP/BP) │ (0b10)│    (zeros)    │    (zeros)    │                                                               ┃
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
    ┃           │           │       │               │               │                   │                                                       ┃
    ┃  PACKET   │   ENCAP   │  LEN  │     USER      │ ENCAPSULATION │       CCSDS       │                        PACKET                         ┃
    ┃  VERSION  │  PROTOCOL │  OF   │    DEFINED    │  PROTOCOL ID  │      DEFINED      │                        LENGTH                         ┃  LTP/BP PDU     ┃
    ┃  NUMBER   │     ID    │  LEN  │     FIELD     │   EXTENSION   │       FIELD       │                     (big endian)                      ┃
    ┃  (0b111)  │ (LTP/BP)  │ (0b11)│    (zeros)    │    (zeros)    │      (zeros)      │                                                       ┃
    ┣━━━━━━━━━━━┿━━━━━━━━━━━┿━━━━━━━┿━━━━━━━━━━━━━━━┿━━━━━━━━━━━━━━━┿━━━━━━━━━┯━━━━━━━━━┿━━━━━━━━━━━━━┯━━━━━━━━━━━━━┯━━━━━━━━━━━━━┯━━━━━━━━━━━━━┫
    ┃ 7 │ 6 │ 5 │ 4 │ 3 │ 2 │ 1 │ 0 │ 7 │ 6 │ 5 │ 4 │ 3 │ 2 │ 1 │ 0 │ 7 ... 0 │ 7 ... 0 │ 7 │ ... │ 0 │ 7 │ ... │ 0 │ 7 │ ... │ 0 │ 7 │ ... │ 0 ┃
    ┃             data[0]           │            data[1]            │ data[2] │ data[3] │   data[4]   │   data[5]   │   data[6]   │   data[7]   ┃
    ┗━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┷━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┷━━━━━━━━━┷━━━━━━━━━┷━━━━━━━━━━━━━┷━━━━━━━━━━━━━┻━━━━━━━━━━━━━━━━━━━━━━━━━━━┛

 */


/** Get a CCSDS Encap header.
*
* @param type The SANA type of encap packet to generate.
* @param outHeader8Byte An 8-byte buffer to write/return the generated 1, 2, 4, or 8 byte Encap header
* @param encappedPayloadSize The size of the PDU or payload part that is getting encapsulated.
* @param outHeaderSize The returned size of the encap header (valid only when the function returns true).
* @return true on success, false on failure.
*/
static bool GetCcsdsEncapHeader(const ENCAP_PACKET_TYPE type, uint8_t* outHeader8Byte, const uint32_t encappedPayloadSize, uint8_t& outHeaderSize) {

    uint8_t lengthOfLength;

    // different packet-length field sizes have different header sections
    if (encappedPayloadSize > (0xffffffffu - 8u)) { //max size
        outHeaderSize = 0;
        return false;
    }
    else if (encappedPayloadSize == 0) { //idle packet
        lengthOfLength = 0; // outHeaderSize = 1;
        if (type != ENCAP_PACKET_TYPE::IDLE) {
            return false;
        }
    }
    else if (type == ENCAP_PACKET_TYPE::IDLE) { //implicit && (encappedPayloadSize > 0)
        return false;
    }
    else if (encappedPayloadSize <= (0xff - 2)) {
        lengthOfLength = 1; // outHeaderSize = 2;
    }
    else if (encappedPayloadSize <= (0xffffu - 4)) {
        lengthOfLength = 2; // outHeaderSize = 4;
    }
    else {
        lengthOfLength = 3; // outHeaderSize = 8;
    }
    outHeaderSize = 1u << lengthOfLength;

    *outHeader8Byte++ = (static_cast<uint8_t>(type)) | lengthOfLength;

    if (lengthOfLength == 0) { // length field absent
        return true;
    }

    const uint32_t encapLen = encappedPayloadSize + outHeaderSize; // total size of encapsulation packet

    if (lengthOfLength == 1) { // 1 octet length field
        *outHeader8Byte = static_cast<uint8_t>(encapLen);
    }
    else { //if(lengthOfLength >= 2) {
        *outHeader8Byte++ = (CCSDS_ENCAP_USER_DEFINED_FIELD << 4) | (CCSDS_ENCAP_PROTOCOL_ID_EXT);
        
        if(lengthOfLength == 3) { // 4 octet length field
            *outHeader8Byte++ = CCSDS_ENCAP_DEFINED_FIELD >> 8;
            *outHeader8Byte++ = CCSDS_ENCAP_DEFINED_FIELD;
            const uint32_t encapLenBe = boost::endian::native_to_big(encapLen);
            memcpy(outHeader8Byte, &encapLenBe, sizeof(encapLenBe));
        }
        else { //if(lengthOfLength == 2) { // 2 octet length field
            const uint16_t encapLenBe = boost::endian::native_to_big(static_cast<uint16_t>(encapLen));
            memcpy(outHeader8Byte, &encapLenBe, sizeof(encapLenBe));
        }
    }

    return true;
}

/** Decode the first byte of a CCSDS Encap header.
*
* @param type The non-idle SANA type of encap packet expected to be decoded.
* @param firstByte The first byte of the Encap header to decode.
* @return 1, 2, 4, or 8 on success (denoting the size of the encap header), or 0 on failure.
*/
BOOST_FORCEINLINE static uint8_t DecodeCcsdsEncapHeaderSizeFromFirstByte(const ENCAP_PACKET_TYPE type, const uint8_t firstByte) {
    if (firstByte == (static_cast<uint8_t>(ENCAP_PACKET_TYPE::IDLE))) { //in this case, lengthOfLength == 0
        return 1;
    }
    const uint8_t lengthOfLength = firstByte & 0x3u;
    const uint8_t headerLength = 1u << lengthOfLength;
    const uint8_t expectedHeaderFirstByte = (static_cast<uint8_t>(type)) | lengthOfLength;
    const bool valid = (static_cast<bool>(firstByte == expectedHeaderFirstByte)) * (static_cast<bool>(lengthOfLength != 0));
    return valid * headerLength;
}

/** Decode the second to remaining byte(s) of a CCSDS Encap header,
* called after the DecodeCcsdsEncapHeaderSizeFromFirstByte function
*
* @param encapHeaderLength The 1, 2, 4, or 8 byte size of encap packet expected to be decoded.
*                          This function need not be called if this value is 1.
* @param secondByte The pointer to the start of the second byte of the Encap header to decode.
* @param userDefinedField The returned user defined field of the Encap packet (usually 0)
*                         if encapHeaderLength is 4 or 8 bytes.
* @param payloadSize The returned payload size (i.e. size of the data being encapsulated)
*                    if encapHeaderLength is 2, 4, or 8 bytes.
* @return true on success (denoting valid userDefinedField and payloadSize), or false on failure.
*/
static bool DecodeCcsdsEncapPayloadSizeFromSecondToRemainingBytes(const uint8_t encapHeaderLength,
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
        if ((udfPlusExt & 0x0fu) != CCSDS_ENCAP_PROTOCOL_ID_EXT) {
            return false; //failure
        }
        userDefinedField = (udfPlusExt >> 4);

        if (encapHeaderLength == 8) {
            if ((*secondByte++) != (static_cast<uint8_t>(CCSDS_ENCAP_DEFINED_FIELD >> 8))) {
                return false; //failure
            }
            if ((*secondByte++) != (static_cast<uint8_t>(CCSDS_ENCAP_DEFINED_FIELD))) {
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
#endif // _CCSDS_ENCAP_H
