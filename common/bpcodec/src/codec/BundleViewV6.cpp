/**
 * @file BundleViewV6.cpp
 * @author  Brian Tomko <brian.j.tomko@nasa.gov>
 *
 * @copyright Copyright © 2021 United States Government as represented by
 * the National Aeronautics and Space Administration.
 * No copyright is claimed in the United States under Title 17, U.S.Code.
 * All Other Rights Reserved.
 *
 * @section LICENSE
 * Released under the NASA Open Source Agreement (NOSA)
 * See LICENSE.md in the source root directory for more information.
 */

#include "codec/BundleViewV6.h"
#include <iostream>
#include <boost/next_prior.hpp>
#include "Uri.h"

void BundleViewV6::Bpv6PrimaryBlockView::SetManuallyModified() {
    dirty = true;
}
void BundleViewV6::Bpv6CanonicalBlockView::SetManuallyModified() {
    dirty = true;
}
void BundleViewV6::Bpv6CanonicalBlockView::SetBlockProcessingControlFlagAndDirtyIfNecessary(const BPV6_BLOCKFLAG flag) {
    if (((static_cast<uint64_t>(headerPtr->m_blockProcessingControlFlags)) <= 127) && ((static_cast<uint64_t>(flag)) <= 127) && (actualSerializedBlockPtr.size() >= 2)) {
        headerPtr->m_blockProcessingControlFlags |= flag;
        uint8_t * const data = static_cast<uint8_t*>(const_cast<void*>(actualSerializedBlockPtr.data()));
        data[1] = static_cast<uint8_t>(headerPtr->m_blockProcessingControlFlags);
    }
    else {
        headerPtr->m_blockProcessingControlFlags |= flag;
        dirty = true;
    }
}
void BundleViewV6::Bpv6CanonicalBlockView::ClearBlockProcessingControlFlagAndDirtyIfNecessary(const BPV6_BLOCKFLAG flag) {
    if (((static_cast<uint64_t>(headerPtr->m_blockProcessingControlFlags)) <= 127) && ((static_cast<uint64_t>(flag)) <= 127) && (actualSerializedBlockPtr.size() >= 2)) {
        headerPtr->m_blockProcessingControlFlags &= (~flag);
        uint8_t * const data = static_cast<uint8_t*>(const_cast<void*>(actualSerializedBlockPtr.data()));
        data[1] = static_cast<uint8_t>(headerPtr->m_blockProcessingControlFlags);
    }
    else {
        headerPtr->m_blockProcessingControlFlags &= (~flag);
        dirty = true;
    }
}
bool BundleViewV6::Bpv6CanonicalBlockView::HasBlockProcessingControlFlagSet(const BPV6_BLOCKFLAG flag) const {
    return ((headerPtr->m_blockProcessingControlFlags & flag) != BPV6_BLOCKFLAG::NO_FLAGS_SET);
}

BundleViewV6::BundleViewV6() {}
BundleViewV6::~BundleViewV6() {}

bool BundleViewV6::Load(const bool loadPrimaryBlockOnly) {
    const uint8_t * const serializationBase = (uint8_t*)m_renderedBundle.data();
    uint8_t * serialization = (uint8_t*)m_renderedBundle.data();
    uint64_t bufferSize = m_renderedBundle.size();
    uint64_t decodedBlockSize;

    if (!m_primaryBlockView.header.DeserializeBpv6(serialization, decodedBlockSize, bufferSize)) {
        return false;
    }
    serialization += decodedBlockSize;
    bufferSize -= decodedBlockSize;

    const bool isFragment = ((m_primaryBlockView.header.m_bundleProcessingControlFlags & BPV6_BUNDLEFLAG::ISFRAGMENT) != BPV6_BUNDLEFLAG::NO_FLAGS_SET);
    if (isFragment) { //not currently supported
        return false;
    }
    m_primaryBlockView.actualSerializedPrimaryBlockPtr = boost::asio::buffer(m_renderedBundle.data(), decodedBlockSize);
    m_primaryBlockView.dirty = false;
    m_applicationDataUnitStartPtr = (static_cast<const uint8_t*>(m_renderedBundle.data())) + decodedBlockSize;
    const bool isAdminRecord = ((m_primaryBlockView.header.m_bundleProcessingControlFlags & BPV6_BUNDLEFLAG::ADMINRECORD) != BPV6_BUNDLEFLAG::NO_FLAGS_SET);

    if (loadPrimaryBlockOnly) {
        return true;
    }

    while (true) {
        uint8_t * const serializationThisCanonicalBlockBeginPtr = serialization;
        m_listCanonicalBlockView.emplace_back();
        Bpv6CanonicalBlockView & cbv = m_listCanonicalBlockView.back();
        cbv.dirty = false;
        cbv.markedForDeletion = false;
        if (!Bpv6CanonicalBlock::DeserializeBpv6(cbv.headerPtr, serialization,
            decodedBlockSize, bufferSize, isAdminRecord,
            m_blockNumberToRecycledCanonicalBlockArray))
        {
            return false;
        }
        serialization += decodedBlockSize;
        bufferSize -= decodedBlockSize;
        cbv.actualSerializedBlockPtr = boost::asio::buffer(serializationThisCanonicalBlockBeginPtr, decodedBlockSize);
        if (!cbv.headerPtr->Virtual_DeserializeExtensionBlockDataBpv6()) { //requires m_blockTypeSpecificDataPtr and m_blockTypeSpecificDataLength to be set (done in Bpv6CanonicalBlock::DeserializeBpv6)
            return false;
        }
        const uint64_t bundleSerializedLength = serialization - serializationBase;
        if ((cbv.headerPtr->m_blockProcessingControlFlags & BPV6_BLOCKFLAG::IS_LAST_BLOCK) != BPV6_BLOCKFLAG::NO_FLAGS_SET) {
            if (bundleSerializedLength != m_renderedBundle.size()) {
            }
            return (bundleSerializedLength == m_renderedBundle.size()); //todo aggregation support
        }
        else if (bundleSerializedLength >= m_renderedBundle.size()) {
            return false;
        }
    }

}

bool BundleViewV6::Render(const std::size_t maxBundleSizeBytes) {
    //first render to the back buffer, copying over non-dirty blocks from the m_renderedBundle which may be the front buffer or other memory from a load operation
    m_backBuffer.resize(maxBundleSizeBytes);
    uint64_t sizeSerialized;
    if (!Render(&m_backBuffer[0], sizeSerialized)) {
        return false;
    }
    m_backBuffer.resize(sizeSerialized);
    m_frontBuffer.swap(m_backBuffer); //m_frontBuffer is now the rendered bundle
    m_renderedBundle = boost::asio::buffer(m_frontBuffer);
    return true;
}

bool BundleViewV6::Render(uint8_t * serialization, uint64_t & sizeSerialized) {
    uint8_t * const serializationBase = serialization;
    if (m_primaryBlockView.dirty) {
        const uint64_t sizeSerialized = m_primaryBlockView.header.SerializeBpv6(serialization);
        if (sizeSerialized == 0) {
            return false;
        }
        m_primaryBlockView.actualSerializedPrimaryBlockPtr = boost::asio::buffer(serialization, sizeSerialized);
        serialization += sizeSerialized;
        m_primaryBlockView.dirty = false;
    }
    else {
        const std::size_t size = m_primaryBlockView.actualSerializedPrimaryBlockPtr.size();
        std::memmove(serialization, m_primaryBlockView.actualSerializedPrimaryBlockPtr.data(), size);
        m_primaryBlockView.actualSerializedPrimaryBlockPtr = boost::asio::buffer(serialization, size);
        serialization += size;
    }
    const bool isAdminRecord = ((m_primaryBlockView.header.m_bundleProcessingControlFlags & (BPV6_BUNDLEFLAG::ADMINRECORD)) != BPV6_BUNDLEFLAG::NO_FLAGS_SET);
    const bool isFragment = ((m_primaryBlockView.header.m_bundleProcessingControlFlags & (BPV6_BUNDLEFLAG::ISFRAGMENT)) != BPV6_BUNDLEFLAG::NO_FLAGS_SET);
    if (isFragment) {
        return false;
    }
    
    m_listCanonicalBlockView.remove_if([&](Bpv6CanonicalBlockView & v) {
        if (v.markedForDeletion && v.headerPtr) {
            const std::size_t blockTypeCode = static_cast<std::size_t>(v.headerPtr->m_blockTypeCode);
            if (blockTypeCode < MAX_NUM_BLOCK_TYPE_CODES) {
                m_blockNumberToRecycledCanonicalBlockArray[blockTypeCode] = std::move(v.headerPtr);
            }
        }
        return v.markedForDeletion;
    }); //makes easier last block detection

    for (canonical_block_view_list_t::iterator it = m_listCanonicalBlockView.begin(); it != m_listCanonicalBlockView.end(); ++it) {
        const bool isLastBlock = (boost::next(it) == m_listCanonicalBlockView.end());
        if (isLastBlock) {
            it->SetBlockProcessingControlFlagAndDirtyIfNecessary(BPV6_BLOCKFLAG::IS_LAST_BLOCK);
        }
        else {
            it->ClearBlockProcessingControlFlagAndDirtyIfNecessary(BPV6_BLOCKFLAG::IS_LAST_BLOCK);
        }
        uint64_t currentBlockSizeSerialized;
        if (it->dirty) {
            //always reencode canonical block if dirty
            currentBlockSizeSerialized = it->headerPtr->SerializeBpv6(serialization);
            if (currentBlockSizeSerialized <= 2) {
                return false;
            }
            it->dirty = false;
        }
        else {
            //update it->headerPtr->m_blockTypeSpecificDataPtr to point to new serialization location
            const uint8_t* const originalBlockPtr = (const uint8_t*)it->actualSerializedBlockPtr.data();
            const uint8_t* const originalBlockDataPtr = it->headerPtr->m_blockTypeSpecificDataPtr;
            const std::uintptr_t headerPartSize = originalBlockDataPtr - originalBlockPtr;
            it->headerPtr->m_blockTypeSpecificDataPtr = serialization + headerPartSize;

            currentBlockSizeSerialized = it->actualSerializedBlockPtr.size();
            std::memmove(serialization, it->actualSerializedBlockPtr.data(), currentBlockSizeSerialized);
        }
        it->actualSerializedBlockPtr = boost::asio::buffer(serialization, currentBlockSizeSerialized);
        serialization += currentBlockSizeSerialized;
    }
    sizeSerialized = (serialization - serializationBase);
    return true;
}
bool BundleViewV6::GetSerializationSize(uint64_t & serializationSize) const {
    serializationSize = 0;
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

    for (canonical_block_view_list_t::const_iterator it = m_listCanonicalBlockView.cbegin(); it != m_listCanonicalBlockView.cend(); ++it) {
        const bool isLastBlock = (boost::next(it) == m_listCanonicalBlockView.end());
        //DON'T NEED TO CHECK IS_LAST_BLOCK FLAG AS IT RESIDES WITHIN THE 1-BYTE SDNV SIZE
        uint64_t currentBlockSizeSerialized;
        if (it->markedForDeletion) {
            currentBlockSizeSerialized = 0;
        }
        else if (it->dirty) { //always reencode canonical block if dirty
            currentBlockSizeSerialized = it->headerPtr->GetSerializationSize();
            if (currentBlockSizeSerialized <= 2) {
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

void BundleViewV6::AppendMoveCanonicalBlock(std::unique_ptr<Bpv6CanonicalBlock>&& headerPtr) {
    m_listCanonicalBlockView.emplace_back();
    Bpv6CanonicalBlockView & cbv = m_listCanonicalBlockView.back();
    cbv.dirty = true; //true will ignore and set actualSerializedBlockPtr after render
    cbv.markedForDeletion = false;
    cbv.headerPtr = std::move(headerPtr);
}
void BundleViewV6::PrependMoveCanonicalBlock(std::unique_ptr<Bpv6CanonicalBlock>&& headerPtr) {
    m_listCanonicalBlockView.emplace_front();
    Bpv6CanonicalBlockView & cbv = m_listCanonicalBlockView.front();
    cbv.dirty = true; //true will ignore and set actualSerializedBlockPtr after render
    cbv.markedForDeletion = false;
    cbv.headerPtr = std::move(headerPtr);
}
std::size_t BundleViewV6::GetCanonicalBlockCountByType(const BPV6_BLOCK_TYPE_CODE canonicalBlockTypeCode) const {
    std::size_t count = 0;
    for (canonical_block_view_list_t::const_iterator it = m_listCanonicalBlockView.cbegin(); it != m_listCanonicalBlockView.cend(); ++it) {
        count += (it->headerPtr->m_blockTypeCode == canonicalBlockTypeCode);
    }
    return count;
}
std::size_t BundleViewV6::GetNumCanonicalBlocks() const {
    return m_listCanonicalBlockView.size();
}
void BundleViewV6::GetCanonicalBlocksByType(const BPV6_BLOCK_TYPE_CODE canonicalBlockTypeCode, std::vector<Bpv6CanonicalBlockView*> & blocks) {
    blocks.clear();
    for (canonical_block_view_list_t::iterator it = m_listCanonicalBlockView.begin(); it != m_listCanonicalBlockView.end(); ++it) {
        if (it->headerPtr->m_blockTypeCode == canonicalBlockTypeCode) {
            blocks.push_back(&(*it));
        }
    }
}
std::size_t BundleViewV6::DeleteAllCanonicalBlocksByType(const BPV6_BLOCK_TYPE_CODE canonicalBlockTypeCode) {
    std::size_t count = 0;
    m_listCanonicalBlockView.remove_if([&](Bpv6CanonicalBlockView& v) {
        const bool doDelete = (v.headerPtr) && (v.headerPtr->m_blockTypeCode == canonicalBlockTypeCode);
        if (doDelete) {
            const std::size_t blockTypeCode = static_cast<std::size_t>(v.headerPtr->m_blockTypeCode);
            if (blockTypeCode < MAX_NUM_BLOCK_TYPE_CODES) {
                m_blockNumberToRecycledCanonicalBlockArray[blockTypeCode] = std::move(v.headerPtr);
            }
        }
        count += doDelete;
        return doDelete;
    });
    return count;
}
bool BundleViewV6::LoadBundle(uint8_t * bundleData, const std::size_t size, const bool loadPrimaryBlockOnly) {
    Reset();
    m_renderedBundle = boost::asio::buffer(bundleData, size);
    return Load(loadPrimaryBlockOnly);
}
bool BundleViewV6::SwapInAndLoadBundle(padded_vector_uint8_t& bundleData, const bool loadPrimaryBlockOnly) {
    Reset();
    m_frontBuffer.swap(bundleData);
    m_renderedBundle = boost::asio::buffer(m_frontBuffer);
    return Load(loadPrimaryBlockOnly);
}
bool BundleViewV6::CopyAndLoadBundle(const uint8_t * bundleData, const std::size_t size, const bool loadPrimaryBlockOnly) {
    Reset();
    m_frontBuffer.assign(bundleData, bundleData + size);
    m_renderedBundle = boost::asio::buffer(m_frontBuffer);
    return Load(loadPrimaryBlockOnly);
}
bool BundleViewV6::IsValid() const {
    if (GetCanonicalBlockCountByType(BPV6_BLOCK_TYPE_CODE::PAYLOAD) > 1) {
        return false;
    }
    return true;
}


void BundleViewV6::Reset() {
    m_primaryBlockView.header.SetZero();

    //clear the list, recycling content (also implicitly does m_listCanonicalBlockView.clear();)
    m_listCanonicalBlockView.remove_if([&](Bpv6CanonicalBlockView& v) {
        if (v.headerPtr) {
            const std::size_t blockTypeCode = static_cast<std::size_t>(v.headerPtr->m_blockTypeCode);
            if (blockTypeCode < MAX_NUM_BLOCK_TYPE_CODES) {
                m_blockNumberToRecycledCanonicalBlockArray[blockTypeCode] = std::move(v.headerPtr);
            }
        }
        return true; //always delete
    });

    m_applicationDataUnitStartPtr = NULL;

    m_renderedBundle = boost::asio::buffer((void*)NULL, 0);
    m_frontBuffer.clear();
    m_backBuffer.clear();
}

