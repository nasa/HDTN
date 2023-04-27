/**
 * @file bpv7.h
 * @author  Brian Tomko <brian.j.tomko@nasa.gov>
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
 * The bpv7.h file defines all of the classes used for Bundle Protocol Version 7.
 */

#ifndef BPV7_H
#define BPV7_H 1
#include <cstdint>
#include <cstddef>
#include "Cbhe.h"
#include "TimestampUtil.h"
#include "codec/PrimaryBlock.h"
#include "codec/Cose.h"
#include "EnumAsFlagsMacro.h"
#include <array>
#include "bpcodec_export.h"
#ifndef CLASS_VISIBILITY_BPCODEC
#  ifdef _WIN32
#    define CLASS_VISIBILITY_BPCODEC
#  else
#    define CLASS_VISIBILITY_BPCODEC BPCODEC_EXPORT
#  endif
#endif

enum class BPV7_CRC_TYPE : uint8_t {
    NONE       = 0,
    CRC16_X25  = 1,
    CRC32C     = 2
};
MAKE_ENUM_SUPPORT_OSTREAM_OPERATOR(BPV7_CRC_TYPE);

enum class BPV7_BUNDLEFLAG : uint64_t {
    NO_FLAGS_SET =                        0,
    ISFRAGMENT =                          1 << 0, //(0x0001)
    ADMINRECORD =                         1 << 1, //(0x0002)
    NOFRAGMENT =                          1 << 2, //(0x0004)
    USER_APP_ACK_REQUESTED =              1 << 5, //(0x0020)
    STATUSTIME_REQUESTED =                1 << 6, //(0x0040)
    RECEPTION_STATUS_REPORTS_REQUESTED =  1 << 14,//(0x4000)
    FORWARDING_STATUS_REPORTS_REQUESTED = 1 << 16,//(0x10000)
    DELIVERY_STATUS_REPORTS_REQUESTED =   1 << 17,//(0x20000)
    DELETION_STATUS_REPORTS_REQUESTED =   1 << 18 //(0x40000)
};
MAKE_ENUM_SUPPORT_FLAG_OPERATORS(BPV7_BUNDLEFLAG);
MAKE_ENUM_SUPPORT_OSTREAM_OPERATOR(BPV7_BUNDLEFLAG);

enum class BPV7_BLOCKFLAG : uint64_t {
    NO_FLAGS_SET =                                       0,
    MUST_BE_REPLICATED =                                 1 << 0, //(0x01)
    STATUS_REPORT_REQUESTED_IF_BLOCK_CANT_BE_PROCESSED = 1 << 1, //(0x02)
    DELETE_BUNDLE_IF_BLOCK_CANT_BE_PROCESSED =           1 << 2, //(0x04)
    REMOVE_BLOCK_IF_IT_CANT_BE_PROCESSED =               1 << 4  //(0x10)
};
MAKE_ENUM_SUPPORT_FLAG_OPERATORS(BPV7_BLOCKFLAG);
MAKE_ENUM_SUPPORT_OSTREAM_OPERATOR(BPV7_BLOCKFLAG);

// https://www.iana.org/assignments/bundle/bundle.xhtml#block-types
enum class BPV7_BLOCK_TYPE_CODE : uint8_t {
    PRIMARY_IMPLICIT_ZERO       = 0,
    PAYLOAD                     = 1,
    UNUSED_2                    = 2,
    UNUSED_3                    = 3,
    UNUSED_4                    = 4,
    UNUSED_5                    = 5,
    PREVIOUS_NODE               = 6,
    BUNDLE_AGE                  = 7,
    HOP_COUNT                   = 10,
    INTEGRITY                   = 11,
    CONFIDENTIALITY             = 12,
    PRIORITY                    = 13
};
MAKE_ENUM_SUPPORT_OSTREAM_OPERATOR(BPV7_BLOCK_TYPE_CODE);

enum class BPV7_ADMINISTRATIVE_RECORD_TYPE_CODE : uint64_t {
    UNUSED_ZERO                 = 0,
    BUNDLE_STATUS_REPORT        = 1,
    BIBE_PDU                    = 3, //bundle-in-bundle encapsulation (BIBE) Protocol Data Unit (BPDU)
    CUSTODY_SIGNAL              = 4
};
MAKE_ENUM_SUPPORT_OSTREAM_OPERATOR(BPV7_ADMINISTRATIVE_RECORD_TYPE_CODE);

enum class BPV7_STATUS_REPORT_REASON_CODE : uint64_t {
    NO_FURTHER_INFORMATION                    = 0,
    LIFETIME_EXPIRED                          = 1,
    FORWARDED_OVER_UNIDIRECTIONAL_LINK        = 2,
    TRANSMISSION_CANCELLED                    = 3, // reception by a node that already has a copy of this bundle
    DEPLETED_STORAGE                          = 4,
    DESTINATION_EID_UNINTELLIGIBLE            = 5,
    NO_KNOWN_ROUTE_DESTINATION_FROM_HERE      = 6,
    NO_TIMELY_CONTACT_WITH_NEXT_NODE_ON_ROUTE = 7,
    BLOCK_UNINTELLIGIBLE                      = 8,
    HOP_LIMIT_EXCEEDED                        = 9,
    TRAFFIC_PARED                             = 10, // e.g., status reports
    BLOCK_UNSUPPORTED                         = 11
};
MAKE_ENUM_SUPPORT_OSTREAM_OPERATOR(BPV7_STATUS_REPORT_REASON_CODE);

enum class BPV7_CUSTODY_SIGNAL_DISPOSITION_CODE : uint64_t {
    CUSTODY_ACCEPTED                          = 0,
    NO_FURTHER_INFORMATION                    = 1,
    RESERVED_2                                = 2,
    REDUNDANT                                 = 3, // reception by a node that already has a copy of this bundle
    DEPLETED_STORAGE                          = 4,
    DESTINATION_EID_UNINTELLIGIBLE            = 5,
    NO_KNOWN_ROUTE_DESTINATION_FROM_HERE      = 6,
    NO_TIMELY_CONTACT_WITH_NEXT_NODE_ON_ROUTE = 7,
    BLOCK_UNINTELLIGIBLE                      = 8
};
MAKE_ENUM_SUPPORT_OSTREAM_OPERATOR(BPV7_CUSTODY_SIGNAL_DISPOSITION_CODE);


//https://www.iana.org/assignments/bundle/bundle.xhtml
enum class BPSEC_SECURITY_CONTEXT_IDENTIFIERS {
    //name = value          Description
    //------------          -----------
    BIB_HMAC_SHA2 = 1, //   BIB-HMAC-SHA2  [RFC-ietf-dtn-bpsec-default-sc-11]
    BCB_AES_GCM = 2 //      BCB-AES-GCM    [RFC-ietf-dtn-bpsec-default-sc-11]
};
enum class BPSEC_BIB_HMAX_SHA2_INTEGRITY_SCOPE_FLAGS {
    //name = value                       Description
    //------------                       -----------
    INCLUDE_PRIMARY_BLOCK_FLAG = 0, //   [RFC-ietf-dtn-bpsec-default-sc-11]
    INCLUDE_TARGET_HEADER_FLAG = 1, //   [RFC-ietf-dtn-bpsec-default-sc-11]
    INCLUDE_SECURITY_HEADER_FLAG = 2 //  [RFC-ietf-dtn-bpsec-default-sc-11]
};
enum class BPSEC_BIB_HMAX_SHA2_INTEGRITY_SCOPE_MASKS : uint64_t {
    //https://datatracker.ietf.org/doc/draft-ietf-dtn-bpsec-default-sc/ 3.3.3.  Integrity Scope Flags
    //Bit 0 (the low-order bit, 0x0001): Primary Block Flag.
    //Bit 1 (0x0002): Target Header Flag.
    //Bit 2 (0x0004): Security Header Flag.
    NO_ADDITIONAL_SCOPE = 0,
    INCLUDE_PRIMARY_BLOCK = 1 << (static_cast<uint8_t>(BPSEC_BIB_HMAX_SHA2_INTEGRITY_SCOPE_FLAGS::INCLUDE_PRIMARY_BLOCK_FLAG)),
    INCLUDE_TARGET_HEADER = 1 << (static_cast<uint8_t>(BPSEC_BIB_HMAX_SHA2_INTEGRITY_SCOPE_FLAGS::INCLUDE_TARGET_HEADER_FLAG)),
    INCLUDE_SECURITY_HEADER = 1 << (static_cast<uint8_t>(BPSEC_BIB_HMAX_SHA2_INTEGRITY_SCOPE_FLAGS::INCLUDE_SECURITY_HEADER_FLAG)),
};
MAKE_ENUM_SUPPORT_FLAG_OPERATORS(BPSEC_BIB_HMAX_SHA2_INTEGRITY_SCOPE_MASKS);
MAKE_ENUM_SUPPORT_OSTREAM_OPERATOR(BPSEC_BIB_HMAX_SHA2_INTEGRITY_SCOPE_MASKS);
/*
//https://datatracker.ietf.org/doc/draft-ietf-dtn-bpsec-default-sc/
3.3.4.  Enumerations

   The BIB-HMAC-SHA2 security context parameters are listed in Table 2.
   In this table, the "Parm Id" column refers to the expected Parameter
   Identifier described in [I-D.ietf-dtn-bpsec], Section 3.10 "Parameter
   and Result Identification".

   If the default value column is empty, this indicates that the
   security parameter does not have a default value.

   BIB-HMAC-SHA2 Security Parameters

      +=========+=============+====================+===============+
      | Parm Id |  Parm Name  | CBOR Encoding Type | Default Value |
      +=========+=============+====================+===============+
      |    1    | SHA Variant |  unsigned integer  |       6       |
      +---------+-------------+--------------------+---------------+
      |    2    | Wrapped Key |    Byte String     |               |
      +---------+-------------+--------------------+---------------+
      |    3    |  Integrity  |  unsigned integer  |       7       |
      |         | Scope Flags |                    |               |
      +---------+-------------+--------------------+---------------+

                                 Table 2*/
enum class BPSEC_BIB_HMAX_SHA2_SECURITY_PARAMETERS {
    SHA_VARIANT = 1,
    WRAPPED_KEY = 2,
    INTEGRITY_SCOPE_FLAGS = 3
};

enum class BPSEC_SHA2_VARIANT {
    HMAC256 = 1,
    HMAC512 = 2,
    HMAC384 = 3
};


/*
//https://datatracker.ietf.org/doc/draft-ietf-dtn-bpsec-default-sc/
3.4.  Results

   The BIB-HMAC-SHA2 security context results are listed in Table 3.  In
   this table, the "Result Id" column refers to the expected Result
   Identifier described in [I-D.ietf-dtn-bpsec], Section 3.10 "Parameter
   and Result Identification".

   BIB-HMAC-SHA2 Security Results

       +========+==========+===============+======================+
       | Result |  Result  | CBOR Encoding |     Description      |
       |   Id   |   Name   |      Type     |                      |
       +========+==========+===============+======================+
       |   1    | Expected |  byte string  |  The output of the   |
       |        |   HMAC   |               | HMAC calculation at  |
       |        |          |               | the security source. |
       +--------+----------+---------------+----------------------+

                                 Table 3*/
enum class BPSEC_BIB_HMAX_SHA2_SECURITY_RESULTS {
    EXPECTED_HMAC = 1
};

enum class BPSEC_BCB_AES_GCM_AAD_SCOPE_FLAGS { //BPSec BCB-AES-GCM AAD Scope Flag
    //name = value                       Description
    //------------                       -----------
    INCLUDE_PRIMARY_BLOCK_FLAG = 0, //   [RFC-ietf-dtn-bpsec-default-sc-11]
    INCLUDE_TARGET_HEADER_FLAG = 1, //   [RFC-ietf-dtn-bpsec-default-sc-11]
    INCLUDE_SECURITY_HEADER_FLAG = 2 //  [RFC-ietf-dtn-bpsec-default-sc-11]
};
enum class BPSEC_BCB_AES_GCM_AAD_SCOPE_MASKS : uint64_t {
    //https://datatracker.ietf.org/doc/draft-ietf-dtn-bpsec-default-sc/ 4.3.4.  AAD Scope Flags
    //Bit 0 (the low-order bit, 0x0001): Primary Block Flag.
    //Bit 1 (0x0002): Target Header Flag.
    //Bit 2 (0x0004): Security Header Flag.
    NO_ADDITIONAL_SCOPE = 0,
    INCLUDE_PRIMARY_BLOCK = 1 << (static_cast<uint8_t>(BPSEC_BCB_AES_GCM_AAD_SCOPE_FLAGS::INCLUDE_PRIMARY_BLOCK_FLAG)),
    INCLUDE_TARGET_HEADER = 1 << (static_cast<uint8_t>(BPSEC_BCB_AES_GCM_AAD_SCOPE_FLAGS::INCLUDE_TARGET_HEADER_FLAG)),
    INCLUDE_SECURITY_HEADER = 1 << (static_cast<uint8_t>(BPSEC_BCB_AES_GCM_AAD_SCOPE_FLAGS::INCLUDE_SECURITY_HEADER_FLAG)),
};
MAKE_ENUM_SUPPORT_FLAG_OPERATORS(BPSEC_BCB_AES_GCM_AAD_SCOPE_MASKS);
MAKE_ENUM_SUPPORT_OSTREAM_OPERATOR(BPSEC_BCB_AES_GCM_AAD_SCOPE_MASKS);
/*
//https://datatracker.ietf.org/doc/draft-ietf-dtn-bpsec-default-sc/
4.3.5.  Enumerations

   The BCB-AES-GCM security context parameters are listed in Table 5.
   In this table, the "Parm Id" column refers to the expected Parameter
   Identifier described in [I-D.ietf-dtn-bpsec], Section 3.10 "Parameter
   and Result Identification".

   If the default value column is empty, this indicates that the
   security parameter does not have a default value.

   BCB-AES-GCM Security Parameters

     +=========+================+====================+===============+
     | Parm Id |   Parm Name    | CBOR Encoding Type | Default Value |
     +=========+================+====================+===============+
     |    1    | Initialization |    Byte String     |               |
     |         |     Vector     |                    |               |
     +---------+----------------+--------------------+---------------+
     |    2    |  AES Variant   |  Unsigned Integer  |       3       |
     +---------+----------------+--------------------+---------------+
     |    3    |  Wrapped Key   |    Byte String     |               |
     +---------+----------------+--------------------+---------------+
     |    4    |   AAD Scope    |  Unsigned Integer  |       7       |
     |         |     Flags      |                    |               |
     +---------+----------------+--------------------+---------------+

                                  Table 5*/
enum class BPSEC_BCB_AES_GCM_AAD_SECURITY_PARAMETERS {
    INITIALIZATION_VECTOR = 1,
    AES_VARIANT = 2,
    WRAPPED_KEY = 3,
    AAD_SCOPE_FLAGS = 4
};
/*
//https://datatracker.ietf.org/doc/draft-ietf-dtn-bpsec-default-sc/
4.4.2.  Enumerations

   The BCB-AES-GCM security context results are listed in Table 6.  In
   this table, the "Result Id" column refers to the expected Result
   Identifier described in [I-D.ietf-dtn-bpsec], Section 3.10 "Parameter
   and Result Identification".

   BCB-AES-GCM Security Results

          +===========+====================+====================+
          | Result Id |    Result Name     | CBOR Encoding Type |
          +===========+====================+====================+
          |     1     | Authentication Tag |    Byte String     |
          +-----------+--------------------+--------------------+

                                  Table 6*/
enum class BPSEC_BCB_AES_GCM_AAD_SECURITY_RESULTS {
    AUTHENTICATION_TAG = 1
};

// Bundle priorities used by Bpv7PriorityCanonicalBlock extension block
enum BPV7_PRIORITY : uint64_t {
    BULK = 0,
    NORMAL = 1,
    EXPEDITED = 2,
    MAX_PRIORITY = 2,
    DEFAULT = 2,
    INVALID = uint64_t(~0)
};
MAKE_ENUM_SUPPORT_OSTREAM_OPERATOR(BPV7_PRIORITY);


struct CLASS_VISIBILITY_BPCODEC Bpv7CbhePrimaryBlock : public PrimaryBlock {
    static constexpr uint64_t smallestSerializedPrimarySize = //uint64_t bufferSize
        1 + //cbor initial byte denoting cbor array
        1 + //bundle version 7 byte
        1 + //m_bundleProcessingControlFlags
        1 + //crc type code byte
        3 + //destEid
        3 + //srcNodeId
        3 + //reportToEid
        3 + //creation timestamp
        1; //lifetime;

    BPV7_BUNDLEFLAG m_bundleProcessingControlFlags;
    cbhe_eid_t m_destinationEid;
    cbhe_eid_t m_sourceNodeId; //A "node ID" is an EID that identifies the administrative endpoint of a node (uses eid data type).
    cbhe_eid_t m_reportToEid;
    TimestampUtil::bpv7_creation_timestamp_t m_creationTimestamp;
    uint64_t m_lifetimeMilliseconds;
    uint64_t m_fragmentOffset;
    uint64_t m_totalApplicationDataUnitLength;
    uint32_t m_computedCrc32; //computed after serialization or deserialization
    uint16_t m_computedCrc16; //computed after serialization or deserialization
    BPV7_CRC_TYPE m_crcType; //placed uint8 at the end of struct (should be at the beginning) for more efficient memory usage

    BPCODEC_EXPORT Bpv7CbhePrimaryBlock(); //a default constructor: X()
    BPCODEC_EXPORT ~Bpv7CbhePrimaryBlock(); //a destructor: ~X()
    BPCODEC_EXPORT Bpv7CbhePrimaryBlock(const Bpv7CbhePrimaryBlock& o); //a copy constructor: X(const X&)
    BPCODEC_EXPORT Bpv7CbhePrimaryBlock(Bpv7CbhePrimaryBlock&& o); //a move constructor: X(X&&)
    BPCODEC_EXPORT Bpv7CbhePrimaryBlock& operator=(const Bpv7CbhePrimaryBlock& o); //a copy assignment: operator=(const X&)
    BPCODEC_EXPORT Bpv7CbhePrimaryBlock& operator=(Bpv7CbhePrimaryBlock&& o); //a move assignment: operator=(X&&)
    BPCODEC_EXPORT bool operator==(const Bpv7CbhePrimaryBlock & o) const; //operator ==
    BPCODEC_EXPORT bool operator!=(const Bpv7CbhePrimaryBlock & o) const; //operator !=
    BPCODEC_EXPORT void SetZero();
    BPCODEC_EXPORT uint64_t SerializeBpv7(uint8_t * serialization); //modifies m_computedCrcXX
    BPCODEC_EXPORT uint64_t GetSerializationSize() const;
    BPCODEC_EXPORT bool DeserializeBpv7(uint8_t * serialization, uint64_t & numBytesTakenToDecode, uint64_t bufferSize); //serialization must be temporarily modifyable to zero crc and restore it
    BPCODEC_EXPORT uint64_t GetMillisecondsSinceCreate() const;

    BPCODEC_EXPORT virtual bool HasCustodyFlagSet() const override;
    BPCODEC_EXPORT virtual bool HasFragmentationFlagSet() const override;
    BPCODEC_EXPORT virtual cbhe_bundle_uuid_t GetCbheBundleUuidFromPrimary() const override;
    BPCODEC_EXPORT virtual cbhe_bundle_uuid_nofragment_t GetCbheBundleUuidNoFragmentFromPrimary() const override;
    BPCODEC_EXPORT virtual cbhe_eid_t GetFinalDestinationEid() const override;
    BPCODEC_EXPORT virtual uint8_t GetPriority() const override;
    BPCODEC_EXPORT virtual uint64_t GetExpirationSeconds() const override;
    BPCODEC_EXPORT virtual uint64_t GetSequenceForSecondsScale() const override;
    BPCODEC_EXPORT virtual uint64_t GetExpirationMilliseconds() const override;
    BPCODEC_EXPORT virtual uint64_t GetSequenceForMillisecondsScale() const override;
};

struct CLASS_VISIBILITY_BPCODEC Bpv7CanonicalBlock {

    static constexpr uint64_t smallestSerializedCanonicalSize = //uint64_t bufferSize
        1 + //cbor initial byte denoting cbor array
        1 + //block type code byte
        1 + //block number
        1 + //m_blockProcessingControlFlags
        1 + //crc type code byte
        1 + //byte string header
        0 + //data
        0; //crc if not present

    static constexpr uint64_t largestZeroDataSerializedCanonicalSize = //uint64_t bufferSize
        2 + //cbor initial byte denoting cbor array
        2 + //block type code byte
        9 + //block number
        9 + //m_blockProcessingControlFlags
        1 + //crc type code byte
        9 + //byte string header
        0 + //data
        5; //crc32

    uint64_t m_blockNumber;
    BPV7_BLOCKFLAG m_blockProcessingControlFlags;
    uint8_t * m_dataPtr; //if NULL, data won't be copied (just allocated) and crc won't be computed
    uint64_t m_dataLength;
    uint32_t m_computedCrc32; //computed after serialization or deserialization
    uint16_t m_computedCrc16; //computed after serialization or deserialization
    BPV7_BLOCK_TYPE_CODE m_blockTypeCode; //placed uint8 at the end of struct (should be at the beginning) for more efficient memory usage
    BPV7_CRC_TYPE m_crcType; //placed uint8 at the end of struct for more efficient memory usage
    

    BPCODEC_EXPORT Bpv7CanonicalBlock(); //a default constructor: X()
    BPCODEC_EXPORT virtual ~Bpv7CanonicalBlock(); //a destructor: ~X()
    BPCODEC_EXPORT Bpv7CanonicalBlock(const Bpv7CanonicalBlock& o); //a copy constructor: X(const X&)
    BPCODEC_EXPORT Bpv7CanonicalBlock(Bpv7CanonicalBlock&& o); //a move constructor: X(X&&)
    BPCODEC_EXPORT Bpv7CanonicalBlock& operator=(const Bpv7CanonicalBlock& o); //a copy assignment: operator=(const X&)
    BPCODEC_EXPORT Bpv7CanonicalBlock& operator=(Bpv7CanonicalBlock&& o); //a move assignment: operator=(X&&)
    BPCODEC_EXPORT bool operator==(const Bpv7CanonicalBlock & o) const; //operator ==
    BPCODEC_EXPORT bool operator!=(const Bpv7CanonicalBlock & o) const; //operator !=
    BPCODEC_EXPORT virtual void SetZero();
    BPCODEC_EXPORT virtual uint64_t SerializeBpv7(uint8_t * serialization); //modifies m_dataPtr to serialized location
    BPCODEC_EXPORT uint64_t GetSerializationSize() const;
    BPCODEC_EXPORT virtual uint64_t GetCanonicalBlockTypeSpecificDataSerializationSize() const;
    BPCODEC_EXPORT void RecomputeCrcAfterDataModification(uint8_t * serializationBase, const uint64_t sizeSerialized);
    BPCODEC_EXPORT static bool DeserializeBpv7(std::unique_ptr<Bpv7CanonicalBlock> & canonicalPtr, uint8_t * serialization,
        uint64_t & numBytesTakenToDecode, uint64_t bufferSize, const bool skipCrcVerify, const bool isAdminRecord);
    BPCODEC_EXPORT virtual bool Virtual_DeserializeExtensionBlockDataBpv7();
};


struct CLASS_VISIBILITY_BPCODEC Bpv7PreviousNodeCanonicalBlock : public Bpv7CanonicalBlock {
    static constexpr uint64_t largestSerializedDataOnlySize =
        1 + //cbor initial byte denoting cbor array (major type 4, additional information 2)
        9 + //node number
        9; //service number
        
    BPCODEC_EXPORT Bpv7PreviousNodeCanonicalBlock(); //a default constructor: X()
    BPCODEC_EXPORT virtual ~Bpv7PreviousNodeCanonicalBlock() override; //a destructor: ~X()
    BPCODEC_EXPORT Bpv7PreviousNodeCanonicalBlock(const Bpv7PreviousNodeCanonicalBlock& o); //a copy constructor: X(const X&)
    BPCODEC_EXPORT Bpv7PreviousNodeCanonicalBlock(Bpv7PreviousNodeCanonicalBlock&& o); //a move constructor: X(X&&)
    BPCODEC_EXPORT Bpv7PreviousNodeCanonicalBlock& operator=(const Bpv7PreviousNodeCanonicalBlock& o); //a copy assignment: operator=(const X&)
    BPCODEC_EXPORT Bpv7PreviousNodeCanonicalBlock& operator=(Bpv7PreviousNodeCanonicalBlock&& o); //a move assignment: operator=(X&&)
    BPCODEC_EXPORT bool operator==(const Bpv7PreviousNodeCanonicalBlock & o) const; //operator ==
    BPCODEC_EXPORT bool operator!=(const Bpv7PreviousNodeCanonicalBlock & o) const; //operator !=
    BPCODEC_EXPORT virtual void SetZero() override;
    BPCODEC_EXPORT virtual uint64_t SerializeBpv7(uint8_t * serialization) override; //modifies m_dataPtr to serialized location
    BPCODEC_EXPORT virtual uint64_t GetCanonicalBlockTypeSpecificDataSerializationSize() const override;
    BPCODEC_EXPORT virtual bool Virtual_DeserializeExtensionBlockDataBpv7() override;

    cbhe_eid_t m_previousNode;
};

struct CLASS_VISIBILITY_BPCODEC Bpv7BundleAgeCanonicalBlock : public Bpv7CanonicalBlock {
    static constexpr uint64_t largestSerializedDataOnlySize = 9;

    BPCODEC_EXPORT Bpv7BundleAgeCanonicalBlock(); //a default constructor: X()
    BPCODEC_EXPORT virtual ~Bpv7BundleAgeCanonicalBlock() override; //a destructor: ~X()
    BPCODEC_EXPORT Bpv7BundleAgeCanonicalBlock(const Bpv7BundleAgeCanonicalBlock& o); //a copy constructor: X(const X&)
    BPCODEC_EXPORT Bpv7BundleAgeCanonicalBlock(Bpv7BundleAgeCanonicalBlock&& o); //a move constructor: X(X&&)
    BPCODEC_EXPORT Bpv7BundleAgeCanonicalBlock& operator=(const Bpv7BundleAgeCanonicalBlock& o); //a copy assignment: operator=(const X&)
    BPCODEC_EXPORT Bpv7BundleAgeCanonicalBlock& operator=(Bpv7BundleAgeCanonicalBlock&& o); //a move assignment: operator=(X&&)
    BPCODEC_EXPORT bool operator==(const Bpv7BundleAgeCanonicalBlock & o) const; //operator ==
    BPCODEC_EXPORT bool operator!=(const Bpv7BundleAgeCanonicalBlock & o) const; //operator !=
    BPCODEC_EXPORT virtual void SetZero() override;
    BPCODEC_EXPORT virtual uint64_t SerializeBpv7(uint8_t * serialization) override; //modifies m_dataPtr to serialized location
    BPCODEC_EXPORT virtual uint64_t GetCanonicalBlockTypeSpecificDataSerializationSize() const override;
    BPCODEC_EXPORT virtual bool Virtual_DeserializeExtensionBlockDataBpv7() override;

    uint64_t m_bundleAgeMilliseconds;
};

struct CLASS_VISIBILITY_BPCODEC Bpv7HopCountCanonicalBlock : public Bpv7CanonicalBlock {
    static constexpr uint64_t largestSerializedDataOnlySize =
        1 + //cbor initial byte denoting cbor array (major type 4, additional information 2)
        9 + //hop limit
        9; //hop count

    BPCODEC_EXPORT Bpv7HopCountCanonicalBlock(); //a default constructor: X()
    BPCODEC_EXPORT virtual ~Bpv7HopCountCanonicalBlock() override; //a destructor: ~X()
    BPCODEC_EXPORT Bpv7HopCountCanonicalBlock(const Bpv7HopCountCanonicalBlock& o); //a copy constructor: X(const X&)
    BPCODEC_EXPORT Bpv7HopCountCanonicalBlock(Bpv7HopCountCanonicalBlock&& o); //a move constructor: X(X&&)
    BPCODEC_EXPORT Bpv7HopCountCanonicalBlock& operator=(const Bpv7HopCountCanonicalBlock& o); //a copy assignment: operator=(const X&)
    BPCODEC_EXPORT Bpv7HopCountCanonicalBlock& operator=(Bpv7HopCountCanonicalBlock&& o); //a move assignment: operator=(X&&)
    BPCODEC_EXPORT bool operator==(const Bpv7HopCountCanonicalBlock & o) const; //operator ==
    BPCODEC_EXPORT bool operator!=(const Bpv7HopCountCanonicalBlock & o) const; //operator !=
    BPCODEC_EXPORT virtual void SetZero() override;
    BPCODEC_EXPORT virtual uint64_t SerializeBpv7(uint8_t * serialization) override; //modifies m_dataPtr to serialized location
    BPCODEC_EXPORT virtual uint64_t GetCanonicalBlockTypeSpecificDataSerializationSize() const override;
    BPCODEC_EXPORT virtual bool Virtual_DeserializeExtensionBlockDataBpv7() override;
    BPCODEC_EXPORT bool TryReserializeExtensionBlockDataWithoutResizeBpv7();

    uint64_t m_hopLimit;
    uint64_t m_hopCount;
};

struct CLASS_VISIBILITY_BPCODEC Bpv7AbstractSecurityBlockValueBase {
    BPCODEC_EXPORT virtual ~Bpv7AbstractSecurityBlockValueBase() = 0; // Pure virtual destructor
    virtual uint64_t SerializeBpv7(uint8_t * serialization, uint64_t bufferSize) = 0;
    virtual uint64_t GetSerializationSize() const = 0;
    virtual bool DeserializeBpv7(uint8_t * serialization, uint64_t & numBytesTakenToDecode, uint64_t bufferSize) = 0;
    virtual bool IsEqual(const Bpv7AbstractSecurityBlockValueBase * otherPtr) const = 0;
};
struct CLASS_VISIBILITY_BPCODEC Bpv7AbstractSecurityBlockValueUint : public Bpv7AbstractSecurityBlockValueBase {
    BPCODEC_EXPORT virtual ~Bpv7AbstractSecurityBlockValueUint() override;
    BPCODEC_EXPORT virtual uint64_t SerializeBpv7(uint8_t * serialization, uint64_t bufferSize) override;
    BPCODEC_EXPORT virtual uint64_t GetSerializationSize() const override;
    BPCODEC_EXPORT virtual bool DeserializeBpv7(uint8_t * serialization, uint64_t & numBytesTakenToDecode, uint64_t bufferSize) override;
    BPCODEC_EXPORT virtual bool IsEqual(const Bpv7AbstractSecurityBlockValueBase * otherPtr) const override;

    uint64_t m_uintValue;
};
struct CLASS_VISIBILITY_BPCODEC Bpv7AbstractSecurityBlockValueByteString : public Bpv7AbstractSecurityBlockValueBase {
    BPCODEC_EXPORT virtual ~Bpv7AbstractSecurityBlockValueByteString() override;
    BPCODEC_EXPORT virtual uint64_t SerializeBpv7(uint8_t * serialization, uint64_t bufferSize) override;
    BPCODEC_EXPORT virtual uint64_t GetSerializationSize() const override;
    BPCODEC_EXPORT virtual bool DeserializeBpv7(uint8_t * serialization, uint64_t & numBytesTakenToDecode, uint64_t bufferSize) override;
    BPCODEC_EXPORT virtual bool IsEqual(const Bpv7AbstractSecurityBlockValueBase * otherPtr) const override;

    std::vector<uint8_t> m_byteString;
};

struct CLASS_VISIBILITY_BPCODEC Bpv7AbstractSecurityBlock : public Bpv7CanonicalBlock {

    typedef std::vector<uint64_t> security_targets_t;
    typedef uint64_t security_context_id_t;
    typedef uint8_t security_context_flags_t;
    //generic typedefs for cipher suite parameters and security results
    typedef uint64_t id_t;
    typedef std::unique_ptr<Bpv7AbstractSecurityBlockValueBase> value_ptr_t;
    typedef std::pair<id_t, value_ptr_t> id_value_pair_t;
    typedef std::vector<id_value_pair_t> id_value_pairs_vec_t;
    //cipher suite parameters:
    typedef id_t parameter_id_t;
    typedef value_ptr_t parameter_value_t;
    typedef id_value_pair_t security_context_parameter_t;
    typedef id_value_pairs_vec_t security_context_parameters_t;
    //security result:
    typedef id_t security_result_id_t;
    typedef value_ptr_t security_result_value_t;
    typedef id_value_pair_t security_result_t;
    typedef id_value_pairs_vec_t security_results_t;


    BPCODEC_EXPORT Bpv7AbstractSecurityBlock(); //a default constructor: X()
    BPCODEC_EXPORT virtual ~Bpv7AbstractSecurityBlock() override; //a destructor: ~X()
    BPCODEC_EXPORT Bpv7AbstractSecurityBlock(const Bpv7AbstractSecurityBlock& o) = delete; //a copy constructor: X(const X&)
    BPCODEC_EXPORT Bpv7AbstractSecurityBlock(Bpv7AbstractSecurityBlock&& o); //a move constructor: X(X&&)
    BPCODEC_EXPORT Bpv7AbstractSecurityBlock& operator=(const Bpv7AbstractSecurityBlock& o) = delete; //a copy assignment: operator=(const X&)
    BPCODEC_EXPORT Bpv7AbstractSecurityBlock& operator=(Bpv7AbstractSecurityBlock&& o); //a move assignment: operator=(X&&)
    BPCODEC_EXPORT bool operator==(const Bpv7AbstractSecurityBlock & o) const; //operator ==
    BPCODEC_EXPORT bool operator!=(const Bpv7AbstractSecurityBlock & o) const; //operator !=
    BPCODEC_EXPORT virtual void SetZero() override;
    BPCODEC_EXPORT virtual uint64_t SerializeBpv7(uint8_t * serialization) override; //modifies m_dataPtr to serialized location
    BPCODEC_EXPORT virtual uint64_t GetCanonicalBlockTypeSpecificDataSerializationSize() const override;
    BPCODEC_EXPORT virtual bool Virtual_DeserializeExtensionBlockDataBpv7() override;
    BPCODEC_EXPORT bool IsSecurityContextParametersPresent() const;
    BPCODEC_EXPORT void SetSecurityContextParametersPresent();
    BPCODEC_EXPORT void ClearSecurityContextParametersPresent();
    BPCODEC_EXPORT void SetSecurityContextId(BPSEC_SECURITY_CONTEXT_IDENTIFIERS id);

    BPCODEC_EXPORT static uint64_t SerializeIdValuePairsVecBpv7(uint8_t * serialization,
        const id_value_pairs_vec_t & idValuePairsVec, uint64_t bufferSize, bool encapsulatePairInArrayOfSizeOne);
    BPCODEC_EXPORT static uint64_t IdValuePairsVecBpv7SerializationSize(const id_value_pairs_vec_t & idValuePairsVec, bool encapsulatePairInArrayOfSizeOne);
    BPCODEC_EXPORT static bool DeserializeIdValuePairsVecBpv7(uint8_t * serialization, uint64_t & numBytesTakenToDecode, uint64_t bufferSize, id_value_pairs_vec_t & idValuePairsVec,
        const BPSEC_SECURITY_CONTEXT_IDENTIFIERS securityContext, const bool isForSecurityParameters, const uint64_t maxElements, bool pairIsEncapsulateInArrayOfSizeOne);
    BPCODEC_EXPORT static bool DeserializeIdValuePairBpv7(uint8_t * serialization, uint64_t & numBytesTakenToDecode, uint64_t bufferSize, id_value_pair_t & idValuePair,
        const BPSEC_SECURITY_CONTEXT_IDENTIFIERS securityContext, const bool isForSecurityParameters);
    BPCODEC_EXPORT static bool IsEqual(const id_value_pairs_vec_t & pVec1, const id_value_pairs_vec_t & pVec2);

    security_targets_t m_securityTargets;
    security_context_id_t m_securityContextId;
    security_context_flags_t m_securityContextFlags;
    cbhe_eid_t m_securitySource;
    security_context_parameters_t m_securityContextParametersOptional;
    security_results_t m_securityResults;

protected:
    std::vector<uint8_t> * Protected_AppendAndGetSecurityResultByteStringPtr(uint64_t resultType);
    std::vector<std::vector<uint8_t>*> Protected_GetAllSecurityResultsByteStringPtrs(uint64_t resultType);
};

struct CLASS_VISIBILITY_BPCODEC Bpv7BlockIntegrityBlock : public Bpv7AbstractSecurityBlock {
    BPCODEC_EXPORT Bpv7BlockIntegrityBlock(); //a default constructor: X()
    BPCODEC_EXPORT virtual ~Bpv7BlockIntegrityBlock() override; //a destructor: ~X()
    BPCODEC_EXPORT Bpv7BlockIntegrityBlock(const Bpv7BlockIntegrityBlock& o) = delete; //a copy constructor: X(const X&)
    BPCODEC_EXPORT Bpv7BlockIntegrityBlock(Bpv7BlockIntegrityBlock&& o); //a move constructor: X(X&&)
    BPCODEC_EXPORT Bpv7BlockIntegrityBlock& operator=(const Bpv7BlockIntegrityBlock& o) = delete; //a copy assignment: operator=(const X&)
    BPCODEC_EXPORT Bpv7BlockIntegrityBlock& operator=(Bpv7BlockIntegrityBlock&& o); //a move assignment: operator=(X&&)
    BPCODEC_EXPORT bool operator==(const Bpv7BlockIntegrityBlock & o) const; //operator ==
    BPCODEC_EXPORT bool operator!=(const Bpv7BlockIntegrityBlock & o) const; //operator !=
    BPCODEC_EXPORT virtual void SetZero() override;
    
    BPCODEC_EXPORT bool AddOrUpdateSecurityParameterShaVariant(COSE_ALGORITHMS alg);
    BPCODEC_EXPORT COSE_ALGORITHMS GetSecurityParameterShaVariant(bool & success) const;
    BPCODEC_EXPORT bool AddSecurityParameterIntegrityScope(BPSEC_BIB_HMAX_SHA2_INTEGRITY_SCOPE_MASKS integrityScope);
    BPCODEC_EXPORT bool IsSecurityParameterIntegrityScopePresentAndSet(BPSEC_BIB_HMAX_SHA2_INTEGRITY_SCOPE_MASKS integrityScope) const;
    BPCODEC_EXPORT std::vector<uint8_t> * AddAndGetWrappedKeyPtr();
    BPCODEC_EXPORT std::vector<uint8_t> * AppendAndGetExpectedHmacPtr();
    BPCODEC_EXPORT std::vector<std::vector<uint8_t>*> GetAllExpectedHmacPtrs();
};

struct CLASS_VISIBILITY_BPCODEC Bpv7BlockConfidentialityBlock : public Bpv7AbstractSecurityBlock {
    BPCODEC_EXPORT Bpv7BlockConfidentialityBlock(); //a default constructor: X()
    BPCODEC_EXPORT virtual ~Bpv7BlockConfidentialityBlock() override; //a destructor: ~X()
    BPCODEC_EXPORT Bpv7BlockConfidentialityBlock(const Bpv7BlockConfidentialityBlock& o) = delete; //a copy constructor: X(const X&)
    BPCODEC_EXPORT Bpv7BlockConfidentialityBlock(Bpv7BlockConfidentialityBlock&& o); //a move constructor: X(X&&)
    BPCODEC_EXPORT Bpv7BlockConfidentialityBlock& operator=(const Bpv7BlockConfidentialityBlock& o) = delete; //a copy assignment: operator=(const X&)
    BPCODEC_EXPORT Bpv7BlockConfidentialityBlock& operator=(Bpv7BlockConfidentialityBlock&& o); //a move assignment: operator=(X&&)
    BPCODEC_EXPORT bool operator==(const Bpv7BlockConfidentialityBlock & o) const; //operator ==
    BPCODEC_EXPORT bool operator!=(const Bpv7BlockConfidentialityBlock & o) const; //operator !=
    BPCODEC_EXPORT virtual void SetZero() override;

    BPCODEC_EXPORT bool AddOrUpdateSecurityParameterAesVariant(COSE_ALGORITHMS alg);
    BPCODEC_EXPORT COSE_ALGORITHMS GetSecurityParameterAesVariant(bool & success) const;
    BPCODEC_EXPORT bool AddSecurityParameterScope(BPSEC_BCB_AES_GCM_AAD_SCOPE_MASKS scope);
    BPCODEC_EXPORT bool IsSecurityParameterScopePresentAndSet(BPSEC_BCB_AES_GCM_AAD_SCOPE_MASKS scope) const;
    BPCODEC_EXPORT std::vector<uint8_t> * AddAndGetAesWrappedKeyPtr();
    BPCODEC_EXPORT std::vector<uint8_t> * GetAesWrappedKeyPtr();
    BPCODEC_EXPORT std::vector<uint8_t> * AddAndGetInitializationVectorPtr();
    BPCODEC_EXPORT std::vector<uint8_t> * GetInitializationVectorPtr();
    BPCODEC_EXPORT std::vector<uint8_t> * AppendAndGetPayloadAuthenticationTagPtr();
    BPCODEC_EXPORT std::vector<std::vector<uint8_t>*> GetAllPayloadAuthenticationTagPtrs();
private:
    BPCODEC_NO_EXPORT std::vector<uint8_t> * Private_AddAndGetByteStringParamPtr(BPSEC_BCB_AES_GCM_AAD_SECURITY_PARAMETERS parameter);
    BPCODEC_NO_EXPORT std::vector<uint8_t> * Private_GetByteStringParamPtr(BPSEC_BCB_AES_GCM_AAD_SECURITY_PARAMETERS parameter);
};

struct CLASS_VISIBILITY_BPCODEC Bpv7AdministrativeRecordContentBase {
    BPCODEC_EXPORT virtual ~Bpv7AdministrativeRecordContentBase() = 0; // Pure virtual destructor
    virtual uint64_t SerializeBpv7(uint8_t * serialization, uint64_t bufferSize) = 0;
    virtual uint64_t GetSerializationSize() const = 0;
    virtual bool DeserializeBpv7(uint8_t * serialization, uint64_t & numBytesTakenToDecode, uint64_t bufferSize) = 0;
    virtual bool IsEqual(const Bpv7AdministrativeRecordContentBase * otherPtr) const = 0;
};
struct CLASS_VISIBILITY_BPCODEC Bpv7AdministrativeRecordContentBundleStatusReport : public Bpv7AdministrativeRecordContentBase {
    typedef std::pair<bool, uint64_t> status_info_content_t; //[status-indicator: bool, optional_timestamp: dtn_time]
    typedef std::array<status_info_content_t, 4> bundle_status_information_t;

    BPCODEC_EXPORT virtual ~Bpv7AdministrativeRecordContentBundleStatusReport() override;
    BPCODEC_EXPORT virtual uint64_t SerializeBpv7(uint8_t * serialization, uint64_t bufferSize) override;
    BPCODEC_EXPORT virtual uint64_t GetSerializationSize() const override;
    BPCODEC_EXPORT virtual bool DeserializeBpv7(uint8_t * serialization, uint64_t & numBytesTakenToDecode, uint64_t bufferSize) override;
    BPCODEC_EXPORT virtual bool IsEqual(const Bpv7AdministrativeRecordContentBase * otherPtr) const override;

    
    // status-record-content = [
    //  bundle-status-information,
    //  status-report-reason-code: uint,
    //  source-node-eid: eid,
    //  subject-creation-timestamp: creation-timestamp,
    //  ? (
    //    subject-payload-offset: uint,
    //    subject-payload-length: uint
    //  )
    // ]
    bundle_status_information_t m_bundleStatusInfo;
    BPV7_STATUS_REPORT_REASON_CODE m_statusReportReasonCode;
    cbhe_eid_t m_sourceNodeEid;
    TimestampUtil::bpv7_creation_timestamp_t m_creationTimestamp;
    uint64_t m_optionalSubjectPayloadFragmentOffset;
    uint64_t m_optionalSubjectPayloadFragmentLength;
    bool m_subjectBundleIsFragment;
    bool m_reportStatusTimeFlagWasSet;
};
struct CLASS_VISIBILITY_BPCODEC Bpv7AdministrativeRecordContentBibePduMessage : public Bpv7AdministrativeRecordContentBase {
    
    BPCODEC_EXPORT virtual ~Bpv7AdministrativeRecordContentBibePduMessage() override;
    BPCODEC_EXPORT virtual uint64_t SerializeBpv7(uint8_t * serialization, uint64_t bufferSize) override;
    BPCODEC_EXPORT virtual uint64_t GetSerializationSize() const override;
    BPCODEC_EXPORT virtual bool DeserializeBpv7(uint8_t * serialization, uint64_t & numBytesTakenToDecode, uint64_t bufferSize) override;
    BPCODEC_EXPORT virtual bool IsEqual(const Bpv7AdministrativeRecordContentBase * otherPtr) const override;

    uint64_t m_transmissionId;
    uint64_t m_custodyRetransmissionTime;
    uint8_t * m_encapsulatedBundlePtr;
    uint64_t m_encapsulatedBundleLength;
    std::vector<uint8_t> m_temporaryEncapsulatedBundleStorage;
};
struct CLASS_VISIBILITY_BPCODEC Bpv7AdministrativeRecord : public Bpv7CanonicalBlock {

    BPV7_ADMINISTRATIVE_RECORD_TYPE_CODE m_adminRecordTypeCode;
    std::unique_ptr<Bpv7AdministrativeRecordContentBase> m_adminRecordContentPtr;
    
    BPCODEC_EXPORT Bpv7AdministrativeRecord(); //a default constructor: X()
    BPCODEC_EXPORT virtual ~Bpv7AdministrativeRecord() override; //a destructor: ~X()
    BPCODEC_EXPORT Bpv7AdministrativeRecord(const Bpv7AdministrativeRecord& o) = delete;; //a copy constructor: X(const X&)
    BPCODEC_EXPORT Bpv7AdministrativeRecord(Bpv7AdministrativeRecord&& o); //a move constructor: X(X&&)
    BPCODEC_EXPORT Bpv7AdministrativeRecord& operator=(const Bpv7AdministrativeRecord& o) = delete;; //a copy assignment: operator=(const X&)
    BPCODEC_EXPORT Bpv7AdministrativeRecord& operator=(Bpv7AdministrativeRecord&& o); //a move assignment: operator=(X&&)
    BPCODEC_EXPORT bool operator==(const Bpv7AdministrativeRecord & o) const; //operator ==
    BPCODEC_EXPORT bool operator!=(const Bpv7AdministrativeRecord & o) const; //operator !=
    BPCODEC_EXPORT virtual void SetZero() override;
    BPCODEC_EXPORT virtual uint64_t SerializeBpv7(uint8_t * serialization) override; //modifies m_dataPtr to serialized location
    BPCODEC_EXPORT virtual uint64_t GetCanonicalBlockTypeSpecificDataSerializationSize() const override;
    BPCODEC_EXPORT virtual bool Virtual_DeserializeExtensionBlockDataBpv7() override;
};

struct CLASS_VISIBILITY_BPCODEC Bpv7PriorityCanonicalBlock : public Bpv7CanonicalBlock {
    static constexpr uint64_t largestSerializedDataOnlySize = 9;

    BPCODEC_EXPORT Bpv7PriorityCanonicalBlock();
    BPCODEC_EXPORT virtual ~Bpv7PriorityCanonicalBlock() override;
    BPCODEC_EXPORT Bpv7PriorityCanonicalBlock(const Bpv7PriorityCanonicalBlock& o);
    BPCODEC_EXPORT Bpv7PriorityCanonicalBlock(Bpv7PriorityCanonicalBlock&& o);
    BPCODEC_EXPORT Bpv7PriorityCanonicalBlock& operator=(const Bpv7PriorityCanonicalBlock& o);
    BPCODEC_EXPORT Bpv7PriorityCanonicalBlock& operator=(Bpv7PriorityCanonicalBlock&& o);
    BPCODEC_EXPORT bool operator==(const Bpv7PriorityCanonicalBlock & o) const;
    BPCODEC_EXPORT bool operator!=(const Bpv7PriorityCanonicalBlock & o) const;
    BPCODEC_EXPORT virtual void SetZero() override;
    BPCODEC_EXPORT virtual uint64_t SerializeBpv7(uint8_t * serialization) override;
    BPCODEC_EXPORT virtual uint64_t GetCanonicalBlockTypeSpecificDataSerializationSize() const override;
    BPCODEC_EXPORT virtual bool Virtual_DeserializeExtensionBlockDataBpv7() override;

    uint64_t m_bundlePriority;
};

#endif //BPV7_H

