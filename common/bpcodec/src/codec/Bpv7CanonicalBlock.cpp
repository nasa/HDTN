/***************************************************************************
 * NASA Glenn Research Center, Cleveland, OH
 * Released under the NASA Open Source Agreement (NOSA)
 * May  2021
 *
 ****************************************************************************
 */
#include "codec/bpv7.h"
#include "codec/Bpv7Crc.h"
#include "CborUint.h"
#include <boost/format.hpp>
#include <boost/make_unique.hpp>

Bpv7CanonicalBlock::Bpv7CanonicalBlock() { } //a default constructor: X() //don't initialize anything for efficiency, use SetZero if required
Bpv7CanonicalBlock::~Bpv7CanonicalBlock() { } //a destructor: ~X()
Bpv7CanonicalBlock::Bpv7CanonicalBlock(const Bpv7CanonicalBlock& o) :
    m_blockNumber(o.m_blockNumber),
    m_blockProcessingControlFlags(o.m_blockProcessingControlFlags),
    m_dataPtr(o.m_dataPtr),
    m_dataLength(o.m_dataLength),
    m_computedCrc32(o.m_computedCrc32),
    m_computedCrc16(o.m_computedCrc16),
    m_blockTypeCode(o.m_blockTypeCode),
    m_crcType(o.m_crcType) { } //a copy constructor: X(const X&)
Bpv7CanonicalBlock::Bpv7CanonicalBlock(Bpv7CanonicalBlock&& o) :
    m_blockNumber(o.m_blockNumber),
    m_blockProcessingControlFlags(o.m_blockProcessingControlFlags),
    m_dataPtr(o.m_dataPtr),
    m_dataLength(o.m_dataLength),
    m_computedCrc32(o.m_computedCrc32),
    m_computedCrc16(o.m_computedCrc16),
    m_blockTypeCode(o.m_blockTypeCode),
    m_crcType(o.m_crcType) { } //a move constructor: X(X&&)
Bpv7CanonicalBlock& Bpv7CanonicalBlock::operator=(const Bpv7CanonicalBlock& o) { //a copy assignment: operator=(const X&)
    m_blockNumber = o.m_blockNumber;
    m_blockProcessingControlFlags = o.m_blockProcessingControlFlags;
    m_dataPtr = o.m_dataPtr;
    m_dataLength = o.m_dataLength;
    m_computedCrc32 = o.m_computedCrc32;
    m_computedCrc16 = o.m_computedCrc16;
    m_blockTypeCode = o.m_blockTypeCode;
    m_crcType = o.m_crcType;
    return *this;
}
Bpv7CanonicalBlock& Bpv7CanonicalBlock::operator=(Bpv7CanonicalBlock && o) { //a move assignment: operator=(X&&)
    m_blockNumber = o.m_blockNumber;
    m_blockProcessingControlFlags = o.m_blockProcessingControlFlags;
    m_dataPtr = o.m_dataPtr;
    m_dataLength = o.m_dataLength;
    m_computedCrc32 = o.m_computedCrc32;
    m_computedCrc16 = o.m_computedCrc16;
    m_blockTypeCode = o.m_blockTypeCode;
    m_crcType = o.m_crcType;
    return *this;
}
bool Bpv7CanonicalBlock::operator==(const Bpv7CanonicalBlock & o) const {
    const bool initialValue = (m_blockNumber == o.m_blockNumber)
        && (m_blockProcessingControlFlags == o.m_blockProcessingControlFlags)
        //&& (m_dataPtr == o.m_dataPtr)
        && (m_dataLength == o.m_dataLength)        
        && (m_computedCrc32 == o.m_computedCrc32)
        && (m_computedCrc16 == o.m_computedCrc16)
        && (m_blockTypeCode == o.m_blockTypeCode)
        && (m_crcType == o.m_crcType);
    if (!initialValue) {
        return false;
    }
    if ((m_dataPtr == NULL) && (o.m_dataPtr == NULL)) {
        return true;
    }
    else if ((m_dataPtr != NULL) && (o.m_dataPtr != NULL)) {
        return (memcmp(m_dataPtr, o.m_dataPtr, m_dataLength) == 0);
    }
    else {
        return false;
    }
}
bool Bpv7CanonicalBlock::operator!=(const Bpv7CanonicalBlock & o) const {
    return !(*this == o);
}
void Bpv7CanonicalBlock::SetZero() {
    //memset(this, 0, sizeof(*this)); //bad, clears vtable ptr
    m_blockNumber = 0;
    m_blockProcessingControlFlags = BPV7_BLOCKFLAG::NO_FLAGS_SET;
    m_dataPtr = 0;
    m_dataLength = 0;
    m_computedCrc32 = 0;
    m_computedCrc16 = 0;
    m_blockTypeCode = 0;
    m_crcType = 0;
}



uint64_t Bpv7CanonicalBlock::SerializeBpv7(uint8_t * serialization) {
    uint8_t * const serializationBase = serialization;

    //Every block other than the primary block (all such blocks are termed
    //"canonical" blocks) SHALL be represented as a CBOR array; the number
    //of elements in the array SHALL be 5 (if CRC type is zero) or 6
    //(otherwise).
    const bool hasCrc = (m_crcType != 0);
    const uint8_t cborArraySize = 5 + hasCrc;
    *serialization++ = (4U << 5) | cborArraySize; //major type 4, additional information [5..6]
    
    //The fields of every canonical block SHALL be as follows, listed in
    //the order in which they MUST appear:

    //Block type code, an unsigned integer. Bundle block type code 1
    //indicates that the block is a bundle payload block. Block type
    //codes 2 through 9 are explicitly reserved as noted later in
    //this specification.  Block type codes 192 through 255 are not
    //reserved and are available for private and/or experimental use.
    //All other block type code values are reserved for future use.
    serialization += CborEncodeU64BufSize9(serialization, m_blockTypeCode);

    //Block number, an unsigned integer as discussed in 4.1 above.
    //Block number SHALL be represented as a CBOR unsigned integer.
    serialization += CborEncodeU64BufSize9(serialization, m_blockNumber);

    //Block processing control flags as discussed in Section 4.2.4
    //above.
    //The block processing control flags SHALL be represented as a CBOR
    //unsigned integer item, the value of which SHALL be processed as a
    //bit field indicating the control flag values as follows (note that
    //bit numbering in this instance is reversed from the usual practice,
    //beginning with the low-order bit instead of the high-order bit, for
    //agreement with the bit numbering of the bundle processing control
    //flags):
    serialization += CborEncodeU64BufSize9(serialization, static_cast<uint64_t>(m_blockProcessingControlFlags));

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
    *serialization++ = m_crcType;
    
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
    uint8_t * const byteStringHeaderStartPtr = serialization;
    serialization += CborEncodeU64BufSize9(serialization, m_dataLength);
    *byteStringHeaderStartPtr |= (2U << 5); //change from major type 0 (unsigned integer) to major type 2 (byte string)
    uint8_t * const byteStringAllocatedDataStartPtr = serialization;

    bool doCrcComputation = false;
    if (m_dataPtr) { //if not NULL, copy data and compute crc
        memcpy(byteStringAllocatedDataStartPtr, m_dataPtr, m_dataLength);
        doCrcComputation = true;
    }
    //else { }//if NULL, data won't be copied (just allocated) and crc won't be computed
    m_dataPtr = byteStringAllocatedDataStartPtr;  //m_dataPtr now points to new allocated or copied data within the block

    serialization += m_dataLength; //todo safety check on data length

    if (hasCrc) {
        //If and only if the value of the CRC type field of this block is
        //non-zero, a CRC. If present, the length and nature of the CRC
        //SHALL be as indicated by the CRC type and the CRC SHALL be
        //computed over the concatenation of all bytes of the block
        //(including CBOR "break" characters) including the CRC field
        //itself, which for this purpose SHALL be temporarily populated
        //with all bytes set to zero.
        uint8_t * const crcStartPtr = serialization;
        if (m_crcType == BPV7_CRC_TYPE_CRC16_X25) {
            serialization += Bpv7Crc::SerializeZeroedCrc16ForBpv7(serialization);
            const uint64_t blockSerializedLength = serialization - serializationBase;
            if (doCrcComputation) {
                m_computedCrc32 = 0;
                m_computedCrc16 = Bpv7Crc::Crc16_X25_Unaligned(serializationBase, blockSerializedLength);
                Bpv7Crc::SerializeCrc16ForBpv7(crcStartPtr, m_computedCrc16);
            }
            return blockSerializedLength;
        }
        else if (m_crcType == BPV7_CRC_TYPE_CRC32C) {
            serialization += Bpv7Crc::SerializeZeroedCrc32ForBpv7(serialization);
            const uint64_t blockSerializedLength = serialization - serializationBase;
            if (doCrcComputation) {
                m_computedCrc16 = 0;
                m_computedCrc32 = Bpv7Crc::Crc32C_Unaligned(serializationBase, blockSerializedLength);
                Bpv7Crc::SerializeCrc32ForBpv7(crcStartPtr, m_computedCrc32);
            }
            return blockSerializedLength;
        }
        else {
            //error
            return 0;
        }
    }
    else {
        m_computedCrc32 = 0;
        m_computedCrc16 = 0;
        return serialization - serializationBase;
    }
}

uint64_t Bpv7CanonicalBlock::GetSerializationSize() const {
    return Bpv7CanonicalBlock::GetSerializationSize(m_dataLength);
}
uint64_t Bpv7CanonicalBlock::GetSerializationSize(const uint64_t dataLength) const {
    uint64_t serializationSize = 2; //cbor byte (major type 4, additional information [5..6]) plus crcType
    serializationSize += CborGetEncodingSizeU64(m_blockTypeCode);
    serializationSize += CborGetEncodingSizeU64(m_blockNumber);
    serializationSize += CborGetEncodingSizeU64(static_cast<uint64_t>(m_blockProcessingControlFlags));
    serializationSize += CborGetEncodingSizeU64(dataLength);
    serializationSize += dataLength; //todo safety check on data length
    static const uint8_t CRC_TYPE_TO_SIZE[4] = { 0,3,5,0 };
    serializationSize += CRC_TYPE_TO_SIZE[m_crcType & 3];
    return serializationSize;
}

void Bpv7CanonicalBlock::RecomputeCrcAfterDataModification(uint8_t * serializationBase, const uint64_t sizeSerialized) {

    if (m_crcType == BPV7_CRC_TYPE_CRC16_X25) {
        uint8_t * const crcStartPtr = serializationBase + (sizeSerialized - 3U);
        Bpv7Crc::SerializeZeroedCrc16ForBpv7(crcStartPtr);
        m_computedCrc32 = 0;
        m_computedCrc16 = Bpv7Crc::Crc16_X25_Unaligned(serializationBase, sizeSerialized);
        Bpv7Crc::SerializeCrc16ForBpv7(crcStartPtr, m_computedCrc16);
    }
    else if (m_crcType == BPV7_CRC_TYPE_CRC32C) {
        uint8_t * const crcStartPtr = serializationBase + (sizeSerialized - 5U);
        Bpv7Crc::SerializeZeroedCrc32ForBpv7(crcStartPtr);
        m_computedCrc16 = 0;
        m_computedCrc32 = Bpv7Crc::Crc32C_Unaligned(serializationBase, sizeSerialized);
        Bpv7Crc::SerializeCrc32ForBpv7(crcStartPtr, m_computedCrc32);
    }
}

//serialization must be temporarily modifyable to zero crc and restore it
bool Bpv7CanonicalBlock::DeserializeBpv7(std::unique_ptr<Bpv7CanonicalBlock> & canonicalPtr, uint8_t * serialization, uint64_t & numBytesTakenToDecode, uint64_t bufferSize, const bool skipCrcVerify) {
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
    const uint8_t blockTypeCode = static_cast<uint8_t>(CborDecodeU64(serialization, &cborSizeDecoded, bufferSize));
    if ((cborSizeDecoded == 0) || (cborSizeDecoded > 2)) { //uint8_t should be size 1 or 2 encoded bytes
        return false; //failure
    }
    serialization += cborSizeDecoded;
    bufferSize -= cborSizeDecoded;

    switch (blockTypeCode) {
        case BPV7_BLOCKTYPE_PREVIOUS_NODE:
            canonicalPtr = boost::make_unique<Bpv7PreviousNodeCanonicalBlock>();
            break;
        case BPV7_BLOCKTYPE_BUNDLE_AGE:
            canonicalPtr = boost::make_unique<Bpv7BundleAgeCanonicalBlock>();
            break;
        case BPV7_BLOCKTYPE_HOP_COUNT:
            canonicalPtr = boost::make_unique<Bpv7HopCountCanonicalBlock>();
            break;
        case BPV7_BLOCKTYPE_BLOCK_INTEGRITY:
            canonicalPtr = boost::make_unique<Bpv7BlockIntegrityBlock>();
            break;
        case BPV7_BLOCKTYPE_BLOCK_CONFIDENTIALITY:
            canonicalPtr = boost::make_unique<Bpv7BlockConfidentialityBlock>();
            break;
        //case BPV7_BLOCKTYPE_PAYLOAD:
        default:
            canonicalPtr = boost::make_unique<Bpv7CanonicalBlock>();
            break;
    }
    canonicalPtr->m_blockTypeCode = blockTypeCode;

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
    if (bufferSize < 2) { //for crcType and byteStringHeader
        return false;
    }
    canonicalPtr->m_crcType = *serialization++;
    --bufferSize;

    //verify cbor array size
    const bool hasCrc = (canonicalPtr->m_crcType != 0);
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
        if (canonicalPtr->m_crcType == BPV7_CRC_TYPE_CRC16_X25) {
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
        else if (canonicalPtr->m_crcType == BPV7_CRC_TYPE_CRC32C) {
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

bool Bpv7CanonicalBlock::Virtual_DeserializeExtensionBlockDataBpv7() {
    return true;
}
