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

#ifdef __cplusplus
extern "C" {
#endif

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
    uint8_t  eidlen;    // synthetic: not directly encoded or decoded, but used for IF conversion
    uint8_t  vpad[6];
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
 * Encodes up to a 64-bit value as an SDNV.  Resulting SDNV can be up to 10 bytes long.
 *
 * @param target value to encode
 * @param buffer buffer into which encoded value will be written
 * @param offset offset into buffer at which encoded value will be written
 * @param bufsz maximum size of the buffer
 * @return the number of bytes the SDNV was encoded as, or 0 on failure to encode
 */
uint8_t  bpv6_sdnv_encode(const uint64_t target, char* buffer, const size_t offset, const size_t bufsz);

/**
 * Decodes up to a 56-bit value from an SDNV - SDNV can only be up to 8 bytes in length.
 *
 * @param target address to which decoded value should be written - field must be 64 bits in length
 * @param buffer buffer from which encoded value will be read
 * @param offset offset into buffer at which encoded value will be read
 * @param bufsz maximum size of the buffer
 * @return the number of bytes the SDNV was decoded from, or 0 on failure to decode
 */
uint8_t  bpv6_sdnv_decode(uint64_t* target, const char* buffer, const size_t offset, const size_t bufsz);

/**
 * Dumps a primary block to stdout in a human-readable way
 *
 * @param primary Primary block to print
 */
void bpv6_primary_block_print(bpv6_primary_block* primary);

/**
 * Reads an RFC5050 primary block from a buffer and decodes it into 'primary'
 *
 * @param primary structure into which values should be decoded
 * @param buffer target from which values should be decoded
 * @param offset offset into target from which values should be decoded
 * @param bufsz maximum size of the buffer
 * @return the number of bytes the primary block was decoded from, or 0 on failure to decode
 */
uint32_t bpv6_primary_block_decode(bpv6_primary_block* primary, const char* buffer, const size_t offset, const size_t bufsz);

/**
 * Writes an RFC5050 primary block into a buffer as encoded from 'primary'.  Note that block length is automatically
 * computed based on the encoded length of other fields ... but that the block length cannot exceed 128 bytes, or
 * encoding will fail.
 *
 * @param primary structure from which values should be read and encoded
 * @param buffer target into which values should be encoded
 * @param offset offset into target into which values should be encoded
 * @param bufsz maximum size of the buffer
 * @return the number of bytes the primary block was encoded into, or 0 on failure to encode
 */
uint32_t bpv6_primary_block_encode(const bpv6_primary_block* primary, char* buffer, const size_t offset, const size_t bufsz);

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
}
#endif

#endif //BPV6_H
