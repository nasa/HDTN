#include "codec/BundleViewV7.h"
#include <iostream>
#include <boost/next_prior.hpp>
#include "Uri.h"
#include "PaddedVectorUint8.h"
#include "Logger.h"

static constexpr hdtn::Logger::SubProcess subprocess = hdtn::Logger::SubProcess::none;

void BundleViewV7::Bpv7PrimaryBlockView::SetManuallyModified() {
    dirty = true;
}
void BundleViewV7::Bpv7CanonicalBlockView::SetManuallyModified() {
    dirty = true;
}
BundleViewV7::Bpv7CanonicalBlockView::Bpv7CanonicalBlockView() : isEncrypted(false) {}

BundleViewV7::BundleViewV7() {}
BundleViewV7::~BundleViewV7() {}

bool BundleViewV7::Load(const bool skipCrcVerifyInCanonicalBlocks, const bool loadPrimaryBlockOnly) {
    const uint8_t * const serializationBase = (uint8_t*)m_renderedBundle.data();
    uint8_t * serialization = (uint8_t*)m_renderedBundle.data();
    uint64_t bufferSize = m_renderedBundle.size();
    uint64_t decodedBlockSize;

    
    //Each bundle SHALL be a concatenated sequence of at least two blocks,
    //represented as a CBOR indefinite-length array.
    if (bufferSize == 0) {
        return false;
    }
    --bufferSize;
    const uint8_t initialCborByte = *serialization++;
    if (initialCborByte != ((4U << 5) | 31U)) { //major type 4, additional information 31 (Indefinite-Length Array)
        return false;
    }

    //The first block in
    //the sequence (the first item of the array) MUST be a primary bundle
    //block in CBOR representation as described below; the bundle MUST
    //have exactly one primary bundle block.
    const uint8_t * const serializationPrimaryBlockBeginPtr = serialization;
    if (!m_primaryBlockView.header.DeserializeBpv7(serialization, decodedBlockSize, bufferSize)) {
        return false;
    }
    serialization += decodedBlockSize;
    bufferSize -= decodedBlockSize;
    const bool isFragment = ((m_primaryBlockView.header.m_bundleProcessingControlFlags & BPV7_BUNDLEFLAG::ISFRAGMENT) != BPV7_BUNDLEFLAG::NO_FLAGS_SET);
    if (isFragment) { //not currently supported
        return false;
    }
    m_primaryBlockView.actualSerializedPrimaryBlockPtr = boost::asio::buffer(serializationPrimaryBlockBeginPtr, decodedBlockSize);
    m_primaryBlockView.dirty = false;
    m_applicationDataUnitStartPtr = serializationPrimaryBlockBeginPtr + decodedBlockSize;
    //todo application data unit length?
    const bool isAdminRecord = ((m_primaryBlockView.header.m_bundleProcessingControlFlags & BPV7_BUNDLEFLAG::ADMINRECORD) != BPV7_BUNDLEFLAG::NO_FLAGS_SET);
    
    if (loadPrimaryBlockOnly) {
        return true;
    }

    //The primary block MUST be
    //followed by one or more canonical bundle blocks (additional array
    //items) in CBOR representation as described in 4.3.2 below.  Every
    //block following the primary block SHALL be the CBOR representation
    //of a canonical block.
    while (true) {
        uint8_t * const serializationThisCanonicalBlockBeginPtr = serialization;
        m_listCanonicalBlockView.emplace_back();
        Bpv7CanonicalBlockView & cbv = m_listCanonicalBlockView.back();
        cbv.dirty = false;
        cbv.markedForDeletion = false;
        cbv.isEncrypted = false;
        if(!Bpv7CanonicalBlock::DeserializeBpv7(cbv.headerPtr, serialization, decodedBlockSize, bufferSize, skipCrcVerifyInCanonicalBlocks, isAdminRecord)) {
            return false;
        }
        serialization += decodedBlockSize;
        bufferSize -= decodedBlockSize;
        cbv.actualSerializedBlockPtr = boost::asio::buffer(serializationThisCanonicalBlockBeginPtr, decodedBlockSize);

        if (cbv.headerPtr->m_blockTypeCode == BPV7_BLOCK_TYPE_CODE::CONFIDENTIALITY) {
            cbv.isEncrypted = false; //bcb block is never encrypted
            if (!cbv.headerPtr->Virtual_DeserializeExtensionBlockDataBpv7()) { //requires m_dataPtr and m_dataLength to be set (done in Bpv7CanonicalBlock::DeserializeBpv7)
                return false;
            }
            if (Bpv7BlockConfidentialityBlock * bcbPtr = dynamic_cast<Bpv7BlockConfidentialityBlock*>(cbv.headerPtr.get())) {
                const std::vector<uint64_t> & securityTargets = bcbPtr->m_securityTargets;
                for (std::size_t i = 0; i < securityTargets.size(); ++i) {
                    if (!m_mapEncryptedBlockNumberToBcbPtr.emplace(securityTargets[i], bcbPtr).second) {
                        return false;
                    }
                }
            }
            else {
                return false;
            }
        }

        //The last such block MUST be a payload block;
        //the bundle MUST have exactly one payload block.  The payload block
        //SHALL be followed by a CBOR "break" stop code, terminating the
        //array.
        if (cbv.headerPtr->m_blockTypeCode == BPV7_BLOCK_TYPE_CODE::PAYLOAD) { //last block
            if (cbv.headerPtr->m_blockNumber != 1) { //The block number of the payload block is always 1.
                return false;
            }
            if (bufferSize == 0) {
                return false;
            }
            --bufferSize;
            const uint8_t expectedCborBreakStopCode = *serialization++;
            if (expectedCborBreakStopCode != 0xff) { //0xff is break character
                return false;
            }
            const uint64_t bundleSerializedLength = serialization - serializationBase;
            if (bundleSerializedLength != m_renderedBundle.size()) { //todo aggregation support
                return false;
            }
            break;
        }
        else if ((static_cast<uint64_t>(serialization - serializationBase)) >= m_renderedBundle.size()) {
            return false;
        }
    }

    for (std::list<Bpv7CanonicalBlockView>::iterator it = m_listCanonicalBlockView.begin(); it != m_listCanonicalBlockView.end(); ++it) {
        Bpv7CanonicalBlockView & cbv = *it;
        if (cbv.headerPtr->m_blockTypeCode != BPV7_BLOCK_TYPE_CODE::CONFIDENTIALITY) { //bcbs were already processed above
            cbv.isEncrypted = (m_mapEncryptedBlockNumberToBcbPtr.count(cbv.headerPtr->m_blockNumber) != 0); //bcb block is never encrypted
            if ((!cbv.isEncrypted) && (!cbv.headerPtr->Virtual_DeserializeExtensionBlockDataBpv7())) { //requires m_dataPtr and m_dataLength to be set (done in Bpv7CanonicalBlock::DeserializeBpv7)
                return false;
            }
        }
    }
    return true;
}
bool BundleViewV7::Render(const std::size_t maxBundleSizeBytes) {
    //first render to the back buffer, copying over non-dirty blocks from the m_renderedBundle which may be the front buffer or other memory from a load operation
    m_backBuffer.resize(maxBundleSizeBytes);
    uint64_t sizeSerialized;
    if (!Render(&m_backBuffer[0], sizeSerialized, false)) {
        return false;
    }
    m_backBuffer.resize(sizeSerialized);
    m_frontBuffer.swap(m_backBuffer); //m_frontBuffer is now the rendered bundle
    m_renderedBundle = boost::asio::buffer(m_frontBuffer);
    return true;
}

//keep the huge payload last block (relatively speaking to the rest of the blocks) in place and render everything before
bool BundleViewV7::RenderInPlace(const std::size_t paddingLeft) {
    const uint64_t originalBundleSize = m_renderedBundle.size();
    uint64_t newBundleSize;
    if (!GetSerializationSize(newBundleSize)) {
        return false;
    }

    uint8_t * newStart;
    uint64_t maxRenderSpaceRequired;

    if (m_listCanonicalBlockView.size() == 0) { //payload block must exist
        return false;
    }
    const uint64_t payloadLastBlockSize = m_listCanonicalBlockView.back().actualSerializedBlockPtr.size();

    if (newBundleSize > originalBundleSize) { //shift left, will overlap paddingLeft
        const uint64_t diff = newBundleSize - originalBundleSize;
        if (diff > paddingLeft) {
            return false;
        }
        newStart = ((uint8_t*)m_renderedBundle.data()) - diff;
        maxRenderSpaceRequired = (newBundleSize - payloadLastBlockSize) + 10;
    }
    else {  //if (newBundleSize <= originalBundleSize) { //shift right
        const uint64_t diff = originalBundleSize - newBundleSize;
        newStart = ((uint8_t*)m_renderedBundle.data()) + diff;
        maxRenderSpaceRequired = (originalBundleSize - payloadLastBlockSize) + 10;
    }
    std::vector<uint8_t> tmpRender(maxRenderSpaceRequired);
    uint64_t sizeSerialized;
    if (!Render(&tmpRender[0], sizeSerialized, true)) { //render to temporary space first
        return false;
    }
    //everything not dirty so memcpy will be used in next Render step
    if (!Render(newStart, sizeSerialized, true)) { //render to newStart
        return false;
    }
    m_renderedBundle = boost::asio::buffer(newStart, newBundleSize);
    return true;
}
bool BundleViewV7::Render(uint8_t * serialization, uint64_t & sizeSerialized, bool terminateBeforeLastBlock) {
    uint8_t * const serializationBase = serialization;
    *serialization++ = (4U << 5) | 31U; //major type 4, additional information 31 (Indefinite-Length Array)
    if (m_primaryBlockView.dirty) {
        const uint64_t sizeSerialized = m_primaryBlockView.header.SerializeBpv7(serialization);
        if (sizeSerialized == 0) {
            return false;
        }
        m_primaryBlockView.actualSerializedPrimaryBlockPtr = boost::asio::buffer(serialization, sizeSerialized);
        serialization += sizeSerialized;
        m_primaryBlockView.dirty = false;
    }
    else {
        const std::size_t size = m_primaryBlockView.actualSerializedPrimaryBlockPtr.size();
        memcpy(serialization, m_primaryBlockView.actualSerializedPrimaryBlockPtr.data(), size);
        m_primaryBlockView.actualSerializedPrimaryBlockPtr = boost::asio::buffer(serialization, size);
        serialization += size;
    }
    const bool isAdminRecord = ((m_primaryBlockView.header.m_bundleProcessingControlFlags & (BPV7_BUNDLEFLAG::ADMINRECORD)) != BPV7_BUNDLEFLAG::NO_FLAGS_SET);
    const bool isFragment = ((m_primaryBlockView.header.m_bundleProcessingControlFlags & (BPV7_BUNDLEFLAG::ISFRAGMENT)) != BPV7_BUNDLEFLAG::NO_FLAGS_SET);
    if (isFragment) {
        return false;
    }
    
    m_listCanonicalBlockView.remove_if([](const Bpv7CanonicalBlockView & v) { return v.markedForDeletion; }); //makes easier last block detection

    for (std::list<Bpv7CanonicalBlockView>::iterator it = m_listCanonicalBlockView.begin(); it != m_listCanonicalBlockView.end(); ++it) {
        const bool isLastBlock = (boost::next(it) == m_listCanonicalBlockView.end());
        if (isLastBlock) {
            if (it->headerPtr->m_blockTypeCode != BPV7_BLOCK_TYPE_CODE::PAYLOAD) {
                LOG_ERROR(subprocess) << "BundleViewV7::Render: last block is not payload block";
                return false;
            }
            if (it->headerPtr->m_blockNumber != 1) { //The block number of the payload block is always 1.
                LOG_ERROR(subprocess) << "BundleViewV7::Render: last block (payload block) has block number " << it->headerPtr->m_blockNumber 
                    << " but the block number of the payload block is always 1.";
                return false;
            }
            if (terminateBeforeLastBlock) {
                if (it->dirty) {
                    return false;
                }
                sizeSerialized = (serialization - serializationBase);
                return true;
            }
        }
        uint64_t currentBlockSizeSerialized;
        if (it->dirty) { //always reencode canonical block if dirty
            if (it->isEncrypted) {
                currentBlockSizeSerialized = it->headerPtr->Bpv7CanonicalBlock::SerializeBpv7(serialization);
            }
            else {
                currentBlockSizeSerialized = it->headerPtr->SerializeBpv7(serialization);
            }
            if (currentBlockSizeSerialized < Bpv7CanonicalBlock::smallestSerializedCanonicalSize) {
                return false;
            }
            it->dirty = false;
        }
        else {
            currentBlockSizeSerialized = it->actualSerializedBlockPtr.size();
            memcpy(serialization, it->actualSerializedBlockPtr.data(), currentBlockSizeSerialized);
        }
        it->actualSerializedBlockPtr = boost::asio::buffer(serialization, currentBlockSizeSerialized);
        serialization += currentBlockSizeSerialized;
    }
    *serialization++ = 0xff; //0xff is break character
    sizeSerialized = (serialization - serializationBase);
    return true;
}
bool BundleViewV7::GetSerializationSize(uint64_t & serializationSize) const {
    serializationSize = 2; //major type 4, additional information 31 (Indefinite-Length Array) + cbor break character 0xff
    if (m_primaryBlockView.dirty) {
        const uint64_t sizeSerialized = m_primaryBlockView.header.GetSerializationSize();
        if (sizeSerialized == 0) {
            return false;
        }
        serializationSize += sizeSerialized;
    }
    else {
        serializationSize += m_primaryBlockView.actualSerializedPrimaryBlockPtr.size();
    }
    
    for (std::list<Bpv7CanonicalBlockView>::const_iterator it = m_listCanonicalBlockView.cbegin(); it != m_listCanonicalBlockView.cend(); ++it) {
        const bool isLastBlock = (boost::next(it) == m_listCanonicalBlockView.end());
        if (isLastBlock && (it->headerPtr->m_blockTypeCode != BPV7_BLOCK_TYPE_CODE::PAYLOAD)) {
            LOG_ERROR(subprocess) << "BundleViewV7::GetSerializationSize: last block is not payload block";
            return false;
        }
        uint64_t currentBlockSizeSerialized;
        if (it->markedForDeletion) {
            currentBlockSizeSerialized = 0;
        }
        else if (it->dirty) { //always reencode canonical block if dirty
            currentBlockSizeSerialized = it->headerPtr->GetSerializationSize();
            if (currentBlockSizeSerialized < Bpv7CanonicalBlock::smallestSerializedCanonicalSize) {
                return false;
            }
        }
        else {
            currentBlockSizeSerialized = it->actualSerializedBlockPtr.size();
        }
        serializationSize += currentBlockSizeSerialized;
    }
    return true;
}

void BundleViewV7::AppendMoveCanonicalBlock(std::unique_ptr<Bpv7CanonicalBlock> & headerPtr) {
    m_listCanonicalBlockView.emplace_back();
    Bpv7CanonicalBlockView & cbv = m_listCanonicalBlockView.back();
    cbv.dirty = true; //true will ignore and set actualSerializedBlockPtr after render
    cbv.markedForDeletion = false;
    cbv.headerPtr = std::move(headerPtr);
}
void BundleViewV7::PrependMoveCanonicalBlock(std::unique_ptr<Bpv7CanonicalBlock> & headerPtr) {
    m_listCanonicalBlockView.emplace_front();
    Bpv7CanonicalBlockView & cbv = m_listCanonicalBlockView.front();
    cbv.dirty = true; //true will ignore and set actualSerializedBlockPtr after render
    cbv.markedForDeletion = false;
    cbv.headerPtr = std::move(headerPtr);
}
bool BundleViewV7::InsertMoveCanonicalBlockAfterBlockNumber(std::unique_ptr<Bpv7CanonicalBlock> & headerPtr, const uint64_t blockNumber) {
    for (std::list<Bpv7CanonicalBlockView>::iterator it = m_listCanonicalBlockView.begin(); it != m_listCanonicalBlockView.end(); ++it) {
        if (it->headerPtr->m_blockNumber == blockNumber) {
            Bpv7CanonicalBlockView & cbv = *m_listCanonicalBlockView.emplace(boost::next(it));
            cbv.dirty = true; //true will ignore and set actualSerializedBlockPtr after render
            cbv.markedForDeletion = false;
            cbv.headerPtr = std::move(headerPtr);
            return true;
        }
    }
    return false;
}
bool BundleViewV7::InsertMoveCanonicalBlockBeforeBlockNumber(std::unique_ptr<Bpv7CanonicalBlock> & headerPtr, const uint64_t blockNumber) {
    for (std::list<Bpv7CanonicalBlockView>::iterator it = m_listCanonicalBlockView.begin(); it != m_listCanonicalBlockView.end(); ++it) {
        if (it->headerPtr->m_blockNumber == blockNumber) {
            Bpv7CanonicalBlockView & cbv = *m_listCanonicalBlockView.emplace(it);
            cbv.dirty = true; //true will ignore and set actualSerializedBlockPtr after render
            cbv.markedForDeletion = false;
            cbv.headerPtr = std::move(headerPtr);
            return true;
        }
    }
    return false;
}

std::size_t BundleViewV7::GetCanonicalBlockCountByType(const BPV7_BLOCK_TYPE_CODE canonicalBlockTypeCode) const {
    std::size_t count = 0;
    for (std::list<Bpv7CanonicalBlockView>::const_iterator it = m_listCanonicalBlockView.cbegin(); it != m_listCanonicalBlockView.cend(); ++it) {
        count += (it->headerPtr->m_blockTypeCode == canonicalBlockTypeCode);
    }
    return count;
}
std::size_t BundleViewV7::GetNumCanonicalBlocks() const {
    return m_listCanonicalBlockView.size();
}
void BundleViewV7::GetCanonicalBlocksByType(const BPV7_BLOCK_TYPE_CODE canonicalBlockTypeCode, std::vector<Bpv7CanonicalBlockView*> & blocks) {
    blocks.clear();
    for (std::list<Bpv7CanonicalBlockView>::iterator it = m_listCanonicalBlockView.begin(); it != m_listCanonicalBlockView.end(); ++it) {
        if (it->headerPtr->m_blockTypeCode == canonicalBlockTypeCode) {
            blocks.push_back(&(*it));
        }
    }
}
uint64_t BundleViewV7::GetNextFreeCanonicalBlockNumber() const {
    uint64_t largestCanonicalBlockNumber = 1;
    for (std::list<Bpv7CanonicalBlockView>::const_iterator it = m_listCanonicalBlockView.cbegin(); it != m_listCanonicalBlockView.cend(); ++it) {
        const uint64_t blockNumber = it->headerPtr->m_blockNumber;
        if (largestCanonicalBlockNumber < blockNumber) {
            largestCanonicalBlockNumber = blockNumber;
        }
    }
    return largestCanonicalBlockNumber + 1;
}
std::size_t BundleViewV7::DeleteAllCanonicalBlocksByType(const BPV7_BLOCK_TYPE_CODE canonicalBlockTypeCode) {
    std::size_t count = 0;
    for (std::list<Bpv7CanonicalBlockView>::iterator it = m_listCanonicalBlockView.begin(); it != m_listCanonicalBlockView.end();) {
        if (it->headerPtr->m_blockTypeCode == canonicalBlockTypeCode) {
            it = m_listCanonicalBlockView.erase(it);
            ++count;
        }
    }
    return count;
}
bool BundleViewV7::LoadBundle(uint8_t * bundleData, const std::size_t size, const bool skipCrcVerifyInCanonicalBlocks, const bool loadPrimaryBlockOnly) {
    Reset();
    m_renderedBundle = boost::asio::buffer(bundleData, size);
    return Load(skipCrcVerifyInCanonicalBlocks, loadPrimaryBlockOnly);
}
bool BundleViewV7::SwapInAndLoadBundle(std::vector<uint8_t> & bundleData, const bool skipCrcVerifyInCanonicalBlocks, const bool loadPrimaryBlockOnly) {
    Reset();
    m_frontBuffer.swap(bundleData);
    m_renderedBundle = boost::asio::buffer(m_frontBuffer);
    return Load(skipCrcVerifyInCanonicalBlocks, loadPrimaryBlockOnly);
}
bool BundleViewV7::CopyAndLoadBundle(const uint8_t * bundleData, const std::size_t size, const bool skipCrcVerifyInCanonicalBlocks, const bool loadPrimaryBlockOnly) {
    Reset();
    m_frontBuffer.assign(bundleData, bundleData + size);
    m_renderedBundle = boost::asio::buffer(m_frontBuffer);
    return Load(skipCrcVerifyInCanonicalBlocks, loadPrimaryBlockOnly);
}
bool BundleViewV7::IsValid() const {
    if (GetCanonicalBlockCountByType(BPV7_BLOCK_TYPE_CODE::PAYLOAD) > 1) {
        return false;
    }
    return true;
}


void BundleViewV7::Reset() {
    m_primaryBlockView.header.SetZero();
    m_listCanonicalBlockView.clear();
    m_mapEncryptedBlockNumberToBcbPtr.clear();
    m_applicationDataUnitStartPtr = NULL;

    m_renderedBundle = boost::asio::buffer((void*)NULL, 0);
    m_frontBuffer.clear();
    m_backBuffer.clear();
}

