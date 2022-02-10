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
#include <boost/make_unique.hpp>

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

uint32_t Bpv6CanonicalBlock::bpv6_canonical_block_decode(const char* buffer, const size_t offset, const size_t bufsz) {
    uint64_t index = offset;
    uint8_t sdnvSize;
    uint64_t bufferSize = bufsz;
    if (bufferSize < 2) { //block type and 1-byte flag
        return 0;
    }
    m_blockTypeCode = static_cast<BPV6_BLOCK_TYPE_CODE>(buffer[index++]);
    --bufferSize;

    const uint8_t flag8bit = buffer[index];
    if (flag8bit <= 127) {
        m_blockProcessingControlFlags = static_cast<BPV6_BLOCKFLAG>(flag8bit);
        ++index;
        --bufferSize;
    }
    else {
        m_blockProcessingControlFlags = static_cast<BPV6_BLOCKFLAG>(SdnvDecodeU64((const uint8_t *)&buffer[index], &sdnvSize, bufferSize));
        if (sdnvSize == 0) {
            return 0; //return 0 on failure
        }
        index += sdnvSize;
        bufferSize -= sdnvSize;
    }

    m_blockTypeSpecificDataLength = SdnvDecodeU64((const uint8_t *)&buffer[index], &sdnvSize, bufferSize);
    if (sdnvSize == 0) {
        return 0; //return 0 on failure
    }
    index += sdnvSize;
    bufferSize -= sdnvSize;

    return static_cast<uint32_t>(index - offset);
}

uint32_t Bpv6CanonicalBlock::bpv6_canonical_block_encode(char* buffer, const size_t offset, const size_t bufsz) const {
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

    sdnvSize = SdnvEncodeU64((uint8_t *)&buffer[index], m_blockTypeSpecificDataLength);
    index += sdnvSize;

    return static_cast<uint32_t>(index - offset);
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
        serialization += SdnvEncodeU64(serialization, static_cast<uint64_t>(m_blockProcessingControlFlags));
    }

    //Block EID reference count and EID references (optional). (NOT CURRENTLY SUPPORTED)

    
    //Block data length, an unsigned integer expressed as an SDNV.  The
    //Block data length field contains the aggregate length of all
    //remaining fields of the block, i.e., the block-type-specific data
    //fields.
    serialization += SdnvEncodeU64(serialization, m_blockTypeSpecificDataLength);

    //Block-type-specific data fields, whose format and order are type-
    //specific and whose aggregate length in octets is the value of the
    //block data length field.  All multi-byte block-type-specific data
    //fields are represented in network byte order.
    if (m_blockTypeSpecificDataPtr) { //if not NULL, copy data and compute crc
        memcpy(serialization, m_blockTypeSpecificDataPtr, m_blockTypeSpecificDataLength);
    }
    //else { }//if NULL, data won't be copied (just allocated) and crc won't be computed
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
#if 0
bool Bpv6CanonicalBlock::DeserializeBpv6(std::unique_ptr<Bpv6CanonicalBlock> & canonicalPtr, const uint8_t * serialization, uint64_t & numBytesTakenToDecode,
    uint64_t bufferSize, const bool isAdminRecord)
{

    uint8_t sdnvSize;

    //Every bundle block of every type other than the primary bundle block
    //comprises the following fields, in this order:

    //Block type code, expressed as an 8-bit unsigned binary integer.
    //Bundle block type code 1 indicates that the block is a bundle
    //payload block.  Block type codes 192 through 255 are not defined
    //in this specification and are available for private and/or
    //experimental use.  All other values of the block type code are
    //reserved for future use.
    if (bufferSize < 1) {
        return false;
    }
    const BPV6_BLOCK_TYPE_CODE blockTypeCode = static_cast<BPV6_BLOCK_TYPE_CODE>(*serialization++);
    if (isAdminRecord) {
        if (blockTypeCode != BPV6_BLOCK_TYPE_CODE::PAYLOAD) { //admin records always go into a payload block
            return false;
        }
        //canonicalPtr = boost::make_unique<Bpv6AdministrativeRecord>();
    }
    else {
        switch (blockTypeCode) {
            /*
            case BPV7_BLOCK_TYPE_CODE::PREVIOUS_NODE:
                canonicalPtr = boost::make_unique<Bpv7PreviousNodeCanonicalBlock>();
                break;
            case BPV7_BLOCK_TYPE_CODE::BUNDLE_AGE:
                canonicalPtr = boost::make_unique<Bpv7BundleAgeCanonicalBlock>();
                break;
            case BPV7_BLOCK_TYPE_CODE::HOP_COUNT:
                canonicalPtr = boost::make_unique<Bpv7HopCountCanonicalBlock>();
                break;
            case BPV7_BLOCK_TYPE_CODE::INTEGRITY:
                canonicalPtr = boost::make_unique<Bpv7BlockIntegrityBlock>();
                break;
            case BPV7_BLOCK_TYPE_CODE::CONFIDENTIALITY:
                canonicalPtr = boost::make_unique<Bpv7BlockConfidentialityBlock>();
                break;
            case BPV7_BLOCK_TYPE_CODE::PAYLOAD:
            */
            default:
                canonicalPtr = boost::make_unique<Bpv6CanonicalBlock>();
                break;
        }
    }
    canonicalPtr->m_blockTypeCode = blockTypeCode;

    const uint8_t flag8bit = *serialization;
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

    m_blockTypeSpecificDataLength = SdnvDecodeU64((const uint8_t *)&buffer[index], &sdnvSize);
    if (sdnvSize == 0) {
        return 0; //return 0 on failure
    }
    index += sdnvSize;

    return static_cast<uint32_t>(index - offset);


    uint8_t cborSizeDecoded;
    const uint8_t * const serializationBase = serialization;
    if (bufferSize < Bpv7CanonicalBlock::smallestSerializedCanonicalSize) {
        return false;
    }


    //Every block other than the primary block (all such blocks are termed
    //"canonical" blocks) SHALL be represented as a CBOR array; the number
    //of elements in the array SHALL be 5 (if CRC type is zero) or 6
    //(otherwise).
    const uint8_t initialCborByte = *serialization++;
    --bufferSize;
    const uint8_t cborMajorType = initialCborByte >> 5;
    const uint8_t cborArraySize = initialCborByte & 0x1f;
    if ((cborMajorType != 4U) || //major type 4
        ((cborArraySize - 5U) > (6U - 5U))) { //additional information [5..6] (array of length [5..6])
        return false;
    }

    //The fields of every canonical block SHALL be as follows, listed in
    //the order in which they MUST appear:

    //Block type code, an unsigned integer. Bundle block type code 1
    //indicates that the block is a bundle payload block. Block type
    //codes 2 through 9 are explicitly reserved as noted later in
    //this specification.  Block type codes 192 through 255 are not
    //reserved and are available for private and/or experimental use.
    //All other block type code values are reserved for future use.
    const BPV7_BLOCK_TYPE_CODE blockTypeCode = static_cast<BPV7_BLOCK_TYPE_CODE>(CborDecodeU64(serialization, &cborSizeDecoded, bufferSize));
    if ((cborSizeDecoded == 0) || (cborSizeDecoded > 2)) { //uint8_t should be size 1 or 2 encoded bytes
        return false; //failure
    }
    serialization += cborSizeDecoded;
    bufferSize -= cborSizeDecoded;
    

    //Block number, an unsigned integer as discussed in 4.1 above.
    //Block number SHALL be represented as a CBOR unsigned integer.
    canonicalPtr->m_blockNumber = CborDecodeU64(serialization, &cborSizeDecoded, bufferSize);
    if (cborSizeDecoded == 0) {
        return false; //failure
    }
    serialization += cborSizeDecoded;
    bufferSize -= cborSizeDecoded;

    //Block processing control flags as discussed in Section 4.2.4
    //above.
    //The block processing control flags SHALL be represented as a CBOR
    //unsigned integer item, the value of which SHALL be processed as a
    //bit field indicating the control flag values as follows (note that
    //bit numbering in this instance is reversed from the usual practice,
    //beginning with the low-order bit instead of the high-order bit, for
    //agreement with the bit numbering of the bundle processing control
    //flags):
    canonicalPtr->m_blockProcessingControlFlags = static_cast<BPV7_BLOCKFLAG>(CborDecodeU64(serialization, &cborSizeDecoded, bufferSize));
    if (cborSizeDecoded == 0) {
        return false; //failure
    }
    serialization += cborSizeDecoded;
    bufferSize -= cborSizeDecoded;

    //CRC type as discussed in Section 4.2.1 above.
    //CRC type is an unsigned integer type code for which the following
    //values (and no others) are valid:
    //
    //   0 indicates "no CRC is present."
    //   1 indicates "a standard X-25 CRC-16 is present." [CRC16]
    //   2 indicates "a standard CRC32C (Castagnoli) CRC-32 is present."
    //     [RFC4960]
    //
    //CRC type SHALL be represented as a CBOR unsigned integer.
    //(cbor uint's < 24 are the value itself)
    if (bufferSize < 2) { //for crcType and [potentialTag24 or byteStringHeader]
        return false;
    }
    canonicalPtr->m_crcType = static_cast<BPV7_CRC_TYPE>(*serialization++);
    --bufferSize;

    //verify cbor array size
    const bool hasCrc = (canonicalPtr->m_crcType != BPV7_CRC_TYPE::NONE);
    const uint8_t expectedCborArraySize = 5 + hasCrc;
    if (expectedCborArraySize != cborArraySize) {
        return false; //failure
    }

    //Block-type-specific data represented as a single definite-
    //length CBOR byte string, i.e., a CBOR byte string that is not
    //of indefinite length.  For each type of block, the block-type-
    //specific data byte string is the serialization, in a block-
    //type-specific manner, of the data conveyed by that type of
    //block; definitions of blocks are required to define the manner
    //in which block-type-specific data are serialized within the
    //block-type-specific data field. For the Payload Block in
    //particular (block type 1), the block-type-specific data field,
    //termed the "payload", SHALL be an application data unit, or
    //some contiguous extent thereof, represented as a definite-
    //length CBOR byte string.
    //block-type-specific-data = bstr / #6.24(bstr)
    //; Actual CBOR data embedded in a bytestring, with optional tag to
    //indicate so.
    //; Additional plain bstr allows ciphertext data.
    //3.4.5.1.  Encoded CBOR Data Item
    //Sometimes it is beneficial to carry an embedded CBOR data item that
    //is not meant to be decoded immediately at the time the enclosing data
    //item is being decoded.  Tag number 24 (CBOR data item) can be used to
    //tag the embedded byte string as a single data item encoded in CBOR
    //format.  Contained items that aren't byte strings are invalid.  A
    //contained byte string is valid if it encodes a well-formed CBOR data
    //item; validity checking of the decoded CBOR item is not required for
    //tag validity (but could be offered by a generic decoder as a special
    //option).
    const uint8_t potentialTag24 = *serialization;
    if (potentialTag24 == ((6U << 5) | 24U)) { //major type 6, additional information 24 (Tag number 24 (CBOR data item))
        ++serialization;
        --bufferSize;
        if (bufferSize < 1) { //forbyteStringHeader
            return false;
        }
    }
    uint8_t * const byteStringHeaderStartPtr = serialization; //buffer size verified above
    const uint8_t cborMajorTypeByteString = (*byteStringHeaderStartPtr) >> 5;
    if (cborMajorTypeByteString != 2) {
        return false; //failure
    }
    *byteStringHeaderStartPtr &= 0x1f; //temporarily zero out major type to 0 to make it unsigned integer
    canonicalPtr->m_dataLength = CborDecodeU64(byteStringHeaderStartPtr, &cborSizeDecoded, bufferSize);
    *byteStringHeaderStartPtr |= (2U << 5); // restore to major type to 2 (change from major type 0 (unsigned integer) to major type 2 (byte string))
    if (cborSizeDecoded == 0) {
        return false; //failure
    }
    serialization += cborSizeDecoded;
    bufferSize -= cborSizeDecoded;

    if (canonicalPtr->m_dataLength > bufferSize) {
        return false;
    }
    canonicalPtr->m_dataPtr = serialization;
    serialization += canonicalPtr->m_dataLength;


    if (hasCrc) {
        //If and only if the value of the CRC type field of this block is
        //non-zero, a CRC. If present, the length and nature of the CRC
        //SHALL be as indicated by the CRC type and the CRC SHALL be
        //computed over the concatenation of all bytes of the block
        //(including CBOR "break" characters) including the CRC field
        //itself, which for this purpose SHALL be temporarily populated
        //with all bytes set to zero.
        bufferSize -= canonicalPtr->m_dataLength; //only need to do this if hasCrc
        uint8_t * const crcStartPtr = serialization;
        if (canonicalPtr->m_crcType == BPV7_CRC_TYPE::CRC16_X25) {
            canonicalPtr->m_computedCrc32 = 0;
            if ((bufferSize < 3) || (!Bpv7Crc::DeserializeCrc16ForBpv7(serialization, &cborSizeDecoded, canonicalPtr->m_computedCrc16))) {
                return false;
            }
            serialization += 3;
            const uint64_t blockSerializedLength = serialization - serializationBase;
            numBytesTakenToDecode = blockSerializedLength;
            if (skipCrcVerify) {
                return true;
            }
            Bpv7Crc::SerializeZeroedCrc16ForBpv7(crcStartPtr);
            const uint16_t computedCrc16 = Bpv7Crc::Crc16_X25_Unaligned(serializationBase, blockSerializedLength);
            Bpv7Crc::SerializeCrc16ForBpv7(crcStartPtr, canonicalPtr->m_computedCrc16); //restore original received crc after zeroing
            if (computedCrc16 == canonicalPtr->m_computedCrc16) {
                return true;
            }
            else {
                static const boost::format fmtTemplate("Error: Bpv7CanonicalBlock deserialize Crc16_X25 mismatch: block came with crc %04x but Decode just computed %04x");
                boost::format fmt(fmtTemplate);
                fmt % canonicalPtr->m_computedCrc16 % computedCrc16;
                const std::string message(std::move(fmt.str()));
                std::cout << message << "\n";
                return false;
            }
        }
        else if (canonicalPtr->m_crcType == BPV7_CRC_TYPE::CRC32C) {
            canonicalPtr->m_computedCrc16 = 0;
            if ((bufferSize < 5) || (!Bpv7Crc::DeserializeCrc32ForBpv7(serialization, &cborSizeDecoded, canonicalPtr->m_computedCrc32))) {
                return false;
            }
            serialization += 5;
            const uint64_t blockSerializedLength = serialization - serializationBase;
            numBytesTakenToDecode = blockSerializedLength;
            if (skipCrcVerify) {
                return true;
            }
            Bpv7Crc::SerializeZeroedCrc32ForBpv7(crcStartPtr);
            const uint32_t computedCrc32 = Bpv7Crc::Crc32C_Unaligned(serializationBase, blockSerializedLength);
            Bpv7Crc::SerializeCrc32ForBpv7(crcStartPtr, canonicalPtr->m_computedCrc32); //restore original received crc after zeroing
            if (computedCrc32 == canonicalPtr->m_computedCrc32) {
                return true;
            }
            else {
                static const boost::format fmtTemplate("Error: Bpv7CanonicalBlock deserialize Crc32C mismatch: block came with crc %08x but Decode just computed %08x");
                boost::format fmt(fmtTemplate);
                fmt % canonicalPtr->m_computedCrc32 % computedCrc32;
                const std::string message(std::move(fmt.str()));
                std::cout << message << "\n";
                return false;
            }
        }
        else {
            //error
            return false;
        }
    }
    else {
        canonicalPtr->m_computedCrc32 = 0;
        canonicalPtr->m_computedCrc16 = 0;
        numBytesTakenToDecode = serialization - serializationBase;
        return true;
    }
}
#endif
bool Bpv6CanonicalBlock::Virtual_DeserializeExtensionBlockDataBpv6() {
    return true;
}


void Bpv6CanonicalBlock::bpv6_canonical_block_print() const {
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
    printf("Block length: %" PRIu64 " bytes\n", m_blockTypeSpecificDataLength);
}

void Bpv6CanonicalBlock::bpv6_block_flags_print() const {
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
