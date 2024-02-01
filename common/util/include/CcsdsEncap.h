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
 * This is a header-only library for LTP/BP/IDLE encapsulation packet defines.
 * Encode and decode functions are in CcsdsEncapEncode.h and CcsdsEncapDecode.h, respectively.
 * Based on: Encapsulation Packet Protocol: https://public.ccsds.org/Pubs/133x1b3e1.pdf
 */

#ifndef _CCSDS_ENCAP_H
#define _CCSDS_ENCAP_H 1

#include <cstdint>

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

//Encode and decode functions are in CcsdsEncapEncode.h and CcsdsEncapDecode.h, respectively.
#endif // _CCSDS_ENCAP_H
