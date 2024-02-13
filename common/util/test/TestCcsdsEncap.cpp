/**
 * @file TestCcsdsEncap.cpp
 * @author  Brian Tomko <brian.j.tomko@nasa.gov>
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

#include <boost/test/unit_test.hpp>
#include <vector>
#include "CcsdsEncapDecode.h"
#include "CcsdsEncapEncode.h"

BOOST_AUTO_TEST_CASE(CcsdsEncapTestCase)
{
    typedef std::tuple<uint32_t, uint8_t, bool> payloadsize_headersize_valid_pair_t;
    const std::vector<payloadsize_headersize_valid_pair_t> testVals = {
        {0, 1, true},
        {1, 2, true},
        {2, 2, true},
        {3, 2, true},
        {4, 2, true},

        {255 - 4, 2, true},
        {255 - 3, 2, true},
        {255 - 2, 2, true},
        {255 - 1, 4, true},
        {255, 4, true},
        {255 + 1, 4, true},
        {255 + 2, 4, true},
        {255 + 3, 4, true},
        {255 + 4, 4, true},

        {65535 - 6, 4, true},
        {65535 - 5, 4, true},
        {65535 - 4, 4, true},
        {65535 - 3, 8, true},
        {65535 - 2, 8, true},
        {65535 - 1, 8, true},
        {65535, 8, true},
        {65535 + 1, 8, true},
        {65535 + 2, 8, true},
        {65535 + 3, 8, true},
        {65535 + 4, 8, true},

        {UINT32_MAX - 10, 8, true},
        {UINT32_MAX - 9, 8, true},
        {UINT32_MAX - 8, 8, true},
        //invalid (0 -> invalid)
        {UINT32_MAX - 7, 0, false},
        {UINT32_MAX - 6, 0, false},
        {UINT32_MAX - 5, 0, false},
        {UINT32_MAX - 4, 0, false},
        {UINT32_MAX - 3, 0, false},
        {UINT32_MAX - 2, 0, false},
        {UINT32_MAX - 1, 0, false},
        {UINT32_MAX, 0, false},
    };

    { //make sure you can't encode LTP or BP type Idle packets
        uint8_t outHeader8Byte[8] = { 0 };
        uint8_t encodedEncapHeaderSize;
        BOOST_REQUIRE(!GetCcsdsEncapHeader(ENCAP_PACKET_TYPE::LTP,
            outHeader8Byte, 0, encodedEncapHeaderSize));
        BOOST_REQUIRE(!GetCcsdsEncapHeader(ENCAP_PACKET_TYPE::BP,
            outHeader8Byte, 0, encodedEncapHeaderSize));
        BOOST_REQUIRE(!GetCcsdsEncapHeader(ENCAP_PACKET_TYPE::IDLE,
            outHeader8Byte, 1, encodedEncapHeaderSize)); //fail due to non-zero payload size

        //encode Idle packet
        encodedEncapHeaderSize = 10;
        BOOST_REQUIRE(GetCcsdsEncapHeader(ENCAP_PACKET_TYPE::IDLE,
            outHeader8Byte, 0, encodedEncapHeaderSize));
        BOOST_REQUIRE_EQUAL(encodedEncapHeaderSize, 1);
        BOOST_REQUIRE_EQUAL(outHeader8Byte[0], static_cast<uint8_t>(ENCAP_PACKET_TYPE::IDLE));
    }

    for (unsigned int typeI = 0; typeI < 2; ++typeI) {
        static const ENCAP_PACKET_TYPE ENCAP_TYPE_LUT[2] = {
            ENCAP_PACKET_TYPE::LTP,
            ENCAP_PACKET_TYPE::BP
        };
        const ENCAP_PACKET_TYPE encapType = ENCAP_TYPE_LUT[typeI];
        for (std::size_t i = 0; i < testVals.size(); ++i) {
            const payloadsize_headersize_valid_pair_t& testVal = testVals[i];
            const uint32_t expectedPayloadSize = std::get<0>(testVal);
            const uint8_t expectedEncapHeaderSize = std::get<1>(testVal);
            const bool valid = std::get<2>(testVal);
            uint8_t outHeader8Byte[8] = { 0 };
            uint8_t encodedEncapHeaderSize = 0xff;
            //encode
            BOOST_REQUIRE_EQUAL(GetCcsdsEncapHeader((expectedPayloadSize == 0) ? ENCAP_PACKET_TYPE::IDLE : encapType,
                outHeader8Byte, expectedPayloadSize, encodedEncapHeaderSize), valid);
            BOOST_REQUIRE_EQUAL(encodedEncapHeaderSize, expectedEncapHeaderSize);
            //decode
            if (valid) {
                const uint8_t decodedEncapHeaderSize = DecodeCcsdsEncapHeaderSizeFromFirstByte(encapType, outHeader8Byte[0]);
                BOOST_REQUIRE_EQUAL(decodedEncapHeaderSize, expectedEncapHeaderSize);
                uint8_t userDefinedField = 0xff;
                uint32_t decodedEncapPayloadSize = 0xfff;
                BOOST_REQUIRE(DecodeCcsdsEncapPayloadSizeFromSecondToRemainingBytes(expectedEncapHeaderSize,
                    &outHeader8Byte[1],
                    userDefinedField,
                    decodedEncapPayloadSize));
                BOOST_REQUIRE_EQUAL(decodedEncapPayloadSize, expectedPayloadSize);
                BOOST_REQUIRE_EQUAL(userDefinedField, 0);
            }
        }
    }

    //valid decodes
    BOOST_REQUIRE_EQUAL(DecodeCcsdsEncapHeaderSizeFromFirstByte(ENCAP_PACKET_TYPE::LTP,
        ((CCSDS_ENCAP_PACKET_VERSION_NUMBER << 5) | (SANA_IDLE_ENCAP_PROTOCOL_ID << 2)) | 0), 1); //lol 0 => 1 byte encap
    BOOST_REQUIRE_EQUAL(DecodeCcsdsEncapHeaderSizeFromFirstByte(ENCAP_PACKET_TYPE::LTP,
        ((CCSDS_ENCAP_PACKET_VERSION_NUMBER << 5) | (SANA_LTP_ENCAP_PROTOCOL_ID << 2)) | 1), 2); //lol 1 => 2 byte encap
    BOOST_REQUIRE_EQUAL(DecodeCcsdsEncapHeaderSizeFromFirstByte(ENCAP_PACKET_TYPE::LTP,
        ((CCSDS_ENCAP_PACKET_VERSION_NUMBER << 5) | (SANA_LTP_ENCAP_PROTOCOL_ID << 2)) | 2), 4); //lol 2 => 4 byte encap
    BOOST_REQUIRE_EQUAL(DecodeCcsdsEncapHeaderSizeFromFirstByte(ENCAP_PACKET_TYPE::LTP,
        ((CCSDS_ENCAP_PACKET_VERSION_NUMBER << 5) | (SANA_LTP_ENCAP_PROTOCOL_ID << 2)) | 3), 8); //lol 3 => 8 byte encap
    //invalid decodes
    BOOST_REQUIRE_EQUAL(DecodeCcsdsEncapHeaderSizeFromFirstByte(ENCAP_PACKET_TYPE::LTP,
        ((CCSDS_ENCAP_PACKET_VERSION_NUMBER << 5) | (SANA_LTP_ENCAP_PROTOCOL_ID << 2)) | 0), 0); //cannot have non-idle packet with lol == 0
    BOOST_REQUIRE_EQUAL(DecodeCcsdsEncapHeaderSizeFromFirstByte(ENCAP_PACKET_TYPE::LTP,
        ((CCSDS_ENCAP_PACKET_VERSION_NUMBER << 5) | ((SANA_LTP_ENCAP_PROTOCOL_ID+1) << 2)) | 1), 0); //non-ltp, non-bp, non-idle encap protocol id
    BOOST_REQUIRE_EQUAL(DecodeCcsdsEncapHeaderSizeFromFirstByte(ENCAP_PACKET_TYPE::LTP,
        (((CCSDS_ENCAP_PACKET_VERSION_NUMBER-1) << 5) | (SANA_LTP_ENCAP_PROTOCOL_ID << 2)) | 1), 0); //wrong packet version number
    
}
