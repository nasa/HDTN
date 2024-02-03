/**
 * @file CcsdsEncapDecode.h
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
 * This is a header-only library for LTP/BP/IDLE decapsulation/decode only functions.
 * Based on: Encapsulation Packet Protocol: https://public.ccsds.org/Pubs/133x1b3e1.pdf
 */

#ifndef _CCSDS_ENCAP_DECODE_H
#define _CCSDS_ENCAP_DECODE_H 1

#include "CcsdsEncap.h"
#include <boost/endian/conversion.hpp>
#include <boost/config/detail/suffix.hpp>


/** Decode the first byte of a CCSDS Encap header.  See CcsdsEncap.h for a picture description of the packet.
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
    const bool valid = (static_cast<bool>(firstByte == expectedHeaderFirstByte)) && (static_cast<bool>(lengthOfLength != 0)); //&& instead of * to prevent warning: ‘*’ in boolean context, suggest ‘&&’ instead
    return valid * headerLength;
}

/** Decode the second to remaining byte(s) of a CCSDS Encap header,
* called after the DecodeCcsdsEncapHeaderSizeFromFirstByte function.
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
#endif // _CCSDS_ENCAP_DECODE_H
