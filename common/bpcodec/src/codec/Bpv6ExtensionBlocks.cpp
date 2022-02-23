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
