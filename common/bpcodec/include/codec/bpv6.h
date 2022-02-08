#ifndef BPV6_H
#define BPV6_H

#include <cstdint>
#include <cstddef>
#include "Cbhe.h"
#include "TimestampUtil.h"
#include "codec/PrimaryBlock.h"
#include "EnumAsFlagsMacro.h"
#include <array>

enum class CANONICAL_BLOCK_TYPE_CODES : uint8_t {
    BUNDLE_PAYLOAD_BLOCK = 1,
    CUSTODY_TRANSFER_ENHANCEMENT_BLOCK = 10
};

enum class BLOCK_PROCESSING_CONTROL_FLAGS : uint64_t {
    BLOCK_MUST_BE_REPLICATED_IN_EVERY_FRAGMENT = 1 << 0,
    TRANSMIT_STATUS_REPORT_IF_BLOCK_CANT_BE_PROCESSED = 1 << 1,
    DELETE_BUNDLE_IF_BLOCK_CANT_BE_PROCESSED = 1 << 2,
    LAST_BLOCK = 1 << 3,
    DISCARD_BLOCK_IF_IT_CANT_BE_PROCESSED = 1 << 4,
    BLOCK_WAS_FORWARDED_WITHOUT_BEING_PROCESSED = 1 << 5,
    BLOCK_CONTAINS_AN_EID_REFERENCE_FIELD = 1 << 6
};


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
struct Bpv6CbhePrimaryBlock : public PrimaryBlock {
    BPV6_BUNDLEFLAG m_bundleProcessingControlFlags;
    uint64_t block_length;
    uint64_t creation;
    uint64_t sequence;
    uint64_t lifetime;
    uint64_t fragment_offset;
    uint64_t data_length;      // 64 bytes

    // for the IPN scheme, we use NODE.SVC
    cbhe_eid_t m_destinationEid;
    cbhe_eid_t m_sourceNodeId;
    cbhe_eid_t m_reportToEid;
    cbhe_eid_t m_custodianEid;

    void SetZero();

    /**
     * Dumps a primary block to stdout in a human-readable way
     *
     * @param primary Primary block to print
     */
    void bpv6_primary_block_print() const;

    bool DeserializeBpv6(uint8_t * serialization, uint64_t & numBytesTakenToDecode, uint64_t bufferSize);
    uint64_t SerializeBpv6(uint8_t * serialization) const;


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

#define BPV6_BLOCKTYPE_PAYLOAD              (1)
#define BPV6_BLOCKTYPE_AUTHENTICATION       (2)
#define BPV6_BLOCKTYPE_INTEGRITY            (3)
#define BPV6_BLOCKTYPE_PAYLOAD_CONF         (4)
#define BPV6_BLOCKTYPE_PREV_HOP_INSERTION   (5)
#define BPV6_BLOCKTYPE_METADATA_EXTENSION   (8)
#define BPV6_BLOCKTYPE_EXTENSION_SECURITY   (9)
#define BPV6_BLOCKTYPE_CUST_TRANSFER_EXT    (10)
#define BPV6_BLOCKTYPE_BPLIB_BIB            (13)
#define BPV6_BLOCKTYPE_BUNDLE_AGE           (20)

#define BPV6_BLOCKFlAG_REPLICATE               (0x01)
#define BPV6_BLOCKFLAG_REPORT_PROCESS_FAILURE  (0x02)
#define BPV6_BLOCKFLAG_DISCARD_BUNDLE_FAILURE  (0x04)
#define BPV6_BLOCKFLAG_LAST_BLOCK              (0x08)
#define BPV6_BLOCKFLAG_DISCARD_BLOCK_FAILURE   (0x10)
#define BPV6_BLOCKFLAG_FORWARD_NOPROCESS       (0x20)
#define BPV6_BLOCKFLAG_EID_REFERENCE           (0x40)

/**
 * Structure that contains information necessary for a 5050-compatible canonical block
 */
struct bpv6_canonical_block {
    uint64_t flags;
    uint64_t length;
    uint8_t type; //should be at beginning but here do to better packing

    void SetZero();

    /**
     * Dumps a canonical block to stdout in a human-readable fashion
     *
     * @param block Canonical block which should be displayed
     */
    void bpv6_canonical_block_print() const;

    /**
     * Print just the block flags for a generic canonical block
     *
     * @param block The canonical block with flags to be displayed
     */
    void bpv6_block_flags_print() const;

    /**
     * Reads an RFC5050 canonical block from a buffer and decodes it into 'block'
     *
     * @param block structure into which values should be decoded
     * @param buffer target from which values should be read
     * @param offset offset into target from which values should be read
     * @param bufsz maximum size of the buffer
     * @return the number of bytes the canonical block was encoded into, or 0 on failure to decode
     */
    uint32_t bpv6_canonical_block_decode(const char* buffer, const size_t offset, const size_t bufsz);

    /**
     * Writes an RFC5050 canonical into a buffer as encoded by 'block'
     *
     * @param block structure from which values should be read and encoded
     * @param buffer target into which values should be encoded
     * @param offset offset into target into which values should be encoded
     * @param bufsz maximum size of the buffer
     * @return the number of bytes the canonical block was encoded into, or 0 on failure to encode
     */
    uint32_t bpv6_canonical_block_encode(char* buffer, const size_t offset, const size_t bufsz) const;
};



#endif //BPV6_H
