/**
 * @file BundleStorageManagerBase.cpp
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

#include "BundleStorageManagerBase.h"
#include <string>
#include <boost/filesystem.hpp>
#include <memory>
#include <boost/make_unique.hpp>
#include <boost/endian/conversion.hpp>
#include "codec/BundleViewV6.h"
#include "codec/BundleViewV7.h"

 //#ifdef _MSC_VER //Windows tests
 //static const char * FILE_PATHS[NUM_STORAGE_THREADS] = { "map0.bin", "map1.bin", "map2.bin", "map3.bin" };
 //#else //Lambda Linux tests
 //static const char * FILE_PATHS[NUM_STORAGE_THREADS] = { "/mnt/sda1/test/map0.bin", "/mnt/sdb1/test/map1.bin", "/mnt/sdc1/test/map2.bin", "/mnt/sdd1/test/map3.bin" };
 //#endif

static constexpr hdtn::Logger::SubProcess subprocess = hdtn::Logger::SubProcess::storage;

struct StorageSegmentHeader {
    StorageSegmentHeader();
    void ToLittleEndianInplace();
    void ToNativeEndianInplace();
    uint64_t bundleSizeBytes;
    uint64_t custodyId;
    segment_id_t nextSegmentId;
};
StorageSegmentHeader::StorageSegmentHeader() {}
BOOST_FORCEINLINE void StorageSegmentHeader::ToLittleEndianInplace() {
    boost::endian::native_to_little_inplace(bundleSizeBytes);
    boost::endian::native_to_little_inplace(custodyId);
    boost::endian::native_to_little_inplace(nextSegmentId);
}
BOOST_FORCEINLINE void StorageSegmentHeader::ToNativeEndianInplace() {
    boost::endian::little_to_native_inplace(bundleSizeBytes);
    boost::endian::little_to_native_inplace(custodyId);
    boost::endian::little_to_native_inplace(nextSegmentId);
}

BundleStorageManagerSession_ReadFromDisk::BundleStorageManagerSession_ReadFromDisk() :
    readCache(new volatile uint8_t[READ_CACHE_NUM_SEGMENTS_PER_SESSION * SEGMENT_SIZE]) {}

BundleStorageManagerSession_ReadFromDisk::~BundleStorageManagerSession_ReadFromDisk() {}

BundleStorageManagerBase::BundleStorageManagerBase() : BundleStorageManagerBase("storageConfig.json") {}

BundleStorageManagerBase::BundleStorageManagerBase(const boost::filesystem::path& jsonConfigFilePath) :
    BundleStorageManagerBase(StorageConfig::CreateFromJsonFilePath(jsonConfigFilePath))
{
    if (!m_storageConfigPtr) {
        LOG_ERROR(subprocess) << "cannot open storage json config file: " << jsonConfigFilePath;
        return;
    }
}

BundleStorageManagerBase::BundleStorageManagerBase(const StorageConfig_ptr & storageConfigPtr) :
    m_storageConfigPtr(storageConfigPtr),
    M_NUM_STORAGE_DISKS((m_storageConfigPtr) ? static_cast<unsigned int>(m_storageConfigPtr->m_storageDiskConfigVector.size()) : 1),
    M_TOTAL_STORAGE_CAPACITY_BYTES((m_storageConfigPtr) ? m_storageConfigPtr->m_totalStorageCapacityBytes : 1),
    M_MAX_SEGMENTS(M_TOTAL_STORAGE_CAPACITY_BYTES / SEGMENT_SIZE),
    m_memoryManager(M_MAX_SEGMENTS),
    m_filePathsVec(M_NUM_STORAGE_DISKS),
    m_circularIndexBuffersVec(M_NUM_STORAGE_DISKS, CircularIndexBufferSingleProducerSingleConsumerConfigurable(CIRCULAR_INDEX_BUFFER_SIZE)),
    m_circularBufferBlockDataPtr(NULL),
    m_circularBufferSegmentIdsPtr(NULL),
    m_circularBufferIsReadCompletedPointers(), //zero initialize
    m_circularBufferReadFromStoragePointers(), //zero initialize
    m_autoDeleteFilesOnExit((m_storageConfigPtr) ? m_storageConfigPtr->m_autoDeleteFilesOnExit : false),
    m_successfullyRestoredFromDisk(false),
    m_totalBundlesRestored(0),
    m_totalBytesRestored(0),
    m_totalSegmentsRestored(0)
{
    if (!m_storageConfigPtr) {
        return;
    }

    if (m_storageConfigPtr->m_tryToRestoreFromDisk) {
        m_successfullyRestoredFromDisk = RestoreFromDisk(&m_totalBundlesRestored, &m_totalBytesRestored, &m_totalSegmentsRestored);
    }


    for (unsigned int diskId = 0; diskId < M_NUM_STORAGE_DISKS; ++diskId) {
        m_filePathsVec[diskId] = boost::filesystem::path(m_storageConfigPtr->m_storageDiskConfigVector[diskId].storeFilePath);
    }


    if (M_MAX_SEGMENTS > MAX_MEMORY_MANAGER_SEGMENTS) {
        LOG_ERROR(subprocess) << "MAX SEGMENTS GREATER THAN WHAT MEMORY MANAGER CAN HANDLE";
        return;
    }

    m_circularBufferBlockDataPtr = (uint8_t*)malloc(CIRCULAR_INDEX_BUFFER_SIZE * M_NUM_STORAGE_DISKS * SEGMENT_SIZE * sizeof(uint8_t));
    m_circularBufferSegmentIdsPtr = (segment_id_t*)malloc(CIRCULAR_INDEX_BUFFER_SIZE * M_NUM_STORAGE_DISKS * sizeof(segment_id_t));


}

BundleStorageManagerBase::~BundleStorageManagerBase() {

    free(m_circularBufferBlockDataPtr);
    free(m_circularBufferSegmentIdsPtr);

    for (unsigned int diskId = 0; diskId < M_NUM_STORAGE_DISKS; ++diskId) {
        const boost::filesystem::path & p = m_filePathsVec[diskId];

        if (m_autoDeleteFilesOnExit && boost::filesystem::exists(p)) {
            if (boost::filesystem::remove(p)) {
                LOG_DEBUG(subprocess) << "deleted " << p;
            }
            else {
                LOG_ERROR(subprocess) << "unable to delete " << p;
            }
        }
    }
}


const MemoryManagerTreeArray& BundleStorageManagerBase::GetMemoryManagerConstRef() const {
    return m_memoryManager;
}
const BundleStorageCatalog& BundleStorageManagerBase::GetBundleStorageCatalogConstRef() const {
    return m_bundleStorageCatalog;
}
uint64_t BundleStorageManagerBase::GetFreeSpaceBytes() const noexcept {
    return (M_MAX_SEGMENTS - m_memoryManager.GetNumAllocatedSegments_NotThreadSafe()) * SEGMENT_SIZE;
}
uint64_t BundleStorageManagerBase::GetUsedSpaceBytes() const noexcept {
    return m_memoryManager.GetNumAllocatedSegments_NotThreadSafe() * SEGMENT_SIZE;
}
uint64_t BundleStorageManagerBase::GetTotalCapacityBytes() const noexcept {
    return M_MAX_SEGMENTS * SEGMENT_SIZE;
}


uint64_t BundleStorageManagerBase::Push(BundleStorageManagerSession_WriteToDisk & session, const PrimaryBlock & bundlePrimaryBlock, const uint64_t bundleSizeBytes) {
    catalog_entry_t & catalogEntry = session.catalogEntry;
    segment_id_chain_vec_t & segmentIdChainVec = catalogEntry.segmentIdChainVec;
    const uint64_t totalSegmentsRequired = (bundleSizeBytes / BUNDLE_STORAGE_PER_SEGMENT_SIZE) + ((bundleSizeBytes % BUNDLE_STORAGE_PER_SEGMENT_SIZE) == 0 ? 0 : 1);

    catalogEntry.Init(bundlePrimaryBlock, bundleSizeBytes, totalSegmentsRequired, NULL); //NULL replaced later at CatalogIncomingBundleForStore
    session.nextLogicalSegment = 0;


    if (m_memoryManager.AllocateSegments_ThreadSafe(segmentIdChainVec)) {
        return totalSegmentsRequired;
    }

    return 0;
}

int BundleStorageManagerBase::PushSegment(BundleStorageManagerSession_WriteToDisk & session, const PrimaryBlock & bundlePrimaryBlock,
    const uint64_t custodyId, const uint8_t * buf, std::size_t size)
{
    catalog_entry_t & catalogEntry = session.catalogEntry;
    segment_id_chain_vec_t & segmentIdChainVec = catalogEntry.segmentIdChainVec;

    if (session.nextLogicalSegment >= segmentIdChainVec.size()) {
        return 0;
    }
    StorageSegmentHeader storageSegmentHeader;
    storageSegmentHeader.bundleSizeBytes = (session.nextLogicalSegment == 0) ? catalogEntry.bundleSizeBytes : UINT64_MAX;
    const segment_id_t segmentId = segmentIdChainVec[session.nextLogicalSegment++];
    const unsigned int diskIndex = segmentId % M_NUM_STORAGE_DISKS;
    CircularIndexBufferSingleProducerSingleConsumerConfigurable & cb = m_circularIndexBuffersVec[diskIndex];
    unsigned int produceIndex = cb.GetIndexForWrite();
    while (produceIndex == CIRCULAR_INDEX_BUFFER_FULL) { //if full, wait until not full	
        //try again, but with the mutex
        boost::mutex::scoped_lock lockMainThread(m_mutexMainThread);
        produceIndex = cb.GetIndexForWrite();
        if (produceIndex == CIRCULAR_INDEX_BUFFER_FULL) { //if full again (lock mutex (above) before checking condition)
            m_conditionVariableMainThread.wait(lockMainThread); // call lock.unlock() and blocks the current thread
            //thread is now unblocked, and the lock is reacquired by invoking lock.lock()
            produceIndex = cb.GetIndexForWrite(); //should definitely have an index now (prevents an extra lock, unlock operation)
        }
    }

    uint8_t * const circularBufferBlockDataPtr = &m_circularBufferBlockDataPtr[diskIndex * CIRCULAR_INDEX_BUFFER_SIZE * SEGMENT_SIZE];
    segment_id_t * const circularBufferSegmentIdsPtr = &m_circularBufferSegmentIdsPtr[diskIndex * CIRCULAR_INDEX_BUFFER_SIZE];


    uint8_t * const dataCb = &circularBufferBlockDataPtr[produceIndex * SEGMENT_SIZE];
    circularBufferSegmentIdsPtr[produceIndex] = segmentId;
    m_circularBufferReadFromStoragePointers[diskIndex * CIRCULAR_INDEX_BUFFER_SIZE + produceIndex] = NULL; //isWriteToDisk = true

    storageSegmentHeader.nextSegmentId = (session.nextLogicalSegment == segmentIdChainVec.size()) ? SEGMENT_ID_LAST : segmentIdChainVec[session.nextLogicalSegment];
    storageSegmentHeader.custodyId = custodyId;
    storageSegmentHeader.ToLittleEndianInplace(); //should optimize out and do nothing
    memcpy(dataCb, &storageSegmentHeader, SEGMENT_RESERVED_SPACE);
    memcpy(dataCb + SEGMENT_RESERVED_SPACE, buf, size);

    CommitWriteAndNotifyDiskOfWorkToDo_ThreadSafe(diskIndex);
    if (session.nextLogicalSegment == segmentIdChainVec.size()) {
        m_bundleStorageCatalog.CatalogIncomingBundleForStore(catalogEntry, bundlePrimaryBlock, custodyId, BundleStorageCatalog::DUPLICATE_EXPIRY_ORDER::FIFO);
    }

    return 1;
}

//return total bytes pushed
uint64_t BundleStorageManagerBase::PushAllSegments(BundleStorageManagerSession_WriteToDisk & session,
    const PrimaryBlock & bundlePrimaryBlock,
    const uint64_t custodyId, const uint8_t * allData, const std::size_t allDataSize)
{
    uint64_t totalBytesCopied = 0;
    const uint64_t totalSegmentsRequired = session.catalogEntry.segmentIdChainVec.size();
    for (uint64_t i = 0; i < totalSegmentsRequired; ++i) {
        std::size_t bytesToCopy = BUNDLE_STORAGE_PER_SEGMENT_SIZE;
        if (i == totalSegmentsRequired - 1) {
            uint64_t modBytes = (allDataSize % BUNDLE_STORAGE_PER_SEGMENT_SIZE);
            if (modBytes != 0) {
                bytesToCopy = modBytes;
            }
        }

        if (!PushSegment(session, bundlePrimaryBlock, custodyId, &allData[i*BUNDLE_STORAGE_PER_SEGMENT_SIZE], bytesToCopy)) {
            return 0;
        }
        totalBytesCopied += bytesToCopy;
    }
    return totalBytesCopied;
}


uint64_t BundleStorageManagerBase::PopTop(BundleStorageManagerSession_ReadFromDisk & session, const std::vector<cbhe_eid_t> & availableDestinationEids) { //0 if empty, size if entry

    session.catalogEntryPtr = m_bundleStorageCatalog.PopEntryFromAwaitingSend(session.custodyId, availableDestinationEids);
    if (session.catalogEntryPtr == NULL) {
        return 0;
    }
    session.nextLogicalSegment = 0;
    session.nextLogicalSegmentToCache = 0;
    session.cacheReadIndex = 0;
    session.cacheWriteIndex = 0;

    return session.catalogEntryPtr->bundleSizeBytes;
}
uint64_t BundleStorageManagerBase::PopTop(BundleStorageManagerSession_ReadFromDisk & session, const std::vector<uint64_t> & availableDestNodeIds) { //0 if empty, size if entry

    session.catalogEntryPtr = m_bundleStorageCatalog.PopEntryFromAwaitingSend(session.custodyId, availableDestNodeIds);
    if (session.catalogEntryPtr == NULL) {
        return 0;
    }
    session.nextLogicalSegment = 0;
    session.nextLogicalSegmentToCache = 0;
    session.cacheReadIndex = 0;
    session.cacheWriteIndex = 0;

    return session.catalogEntryPtr->bundleSizeBytes;
}
uint64_t BundleStorageManagerBase::PopTop(BundleStorageManagerSession_ReadFromDisk & session, const std::vector<std::pair<cbhe_eid_t, bool> > & availableDests) { //0 if empty, size if entry

    session.catalogEntryPtr = m_bundleStorageCatalog.PopEntryFromAwaitingSend(session.custodyId, availableDests);
    if (session.catalogEntryPtr == NULL) {
        return 0;
    }
    session.nextLogicalSegment = 0;
    session.nextLogicalSegmentToCache = 0;
    session.cacheReadIndex = 0;
    session.cacheWriteIndex = 0;

    return session.catalogEntryPtr->bundleSizeBytes;
}


bool BundleStorageManagerBase::ReturnTop(BundleStorageManagerSession_ReadFromDisk & session) { //0 if empty, size if entry
    return ((session.catalogEntryPtr != NULL) && m_bundleStorageCatalog.ReturnEntryToAwaitingSend(*session.catalogEntryPtr, session.custodyId));
}

bool BundleStorageManagerBase::ReturnCustodyIdToAwaitingSend(const uint64_t custodyId) { //0 if empty, size if entry
    if (catalog_entry_t * catalogEntryPtr = m_bundleStorageCatalog.GetEntryFromCustodyId(custodyId)) {
        return m_bundleStorageCatalog.ReturnEntryToAwaitingSend(*catalogEntryPtr, custodyId);
    }
    return false;
}
catalog_entry_t * BundleStorageManagerBase::GetCatalogEntryPtrFromCustodyId(const uint64_t custodyId) { //NULL if unavailable
    return m_bundleStorageCatalog.GetEntryFromCustodyId(custodyId);
}

std::size_t BundleStorageManagerBase::TopSegment(BundleStorageManagerSession_ReadFromDisk & session, void * buf) {
    const segment_id_chain_vec_t & segments = session.catalogEntryPtr->segmentIdChainVec;

    while (((session.nextLogicalSegmentToCache - session.nextLogicalSegment) < READ_CACHE_NUM_SEGMENTS_PER_SESSION)
        && (session.nextLogicalSegmentToCache < segments.size()))
    {
        const segment_id_t segmentId = segments[session.nextLogicalSegmentToCache++];
        const unsigned int diskIndex = segmentId % M_NUM_STORAGE_DISKS;
        CircularIndexBufferSingleProducerSingleConsumerConfigurable & cb = m_circularIndexBuffersVec[diskIndex];
        unsigned int produceIndex = cb.GetIndexForWrite();
        while (produceIndex == CIRCULAR_INDEX_BUFFER_FULL) { //if full, wait until not full	
            //try again, but with the mutex
            boost::mutex::scoped_lock lockMainThread(m_mutexMainThread);
            produceIndex = cb.GetIndexForWrite();
            if (produceIndex == CIRCULAR_INDEX_BUFFER_FULL) { //if full again (lock mutex (above) before checking condition)
                m_conditionVariableMainThread.wait(lockMainThread); // call lock.unlock() and blocks the current thread
                //thread is now unblocked, and the lock is reacquired by invoking lock.lock()
                produceIndex = cb.GetIndexForWrite(); //should definitely have an index now (prevents an extra lock, unlock operation)
            }
        }

        session.readCacheIsSegmentReady[session.cacheWriteIndex] = false;
        m_circularBufferIsReadCompletedPointers[diskIndex * CIRCULAR_INDEX_BUFFER_SIZE + produceIndex] = &session.readCacheIsSegmentReady[session.cacheWriteIndex];
        m_circularBufferReadFromStoragePointers[diskIndex * CIRCULAR_INDEX_BUFFER_SIZE + produceIndex] = &session.readCache[session.cacheWriteIndex * SEGMENT_SIZE];
        session.cacheWriteIndex = (session.cacheWriteIndex + 1) % READ_CACHE_NUM_SEGMENTS_PER_SESSION;
        m_circularBufferSegmentIdsPtr[diskIndex * CIRCULAR_INDEX_BUFFER_SIZE + produceIndex] = segmentId;

        CommitWriteAndNotifyDiskOfWorkToDo_ThreadSafe(diskIndex);
    }

    volatile bool & readIsReadyRef = session.readCacheIsSegmentReady[session.cacheReadIndex];
    while (!readIsReadyRef) { //store the volatile, wait until read is ready		
        //try again, but with the mutex
        boost::mutex::scoped_lock lockMainThread(m_mutexMainThread);
        if (!readIsReadyRef) { //if not ready again (lock mutex (above) before checking condition)
            m_conditionVariableMainThread.wait(lockMainThread); // call lock.unlock() and blocks the current thread
            //thread is now unblocked, and the lock is reacquired by invoking lock.lock()
        }
    }

    StorageSegmentHeader storageSegmentHeader;
    memcpy(&storageSegmentHeader, (void*)&session.readCache[session.cacheReadIndex * SEGMENT_SIZE + 0], SEGMENT_RESERVED_SPACE);
    storageSegmentHeader.ToNativeEndianInplace(); //should optimize out and do nothing
    if ((session.nextLogicalSegment == 0) && (storageSegmentHeader.bundleSizeBytes != session.catalogEntryPtr->bundleSizeBytes)) {// ? chainInfo.first : UINT64_MAX;
        LOG_ERROR(subprocess) << "Error: read bundle size bytes = " << storageSegmentHeader.bundleSizeBytes <<
            " does not match catalog bundleSizeBytes = " << session.catalogEntryPtr->bundleSizeBytes;
    }
    else if (session.nextLogicalSegment != 0 && storageSegmentHeader.bundleSizeBytes != UINT64_MAX) {// ? chainInfo.first : UINT64_MAX;
        LOG_ERROR(subprocess) << "Error: read bundle size bytes = " << storageSegmentHeader.bundleSizeBytes << " is not UINT64_MAX";
    }

    ++session.nextLogicalSegment;
    if ((session.nextLogicalSegment != segments.size()) && (storageSegmentHeader.nextSegmentId != segments[session.nextLogicalSegment])) {
        LOG_ERROR(subprocess) << "Error: read nextSegmentId = " << (storageSegmentHeader.nextSegmentId) <<
            " does not match segment = " << segments[session.nextLogicalSegment];
    }
    else if ((session.nextLogicalSegment == segments.size()) && (storageSegmentHeader.nextSegmentId != SEGMENT_ID_LAST)) {
        LOG_ERROR(subprocess) << "Error: read nextSegmentId = " << storageSegmentHeader.nextSegmentId << " is not SEGMENT_ID_LAST";
    }

    std::size_t size = BUNDLE_STORAGE_PER_SEGMENT_SIZE;
    if (storageSegmentHeader.nextSegmentId == SEGMENT_ID_LAST) {
        uint64_t modBytes = (session.catalogEntryPtr->bundleSizeBytes % BUNDLE_STORAGE_PER_SEGMENT_SIZE);
        if (modBytes != 0) {
            size = modBytes;
        }
    }

    memcpy(buf, (void*)&session.readCache[session.cacheReadIndex * SEGMENT_SIZE + SEGMENT_RESERVED_SPACE], size);
    session.cacheReadIndex = (session.cacheReadIndex + 1) % READ_CACHE_NUM_SEGMENTS_PER_SESSION;


    return size;
}
bool BundleStorageManagerBase::ReadAllSegments(BundleStorageManagerSession_ReadFromDisk & session, std::vector<uint8_t> & buf) {
    const std::size_t numSegmentsToRead = session.catalogEntryPtr->segmentIdChainVec.size();
    const uint64_t totalBytesToRead = session.catalogEntryPtr->bundleSizeBytes;
    buf.resize(totalBytesToRead);
    std::size_t totalBytesRead = 0;
    for (std::size_t i = 0; i < numSegmentsToRead; ++i) {
        totalBytesRead += TopSegment(session, &buf[i*BUNDLE_STORAGE_PER_SEGMENT_SIZE]);
    }
    return (totalBytesRead == totalBytesToRead);
}
bool BundleStorageManagerBase::RemoveBundleFromDisk(const catalog_entry_t *catalogEntryPtr, const uint64_t custodyId) {
    // "read" the bundle so that we can call RemoveReadBundleFromDisk
    // don't care if this fails, that just means that it wasn't awaiting send
    bool err = m_bundleStorageCatalog.RemoveEntryFromAwaitingSend(*catalogEntryPtr, custodyId);
    return RemoveReadBundleFromDisk(catalogEntryPtr, custodyId);
}
bool BundleStorageManagerBase::RemoveReadBundleFromDisk(const uint64_t custodyId) {
    const catalog_entry_t * catalogEntryPtr = m_bundleStorageCatalog.GetEntryFromCustodyId(custodyId);
    if (catalogEntryPtr == NULL) {
        return false;
    }
    return RemoveReadBundleFromDisk(catalogEntryPtr, custodyId);
}
bool BundleStorageManagerBase::RemoveReadBundleFromDisk(BundleStorageManagerSession_ReadFromDisk & sessionRead) {
    return RemoveReadBundleFromDisk(sessionRead.catalogEntryPtr, sessionRead.custodyId);
}
bool BundleStorageManagerBase::RemoveReadBundleFromDisk(const catalog_entry_t * catalogEntryPtr, const uint64_t custodyId) {
    const segment_id_chain_vec_t & segmentIdChainVec = catalogEntryPtr->segmentIdChainVec;

    //destroy the head on the disk by writing UINT64_MAX to bundleSizeBytes of first logical segment


    static const uint64_t bundleSizeBytesLittleEndian = UINT64_MAX;
    const segment_id_t segmentId = segmentIdChainVec[0];
    const unsigned int diskIndex = segmentId % M_NUM_STORAGE_DISKS;
    CircularIndexBufferSingleProducerSingleConsumerConfigurable & cb = m_circularIndexBuffersVec[diskIndex];
    unsigned int produceIndex = cb.GetIndexForWrite();
    while (produceIndex == CIRCULAR_INDEX_BUFFER_FULL) { //if full, wait until not full	
        //try again, but with the mutex
        boost::mutex::scoped_lock lockMainThread(m_mutexMainThread);
        produceIndex = cb.GetIndexForWrite();
        if (produceIndex == CIRCULAR_INDEX_BUFFER_FULL) { //if full again (lock mutex (above) before checking condition)
            m_conditionVariableMainThread.wait(lockMainThread); // call lock.unlock() and blocks the current thread
            //thread is now unblocked, and the lock is reacquired by invoking lock.lock()
            produceIndex = cb.GetIndexForWrite(); //should definitely have an index now (prevents an extra lock, unlock operation)
        }
    }

    uint8_t * const circularBufferBlockDataPtr = &m_circularBufferBlockDataPtr[diskIndex * CIRCULAR_INDEX_BUFFER_SIZE * SEGMENT_SIZE];
    segment_id_t * const circularBufferSegmentIdsPtr = &m_circularBufferSegmentIdsPtr[diskIndex * CIRCULAR_INDEX_BUFFER_SIZE];


    uint8_t * const dataCb = &circularBufferBlockDataPtr[produceIndex * SEGMENT_SIZE];
    circularBufferSegmentIdsPtr[produceIndex] = segmentId;
    m_circularBufferReadFromStoragePointers[diskIndex * CIRCULAR_INDEX_BUFFER_SIZE + produceIndex] = NULL; //isWriteToDisk = true

    memcpy(dataCb, &bundleSizeBytesLittleEndian, sizeof(bundleSizeBytesLittleEndian));


    CommitWriteAndNotifyDiskOfWorkToDo_ThreadSafe(diskIndex);

    const bool successFreedSegments = m_memoryManager.FreeSegments_ThreadSafe(segmentIdChainVec);
    return (m_bundleStorageCatalog.Remove(custodyId, false).first && successFreedSegments);
}
uint64_t * BundleStorageManagerBase::GetCustodyIdFromUuid(const cbhe_bundle_uuid_t & bundleUuid) {
    return m_bundleStorageCatalog.GetCustodyIdFromUuid(bundleUuid);
}
uint64_t * BundleStorageManagerBase::GetCustodyIdFromUuid(const cbhe_bundle_uuid_nofragment_t & bundleUuid) {
    return m_bundleStorageCatalog.GetCustodyIdFromUuid(bundleUuid);
}
bool BundleStorageManagerBase::GetStorageExpiringBeforeThresholdTelemetry(StorageExpiringBeforeThresholdTelemetry_t& telem) {
    return m_bundleStorageCatalog.GetStorageExpiringBeforeThresholdTelemetry(telem);
}
void BundleStorageManagerBase::GetExpiredBundleIds(const uint64_t expiry, const uint64_t maxNumberToFind, std::vector<uint64_t> & returnedIds) {
    m_bundleStorageCatalog.GetExpiredBundleIds(expiry, maxNumberToFind, returnedIds);
}
//uint64_t BundleStorageManagerMT::TopSegmentCount(BundleStorageManagerSession_ReadFromDisk & session) {
//	return session.chainInfoVecPtr->front().second.size(); //use the front as new writes will be pushed back
//}

bool BundleStorageManagerBase::RestoreFromDisk(uint64_t * totalBundlesRestored, uint64_t * totalBytesRestored, uint64_t * totalSegmentsRestored) {
    *totalBundlesRestored = 0; *totalBytesRestored = 0; *totalSegmentsRestored = 0;
    uint8_t dataReadBuf[SEGMENT_SIZE];
    std::vector<FILE *> fileHandlesVec(M_NUM_STORAGE_DISKS);
    std::vector <uint64_t> fileSizesVec(M_NUM_STORAGE_DISKS);
    for (unsigned int diskId = 0; diskId < M_NUM_STORAGE_DISKS; ++diskId) {
        const char * const filePath = m_storageConfigPtr->m_storageDiskConfigVector[diskId].storeFilePath.c_str();
        const boost::filesystem::path p(filePath);
        if (boost::filesystem::exists(p)) {
            fileSizesVec[diskId] = boost::filesystem::file_size(p);
            LOG_DEBUG(subprocess) << "diskId " << diskId
                << " has file size of " << fileSizesVec[diskId];
        }
        else {
            LOG_ERROR(subprocess) << "Error: " << filePath << " does not exist";
            return false;
        }
        fileHandlesVec[diskId] = fopen(filePath, "rbR");
        if (fileHandlesVec[diskId] == NULL) {
            LOG_ERROR(subprocess) << "Error opening file " << filePath <<
                " for reading and restoring";
            return false;
        }
    }

    bool restoreInProgress = true;
    BundleViewV6 bv6;
    BundleViewV7 bv7;
    for (segment_id_t potentialHeadSegmentId = 0; restoreInProgress; ++potentialHeadSegmentId) {
        if (!m_memoryManager.IsSegmentFree(potentialHeadSegmentId)) continue;
        segment_id_t segmentId = potentialHeadSegmentId;
        BundleStorageManagerSession_WriteToDisk session;
        catalog_entry_t & catalogEntry = session.catalogEntry;
        segment_id_chain_vec_t & segmentIdChainVec = catalogEntry.segmentIdChainVec;
        bool headSegmentFound = false;
        uint64_t custodyIdHeadSegment;
        PrimaryBlock * primaryBasePtr = NULL;
        for (session.nextLogicalSegment = 0; ; ++session.nextLogicalSegment) {
            const unsigned int diskIndex = segmentId % M_NUM_STORAGE_DISKS;
            FILE * const fileHandle = fileHandlesVec[diskIndex];
            const uint64_t offsetBytes = static_cast<uint64_t>(segmentId / M_NUM_STORAGE_DISKS) * SEGMENT_SIZE;
            const uint64_t fileSize = fileSizesVec[diskIndex];
            if ((session.nextLogicalSegment == 0) && ((offsetBytes + SEGMENT_SIZE) > fileSize)) {
                LOG_INFO(subprocess) << "end of restore";
                restoreInProgress = false;
                break;
            }
#ifdef _MSC_VER 
            _fseeki64_nolock(fileHandle, offsetBytes, SEEK_SET);
#elif defined __APPLE__ 
            fseeko(fileHandle, offsetBytes, SEEK_SET);
#else
            fseeko64(fileHandle, offsetBytes, SEEK_SET);
#endif

            const std::size_t bytesReadFromFread = fread((void*)dataReadBuf, 1, SEGMENT_SIZE, fileHandle);
            if (bytesReadFromFread != SEGMENT_SIZE) {
                LOG_ERROR(subprocess) << "Error reading at offset " << offsetBytes <<
                    " for disk " << diskIndex << " filesize " << fileSize << " logical segment "
                    << session.nextLogicalSegment << " bytesread " << bytesReadFromFread;
                return false;
            }

            StorageSegmentHeader storageSegmentHeader;
            memcpy(&storageSegmentHeader, dataReadBuf, SEGMENT_RESERVED_SPACE);
            storageSegmentHeader.ToNativeEndianInplace(); //should optimize out and do nothing

            
            if ((session.nextLogicalSegment == 0) && (storageSegmentHeader.bundleSizeBytes != UINT64_MAX)) { //head segment
                headSegmentFound = true;
                custodyIdHeadSegment = storageSegmentHeader.custodyId;

                uint8_t * bundleDataBegin = dataReadBuf + SEGMENT_RESERVED_SPACE;
                
                const uint8_t firstByte = bundleDataBegin[0];
                const bool isBpVersion6 = (firstByte == 6);
                const bool isBpVersion7 = (firstByte == ((4U << 5) | 31U));  //CBOR major type 4, additional information 31 (Indefinite-Length Array)
                if (isBpVersion6) {
                    if (!bv6.LoadBundle(bundleDataBegin, BUNDLE_STORAGE_PER_SEGMENT_SIZE, true)) { //load primary only
                        LOG_ERROR(subprocess) << "malformed bundle";
                        return false;
                    }
                    Bpv6CbhePrimaryBlock & primary = bv6.m_primaryBlockView.header;
                    primaryBasePtr = &primary;
                }
                else if (isBpVersion7) {
                    if (!bv7.LoadBundle(bundleDataBegin, BUNDLE_STORAGE_PER_SEGMENT_SIZE, true, true)) { //load primary only
                        LOG_ERROR(subprocess) << "malformed bundle";
                        return false;
                    }
                    Bpv7CbhePrimaryBlock & primary = bv7.m_primaryBlockView.header;
                    primaryBasePtr = &primary;
                }
                else {
                    LOG_ERROR(subprocess) << "error in BundleStorageManagerBase::RestoreFromDisk: unknown bundle version detected";
                    return false;
                }
                const uint64_t totalSegmentsRequired = (storageSegmentHeader.bundleSizeBytes / BUNDLE_STORAGE_PER_SEGMENT_SIZE) + ((storageSegmentHeader.bundleSizeBytes % BUNDLE_STORAGE_PER_SEGMENT_SIZE) == 0 ? 0 : 1);

                *totalBytesRestored += storageSegmentHeader.bundleSizeBytes;
                *totalSegmentsRestored += totalSegmentsRequired;
                catalogEntry.Init(*primaryBasePtr, storageSegmentHeader.bundleSizeBytes, totalSegmentsRequired, NULL); //NULL replaced later at CatalogIncomingBundleForStore
            }
            if (!headSegmentFound) break;
            if (custodyIdHeadSegment != storageSegmentHeader.custodyId) { //shall be the same across all segments
                LOG_ERROR(subprocess) << "error: custodyIdHeadSegment != custodyId";
                return false;
            }
            if ((session.nextLogicalSegment) >= segmentIdChainVec.size()) {
                LOG_ERROR(subprocess) << "error: logical segment exceeds total segments required";
                return false;
            }
            if (!m_memoryManager.IsSegmentFree(segmentId)) { //todo this is redundant per function below
                LOG_ERROR(subprocess) << "error: segmentId is already allocated";
                return false;
            }
            if (!m_memoryManager.AllocateSegmentId_NotThreadSafe(segmentId)) {
                LOG_ERROR(subprocess) << "error: AllocateSegmentId_NotThreadSafe: segmentId is already allocated";
                return false;
            }
            segmentIdChainVec[session.nextLogicalSegment] = segmentId;



            if ((session.nextLogicalSegment + 1) >= segmentIdChainVec.size()) { //==
                if (storageSegmentHeader.nextSegmentId != SEGMENT_ID_LAST) { //there are more segments
                    LOG_ERROR(subprocess) << "error: at the last logical segment but nextSegmentId != SEGMENT_ID_LAST";
                    return false;
                }
                m_bundleStorageCatalog.CatalogIncomingBundleForStore(catalogEntry, *primaryBasePtr, storageSegmentHeader.custodyId, BundleStorageCatalog::DUPLICATE_EXPIRY_ORDER::FIFO);
                *totalBundlesRestored += 1;
                break;
            }

            if (storageSegmentHeader.nextSegmentId == SEGMENT_ID_LAST) { //there are more segments
                LOG_ERROR(subprocess) << "error: there are more logical segments but nextSegmentId == SEGMENT_ID_LAST";
                return false;
            }
            segmentId = storageSegmentHeader.nextSegmentId;

        }
    }


    for (unsigned int tId = 0; tId < M_NUM_STORAGE_DISKS; ++tId) {
        fclose(fileHandlesVec[tId]);
    }

    m_successfullyRestoredFromDisk = true;
    return true;
}


