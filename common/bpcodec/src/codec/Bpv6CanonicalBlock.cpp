/**
 * @file Bpv6CanonicalBlock.cpp
 * @author  Brian Tomko <brian.j.tomko@nasa.gov>
 * @author  Gilbert Clark
 *
 * @copyright Copyright (c) 2021 United States Government as represented by
 * the National Aeronautics and Space Administration.
 * No copyright is claimed in the United States under Title 17, U.S.Code.
 * All Other Rights Reserved.
 *
 * @section LICENSE
 * Released under the NASA Open Source Agreement (NOSA)
 * See LICENSE.md in the source root directory for more information.
 */


#include <stdio.h>
#include <assert.h>
#include <string.h>
#include "codec/bpv6.h"
#include <inttypes.h>
#include "Sdnv.h"
#include "Logger.h"
#include <utility>
#include <iostream>
#include <boost/make_unique.hpp>

static constexpr hdtn::Logger::SubProcess subprocess = hdtn::Logger::SubProcess::none;

Bpv6CanonicalBlock::Bpv6CanonicalBlock() { } //a default constructor: X() //don't initialize anything for efficiency, use SetZero if required
Bpv6CanonicalBlock::~Bpv6CanonicalBlock() { } //a destructor: ~X()
Bpv6CanonicalBlock::Bpv6CanonicalBlock(const Bpv6CanonicalBlock& o) :
    m_blockProcessingControlFlags(o.m_blockProcessingControlFlags),
    m_blockTypeSpecificDataLength(o.m_blockTypeSpecificDataLength),
    m_blockTypeSpecificDataPtr(o.m_blockTypeSpecificDataPtr),
    m_blockTypeCode(o.m_blockTypeCode) { } //a copy constructor: X(const X&)
Bpv6CanonicalBlock::Bpv6CanonicalBlock(Bpv6CanonicalBlock&& o) :
    m_blockProcessingControlFlags(o.m_blockProcessingControlFlags),
    m_blockTypeSpecificDataLength(o.m_blockTypeSpecificDataLength),
    m_blockTypeSpecificDataPtr(o.m_blockTypeSpecificDataPtr),
    m_blockTypeCode(o.m_blockTypeCode) { } //a move constructor: X(X&&)
Bpv6CanonicalBlock& Bpv6CanonicalBlock::operator=(const Bpv6CanonicalBlock& o) { //a copy assignment: operator=(const X&)
    m_blockProcessingControlFlags = o.m_blockProcessingControlFlags;
    m_blockTypeSpecificDataLength = o.m_blockTypeSpecificDataLength;
    m_blockTypeSpecificDataPtr = o.m_blockTypeSpecificDataPtr;
    m_blockTypeCode = o.m_blockTypeCode;
    return *this;
}
Bpv6CanonicalBlock& Bpv6CanonicalBlock::operator=(Bpv6CanonicalBlock && o) { //a move assignment: operator=(X&&)
    m_blockProcessingControlFlags = o.m_blockProcessingControlFlags;
    m_blockTypeSpecificDataLength = o.m_blockTypeSpecificDataLength;
    m_blockTypeSpecificDataPtr = o.m_blockTypeSpecificDataPtr;
    m_blockTypeCode = o.m_blockTypeCode;
    return *this;
}
bool Bpv6CanonicalBlock::operator==(const Bpv6CanonicalBlock & o) const {
    const bool initialValue = (m_blockProcessingControlFlags == o.m_blockProcessingControlFlags)
        //&& (m_dataPtr == o.m_dataPtr)
        && (m_blockTypeSpecificDataLength == o.m_blockTypeSpecificDataLength)
        && (m_blockTypeCode == o.m_blockTypeCode);
    if (!initialValue) {
        return false;
    }
    if ((m_blockTypeSpecificDataPtr == NULL) && (o.m_blockTypeSpecificDataPtr == NULL)) {
        return true;
    }
    else if ((m_blockTypeSpecificDataPtr != NULL) && (o.m_blockTypeSpecificDataPtr != NULL)) {
        return (memcmp(m_blockTypeSpecificDataPtr, o.m_blockTypeSpecificDataPtr, m_blockTypeSpecificDataLength) == 0);
    }
    else {
        return false;
    }
}
bool Bpv6CanonicalBlock::operator!=(const Bpv6CanonicalBlock & o) const {
    return !(*this == o);
}
void Bpv6CanonicalBlock::SetZero() {
    m_blockProcessingControlFlags = BPV6_BLOCKFLAG::NO_FLAGS_SET;
    m_blockTypeSpecificDataLength = 0;
    m_blockTypeSpecificDataPtr = NULL;
    m_blockTypeCode = BPV6_BLOCK_TYPE_CODE::PRIMARY_IMPLICIT_ZERO;
}


uint64_t Bpv6CanonicalBlock::SerializeBpv6(uint8_t * serialization) {
    uint8_t * const serializationBase = serialization;

    
    //Every bundle block of every type other than the primary bundle block
    //comprises the following fields, in this order:

    //Block type code, expressed as an 8-bit unsigned binary integer.
    //Bundle block type code 1 indicates that the block is a bundle
    //payload block.  Block type codes 192 through 255 are not defined
    //in this specification and are available for private and/or
    //experimental use.  All other values of the block type code are
    //reserved for future use.
    *serialization++ = static_cast<uint8_t>(m_blockTypeCode);

    //Block processing control flags, an unsigned integer expressed as
    //an SDNV.  The individual bits of this integer are used to invoke
    //selected block processing control features.
    if ((static_cast<uint64_t>(m_blockProcessingControlFlags)) <= 127) { //will almost always be the predicted branch
        *serialization++ = static_cast<uint8_t>(m_blockProcessingControlFlags);
    }
    else {
        serialization += SdnvEncodeU64BufSize10(serialization, static_cast<uint64_t>(m_blockProcessingControlFlags));
    }

    //Block EID reference count and EID references (optional). (NOT CURRENTLY SUPPORTED)

    
    //Block data length, an unsigned integer expressed as an SDNV.  The
    //Block data length field contains the aggregate length of all
    //remaining fields of the block, i.e., the block-type-specific data
    //fields.
    serialization += SdnvEncodeU64BufSize10(serialization, m_blockTypeSpecificDataLength);

    //Block-type-specific data fields, whose format and order are type-
    //specific and whose aggregate length in octets is the value of the
    //block data length field.  All multi-byte block-type-specific data
    //fields are represented in network byte order.
    if (m_blockTypeSpecificDataPtr) { //if not NULL, copy data
        memcpy(serialization, m_blockTypeSpecificDataPtr, m_blockTypeSpecificDataLength);
    }
    //else { }//if NULL, data won't be copied (just allocated)
    m_blockTypeSpecificDataPtr = serialization;  //m_dataPtr now points to new allocated or copied data within the block

    serialization += m_blockTypeSpecificDataLength; //todo safety check on data length


    return serialization - serializationBase;
}

uint64_t Bpv6CanonicalBlock::GetSerializationSize() const {
    uint64_t serializationSize = 1; //block type code
    serializationSize += SdnvGetNumBytesRequiredToEncode(static_cast<uint64_t>(m_blockProcessingControlFlags));
    const uint64_t canonicalBlockTypeSpecificDataSerializationSize = GetCanonicalBlockTypeSpecificDataSerializationSize();
    serializationSize += SdnvGetNumBytesRequiredToEncode(canonicalBlockTypeSpecificDataSerializationSize);
    serializationSize += canonicalBlockTypeSpecificDataSerializationSize; //todo safety check on data length
    return serializationSize;
}

uint64_t Bpv6CanonicalBlock::GetCanonicalBlockTypeSpecificDataSerializationSize() const {
    return m_blockTypeSpecificDataLength;
}

bool Bpv6CanonicalBlock::DeserializeBpv6(std::unique_ptr<Bpv6CanonicalBlock> & canonicalPtr,
    const uint8_t * serialization, uint64_t & numBytesTakenToDecode,
    uint64_t bufferSize, const bool isAdminRecord,
    std::unique_ptr<Bpv6CanonicalBlock>* blockNumberToRecycledCanonicalBlockArray)
{
    static constexpr std::size_t MAX_NUM_BLOCK_TYPE_CODES = static_cast<std::size_t>(BPV6_BLOCK_TYPE_CODE::RESERVED_MAX_BLOCK_TYPES);
    uint8_t sdnvSize;
    const uint8_t * const serializationBase = serialization;

    //Every bundle block of every type other than the primary bundle block
    //comprises the following fields, in this order:

    //Block type code, expressed as an 8-bit unsigned binary integer.
    //Bundle block type code 1 indicates that the block is a bundle
    //payload block.  Block type codes 192 through 255 are not defined
    //in this specification and are available for private and/or
    //experimental use.  All other values of the block type code are
    //reserved for future use.
    if (bufferSize < 2) { //blockTypeCode + 1 byte of m_blockProcessingControlFlags
        return false;
    }
    const BPV6_BLOCK_TYPE_CODE blockTypeCode = static_cast<BPV6_BLOCK_TYPE_CODE>(*serialization++);
    --bufferSize;
    const std::size_t blockTypeCodeAsSizeT = static_cast<std::size_t>(blockTypeCode);
    if (blockNumberToRecycledCanonicalBlockArray
        && (blockTypeCodeAsSizeT < MAX_NUM_BLOCK_TYPE_CODES)
        && (blockNumberToRecycledCanonicalBlockArray[blockTypeCodeAsSizeT]))
    {
        //take from recycle bin (prevents allocations and deallocations)
        canonicalPtr = std::move(blockNumberToRecycledCanonicalBlockArray[blockTypeCodeAsSizeT]);
        //std::cout << "recycled block code " << blockTypeCodeAsSizeT << "\n";
    }
    else {
        switch (blockTypeCode) {
            case BPV6_BLOCK_TYPE_CODE::PREVIOUS_HOP_INSERTION:
                canonicalPtr = boost::make_unique<Bpv6PreviousHopInsertionCanonicalBlock>();
                break;
            case BPV6_BLOCK_TYPE_CODE::METADATA_EXTENSION:
                canonicalPtr = boost::make_unique<Bpv6MetadataCanonicalBlock>();
                break;
            case BPV6_BLOCK_TYPE_CODE::CUSTODY_TRANSFER_ENHANCEMENT:
                canonicalPtr = boost::make_unique<Bpv6CustodyTransferEnhancementBlock>();
                break;
            case BPV6_BLOCK_TYPE_CODE::BUNDLE_AGE:
                canonicalPtr = boost::make_unique<Bpv6BundleAgeCanonicalBlock>();
                break;
            case BPV6_BLOCK_TYPE_CODE::PAYLOAD:
                //admin records always go into a payload block
                canonicalPtr = (isAdminRecord) ? boost::make_unique<Bpv6AdministrativeRecord>() : boost::make_unique<Bpv6CanonicalBlock>();
                break;
            default:
                canonicalPtr = boost::make_unique<Bpv6CanonicalBlock>();
                break;
        }
        canonicalPtr->m_blockTypeCode = blockTypeCode;
    }
    //Block processing control flags, an unsigned integer expressed as
    //an SDNV.  The individual bits of this integer are used to invoke
    //selected block processing control features.
    const uint8_t flag8bit = *serialization;
    if (flag8bit <= 127) {
        canonicalPtr->m_blockProcessingControlFlags = static_cast<BPV6_BLOCKFLAG>(flag8bit);
        ++serialization;
        --bufferSize;
    }
    else {
        canonicalPtr->m_blockProcessingControlFlags = static_cast<BPV6_BLOCKFLAG>(SdnvDecodeU64(serialization, &sdnvSize, bufferSize));
        if (sdnvSize == 0) {
            return false; //failure
        }
        serialization += sdnvSize;
        bufferSize -= sdnvSize;
    }

    //Block EID reference count and EID references (optional). (NOT CURRENTLY SUPPORTED)


    //Block data length, an unsigned integer expressed as an SDNV.  The
    //Block data length field contains the aggregate length of all
    //remaining fields of the block, i.e., the block-type-specific data
    //fields.
    canonicalPtr->m_blockTypeSpecificDataLength = SdnvDecodeU64(serialization, &sdnvSize, bufferSize);
    if (sdnvSize == 0) {
        return false; //failure
    }
    serialization += sdnvSize;
    bufferSize -= sdnvSize;

    //Block-type-specific data fields, whose format and order are type-
    //specific and whose aggregate length in octets is the value of the
    //block data length field.  All multi-byte block-type-specific data
    //fields are represented in network byte order.
    if (canonicalPtr->m_blockTypeSpecificDataLength > bufferSize) {
        return false;
    }
    canonicalPtr->m_blockTypeSpecificDataPtr = (uint8_t*)serialization;
    serialization += canonicalPtr->m_blockTypeSpecificDataLength;

    numBytesTakenToDecode = serialization - serializationBase;
    return true;
}

bool Bpv6CanonicalBlock::Virtual_DeserializeExtensionBlockDataBpv6() {
    return true;
}


void Bpv6CanonicalBlock::bpv6_canonical_block_print() const {
    LOG_INFO(subprocess) << "Canonical block [type " << m_blockTypeCode << "]";
    switch(m_blockTypeCode) {
        case BPV6_BLOCK_TYPE_CODE::BUNDLE_AUTHENTICATION:
            LOG_INFO(subprocess) << "> Authentication block";
            break;
        case BPV6_BLOCK_TYPE_CODE::EXTENSION_SECURITY:
            LOG_INFO(subprocess) << "> Extension security block";
            break;
        case BPV6_BLOCK_TYPE_CODE::PAYLOAD_INTEGRITY:
            LOG_INFO(subprocess) << "> Integrity block";
            break;
        case BPV6_BLOCK_TYPE_CODE::METADATA_EXTENSION:
            LOG_INFO(subprocess) << "> Metadata block"   ;
            break;
        case BPV6_BLOCK_TYPE_CODE::PAYLOAD:
            LOG_INFO(subprocess) << "> Payload block";
            break;
        case BPV6_BLOCK_TYPE_CODE::PAYLOAD_CONFIDENTIALITY:
            LOG_INFO(subprocess) << "> Payload confidentiality block";
            break;
        case BPV6_BLOCK_TYPE_CODE::PREVIOUS_HOP_INSERTION:
            LOG_INFO(subprocess) << "> Previous hop insertion block";
            break;
        case BPV6_BLOCK_TYPE_CODE::CUSTODY_TRANSFER_ENHANCEMENT:
            LOG_INFO(subprocess) << "> ACS custody transfer enhancement block (CTEB)";
            break;
        case BPV6_BLOCK_TYPE_CODE::BPLIB_BIB:
            LOG_INFO(subprocess) << "> Bplib bundle integrity block (BIB)";
            break;
        case BPV6_BLOCK_TYPE_CODE::BUNDLE_AGE:
            LOG_INFO(subprocess) << "> Bundle age extension (BAE)";
            break;
        default:
            LOG_INFO(subprocess) << "> Unknown block type";
            break;
    }
    bpv6_block_flags_print();
    LOG_INFO(subprocess) << "Block length: " << m_blockTypeSpecificDataLength << " bytes";
}

void Bpv6CanonicalBlock::bpv6_block_flags_print() const {
    LOG_INFO(subprocess) << "Flags: " << m_blockProcessingControlFlags;
    if ((m_blockProcessingControlFlags & BPV6_BLOCKFLAG::MUST_BE_REPLICATED_IN_EVERY_FRAGMENT) != BPV6_BLOCKFLAG::NO_FLAGS_SET) {
        LOG_INFO(subprocess) << "* Block must be replicated in every fragment.";
    }
    if ((m_blockProcessingControlFlags & BPV6_BLOCKFLAG::STATUS_REPORT_REQUESTED_IF_BLOCK_CANT_BE_PROCESSED) != BPV6_BLOCKFLAG::NO_FLAGS_SET) {
        LOG_INFO(subprocess) << "* Transmit status report if block can't be processed.";
    }
    if ((m_blockProcessingControlFlags & BPV6_BLOCKFLAG::DELETE_BUNDLE_IF_BLOCK_CANT_BE_PROCESSED) != BPV6_BLOCKFLAG::NO_FLAGS_SET) {
        LOG_INFO(subprocess) << "* Delete bundle if block can't be processed.";
    }
    if ((m_blockProcessingControlFlags & BPV6_BLOCKFLAG::IS_LAST_BLOCK) != BPV6_BLOCKFLAG::NO_FLAGS_SET) {
        LOG_INFO(subprocess) << "* Last block in this bundle.";
    }
    if ((m_blockProcessingControlFlags & BPV6_BLOCKFLAG::DISCARD_BLOCK_IF_IT_CANT_BE_PROCESSED) != BPV6_BLOCKFLAG::NO_FLAGS_SET) {
        LOG_INFO(subprocess) << "* Discard block if it can't be processed.";
    }
    if ((m_blockProcessingControlFlags & BPV6_BLOCKFLAG::BLOCK_WAS_FORWARDED_WITHOUT_BEING_PROCESSED) != BPV6_BLOCKFLAG::NO_FLAGS_SET) {
        LOG_INFO(subprocess) << "* Block was forwarded without being processed.";
    }
    if ((m_blockProcessingControlFlags & BPV6_BLOCKFLAG::BLOCK_CONTAINS_AN_EID_REFERENCE_FIELD) != BPV6_BLOCKFLAG::NO_FLAGS_SET) {
        LOG_INFO(subprocess) << "* Block contains an EID-reference field.";
    }
}
