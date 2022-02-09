/***************************************************************************
 * NASA Glenn Research Center, Cleveland, OH
 * Released under the NASA Open Source Agreement (NOSA)
 * May  2021
 *
 ****************************************************************************
 */


#include <stdio.h>
#include <assert.h>
#include <string.h>
#include "codec/bpv6.h"
#include <inttypes.h>
#include "Sdnv.h"
#include <utility>
#include <iostream>


void bpv6_canonical_block::SetZero() {
    m_blockProcessingControlFlags = BPV6_BLOCKFLAG::NO_FLAGS_SET;
    length = 0;
    m_blockTypeCode = BPV6_BLOCK_TYPE_CODE::PRIMARY_IMPLICIT_ZERO;
}

uint32_t bpv6_canonical_block::bpv6_canonical_block_decode(const char* buffer, const size_t offset, const size_t bufsz) {
    uint64_t index = offset;
    uint8_t sdnvSize;
    m_blockTypeCode = static_cast<BPV6_BLOCK_TYPE_CODE>(buffer[index++]);

    const uint8_t flag8bit = buffer[index];
    if (flag8bit <= 127) {
        m_blockProcessingControlFlags = static_cast<BPV6_BLOCKFLAG>(flag8bit);
        ++index;
    }
    else {
        m_blockProcessingControlFlags = static_cast<BPV6_BLOCKFLAG>(SdnvDecodeU64((const uint8_t *)&buffer[index], &sdnvSize));
        if (sdnvSize == 0) {
            return 0; //return 0 on failure
        }
        index += sdnvSize;
    }

    length = SdnvDecodeU64((const uint8_t *)&buffer[index], &sdnvSize);
    if (sdnvSize == 0) {
        return 0; //return 0 on failure
    }
    index += sdnvSize;

    return static_cast<uint32_t>(index - offset);
}

uint32_t bpv6_canonical_block::bpv6_canonical_block_encode(char* buffer, const size_t offset, const size_t bufsz) const {
    uint64_t index = offset;
    uint64_t sdnvSize;
    buffer[index++] = static_cast<char>(m_blockTypeCode);

    if ((static_cast<uint64_t>(m_blockProcessingControlFlags)) <= 127) {
        buffer[index++] = static_cast<uint8_t>(m_blockProcessingControlFlags);
    }
    else {
        sdnvSize = SdnvEncodeU64((uint8_t *)&buffer[index], static_cast<uint64_t>(m_blockProcessingControlFlags));
        index += sdnvSize;
    }

    sdnvSize = SdnvEncodeU64((uint8_t *)&buffer[index], length);
    index += sdnvSize;

    return static_cast<uint32_t>(index - offset);
}


void bpv6_canonical_block::bpv6_canonical_block_print() const {
    printf("Canonical block [type %u]\n", static_cast<unsigned int>(m_blockTypeCode));
    switch(m_blockTypeCode) {
        case BPV6_BLOCK_TYPE_CODE::BUNDLE_AUTHENTICATION:
            printf("> Authentication block\n");
            break;
        case BPV6_BLOCK_TYPE_CODE::EXTENSION_SECURITY:
            printf("> Extension security block\n");
            break;
        case BPV6_BLOCK_TYPE_CODE::PAYLOAD_INTEGRITY:
            printf("> Integrity block\n");
            break;
        case BPV6_BLOCK_TYPE_CODE::METADATA_EXTENSION:
            printf("> Metadata block\n");
            break;
        case BPV6_BLOCK_TYPE_CODE::PAYLOAD:
            printf("> Payload block\n");
            break;
        case BPV6_BLOCK_TYPE_CODE::PAYLOAD_CONFIDENTIALITY:
            printf("> Payload confidentiality block\n");
            break;
        case BPV6_BLOCK_TYPE_CODE::PREVIOUS_HOP_INSERTION:
            printf("> Previous hop insertion block\n");
            break;
        case BPV6_BLOCK_TYPE_CODE::CUSTODY_TRANSFER_ENHANCEMENT:
            printf("> ACS custody transfer enhancement block (CTEB)\n");
            break;
        case BPV6_BLOCK_TYPE_CODE::BPLIB_BIB:
            printf("> Bplib bundle integrity block (BIB)\n");
            break;
        case BPV6_BLOCK_TYPE_CODE::BUNDLE_AGE:
            printf("> Bundle age extension (BAE)\n");
            break;
        default:
            printf("> Unknown block type\n");
            break;
    }
    bpv6_block_flags_print();
    printf("Block length: %" PRIu64 " bytes\n", length);
}

void bpv6_canonical_block::bpv6_block_flags_print() const {
    printf("Flags: 0x%" PRIx64 "\n", static_cast<uint64_t>(m_blockProcessingControlFlags));
    if ((m_blockProcessingControlFlags & BPV6_BLOCKFLAG::MUST_BE_REPLICATED_IN_EVERY_FRAGMENT) != BPV6_BLOCKFLAG::NO_FLAGS_SET) {
        printf("* Block must be replicated in every fragment.\n");
    }
    if ((m_blockProcessingControlFlags & BPV6_BLOCKFLAG::STATUS_REPORT_REQUESTED_IF_BLOCK_CANT_BE_PROCESSED) != BPV6_BLOCKFLAG::NO_FLAGS_SET) {
        printf("* Transmit status report if block can't be processed.\n");
    }
    if ((m_blockProcessingControlFlags & BPV6_BLOCKFLAG::DELETE_BUNDLE_IF_BLOCK_CANT_BE_PROCESSED) != BPV6_BLOCKFLAG::NO_FLAGS_SET) {
        printf("* Delete bundle if block can't be processed.\n");
    }
    if ((m_blockProcessingControlFlags & BPV6_BLOCKFLAG::IS_LAST_BLOCK) != BPV6_BLOCKFLAG::NO_FLAGS_SET) {
        printf("* Last block in this bundle.\n");
    }
    if ((m_blockProcessingControlFlags & BPV6_BLOCKFLAG::DISCARD_BLOCK_IF_IT_CANT_BE_PROCESSED) != BPV6_BLOCKFLAG::NO_FLAGS_SET) {
        printf("* Discard block if it can't be processed.\n");
    }
    if ((m_blockProcessingControlFlags & BPV6_BLOCKFLAG::BLOCK_WAS_FORWARDED_WITHOUT_BEING_PROCESSED) != BPV6_BLOCKFLAG::NO_FLAGS_SET) {
        printf("* Block was forwarded without being processed.\n");
    }
    if ((m_blockProcessingControlFlags & BPV6_BLOCKFLAG::BLOCK_CONTAINS_AN_EID_REFERENCE_FIELD) != BPV6_BLOCKFLAG::NO_FLAGS_SET) {
        printf("* Block contains an EID-reference field.\n");
    }
}
