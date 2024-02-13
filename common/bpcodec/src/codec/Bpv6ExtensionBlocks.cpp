/**
 * @file Bpv6ExtensionBlocks.cpp
 * @author  Brian Tomko <brian.j.tomko@nasa.gov>
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

#include "codec/bpv6.h"
#include "Sdnv.h"
#include <cstring>
#include "Uri.h"

Bpv6CustodyTransferEnhancementBlock::Bpv6CustodyTransferEnhancementBlock() : Bpv6CanonicalBlock(), m_custodyId(0) {
    m_blockTypeCode = BPV6_BLOCK_TYPE_CODE::CUSTODY_TRANSFER_ENHANCEMENT;
} //a default constructor: X()
Bpv6CustodyTransferEnhancementBlock::~Bpv6CustodyTransferEnhancementBlock() { } //a destructor: ~X()
Bpv6CustodyTransferEnhancementBlock::Bpv6CustodyTransferEnhancementBlock(const Bpv6CustodyTransferEnhancementBlock& o) :
    Bpv6CanonicalBlock(o),
    m_custodyId(o.m_custodyId),
    m_ctebCreatorCustodianEidString(o.m_ctebCreatorCustodianEidString){ } //a copy constructor: X(const X&)
Bpv6CustodyTransferEnhancementBlock::Bpv6CustodyTransferEnhancementBlock(Bpv6CustodyTransferEnhancementBlock&& o) :
    Bpv6CanonicalBlock(std::move(o)),
    m_custodyId(o.m_custodyId),
    m_ctebCreatorCustodianEidString(std::move(o.m_ctebCreatorCustodianEidString)) { } //a move constructor: X(X&&)
Bpv6CustodyTransferEnhancementBlock& Bpv6CustodyTransferEnhancementBlock::operator=(const Bpv6CustodyTransferEnhancementBlock& o) { //a copy assignment: operator=(const X&)
    m_custodyId = o.m_custodyId;
    m_ctebCreatorCustodianEidString = o.m_ctebCreatorCustodianEidString;
    return static_cast<Bpv6CustodyTransferEnhancementBlock&>(Bpv6CanonicalBlock::operator=(o));
}
Bpv6CustodyTransferEnhancementBlock& Bpv6CustodyTransferEnhancementBlock::operator=(Bpv6CustodyTransferEnhancementBlock && o) { //a move assignment: operator=(X&&)
    m_custodyId = o.m_custodyId;
    m_ctebCreatorCustodianEidString = std::move(o.m_ctebCreatorCustodianEidString);
    return static_cast<Bpv6CustodyTransferEnhancementBlock&>(Bpv6CanonicalBlock::operator=(std::move(o)));
}
bool Bpv6CustodyTransferEnhancementBlock::operator==(const Bpv6CustodyTransferEnhancementBlock & o) const {
    return (m_custodyId == o.m_custodyId) &&
        (m_ctebCreatorCustodianEidString == o.m_ctebCreatorCustodianEidString) &&
        Bpv6CanonicalBlock::operator==(o);
}
bool Bpv6CustodyTransferEnhancementBlock::operator!=(const Bpv6CustodyTransferEnhancementBlock & o) const {
    return !(*this == o);
}
void Bpv6CustodyTransferEnhancementBlock::SetZero() {
    Bpv6CanonicalBlock::SetZero();
    m_custodyId = 0;
    m_ctebCreatorCustodianEidString.clear();
    m_blockTypeCode = BPV6_BLOCK_TYPE_CODE::CUSTODY_TRANSFER_ENHANCEMENT;
}
uint64_t Bpv6CustodyTransferEnhancementBlock::SerializeBpv6(uint8_t * serialization) {
    /* https://public.ccsds.org/Pubs/734x2b1.pdf
   +----------------+----------------+----------------+----------------+
   |     Canonical block type 0x0a     |  Block Flags* | Block Length* |
   +----------------+----------------+----------------+----------------+
   |   Custody ID* | CTEB creator custodian EID (variable len string)  |
   +----------------+----------------+----------------+----------------+
       * Field is an SDNV
  */
    m_blockTypeCode = BPV6_BLOCK_TYPE_CODE::CUSTODY_TRANSFER_ENHANCEMENT;

    m_blockTypeSpecificDataPtr = NULL;
    m_blockTypeSpecificDataLength = GetCanonicalBlockTypeSpecificDataSerializationSize();
    const uint64_t serializationSizeCanonical = Bpv6CanonicalBlock::SerializeBpv6(serialization);
    uint64_t bufferSize = m_blockTypeSpecificDataLength;
    uint8_t * blockSpecificDataSerialization = m_blockTypeSpecificDataPtr;
    uint64_t thisSerializationSize;

    thisSerializationSize = SdnvEncodeU64(blockSpecificDataSerialization, m_custodyId, bufferSize);
    blockSpecificDataSerialization += thisSerializationSize;
    bufferSize -= thisSerializationSize;

    const std::size_t lengthEidStr = m_ctebCreatorCustodianEidString.length();
    if (bufferSize < lengthEidStr) {
        return 0;
    }
    memcpy(blockSpecificDataSerialization, m_ctebCreatorCustodianEidString.data(), lengthEidStr);
    //blockSpecificDataSerialization += lengthEidStr;
    //bufferSize -= lengthEidStr;

    return serializationSizeCanonical;
}
uint64_t Bpv6CustodyTransferEnhancementBlock::GetCanonicalBlockTypeSpecificDataSerializationSize() const {
    return SdnvGetNumBytesRequiredToEncode(m_custodyId) + m_ctebCreatorCustodianEidString.length();
}

bool Bpv6CustodyTransferEnhancementBlock::Virtual_DeserializeExtensionBlockDataBpv6() {
    uint8_t sdnvSize;
    if (m_blockTypeSpecificDataPtr == NULL) {
        return false;
    }

    uint8_t * serialization = m_blockTypeSpecificDataPtr;
    uint64_t bufferSize = m_blockTypeSpecificDataLength;

    m_custodyId = SdnvDecodeU64(serialization, &sdnvSize, bufferSize);
    if (sdnvSize == 0) {
        return false; //failure
    }
    serialization += sdnvSize;
    bufferSize -= sdnvSize;

    if (bufferSize > 45) { //length of "ipn:18446744073709551615.18446744073709551615"
        return false;
    }
    m_ctebCreatorCustodianEidString.assign((const char *)serialization, bufferSize);
    return true;
}



//https://datatracker.ietf.org/doc/html/rfc6259
Bpv6PreviousHopInsertionCanonicalBlock::Bpv6PreviousHopInsertionCanonicalBlock() : Bpv6CanonicalBlock() {
    //Block-type code (one byte) - The block-type code for the PHIB is 0x05.
    m_blockTypeCode = BPV6_BLOCK_TYPE_CODE::PREVIOUS_HOP_INSERTION;
    //Block processing control flags (SDNV) - The following block processing control flag MUST be set:
    // - Discard block if it can't be processed
    m_blockProcessingControlFlags = BPV6_BLOCKFLAG::DISCARD_BLOCK_IF_IT_CANT_BE_PROCESSED;
} //a default constructor: X()
Bpv6PreviousHopInsertionCanonicalBlock::~Bpv6PreviousHopInsertionCanonicalBlock() { } //a destructor: ~X()
Bpv6PreviousHopInsertionCanonicalBlock::Bpv6PreviousHopInsertionCanonicalBlock(const Bpv6PreviousHopInsertionCanonicalBlock& o) :
    Bpv6CanonicalBlock(o),
    m_previousNode(o.m_previousNode) { } //a copy constructor: X(const X&)
Bpv6PreviousHopInsertionCanonicalBlock::Bpv6PreviousHopInsertionCanonicalBlock(Bpv6PreviousHopInsertionCanonicalBlock&& o) :
    Bpv6CanonicalBlock(std::move(o)),
    m_previousNode(std::move(o.m_previousNode)) { } //a move constructor: X(X&&)
Bpv6PreviousHopInsertionCanonicalBlock& Bpv6PreviousHopInsertionCanonicalBlock::operator=(const Bpv6PreviousHopInsertionCanonicalBlock& o) { //a copy assignment: operator=(const X&)
    m_previousNode = o.m_previousNode;
    return static_cast<Bpv6PreviousHopInsertionCanonicalBlock&>(Bpv6CanonicalBlock::operator=(o));
}
Bpv6PreviousHopInsertionCanonicalBlock& Bpv6PreviousHopInsertionCanonicalBlock::operator=(Bpv6PreviousHopInsertionCanonicalBlock && o) { //a move assignment: operator=(X&&)
    m_previousNode = std::move(o.m_previousNode);
    return static_cast<Bpv6PreviousHopInsertionCanonicalBlock&>(Bpv6CanonicalBlock::operator=(std::move(o)));
}
bool Bpv6PreviousHopInsertionCanonicalBlock::operator==(const Bpv6PreviousHopInsertionCanonicalBlock & o) const {
    return (m_previousNode == o.m_previousNode) &&
        Bpv6CanonicalBlock::operator==(o);
}
bool Bpv6PreviousHopInsertionCanonicalBlock::operator!=(const Bpv6PreviousHopInsertionCanonicalBlock & o) const {
    return !(*this == o);
}
void Bpv6PreviousHopInsertionCanonicalBlock::SetZero() {
    Bpv6CanonicalBlock::SetZero();
    m_previousNode.SetZero();
    m_blockTypeCode = BPV6_BLOCK_TYPE_CODE::PREVIOUS_HOP_INSERTION;
    m_blockProcessingControlFlags = BPV6_BLOCKFLAG::DISCARD_BLOCK_IF_IT_CANT_BE_PROCESSED;
}

uint64_t Bpv6PreviousHopInsertionCanonicalBlock::SerializeBpv6(uint8_t * serialization) {
    //https://datatracker.ietf.org/doc/html/rfc6259
    // PHIB Format:
    // +----+------------+--------------------------------- -+-------------+
    // |type|flags (SDNV)|EID-ref count and list (comp) (opt)|length (SDNV)|
    // +----+------------+-----------------------------------+-------------+
    // | Inserting Node EID Scheme Name (opt)| Inserting Node EID SSP (opt)|
    // +-------------------------------------------------------------------+
    //
    // Block-type-specific data fields (optional) as follows:
    //
    //    *  Inserting Node's EID Scheme Name - A null-terminated array of
    //       bytes that comprises the scheme name of an M-EID of the node
    //       inserting this PHIB.
    //
    //    *  Inserting Node's EID SSP - A null-terminated array of bytes
    //       that comprises the scheme-specific part (SSP) of an M-EID of
    //       the node inserting this PHIB.
  
    m_blockTypeCode = BPV6_BLOCK_TYPE_CODE::PREVIOUS_HOP_INSERTION;
    m_blockProcessingControlFlags |= BPV6_BLOCKFLAG::DISCARD_BLOCK_IF_IT_CANT_BE_PROCESSED;

    m_blockTypeSpecificDataPtr = NULL;
    m_blockTypeSpecificDataLength = GetCanonicalBlockTypeSpecificDataSerializationSize();
    const uint64_t serializationSizeCanonical = Bpv6CanonicalBlock::SerializeBpv6(serialization);
    const uint64_t bufferSize = m_blockTypeSpecificDataLength;
    uint8_t * blockSpecificDataSerialization = m_blockTypeSpecificDataPtr;
    //uint64_t thisSerializationSize;

    //don't need to do the following 4 lines, just use the full ipn string (done below)
    //*blockSpecificDataSerialization++ = 'i';
    //*blockSpecificDataSerialization++ = 'p';
    //*blockSpecificDataSerialization++ = 'n';
    //*blockSpecificDataSerialization++ = '\0';

    const std::string ipnString = Uri::GetIpnUriString(m_previousNode.nodeId, m_previousNode.serviceId);
    const std::size_t ipnStringLength = ipnString.length();
    if (bufferSize != (ipnStringLength + 1)) { //in lieu of bufferSize checks, +1 is for the last string's null terminator
        return 0;
    }
    memcpy(blockSpecificDataSerialization, ipnString.data(), ipnStringLength);
    //null terminate the first string by replacing the ":" after "ipn" with \0
    blockSpecificDataSerialization[3] = '\0';
    blockSpecificDataSerialization[ipnStringLength] = '\0'; //null terminate the second and last string by append \0
    return serializationSizeCanonical;
}
uint64_t Bpv6PreviousHopInsertionCanonicalBlock::GetCanonicalBlockTypeSpecificDataSerializationSize() const {
    return 4 + //ipn\0
        Uri::GetStringLengthOfUint(m_previousNode.nodeId) +
        1 + // :
        Uri::GetStringLengthOfUint(m_previousNode.serviceId) +
        1; // \0
}

bool Bpv6PreviousHopInsertionCanonicalBlock::Virtual_DeserializeExtensionBlockDataBpv6() {
    if (m_blockTypeSpecificDataPtr == NULL) {
        return false;
    }

    const char * serialization = (const char *) m_blockTypeSpecificDataPtr;
    const uint64_t bufferSize = m_blockTypeSpecificDataLength;
    if ((bufferSize < 7) || (serialization[0] != 'i') || (serialization[1] != 'p') || (serialization[2] != 'n') || (serialization[3] != '\0') || (serialization[bufferSize-1] != '\0')) {
        return false;
    }
    if (bufferSize > 45) { //length of "ipn:18446744073709551615.18446744073709551615"
        return false;
    }
    return Uri::ParseIpnSspString(serialization + 4, bufferSize - 5, m_previousNode.nodeId, m_previousNode.serviceId); //bufferSize - 5 removes the null terminating character
}

//https://datatracker.ietf.org/doc/html/draft-irtf-dtnrg-bundle-age-block-01
Bpv6BundleAgeCanonicalBlock::Bpv6BundleAgeCanonicalBlock() : Bpv6CanonicalBlock(), m_bundleAgeMicroseconds(0) {
    //Block-type code (one byte) - The block-type code for the PHIB is 0x05.
    m_blockTypeCode = BPV6_BLOCK_TYPE_CODE::BUNDLE_AGE;
    //"Block Processing Control Flags" is an SDNV that contains the
    //Bundle Protocol block processing control flags.  For the AEB, the
    //"Block must be replicated in every fragment" bit MUST be set.
    //This also dictates that the AEB must occur before the payload
    //block.  See RFC 5050 [RFC5050] Sec 4.3.
    m_blockProcessingControlFlags = BPV6_BLOCKFLAG::MUST_BE_REPLICATED_IN_EVERY_FRAGMENT;
} //a default constructor: X()
Bpv6BundleAgeCanonicalBlock::~Bpv6BundleAgeCanonicalBlock() { } //a destructor: ~X()
Bpv6BundleAgeCanonicalBlock::Bpv6BundleAgeCanonicalBlock(const Bpv6BundleAgeCanonicalBlock& o) :
    Bpv6CanonicalBlock(o),
    m_bundleAgeMicroseconds(o.m_bundleAgeMicroseconds) { } //a copy constructor: X(const X&)
Bpv6BundleAgeCanonicalBlock::Bpv6BundleAgeCanonicalBlock(Bpv6BundleAgeCanonicalBlock&& o) :
    Bpv6CanonicalBlock(std::move(o)),
    m_bundleAgeMicroseconds(o.m_bundleAgeMicroseconds) { } //a move constructor: X(X&&)
Bpv6BundleAgeCanonicalBlock& Bpv6BundleAgeCanonicalBlock::operator=(const Bpv6BundleAgeCanonicalBlock& o) { //a copy assignment: operator=(const X&)
    m_bundleAgeMicroseconds = o.m_bundleAgeMicroseconds;
    return static_cast<Bpv6BundleAgeCanonicalBlock&>(Bpv6CanonicalBlock::operator=(o));
}
Bpv6BundleAgeCanonicalBlock& Bpv6BundleAgeCanonicalBlock::operator=(Bpv6BundleAgeCanonicalBlock && o) { //a move assignment: operator=(X&&)
    m_bundleAgeMicroseconds = o.m_bundleAgeMicroseconds;
    return static_cast<Bpv6BundleAgeCanonicalBlock&>(Bpv6CanonicalBlock::operator=(std::move(o)));
}
bool Bpv6BundleAgeCanonicalBlock::operator==(const Bpv6BundleAgeCanonicalBlock & o) const {
    return (m_bundleAgeMicroseconds == o.m_bundleAgeMicroseconds) &&
        Bpv6CanonicalBlock::operator==(o);
}
bool Bpv6BundleAgeCanonicalBlock::operator!=(const Bpv6BundleAgeCanonicalBlock & o) const {
    return !(*this == o);
}
void Bpv6BundleAgeCanonicalBlock::SetZero() {
    Bpv6CanonicalBlock::SetZero();
    m_bundleAgeMicroseconds = 0;
    m_blockTypeCode = BPV6_BLOCK_TYPE_CODE::BUNDLE_AGE;
    m_blockProcessingControlFlags = BPV6_BLOCKFLAG::MUST_BE_REPLICATED_IN_EVERY_FRAGMENT;
}

uint64_t Bpv6BundleAgeCanonicalBlock::SerializeBpv6(uint8_t * serialization) {
    //https://datatracker.ietf.org/doc/html/draft-irtf-dtnrg-bundle-age-block-01
    //The Age Extension Block format below includes the [RFC5050] required
    //block header fields.
    //
    // +----------------+----------------+-------------------+------------+
    // |   Block Type   | Proc. Flags(*) |  Block Length(*)  |   Age(*)   |
    // +----------------+----------------+-------------------+------------+
    //
    //Support for the AEB by BPA implementations is RECOMMENDED for
    //interoperability but not required.
    //
    //The Age field is defined to represent the approximate elapsed number
    //of microseconds since the creation of the bundle.
    //
    //Notes:
    //
    // -  (*) Indicates field contains a Self-Delimiting Numeric Value
    //    (SDNVs).  See RFC 5050 [RFC5050] Sec. 4.1.
    //
    // -  "Block Type" is a 1-byte mandatory field set to the value 10,
    //    indicating the Age Extension Block.  See RFC 5050 [RFC5050] Sec. 4.3.
    //
    // -  "Block Processing Control Flags" is an SDNV that contains the
    //    Bundle Protocol block processing control flags.  For the AEB, the
    //    "Block must be replicated in every fragment" bit MUST be set.
    //    This also dictates that the AEB must occur before the payload
    //    block.  See RFC 5050 [RFC5050] Sec 4.3.
    //
    // -  "Block Length" is a mandatory SDNV that contains the aggregate
    //    length of all remaining fields of the block.  A one octet SDNV is
    //    shown here for convenience in representation.  See RFC 5050
    //    [RFC5050] Sec 3.1.

    m_blockTypeCode = BPV6_BLOCK_TYPE_CODE::BUNDLE_AGE;
    m_blockProcessingControlFlags |= BPV6_BLOCKFLAG::MUST_BE_REPLICATED_IN_EVERY_FRAGMENT;

    m_blockTypeSpecificDataPtr = NULL;
    m_blockTypeSpecificDataLength = GetCanonicalBlockTypeSpecificDataSerializationSize();
    const uint64_t serializationSizeCanonical = Bpv6CanonicalBlock::SerializeBpv6(serialization);
    const uint64_t bufferSize = m_blockTypeSpecificDataLength;
    uint8_t * blockSpecificDataSerialization = m_blockTypeSpecificDataPtr;
    //uint64_t thisSerializationSize;
    
    //thisSerializationSize = 
    SdnvEncodeU64(blockSpecificDataSerialization, m_bundleAgeMicroseconds, bufferSize);
    //blockSpecificDataSerialization += thisSerializationSize;
    //bufferSize -= thisSerializationSize;

    return serializationSizeCanonical;
}
uint64_t Bpv6BundleAgeCanonicalBlock::GetCanonicalBlockTypeSpecificDataSerializationSize() const {
    return SdnvGetNumBytesRequiredToEncode(m_bundleAgeMicroseconds);
}

bool Bpv6BundleAgeCanonicalBlock::Virtual_DeserializeExtensionBlockDataBpv6() {
    if (m_blockTypeSpecificDataPtr == NULL) {
        return false;
    }
    uint8_t numBytesTakenToDecode;
    m_bundleAgeMicroseconds = SdnvDecodeU64(m_blockTypeSpecificDataPtr, &numBytesTakenToDecode, m_blockTypeSpecificDataLength);
    return ((numBytesTakenToDecode != 0) && (numBytesTakenToDecode == m_blockTypeSpecificDataLength));
}
