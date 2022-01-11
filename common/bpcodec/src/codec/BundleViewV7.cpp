#include "codec/BundleViewV7.h"
#include <iostream>
#include <boost/next_prior.hpp>
#include "Uri.h"

void BundleViewV7::Bpv7PrimaryBlockView::SetManuallyModified() {
    dirty = true;
}
void BundleViewV7::Bpv7CanonicalBlockView::SetManuallyModified() {
    dirty = true;
}


BundleViewV7::BundleViewV7() {}
BundleViewV7::~BundleViewV7() {}

bool BundleViewV7::Load() {
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
    if (m_primaryBlockView.header.m_bundleProcessingControlFlags & (BPV7_BUNDLEFLAG_ISFRAGMENT)) { //not currently supported
        return false;
    }
    m_primaryBlockView.actualSerializedPrimaryBlockPtr = boost::asio::buffer(serializationPrimaryBlockBeginPtr, decodedBlockSize);
    m_primaryBlockView.dirty = false;
    m_applicationDataUnitStartPtr = serializationPrimaryBlockBeginPtr + decodedBlockSize;
    //todo application data unit length?
    if (m_primaryBlockView.header.m_bundleProcessingControlFlags & (BPV7_BUNDLEFLAG_ADMINRECORD)) {
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
        Bpv7CanonicalBlock & canonical = cbv.header;
        if(!canonical.DeserializeBpv7(serialization, decodedBlockSize, bufferSize)) {
            return false;
        }
        serialization += decodedBlockSize;
        bufferSize -= decodedBlockSize;
        cbv.actualSerializedBlockPtr = boost::asio::buffer(serializationThisCanonicalBlockBeginPtr, decodedBlockSize);

        //The last such block MUST be a payload block;
        //the bundle MUST have exactly one payload block.  The payload block
        //SHALL be followed by a CBOR "break" stop code, terminating the
        //array.
        if (canonical.m_blockTypeCode == BPV7_BLOCKTYPE_PAYLOAD) { //last block
            if (bufferSize == 0) {
                return false;
            }
            --bufferSize;
            const uint8_t expectedCborBreakStopCode = *serialization++;
            if (expectedCborBreakStopCode != 0xff) { //0xff is break character
                return false;
            }
            const uint64_t bundleSerializedLength = serialization - serializationBase;

            return (bundleSerializedLength == m_renderedBundle.size()); //todo aggregation support
        }
        else if ((static_cast<uint64_t>(serialization - serializationBase)) >= m_renderedBundle.size()) {
            return false;
        }
    }


}
bool BundleViewV7::Render(const std::size_t maxBundleSizeBytes) {
    //first render to the back buffer, copying over non-dirty blocks from the m_renderedBundle which may be the front buffer or other memory from a load operation
    m_backBuffer.resize(maxBundleSizeBytes);
    uint8_t * serialization = &m_backBuffer[0];
    uint8_t * const serializationBase = serialization;
    *serialization++ = (4U << 5) | 31U; //major type 4, additional information 31 (Indefinite-Length Array)
    if (m_primaryBlockView.dirty) {
        //std::cout << "pd\n";
        const uint64_t sizeSerialized = m_primaryBlockView.header.SerializeBpv7(serialization);
        if (sizeSerialized == 0) {
            return false;
        }
        m_primaryBlockView.actualSerializedPrimaryBlockPtr = boost::asio::buffer(serialization, sizeSerialized);
        serialization += sizeSerialized;
        m_primaryBlockView.dirty = false;
    }
    else {
        //std::cout << "pnd\n";
        const std::size_t size = m_primaryBlockView.actualSerializedPrimaryBlockPtr.size();
        memcpy(serialization, m_primaryBlockView.actualSerializedPrimaryBlockPtr.data(), size);
        m_primaryBlockView.actualSerializedPrimaryBlockPtr = boost::asio::buffer(serialization, size);
        serialization += size;
    }
    if (m_primaryBlockView.header.m_bundleProcessingControlFlags & (BPV7_BUNDLEFLAG_ISFRAGMENT | BPV7_BUNDLEFLAG_ADMINRECORD)) {
        return false;
    }
    
    m_listCanonicalBlockView.remove_if([](const Bpv7CanonicalBlockView & v) { return v.markedForDeletion; }); //makes easier last block detection

    for (std::list<Bpv7CanonicalBlockView>::iterator it = m_listCanonicalBlockView.begin(); it != m_listCanonicalBlockView.end(); ++it) {
        const bool isLastBlock = (boost::next(it) == m_listCanonicalBlockView.end());
        if (isLastBlock && (it->header.m_blockTypeCode != BPV7_BLOCKTYPE_PAYLOAD)) {
            std::cerr << "error in BundleViewV7::Render: last block is not payload block\n";
            return false;
        }
        uint64_t sizeSerialized;
        if (it->dirty) { //always reencode canonical block if dirty
            sizeSerialized = it->header.SerializeBpv7(serialization);
            if (sizeSerialized < Bpv7CanonicalBlock::smallestSerializedCanonicalSize) {
                return false;
            }
            it->dirty = false;
        }
        else {
            //std::cout << "cnd\n";
            sizeSerialized = it->actualSerializedBlockPtr.size();
            memcpy(serialization, it->actualSerializedBlockPtr.data(), sizeSerialized);
        }
        it->actualSerializedBlockPtr = boost::asio::buffer(serialization, sizeSerialized);
        serialization += sizeSerialized;
    }
    m_backBuffer.resize(serialization - serializationBase);
    m_frontBuffer.swap(m_backBuffer); //m_frontBuffer is now the rendered bundle
    m_renderedBundle = boost::asio::buffer(m_frontBuffer);
    return true;
}

void BundleViewV7::AppendCanonicalBlock(const Bpv7CanonicalBlock & header) {
    m_listCanonicalBlockView.emplace_back();
    Bpv7CanonicalBlockView & cbv = m_listCanonicalBlockView.back();
    cbv.dirty = true; //true will ignore and set actualSerializedBlockPtr after render
    cbv.markedForDeletion = false;
    cbv.header = header;
}
std::size_t BundleViewV7::GetCanonicalBlockCountByType(const uint8_t canonicalBlockTypeCode) const {
    std::size_t count = 0;
    for (std::list<Bpv7CanonicalBlockView>::const_iterator it = m_listCanonicalBlockView.cbegin(); it != m_listCanonicalBlockView.cend(); ++it) {
        count += (it->header.m_blockTypeCode == canonicalBlockTypeCode);
    }
    return count;
}
std::size_t BundleViewV7::GetNumCanonicalBlocks() const {
    return m_listCanonicalBlockView.size();
}
void BundleViewV7::GetCanonicalBlocksByType(const uint8_t canonicalBlockTypeCode, std::vector<Bpv7CanonicalBlockView*> & blocks) {
    blocks.clear();
    for (std::list<Bpv7CanonicalBlockView>::iterator it = m_listCanonicalBlockView.begin(); it != m_listCanonicalBlockView.end(); ++it) {
        if (it->header.m_blockTypeCode == canonicalBlockTypeCode) {
            blocks.push_back(&(*it));
        }
    }
}
std::size_t BundleViewV7::DeleteAllCanonicalBlocksByType(const uint8_t canonicalBlockTypeCode) {
    std::size_t count = 0;
    for (std::list<Bpv7CanonicalBlockView>::iterator it = m_listCanonicalBlockView.begin(); it != m_listCanonicalBlockView.end();) {
        if (it->header.m_blockTypeCode == canonicalBlockTypeCode) {
            it = m_listCanonicalBlockView.erase(it);
            ++count;
        }
    }
    return count;
}
bool BundleViewV7::LoadBundle(uint8_t * bundleData, const std::size_t size) {
    Reset();
    m_renderedBundle = boost::asio::buffer(bundleData, size);
    return Load();
}
bool BundleViewV7::SwapInAndLoadBundle(std::vector<uint8_t> & bundleData) {
    Reset();
    m_frontBuffer.swap(bundleData);
    m_renderedBundle = boost::asio::buffer(m_frontBuffer);
    return Load();
}
bool BundleViewV7::CopyAndLoadBundle(const uint8_t * bundleData, const std::size_t size) {
    Reset();
    m_frontBuffer.assign(bundleData, bundleData + size);
    m_renderedBundle = boost::asio::buffer(m_frontBuffer);
    return Load();
}
bool BundleViewV7::IsValid() const {
    if (GetCanonicalBlockCountByType(BPV7_BLOCKTYPE_PAYLOAD) > 1) {
        return false;
    }
    return true;
}


void BundleViewV7::Reset() {
    m_primaryBlockView.header.SetZero();
    m_listCanonicalBlockView.clear();
    m_applicationDataUnitStartPtr = NULL;

    m_renderedBundle = boost::asio::buffer((void*)NULL, 0);
    m_frontBuffer.clear();
    m_backBuffer.clear();
}

