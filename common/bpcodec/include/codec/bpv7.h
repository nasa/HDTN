/**
 * Simple encoder / decoder for bpbis-14 bundles.
 *
 * NOTICE: This code should be considered experimental, and should thus not be used in critical applications / contexts.
 * @author Gilbert Clark (GRC-LCN0)
 */

#ifndef BPV7_H
#define BPV7_H
#include <cstdint>
#include <cstddef>

struct Bpv7PrimaryBlock {
    uint64_t bundleProcessingControlFlags;
    uint32_t crc;

};








#if 0
#ifdef __cplusplus
extern "C" {
#endif

#ifdef __APPLE__
#include <unitypes.h>
#endif
#include <stdlib.h>
#include <stdint.h>

#define BPV7_CCSDS_VERSION         (7)
#define BPV7_VERSION_OFFSET        (2)
#define BPV7_MAX_PATHLEN           (232)

#define BPV7_EID_SCHEME_DTN        (1)
#define BPV7_EID_SCHEME_IPN        (2)

#define bpv7_field_count(VALUE)    (VALUE & 0x1F)

#define BPV7_MAGIC_NOFRAGMENT_LE      (0x1907899f)
#define BPV7_MAGIC_FRAGMENT_LE        (0x19078b9f)

#define BPV7_MAGIC_NOFRAGMENT_BE      (0x9f890719)
#define BPV7_MAGIC_FRAGMENT_BE        (0x9f8b0719)

#define BPV7_MAGIC_NOFRAGMENT         (BPV7_MAGIC_NOFRAGMENT_LE)
#define BPV7_MAGIC_FRAGMENT           (BPV7_MAGIC_FRAGMENT_LE)

#define BPV7_DTN_EPOCH             (946684800)  // See: 4.1.6

#define BPV7_BUNDLEFLAG_TEMPLATE        (0x2000)

#define BPV7_BUNDLEFLAG_DELETESTATUS    (0x1000)
#define BPV7_BUNDLEFLAG_DELIVERYSTATUS  (0x0800)
#define BPV7_BUNDLEFLAG_FORWARDSTATUS   (0x0400)
#define BPV7_BUNDLEFLAG_RECEIVESTATUS   (0x0100)
#define BPV7_BUNDLEFLAG_STATUSTIME      (0x0040)
#define BPV7_BUNDLEFLAG_USERAPPACK      (0x0020)
#define BPV7_BUNDLEFLAG_NOFRAGMENT      (0x0004)
#define BPV7_BUNDLEFLAG_ADMINRECORD     (0x0002)
#define BPV7_BUNDLEFLAG_ISFRAGMENT      (0x0001)

#define bpv7_cbor_major_type(value)   (value >> 5)
#define BPV7_CBOR_TYPE_UINT           (0)
#define BPV7_CBOR_TYPE_BYTESTRING     (2)
#define BPV7_CBOR_TYPE_ARRAY          (4)

namespace hdtn {
#pragma pack(push, 1)
    typedef struct bpv7_hdr {
        union {
            uint8_t   bytes[4];
            uint32_t  magic;       // pack first four elements into single 32 bit value
        } hdr;

        uint16_t flags;     // bundle flags go here
        uint8_t  crc;       // CRC type
    } /*__attribute__((packed))*/ bpv7_hdr;

    typedef struct bpv7_eid {
        uint8_t  type;
        uint8_t  _padding[7];
        uint64_t node;
        uint64_t service;
        char     path[BPV7_MAX_PATHLEN];
    } /*__attribute__((packed))*/ bpv7_ipn_eid;

    typedef struct bpv7_primary_block {
        uint8_t        version;
        uint8_t        crc_type;
        uint16_t       flags;
        uint8_t        crc_data[4];
        uint64_t       creation;
        uint64_t       sequence;
        uint64_t       lifetime;
        uint64_t       offset;
        uint64_t       app_length;
        uint8_t        _padding2[16];  // pad to 64 bytes
        bpv7_eid       dst;            // +256 + 64
        bpv7_eid       src;            // +512 + 64
        bpv7_eid       report;         // +768 + 64
    } /*__attribute__((packed)) __attribute__(( aligned(64) ))*/ bpv7_primary_block;

    /**
     * Reads a bpbis primary block from a buffer and decodes it into 'primary'
     *
     * @param primary structure into which values should be decoded
     * @param buffer target from which values should be decoded
     * @param offset offset into target from which values should be decoded
     * @param bufsz maximum size of the buffer
     * @return the number of bytes the primary block was decoded from, or 0 on failure to decode
     */
    uint32_t bpv7_primary_block_decode(bpv7_primary_block* primary, char* buffer, size_t offset, size_t bufsz);

    /**
     * Writes a bpbis primary block into a buffer as encoded from 'primary'.
     *
     * @param primary structure from which values should be read and encoded
     * @param buffer target into which values should be encoded
     * @param offset offset into target into which values should be encoded
     * @param bufsz maximum size of the buffer
     * @return the number of bytes the primary block was encoded into, or 0 on failure to encode
     */
    uint32_t bpv7_primary_block_encode(bpv7_primary_block* primary, char* buffer, size_t offset, size_t bufsz);

#define BPV7_BLOCKFLAG_DEL_NOPROC   (0x08)
#define BPV7_BLOCKFLAG_RPT_NOPROC   (0x04)
#define BPV7_BLOCKFLAG_RMV_NOPROC   (0x02)
#define BPV7_BLOCKFLAG_REPLICATED   (0x01)

#define BPV7_BLOCKTYPE_PAYLOAD      (0x01)
#define BPV7_BLOCKTYPE_PREVHOP      (0x07)
#define BPV7_BLOCKTYPE_AGE          (0x08)

    typedef struct bpv7_canonical_block {
        uint8_t        block_type;
        uint8_t        flags;
        uint8_t        crc_type;
        uint8_t        padding[1];
        uint32_t       block_id;
        // keep track of the offset at which the block starts, and the length of the block
        uint64_t       offset;
        uint64_t       len;
        uint8_t        crc_data[4];
        uint8_t        padding2[4];
    } /*__attribute__((packed)) __attribute__(( aligned(64) ))*/ bpv7_canonical_block;
#pragma pack(pop)
    /**
     * Reads a bundle canonical block from a buffer and decodes it as 'block'
     *
     * @param block location into which data should be decoded
     * @param buffer target from which values should be decoded
     * @param offset offset into target from which values should be decoded
     * @param bufsz maximum size of the buffer
     * @return the number of bytes the canonical block was decoded from, or 0 on error
     */
    uint32_t bpv7_canonical_block_decode(bpv7_canonical_block* block, char* buffer, size_t offset, size_t bufsz);

    /**
     * Writes a bundle primary block into a buffer as encoded from 'primary'.
     *
     * @param block structure from which values should be read and encoded
     * @param buffer target into which values should be encoded
     * @param offset offset into target into which values should be encoded
     * @param bufsz maximum size of the buffer
     * @return the number of bytes the canonical block was encoded into, or 0 on error
     */
    uint32_t bpv7_canonical_block_encode(bpv7_canonical_block* block, char* buffer, size_t offset, size_t bufsz);

    /**
     * Initializes the CBOR routines for use.  Only must be called explicitly if the application is threaded *and*
     * CBOR encoding / decoding will be handled in multiple threads at once.
     */
     void cbor_init();

    /**
     * Decodes an unsigned integer CBOR type into memory location 'dst'
     *
     * @param dst Decoded unsigned integer value
     * @param src Encoded unsigned integer value
     * @param offset Offset into src from which data will be read
     * @param bufsz Maximum size of 'src'
     * @return Non-zero number of bytes advanced, or 0 on error
     */
    uint8_t cbor_decode_uint(uint64_t* dst, uint8_t* src, uint64_t offset, uint64_t bufsz);

    /**
     * Encodes an unsigned integer CBOR type into memory location 'dst'
     *
     * @param dst Encoded unsigned integer value
     * @param src Decoded unsigned integer value
     * @param offset Offset into 'dst' at which data will be written
     * @param bufsz Maximum size of 'dst'
     * @return Non-zero number of bytes advanced, or 0 on error
     */
     uint8_t cbor_encode_uint(uint8_t* dst, uint64_t src, uint64_t offset, uint64_t bufsz);
}

#ifdef __cplusplus
}
#endif
#endif

#endif //BPV7_H
