#include <stdio.h>
#include <assert.h>
#include <string.h>
#include "codec/bpv6.h"

uint32_t bpv6_primary_block_decode(bpv6_primary_block* primary, const char* buffer, const size_t offset, const size_t bufsz) {
    primary->version = buffer[offset];
    uint8_t  is_fragment = 0;
    uint64_t tmp;
    uint64_t index = offset + 1;
    uint8_t  incr = 0;

    primary->eidlen = 0;
    if(primary->version != BPV6_CCSDS_VERSION) {
        return 0;
    }

    incr = bpv6_sdnv_decode(&primary->flags, buffer, index, bufsz);
    index += incr;
    if(bpv6_bundle_get_gflags(primary->flags) & BPV6_BUNDLEFLAG_FRAGMENT) {
        is_fragment = 1;
    }
    incr = bpv6_sdnv_decode(&primary->block_length, buffer, index, bufsz);
    index += incr;

    incr = bpv6_sdnv_decode(&primary->dst_node, buffer, index, bufsz);
    index += incr;
    primary->eidlen = (incr > primary->eidlen) ? incr : primary->eidlen;
    incr = bpv6_sdnv_decode(&primary->dst_svc, buffer, index, bufsz);
    index += incr;
    primary->eidlen = (incr > primary->eidlen) ? incr : primary->eidlen;
    incr = bpv6_sdnv_decode(&primary->src_node, buffer, index, bufsz);
    index += incr;
    primary->eidlen = (incr > primary->eidlen) ? incr : primary->eidlen;
    incr = bpv6_sdnv_decode(&primary->src_svc, buffer, index, bufsz);
    index += incr;
    primary->eidlen = (incr > primary->eidlen) ? incr : primary->eidlen;
    incr = bpv6_sdnv_decode(&primary->report_node, buffer, index, bufsz);
    index += incr;
    primary->eidlen = (incr > primary->eidlen) ? incr : primary->eidlen;
    incr = bpv6_sdnv_decode(&primary->report_svc, buffer, index, bufsz);
    index += incr;
    primary->eidlen = (incr > primary->eidlen) ? incr : primary->eidlen;
    incr = bpv6_sdnv_decode(&primary->custodian_node, buffer, index, bufsz);
    index += incr;
    primary->eidlen = (incr > primary->eidlen) ? incr : primary->eidlen;
    incr = bpv6_sdnv_decode(&primary->custodian_svc, buffer, index, bufsz);
    index += incr;
    primary->eidlen = (incr > primary->eidlen) ? incr : primary->eidlen;

    incr = bpv6_sdnv_decode(&primary->creation, buffer, index, bufsz);
    index += incr;
    incr = bpv6_sdnv_decode(&primary->sequence, buffer, index, bufsz);
    index += incr;
    incr = bpv6_sdnv_decode(&primary->lifetime, buffer, index, bufsz);
    index += incr;
    incr = bpv6_sdnv_decode(&tmp, buffer, index, bufsz);
    index += incr;
    // Skip the entirety of the dictionary - we assume an IPN scheme
    index += tmp;
    if(is_fragment) {
        incr = bpv6_sdnv_decode(&primary->fragment_offset, buffer, index, bufsz);
        index += incr;
        incr = bpv6_sdnv_decode(&primary->data_length, buffer, index, bufsz);
        index += incr;
    }

    return (index - offset);
}

uint32_t bpv6_primary_block_encode(const bpv6_primary_block* primary, char* buffer, const size_t offset, const size_t bufsz) {
    buffer[offset] = primary->version;
    uint8_t  is_fragment = 0;
    uint64_t index = offset + 1;
    uint8_t  incr = 0;
    uint32_t block_length = 0;
    uint64_t block_length_offset = 0;

    if(primary->version != BPV6_CCSDS_VERSION) {
        return 0;
    }

    incr = bpv6_sdnv_encode(primary->flags, buffer, index, bufsz);
    index += incr;
    if(bpv6_bundle_get_gflags(primary->flags) & BPV6_BUNDLEFLAG_FRAGMENT) {
        is_fragment = 1;
    }
    block_length_offset = index;
    ++index;  // we skip one byte so we can come back and write it later
    // incr = bpv6_sdnv_encode(primary->block_length, buffer, index, bufsz);
    // index += incr;

    incr = bpv6_sdnv_encode(primary->dst_node, buffer, index, bufsz);
    index += incr;
    block_length += incr;
    incr = bpv6_sdnv_encode(primary->dst_svc, buffer, index, bufsz);
    index += incr;
    block_length += incr;
    incr = bpv6_sdnv_encode(primary->src_node, buffer, index, bufsz);
    index += incr;
    block_length += incr;
    incr = bpv6_sdnv_encode(primary->src_svc, buffer, index, bufsz);
    index += incr;
    block_length += incr;
    incr = bpv6_sdnv_encode(primary->report_node, buffer, index, bufsz);
    index += incr;
    block_length += incr;
    incr = bpv6_sdnv_encode(primary->report_svc, buffer, index, bufsz);
    index += incr;
    block_length += incr;
    incr = bpv6_sdnv_encode(primary->custodian_node, buffer, index, bufsz);
    index += incr;
    block_length += incr;
    incr = bpv6_sdnv_encode(primary->custodian_svc, buffer, index, bufsz);
    index += incr;
    block_length += incr;

    incr = bpv6_sdnv_encode(primary->creation, buffer, index, bufsz);
    index += incr;
    block_length += incr;
    incr = bpv6_sdnv_encode(primary->sequence, buffer, index, bufsz);
    index += incr;
    block_length += incr;
    incr = bpv6_sdnv_encode(primary->lifetime, buffer, index, bufsz);
    index += incr;
    block_length += incr;
    // encode a zero-length dictionary
    incr = bpv6_sdnv_encode(0, buffer, index, bufsz);
    index += incr;
    block_length += incr;
    if(is_fragment) {
        incr = bpv6_sdnv_encode(primary->fragment_offset, buffer, index, bufsz);
        index += incr;
        block_length += incr;
        incr = bpv6_sdnv_encode(primary->data_length, buffer, index, bufsz);
        index += incr;
        block_length += incr;
    }
    incr = bpv6_sdnv_encode(block_length, buffer, block_length_offset, bufsz);
    if(incr > 1) {
        return 0;  // our encoding failed because our block length was too long ...
    }

    return (index - offset);
}

uint32_t bpv6_canonical_block_decode(bpv6_canonical_block* block, const char* buffer, const size_t offset, const size_t bufsz) {
    uint64_t index = offset;
    uint8_t  incr  = 0;
    block->type = buffer[index];
    ++index;
    incr = bpv6_sdnv_decode(&block->flags, buffer, index, bufsz);
    index += incr;
    incr = bpv6_sdnv_decode(&block->length, buffer, index, bufsz);
    index += incr;

    return index - offset;
}

uint32_t bpv6_canonical_block_encode(const bpv6_canonical_block* block, char* buffer, const size_t offset, const size_t bufsz) {
    uint64_t index = offset;
    uint8_t  incr  = 0;
    buffer[index] = block->type;
    ++index;
    incr = bpv6_sdnv_encode(block->flags, buffer, index, bufsz);
    index += incr;
    incr = bpv6_sdnv_encode(block->length, buffer, index, bufsz);
    index += incr;

    return index - offset;
}

void bpv6_primary_block_print(bpv6_primary_block* primary) {
    printf("BPv%d / Primary block (%d bytes)\n", (int)primary->version, (int)primary->block_length);
    printf("Flags: 0x%lx\n", primary->flags);
    uint32_t flags = bpv6_bundle_get_gflags(primary->flags);
    if(flags & BPV6_BUNDLEFLAG_NOFRAGMENT) {
        printf("* No fragmentation allowed\n");
    }
    if(flags & BPV6_BUNDLEFLAG_FRAGMENT) {
        printf("* Bundle is a fragment\n");
    }
    if(flags & BPV6_BUNDLEFLAG_ADMIN_RECORD) {
        printf("* Bundle is administrative (control) traffic\n");
    }
    if(flags & BPV6_BUNDLEFLAG_CUSTODY) {
        printf("* Custody transfer requested\n");
    }
    if(flags & BPV6_BUNDLEFLAG_APPL_ACK) {
        printf("* Application acknowledgment requested.\n");
    }
    flags = bpv6_bundle_get_reporting(primary->flags);
    if(flags & BPV6_REPORTFLAG_CUSTODY) {
        printf("* Custody reporting requested.\n");
    }
    if(flags & BPV6_REPORTFLAG_DELIVERY) {
        printf("* Delivery reporting requested.\n");
    }
    if(flags & BPV6_REPORTFLAG_DELETION) {
        printf("* Deletion reporting requested.\n");
    }
    if(flags & BPV6_REPORTFLAG_FORWARD) {
        printf("* Forward reporting requested.\n");
    }
    if(flags & BPV6_REPORTFLAG_RECEPTION) {
        printf("* Reception reporting requested.\n");
    }
    flags = bpv6_bundle_get_priority(primary->flags);
    printf("Priority: %u\n", flags);
    printf("");
    printf("Destination: ipn:%d.%d\n", (int)primary->dst_node, (int)primary->dst_svc);
    if(primary->src_node != 0) {
        printf("Source: ipn:%d.%d\n", (int)primary->src_node, (int)primary->src_svc);
    }
    else {
        printf("Source: dtn:none\n");
    }
    if(primary->custodian_node != 0) {
        printf("Custodian: ipn:%d.%d\n", (int)primary->custodian_node, (int)primary->custodian_svc);
    }
    if(primary->report_node != 0) {
        printf("Report-to: ipn:%d.%d\n", (int)primary->report_node, (int)primary->report_svc);
    }
    printf("Creation: %lu / %lu\n", primary->creation, primary->sequence);
    printf("Lifetime: %lu\n", primary->lifetime);
}

void bpv6_canonical_block_print(bpv6_canonical_block* block) {
    printf("Canonical block [type %u]\n", block->type);
    switch(block->type) {
        case BPV6_BLOCKTYPE_AUTHENTICATION:
            printf("> Authentication block\n");
            break;
        case BPV6_BLOCKTYPE_EXTENSION_SECURITY:
            printf("> Extension security block\n");
            break;
        case BPV6_BLOCKTYPE_INTEGRITY:
            printf("> Integrity block\n");
            break;
        case BPV6_BLOCKTYPE_METADATA_EXTENSION:
            printf("> Metadata block\n");
            break;
        case BPV6_BLOCKTYPE_PAYLOAD:
            printf("> Payload block\n");
            break;
        case BPV6_BLOCKTYPE_PAYLOAD_CONF:
            printf("> Payload confidentiality block\n");
            break;
        case BPV6_BLOCKTYPE_PREV_HOP_INSERTION:
            printf("> Previous hop insertion block\n");
            break;
        case BPV6_BLOCKTYPE_CUST_TRANSFER_EXT:
       	    printf("> ACS custody transfer extension block (CTEB)\n");
            break;
        case BPV6_BLOCKTYPE_BPLIB_BIB:
   	    printf("> Bplib bundle integrity block (BIB)\n");
            break;
        case BPV6_BLOCKTYPE_BUNDLE_AGE:
            printf("> Bundle age extension (BAE)\n");
            break;
        default:
            printf("> Unknown block type\n");
            break;
    }
    printf("Flags: 0x%lx\n", block->flags);
    if(block->flags & BPV6_BLOCKFLAG_LAST_BLOCK) {
        printf("* Last block in this bundle.\n");
    }
    if(block->flags & BPV6_BLOCKFLAG_DISCARD_BLOCK_FAILURE) {
        printf("* Block should be discarded upon failure to process.\n");
    }
    if(block->flags & BPV6_BLOCKFLAG_DISCARD_BUNDLE_FAILURE) {
        printf("* Bundle should be discarded upon failure to process.\n");
    }
    if(block->flags & BPV6_BLOCKFLAG_EID_REFERENCE) {
        printf("* This block references elements from the dictionary.\n");
    }
    if(block->flags & BPV6_BLOCKFLAG_FORWARD_NOPROCESS) {
        printf("* This block was forwarded without being processed.\n");
    }
    printf("Block length: %lu bytes\n", block->length);
}
