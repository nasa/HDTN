/**
 * @file bpv6.h
 * @author  Brian Tomko <brian.j.tomko@nasa.gov>
 * @author  Gilbert Clark
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
 * The bpv6.h file defines all of the classes used for Bundle Protocol Version 6.
 */

#ifndef BPV6_H
#define BPV6_H

#include <cstdint>
#include <cstddef>
#include "Cbhe.h"
#include "TimestampUtil.h"
#include "codec/PrimaryBlock.h"
#include "EnumAsFlagsMacro.h"
#include <array>
#include "FragmentSet.h"
#include "bpcodec_export.h"
#ifndef CLASS_VISIBILITY_BPCODEC
#  ifdef _WIN32
#    define CLASS_VISIBILITY_BPCODEC
#  else
#    define CLASS_VISIBILITY_BPCODEC BPCODEC_EXPORT
#  endif
#endif


// (1-byte version) + (1-byte sdnv block length) + (1-byte sdnv zero dictionary length) + (up to 14 10-byte sdnvs) + (32 bytes hardware accelerated SDNV overflow instructions) 
#define CBHE_BPV6_MINIMUM_SAFE_PRIMARY_HEADER_ENCODE_SIZE (1 + 1 + 1 + (14*10) + 32)

// (1-byte block type) + (2 10-byte sdnvs) + (32 bytes hardware accelerated SDNV overflow instructions) 
#define BPV6_MINIMUM_SAFE_CANONICAL_HEADER_ENCODE_SIZE (1 + (2*10) + 32)

// (1-byte block type) + (2 10-byte sdnvs) + primary
#define CBHE_BPV6_MINIMUM_SAFE_PRIMARY_PLUS_CANONICAL_HEADER_ENCODE_SIZE (1 + (2*10) + CBHE_BPV6_MINIMUM_SAFE_PRIMARY_HEADER_ENCODE_SIZE)

#define BPV6_CCSDS_VERSION        (6)
#define BPV6_5050_TIME_OFFSET     (946684800)

#define bpv6_unix_to_5050(time)          (time - BPV6_5050_TIME_OFFSET)
#define bpv6_5050_to_unix(time)          (time + BPV6_5050_TIME_OFFSET)

enum class BPV6_PRIORITY : uint64_t {
    BULK = 0,
    NORMAL = 1,
    EXPEDITED = 2
};
MAKE_ENUM_SUPPORT_OSTREAM_OPERATOR(BPV6_PRIORITY);

enum class BPV6_BUNDLEFLAG : uint64_t {
    NO_FLAGS_SET                          = 0,
    ISFRAGMENT                            = 1 << 0,
    ADMINRECORD                           = 1 << 1,
    NOFRAGMENT                            = 1 << 2,
    CUSTODY_REQUESTED                     = 1 << 3,
    SINGLETON                             = 1 << 4,
    USER_APP_ACK_REQUESTED                = 1 << 5,
    PRIORITY_BULK                         = (static_cast<uint64_t>(BPV6_PRIORITY::BULK)) << 7,
    PRIORITY_NORMAL                       = (static_cast<uint64_t>(BPV6_PRIORITY::NORMAL)) << 7,
    PRIORITY_EXPEDITED                    = (static_cast<uint64_t>(BPV6_PRIORITY::EXPEDITED)) << 7,
    PRIORITY_BIT_MASK                     = 3 << 7,
    RECEPTION_STATUS_REPORTS_REQUESTED    = 1 << 14,
    CUSTODY_STATUS_REPORTS_REQUESTED      = 1 << 15,
    FORWARDING_STATUS_REPORTS_REQUESTED   = 1 << 16,
    DELIVERY_STATUS_REPORTS_REQUESTED     = 1 << 17,
    DELETION_STATUS_REPORTS_REQUESTED     = 1 << 18
};
MAKE_ENUM_SUPPORT_FLAG_OPERATORS(BPV6_BUNDLEFLAG);
MAKE_ENUM_SUPPORT_OSTREAM_OPERATOR(BPV6_BUNDLEFLAG);

//#define bpv6_bundle_set_priority(flags)  ((uint32_t)((flags & 0x000003) << 7))
//#define bpv6_bundle_get_priority(flags)  ((BPV6_PRIORITY)((flags & 0x000180) >> 7))
BOOST_FORCEINLINE BPV6_PRIORITY GetPriorityFromFlags(BPV6_BUNDLEFLAG flags) {
    return static_cast<BPV6_PRIORITY>(((static_cast<std::underlying_type<BPV6_BUNDLEFLAG>::type>(flags)) >> 7) & 3);
}


/**
 * Structure that contains information necessary for an RFC5050-compatible primary block
 */
struct CLASS_VISIBILITY_BPCODEC Bpv6CbhePrimaryBlock : public PrimaryBlock {
    BPV6_BUNDLEFLAG m_bundleProcessingControlFlags;
    uint64_t m_blockLength;
    cbhe_eid_t m_destinationEid;
    cbhe_eid_t m_sourceNodeId;
    cbhe_eid_t m_reportToEid;
    cbhe_eid_t m_custodianEid;
    TimestampUtil::bpv6_creation_timestamp_t m_creationTimestamp;
    uint64_t m_lifetimeSeconds;
    uint64_t m_tmpDictionaryLengthIgnoredAndAssumedZero; //Used only by sdnv decode operations as temporary variable to preserve sdnv encoded order. Class members ignore (treat as padding bytes).
    uint64_t m_fragmentOffset;
    uint64_t m_totalApplicationDataUnitLength;

    

    BPCODEC_EXPORT Bpv6CbhePrimaryBlock(); //a default constructor: X()
    BPCODEC_EXPORT ~Bpv6CbhePrimaryBlock(); //a destructor: ~X()
    BPCODEC_EXPORT Bpv6CbhePrimaryBlock(const Bpv6CbhePrimaryBlock& o); //a copy constructor: X(const X&)
    BPCODEC_EXPORT Bpv6CbhePrimaryBlock(Bpv6CbhePrimaryBlock&& o); //a move constructor: X(X&&)
    BPCODEC_EXPORT Bpv6CbhePrimaryBlock& operator=(const Bpv6CbhePrimaryBlock& o); //a copy assignment: operator=(const X&)
    BPCODEC_EXPORT Bpv6CbhePrimaryBlock& operator=(Bpv6CbhePrimaryBlock&& o); //a move assignment: operator=(X&&)
    BPCODEC_EXPORT bool operator==(const Bpv6CbhePrimaryBlock & o) const; //operator ==
    BPCODEC_EXPORT bool operator!=(const Bpv6CbhePrimaryBlock & o) const; //operator !=
    BPCODEC_EXPORT void SetZero();
    BPCODEC_EXPORT uint64_t SerializeBpv6(uint8_t * serialization); //not const as it needs to modify m_blockLength
    BPCODEC_EXPORT uint64_t GetSerializationSize() const;
    BPCODEC_EXPORT bool DeserializeBpv6(const uint8_t * serialization, uint64_t & numBytesTakenToDecode, uint64_t bufferSize);
    BPCODEC_EXPORT uint64_t GetSecondsSinceCreate() const;
    BPCODEC_EXPORT bool HasFlagSet(BPV6_BUNDLEFLAG flag) const;
    
    /**
     * Dumps a primary block to stdout in a human-readable way
     *
     * @param primary Primary block to print
     */
    BPCODEC_EXPORT void bpv6_primary_block_print() const;

    
    


    BPCODEC_EXPORT virtual bool HasCustodyFlagSet() const override;
    BPCODEC_EXPORT virtual bool HasFragmentationFlagSet() const override;
    BPCODEC_EXPORT virtual cbhe_bundle_uuid_t GetCbheBundleUuidFromPrimary(uint64_t payloadSizeBytes) const override;
    BPCODEC_EXPORT virtual cbhe_bundle_uuid_nofragment_t GetCbheBundleUuidNoFragmentFromPrimary() const override;
    BPCODEC_EXPORT virtual cbhe_eid_t GetFinalDestinationEid() const override;
    BPCODEC_EXPORT virtual cbhe_eid_t GetSourceEid() const override;
    BPCODEC_EXPORT virtual uint8_t GetPriority() const override;
    BPCODEC_EXPORT virtual uint64_t GetExpirationSeconds() const override;
    BPCODEC_EXPORT virtual uint64_t GetSequenceForSecondsScale() const override;
    BPCODEC_EXPORT virtual uint64_t GetExpirationMilliseconds() const override;
    BPCODEC_EXPORT virtual uint64_t GetSequenceForMillisecondsScale() const override;
};

// https://www.iana.org/assignments/bundle/bundle.xhtml#block-types
enum class BPV6_BLOCK_TYPE_CODE : uint8_t {
    PRIMARY_IMPLICIT_ZERO             = 0,
    PAYLOAD                           = 1,
    BUNDLE_AUTHENTICATION             = 2,
    PAYLOAD_INTEGRITY                 = 3,
    PAYLOAD_CONFIDENTIALITY           = 4,
    PREVIOUS_HOP_INSERTION            = 5,
    UNUSED_6                          = 6,
    UNUSED_7                          = 7,
    METADATA_EXTENSION                = 8,
    EXTENSION_SECURITY                = 9,
    CUSTODY_TRANSFER_ENHANCEMENT      = 10,
    UNUSED_11                         = 11,
    UNUSED_12                         = 12,
    BPLIB_BIB                         = 13,
    BUNDLE_AGE                        = 20,
    RESERVED_MAX_BLOCK_TYPES          = 21 //for sizing lookup tables
};
MAKE_ENUM_SUPPORT_OSTREAM_OPERATOR(BPV6_BLOCK_TYPE_CODE);

enum class BPV6_BLOCKFLAG : uint64_t {
    NO_FLAGS_SET                                        = 0,
    MUST_BE_REPLICATED_IN_EVERY_FRAGMENT                = 1 << 0,
    STATUS_REPORT_REQUESTED_IF_BLOCK_CANT_BE_PROCESSED  = 1 << 1,
    DELETE_BUNDLE_IF_BLOCK_CANT_BE_PROCESSED            = 1 << 2,
    IS_LAST_BLOCK                                       = 1 << 3,
    DISCARD_BLOCK_IF_IT_CANT_BE_PROCESSED               = 1 << 4,
    BLOCK_WAS_FORWARDED_WITHOUT_BEING_PROCESSED         = 1 << 5,
    BLOCK_CONTAINS_AN_EID_REFERENCE_FIELD               = 1 << 6,
};
MAKE_ENUM_SUPPORT_FLAG_OPERATORS(BPV6_BLOCKFLAG);
MAKE_ENUM_SUPPORT_OSTREAM_OPERATOR(BPV6_BLOCKFLAG);

/**
 * Structure that contains information necessary for a 5050-compatible canonical block
 */
struct CLASS_VISIBILITY_BPCODEC Bpv6CanonicalBlock {
    BPV6_BLOCKFLAG m_blockProcessingControlFlags;
    uint64_t m_blockTypeSpecificDataLength;
    uint8_t * m_blockTypeSpecificDataPtr; //if NULL, data won't be copied (just allocated)
    BPV6_BLOCK_TYPE_CODE m_blockTypeCode; //should be at beginning but here do to better packing

    BPCODEC_EXPORT Bpv6CanonicalBlock(); //a default constructor: X()
    BPCODEC_EXPORT virtual ~Bpv6CanonicalBlock(); //a destructor: ~X()
    BPCODEC_EXPORT Bpv6CanonicalBlock(const Bpv6CanonicalBlock& o); //a copy constructor: X(const X&)
    BPCODEC_EXPORT Bpv6CanonicalBlock(Bpv6CanonicalBlock&& o); //a move constructor: X(X&&)
    BPCODEC_EXPORT Bpv6CanonicalBlock& operator=(const Bpv6CanonicalBlock& o); //a copy assignment: operator=(const X&)
    BPCODEC_EXPORT Bpv6CanonicalBlock& operator=(Bpv6CanonicalBlock&& o); //a move assignment: operator=(X&&)
    BPCODEC_EXPORT bool operator==(const Bpv6CanonicalBlock & o) const; //operator ==
    BPCODEC_EXPORT bool operator!=(const Bpv6CanonicalBlock & o) const; //operator !=
    BPCODEC_EXPORT virtual void SetZero();
    BPCODEC_EXPORT virtual uint64_t SerializeBpv6(uint8_t * serialization); //modifies m_blockTypeSpecificDataPtr to serialized location
    BPCODEC_EXPORT uint64_t GetSerializationSize() const;
    BPCODEC_EXPORT virtual uint64_t GetCanonicalBlockTypeSpecificDataSerializationSize() const;
    BPCODEC_EXPORT static bool DeserializeBpv6(std::unique_ptr<Bpv6CanonicalBlock> & canonicalPtr, const uint8_t * serialization,
        uint64_t & numBytesTakenToDecode, uint64_t bufferSize, const bool isAdminRecord,
        std::unique_ptr<Bpv6CanonicalBlock>* blockNumberToRecycledCanonicalBlockArray);
    BPCODEC_EXPORT virtual bool Virtual_DeserializeExtensionBlockDataBpv6();
    //virtual bool Virtual_DeserializeExtensionBlockDataBpv7();
    /**
     * Dumps a canonical block to stdout in a human-readable fashion
     *
     * @param block Canonical block which should be displayed
     */
    BPCODEC_EXPORT void bpv6_canonical_block_print() const;

    /**
     * Print just the block flags for a generic canonical block
     *
     * @param block The canonical block with flags to be displayed
     */
    BPCODEC_EXPORT void bpv6_block_flags_print() const;

};


struct CLASS_VISIBILITY_BPCODEC Bpv6CustodyTransferEnhancementBlock : public Bpv6CanonicalBlock {
    
public:
    static constexpr unsigned int CBHE_MAX_SERIALIZATION_SIZE =
        1 + //block type
        10 + //block flags sdnv +
        1 + //block length (1-byte-min-sized-sdnv) +
        10 + //custody id sdnv +
        45; //length of "ipn:18446744073709551615.18446744073709551615" (note 45 > 32 so sdnv hardware acceleration overwrite is satisfied)

    uint64_t m_custodyId;
    std::string m_ctebCreatorCustodianEidString;

public:
    BPCODEC_EXPORT Bpv6CustodyTransferEnhancementBlock(); //a default constructor: X()
    BPCODEC_EXPORT virtual ~Bpv6CustodyTransferEnhancementBlock() override; //a destructor: ~X()
    BPCODEC_EXPORT Bpv6CustodyTransferEnhancementBlock(const Bpv6CustodyTransferEnhancementBlock& o); //a copy constructor: X(const X&)
    BPCODEC_EXPORT Bpv6CustodyTransferEnhancementBlock(Bpv6CustodyTransferEnhancementBlock&& o); //a move constructor: X(X&&)
    BPCODEC_EXPORT Bpv6CustodyTransferEnhancementBlock& operator=(const Bpv6CustodyTransferEnhancementBlock& o); //a copy assignment: operator=(const X&)
    BPCODEC_EXPORT Bpv6CustodyTransferEnhancementBlock& operator=(Bpv6CustodyTransferEnhancementBlock&& o); //a move assignment: operator=(X&&)
    BPCODEC_EXPORT bool operator==(const Bpv6CustodyTransferEnhancementBlock & o) const; //operator ==
    BPCODEC_EXPORT bool operator!=(const Bpv6CustodyTransferEnhancementBlock & o) const; //operator !=
    BPCODEC_EXPORT virtual void SetZero() override;
    BPCODEC_EXPORT virtual uint64_t SerializeBpv6(uint8_t * serialization) override; //modifies m_dataPtr to serialized location
    BPCODEC_EXPORT virtual uint64_t GetCanonicalBlockTypeSpecificDataSerializationSize() const override;
    BPCODEC_EXPORT virtual bool Virtual_DeserializeExtensionBlockDataBpv6() override;
};

//https://datatracker.ietf.org/doc/html/rfc6259
struct CLASS_VISIBILITY_BPCODEC Bpv6PreviousHopInsertionCanonicalBlock : public Bpv6CanonicalBlock {
    static constexpr uint64_t largestSerializedDataOnlySize =
        4 + //ipn\0
        20 + // 18446744073709551615
        1 + // :
        20 + // 18446744073709551615
        1; // \0

    BPCODEC_EXPORT Bpv6PreviousHopInsertionCanonicalBlock(); //a default constructor: X()
    BPCODEC_EXPORT virtual ~Bpv6PreviousHopInsertionCanonicalBlock() override; //a destructor: ~X()
    BPCODEC_EXPORT Bpv6PreviousHopInsertionCanonicalBlock(const Bpv6PreviousHopInsertionCanonicalBlock& o); //a copy constructor: X(const X&)
    BPCODEC_EXPORT Bpv6PreviousHopInsertionCanonicalBlock(Bpv6PreviousHopInsertionCanonicalBlock&& o); //a move constructor: X(X&&)
    BPCODEC_EXPORT Bpv6PreviousHopInsertionCanonicalBlock& operator=(const Bpv6PreviousHopInsertionCanonicalBlock& o); //a copy assignment: operator=(const X&)
    BPCODEC_EXPORT Bpv6PreviousHopInsertionCanonicalBlock& operator=(Bpv6PreviousHopInsertionCanonicalBlock&& o); //a move assignment: operator=(X&&)
    BPCODEC_EXPORT bool operator==(const Bpv6PreviousHopInsertionCanonicalBlock & o) const; //operator ==
    BPCODEC_EXPORT bool operator!=(const Bpv6PreviousHopInsertionCanonicalBlock & o) const; //operator !=
    BPCODEC_EXPORT virtual void SetZero() override;
    BPCODEC_EXPORT virtual uint64_t SerializeBpv6(uint8_t * serialization) override; //modifies m_dataPtr to serialized location
    BPCODEC_EXPORT virtual uint64_t GetCanonicalBlockTypeSpecificDataSerializationSize() const override;
    BPCODEC_EXPORT virtual bool Virtual_DeserializeExtensionBlockDataBpv6() override;

    cbhe_eid_t m_previousNode;
};

//https://datatracker.ietf.org/doc/html/draft-irtf-dtnrg-bundle-age-block-01
struct CLASS_VISIBILITY_BPCODEC Bpv6BundleAgeCanonicalBlock : public Bpv6CanonicalBlock {
    static constexpr uint64_t largestSerializedDataOnlySize = 10; //sdnv bundle age

    BPCODEC_EXPORT Bpv6BundleAgeCanonicalBlock(); //a default constructor: X()
    BPCODEC_EXPORT virtual ~Bpv6BundleAgeCanonicalBlock() override; //a destructor: ~X()
    BPCODEC_EXPORT Bpv6BundleAgeCanonicalBlock(const Bpv6BundleAgeCanonicalBlock& o); //a copy constructor: X(const X&)
    BPCODEC_EXPORT Bpv6BundleAgeCanonicalBlock(Bpv6BundleAgeCanonicalBlock&& o); //a move constructor: X(X&&)
    BPCODEC_EXPORT Bpv6BundleAgeCanonicalBlock& operator=(const Bpv6BundleAgeCanonicalBlock& o); //a copy assignment: operator=(const X&)
    BPCODEC_EXPORT Bpv6BundleAgeCanonicalBlock& operator=(Bpv6BundleAgeCanonicalBlock&& o); //a move assignment: operator=(X&&)
    BPCODEC_EXPORT bool operator==(const Bpv6BundleAgeCanonicalBlock & o) const; //operator ==
    BPCODEC_EXPORT bool operator!=(const Bpv6BundleAgeCanonicalBlock & o) const; //operator !=
    BPCODEC_EXPORT virtual void SetZero() override;
    BPCODEC_EXPORT virtual uint64_t SerializeBpv6(uint8_t * serialization) override; //modifies m_dataPtr to serialized location
    BPCODEC_EXPORT virtual uint64_t GetCanonicalBlockTypeSpecificDataSerializationSize() const override;
    BPCODEC_EXPORT virtual bool Virtual_DeserializeExtensionBlockDataBpv6() override;

    uint64_t m_bundleAgeMicroseconds;
};

enum class BPV6_METADATA_TYPE_CODE : uint64_t {
    UNDEFINED_ZERO = 0,
    URI = 1
};
MAKE_ENUM_SUPPORT_OSTREAM_OPERATOR(BPV6_METADATA_TYPE_CODE);

struct CLASS_VISIBILITY_BPCODEC Bpv6MetadataContentBase {
    BPCODEC_EXPORT virtual ~Bpv6MetadataContentBase() = 0; // Pure virtual destructor
    virtual uint64_t SerializeBpv6(uint8_t * serialization, uint64_t bufferSize) const = 0;
    virtual uint64_t GetSerializationSize() const = 0;
    virtual bool DeserializeBpv6(const uint8_t * serialization, uint64_t & numBytesTakenToDecode, uint64_t bufferSize) = 0;
    virtual bool IsEqual(const Bpv6MetadataContentBase * otherPtr) const = 0;
};

class CLASS_VISIBILITY_BPCODEC Bpv6MetadataContentUriList : public Bpv6MetadataContentBase {
public:
    std::vector<cbhe_eid_t> m_uriArray;

public:
    BPCODEC_EXPORT Bpv6MetadataContentUriList(); //a default constructor: X()
    BPCODEC_EXPORT virtual ~Bpv6MetadataContentUriList() override; //a destructor: ~X()
    BPCODEC_EXPORT Bpv6MetadataContentUriList(const Bpv6MetadataContentUriList& o); //a copy constructor: X(const X&)
    BPCODEC_EXPORT Bpv6MetadataContentUriList(Bpv6MetadataContentUriList&& o); //a move constructor: X(X&&)
    BPCODEC_EXPORT Bpv6MetadataContentUriList& operator=(const Bpv6MetadataContentUriList& o); //a copy assignment: operator=(const X&)
    BPCODEC_EXPORT Bpv6MetadataContentUriList& operator=(Bpv6MetadataContentUriList&& o); //a move assignment: operator=(X&&)
    BPCODEC_EXPORT bool operator==(const Bpv6MetadataContentUriList & o) const;
    BPCODEC_EXPORT bool operator!=(const Bpv6MetadataContentUriList & o) const;

    BPCODEC_EXPORT virtual uint64_t SerializeBpv6(uint8_t * serialization, uint64_t bufferSize) const override;
    BPCODEC_EXPORT virtual uint64_t GetSerializationSize() const override;
    BPCODEC_EXPORT virtual bool DeserializeBpv6(const uint8_t * serialization, uint64_t & numBytesTakenToDecode, uint64_t bufferSize) override;
    BPCODEC_EXPORT virtual bool IsEqual(const Bpv6MetadataContentBase * otherPtr) const override;

    BPCODEC_EXPORT void Reset();
};

class CLASS_VISIBILITY_BPCODEC Bpv6MetadataContentGeneric : public Bpv6MetadataContentBase {
public:
    std::vector<uint8_t> m_genericRawMetadata;

public:
    BPCODEC_EXPORT Bpv6MetadataContentGeneric(); //a default constructor: X()
    BPCODEC_EXPORT virtual ~Bpv6MetadataContentGeneric() override; //a destructor: ~X()
    BPCODEC_EXPORT Bpv6MetadataContentGeneric(const Bpv6MetadataContentGeneric& o); //a copy constructor: X(const X&)
    BPCODEC_EXPORT Bpv6MetadataContentGeneric(Bpv6MetadataContentGeneric&& o); //a move constructor: X(X&&)
    BPCODEC_EXPORT Bpv6MetadataContentGeneric& operator=(const Bpv6MetadataContentGeneric& o); //a copy assignment: operator=(const X&)
    BPCODEC_EXPORT Bpv6MetadataContentGeneric& operator=(Bpv6MetadataContentGeneric&& o); //a move assignment: operator=(X&&)
    BPCODEC_EXPORT bool operator==(const Bpv6MetadataContentGeneric & o) const;
    BPCODEC_EXPORT bool operator!=(const Bpv6MetadataContentGeneric & o) const;

    BPCODEC_EXPORT virtual uint64_t SerializeBpv6(uint8_t * serialization, uint64_t bufferSize) const override;
    BPCODEC_EXPORT virtual uint64_t GetSerializationSize() const override;
    BPCODEC_EXPORT virtual bool DeserializeBpv6(const uint8_t * serialization, uint64_t & numBytesTakenToDecode, uint64_t bufferSize) override;
    BPCODEC_EXPORT virtual bool IsEqual(const Bpv6MetadataContentBase * otherPtr) const override;

    BPCODEC_EXPORT void Reset();
};

//https://datatracker.ietf.org/doc/html/rfc6258
struct CLASS_VISIBILITY_BPCODEC Bpv6MetadataCanonicalBlock : public Bpv6CanonicalBlock {

    BPCODEC_EXPORT Bpv6MetadataCanonicalBlock(); //a default constructor: X()
    BPCODEC_EXPORT virtual ~Bpv6MetadataCanonicalBlock() override; //a destructor: ~X()
    BPCODEC_EXPORT Bpv6MetadataCanonicalBlock(const Bpv6MetadataCanonicalBlock& o) = delete; //a copy constructor: X(const X&)
    BPCODEC_EXPORT Bpv6MetadataCanonicalBlock(Bpv6MetadataCanonicalBlock&& o); //a move constructor: X(X&&)
    BPCODEC_EXPORT Bpv6MetadataCanonicalBlock& operator=(const Bpv6MetadataCanonicalBlock& o) = delete; //a copy assignment: operator=(const X&)
    BPCODEC_EXPORT Bpv6MetadataCanonicalBlock& operator=(Bpv6MetadataCanonicalBlock&& o); //a move assignment: operator=(X&&)
    BPCODEC_EXPORT bool operator==(const Bpv6MetadataCanonicalBlock & o) const; //operator ==
    BPCODEC_EXPORT bool operator!=(const Bpv6MetadataCanonicalBlock & o) const; //operator !=
    BPCODEC_EXPORT virtual void SetZero() override;
    BPCODEC_EXPORT virtual uint64_t SerializeBpv6(uint8_t * serialization) override; //modifies m_dataPtr to serialized location
    BPCODEC_EXPORT virtual uint64_t GetCanonicalBlockTypeSpecificDataSerializationSize() const override;
    BPCODEC_EXPORT virtual bool Virtual_DeserializeExtensionBlockDataBpv6() override;

    BPV6_METADATA_TYPE_CODE m_metadataTypeCode;
    std::unique_ptr<Bpv6MetadataContentBase> m_metadataContentPtr;
};

//Administrative record types
enum class BPV6_ADMINISTRATIVE_RECORD_TYPE_CODE : uint8_t {
    UNUSED_ZERO              = 0,
    BUNDLE_STATUS_REPORT     = 1,
    CUSTODY_SIGNAL           = 2,
    AGGREGATE_CUSTODY_SIGNAL = 4,
    ENCAPSULATED_BUNDLE      = 7,
    SAGA_MESSAGE             = 42
};
MAKE_ENUM_SUPPORT_OSTREAM_OPERATOR(BPV6_ADMINISTRATIVE_RECORD_TYPE_CODE);

//Administrative record flags
enum class BPV6_ADMINISTRATIVE_RECORD_FLAGS : uint8_t {
    BUNDLE_IS_A_FRAGMENT = 1 //00000001
};
MAKE_ENUM_SUPPORT_FLAG_OPERATORS(BPV6_ADMINISTRATIVE_RECORD_FLAGS);
MAKE_ENUM_SUPPORT_OSTREAM_OPERATOR(BPV6_ADMINISTRATIVE_RECORD_FLAGS);


enum class BPV6_BUNDLE_STATUS_REPORT_STATUS_FLAGS : uint8_t {
    NO_FLAGS_SET                                 = 0,
    REPORTING_NODE_RECEIVED_BUNDLE               = (1 << 0),
    REPORTING_NODE_ACCEPTED_CUSTODY_OF_BUNDLE    = (1 << 1),
    REPORTING_NODE_FORWARDED_BUNDLE              = (1 << 2),
    REPORTING_NODE_DELIVERED_BUNDLE              = (1 << 3),
    REPORTING_NODE_DELETED_BUNDLE                = (1 << 4),
};
MAKE_ENUM_SUPPORT_FLAG_OPERATORS(BPV6_BUNDLE_STATUS_REPORT_STATUS_FLAGS);
MAKE_ENUM_SUPPORT_OSTREAM_OPERATOR(BPV6_BUNDLE_STATUS_REPORT_STATUS_FLAGS);

enum class BPV6_BUNDLE_STATUS_REPORT_REASON_CODES : uint8_t {
    NO_ADDITIONAL_INFORMATION                   = 0,
    LIFETIME_EXPIRED                            = 1,
    FORWARDED_OVER_UNIDIRECTIONAL_LINK          = 2,
    TRANSMISSION_CANCELLED                      = 3,
    DEPLETED_STORAGE                            = 4,
    DESTINATION_ENDPOINT_ID_UNINTELLIGIBLE      = 5,
    NO_KNOWN_ROUTE_TO_DESTINATION_FROM_HERE     = 6,
    NO_TIMELY_CONTACT_WITH_NEXT_NODE_ON_ROUTE   = 7,
    BLOCK_UNINTELLIGIBLE                        = 8
};
MAKE_ENUM_SUPPORT_OSTREAM_OPERATOR(BPV6_BUNDLE_STATUS_REPORT_REASON_CODES);

enum class BPV6_CUSTODY_SIGNAL_REASON_CODES_7BIT : uint8_t {
    NO_ADDITIONAL_INFORMATION                   = 0,
    REDUNDANT_RECEPTION                         = 3,
    DEPLETED_STORAGE                            = 4,
    DESTINATION_ENDPOINT_ID_UNINTELLIGIBLE      = 5,
    NO_KNOWN_ROUTE_TO_DESTINATION_FROM_HERE     = 6,
    NO_TIMELY_CONTACT_WITH_NEXT_NODE_ON_ROUTE   = 7,
    BLOCK_UNINTELLIGIBLE                        = 8
};
MAKE_ENUM_SUPPORT_OSTREAM_OPERATOR(BPV6_CUSTODY_SIGNAL_REASON_CODES_7BIT);


struct CLASS_VISIBILITY_BPCODEC Bpv6AdministrativeRecordContentBase {
    BPCODEC_EXPORT virtual ~Bpv6AdministrativeRecordContentBase() = 0; // Pure virtual destructor
    virtual uint64_t SerializeBpv6(uint8_t * serialization, uint64_t bufferSize) const = 0;
    virtual uint64_t GetSerializationSize() const = 0;
    virtual bool DeserializeBpv6(const uint8_t * serialization, uint64_t & numBytesTakenToDecode, uint64_t bufferSize) = 0;
    virtual bool IsEqual(const Bpv6AdministrativeRecordContentBase * otherPtr) const = 0;
};

class CLASS_VISIBILITY_BPCODEC Bpv6AdministrativeRecordContentBundleStatusReport : public Bpv6AdministrativeRecordContentBase {


public:
    static constexpr unsigned int CBHE_MAX_SERIALIZATION_SIZE =
        3 + //admin flags + status flags + reason code
        10 + //fragmentOffsetSdnv.length +
        10 + //fragmentLengthSdnv.length +
        10 + //receiptTimeSecondsSdnv.length +
        5 + //receiptTimeNanosecSdnv.length +
        10 + //custodyTimeSecondsSdnv.length +
        5 + //custodyTimeNanosecSdnv.length +
        10 + //forwardTimeSecondsSdnv.length +
        5 + //forwardTimeNanosecSdnv.length +
        10 + //deliveryTimeSecondsSdnv.length +
        5 + //deliveryTimeNanosecSdnv.length +
        10 + //deletionTimeSecondsSdnv.length +
        5 + //deletionTimeNanosecSdnv.length +
        10 + //creationTimeSecondsSdnv.length +
        10 + //creationTimeCountSdnv.length +
        1 + //eidLengthSdnv.length +
        45; //length of "ipn:18446744073709551615.18446744073709551615" (note 45 > 32 so sdnv hardware acceleration overwrite is satisfied)
    BPV6_BUNDLE_STATUS_REPORT_STATUS_FLAGS m_statusFlags;
    BPV6_BUNDLE_STATUS_REPORT_REASON_CODES m_reasonCode;
    bool m_isFragment;
    uint64_t m_fragmentOffsetIfPresent;
    uint64_t m_fragmentLengthIfPresent;

    TimestampUtil::dtn_time_t m_timeOfReceiptOfBundle;
    TimestampUtil::dtn_time_t m_timeOfCustodyAcceptanceOfBundle;
    TimestampUtil::dtn_time_t m_timeOfForwardingOfBundle;
    TimestampUtil::dtn_time_t m_timeOfDeliveryOfBundle;
    TimestampUtil::dtn_time_t m_timeOfDeletionOfBundle;

    //from primary block of subject bundle
    TimestampUtil::bpv6_creation_timestamp_t m_copyOfBundleCreationTimestamp;

    std::string m_bundleSourceEid;

public:
    BPCODEC_EXPORT Bpv6AdministrativeRecordContentBundleStatusReport(); //a default constructor: X()
    BPCODEC_EXPORT virtual ~Bpv6AdministrativeRecordContentBundleStatusReport() override; //a destructor: ~X()
    BPCODEC_EXPORT Bpv6AdministrativeRecordContentBundleStatusReport(const Bpv6AdministrativeRecordContentBundleStatusReport& o); //a copy constructor: X(const X&)
    BPCODEC_EXPORT Bpv6AdministrativeRecordContentBundleStatusReport(Bpv6AdministrativeRecordContentBundleStatusReport&& o); //a move constructor: X(X&&)
    BPCODEC_EXPORT Bpv6AdministrativeRecordContentBundleStatusReport& operator=(const Bpv6AdministrativeRecordContentBundleStatusReport& o); //a copy assignment: operator=(const X&)
    BPCODEC_EXPORT Bpv6AdministrativeRecordContentBundleStatusReport& operator=(Bpv6AdministrativeRecordContentBundleStatusReport&& o); //a move assignment: operator=(X&&)
    BPCODEC_EXPORT bool operator==(const Bpv6AdministrativeRecordContentBundleStatusReport & o) const;
    BPCODEC_EXPORT bool operator!=(const Bpv6AdministrativeRecordContentBundleStatusReport & o) const;

    BPCODEC_EXPORT virtual uint64_t SerializeBpv6(uint8_t * serialization, uint64_t bufferSize) const override;
    BPCODEC_EXPORT virtual uint64_t GetSerializationSize() const override;
    BPCODEC_EXPORT virtual bool DeserializeBpv6(const uint8_t * serialization, uint64_t & numBytesTakenToDecode, uint64_t bufferSize) override;
    BPCODEC_EXPORT virtual bool IsEqual(const Bpv6AdministrativeRecordContentBase * otherPtr) const override;

    BPCODEC_EXPORT void Reset();

    BPCODEC_EXPORT void SetTimeOfReceiptOfBundleAndStatusFlag(const TimestampUtil::dtn_time_t & dtnTime);
    BPCODEC_EXPORT void SetTimeOfCustodyAcceptanceOfBundleAndStatusFlag(const TimestampUtil::dtn_time_t & dtnTime);
    BPCODEC_EXPORT void SetTimeOfForwardingOfBundleAndStatusFlag(const TimestampUtil::dtn_time_t & dtnTime);
    BPCODEC_EXPORT void SetTimeOfDeliveryOfBundleAndStatusFlag(const TimestampUtil::dtn_time_t & dtnTime);
    BPCODEC_EXPORT void SetTimeOfDeletionOfBundleAndStatusFlag(const TimestampUtil::dtn_time_t & dtnTime);
    BPCODEC_EXPORT bool HasBundleStatusReportStatusFlagSet(const BPV6_BUNDLE_STATUS_REPORT_STATUS_FLAGS & flag) const;
};

class CLASS_VISIBILITY_BPCODEC Bpv6AdministrativeRecordContentCustodySignal : public Bpv6AdministrativeRecordContentBase {
    

public:
    static constexpr unsigned int CBHE_MAX_SERIALIZATION_SIZE =
        2 + //admin flags + (bit7 status flags |  bit 6..0 reason code)
        10 + //fragmentOffsetSdnv.length +
        10 + //fragmentLengthSdnv.length +
        10 + //signalTimeSecondsSdnv.length +
        5 + //signalTimeNanosecSdnv.length +
        10 + //creationTimeSecondsSdnv.length +
        10 + //creationTimeCountSdnv.length +
        1 + //eidLengthSdnv.length +
        45; //length of "ipn:18446744073709551615.18446744073709551615" (note 45 > 32 so sdnv hardware acceleration overwrite is satisfied)
private:
    uint8_t m_statusFlagsPlus7bitReasonCode;
public:
    
    bool m_isFragment;
    uint64_t m_fragmentOffsetIfPresent;
    uint64_t m_fragmentLengthIfPresent;

    TimestampUtil::dtn_time_t m_timeOfSignalGeneration;

    //from primary block of subject bundle
    TimestampUtil::bpv6_creation_timestamp_t m_copyOfBundleCreationTimestamp;

    std::string m_bundleSourceEid;

public:
    BPCODEC_EXPORT Bpv6AdministrativeRecordContentCustodySignal(); //a default constructor: X()
    BPCODEC_EXPORT virtual ~Bpv6AdministrativeRecordContentCustodySignal() override; //a destructor: ~X()
    BPCODEC_EXPORT Bpv6AdministrativeRecordContentCustodySignal(const Bpv6AdministrativeRecordContentCustodySignal& o); //a copy constructor: X(const X&)
    BPCODEC_EXPORT Bpv6AdministrativeRecordContentCustodySignal(Bpv6AdministrativeRecordContentCustodySignal&& o); //a move constructor: X(X&&)
    BPCODEC_EXPORT Bpv6AdministrativeRecordContentCustodySignal& operator=(const Bpv6AdministrativeRecordContentCustodySignal& o); //a copy assignment: operator=(const X&)
    BPCODEC_EXPORT Bpv6AdministrativeRecordContentCustodySignal& operator=(Bpv6AdministrativeRecordContentCustodySignal&& o); //a move assignment: operator=(X&&)
    BPCODEC_EXPORT bool operator==(const Bpv6AdministrativeRecordContentCustodySignal & o) const;
    BPCODEC_EXPORT bool operator!=(const Bpv6AdministrativeRecordContentCustodySignal & o) const;

    BPCODEC_EXPORT virtual uint64_t SerializeBpv6(uint8_t * serialization, uint64_t bufferSize) const override;
    BPCODEC_EXPORT virtual uint64_t GetSerializationSize() const override;
    BPCODEC_EXPORT virtual bool DeserializeBpv6(const uint8_t * serialization, uint64_t & numBytesTakenToDecode, uint64_t bufferSize) override;
    BPCODEC_EXPORT virtual bool IsEqual(const Bpv6AdministrativeRecordContentBase * otherPtr) const override;

    BPCODEC_EXPORT void Reset();

    BPCODEC_EXPORT void SetTimeOfSignalGeneration(const TimestampUtil::dtn_time_t & dtnTime);
    BPCODEC_EXPORT void SetCustodyTransferStatusAndReason(bool custodyTransferSucceeded, BPV6_CUSTODY_SIGNAL_REASON_CODES_7BIT reasonCode7bit);
    BPCODEC_EXPORT bool DidCustodyTransferSucceed() const;
    BPCODEC_EXPORT BPV6_CUSTODY_SIGNAL_REASON_CODES_7BIT GetReasonCode() const;
};

class CLASS_VISIBILITY_BPCODEC Bpv6AdministrativeRecordContentAggregateCustodySignal : public Bpv6AdministrativeRecordContentBase {
    
private:
    //The second field shall be a �Status� byte encoded in the same way as the status byte
    //for administrative records in RFC 5050, using the same reason codes
    uint8_t m_statusFlagsPlus7bitReasonCode;
public:
    FragmentSet::data_fragment_set_t m_custodyIdFills;

public:
    BPCODEC_EXPORT Bpv6AdministrativeRecordContentAggregateCustodySignal(); //a default constructor: X()
    BPCODEC_EXPORT virtual ~Bpv6AdministrativeRecordContentAggregateCustodySignal() override; //a destructor: ~X()
    BPCODEC_EXPORT Bpv6AdministrativeRecordContentAggregateCustodySignal(const Bpv6AdministrativeRecordContentAggregateCustodySignal& o); //a copy constructor: X(const X&)
    BPCODEC_EXPORT Bpv6AdministrativeRecordContentAggregateCustodySignal(Bpv6AdministrativeRecordContentAggregateCustodySignal&& o); //a move constructor: X(X&&)
    BPCODEC_EXPORT Bpv6AdministrativeRecordContentAggregateCustodySignal& operator=(const Bpv6AdministrativeRecordContentAggregateCustodySignal& o); //a copy assignment: operator=(const X&)
    BPCODEC_EXPORT Bpv6AdministrativeRecordContentAggregateCustodySignal& operator=(Bpv6AdministrativeRecordContentAggregateCustodySignal&& o); //a move assignment: operator=(X&&)
    BPCODEC_EXPORT bool operator==(const Bpv6AdministrativeRecordContentAggregateCustodySignal & o) const;
    BPCODEC_EXPORT bool operator!=(const Bpv6AdministrativeRecordContentAggregateCustodySignal & o) const;

    BPCODEC_EXPORT virtual uint64_t SerializeBpv6(uint8_t * serialization, uint64_t bufferSize) const override;
    BPCODEC_EXPORT virtual uint64_t GetSerializationSize() const override;
    BPCODEC_EXPORT virtual bool DeserializeBpv6(const uint8_t * serialization, uint64_t & numBytesTakenToDecode, uint64_t bufferSize) override;
    BPCODEC_EXPORT virtual bool IsEqual(const Bpv6AdministrativeRecordContentBase * otherPtr) const override;

    BPCODEC_EXPORT void Reset();

    
    BPCODEC_EXPORT void SetCustodyTransferStatusAndReason(bool custodyTransferSucceeded, BPV6_CUSTODY_SIGNAL_REASON_CODES_7BIT reasonCode7bit);
    BPCODEC_EXPORT bool DidCustodyTransferSucceed() const;
    BPCODEC_EXPORT BPV6_CUSTODY_SIGNAL_REASON_CODES_7BIT GetReasonCode() const;
    //return number of fills
    BPCODEC_EXPORT uint64_t AddCustodyIdToFill(const uint64_t custodyId);
    //return number of fills
    BPCODEC_EXPORT uint64_t AddContiguousCustodyIdsToFill(const uint64_t firstCustodyId, const uint64_t lastCustodyId);
public: //only public for unit testing
    BPCODEC_EXPORT uint64_t SerializeFills(uint8_t * serialization, uint64_t bufferSize) const;
    BPCODEC_EXPORT uint64_t GetFillSerializedSize() const;
    BPCODEC_EXPORT bool DeserializeFills(const uint8_t * serialization, uint64_t & numBytesTakenToDecode, uint64_t bufferSize);
};

struct CLASS_VISIBILITY_BPCODEC Bpv6AdministrativeRecord : public Bpv6CanonicalBlock {

    BPV6_ADMINISTRATIVE_RECORD_TYPE_CODE m_adminRecordTypeCode;
    std::unique_ptr<Bpv6AdministrativeRecordContentBase> m_adminRecordContentPtr;
    bool m_isFragment;

    BPCODEC_EXPORT Bpv6AdministrativeRecord(); //a default constructor: X()
    BPCODEC_EXPORT virtual ~Bpv6AdministrativeRecord() override; //a destructor: ~X()
    BPCODEC_EXPORT Bpv6AdministrativeRecord(const Bpv6AdministrativeRecord& o) = delete; //a copy constructor: X(const X&)
    BPCODEC_EXPORT Bpv6AdministrativeRecord(Bpv6AdministrativeRecord&& o); //a move constructor: X(X&&)
    BPCODEC_EXPORT Bpv6AdministrativeRecord& operator=(const Bpv6AdministrativeRecord& o) = delete; //a copy assignment: operator=(const X&)
    BPCODEC_EXPORT Bpv6AdministrativeRecord& operator=(Bpv6AdministrativeRecord&& o); //a move assignment: operator=(X&&)
    BPCODEC_EXPORT bool operator==(const Bpv6AdministrativeRecord & o) const; //operator ==
    BPCODEC_EXPORT bool operator!=(const Bpv6AdministrativeRecord & o) const; //operator !=
    BPCODEC_EXPORT virtual void SetZero() override;
    BPCODEC_EXPORT virtual uint64_t SerializeBpv6(uint8_t * serialization) override; //modifies m_dataPtr to serialized location
    BPCODEC_EXPORT virtual uint64_t GetCanonicalBlockTypeSpecificDataSerializationSize() const override;
    BPCODEC_EXPORT virtual bool Virtual_DeserializeExtensionBlockDataBpv6() override;
};



#endif //BPV6_H
