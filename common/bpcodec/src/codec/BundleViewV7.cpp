/**
 * @file BundleViewV7.cpp
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

#include "codec/BundleViewV7.h"
#include <iostream>
#include <boost/next_prior.hpp>
#include "Uri.h"
#include "PaddedVectorUint8.h"
#include "Logger.h"


#include <boost/multiprecision/cpp_int.hpp>
#include <boost/multiprecision/detail/bitscan.hpp>
#ifdef USE_BITTEST
# include <immintrin.h>
# ifdef HAVE_INTRIN_H
#  include <intrin.h>
# endif
# ifdef HAVE_X86INTRIN_H
#  include <x86intrin.h>
# endif
#elif defined(USE_ANDN)
# include <immintrin.h>
# if defined(__GNUC__) && defined(HAVE_X86INTRIN_H)
#  include <x86intrin.h>
#  define _andn_u64(a, b)   (__andn_u64((a), (b)))
# elif defined(_MSC_VER)
#  include <ammintrin.h>
# endif
#endif //USE_BITTEST or USE_ANDN

static constexpr hdtn::Logger::SubProcess subprocess = hdtn::Logger::SubProcess::none;

void BundleViewV7::Bpv7PrimaryBlockView::SetManuallyModified() {
    dirty = true;
}
void BundleViewV7::Bpv7CanonicalBlockView::SetManuallyModified() {
    dirty = true;
}
BundleViewV7::Bpv7CanonicalBlockView::Bpv7CanonicalBlockView() : dirty(false), markedForDeletion(false), isEncrypted(false) {}

static constexpr uint64_t FREE_CANONICAL_BLOCK_INIT_MASK = UINT64_MAX - 3u; //reserve blocks 0 and 1 for primary and payload respectively
BundleViewV7::BundleViewV7() : m_nextFreeCanonicalBlockNumberMask(FREE_CANONICAL_BLOCK_INIT_MASK), m_applicationDataUnitStartPtr(NULL) {}
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
        if(!Bpv7CanonicalBlock::DeserializeBpv7(cbv.headerPtr, serialization, decodedBlockSize, bufferSize,
            skipCrcVerifyInCanonicalBlocks, isAdminRecord, m_blockNumberToRecycledCanonicalBlockArray, &m_recycledAdminRecord))
        {
            return false;
        }
        ReserveBlockNumber(cbv.headerPtr->m_blockNumber);
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

    for (canonical_block_view_list_t::iterator it = m_listCanonicalBlockView.begin(); it != m_listCanonicalBlockView.end(); ++it) {
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
        const uint64_t sizeSerializedPrimary = m_primaryBlockView.header.SerializeBpv7(serialization);
        if (sizeSerializedPrimary == 0) {
            return false;
        }
        m_primaryBlockView.actualSerializedPrimaryBlockPtr = boost::asio::buffer(serialization, sizeSerializedPrimary);
        serialization += sizeSerializedPrimary;
        m_primaryBlockView.dirty = false;
    }
    else {
        const std::size_t size = m_primaryBlockView.actualSerializedPrimaryBlockPtr.size();
        std::memmove(serialization, m_primaryBlockView.actualSerializedPrimaryBlockPtr.data(), size);
        m_primaryBlockView.actualSerializedPrimaryBlockPtr = boost::asio::buffer(serialization, size);
        serialization += size;
    }
    
    const bool isFragment = ((m_primaryBlockView.header.m_bundleProcessingControlFlags & (BPV7_BUNDLEFLAG::ISFRAGMENT)) != BPV7_BUNDLEFLAG::NO_FLAGS_SET);
    if (isFragment) {
        return false;
    }
    
    m_listCanonicalBlockView.remove_if([&](Bpv7CanonicalBlockView & v) {
        if (v.markedForDeletion) {
            if (v.headerPtr) {
                FreeBlockNumber(v.headerPtr->m_blockNumber);
                const std::size_t blockTypeCode = static_cast<std::size_t>(v.headerPtr->m_blockTypeCode);
                const bool isAdminRecord = (v.headerPtr->m_blockTypeCode == BPV7_BLOCK_TYPE_CODE::PAYLOAD)
                    && (dynamic_cast<Bpv7AdministrativeRecord*>(v.headerPtr.get()) != NULL);
                if (isAdminRecord) {
                    m_recycledAdminRecord = std::move(v.headerPtr);
                }
                else if (blockTypeCode < MAX_NUM_BLOCK_TYPE_CODES) {
                    m_blockNumberToRecycledCanonicalBlockArray[blockTypeCode] = std::move(v.headerPtr);
                }
            }
        }
        return v.markedForDeletion;
    }); //makes easier last block detection

    for (canonical_block_view_list_t::iterator it = m_listCanonicalBlockView.begin(); it != m_listCanonicalBlockView.end(); ++it) {
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
            //update it->headerPtr->m_dataPtr to point to new serialization location
            const uint8_t* const originalBlockPtr = (const uint8_t*)it->actualSerializedBlockPtr.data();
            const uint8_t* const originalBlockDataPtr = it->headerPtr->m_dataPtr;
            const std::uintptr_t headerPartSize = originalBlockDataPtr - originalBlockPtr;
            it->headerPtr->m_dataPtr = serialization + headerPartSize;

            currentBlockSizeSerialized = it->actualSerializedBlockPtr.size();
            std::memmove(serialization, it->actualSerializedBlockPtr.data(), currentBlockSizeSerialized);
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
    
    for (canonical_block_view_list_t::const_iterator it = m_listCanonicalBlockView.cbegin(); it != m_listCanonicalBlockView.cend(); ++it) {
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
            currentBlockSizeSerialized = it->headerPtr->GetSerializationSize(it->isEncrypted);
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

void BundleViewV7::AppendMoveCanonicalBlock(std::unique_ptr<Bpv7CanonicalBlock>&& headerPtr) {
    m_listCanonicalBlockView.emplace_back();
    Bpv7CanonicalBlockView & cbv = m_listCanonicalBlockView.back();
    cbv.dirty = true; //true will ignore and set actualSerializedBlockPtr after render
    cbv.markedForDeletion = false;
    ReserveBlockNumber(headerPtr->m_blockNumber);
    cbv.headerPtr = std::move(headerPtr);
}
void BundleViewV7::PrependMoveCanonicalBlock(std::unique_ptr<Bpv7CanonicalBlock>&& headerPtr) {
    m_listCanonicalBlockView.emplace_front();
    Bpv7CanonicalBlockView & cbv = m_listCanonicalBlockView.front();
    cbv.dirty = true; //true will ignore and set actualSerializedBlockPtr after render
    cbv.markedForDeletion = false;
    ReserveBlockNumber(headerPtr->m_blockNumber);
    cbv.headerPtr = std::move(headerPtr);
}
bool BundleViewV7::InsertMoveCanonicalBlockAfterBlockNumber(std::unique_ptr<Bpv7CanonicalBlock>&& headerPtr, const uint64_t blockNumber) {
    for (canonical_block_view_list_t::iterator it = m_listCanonicalBlockView.begin(); it != m_listCanonicalBlockView.end(); ++it) {
        if (it->headerPtr->m_blockNumber == blockNumber) {
            Bpv7CanonicalBlockView & cbv = *m_listCanonicalBlockView.emplace(boost::next(it));
            cbv.dirty = true; //true will ignore and set actualSerializedBlockPtr after render
            cbv.markedForDeletion = false;
            ReserveBlockNumber(headerPtr->m_blockNumber);
            cbv.headerPtr = std::move(headerPtr);
            return true;
        }
    }
    return false;
}
bool BundleViewV7::InsertMoveCanonicalBlockBeforeBlockNumber(std::unique_ptr<Bpv7CanonicalBlock>&& headerPtr, const uint64_t blockNumber) {
    for (canonical_block_view_list_t::iterator it = m_listCanonicalBlockView.begin(); it != m_listCanonicalBlockView.end(); ++it) {
        if (it->headerPtr->m_blockNumber == blockNumber) {
            Bpv7CanonicalBlockView & cbv = *m_listCanonicalBlockView.emplace(it);
            cbv.dirty = true; //true will ignore and set actualSerializedBlockPtr after render
            cbv.markedForDeletion = false;
            ReserveBlockNumber(headerPtr->m_blockNumber);
            cbv.headerPtr = std::move(headerPtr);
            return true;
        }
    }
    return false;
}

std::size_t BundleViewV7::GetCanonicalBlockCountByType(const BPV7_BLOCK_TYPE_CODE canonicalBlockTypeCode) const {
    std::size_t count = 0;
    for (canonical_block_view_list_t::const_iterator it = m_listCanonicalBlockView.cbegin(); it != m_listCanonicalBlockView.cend(); ++it) {
        count += (it->headerPtr->m_blockTypeCode == canonicalBlockTypeCode);
    }
    return count;
}
std::size_t BundleViewV7::GetNumCanonicalBlocks() const {
    return m_listCanonicalBlockView.size();
}
void BundleViewV7::GetCanonicalBlocksByType(const BPV7_BLOCK_TYPE_CODE canonicalBlockTypeCode, std::vector<Bpv7CanonicalBlockView*> & blocks) {
    blocks.clear();
    for (canonical_block_view_list_t::iterator it = m_listCanonicalBlockView.begin(); it != m_listCanonicalBlockView.end(); ++it) {
        if (it->headerPtr->m_blockTypeCode == canonicalBlockTypeCode) {
            blocks.push_back(&(*it));
        }
    }
}
BundleViewV7::Bpv7CanonicalBlockView* BundleViewV7::GetCanonicalBlockByBlockNumber(const uint64_t blockNumber) {
    for (canonical_block_view_list_t::iterator it = m_listCanonicalBlockView.begin(); it != m_listCanonicalBlockView.end(); ++it) {
        if (it->headerPtr->m_blockNumber == blockNumber) {
            return (&(*it));
        }
    }
    return NULL;
}
void BundleViewV7::ReserveBlockNumber(const uint64_t blockNumber) {
    if (blockNumber <= 63) {
#ifdef USE_BITTEST
        _bittestandreset64((int64_t*)&m_nextFreeCanonicalBlockNumberMask, blockNumber);
#else
        const uint64_t mask64 = (((uint64_t)1) << blockNumber);
# if defined(USE_ANDN)
        m_nextFreeCanonicalBlockNumberMask = _andn_u64(mask64, m_nextFreeCanonicalBlockNumberMask);
# else
        m_nextFreeCanonicalBlockNumberMask &= (~mask64);
# endif
#endif
    }
}
void BundleViewV7::FreeBlockNumber(const uint64_t blockNumber) {
    if (blockNumber <= 63) {
#ifdef USE_BITTEST
        _bittestandset64((int64_t*)&m_nextFreeCanonicalBlockNumberMask, blockNumber);
#else
        const uint64_t mask64 = (((uint64_t)1) << blockNumber);
        m_nextFreeCanonicalBlockNumberMask |= mask64;
#endif
    }
}
uint64_t BundleViewV7::GetNextFreeCanonicalBlockNumber() const {
    //The block number of the
    //primary block is implicitly zero; the block numbers of all other
    //blocks are explicitly stated in block headers as noted below.  Block
    //numbering is unrelated to the order in which blocks are sequenced in
    //the bundle.  The block number of the payload block is always 1.
    if (m_nextFreeCanonicalBlockNumberMask) { //blocks between 0 and 63 inclusive
        const unsigned int firstFreeBlockNumber = boost::multiprecision::detail::find_lsb<uint64_t>(m_nextFreeCanonicalBlockNumberMask);
        return firstFreeBlockNumber;
    }
    else { //blocks >= 64
        uint64_t largestCanonicalBlockNumber = 63;
        for (canonical_block_view_list_t::const_iterator it = m_listCanonicalBlockView.cbegin(); it != m_listCanonicalBlockView.cend(); ++it) {
            const uint64_t blockNumber = it->headerPtr->m_blockNumber;
            if (largestCanonicalBlockNumber < blockNumber) {
                largestCanonicalBlockNumber = blockNumber;
            }
        }
        return largestCanonicalBlockNumber + 1;
    }
}
std::size_t BundleViewV7::DeleteAllCanonicalBlocksByType(const BPV7_BLOCK_TYPE_CODE canonicalBlockTypeCode) {
    std::size_t count = 0;
    m_listCanonicalBlockView.remove_if([&](Bpv7CanonicalBlockView& v) {
        const bool doDelete = (v.headerPtr) && (v.headerPtr->m_blockTypeCode == canonicalBlockTypeCode);
        if (doDelete) {
            this->FreeBlockNumber(v.headerPtr->m_blockNumber);
            const std::size_t blockTypeCode = static_cast<std::size_t>(v.headerPtr->m_blockTypeCode);
            const bool isAdminRecord = (v.headerPtr->m_blockTypeCode == BPV7_BLOCK_TYPE_CODE::PAYLOAD)
                && (dynamic_cast<Bpv7AdministrativeRecord*>(v.headerPtr.get()) != NULL);
            if (isAdminRecord) {
                m_recycledAdminRecord = std::move(v.headerPtr);
            }
            else if (blockTypeCode < MAX_NUM_BLOCK_TYPE_CODES) {
                m_blockNumberToRecycledCanonicalBlockArray[blockTypeCode] = std::move(v.headerPtr);
            }
        }
        count += doDelete;
        return doDelete;
    });
    return count;
}
bool BundleViewV7::LoadBundle(uint8_t * bundleData, const std::size_t size, const bool skipCrcVerifyInCanonicalBlocks, const bool loadPrimaryBlockOnly) {
    Reset();
    m_renderedBundle = boost::asio::buffer(bundleData, size);
    return Load(skipCrcVerifyInCanonicalBlocks, loadPrimaryBlockOnly);
}
bool BundleViewV7::SwapInAndLoadBundle(padded_vector_uint8_t& bundleData, const bool skipCrcVerifyInCanonicalBlocks, const bool loadPrimaryBlockOnly) {
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

bool BundleViewV7::GetPayloadSize(uint64_t &payloadSizeBytes) {
    std::vector<Bpv7CanonicalBlockView *> blocks;
    GetCanonicalBlocksByType(BPV7_BLOCK_TYPE_CODE::PAYLOAD, blocks);
    if(blocks.size() != 1 || !blocks[0]) {
        return false;
    }
    BundleViewV7::Bpv7CanonicalBlockView & payload = *blocks[0];
    if(!payload.headerPtr) {
        return false;
    }
    payloadSizeBytes = payload.headerPtr->m_dataLength;
    return true;
}

void BundleViewV7::Reset() {
    m_primaryBlockView.header.SetZero();

    //clear the list, recycling content (also implicitly does m_listCanonicalBlockView.clear();)
    m_listCanonicalBlockView.remove_if([&](Bpv7CanonicalBlockView& v) {
        if (v.headerPtr) {
            const std::size_t blockTypeCode = static_cast<std::size_t>(v.headerPtr->m_blockTypeCode);
            const bool isAdminRecord = (v.headerPtr->m_blockTypeCode == BPV7_BLOCK_TYPE_CODE::PAYLOAD)
                && (dynamic_cast<Bpv7AdministrativeRecord*>(v.headerPtr.get()) != NULL);
            if (isAdminRecord) {
                m_recycledAdminRecord = std::move(v.headerPtr);
            }
            else if (blockTypeCode < MAX_NUM_BLOCK_TYPE_CODES) {
                m_blockNumberToRecycledCanonicalBlockArray[blockTypeCode] = std::move(v.headerPtr);
            }
        }
        return true; //always delete
    });
    
    m_mapEncryptedBlockNumberToBcbPtr.clear();
    m_applicationDataUnitStartPtr = NULL;
    m_nextFreeCanonicalBlockNumberMask = FREE_CANONICAL_BLOCK_INIT_MASK;

    m_renderedBundle = boost::asio::buffer((void*)NULL, 0);
    m_frontBuffer.clear();
    m_backBuffer.clear();
}

