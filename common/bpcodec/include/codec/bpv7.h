#ifndef BPV7_H
#define BPV7_H 1
#include <cstdint>
#include <cstddef>
#include "Cbhe.h"
#include "TimestampUtil.h"
#include "codec/PrimaryBlock.h"

#define BPV7_CRC_TYPE_NONE        0
#define BPV7_CRC_TYPE_CRC16_X25   1
#define BPV7_CRC_TYPE_CRC32C      2


#define BPV7_BUNDLEFLAG_ISFRAGMENT      (0x0001)
#define BPV7_BUNDLEFLAG_ADMINRECORD     (0x0002)
#define BPV7_BUNDLEFLAG_NOFRAGMENT      (0x0004)
#define BPV7_BUNDLEFLAG_USER_APP_ACK_REQUESTED      (0x0020)
#define BPV7_BUNDLEFLAG_STATUSTIME_REQUESTED      (0x0040)
#define BPV7_BUNDLEFLAG_RECEPTION_STATUS_REPORTS_REQUESTED    (0x4000)
#define BPV7_BUNDLEFLAG_FORWARDING_STATUS_REPORTS_REQUESTED    (0x10000)
#define BPV7_BUNDLEFLAG_DELIVERY_STATUS_REPORTS_REQUESTED    (0x20000)
#define BPV7_BUNDLEFLAG_DELETION_STATUS_REPORTS_REQUESTED    (0x40000)


#define BPV7_BLOCKFLAG_MUST_BE_REPLICATED   (0x01)
#define BPV7_BLOCKFLAG_STATUS_REPORT_REQUESTED_IF_BLOCK_CANT_BE_PROCESSED   (0x02)
#define BPV7_BLOCKFLAG_DELETE_BUNDLE_IF_BLOCK_CANT_BE_PROCESSED   (0x04)
#define BPV7_BLOCKFLAG_REMOVE_BLOCK_IF_IT_CANT_BE_PROCESSED   (0x10)



// https://www.iana.org/assignments/bundle/bundle.xhtml#block-types
#define BPV7_BLOCKTYPE_PAYLOAD                     1U
#define BPV7_BLOCKTYPE_PREVIOUS_NODE               6U
#define BPV7_BLOCKTYPE_BUNDLE_AGE                  7U
#define BPV7_BLOCKTYPE_HOP_COUNT                   10U
#define BPV7_BLOCKTYPE_BLOCK_INTEGRITY             11U
#define BPV7_BLOCKTYPE_BLOCK_CONFIDENTIALITY       12U

//abstract security block values
#define BPV7_ASB_VALUE_UNIT_TEST 10000U

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

    uint64_t m_bundleProcessingControlFlags;
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
    uint64_t m_blockProcessingControlFlags;
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
struct Bpv7AbstractSecurityBlockValueUnitTest : public Bpv7AbstractSecurityBlockValueBase {
    virtual uint64_t SerializeBpv7(uint8_t * serialization);
    virtual uint64_t GetSerializationSize() const;
    virtual bool DeserializeBpv7(uint8_t * serialization, uint64_t & numBytesTakenToDecode, uint64_t bufferSize);
    virtual bool IsEqual(const Bpv7AbstractSecurityBlockValueBase * otherPtr) const;

    uint64_t m_unitTestValue;
};

struct Bpv7AbstractSecurityBlock : public Bpv7CanonicalBlock {

    typedef std::vector<uint64_t> security_targets_t;
    typedef uint64_t cipher_suite_id_t;
    typedef uint8_t cipher_suite_flags_t;
    //generic typedefs for cipher suite parameters and security results
    typedef uint64_t id_t;
    typedef std::unique_ptr<Bpv7AbstractSecurityBlockValueBase> value_ptr_t;
    typedef std::pair<id_t, value_ptr_t> id_value_pair_t;
    typedef std::vector<id_value_pair_t> id_value_pairs_vec_t;
    //cipher suite parameters:
    typedef id_t parameter_id_t;
    typedef value_ptr_t parameter_value_t;
    typedef id_value_pair_t cipher_suite_parameter_t;
    typedef id_value_pairs_vec_t cipher_suite_parameters_t;
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
    bool IsSecuritySourcePresent() const;
    void SetSecuritySourcePresent();
    void ClearSecuritySourcePresent();
    bool IsCipherSuiteParametersPresent() const;
    void SetCipherSuiteParametersPresent();
    void ClearCipherSuiteParametersPresent();

    static uint64_t SerializeIdValuePairsVecBpv7(uint8_t * serialization, const id_value_pairs_vec_t & idValuePairsVec);
    static uint64_t IdValuePairsVecBpv7SerializationSize(const id_value_pairs_vec_t & idValuePairsVec);
    static bool DeserializeIdValuePairsVecBpv7(uint8_t * serialization, uint64_t & numBytesTakenToDecode, uint64_t bufferSize, id_value_pairs_vec_t & idValuePairsVec, const uint64_t maxElements);
    static bool DeserializeIdValuePairBpv7(uint8_t * serialization, uint64_t & numBytesTakenToDecode, uint64_t bufferSize, id_value_pair_t & idValuePair);
    static bool IsEqual(const id_value_pairs_vec_t & pVec1, const id_value_pairs_vec_t & pVec2);

    security_targets_t m_securityTargets;
    cipher_suite_id_t m_cipherSuiteId;
    cipher_suite_flags_t m_cipherSuiteFlags;
    cbhe_eid_t m_securitySourceOptional;
    cipher_suite_parameters_t m_cipherSuiteParametersOptional;
    security_results_t m_securityResults;


    
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
};


#endif //BPV7_H
