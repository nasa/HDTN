/**
 * @file CcsdsEncapEncode.h
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
 * This is a header-only library for LTP/BP/IDLE encapsulation/endcoding only function.
 * Based on: Encapsulation Packet Protocol: https://public.ccsds.org/Pubs/133x1b3e1.pdf
 */

#ifndef _CCSDS_ENCAP_ENCODE_H
#define _CCSDS_ENCAP_ENCODE_H 1

#include "CcsdsEncap.h"
#include <boost/endian/conversion.hpp>

/** Get a CCSDS Encap header.  See CcsdsEncap.h for a picture description of the packet.
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
#endif //_CCSDS_ENCAP_ENCODE_H
