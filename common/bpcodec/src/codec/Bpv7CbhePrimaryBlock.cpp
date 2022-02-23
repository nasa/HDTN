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
    m_computedCrc32(o.m_computedCrc32),
    m_computedCrc16(o.m_computedCrc16),
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
    m_computedCrc32(o.m_computedCrc32),
    m_computedCrc16(o.m_computedCrc16),
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
    m_computedCrc32 = o.m_computedCrc32;
    m_computedCrc16 = o.m_computedCrc16;
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
    m_computedCrc32 = o.m_computedCrc32;
    m_computedCrc16 = o.m_computedCrc16;
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
        && (m_computedCrc32 == o.m_computedCrc32)
        && (m_computedCrc16 == o.m_computedCrc16)
        && (m_crcType == o.m_crcType);
}
bool Bpv7CbhePrimaryBlock::operator!=(const Bpv7CbhePrimaryBlock & o) const {
    return !(*this == o);
}
void Bpv7CbhePrimaryBlock::SetZero() {
    //memset(this, 0, sizeof(*this)); //does not work for virtual classes
    m_bundleProcessingControlFlags = BPV7_BUNDLEFLAG::NO_FLAGS_SET;
    m_destinationEid.SetZero();
    m_sourceNodeId.SetZero();
    m_reportToEid.SetZero();
    m_creationTimestamp.SetZero();
    m_lifetimeMilliseconds = 0;
    m_fragmentOffset = 0;
    m_totalApplicationDataUnitLength = 0;
    m_computedCrc32 = 0;
    m_computedCrc16 = 0;
    m_crcType = BPV7_CRC_TYPE::NONE;
}



uint64_t Bpv7CbhePrimaryBlock::SerializeBpv7(uint8_t * serialization) { //not const as it modifies m_computedCrcXX
    uint8_t * const serializationBase = serialization;

    //Each primary block SHALL be represented as a CBOR array; the number
    //of elements in the array SHALL be 8 (if the bundle is not a fragment
    //and the block has no CRC), 9 (if the block has a CRC and the bundle
    //is not a fragment), 10 (if the bundle is a fragment and the block
    //has no CRC), or 11 (if the bundle is a fragment and the block has a
    //CRC).
    const bool hasCrc = (m_crcType != BPV7_CRC_TYPE::NONE);
    const bool isFragment = ((m_bundleProcessingControlFlags & BPV7_BUNDLEFLAG::ISFRAGMENT) != BPV7_BUNDLEFLAG::NO_FLAGS_SET);
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
    serialization += CborEncodeU64BufSize9(serialization, static_cast<uint64_t>(m_bundleProcessingControlFlags));

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
    *serialization++ = static_cast<uint8_t>(m_crcType);
    
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
        if (m_crcType == BPV7_CRC_TYPE::CRC16_X25) {
            serialization += Bpv7Crc::SerializeZeroedCrc16ForBpv7(serialization);
            const uint64_t primaryBlockSerializedLength = serialization - serializationBase;
            m_computedCrc32 = 0;
            m_computedCrc16 = Bpv7Crc::Crc16_X25_Unaligned(serializationBase, primaryBlockSerializedLength);
            Bpv7Crc::SerializeCrc16ForBpv7(crcStartPtr, m_computedCrc16);
            return primaryBlockSerializedLength;
        }
        else if (m_crcType == BPV7_CRC_TYPE::CRC32C) {
            serialization += Bpv7Crc::SerializeZeroedCrc32ForBpv7(serialization);
            const uint64_t primaryBlockSerializedLength = serialization - serializationBase;
            m_computedCrc16 = 0;
            m_computedCrc32 = Bpv7Crc::Crc32C_Unaligned(serializationBase, primaryBlockSerializedLength);
            Bpv7Crc::SerializeCrc32ForBpv7(crcStartPtr, m_computedCrc32);
            return primaryBlockSerializedLength;
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

uint64_t Bpv7CbhePrimaryBlock::GetSerializationSize() const {
    const bool isFragment = ((m_bundleProcessingControlFlags & BPV7_BUNDLEFLAG::ISFRAGMENT) != BPV7_BUNDLEFLAG::NO_FLAGS_SET);

    uint64_t serializationSize = 3; //cbor byte (major type 4, additional information [8..11]) + version7 + crcType
    serializationSize += CborGetEncodingSizeU64(static_cast<uint64_t>(m_bundleProcessingControlFlags));
    serializationSize += m_destinationEid.GetSerializationSizeBpv7();
    serializationSize += m_sourceNodeId.GetSerializationSizeBpv7();
    serializationSize += m_reportToEid.GetSerializationSizeBpv7();
    serializationSize += m_creationTimestamp.GetSerializationSize();
    serializationSize += CborGetEncodingSizeU64(m_lifetimeMilliseconds);
    serializationSize += (static_cast<uint8_t>(CborGetEncodingSizeU64(m_fragmentOffset))) * isFragment; //branchless
    serializationSize += (static_cast<uint8_t>(CborGetEncodingSizeU64(m_totalApplicationDataUnitLength))) * isFragment; //branchless
    static const uint8_t CRC_TYPE_TO_SIZE[4] = { 0,3,5,0 };
    serializationSize += CRC_TYPE_TO_SIZE[(static_cast<uint8_t>(m_crcType)) & 3];
    return serializationSize;
}

//serialization must be temporarily modifyable to zero crc and restore it
bool Bpv7CbhePrimaryBlock::DeserializeBpv7(uint8_t * serialization, uint64_t & numBytesTakenToDecode, uint64_t bufferSize) {
    uint8_t cborSizeDecoded;
    const uint8_t * const serializationBase = serialization;

    if (bufferSize < Bpv7CbhePrimaryBlock::smallestSerializedPrimarySize) {
        return false;
    }

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
    m_bundleProcessingControlFlags = static_cast<BPV7_BUNDLEFLAG>(CborDecodeU64BufSize9(serialization, &cborSizeDecoded));
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
    m_crcType = static_cast<BPV7_CRC_TYPE>(*serialization++);

    bufferSize -= (3 + cborSizeDecoded); //for initialCborByte and bpVersion and crcType and flags.. min size now at 17-3-9=5

    //verify cbor array size
    const bool hasCrc = (m_crcType != BPV7_CRC_TYPE::NONE);
    const bool isFragment = ((m_bundleProcessingControlFlags & BPV7_BUNDLEFLAG::ISFRAGMENT) != BPV7_BUNDLEFLAG::NO_FLAGS_SET);
    const uint8_t expectedCborArraySize = 8 + hasCrc + ((static_cast<uint8_t>(isFragment)) << 1);
    if (expectedCborArraySize != cborArraySize) {
        return false; //failure
    }

    //Destination EID: The Destination EID field identifies the bundle
    //endpoint that is the bundle's destination, i.e., the endpoint that
    //contains the node(s) at which the bundle is to be delivered.
    if(!m_destinationEid.DeserializeBpv7(serialization, &cborSizeDecoded, bufferSize)) { // cborSizeDecoded will never be 0 if function returns true
        return false; //failure
    }
    serialization += cborSizeDecoded;
    bufferSize -= cborSizeDecoded;

    //Source node ID: The Source node ID field identifies the bundle node
    //at which the bundle was initially transmitted, except that Source
    //node ID may be the null endpoint ID in the event that the bundle's
    //source chooses to remain anonymous.
    if (!m_sourceNodeId.DeserializeBpv7(serialization, &cborSizeDecoded, bufferSize)) { // cborSizeDecoded will never be 0 if function returns true
        return false; //failure
    }
    serialization += cborSizeDecoded;
    bufferSize -= cborSizeDecoded;

    //Report-to EID: The Report-to EID field identifies the bundle
    //endpoint to which status reports pertaining to the forwarding and
    //delivery of this bundle are to be transmitted.
    if (!m_reportToEid.DeserializeBpv7(serialization, &cborSizeDecoded, bufferSize)) { // cborSizeDecoded will never be 0 if function returns true
        return false; //failure
    }
    serialization += cborSizeDecoded;
    bufferSize -= cborSizeDecoded;

    //Creation Timestamp: The creation timestamp comprises two unsigned
    //integers that, together with the source node ID and (if the bundle
    //is a fragment) the fragment offset and payload length, serve to
    //identify the bundle. See 4.2.7 above for the definition of this field.
    if (!m_creationTimestamp.DeserializeBpv7(serialization, &cborSizeDecoded, bufferSize)) { // cborSizeDecoded will never be 0 if function returns true
        return false; //failure
    }
    serialization += cborSizeDecoded;
    bufferSize -= cborSizeDecoded;

    //Lifetime: The lifetime field is an unsigned integer that indicates
    //the time at which the bundle's payload will no longer be useful,
    //encoded as a number of milliseconds past the creation time. (For
    //high-rate deployments with very brief disruptions, fine-grained
    //expression of bundle lifetime may be useful.)  When a bundle's age
    //exceeds its lifetime, bundle nodes need no longer retain or forward
    //the bundle; the bundle SHOULD be deleted from the network...
    //Bundle lifetime SHALL be represented as a CBOR unsigned integer item.
    m_lifetimeMilliseconds = CborDecodeU64(serialization, &cborSizeDecoded, bufferSize);
    if (cborSizeDecoded == 0) {
        return false; //failure
    }
    serialization += cborSizeDecoded;
    bufferSize -= cborSizeDecoded;

    if (isFragment) {
        //Fragment offset: If and only if the Bundle Processing Control Flags
        //of this Primary block indicate that the bundle is a fragment,
        //fragment offset SHALL be present in the primary block. Fragment
        //offset SHALL be represented as a CBOR unsigned integer indicating
        //the offset from the start of the original application data unit at
        //which the bytes comprising the payload of this bundle were located.
        m_fragmentOffset = CborDecodeU64(serialization, &cborSizeDecoded, bufferSize);
        if (cborSizeDecoded == 0) {
            return false; //failure
        }
        serialization += cborSizeDecoded;
        bufferSize -= cborSizeDecoded;

        //Total Application Data Unit Length: If and only if the Bundle
        //Processing Control Flags of this Primary block indicate that the
        //bundle is a fragment, total application data unit length SHALL be
        //present in the primary block. Total application data unit length
        //SHALL be represented as a CBOR unsigned integer indicating the total
        //length of the original application data unit of which this bundle's
        //payload is a part.
        m_totalApplicationDataUnitLength = CborDecodeU64(serialization, &cborSizeDecoded, bufferSize);
        if (cborSizeDecoded == 0) {
            return false; //failure
        }
        serialization += cborSizeDecoded;
        bufferSize -= cborSizeDecoded;
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
        if (m_crcType == BPV7_CRC_TYPE::CRC16_X25) {
            m_computedCrc32 = 0;
            if ((bufferSize < 3) || (!Bpv7Crc::DeserializeCrc16ForBpv7(serialization, &cborSizeDecoded, m_computedCrc16))) {
                return false;
            }
            serialization += Bpv7Crc::SerializeZeroedCrc16ForBpv7(serialization);
            const uint64_t primaryBlockSerializedLength = serialization - serializationBase;
            const uint16_t computedCrc16 = Bpv7Crc::Crc16_X25_Unaligned(serializationBase, primaryBlockSerializedLength);
            Bpv7Crc::SerializeCrc16ForBpv7(crcStartPtr, m_computedCrc16); //restore original received crc after zeroing
            numBytesTakenToDecode = primaryBlockSerializedLength;
            if (computedCrc16 == m_computedCrc16) {
                return true;
            }
            else {
                static const boost::format fmtTemplate("Error: Bpv7CbhePrimaryBlock deserialize Crc16_X25 mismatch: primary came with crc %04x but Decode just computed %04x");
                boost::format fmt(fmtTemplate);
                fmt % m_computedCrc16 % computedCrc16;
                const std::string message(std::move(fmt.str()));
                std::cout << message << "\n";
                return false;
            }
        }
        else if (m_crcType == BPV7_CRC_TYPE::CRC32C) {
            m_computedCrc16 = 0;
            if ((bufferSize < 5) || (!Bpv7Crc::DeserializeCrc32ForBpv7(serialization, &cborSizeDecoded, m_computedCrc32))) {
                return false;
            }
            serialization += Bpv7Crc::SerializeZeroedCrc32ForBpv7(serialization);
            const uint64_t primaryBlockSerializedLength = serialization - serializationBase;
            const uint32_t computedCrc32 = Bpv7Crc::Crc32C_Unaligned(serializationBase, primaryBlockSerializedLength);
            Bpv7Crc::SerializeCrc32ForBpv7(crcStartPtr, m_computedCrc32); //restore original received crc after zeroing
            numBytesTakenToDecode = primaryBlockSerializedLength;
            if (computedCrc32 == m_computedCrc32) {
                return true;
            }
            else {
                static const boost::format fmtTemplate("Error: Bpv7CbhePrimaryBlock deserialize Crc32C mismatch: primary came with crc %08x but Decode just computed %08x");
                boost::format fmt(fmtTemplate);
                fmt % m_computedCrc32 % computedCrc32;
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
        m_computedCrc32 = 0;
        m_computedCrc16 = 0;
        numBytesTakenToDecode = serialization - serializationBase;
        return true;
    }
}

bool Bpv7CbhePrimaryBlock::HasCustodyFlagSet() const {
    return false; //unsupported at this time (flags & BPV6_BUNDLEFLAG_CUSTODY);
}
bool Bpv7CbhePrimaryBlock::HasFragmentationFlagSet() const {
    const bool isFragment = ((m_bundleProcessingControlFlags & BPV7_BUNDLEFLAG::ISFRAGMENT) != BPV7_BUNDLEFLAG::NO_FLAGS_SET);
    return isFragment;
}

cbhe_bundle_uuid_t Bpv7CbhePrimaryBlock::GetCbheBundleUuidFromPrimary() const {
    cbhe_bundle_uuid_t uuid;
    uuid.creationSeconds = m_creationTimestamp.millisecondsSinceStartOfYear2000;
    uuid.sequence = m_creationTimestamp.sequenceNumber;
    uuid.srcEid = m_sourceNodeId;
    uuid.fragmentOffset = m_fragmentOffset;
    uuid.dataLength = m_totalApplicationDataUnitLength;
    return uuid;
}
cbhe_bundle_uuid_nofragment_t Bpv7CbhePrimaryBlock::GetCbheBundleUuidNoFragmentFromPrimary() const {
    cbhe_bundle_uuid_nofragment_t uuid;
    uuid.creationSeconds = m_creationTimestamp.millisecondsSinceStartOfYear2000;
    uuid.sequence = m_creationTimestamp.sequenceNumber;
    uuid.srcEid = m_sourceNodeId;
    return uuid;
}

cbhe_eid_t Bpv7CbhePrimaryBlock::GetFinalDestinationEid() const {
    return m_destinationEid;
}
uint8_t Bpv7CbhePrimaryBlock::GetPriority() const {
    return 2; //priority not supported so keep it high by default
}
uint64_t Bpv7CbhePrimaryBlock::GetExpirationSeconds() const {
    return (m_creationTimestamp.millisecondsSinceStartOfYear2000 + m_lifetimeMilliseconds) / 1000;
}
uint64_t Bpv7CbhePrimaryBlock::GetSequenceForSecondsScale() const {
    uint64_t rem = m_creationTimestamp.millisecondsSinceStartOfYear2000 % 1000;
    return (rem << 50) | m_creationTimestamp.sequenceNumber;
}
uint64_t Bpv7CbhePrimaryBlock::GetExpirationMilliseconds() const {
    return m_creationTimestamp.millisecondsSinceStartOfYear2000 + m_lifetimeMilliseconds;
}
uint64_t Bpv7CbhePrimaryBlock::GetSequenceForMillisecondsScale() const {
    return m_creationTimestamp.sequenceNumber;
}
