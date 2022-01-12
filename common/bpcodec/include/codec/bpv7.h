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
#include "Cbhe.h"
#include "TimestampUtil.h"

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




#define BPV7_BLOCKTYPE_PAYLOAD      (0x01)
#define BPV7_BLOCKTYPE_PREVHOP      (0x07)
#define BPV7_BLOCKTYPE_AGE          (0x08)



struct Bpv7CbhePrimaryBlock {
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
    bool DeserializeBpv7(uint8_t * serialization, uint64_t & numBytesTakenToDecode, uint64_t bufferSize); //serialization must be temporarily modifyable to zero crc and restore it
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

    uint64_t m_blockNumber;
    uint64_t m_blockProcessingControlFlags;
    uint8_t * m_dataPtr; //if NULL, data won't be copied (just allocated) and crc won't be computed
    uint64_t m_dataLength;
    uint32_t m_computedCrc32; //computed after serialization or deserialization
    uint16_t m_computedCrc16; //computed after serialization or deserialization
    uint8_t m_blockTypeCode; //placed uint8 at the end of struct (should be at the beginning) for more efficient memory usage
    uint8_t m_crcType; //placed uint8 at the end of struct for more efficient memory usage
    
    
    Bpv7CanonicalBlock(); //a default constructor: X()
    ~Bpv7CanonicalBlock(); //a destructor: ~X()
    Bpv7CanonicalBlock(const Bpv7CanonicalBlock& o); //a copy constructor: X(const X&)
    Bpv7CanonicalBlock(Bpv7CanonicalBlock&& o); //a move constructor: X(X&&)
    Bpv7CanonicalBlock& operator=(const Bpv7CanonicalBlock& o); //a copy assignment: operator=(const X&)
    Bpv7CanonicalBlock& operator=(Bpv7CanonicalBlock&& o); //a move assignment: operator=(X&&)
    bool operator==(const Bpv7CanonicalBlock & o) const; //operator ==
    bool operator!=(const Bpv7CanonicalBlock & o) const; //operator !=
    void SetZero();
    uint64_t SerializeBpv7(uint8_t * serialization); //modifies m_dataPtr to serialized location
    void RecomputeCrcAfterDataModification(uint8_t * serializationBase, const uint64_t sizeSerialized);
    bool DeserializeBpv7(uint8_t * serialization, uint64_t & numBytesTakenToDecode, uint64_t bufferSize); //serialization must be temporarily modifyable to zero crc and restore it
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
