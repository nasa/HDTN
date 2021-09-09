/***************************************************************************
 * NASA Glenn Research Center, Cleveland, OH
 * Released under the NASA Open Source Agreement (NOSA)
 * May  2021
 *
 ****************************************************************************
 */
#include "codec/CustodyTransferEnhancementBlock.h"
#include "Sdnv.h"
#include <cstring>



CustodyTransferEnhancementBlock::CustodyTransferEnhancementBlock() :
    m_blockProcessingControlFlags(0),
    m_custodyId(0){ } //a default constructor: X()
CustodyTransferEnhancementBlock::~CustodyTransferEnhancementBlock() { } //a destructor: ~X()
CustodyTransferEnhancementBlock::CustodyTransferEnhancementBlock(const CustodyTransferEnhancementBlock& o) :
    m_blockProcessingControlFlags(o.m_blockProcessingControlFlags),
    m_custodyId(o.m_custodyId),
    m_ctebCreatorCustodianEidString(o.m_ctebCreatorCustodianEidString){ } //a copy constructor: X(const X&)
CustodyTransferEnhancementBlock::CustodyTransferEnhancementBlock(CustodyTransferEnhancementBlock&& o) :
    m_blockProcessingControlFlags(o.m_blockProcessingControlFlags),
    m_custodyId(o.m_custodyId),
    m_ctebCreatorCustodianEidString(std::move(o.m_ctebCreatorCustodianEidString)) { } //a move constructor: X(X&&)
CustodyTransferEnhancementBlock& CustodyTransferEnhancementBlock::operator=(const CustodyTransferEnhancementBlock& o) { //a copy assignment: operator=(const X&)
    m_blockProcessingControlFlags = o.m_blockProcessingControlFlags;
    m_custodyId = o.m_custodyId;
    m_ctebCreatorCustodianEidString = o.m_ctebCreatorCustodianEidString;
    return *this;
}
CustodyTransferEnhancementBlock& CustodyTransferEnhancementBlock::operator=(CustodyTransferEnhancementBlock && o) { //a move assignment: operator=(X&&)
    m_blockProcessingControlFlags = o.m_blockProcessingControlFlags;
    m_custodyId = o.m_custodyId;
    m_ctebCreatorCustodianEidString = std::move(o.m_ctebCreatorCustodianEidString);
    return *this;
}
bool CustodyTransferEnhancementBlock::operator==(const CustodyTransferEnhancementBlock & o) const {
    return
        (m_blockProcessingControlFlags == o.m_blockProcessingControlFlags) &&
        (m_custodyId == o.m_custodyId) &&
        (m_ctebCreatorCustodianEidString == o.m_ctebCreatorCustodianEidString);
}
bool CustodyTransferEnhancementBlock::operator!=(const CustodyTransferEnhancementBlock & o) const {
    return !(*this == o);
}
void CustodyTransferEnhancementBlock::Reset() { //a copy assignment: operator=(const X&)
    m_blockProcessingControlFlags = 0;
    m_custodyId = 0;
    m_ctebCreatorCustodianEidString.clear();
}

uint64_t CustodyTransferEnhancementBlock::SerializeCtebCanonicalBlock(uint8_t * buffer) const { //use MAX_SERIALIZATION_SIZE sized buffer
    bpv6_canonical_block returnedCanonicalBlock;
    return StaticSerializeCtebCanonicalBlock(buffer, m_blockProcessingControlFlags, m_custodyId, m_ctebCreatorCustodianEidString, returnedCanonicalBlock);
}

//static function
uint64_t CustodyTransferEnhancementBlock::StaticSerializeCtebCanonicalBlock(uint8_t * buffer, const uint64_t blockProcessingControlFlags,
    const uint64_t custodyId, const std::string & ctebCreatorCustodianEidString, bpv6_canonical_block & returnedCanonicalBlock)
{
    uint8_t * const serializationBase = buffer;

    returnedCanonicalBlock.type = static_cast<uint8_t>(CANONICAL_BLOCK_TYPE_CODES::CUSTODY_TRANSFER_ENHANCEMENT_BLOCK);
    returnedCanonicalBlock.flags = blockProcessingControlFlags;

    *buffer++ = static_cast<uint8_t>(CANONICAL_BLOCK_TYPE_CODES::CUSTODY_TRANSFER_ENHANCEMENT_BLOCK);

    if (blockProcessingControlFlags <= 127) {
        *buffer++ = static_cast<uint8_t>(blockProcessingControlFlags);
    }
    else {
        buffer += SdnvEncodeU64(buffer, blockProcessingControlFlags);
    }

    uint8_t* const blockLengthPtr = buffer++; //write in later

    buffer += SdnvEncodeU64(buffer, custodyId);

    const std::size_t lengthEidStr = ctebCreatorCustodianEidString.length();
    memcpy(buffer, ctebCreatorCustodianEidString.data(), lengthEidStr);
    buffer += lengthEidStr;

    const uint64_t blockLength = buffer - (blockLengthPtr + 1);
    if (blockLength > 127) {
        return 0; //failure
    }
    *blockLengthPtr = static_cast<uint8_t>(blockLength);
    returnedCanonicalBlock.length = blockLength;
    return buffer - serializationBase;
}

//static function
uint64_t CustodyTransferEnhancementBlock::StaticSerializeCtebCanonicalBlockBody(uint8_t * buffer,
    const uint64_t custodyId, const std::string & ctebCreatorCustodianEidString, bpv6_canonical_block & returnedCanonicalBlock)
{
    uint8_t * const serializationBase = buffer;

    returnedCanonicalBlock.type = static_cast<uint8_t>(CANONICAL_BLOCK_TYPE_CODES::CUSTODY_TRANSFER_ENHANCEMENT_BLOCK);
    returnedCanonicalBlock.flags = 0;

    
    buffer += SdnvEncodeU64(buffer, custodyId);

    const std::size_t lengthEidStr = ctebCreatorCustodianEidString.length();
    memcpy(buffer, ctebCreatorCustodianEidString.data(), lengthEidStr);
    buffer += lengthEidStr;

    const uint64_t bodyLength = buffer - serializationBase;
    returnedCanonicalBlock.length = bodyLength;
    return bodyLength;
}

uint32_t CustodyTransferEnhancementBlock::DeserializeCtebCanonicalBlock(const uint8_t * serialization) {
    uint8_t sdnvSize;
    const uint8_t * const serializationBase = serialization;

    const CANONICAL_BLOCK_TYPE_CODES blockTypeCode = static_cast<CANONICAL_BLOCK_TYPE_CODES>(*serialization++);
    if (blockTypeCode != CANONICAL_BLOCK_TYPE_CODES::CUSTODY_TRANSFER_ENHANCEMENT_BLOCK) {
        return 0; //failure
    }

    const uint8_t flag8bit = *serialization;
    if (flag8bit <= 127) {
        m_blockProcessingControlFlags = flag8bit;
        ++serialization;
    }
    else {
        m_blockProcessingControlFlags = SdnvDecodeU64(serialization, &sdnvSize);
        if (sdnvSize == 0) {
            return 0; //return 0 on failure
        }
        serialization += sdnvSize;
    }

    uint64_t blockLength = *serialization;
    if (blockLength <= 127) {
        ++serialization;
    }
    else {
        blockLength = SdnvDecodeU64(serialization, &sdnvSize);
        if (sdnvSize == 0) {
            return 0; //return 0 on failure
        }
        serialization += sdnvSize;
    }

    m_custodyId = SdnvDecodeU64(serialization, &sdnvSize);
    if (sdnvSize == 0) {
        return 0; //return 0 on failure
    }
    serialization += sdnvSize;
    

    const std::size_t lengthEidStr = blockLength - sdnvSize; // sdnvSize is custodyIdEncodedSize

    m_ctebCreatorCustodianEidString.assign((const char *)serialization, lengthEidStr);
    serialization += lengthEidStr;

    return static_cast<uint32_t>(serialization - serializationBase);
}

void CustodyTransferEnhancementBlock::AddCanonicalBlockProcessingControlFlag(BLOCK_PROCESSING_CONTROL_FLAGS flag) {
    m_blockProcessingControlFlags |= static_cast<uint64_t>(flag);
}
bool CustodyTransferEnhancementBlock::HasCanonicalBlockProcessingControlFlagSet(BLOCK_PROCESSING_CONTROL_FLAGS flag) const {
    return ((m_blockProcessingControlFlags & static_cast<uint64_t>(flag)) != 0);
}
