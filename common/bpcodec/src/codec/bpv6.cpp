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

void bpv6_primary_block::SetZero() {
    flags = 0;
    block_length = 0;
    creation = 0;
    sequence = 0;
    lifetime = 0;
    fragment_offset = 0;
    data_length = 0;      // 64 bytes

    // for the IPN scheme, we use NODE.SVC
    dst_node = 0;
    dst_svc = 0;
    src_node = 0;
    src_svc = 0;
    report_node = 0;
    report_svc = 0;
    custodian_node = 0;
    custodian_svc = 0;
}

uint32_t bpv6_primary_block::cbhe_bpv6_primary_block_decode(const char* buffer, const size_t offset, const size_t bufsz) {
    const uint8_t version = buffer[offset];
    uint64_t index = offset + 1;
    uint8_t sdnvSize;

    if(version != BPV6_CCSDS_VERSION) {
        return 0;
    }

    
    flags = SdnvDecodeU64((const uint8_t *)&buffer[index], &sdnvSize);
    if (sdnvSize == 0) { 
        return 0; //return 0 on failure
    }
    index += sdnvSize;
    const bool isFragment = ((bpv6_bundle_get_gflags(flags) & BPV6_BUNDLEFLAG_FRAGMENT) != 0);

    block_length = SdnvDecodeU64((const uint8_t *)&buffer[index], &sdnvSize);
    if (sdnvSize == 0) {
        return 0; //return 0 on failure
    }
    index += sdnvSize;

    dst_node = SdnvDecodeU64((const uint8_t *)&buffer[index], &sdnvSize);
    if (sdnvSize == 0) {
        return 0; //return 0 on failure
    }
    index += sdnvSize;

    dst_svc = SdnvDecodeU64((const uint8_t *)&buffer[index], &sdnvSize);
    if (sdnvSize == 0) {
        return 0; //return 0 on failure
    }
    index += sdnvSize;

    src_node = SdnvDecodeU64((const uint8_t *)&buffer[index], &sdnvSize);
    if (sdnvSize == 0) {
        return 0; //return 0 on failure
    }
    index += sdnvSize;

    src_svc = SdnvDecodeU64((const uint8_t *)&buffer[index], &sdnvSize);
    if (sdnvSize == 0) {
        return 0; //return 0 on failure
    }
    index += sdnvSize;

    report_node = SdnvDecodeU64((const uint8_t *)&buffer[index], &sdnvSize);
    if (sdnvSize == 0) {
        return 0; //return 0 on failure
    }
    index += sdnvSize;

    report_svc = SdnvDecodeU64((const uint8_t *)&buffer[index], &sdnvSize);
    if (sdnvSize == 0) {
        return 0; //return 0 on failure
    }
    index += sdnvSize;

    custodian_node = SdnvDecodeU64((const uint8_t *)&buffer[index], &sdnvSize);
    if (sdnvSize == 0) {
        return 0; //return 0 on failure
    }
    index += sdnvSize;

    custodian_svc = SdnvDecodeU64((const uint8_t *)&buffer[index], &sdnvSize);
    if (sdnvSize == 0) {
        return 0; //return 0 on failure
    }
    index += sdnvSize;

    creation = SdnvDecodeU64((const uint8_t *)&buffer[index], &sdnvSize);
    if (sdnvSize == 0) {
        return 0; //return 0 on failure
    }
    index += sdnvSize;

    sequence = SdnvDecodeU64((const uint8_t *)&buffer[index], &sdnvSize);
    if (sdnvSize == 0) {
        return 0; //return 0 on failure
    }
    index += sdnvSize;

    lifetime = SdnvDecodeU64((const uint8_t *)&buffer[index], &sdnvSize);
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
        fragment_offset = SdnvDecodeU64((const uint8_t *)&buffer[index], &sdnvSize);
        if (sdnvSize == 0) {
            return 0; //return 0 on failure
        }
        index += sdnvSize;

        data_length = SdnvDecodeU64((const uint8_t *)&buffer[index], &sdnvSize);
        if (sdnvSize == 0) {
            return 0; //return 0 on failure
        }
        index += sdnvSize;
    }
    else {
        fragment_offset = 0;
        data_length = 0;
    }

    return static_cast<uint32_t>(index - offset);
}

uint32_t bpv6_primary_block::cbhe_bpv6_primary_block_encode(char* buffer, const size_t offset, const size_t bufsz) const {
    buffer[offset] = BPV6_CCSDS_VERSION;
    uint64_t index = offset + 1;
    uint64_t sdnvSize;

    sdnvSize = SdnvEncodeU64((uint8_t *)&buffer[index], flags);
    index += sdnvSize;
    const bool isFragment = ((bpv6_bundle_get_gflags(flags) & BPV6_BUNDLEFLAG_FRAGMENT) != 0);

    const uint64_t blockLengthOffset = index;
    ++index;  // we skip one byte so we can come back and write it later

    sdnvSize = SdnvEncodeU64((uint8_t *)&buffer[index], dst_node);
    index += sdnvSize;
    sdnvSize = SdnvEncodeU64((uint8_t *)&buffer[index], dst_svc);
    index += sdnvSize;
    sdnvSize = SdnvEncodeU64((uint8_t *)&buffer[index], src_node);
    index += sdnvSize;
    sdnvSize = SdnvEncodeU64((uint8_t *)&buffer[index], src_svc);
    index += sdnvSize;
    sdnvSize = SdnvEncodeU64((uint8_t *)&buffer[index], report_node);
    index += sdnvSize;
    sdnvSize = SdnvEncodeU64((uint8_t *)&buffer[index], report_svc);
    index += sdnvSize;
    sdnvSize = SdnvEncodeU64((uint8_t *)&buffer[index], custodian_node);
    index += sdnvSize;
    sdnvSize = SdnvEncodeU64((uint8_t *)&buffer[index], custodian_svc);
    index += sdnvSize;

    sdnvSize = SdnvEncodeU64((uint8_t *)&buffer[index], creation);
    index += sdnvSize;
    sdnvSize = SdnvEncodeU64((uint8_t *)&buffer[index], sequence);
    index += sdnvSize;
    sdnvSize = SdnvEncodeU64((uint8_t *)&buffer[index], lifetime);
    index += sdnvSize;

    // encode a zero-length dictionary
    buffer[index] = 0; // 1-byte sdnv's are the value itself
    index += 1; //this is a 1-byte sdnv

    if (isFragment) {
        sdnvSize = SdnvEncodeU64((uint8_t *)&buffer[index], fragment_offset);
        index += sdnvSize;
        sdnvSize = SdnvEncodeU64((uint8_t *)&buffer[index], data_length);
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

void bpv6_canonical_block::SetZero() {
    flags = 0;
    length = 0;
    type = 0;
}

uint32_t bpv6_canonical_block::bpv6_canonical_block_decode(const char* buffer, const size_t offset, const size_t bufsz) {
    uint64_t index = offset;
    uint8_t sdnvSize;
    type = buffer[index++];

    const uint8_t flag8bit = buffer[index];
    if (flag8bit <= 127) {
        flags = flag8bit;
        ++index;
    }
    else {
        flags = SdnvDecodeU64((const uint8_t *)&buffer[index], &sdnvSize);
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
    buffer[index++] = type;

    if (flags <= 127) {
        buffer[index++] = static_cast<uint8_t>(flags);
    }
    else {
        sdnvSize = SdnvEncodeU64((uint8_t *)&buffer[index], flags);
        index += sdnvSize;
    }

    sdnvSize = SdnvEncodeU64((uint8_t *)&buffer[index], length);
    index += sdnvSize;

    return static_cast<uint32_t>(index - offset);
}

void bpv6_primary_block::bpv6_primary_block_print() const {
    printf("BPv6 / Primary block (%d bytes)\n", (int)block_length);
    printf("Flags: 0x%" PRIx64 "\n", flags);
    uint32_t flags = bpv6_bundle_get_gflags(flags);
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
    flags = bpv6_bundle_get_reporting(flags);
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
    flags = bpv6_bundle_get_priority(flags);
    printf("Priority: %u\n", flags);

    printf("Destination: ipn:%d.%d\n", (int)dst_node, (int)dst_svc);
    if(src_node != 0) {
        printf("Source: ipn:%d.%d\n", (int)src_node, (int)src_svc);
    }
    else {
        printf("Source: dtn:none\n");
    }
    if(custodian_node != 0) {
        printf("Custodian: ipn:%d.%d\n", (int)custodian_node, (int)custodian_svc);
    }
    if(report_node != 0) {
        printf("Report-to: ipn:%d.%d\n", (int)report_node, (int)report_svc);
    }
    printf("Creation: %" PRIu64 " / %" PRIu64 "\n", creation, sequence);
    printf("Lifetime: %" PRIu64 "\n", lifetime);
}

void bpv6_canonical_block::bpv6_canonical_block_print() const {
    printf("Canonical block [type %u]\n", type);
    switch(type) {
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
    bpv6_block_flags_print();
    printf("Block length: %" PRIu64 " bytes\n", length);
}

void bpv6_canonical_block::bpv6_block_flags_print() const {
    printf("Flags: 0x%" PRIx64 "\n", flags);
    if (flags & BPV6_BLOCKFLAG_LAST_BLOCK) {
        printf("* Last block in this bundle.\n");
    }
    if (flags & BPV6_BLOCKFLAG_DISCARD_BLOCK_FAILURE) {
        printf("* Block should be discarded upon failure to process.\n");
    }
    if (flags & BPV6_BLOCKFLAG_DISCARD_BUNDLE_FAILURE) {
        printf("* Bundle should be discarded upon failure to process.\n");
    }
    if (flags & BPV6_BLOCKFLAG_EID_REFERENCE) {
        printf("* This block references elements from the dictionary.\n");
    }
    if (flags & BPV6_BLOCKFLAG_FORWARD_NOPROCESS) {
        printf("* This block was forwarded without being processed.\n");
    }

}

bool bpv6_primary_block::HasCustodyFlagSet() const {
    return (flags & BPV6_BUNDLEFLAG_CUSTODY);
}
bool bpv6_primary_block::HasFragmentationFlagSet() const {
    return (flags & BPV6_BUNDLEFLAG_FRAGMENT);
}

cbhe_bundle_uuid_t bpv6_primary_block::GetCbheBundleUuidFromPrimary() const {
    cbhe_bundle_uuid_t uuid;
    uuid.creationSeconds = creation;
    uuid.sequence = sequence;
    uuid.srcEid.Set(src_node, src_svc);
    uuid.fragmentOffset = fragment_offset;
    uuid.dataLength = data_length;
    return uuid;
}
cbhe_bundle_uuid_nofragment_t bpv6_primary_block::GetCbheBundleUuidNoFragmentFromPrimary() const {
    cbhe_bundle_uuid_nofragment_t uuid;
    uuid.creationSeconds = creation;
    uuid.sequence = sequence;
    uuid.srcEid.Set(src_node, src_svc);
    return uuid;
}
