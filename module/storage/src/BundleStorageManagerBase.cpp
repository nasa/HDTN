/***************************************************************************
 * NASA Glenn Research Center, Cleveland, OH
 * Released under the NASA Open Source Agreement (NOSA)
 * May  2021
 *
 ****************************************************************************
 */

#include "BundleStorageManagerBase.h"
#include <iostream>
#include <string>
#include <boost/filesystem.hpp>
#include <boost/make_shared.hpp>
#include <boost/make_unique.hpp>
#include <boost/endian/conversion.hpp>

 //#ifdef _MSC_VER //Windows tests
 //static const char * FILE_PATHS[NUM_STORAGE_THREADS] = { "map0.bin", "map1.bin", "map2.bin", "map3.bin" };
 //#else //Lambda Linux tests
 //static const char * FILE_PATHS[NUM_STORAGE_THREADS] = { "/mnt/sda1/test/map0.bin", "/mnt/sdb1/test/map1.bin", "/mnt/sdc1/test/map2.bin", "/mnt/sdd1/test/map3.bin" };
 //#endif

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

BundleStorageManagerBase::BundleStorageManagerBase(const std::string & jsonConfigFileName) : BundleStorageManagerBase(StorageConfig::CreateFromJsonFile(jsonConfigFileName)) {
    if (!m_storageConfigPtr) {
        const std::string msg = "cannot open storage json config file: " + jsonConfigFileName;
        std::cerr << msg << std::endl;
        hdtn::Logger::getInstance()->logError("storage", msg);
        return;
    }
}

BundleStorageManagerBase::BundleStorageManagerBase(const StorageConfig_ptr & storageConfigPtr) :
    m_storageConfigPtr(storageConfigPtr),
    M_NUM_STORAGE_DISKS((m_storageConfigPtr) ? static_cast<unsigned int>(m_storageConfigPtr->m_storageDiskConfigVector.size()) : 1),
    M_TOTAL_STORAGE_CAPACITY_BYTES((m_storageConfigPtr) ? m_storageConfigPtr->m_totalStorageCapacityBytes : 1),
    M_MAX_SEGMENTS(M_TOTAL_STORAGE_CAPACITY_BYTES / SEGMENT_SIZE),
    m_memoryManager(M_MAX_SEGMENTS),
    m_lockMainThread(m_mutexMainThread),
    m_filePathsVec(M_NUM_STORAGE_DISKS),
    m_filePathsAsStringVec(M_NUM_STORAGE_DISKS),
    m_circularIndexBuffersVec(M_NUM_STORAGE_DISKS, CircularIndexBufferSingleProducerSingleConsumerConfigurable(CIRCULAR_INDEX_BUFFER_SIZE)),
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
        m_filePathsAsStringVec[diskId] = m_filePathsVec[diskId].string();
    }


    if (M_MAX_SEGMENTS > MAX_MEMORY_MANAGER_SEGMENTS) {
        static const std::string msg = "MAX SEGMENTS GREATER THAN WHAT MEMORY MANAGER CAN HANDLE";
        std::cerr << msg << "\n";
        hdtn::Logger::getInstance()->logError("storage", msg);
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
            boost::filesystem::remove(p);
            const std::string msg = "deleted " + p.string();
            std::cout << msg << "\n";
            hdtn::Logger::getInstance()->logInfo("storage", msg);
        }
    }
}


const MemoryManagerTreeArray & BundleStorageManagerBase::GetMemoryManagerConstRef() {
    return m_memoryManager;
}


uint64_t BundleStorageManagerBase::Push(BundleStorageManagerSession_WriteToDisk & session, const bpv6_primary_block & bundlePrimaryBlock, const uint64_t bundleSizeBytes) {
    catalog_entry_t & catalogEntry = session.catalogEntry;
    segment_id_chain_vec_t & segmentIdChainVec = catalogEntry.segmentIdChainVec;
    const uint64_t totalSegmentsRequired = (bundleSizeBytes / BUNDLE_STORAGE_PER_SEGMENT_SIZE) + ((bundleSizeBytes % BUNDLE_STORAGE_PER_SEGMENT_SIZE) == 0 ? 0 : 1);

    catalogEntry.Init(bundlePrimaryBlock, bundleSizeBytes, totalSegmentsRequired, NULL); //NULL replaced later at CatalogIncomingBundleForStore
    session.nextLogicalSegment = 0;


    if (m_memoryManager.AllocateSegments_ThreadSafe(segmentIdChainVec)) {
        //std::cout << "firstseg " << segmentIdChainVec[0] << "\n";
        return totalSegmentsRequired;
    }

    return 0;
}

int BundleStorageManagerBase::PushSegment(BundleStorageManagerSession_WriteToDisk & session, const bpv6_primary_block & bundlePrimaryBlock,
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
    while (produceIndex == UINT32_MAX) { //store the volatile, wait until not full				
        m_conditionVariableMainThread.timed_wait(m_lockMainThread, boost::posix_time::milliseconds(10)); // call lock.unlock() and blocks the current thread
        //thread is now unblocked, and the lock is reacquired by invoking lock.lock()	
        produceIndex = cb.GetIndexForWrite();
    }

    uint8_t * const circularBufferBlockDataPtr = &m_circularBufferBlockDataPtr[diskIndex * CIRCULAR_INDEX_BUFFER_SIZE * SEGMENT_SIZE];
    segment_id_t * const circularBufferSegmentIdsPtr = &m_circularBufferSegmentIdsPtr[diskIndex * CIRCULAR_INDEX_BUFFER_SIZE];


    uint8_t * const dataCb = &circularBufferBlockDataPtr[produceIndex * SEGMENT_SIZE];
    circularBufferSegmentIdsPtr[produceIndex] = segmentId;
    m_circularBufferReadFromStoragePointers[diskIndex * CIRCULAR_INDEX_BUFFER_SIZE + produceIndex] = NULL; //isWriteToDisk = true

    storageSegmentHeader.nextSegmentId = (session.nextLogicalSegment == segmentIdChainVec.size()) ? UINT32_MAX : segmentIdChainVec[session.nextLogicalSegment];
    storageSegmentHeader.custodyId = custodyId;
    storageSegmentHeader.ToLittleEndianInplace(); //should optimize out and do nothing
    memcpy(dataCb, &storageSegmentHeader, SEGMENT_RESERVED_SPACE);
    memcpy(dataCb + SEGMENT_RESERVED_SPACE, buf, size);

    cb.CommitWrite();
    NotifyDiskOfWorkToDo_ThreadSafe(diskIndex);
    //std::cout << "writing " << size << " bytes\n";
    if (session.nextLogicalSegment == segmentIdChainVec.size()) {
        m_bundleStorageCatalog.CatalogIncomingBundleForStore(catalogEntry, bundlePrimaryBlock, custodyId, BundleStorageCatalog::DUPLICATE_EXPIRY_ORDER::FIFO);
        //std::cout << "write complete\n";
    }

    return 1;
}

//return total bytes pushed
uint64_t BundleStorageManagerBase::PushAllSegments(BundleStorageManagerSession_WriteToDisk & session,
    const bpv6_primary_block & bundlePrimaryBlock,
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
        while (produceIndex == UINT32_MAX) { //store the volatile, wait until not full				
            m_conditionVariableMainThread.timed_wait(m_lockMainThread, boost::posix_time::milliseconds(10)); // call lock.unlock() and blocks the current thread
            //thread is now unblocked, and the lock is reacquired by invoking lock.lock()	
            produceIndex = cb.GetIndexForWrite();
        }

        session.readCacheIsSegmentReady[session.cacheWriteIndex] = false;
        m_circularBufferIsReadCompletedPointers[diskIndex * CIRCULAR_INDEX_BUFFER_SIZE + produceIndex] = &session.readCacheIsSegmentReady[session.cacheWriteIndex];
        m_circularBufferReadFromStoragePointers[diskIndex * CIRCULAR_INDEX_BUFFER_SIZE + produceIndex] = &session.readCache[session.cacheWriteIndex * SEGMENT_SIZE];
        session.cacheWriteIndex = (session.cacheWriteIndex + 1) % READ_CACHE_NUM_SEGMENTS_PER_SESSION;
        m_circularBufferSegmentIdsPtr[diskIndex * CIRCULAR_INDEX_BUFFER_SIZE + produceIndex] = segmentId;

        cb.CommitWrite();
        NotifyDiskOfWorkToDo_ThreadSafe(diskIndex);
    }

    bool readIsReady = session.readCacheIsSegmentReady[session.cacheReadIndex];
    while (!readIsReady) { //store the volatile, wait until not full				
        m_conditionVariableMainThread.timed_wait(m_lockMainThread, boost::posix_time::milliseconds(10)); // call lock.unlock() and blocks the current thread
        //thread is now unblocked, and the lock is reacquired by invoking lock.lock()	
        readIsReady = session.readCacheIsSegmentReady[session.cacheReadIndex];
    }

    StorageSegmentHeader storageSegmentHeader;
    memcpy(&storageSegmentHeader, (void*)&session.readCache[session.cacheReadIndex * SEGMENT_SIZE + 0], SEGMENT_RESERVED_SPACE);
    storageSegmentHeader.ToNativeEndianInplace(); //should optimize out and do nothing
    if ((session.nextLogicalSegment == 0) && (storageSegmentHeader.bundleSizeBytes != session.catalogEntryPtr->bundleSizeBytes)) {// ? chainInfo.first : UINT64_MAX;
        const std::string msg = "Error: read bundle size bytes = " + boost::lexical_cast<std::string>(storageSegmentHeader.bundleSizeBytes) +
            " does not match catalog bundleSizeBytes = " + boost::lexical_cast<std::string>(session.catalogEntryPtr->bundleSizeBytes);
        std::cout << msg << "\n";
        hdtn::Logger::getInstance()->logError("storage", msg);
    }
    else if (session.nextLogicalSegment != 0 && storageSegmentHeader.bundleSizeBytes != UINT64_MAX) {// ? chainInfo.first : UINT64_MAX;
        const std::string msg = "Error: read bundle size bytes = " + boost::lexical_cast<std::string>(storageSegmentHeader.bundleSizeBytes) + " is not UINT64_MAX";
        std::cout << msg << "\n";
        hdtn::Logger::getInstance()->logError("storage", msg);
    }

    ++session.nextLogicalSegment;
    if ((session.nextLogicalSegment != segments.size()) && (storageSegmentHeader.nextSegmentId != segments[session.nextLogicalSegment])) {
        const std::string msg = "Error: read nextSegmentId = " + boost::lexical_cast<std::string>(storageSegmentHeader.nextSegmentId) +
            " does not match segment = " + boost::lexical_cast<std::string>(segments[session.nextLogicalSegment]);
        std::cout << msg << "\n";
        hdtn::Logger::getInstance()->logError("storage", msg);
    }
    else if ((session.nextLogicalSegment == segments.size()) && (storageSegmentHeader.nextSegmentId != UINT32_MAX)) {
        const std::string msg = "Error: read nextSegmentId = " + boost::lexical_cast<std::string>(storageSegmentHeader.nextSegmentId) + " is not UINT32_MAX";
        std::cout << msg << "\n";
        hdtn::Logger::getInstance()->logError("storage", msg);
    }

    std::size_t size = BUNDLE_STORAGE_PER_SEGMENT_SIZE;
    if (storageSegmentHeader.nextSegmentId == UINT32_MAX) {
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
    while (produceIndex == UINT32_MAX) { //store the volatile, wait until not full				
        m_conditionVariableMainThread.timed_wait(m_lockMainThread, boost::posix_time::milliseconds(10)); // call lock.unlock() and blocks the current thread
        //thread is now unblocked, and the lock is reacquired by invoking lock.lock()	
        produceIndex = cb.GetIndexForWrite();
    }

    uint8_t * const circularBufferBlockDataPtr = &m_circularBufferBlockDataPtr[diskIndex * CIRCULAR_INDEX_BUFFER_SIZE * SEGMENT_SIZE];
    segment_id_t * const circularBufferSegmentIdsPtr = &m_circularBufferSegmentIdsPtr[diskIndex * CIRCULAR_INDEX_BUFFER_SIZE];


    uint8_t * const dataCb = &circularBufferBlockDataPtr[produceIndex * SEGMENT_SIZE];
    circularBufferSegmentIdsPtr[produceIndex] = segmentId;
    m_circularBufferReadFromStoragePointers[diskIndex * CIRCULAR_INDEX_BUFFER_SIZE + produceIndex] = NULL; //isWriteToDisk = true

    memcpy(dataCb, &bundleSizeBytesLittleEndian, sizeof(bundleSizeBytesLittleEndian));


    cb.CommitWrite();
    NotifyDiskOfWorkToDo_ThreadSafe(diskIndex);

    const bool successFreedSegments = m_memoryManager.FreeSegments_ThreadSafe(segmentIdChainVec);
    return (m_bundleStorageCatalog.Remove(custodyId, false).first && successFreedSegments);
}
uint64_t * BundleStorageManagerBase::GetCustodyIdFromUuid(const cbhe_bundle_uuid_t & bundleUuid) {
    return m_bundleStorageCatalog.GetCustodyIdFromUuid(bundleUuid);
}
uint64_t * BundleStorageManagerBase::GetCustodyIdFromUuid(const cbhe_bundle_uuid_nofragment_t & bundleUuid) {
    return m_bundleStorageCatalog.GetCustodyIdFromUuid(bundleUuid);
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
            const std::string msg = "diskId " + boost::lexical_cast<std::string>(diskId)
                + " has file size of " + boost::lexical_cast<std::string>(fileSizesVec[diskId]);
            std::cout << msg << "\n";
            hdtn::Logger::getInstance()->logInfo("storage", msg);
        }
        else {
            const std::string msg = "Error: " + boost::lexical_cast<std::string>(filePath) + " does not exist";
            std::cout << msg << "\n";
            hdtn::Logger::getInstance()->logError("storage", msg);
            return false;
        }
        fileHandlesVec[diskId] = fopen(filePath, "rbR");
        if (fileHandlesVec[diskId] == NULL) {
            const std::string msg = "Error opening file " + std::string(filePath) +
                " for reading and restoring";
            std::cout << msg << "\n";
            hdtn::Logger::getInstance()->logError("storage", msg);
            return false;
        }
    }

    bool restoreInProgress = true;
    for (segment_id_t potentialHeadSegmentId = 0; restoreInProgress; ++potentialHeadSegmentId) {
        if (!m_memoryManager.IsSegmentFree(potentialHeadSegmentId)) continue;
        segment_id_t segmentId = potentialHeadSegmentId;
        BundleStorageManagerSession_WriteToDisk session;
        catalog_entry_t & catalogEntry = session.catalogEntry;
        segment_id_chain_vec_t & segmentIdChainVec = catalogEntry.segmentIdChainVec;
        bool headSegmentFound = false;
        uint64_t custodyIdHeadSegment;
        for (session.nextLogicalSegment = 0; ; ++session.nextLogicalSegment) {
            const unsigned int diskIndex = segmentId % M_NUM_STORAGE_DISKS;
            FILE * const fileHandle = fileHandlesVec[diskIndex];
            const uint64_t offsetBytes = static_cast<uint64_t>(segmentId / M_NUM_STORAGE_DISKS) * SEGMENT_SIZE;
            const uint64_t fileSize = fileSizesVec[diskIndex];
            if ((session.nextLogicalSegment == 0) && ((offsetBytes + SEGMENT_SIZE) > fileSize)) {
                static const std::string msg = "end of restore";
                std::cout << msg << "\n";
                hdtn::Logger::getInstance()->logNotification("storage", msg);
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
                const std::string msg = "Error reading at offset " + boost::lexical_cast<std::string>(offsetBytes) +
                    " for disk " + boost::lexical_cast<std::string>(diskIndex) + " filesize " + boost::lexical_cast<std::string>(fileSize) + " logical segment "
                    + boost::lexical_cast<std::string>(session.nextLogicalSegment) + " bytesread " + boost::lexical_cast<std::string>(bytesReadFromFread);
                std::cout << msg << "\n";
                hdtn::Logger::getInstance()->logError("storage", msg);
                return false;
            }
            bpv6_primary_block primary;

            StorageSegmentHeader storageSegmentHeader;
            memcpy(&storageSegmentHeader, dataReadBuf, SEGMENT_RESERVED_SPACE);
            storageSegmentHeader.ToNativeEndianInplace(); //should optimize out and do nothing

            if ((session.nextLogicalSegment == 0) && (storageSegmentHeader.bundleSizeBytes != UINT64_MAX)) { //head segment
                headSegmentFound = true;
                custodyIdHeadSegment = storageSegmentHeader.custodyId;

                //copy bundle header and store to maps, push segmentId to chain vec
                uint64_t decodedBlockSize;
                if (!primary.DeserializeBpv6(dataReadBuf + SEGMENT_RESERVED_SPACE, decodedBlockSize, BUNDLE_STORAGE_PER_SEGMENT_SIZE)) {
                    return false;
                }
                

                const uint64_t totalSegmentsRequired = (storageSegmentHeader.bundleSizeBytes / BUNDLE_STORAGE_PER_SEGMENT_SIZE) + ((storageSegmentHeader.bundleSizeBytes % BUNDLE_STORAGE_PER_SEGMENT_SIZE) == 0 ? 0 : 1);

                //std::cout << "tot segs req " << totalSegmentsRequired << "\n";
                *totalBytesRestored += storageSegmentHeader.bundleSizeBytes;
                *totalSegmentsRestored += totalSegmentsRequired;
                catalogEntry.Init(primary, storageSegmentHeader.bundleSizeBytes, totalSegmentsRequired, NULL); //NULL replaced later at CatalogIncomingBundleForStore
            }
            if (!headSegmentFound) break;
            if (custodyIdHeadSegment != storageSegmentHeader.custodyId) { //shall be the same across all segments
                static const std::string msg = "error: custodyIdHeadSegment != custodyId";
                std::cout << msg << "\n";
                hdtn::Logger::getInstance()->logError("storage", msg);
                return false;
            }
            if ((session.nextLogicalSegment) >= segmentIdChainVec.size()) {
                static const std::string msg = "error: logical segment exceeds total segments required";
                std::cout << msg << "\n";
                hdtn::Logger::getInstance()->logError("storage", msg);
                return false;
            }
            if (!m_memoryManager.IsSegmentFree(segmentId)) {
                static const std::string msg = "error: segmentId is already allocated";
                std::cout << msg << "\n";
                hdtn::Logger::getInstance()->logError("storage", msg);
                return false;
            }
            m_memoryManager.AllocateSegmentId_NoCheck_NotThreadSafe(segmentId);
            segmentIdChainVec[session.nextLogicalSegment] = segmentId;



            if ((session.nextLogicalSegment + 1) >= segmentIdChainVec.size()) { //==
                if (storageSegmentHeader.nextSegmentId != UINT32_MAX) { //there are more segments
                    static const std::string msg = "error: at the last logical segment but nextSegmentId != UINT32_MAX";
                    std::cout << msg << "\n";
                    hdtn::Logger::getInstance()->logError("storage", msg);
                    return false;
                }
                m_bundleStorageCatalog.CatalogIncomingBundleForStore(catalogEntry, primary, storageSegmentHeader.custodyId, BundleStorageCatalog::DUPLICATE_EXPIRY_ORDER::FIFO);
                *totalBundlesRestored += 1;
                break;
                //std::cout << "write complete\n";
            }

            if (storageSegmentHeader.nextSegmentId == UINT32_MAX) { //there are more segments
                static const std::string msg = "error: there are more logical segments but nextSegmentId == UINT32_MAX";
                std::cout << msg << "\n";
                hdtn::Logger::getInstance()->logError("storage", msg);
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


