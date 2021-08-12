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

uint32_t cbhe_bpv6_primary_block_decode(bpv6_primary_block* primary, const char* buffer, const size_t offset, const size_t bufsz) {
    primary->version = buffer[offset];
    uint64_t index = offset + 1;
    uint8_t sdnvSize;

    if(primary->version != BPV6_CCSDS_VERSION) {
        return 0;
    }

    
    primary->flags = SdnvDecodeU64((const uint8_t *)&buffer[index], &sdnvSize);
    if (sdnvSize == 0) { 
        return 0; //return 0 on failure
    }
    index += sdnvSize;
    const bool isFragment = ((bpv6_bundle_get_gflags(primary->flags) & BPV6_BUNDLEFLAG_FRAGMENT) != 0);

    primary->block_length = SdnvDecodeU64((const uint8_t *)&buffer[index], &sdnvSize);
    if (sdnvSize == 0) {
        return 0; //return 0 on failure
    }
    index += sdnvSize;

    primary->dst_node = SdnvDecodeU64((const uint8_t *)&buffer[index], &sdnvSize);
    if (sdnvSize == 0) {
        return 0; //return 0 on failure
    }
    index += sdnvSize;

    primary->dst_svc = SdnvDecodeU64((const uint8_t *)&buffer[index], &sdnvSize);
    if (sdnvSize == 0) {
        return 0; //return 0 on failure
    }
    index += sdnvSize;

    primary->src_node = SdnvDecodeU64((const uint8_t *)&buffer[index], &sdnvSize);
    if (sdnvSize == 0) {
        return 0; //return 0 on failure
    }
    index += sdnvSize;

    primary->src_svc = SdnvDecodeU64((const uint8_t *)&buffer[index], &sdnvSize);
    if (sdnvSize == 0) {
        return 0; //return 0 on failure
    }
    index += sdnvSize;

    primary->report_node = SdnvDecodeU64((const uint8_t *)&buffer[index], &sdnvSize);
    if (sdnvSize == 0) {
        return 0; //return 0 on failure
    }
    index += sdnvSize;

    primary->report_svc = SdnvDecodeU64((const uint8_t *)&buffer[index], &sdnvSize);
    if (sdnvSize == 0) {
        return 0; //return 0 on failure
    }
    index += sdnvSize;

    primary->custodian_node = SdnvDecodeU64((const uint8_t *)&buffer[index], &sdnvSize);
    if (sdnvSize == 0) {
        return 0; //return 0 on failure
    }
    index += sdnvSize;

    primary->custodian_svc = SdnvDecodeU64((const uint8_t *)&buffer[index], &sdnvSize);
    if (sdnvSize == 0) {
        return 0; //return 0 on failure
    }
    index += sdnvSize;

    primary->creation = SdnvDecodeU64((const uint8_t *)&buffer[index], &sdnvSize);
    if (sdnvSize == 0) {
        return 0; //return 0 on failure
    }
    index += sdnvSize;

    primary->sequence = SdnvDecodeU64((const uint8_t *)&buffer[index], &sdnvSize);
    if (sdnvSize == 0) {
        return 0; //return 0 on failure
    }
    index += sdnvSize;

    primary->lifetime = SdnvDecodeU64((const uint8_t *)&buffer[index], &sdnvSize);
    if (sdnvSize == 0) {
        return 0; //return 0 on failure
    }
    index += sdnvSize;

    if (buffer[index] != 0) { //dictionary length (must be 1 byte zero value (1-byte sdnv's are the value itself) 
        //RFC6260
        //3.2.  Reception
        //
        //Upon receiving a bundle whose dictionary length is zero(and only in
        //this circumstance), a CBHE - conformant convergence - layer adapter :
        //
        //1.  MAY infer that the CLA from which the bundle was received is CBHE
        //    conformant.
        //
        //2.  MUST decode the primary block of the bundle in accordance with
        //    the CBHE compression convention described in Section 2.2 before
        //    delivering it to the bundle protocol agent.
        //
        //    Note that when a CLA that is not CBHE conformant receives a bundle
        //    whose dictionary length is zero, it has no choice but to pass it to
        //    the bundle agent without modification.In this case, the bundle
        //    protocol agent will be unable to dispatch the received bundle,
        //    because it will be unable to determine the destination endpoint; the
        //    bundle will be judged to be malformed.The behavior of the bundle
        //    protocol agent in this circumstance is an implementation matter.
        printf("error: cbhe bpv6 primary decode: dictionary size not 0\n");
        return 0;
    }
    index += 1;

    // Skip the entirety of the dictionary - we assume an IPN scheme
    //index += 0;

    if(isFragment) {
        primary->fragment_offset = SdnvDecodeU64((const uint8_t *)&buffer[index], &sdnvSize);
        if (sdnvSize == 0) {
            return 0; //return 0 on failure
        }
        index += sdnvSize;

        primary->data_length = SdnvDecodeU64((const uint8_t *)&buffer[index], &sdnvSize);
        if (sdnvSize == 0) {
            return 0; //return 0 on failure
        }
        index += sdnvSize;
    }
    else {
        primary->fragment_offset = 0;
        primary->data_length = 0;
    }

    return static_cast<uint32_t>(index - offset);
}

uint32_t cbhe_bpv6_primary_block_encode(const bpv6_primary_block* primary, char* buffer, const size_t offset, const size_t bufsz) {
    buffer[offset] = primary->version;
    uint64_t index = offset + 1;
    uint64_t sdnvSize;

    if(primary->version != BPV6_CCSDS_VERSION) {
        return 0;
    }

    sdnvSize = SdnvEncodeU64((uint8_t *)&buffer[index], primary->flags);
    index += sdnvSize;
    const bool isFragment = ((bpv6_bundle_get_gflags(primary->flags) & BPV6_BUNDLEFLAG_FRAGMENT) != 0);

    const uint64_t blockLengthOffset = index;
    ++index;  // we skip one byte so we can come back and write it later

    sdnvSize = SdnvEncodeU64((uint8_t *)&buffer[index], primary->dst_node);
    index += sdnvSize;
    sdnvSize = SdnvEncodeU64((uint8_t *)&buffer[index], primary->dst_svc);
    index += sdnvSize;
    sdnvSize = SdnvEncodeU64((uint8_t *)&buffer[index], primary->src_node);
    index += sdnvSize;
    sdnvSize = SdnvEncodeU64((uint8_t *)&buffer[index], primary->src_svc);
    index += sdnvSize;
    sdnvSize = SdnvEncodeU64((uint8_t *)&buffer[index], primary->report_node);
    index += sdnvSize;
    sdnvSize = SdnvEncodeU64((uint8_t *)&buffer[index], primary->report_svc);
    index += sdnvSize;
    sdnvSize = SdnvEncodeU64((uint8_t *)&buffer[index], primary->custodian_node);
    index += sdnvSize;
    sdnvSize = SdnvEncodeU64((uint8_t *)&buffer[index], primary->custodian_svc);
    index += sdnvSize;

    sdnvSize = SdnvEncodeU64((uint8_t *)&buffer[index], primary->creation);
    index += sdnvSize;
    sdnvSize = SdnvEncodeU64((uint8_t *)&buffer[index], primary->sequence);
    index += sdnvSize;
    sdnvSize = SdnvEncodeU64((uint8_t *)&buffer[index], primary->lifetime);
    index += sdnvSize;

    // encode a zero-length dictionary
    buffer[index] = 0; // 1-byte sdnv's are the value itself
    index += 1; //this is a 1-byte sdnv

    if (isFragment) {
        sdnvSize = SdnvEncodeU64((uint8_t *)&buffer[index], primary->fragment_offset);
        index += sdnvSize;
        sdnvSize = SdnvEncodeU64((uint8_t *)&buffer[index], primary->data_length);
        index += sdnvSize;
    }

    const uint64_t blockLength = index - (blockLengthOffset + 1);
    if (blockLength > 127) { // our encoding failed because our block length was too long ...
        printf("error in cbhe_bpv6_primary_block_encode: blockLength > 127\n");
        return 0;
    }
    buffer[blockLengthOffset] = static_cast<int8_t>(blockLength); // 1-byte sdnv's are the value itself
    
    return static_cast<uint32_t>(index - offset);

}

uint32_t bpv6_canonical_block_decode(bpv6_canonical_block* block, const char* buffer, const size_t offset, const size_t bufsz) {
    uint64_t index = offset;
    uint8_t sdnvSize;
    block->type = buffer[index++];

    const uint8_t flag8bit = buffer[index];
    if (flag8bit <= 127) {
        block->flags = flag8bit;
        ++index;
    }
    else {
        block->flags = SdnvDecodeU64((const uint8_t *)&buffer[index], &sdnvSize);
        if (sdnvSize == 0) {
            return 0; //return 0 on failure
        }
        index += sdnvSize;
    }

    block->length = SdnvDecodeU64((const uint8_t *)&buffer[index], &sdnvSize);
    if (sdnvSize == 0) {
        return 0; //return 0 on failure
    }
    index += sdnvSize;

    return static_cast<uint32_t>(index - offset);
}

uint32_t bpv6_canonical_block_encode(const bpv6_canonical_block* block, char* buffer, const size_t offset, const size_t bufsz) {
    uint64_t index = offset;
    uint64_t sdnvSize;
    buffer[index++] = block->type;

    if (block->flags <= 127) {
        buffer[index++] = static_cast<uint8_t>(block->flags);
    }
    else {
        sdnvSize = SdnvEncodeU64((uint8_t *)&buffer[index], block->flags);
        index += sdnvSize;
    }

    sdnvSize = SdnvEncodeU64((uint8_t *)&buffer[index], block->length);
    index += sdnvSize;

    return static_cast<uint32_t>(index - offset);
}

void bpv6_primary_block_print(bpv6_primary_block* primary) {
    printf("BPv%d / Primary block (%d bytes)\n", (int)primary->version, (int)primary->block_length);
    printf("Flags: 0x%" PRIx64 "\n", primary->flags);
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
    printf("Creation: %" PRIu64 " / %" PRIu64 "\n", primary->creation, primary->sequence);
    printf("Lifetime: %" PRIu64 "\n", primary->lifetime);
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
    printf("Flags: 0x%" PRIx64 "\n", block->flags);
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
    printf("Block length: %" PRIu64 " bytes\n", block->length);
}

cbhe_eid_t::cbhe_eid_t() :
    nodeId(0),
    serviceId(0) { } //a default constructor: X()
cbhe_eid_t::cbhe_eid_t(uint64_t paramNodeId, uint64_t paramServiceId) :
    nodeId(paramNodeId),
    serviceId(paramServiceId) { }
cbhe_eid_t::~cbhe_eid_t() { } //a destructor: ~X()
cbhe_eid_t::cbhe_eid_t(const cbhe_eid_t& o) :
    nodeId(o.nodeId),
    serviceId(o.serviceId) { } //a copy constructor: X(const X&)
cbhe_eid_t::cbhe_eid_t(cbhe_eid_t&& o) :
    nodeId(o.nodeId),
    serviceId(o.serviceId) { } //a move constructor: X(X&&)
cbhe_eid_t& cbhe_eid_t::operator=(const cbhe_eid_t& o) { //a copy assignment: operator=(const X&)
    nodeId = o.nodeId;
    serviceId = o.serviceId;
    return *this;
}
cbhe_eid_t& cbhe_eid_t::operator=(cbhe_eid_t && o) { //a move assignment: operator=(X&&)
    nodeId = o.nodeId;
    serviceId = o.serviceId;
    return *this;
}
bool cbhe_eid_t::operator==(const cbhe_eid_t & o) const {
    return (nodeId == o.nodeId) && (serviceId == o.serviceId);
}
bool cbhe_eid_t::operator!=(const cbhe_eid_t & o) const {
    return (nodeId != o.nodeId) || (serviceId != o.serviceId);
}
bool cbhe_eid_t::operator<(const cbhe_eid_t & o) const {
    if (nodeId == o.nodeId) {
        return (serviceId < o.serviceId);
    }
    return (nodeId < o.nodeId);
}


cbhe_bundle_uuid_t::cbhe_bundle_uuid_t() : 
    creationSeconds(0),
    sequence(0),
    srcNodeId(0),
    srcServiceId(0),
    fragmentOffset(0),
    dataLength(0) { } //a default constructor: X()
cbhe_bundle_uuid_t::cbhe_bundle_uuid_t(uint64_t paramCreationSeconds, uint64_t paramSequence,
    uint64_t paramSrcNodeId, uint64_t paramSrcServiceId, uint64_t paramFragmentOffset, uint64_t paramDataLength) :
    creationSeconds(paramCreationSeconds),
    sequence(paramSequence),
    srcNodeId(paramSrcNodeId),
    srcServiceId(paramSrcServiceId),
    fragmentOffset(paramFragmentOffset),
    dataLength(paramDataLength) { }
cbhe_bundle_uuid_t::cbhe_bundle_uuid_t(const bpv6_primary_block & primary) :
    creationSeconds(primary.creation),
    sequence(primary.sequence),
    srcNodeId(primary.src_node),
    srcServiceId(primary.src_svc),
    fragmentOffset(primary.fragment_offset),
    dataLength(primary.data_length) { }
cbhe_bundle_uuid_t::~cbhe_bundle_uuid_t() { } //a destructor: ~X()
cbhe_bundle_uuid_t::cbhe_bundle_uuid_t(const cbhe_bundle_uuid_t& o) : 
    creationSeconds(o.creationSeconds),
    sequence(o.sequence),
    srcNodeId(o.srcNodeId),
    srcServiceId(o.srcServiceId),
    fragmentOffset(o.fragmentOffset),
    dataLength(o.dataLength) { } //a copy constructor: X(const X&)
cbhe_bundle_uuid_t::cbhe_bundle_uuid_t(cbhe_bundle_uuid_t&& o) : 
    creationSeconds(o.creationSeconds),
    sequence(o.sequence),
    srcNodeId(o.srcNodeId),
    srcServiceId(o.srcServiceId),
    fragmentOffset(o.fragmentOffset),
    dataLength(o.dataLength) { } //a move constructor: X(X&&)
cbhe_bundle_uuid_t& cbhe_bundle_uuid_t::operator=(const cbhe_bundle_uuid_t& o) { //a copy assignment: operator=(const X&)
    creationSeconds = o.creationSeconds;
    sequence = o.sequence;
    srcNodeId = o.srcNodeId;
    srcServiceId = o.srcServiceId;
    fragmentOffset = o.fragmentOffset;
    dataLength = o.dataLength;
    return *this;
}
cbhe_bundle_uuid_t& cbhe_bundle_uuid_t::operator=(cbhe_bundle_uuid_t && o) { //a move assignment: operator=(X&&)
    creationSeconds = o.creationSeconds;
    sequence = o.sequence;
    srcNodeId = o.srcNodeId;
    srcServiceId = o.srcServiceId;
    fragmentOffset = o.fragmentOffset;
    dataLength = o.dataLength;
    return *this;
}
bool cbhe_bundle_uuid_t::operator==(const cbhe_bundle_uuid_t & o) const {
    return 
        (creationSeconds == o.creationSeconds) &&
        (sequence == o.sequence) &&
        (srcNodeId == o.srcNodeId) &&
        (srcServiceId == o.srcServiceId) &&
        (fragmentOffset == o.fragmentOffset) &&
        (dataLength == o.dataLength);
}
bool cbhe_bundle_uuid_t::operator!=(const cbhe_bundle_uuid_t & o) const {
    return 
        (creationSeconds != o.creationSeconds) ||
        (sequence != o.sequence) ||
        (srcNodeId != o.srcNodeId) ||
        (srcServiceId != o.srcServiceId) ||
        (fragmentOffset != o.fragmentOffset) ||
        (dataLength != o.dataLength);
}
bool cbhe_bundle_uuid_t::operator<(const cbhe_bundle_uuid_t & o) const {
    if (creationSeconds == o.creationSeconds) {
        if (sequence == o.sequence) {
            if (srcNodeId == o.srcNodeId) {
                if (srcServiceId == o.srcServiceId) {
                    if (fragmentOffset == o.fragmentOffset) {
                        return (dataLength < o.dataLength);
                    }
                    return (fragmentOffset < o.fragmentOffset);
                }
                return (srcServiceId < o.srcServiceId);
            }
            return (srcNodeId < o.srcNodeId);
        }
        return (sequence < o.sequence);
    }
    return (creationSeconds < o.creationSeconds);
}
