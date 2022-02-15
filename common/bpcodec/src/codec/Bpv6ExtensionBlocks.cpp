/***************************************************************************
 * NASA Glenn Research Center, Cleveland, OH
 * Released under the NASA Open Source Agreement (NOSA)
 * May  2021
 *
 ****************************************************************************
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
