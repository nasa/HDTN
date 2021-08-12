/**
 * Simple encoder / decoder for RFC 5050 bundles.
 *
 * @author Gilbert Clark (GRC-LCN0)
 * NOTICE: This code should be considered experimental, and should thus not be used in critical applications / contexts.
 */

#ifndef BPV6_H
#define BPV6_H

#include <stdlib.h>
#include <stdint.h>
 
/*
#ifdef __cplusplus
#include <vector>
#include <boost/asio/buffer.hpp>
struct Bpv6CanonicalBlockView {
    CANONICAL_BLOCK_TYPE_CODES m_typeCode;
    uint64_t m_blockProcessingControlFlags;
    boost::asio::const_buffer m_actualSerializedBodyPtr; //includes length

    boost::asio::const_buffer m_actualSerializedHeaderAndBodyPtr;
    std::vector<uint8_t> m_optionalSerializedStorage;

    void AddCanonicalBlockProcessingControlFlag(BLOCK_PROCESSING_CONTROL_FLAGS flag);
    bool HasCanonicalBlockProcessingControlFlagSet(BLOCK_PROCESSING_CONTROL_FLAGS flag) const;
};
struct CbheBundleV6 {
    bpv6_primary_block m_primary;
    std::vector<bpv6_canonical_block> m_canonicalBlockHeadersVec;
};
#endif
 */

#ifdef __cplusplus
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

#endif

#ifdef __cplusplus
extern "C" {
#endif

// (1-byte version) + (1-byte sdnv block length) + (1-byte sdnv zero dictionary length) + (up to 14 10-byte sdnvs) + (32 bytes hardware accelerated SDNV overflow instructions) 
#define CBHE_BPV6_MINIMUM_SAFE_PRIMARY_HEADER_ENCODE_SIZE (1 + 1 + 1 + (14*10) + 32)

// (1-byte block type) + (2 10-byte sdnvs) + (32 bytes hardware accelerated SDNV overflow instructions) 
#define BPV6_MINIMUM_SAFE_CANONICAL_HEADER_ENCODE_SIZE (1 + (2*10) + 32)

// (1-byte block type) + (2 10-byte sdnvs) + primary
#define CBHE_BPV6_MINIMUM_SAFE_PRIMARY_PLUS_CANONICAL_HEADER_ENCODE_SIZE (1 + (2*10) + CBHE_BPV6_MINIMUM_SAFE_PRIMARY_HEADER_ENCODE_SIZE)

#define BPV6_CCSDS_VERSION        (6)
#define BPV6_5050_TIME_OFFSET     (946684800)

#define bpv6_bundle_get_gflags(flags)    ((uint32_t)(flags  & 0x00007F))
#define bpv6_bundle_get_priority(flags)  ((uint32_t)((flags & 0x000180) >> 7))
#define bpv6_bundle_get_reporting(flags) ((uint32_t)((flags & 0x3FC000) >> 13))

#define bpv6_bundle_set_gflags(flags)    ((uint32_t)(flags  & 0x00007F))
#define bpv6_bundle_set_priority(flags)  ((uint32_t)((flags & 0x000003) << 7))
#define bpv6_bundle_set_reporting(flags) ((uint32_t)((flags & 0x0000ff) << 13))

#define bpv6_unix_to_5050(time)          (time - BPV6_5050_TIME_OFFSET)
#define bpv6_5050_to_unix(time)          (time + BPV6_5050_TIME_OFFSET)

#define BPV6_BUNDLEFLAG_FRAGMENT      (0x01)
#define BPV6_BUNDLEFLAG_ADMIN_RECORD  (0x02)
#define BPV6_BUNDLEFLAG_NOFRAGMENT    (0x04)
#define BPV6_BUNDLEFLAG_CUSTODY       (0x08)
#define BPV6_BUNDLEFLAG_SINGLETON     (0x10)
#define BPV6_BUNDLEFLAG_APPL_ACK      (0x20)
#define BPV6_BUNDLEFLAG_RESERVED      (0x40)
#define BPV6_BUNDLEFLAG_TEMPLATE      (0x40)

#define BPV6_PRIORITY_BULK            (0x00)
#define BPV6_PRIORITY_NORMAL          (0x01)
#define BPV6_PRIORITY_EXPEDITED       (0x02)
#define BPV6_PRIORITY_RESERVED        (0x03)

#define BPV6_REPORTFLAG_RECEPTION     (0x01)
#define BPV6_REPORTFLAG_CUSTODY       (0x02)
#define BPV6_REPORTFLAG_FORWARD       (0x04)
#define BPV6_REPORTFLAG_DELIVERY      (0x08)
#define BPV6_REPORTFLAG_DELETION      (0x10)

#define BPV6_CLFLAG_STCP   (0x01)
#define BPV6_CLFLAG_LTP    (0x02)
#define BPV6_CLFLAG_UDP    (0x04)
#define BPV6_CLFLAG_AOS    (0x08)
#define BPV6_CLFLAG_TCP    (0x10)
#define BPV6_CLFLAG_SCTP   (0x20)
#define BPV6_CLFLAG_DCCP   (0x40)
#define BPV6_CLFLAG_HTTP   (0x80)
#define BPV6_CLFLAG_COAP   (0x100)
#define BPV6_CLFLAG_ETHER  (0x200)
#define BPV6_CLFLAG_IP4    (0x400)
#define BPV6_CLFLAG_IP6    (0x800)

#define BPV6_CLPORT_UDP      (4556)
#define BPV6_CLPORT_TCP      (4556)
#define BPV6_CLPORT_LTP      (1113)   // LTP over UDP is implied

/**
 * Structure that contains information necessary for an RFC5050-compatible primary block
 */
typedef struct bpv6_primary_block {
    uint8_t  version;
    uint8_t  vpad[7];
    uint64_t flags;
    uint64_t block_length;
    uint64_t creation;
    uint64_t sequence;
    uint64_t lifetime;
    uint64_t fragment_offset;
    uint64_t data_length;      // 64 bytes

    // for the IPN scheme, we use NODE.SVC
    uint64_t dst_node;
    uint64_t dst_svc;
    uint64_t src_node;
    uint64_t src_svc;
    uint64_t report_node;
    uint64_t report_svc;
    uint64_t custodian_node;
    uint64_t custodian_svc;    // 64 bytes
} bpv6_primary_block;

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
typedef struct bpv6_canonical_block {
    uint8_t type;
    uint8_t tpad[7];
    uint64_t flags;
    uint64_t length;
} bpv6_canonical_block;


/**
 * Dumps a primary block to stdout in a human-readable way
 *
 * @param primary Primary block to print
 */
void bpv6_primary_block_print(bpv6_primary_block* primary);

/**
 * Reads an RFC5050 with RFC6260 Compressed Bundle Header Encoding (CBHE) primary block from a buffer and decodes it into 'primary'
 *
 * @param primary structure into which values should be decoded
 * @param buffer target from which values should be decoded
 * @param offset offset into target from which values should be decoded
 * @param bufsz maximum size of the buffer
 * @return the number of bytes the primary block was decoded from, or 0 on failure to decode
 */
uint32_t cbhe_bpv6_primary_block_decode(bpv6_primary_block* primary, const char* buffer, const size_t offset, const size_t bufsz);

/**
 * Writes an RFC5050 with RFC6260 Compressed Bundle Header Encoding (CBHE) primary block into a buffer as encoded from 'primary'.  Note that block length is automatically
 * computed based on the encoded length of other fields ... but that the block length cannot exceed 128 bytes, or
 * encoding will fail.
 *
 * @param primary structure from which values should be read and encoded
 * @param buffer target into which values should be encoded
 * @param offset offset into target into which values should be encoded
 * @param bufsz maximum size of the buffer
 * @return the number of bytes the primary block was encoded into, or 0 on failure to encode
 */
uint32_t cbhe_bpv6_primary_block_encode(const bpv6_primary_block* primary, char* buffer, const size_t offset, const size_t bufsz);

/**
 * Dumps a canonical block to stdout in a human-readable fashion
 *
 * @param block Canonical block which should be displayed
 */
void bpv6_canonical_block_print(bpv6_canonical_block* block);

/**
 * Reads an RFC5050 canonical block from a buffer and decodes it into 'block'
 *
 * @param block structure into which values should be decoded
 * @param buffer target from which values should be read
 * @param offset offset into target from which values should be read
 * @param bufsz maximum size of the buffer
 * @return the number of bytes the canonical block was encoded into, or 0 on failure to decode
 */
uint32_t bpv6_canonical_block_decode(bpv6_canonical_block* block, const char* buffer, const size_t offset, const size_t bufsz);

/**
 * Writes an RFC5050 canonical into a buffer as encoded by 'block'
 *
 * @param block structure from which values should be read and encoded
 * @param buffer target into which values should be encoded
 * @param offset offset into target into which values should be encoded
 * @param bufsz maximum size of the buffer
 * @return the number of bytes the canonical block was encoded into, or 0 on failure to encode
 */
uint32_t bpv6_canonical_block_encode(const bpv6_canonical_block* block, char* buffer, const size_t offset, const size_t bufsz);

#ifdef __cplusplus
struct cbhe_eid_t {
    uint64_t nodeId;
    uint64_t serviceId;
    
    cbhe_eid_t(); //a default constructor: X()
    cbhe_eid_t(uint64_t paramNodeId, uint64_t paramServiceId);
    ~cbhe_eid_t(); //a destructor: ~X()
    cbhe_eid_t(const cbhe_eid_t& o); //a copy constructor: X(const X&)
    cbhe_eid_t(cbhe_eid_t&& o); //a move constructor: X(X&&)
    cbhe_eid_t& operator=(const cbhe_eid_t& o); //a copy assignment: operator=(const X&)
    cbhe_eid_t& operator=(cbhe_eid_t&& o); //a move assignment: operator=(X&&)
    bool operator==(const cbhe_eid_t & o) const; //operator ==
    bool operator!=(const cbhe_eid_t & o) const; //operator !=
    bool operator<(const cbhe_eid_t & o) const; //operator < so it can be used as a map key
};

struct cbhe_bundle_uuid_t {

    //The creation timestamp is a pair of SDNVs that,
    //together with the source endpoint ID and (if the bundle is a
    //fragment) the fragment offset and payload length, serve to
    //identify the bundle.
    //A source Bundle Protocol Agent must never create two distinct bundles with the same source
    //endpoint ID and bundle creation timestamp.The combination of
    //source endpoint ID and bundle creation timestamp therefore serves
    //to identify a single transmission request, enabling it to be
    //acknowledged by the receiving application

    uint64_t creationSeconds;
    uint64_t sequence;
    uint64_t srcNodeId;
    uint64_t srcServiceId;
    //below if isFragment (default 0 if not a fragment)
    uint64_t fragmentOffset;
    uint64_t dataLength;      // 64 bytes



    cbhe_bundle_uuid_t(); //a default constructor: X()
    cbhe_bundle_uuid_t(uint64_t paramCreationSeconds, uint64_t paramSequence,
        uint64_t paramSrcNodeId, uint64_t paramSrcServiceId, uint64_t paramFragmentOffset, uint64_t paramDataLength);
    cbhe_bundle_uuid_t(const bpv6_primary_block & primary);
    ~cbhe_bundle_uuid_t(); //a destructor: ~X()
    cbhe_bundle_uuid_t(const cbhe_bundle_uuid_t& o); //a copy constructor: X(const X&)
    cbhe_bundle_uuid_t(cbhe_bundle_uuid_t&& o); //a move constructor: X(X&&)
    cbhe_bundle_uuid_t& operator=(const cbhe_bundle_uuid_t& o); //a copy assignment: operator=(const X&)
    cbhe_bundle_uuid_t& operator=(cbhe_bundle_uuid_t&& o); //a move assignment: operator=(X&&)
    bool operator==(const cbhe_bundle_uuid_t & o) const; //operator ==
    bool operator!=(const cbhe_bundle_uuid_t & o) const; //operator !=
    bool operator<(const cbhe_bundle_uuid_t & o) const; //operator < so it can be used as a map key
};
#endif


#ifdef __cplusplus
}
#endif

#endif //BPV6_H
