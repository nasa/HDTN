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

void bpv6_primary_block::SetZero() {
    flags = 0;
    block_length = 0;
    creation = 0;
    sequence = 0;
    lifetime = 0;
    fragment_offset = 0;
    data_length = 0;      // 64 bytes

    // for the IPN scheme, we use NODE.SVC
    m_destinationEid.SetZero();
    m_sourceNodeId.SetZero();
    m_reportToEid.SetZero();
    m_custodianEid.SetZero();
}

bool bpv6_primary_block::DeserializeBpv6(uint8_t * serialization, uint64_t & numBytesTakenToDecode, uint64_t bufferSize) {
    uint8_t sdnvSize;
    const uint8_t * const serializationBase = serialization;

    if (bufferSize < (SDNV_DECODE_MINIMUM_SAFE_BUFFER_SIZE + 1)) { //version plus flags
        return false;
    }
    const uint8_t version = *serialization++;
    --bufferSize;
    if(version != BPV6_CCSDS_VERSION) {
        return false;
    }
    flags = SdnvDecodeU64(serialization, &sdnvSize);
    if (sdnvSize == 0) {
        return false;
    }
    serialization += sdnvSize;
    bufferSize -= sdnvSize;
    const bool isFragment = ((flags & BPV6_BUNDLEFLAG_FRAGMENT) != 0);

    if (bufferSize < SDNV_DECODE_MINIMUM_SAFE_BUFFER_SIZE) {
        return false;
    }
    block_length = SdnvDecodeU64(serialization, &sdnvSize);
    if (sdnvSize == 0) {
        return false;
    }
    serialization += sdnvSize;
    bufferSize -= sdnvSize;

    if (!m_destinationEid.DeserializeBpv6(serialization, &sdnvSize, bufferSize)) { // sdnvSize will never be 0 if function returns true
        return false; //failure
    }
    serialization += sdnvSize;
    bufferSize -= sdnvSize;

    if (!m_sourceNodeId.DeserializeBpv6(serialization, &sdnvSize, bufferSize)) { // sdnvSize will never be 0 if function returns true
        return false; //failure
    }
    serialization += sdnvSize;
    bufferSize -= sdnvSize;

    if (!m_reportToEid.DeserializeBpv6(serialization, &sdnvSize, bufferSize)) { // sdnvSize will never be 0 if function returns true
        return false; //failure
    }
    serialization += sdnvSize;
    bufferSize -= sdnvSize;

    
    if (!m_custodianEid.DeserializeBpv6(serialization, &sdnvSize, bufferSize)) { // sdnvSize will never be 0 if function returns true
        return false; //failure
    }
    serialization += sdnvSize;
    bufferSize -= sdnvSize;

    if (bufferSize < SDNV_DECODE_MINIMUM_SAFE_BUFFER_SIZE) {
        return false;
    }
    creation = SdnvDecodeU64(serialization, &sdnvSize);
    if (sdnvSize == 0) {
        return false;
    }
    serialization += sdnvSize;
    bufferSize -= sdnvSize;

    if (bufferSize < SDNV_DECODE_MINIMUM_SAFE_BUFFER_SIZE) {
        return false;
    }
    sequence = SdnvDecodeU64(serialization, &sdnvSize);
    if (sdnvSize == 0) {
        return false;
    }
    serialization += sdnvSize;
    bufferSize -= sdnvSize;

    if (bufferSize < SDNV_DECODE_MINIMUM_SAFE_BUFFER_SIZE) {
        return false;
    }
    lifetime = SdnvDecodeU64(serialization, &sdnvSize);
    if (sdnvSize == 0) {
        return false;
    }
    serialization += sdnvSize;
    bufferSize -= sdnvSize;

    if (bufferSize == 0) { //dictionary length
        return false;
    }
    const uint8_t dictionaryLength = *serialization++;
    --bufferSize;
    if (dictionaryLength != 0) { //dictionary length (must be 1 byte zero value (1-byte sdnv's are the value itself) 
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
        return false;
    }

    // Skip the entirety of the dictionary - we assume an IPN scheme

    if(isFragment) {
        if (bufferSize < SDNV_DECODE_MINIMUM_SAFE_BUFFER_SIZE) {
            return false;
        }
        fragment_offset = SdnvDecodeU64(serialization, &sdnvSize);
        if (sdnvSize == 0) {
            return false;
        }
        serialization += sdnvSize;
        bufferSize -= sdnvSize;

        if (bufferSize < SDNV_DECODE_MINIMUM_SAFE_BUFFER_SIZE) {
            return false;
        }
        data_length = SdnvDecodeU64(serialization, &sdnvSize);
        if (sdnvSize == 0) {
            return false;
        }
        serialization += sdnvSize;
        bufferSize -= sdnvSize;
    }
    else {
        fragment_offset = 0;
        data_length = 0;
    }

    numBytesTakenToDecode = serialization - serializationBase;
    return true;
}

uint64_t bpv6_primary_block::SerializeBpv6(uint8_t * serialization) const {
    uint8_t * const serializationBase = serialization;

    *serialization++ = BPV6_CCSDS_VERSION;
    serialization += SdnvEncodeU64(serialization, flags);
    const bool isFragment = ((bpv6_bundle_get_gflags(flags) & BPV6_BUNDLEFLAG_FRAGMENT) != 0);

    uint8_t * const blockLengthPtrForLater = serialization++; // we skip one byte so we can come back and write it later
    
    serialization += m_destinationEid.SerializeBpv6(serialization);
    serialization += m_sourceNodeId.SerializeBpv6(serialization);
    serialization += m_reportToEid.SerializeBpv6(serialization);
    serialization += m_custodianEid.SerializeBpv6(serialization);
    
    serialization += SdnvEncodeU64(serialization, creation);
    serialization += SdnvEncodeU64(serialization, sequence);
    serialization += SdnvEncodeU64(serialization, lifetime);
    
    // encode a zero-length dictionary
    *serialization++ = 0; // 1-byte sdnv's are the value itself

    if (isFragment) {
        serialization += SdnvEncodeU64(serialization, fragment_offset);
        serialization += SdnvEncodeU64(serialization, data_length);
    }

    const uint64_t blockLength = serialization - (blockLengthPtrForLater + 1);
    if (blockLength > 127) { // our encoding failed because our block length was too long ...
        printf("error in cbhe_bpv6_primary_block_encode: blockLength > 127\n");
        return 0;
    }
    *blockLengthPtrForLater = static_cast<uint8_t>(blockLength); // 1-byte sdnv's are the value itself
    
    return serialization - serializationBase;
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

    std::cout << "Destination: " << m_destinationEid << "\n";
    std::cout << "Source: " << m_sourceNodeId << "\n";
    std::cout << "Custodian: " << m_custodianEid << "\n";
    std::cout << "Report-to: " << m_reportToEid << "\n";

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
    uuid.srcEid = m_sourceNodeId;
    uuid.fragmentOffset = fragment_offset;
    uuid.dataLength = data_length;
    return uuid;
}
cbhe_bundle_uuid_nofragment_t bpv6_primary_block::GetCbheBundleUuidNoFragmentFromPrimary() const {
    cbhe_bundle_uuid_nofragment_t uuid;
    uuid.creationSeconds = creation;
    uuid.sequence = sequence;
    uuid.srcEid = m_sourceNodeId;
    return uuid;
}
