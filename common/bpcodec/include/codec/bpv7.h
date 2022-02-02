#ifndef BPV7_H
#define BPV7_H 1
#include <cstdint>
#include <cstddef>
#include "Cbhe.h"
#include "TimestampUtil.h"
#include "codec/PrimaryBlock.h"
#include "codec/Cose.h"
#include "EnumAsFlagsMacro.h"

#define BPV7_CRC_TYPE_NONE        0
#define BPV7_CRC_TYPE_CRC16_X25   1
#define BPV7_CRC_TYPE_CRC32C      2

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

enum class BPV7_BLOCKFLAG : uint64_t {
    NO_FLAGS_SET =                                       0,
    MUST_BE_REPLICATED =                                 1 << 0, //(0x01)
    STATUS_REPORT_REQUESTED_IF_BLOCK_CANT_BE_PROCESSED = 1 << 1, //(0x02)
    DELETE_BUNDLE_IF_BLOCK_CANT_BE_PROCESSED =           1 << 2, //(0x04)
    REMOVE_BLOCK_IF_IT_CANT_BE_PROCESSED =               1 << 4  //(0x10)
};
MAKE_ENUM_SUPPORT_FLAG_OPERATORS(BPV7_BLOCKFLAG);

// https://www.iana.org/assignments/bundle/bundle.xhtml#block-types
#define BPV7_BLOCKTYPE_PAYLOAD                     1U
#define BPV7_BLOCKTYPE_PREVIOUS_NODE               6U
#define BPV7_BLOCKTYPE_BUNDLE_AGE                  7U
#define BPV7_BLOCKTYPE_HOP_COUNT                   10U
#define BPV7_BLOCKTYPE_BLOCK_INTEGRITY             11U
#define BPV7_BLOCKTYPE_BLOCK_CONFIDENTIALITY       12U

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


struct Bpv7CbhePrimaryBlock : public PrimaryBlock {
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
    uint8_t m_crcType; //placed uint8 at the end of struct (should be at the beginning) for more efficient memory usage

    Bpv7CbhePrimaryBlock(); //a default constructor: X()
    ~Bpv7CbhePrimaryBlock(); //a destructor: ~X()
    Bpv7CbhePrimaryBlock(const Bpv7CbhePrimaryBlock& o); //a copy constructor: X(const X&)
    Bpv7CbhePrimaryBlock(Bpv7CbhePrimaryBlock&& o); //a move constructor: X(X&&)
    Bpv7CbhePrimaryBlock& operator=(const Bpv7CbhePrimaryBlock& o); //a copy assignment: operator=(const X&)
    Bpv7CbhePrimaryBlock& operator=(Bpv7CbhePrimaryBlock&& o); //a move assignment: operator=(X&&)
    bool operator==(const Bpv7CbhePrimaryBlock & o) const; //operator ==
    bool operator!=(const Bpv7CbhePrimaryBlock & o) const; //operator !=
    void SetZero();
    uint64_t SerializeBpv7(uint8_t * serialization); //modifies m_computedCrcXX
    uint64_t GetSerializationSize() const;
    bool DeserializeBpv7(uint8_t * serialization, uint64_t & numBytesTakenToDecode, uint64_t bufferSize); //serialization must be temporarily modifyable to zero crc and restore it

    virtual bool HasCustodyFlagSet() const;
    virtual bool HasFragmentationFlagSet() const;
    virtual cbhe_bundle_uuid_t GetCbheBundleUuidFromPrimary() const;
    virtual cbhe_bundle_uuid_nofragment_t GetCbheBundleUuidNoFragmentFromPrimary() const;
    virtual cbhe_eid_t GetFinalDestinationEid() const;
    virtual uint8_t GetPriority() const;
    virtual uint64_t GetExpirationSeconds() const;
    virtual uint64_t GetSequenceForSecondsScale() const;
    virtual uint64_t GetExpirationMilliseconds() const;
    virtual uint64_t GetSequenceForMillisecondsScale() const;
};

struct Bpv7CanonicalBlock {

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
    uint8_t m_blockTypeCode; //placed uint8 at the end of struct (should be at the beginning) for more efficient memory usage
    uint8_t m_crcType; //placed uint8 at the end of struct for more efficient memory usage
    
    
    Bpv7CanonicalBlock(); //a default constructor: X()
    virtual ~Bpv7CanonicalBlock(); //a destructor: ~X()
    Bpv7CanonicalBlock(const Bpv7CanonicalBlock& o); //a copy constructor: X(const X&)
    Bpv7CanonicalBlock(Bpv7CanonicalBlock&& o); //a move constructor: X(X&&)
    Bpv7CanonicalBlock& operator=(const Bpv7CanonicalBlock& o); //a copy assignment: operator=(const X&)
    Bpv7CanonicalBlock& operator=(Bpv7CanonicalBlock&& o); //a move assignment: operator=(X&&)
    bool operator==(const Bpv7CanonicalBlock & o) const; //operator ==
    bool operator!=(const Bpv7CanonicalBlock & o) const; //operator !=
    virtual void SetZero();
    virtual uint64_t SerializeBpv7(uint8_t * serialization); //modifies m_dataPtr to serialized location
    virtual uint64_t GetSerializationSize() const;
    uint64_t GetSerializationSize(const uint64_t dataLength) const;
    void RecomputeCrcAfterDataModification(uint8_t * serializationBase, const uint64_t sizeSerialized);
    static bool DeserializeBpv7(std::unique_ptr<Bpv7CanonicalBlock> & canonicalPtr, uint8_t * serialization, uint64_t & numBytesTakenToDecode, uint64_t bufferSize, const bool skipCrcVerify);
    virtual bool Virtual_DeserializeExtensionBlockDataBpv7();
};


struct Bpv7PreviousNodeCanonicalBlock : public Bpv7CanonicalBlock {
    static constexpr uint64_t largestSerializedDataOnlySize =
        1 + //cbor initial byte denoting cbor array (major type 4, additional information 2)
        9 + //node number
        9; //service number
        
    Bpv7PreviousNodeCanonicalBlock(); //a default constructor: X()
    virtual ~Bpv7PreviousNodeCanonicalBlock(); //a destructor: ~X()
    Bpv7PreviousNodeCanonicalBlock(const Bpv7PreviousNodeCanonicalBlock& o); //a copy constructor: X(const X&)
    Bpv7PreviousNodeCanonicalBlock(Bpv7PreviousNodeCanonicalBlock&& o); //a move constructor: X(X&&)
    Bpv7PreviousNodeCanonicalBlock& operator=(const Bpv7PreviousNodeCanonicalBlock& o); //a copy assignment: operator=(const X&)
    Bpv7PreviousNodeCanonicalBlock& operator=(Bpv7PreviousNodeCanonicalBlock&& o); //a move assignment: operator=(X&&)
    bool operator==(const Bpv7PreviousNodeCanonicalBlock & o) const; //operator ==
    bool operator!=(const Bpv7PreviousNodeCanonicalBlock & o) const; //operator !=
    virtual void SetZero();
    virtual uint64_t SerializeBpv7(uint8_t * serialization); //modifies m_dataPtr to serialized location
    virtual uint64_t GetSerializationSize() const;
    virtual bool Virtual_DeserializeExtensionBlockDataBpv7();

    cbhe_eid_t m_previousNode;
};

struct Bpv7BundleAgeCanonicalBlock : public Bpv7CanonicalBlock {
    static constexpr uint64_t largestSerializedDataOnlySize = 9;

    Bpv7BundleAgeCanonicalBlock(); //a default constructor: X()
    virtual ~Bpv7BundleAgeCanonicalBlock(); //a destructor: ~X()
    Bpv7BundleAgeCanonicalBlock(const Bpv7BundleAgeCanonicalBlock& o); //a copy constructor: X(const X&)
    Bpv7BundleAgeCanonicalBlock(Bpv7BundleAgeCanonicalBlock&& o); //a move constructor: X(X&&)
    Bpv7BundleAgeCanonicalBlock& operator=(const Bpv7BundleAgeCanonicalBlock& o); //a copy assignment: operator=(const X&)
    Bpv7BundleAgeCanonicalBlock& operator=(Bpv7BundleAgeCanonicalBlock&& o); //a move assignment: operator=(X&&)
    bool operator==(const Bpv7BundleAgeCanonicalBlock & o) const; //operator ==
    bool operator!=(const Bpv7BundleAgeCanonicalBlock & o) const; //operator !=
    virtual void SetZero();
    virtual uint64_t SerializeBpv7(uint8_t * serialization); //modifies m_dataPtr to serialized location
    virtual uint64_t GetSerializationSize() const;
    virtual bool Virtual_DeserializeExtensionBlockDataBpv7();

    uint64_t m_bundleAgeMilliseconds;
};

struct Bpv7HopCountCanonicalBlock : public Bpv7CanonicalBlock {
    static constexpr uint64_t largestSerializedDataOnlySize =
        1 + //cbor initial byte denoting cbor array (major type 4, additional information 2)
        9 + //hop limit
        9; //hop count

    Bpv7HopCountCanonicalBlock(); //a default constructor: X()
    virtual ~Bpv7HopCountCanonicalBlock(); //a destructor: ~X()
    Bpv7HopCountCanonicalBlock(const Bpv7HopCountCanonicalBlock& o); //a copy constructor: X(const X&)
    Bpv7HopCountCanonicalBlock(Bpv7HopCountCanonicalBlock&& o); //a move constructor: X(X&&)
    Bpv7HopCountCanonicalBlock& operator=(const Bpv7HopCountCanonicalBlock& o); //a copy assignment: operator=(const X&)
    Bpv7HopCountCanonicalBlock& operator=(Bpv7HopCountCanonicalBlock&& o); //a move assignment: operator=(X&&)
    bool operator==(const Bpv7HopCountCanonicalBlock & o) const; //operator ==
    bool operator!=(const Bpv7HopCountCanonicalBlock & o) const; //operator !=
    virtual void SetZero();
    virtual uint64_t SerializeBpv7(uint8_t * serialization); //modifies m_dataPtr to serialized location
    virtual uint64_t GetSerializationSize() const;
    virtual bool Virtual_DeserializeExtensionBlockDataBpv7();
    bool TryReserializeExtensionBlockDataWithoutResizeBpv7();

    uint64_t m_hopLimit;
    uint64_t m_hopCount;
};

struct Bpv7AbstractSecurityBlockValueBase {
    virtual uint64_t SerializeBpv7(uint8_t * serialization) = 0;
    virtual uint64_t GetSerializationSize() const = 0;
    virtual bool DeserializeBpv7(uint8_t * serialization, uint64_t & numBytesTakenToDecode, uint64_t bufferSize) = 0;
    virtual bool IsEqual(const Bpv7AbstractSecurityBlockValueBase * otherPtr) const = 0;
};
struct Bpv7AbstractSecurityBlockValueUint : public Bpv7AbstractSecurityBlockValueBase {
    virtual uint64_t SerializeBpv7(uint8_t * serialization);
    virtual uint64_t GetSerializationSize() const;
    virtual bool DeserializeBpv7(uint8_t * serialization, uint64_t & numBytesTakenToDecode, uint64_t bufferSize);
    virtual bool IsEqual(const Bpv7AbstractSecurityBlockValueBase * otherPtr) const;

    uint64_t m_uintValue;
};
struct Bpv7AbstractSecurityBlockValueByteString : public Bpv7AbstractSecurityBlockValueBase {
    virtual uint64_t SerializeBpv7(uint8_t * serialization);
    virtual uint64_t GetSerializationSize() const;
    virtual bool DeserializeBpv7(uint8_t * serialization, uint64_t & numBytesTakenToDecode, uint64_t bufferSize);
    virtual bool IsEqual(const Bpv7AbstractSecurityBlockValueBase * otherPtr) const;

    std::vector<uint8_t> m_byteString;
};

struct Bpv7AbstractSecurityBlock : public Bpv7CanonicalBlock {

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


    Bpv7AbstractSecurityBlock(); //a default constructor: X()
    virtual ~Bpv7AbstractSecurityBlock(); //a destructor: ~X()
    Bpv7AbstractSecurityBlock(const Bpv7AbstractSecurityBlock& o) = delete; //a copy constructor: X(const X&)
    Bpv7AbstractSecurityBlock(Bpv7AbstractSecurityBlock&& o); //a move constructor: X(X&&)
    Bpv7AbstractSecurityBlock& operator=(const Bpv7AbstractSecurityBlock& o) = delete; //a copy assignment: operator=(const X&)
    Bpv7AbstractSecurityBlock& operator=(Bpv7AbstractSecurityBlock&& o); //a move assignment: operator=(X&&)
    bool operator==(const Bpv7AbstractSecurityBlock & o) const; //operator ==
    bool operator!=(const Bpv7AbstractSecurityBlock & o) const; //operator !=
    virtual void SetZero();
    virtual uint64_t SerializeBpv7(uint8_t * serialization); //modifies m_dataPtr to serialized location
    virtual uint64_t GetSerializationSize() const;
    virtual bool Virtual_DeserializeExtensionBlockDataBpv7();
    bool IsSecurityContextParametersPresent() const;
    void SetSecurityContextParametersPresent();
    void ClearSecurityContextParametersPresent();
    void SetSecurityContextId(BPSEC_SECURITY_CONTEXT_IDENTIFIERS id);

    static uint64_t SerializeIdValuePairsVecBpv7(uint8_t * serialization, const id_value_pairs_vec_t & idValuePairsVec);
    static uint64_t IdValuePairsVecBpv7SerializationSize(const id_value_pairs_vec_t & idValuePairsVec);
    static bool DeserializeIdValuePairsVecBpv7(uint8_t * serialization, uint64_t & numBytesTakenToDecode, uint64_t bufferSize, id_value_pairs_vec_t & idValuePairsVec,
        const BPSEC_SECURITY_CONTEXT_IDENTIFIERS securityContext, const bool isForSecurityParameters, const uint64_t maxElements);
    static bool DeserializeIdValuePairBpv7(uint8_t * serialization, uint64_t & numBytesTakenToDecode, uint64_t bufferSize, id_value_pair_t & idValuePair,
        const BPSEC_SECURITY_CONTEXT_IDENTIFIERS securityContext, const bool isForSecurityParameters);
    static bool IsEqual(const id_value_pairs_vec_t & pVec1, const id_value_pairs_vec_t & pVec2);

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

struct Bpv7BlockIntegrityBlock : public Bpv7AbstractSecurityBlock {
    Bpv7BlockIntegrityBlock(); //a default constructor: X()
    virtual ~Bpv7BlockIntegrityBlock(); //a destructor: ~X()
    Bpv7BlockIntegrityBlock(const Bpv7BlockIntegrityBlock& o) = delete; //a copy constructor: X(const X&)
    Bpv7BlockIntegrityBlock(Bpv7BlockIntegrityBlock&& o); //a move constructor: X(X&&)
    Bpv7BlockIntegrityBlock& operator=(const Bpv7BlockIntegrityBlock& o) = delete; //a copy assignment: operator=(const X&)
    Bpv7BlockIntegrityBlock& operator=(Bpv7BlockIntegrityBlock&& o); //a move assignment: operator=(X&&)
    bool operator==(const Bpv7BlockIntegrityBlock & o) const; //operator ==
    bool operator!=(const Bpv7BlockIntegrityBlock & o) const; //operator !=
    virtual void SetZero();
    
    bool AddOrUpdateSecurityParameterShaVariant(COSE_ALGORITHMS alg);
    COSE_ALGORITHMS GetSecurityParameterShaVariant(bool & success) const;
    bool AddSecurityParameterIntegrityScope(BPSEC_BIB_HMAX_SHA2_INTEGRITY_SCOPE_MASKS integrityScope);
    bool IsSecurityParameterIntegrityScopePresentAndSet(BPSEC_BIB_HMAX_SHA2_INTEGRITY_SCOPE_MASKS integrityScope) const;
    std::vector<uint8_t> * AddAndGetWrappedKeyPtr();
    std::vector<uint8_t> * AppendAndGetExpectedHmacPtr();
    std::vector<std::vector<uint8_t>*> GetAllExpectedHmacPtrs();
};

struct Bpv7BlockConfidentialityBlock : public Bpv7AbstractSecurityBlock {
    Bpv7BlockConfidentialityBlock(); //a default constructor: X()
    virtual ~Bpv7BlockConfidentialityBlock(); //a destructor: ~X()
    Bpv7BlockConfidentialityBlock(const Bpv7BlockConfidentialityBlock& o) = delete; //a copy constructor: X(const X&)
    Bpv7BlockConfidentialityBlock(Bpv7BlockConfidentialityBlock&& o); //a move constructor: X(X&&)
    Bpv7BlockConfidentialityBlock& operator=(const Bpv7BlockConfidentialityBlock& o) = delete; //a copy assignment: operator=(const X&)
    Bpv7BlockConfidentialityBlock& operator=(Bpv7BlockConfidentialityBlock&& o); //a move assignment: operator=(X&&)
    bool operator==(const Bpv7BlockConfidentialityBlock & o) const; //operator ==
    bool operator!=(const Bpv7BlockConfidentialityBlock & o) const; //operator !=
    virtual void SetZero();

    bool AddOrUpdateSecurityParameterAesVariant(COSE_ALGORITHMS alg);
    COSE_ALGORITHMS GetSecurityParameterAesVariant(bool & success) const;
    bool AddSecurityParameterScope(BPSEC_BCB_AES_GCM_AAD_SCOPE_MASKS scope);
    bool IsSecurityParameterScopePresentAndSet(BPSEC_BCB_AES_GCM_AAD_SCOPE_MASKS scope) const;
    std::vector<uint8_t> * AddAndGetAesWrappedKeyPtr();
    std::vector<uint8_t> * AddAndGetInitializationVectorPtr();
    std::vector<uint8_t> * AppendAndGetPayloadAuthenticationTagPtr();
    std::vector<std::vector<uint8_t>*> GetAllPayloadAuthenticationTagPtrs();
private:
    std::vector<uint8_t> * Private_AddAndGetByteStringParamPtr(BPSEC_BCB_AES_GCM_AAD_SECURITY_PARAMETERS parameter);
};


#endif //BPV7_H
