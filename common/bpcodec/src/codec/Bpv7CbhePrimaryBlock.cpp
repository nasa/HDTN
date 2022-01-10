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

Bpv7CbhePrimaryBlock::Bpv7CbhePrimaryBlock() { } //a default constructor: X() //don't initialize anything for efficiency, use SetZero if required
Bpv7CbhePrimaryBlock::~Bpv7CbhePrimaryBlock() { } //a destructor: ~X()
Bpv7CbhePrimaryBlock::Bpv7CbhePrimaryBlock(const Bpv7CbhePrimaryBlock& o) :
    m_bundleProcessingControlFlags(o.m_bundleProcessingControlFlags),
    m_destinationEid(o.m_destinationEid),
    m_sourceNodeId(o.m_sourceNodeId),
    m_reportToEid(o.m_reportToEid),
    m_creationTimestamp(o.m_creationTimestamp),
    m_lifetimeMilliseconds(o.m_lifetimeMilliseconds),
    m_fragmentOffset(o.m_fragmentOffset),
    m_totalApplicationDataUnitLength(o.m_totalApplicationDataUnitLength),
    m_deserializedCrc32(o.m_deserializedCrc32),
    m_deserializedCrc16(o.m_deserializedCrc16),
    m_crcType(o.m_crcType) { } //a copy constructor: X(const X&)
Bpv7CbhePrimaryBlock::Bpv7CbhePrimaryBlock(Bpv7CbhePrimaryBlock&& o) :
    m_bundleProcessingControlFlags(o.m_bundleProcessingControlFlags),
    m_destinationEid(std::move(o.m_destinationEid)),
    m_sourceNodeId(std::move(o.m_sourceNodeId)),
    m_reportToEid(std::move(o.m_reportToEid)),
    m_creationTimestamp(std::move(o.m_creationTimestamp)),
    m_lifetimeMilliseconds(o.m_lifetimeMilliseconds),
    m_fragmentOffset(o.m_fragmentOffset),
    m_totalApplicationDataUnitLength(o.m_totalApplicationDataUnitLength),
    m_deserializedCrc32(o.m_deserializedCrc32),
    m_deserializedCrc16(o.m_deserializedCrc16),
    m_crcType(o.m_crcType) { } //a move constructor: X(X&&)
Bpv7CbhePrimaryBlock& Bpv7CbhePrimaryBlock::operator=(const Bpv7CbhePrimaryBlock& o) { //a copy assignment: operator=(const X&)
    m_bundleProcessingControlFlags = o.m_bundleProcessingControlFlags;
    m_destinationEid = o.m_destinationEid;
    m_sourceNodeId = o.m_sourceNodeId;
    m_reportToEid = o.m_reportToEid;
    m_creationTimestamp = o.m_creationTimestamp;
    m_lifetimeMilliseconds = o.m_lifetimeMilliseconds;
    m_fragmentOffset = o.m_fragmentOffset;
    m_totalApplicationDataUnitLength = o.m_totalApplicationDataUnitLength;
    m_deserializedCrc32 = o.m_deserializedCrc32;
    m_deserializedCrc16 = o.m_deserializedCrc16;
    m_crcType = o.m_crcType;
    return *this;
}
Bpv7CbhePrimaryBlock& Bpv7CbhePrimaryBlock::operator=(Bpv7CbhePrimaryBlock && o) { //a move assignment: operator=(X&&)
    m_bundleProcessingControlFlags = o.m_bundleProcessingControlFlags;
    m_destinationEid = std::move(o.m_destinationEid);
    m_sourceNodeId = std::move(o.m_sourceNodeId);
    m_reportToEid = std::move(o.m_reportToEid);
    m_creationTimestamp = std::move(o.m_creationTimestamp);
    m_lifetimeMilliseconds = o.m_lifetimeMilliseconds;
    m_fragmentOffset = o.m_fragmentOffset;
    m_totalApplicationDataUnitLength = o.m_totalApplicationDataUnitLength;
    m_deserializedCrc32 = o.m_deserializedCrc32;
    m_deserializedCrc16 = o.m_deserializedCrc16;
    m_crcType = o.m_crcType;
    return *this;
}
bool Bpv7CbhePrimaryBlock::operator==(const Bpv7CbhePrimaryBlock & o) const {
    return (m_bundleProcessingControlFlags == o.m_bundleProcessingControlFlags)
        && (m_destinationEid == o.m_destinationEid)
        && (m_sourceNodeId == o.m_sourceNodeId)
        && (m_reportToEid == o.m_reportToEid)
        && (m_creationTimestamp == o.m_creationTimestamp)
        && (m_lifetimeMilliseconds == o.m_lifetimeMilliseconds)
        && (m_fragmentOffset == o.m_fragmentOffset)
        && (m_totalApplicationDataUnitLength == o.m_totalApplicationDataUnitLength)
        && (m_deserializedCrc32 == o.m_deserializedCrc32)
        && (m_deserializedCrc16 == o.m_deserializedCrc16)
        && (m_crcType == o.m_crcType);
}
bool Bpv7CbhePrimaryBlock::operator!=(const Bpv7CbhePrimaryBlock & o) const {
    return !(*this == o);
}
void Bpv7CbhePrimaryBlock::SetZero() {
    memset(this, 0, sizeof(*this));
}



uint64_t Bpv7CbhePrimaryBlock::SerializeBpv7(uint8_t * serialization) const {
    uint8_t * const serializationBase = serialization;

    //Each primary block SHALL be represented as a CBOR array; the number
    //of elements in the array SHALL be 8 (if the bundle is not a fragment
    //and the block has no CRC), 9 (if the block has a CRC and the bundle
    //is not a fragment), 10 (if the bundle is a fragment and the block
    //has no CRC), or 11 (if the bundle is a fragment and the block has a
    //CRC).
    const bool hasCrc = (m_crcType != 0);
    const bool isFragment = ((m_bundleProcessingControlFlags & BPV7_BUNDLEFLAG_ISFRAGMENT) != 0);
    const uint8_t cborArraySize = 8 + hasCrc + ((static_cast<uint8_t>(isFragment)) << 1);
    *serialization++ = (4U << 5) | cborArraySize; //major type 4, additional information [8..11]

    //The fields of the primary bundle block SHALL be as follows, listed
    //in the order in which they MUST appear:

    //Version: An unsigned integer value indicating the version of the
    //bundle protocol that constructed this block. The present document
    //describes version 7 of the bundle protocol. Version number SHALL be
    //represented as a CBOR unsigned integer item.
    //(cbor uint's < 24 are the value itself)
    *serialization++ = 7;

    //Bundle Processing Control Flags: The Bundle Processing Control Flags
    //are discussed in Section 4.2.3. above.
    //The bundle processing control flags SHALL be represented as a CBOR
    //unsigned integer item, the value of which SHALL be processed as a
    //bit field indicating the control flag values as follows (note that
    //bit numbering in this instance is reversed from the usual practice,
    //beginning with the low-order bit instead of the high-order bit, in
    //recognition of the potential definition of additional control flag
    //values in the future):
    serialization += CborEncodeU64BufSize9(serialization, m_bundleProcessingControlFlags);

    //CRC Type: CRC Type codes are discussed in Section 4.2.1. above.  The
    //CRC Type code for the primary block MAY be zero if the bundle
    //contains a BPsec [BPSEC] Block Integrity Block whose target is the
    //primary block; otherwise the CRC Type code for the primary block
    //MUST be non-zero.
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
    
    //Destination EID: The Destination EID field identifies the bundle
    //endpoint that is the bundle's destination, i.e., the endpoint that
    //contains the node(s) at which the bundle is to be delivered.
    serialization += m_destinationEid.SerializeBpv7(serialization);

    //Source node ID: The Source node ID field identifies the bundle node
    //at which the bundle was initially transmitted, except that Source
    //node ID may be the null endpoint ID in the event that the bundle's
    //source chooses to remain anonymous.
    serialization += m_sourceNodeId.SerializeBpv7(serialization);

    //Report-to EID: The Report-to EID field identifies the bundle
    //endpoint to which status reports pertaining to the forwarding and
    //delivery of this bundle are to be transmitted.
    serialization += m_reportToEid.SerializeBpv7(serialization);

    //Creation Timestamp: The creation timestamp comprises two unsigned
    //integers that, together with the source node ID and (if the bundle
    //is a fragment) the fragment offset and payload length, serve to
    //identify the bundle. See 4.2.7 above for the definition of this field.
    serialization += m_creationTimestamp.SerializeBpv7(serialization);

    //Lifetime: The lifetime field is an unsigned integer that indicates
    //the time at which the bundle's payload will no longer be useful,
    //encoded as a number of milliseconds past the creation time. (For
    //high-rate deployments with very brief disruptions, fine-grained
    //expression of bundle lifetime may be useful.)  When a bundle's age
    //exceeds its lifetime, bundle nodes need no longer retain or forward
    //the bundle; the bundle SHOULD be deleted from the network...
    //Bundle lifetime SHALL be represented as a CBOR unsigned integer item.
    serialization += CborEncodeU64BufSize9(serialization, m_lifetimeMilliseconds);

    if (isFragment) {
        //Fragment offset: If and only if the Bundle Processing Control Flags
        //of this Primary block indicate that the bundle is a fragment,
        //fragment offset SHALL be present in the primary block. Fragment
        //offset SHALL be represented as a CBOR unsigned integer indicating
        //the offset from the start of the original application data unit at
        //which the bytes comprising the payload of this bundle were located.
        serialization += CborEncodeU64BufSize9(serialization, m_fragmentOffset);

        //Total Application Data Unit Length: If and only if the Bundle
        //Processing Control Flags of this Primary block indicate that the
        //bundle is a fragment, total application data unit length SHALL be
        //present in the primary block. Total application data unit length
        //SHALL be represented as a CBOR unsigned integer indicating the total
        //length of the original application data unit of which this bundle's
        //payload is a part.
        serialization += CborEncodeU64BufSize9(serialization, m_totalApplicationDataUnitLength);
    }

    if (hasCrc) {
        //CRC: A CRC SHALL be present in the primary block unless the bundle
        //includes a BPsec [BPSEC] Block Integrity Block whose target is the
        //primary block, in which case a CRC MAY be present in the primary
        //block.  The length and nature of the CRC SHALL be as indicated by
        //the CRC type.  The CRC SHALL be computed over the concatenation of
        //all bytes (including CBOR "break" characters) of the primary block
        //including the CRC field itself, which for this purpose SHALL be
        //temporarily populated with all bytes set to zero.
        uint8_t * const crcStartPtr = serialization;
        if (m_crcType == BPV7_CRC_TYPE_CRC16_X25) {
            serialization += Bpv7Crc::SerializeZeroedCrc16ForBpv7(serialization);
            const uint64_t primaryBlockSerializedLength = serialization - serializationBase;
            uint16_t crc16 = Bpv7Crc::Crc16_X25_Unaligned(serializationBase, primaryBlockSerializedLength);
            Bpv7Crc::SerializeCrc16ForBpv7(crcStartPtr, crc16);
            return primaryBlockSerializedLength;
        }
        else if (m_crcType == BPV7_CRC_TYPE_CRC32C) {
            serialization += Bpv7Crc::SerializeZeroedCrc32ForBpv7(serialization);
            const uint64_t primaryBlockSerializedLength = serialization - serializationBase;
            uint32_t crc32 = Bpv7Crc::Crc32C_Unaligned(serializationBase, primaryBlockSerializedLength);
            Bpv7Crc::SerializeCrc32ForBpv7(crcStartPtr, crc32);
            return primaryBlockSerializedLength;
        }
        else {
            //error
            return 0;
        }
    }
    else {
        return serialization - serializationBase;
    }
}

//serialization must be temporarily modifyable to zero crc and restore it
bool Bpv7CbhePrimaryBlock::DeserializeBpv7(uint8_t * serialization, uint64_t & numBytesTakenToDecode) {
    uint8_t cborSizeDecoded;
    const uint8_t * const serializationBase = serialization;

    //Each primary block SHALL be represented as a CBOR array; the number
    //of elements in the array SHALL be 8 (if the bundle is not a fragment
    //and the block has no CRC), 9 (if the block has a CRC and the bundle
    //is not a fragment), 10 (if the bundle is a fragment and the block
    //has no CRC), or 11 (if the bundle is a fragment and the block has a
    //CRC).
    const uint8_t initialCborByte = *serialization++;
    const uint8_t cborMajorType = initialCborByte >> 5;
    const uint8_t cborArraySize = initialCborByte & 0x1f;
    if ((cborMajorType != 4U) || //major type 4
        ((cborArraySize - 8U) > (11U-8U))) { //additional information [8..11] (array of length [8..11])
        return false;
    }

    //The fields of the primary bundle block SHALL be as follows, listed
    //in the order in which they MUST appear:

    //Version: An unsigned integer value indicating the version of the
    //bundle protocol that constructed this block. The present document
    //describes version 7 of the bundle protocol. Version number SHALL be
    //represented as a CBOR unsigned integer item.
    //(cbor uint's < 24 are the value itself)
    const uint8_t bpVersion = *serialization++;
    if (bpVersion != 7) {
        return false;
    }

    //Bundle Processing Control Flags: The Bundle Processing Control Flags
    //are discussed in Section 4.2.3. above.
    //The bundle processing control flags SHALL be represented as a CBOR
    //unsigned integer item, the value of which SHALL be processed as a
    //bit field indicating the control flag values as follows (note that
    //bit numbering in this instance is reversed from the usual practice,
    //beginning with the low-order bit instead of the high-order bit, in
    //recognition of the potential definition of additional control flag
    //values in the future):
    m_bundleProcessingControlFlags = CborDecodeU64BufSize9(serialization, &cborSizeDecoded);
    if (cborSizeDecoded == 0) {
        return false; //failure
    }
    serialization += cborSizeDecoded;

    //CRC Type: CRC Type codes are discussed in Section 4.2.1. above.  The
    //CRC Type code for the primary block MAY be zero if the bundle
    //contains a BPsec [BPSEC] Block Integrity Block whose target is the
    //primary block; otherwise the CRC Type code for the primary block
    //MUST be non-zero.
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
    m_crcType = *serialization++;

    //verify cbor array size
    const bool hasCrc = (m_crcType != 0);
    const bool isFragment = ((m_bundleProcessingControlFlags & BPV7_BUNDLEFLAG_ISFRAGMENT) != 0);
    const uint8_t expectedCborArraySize = 8 + hasCrc + ((static_cast<uint8_t>(isFragment)) << 1);
    if (expectedCborArraySize != cborArraySize) {
        return false; //failure
    }

    //Destination EID: The Destination EID field identifies the bundle
    //endpoint that is the bundle's destination, i.e., the endpoint that
    //contains the node(s) at which the bundle is to be delivered.
    if(!m_destinationEid.DeserializeBpv7(serialization, &cborSizeDecoded)) { // cborSizeDecoded will never be 0 if function returns true
        return false; //failure
    }
    serialization += cborSizeDecoded;

    //Source node ID: The Source node ID field identifies the bundle node
    //at which the bundle was initially transmitted, except that Source
    //node ID may be the null endpoint ID in the event that the bundle's
    //source chooses to remain anonymous.
    if (!m_sourceNodeId.DeserializeBpv7(serialization, &cborSizeDecoded)) { // cborSizeDecoded will never be 0 if function returns true
        return false; //failure
    }
    serialization += cborSizeDecoded;

    //Report-to EID: The Report-to EID field identifies the bundle
    //endpoint to which status reports pertaining to the forwarding and
    //delivery of this bundle are to be transmitted.
    if (!m_reportToEid.DeserializeBpv7(serialization, &cborSizeDecoded)) { // cborSizeDecoded will never be 0 if function returns true
        return false; //failure
    }
    serialization += cborSizeDecoded;

    //Creation Timestamp: The creation timestamp comprises two unsigned
    //integers that, together with the source node ID and (if the bundle
    //is a fragment) the fragment offset and payload length, serve to
    //identify the bundle. See 4.2.7 above for the definition of this field.
    if (!m_creationTimestamp.DeserializeBpv7(serialization, &cborSizeDecoded)) { // cborSizeDecoded will never be 0 if function returns true
        return false; //failure
    }
    serialization += cborSizeDecoded;

    //Lifetime: The lifetime field is an unsigned integer that indicates
    //the time at which the bundle's payload will no longer be useful,
    //encoded as a number of milliseconds past the creation time. (For
    //high-rate deployments with very brief disruptions, fine-grained
    //expression of bundle lifetime may be useful.)  When a bundle's age
    //exceeds its lifetime, bundle nodes need no longer retain or forward
    //the bundle; the bundle SHOULD be deleted from the network...
    //Bundle lifetime SHALL be represented as a CBOR unsigned integer item.
    m_lifetimeMilliseconds = CborDecodeU64BufSize9(serialization, &cborSizeDecoded);
    if (cborSizeDecoded == 0) {
        return false; //failure
    }
    serialization += cborSizeDecoded;

    if (isFragment) {
        //Fragment offset: If and only if the Bundle Processing Control Flags
        //of this Primary block indicate that the bundle is a fragment,
        //fragment offset SHALL be present in the primary block. Fragment
        //offset SHALL be represented as a CBOR unsigned integer indicating
        //the offset from the start of the original application data unit at
        //which the bytes comprising the payload of this bundle were located.
        m_fragmentOffset = CborDecodeU64BufSize9(serialization, &cborSizeDecoded);
        if (cborSizeDecoded == 0) {
            return false; //failure
        }
        serialization += cborSizeDecoded;

        //Total Application Data Unit Length: If and only if the Bundle
        //Processing Control Flags of this Primary block indicate that the
        //bundle is a fragment, total application data unit length SHALL be
        //present in the primary block. Total application data unit length
        //SHALL be represented as a CBOR unsigned integer indicating the total
        //length of the original application data unit of which this bundle's
        //payload is a part.
        m_totalApplicationDataUnitLength = CborDecodeU64BufSize9(serialization, &cborSizeDecoded);
        if (cborSizeDecoded == 0) {
            return false; //failure
        }
        serialization += cborSizeDecoded;
    }

    if (hasCrc) {
        //CRC: A CRC SHALL be present in the primary block unless the bundle
        //includes a BPsec [BPSEC] Block Integrity Block whose target is the
        //primary block, in which case a CRC MAY be present in the primary
        //block.  The length and nature of the CRC SHALL be as indicated by
        //the CRC type.  The CRC SHALL be computed over the concatenation of
        //all bytes (including CBOR "break" characters) of the primary block
        //including the CRC field itself, which for this purpose SHALL be
        //temporarily populated with all bytes set to zero.
        uint8_t * const crcStartPtr = serialization;
        if (m_crcType == BPV7_CRC_TYPE_CRC16_X25) {
            if (!Bpv7Crc::DeserializeCrc16ForBpv7(serialization, &cborSizeDecoded, m_deserializedCrc16)) {
                return false;
            }
            serialization += Bpv7Crc::SerializeZeroedCrc16ForBpv7(serialization);
            const uint64_t primaryBlockSerializedLength = serialization - serializationBase;
            const uint16_t computedCrc16 = Bpv7Crc::Crc16_X25_Unaligned(serializationBase, primaryBlockSerializedLength);
            Bpv7Crc::SerializeCrc16ForBpv7(crcStartPtr, m_deserializedCrc16); //restore original received crc after zeroing
            numBytesTakenToDecode = primaryBlockSerializedLength;
            return (computedCrc16 == m_deserializedCrc16);
        }
        else if (m_crcType == BPV7_CRC_TYPE_CRC32C) {
            if (!Bpv7Crc::DeserializeCrc32ForBpv7(serialization, &cborSizeDecoded, m_deserializedCrc32)) {
                return false;
            }
            serialization += Bpv7Crc::SerializeZeroedCrc32ForBpv7(serialization);
            const uint64_t primaryBlockSerializedLength = serialization - serializationBase;
            const uint32_t computedCrc32 = Bpv7Crc::Crc32C_Unaligned(serializationBase, primaryBlockSerializedLength);
            Bpv7Crc::SerializeCrc32ForBpv7(crcStartPtr, m_deserializedCrc32); //restore original received crc after zeroing
            numBytesTakenToDecode = primaryBlockSerializedLength;
            return (computedCrc32 == m_deserializedCrc32);
        }
        else {
            //error
            return false;
        }
    }
    else {
        numBytesTakenToDecode = serialization - serializationBase;
        return true;
    }
}
