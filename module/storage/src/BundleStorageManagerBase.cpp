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

BundleStorageManagerSession_ReadFromDisk::BundleStorageManagerSession_ReadFromDisk() :
    readCache(new volatile uint8_t[READ_CACHE_NUM_SEGMENTS_PER_SESSION * SEGMENT_SIZE]) {}

BundleStorageManagerSession_ReadFromDisk::~BundleStorageManagerSession_ReadFromDisk() {}

BundleStorageManagerBase::BundleStorageManagerBase() : BundleStorageManagerBase("storageConfig.json") {}

BundleStorageManagerBase::BundleStorageManagerBase(const std::string & jsonConfigFileName) : BundleStorageManagerBase(StorageConfig::CreateFromJsonFile(jsonConfigFileName)) {
    if (!m_storageConfigPtr) {
        std::cerr << "cannot open storage json config file: " << jsonConfigFileName << std::endl;
        hdtn::Logger::getInstance()->logError("storage", "cannot open storage json config file: " + jsonConfigFileName);
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
        std::cerr << "MAX SEGMENTS GREATER THAN WHAT MEMORY MANAGER CAN HANDLE\n";
        hdtn::Logger::getInstance()->logError("storage", "MAX SEGMENTS GREATER THAN WHAT MEMORY MANAGER CAN HANDLE");
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
            std::cout << "deleted " << p.string() << std::endl;
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
    const uint64_t bundleSizeBytesLittleEndian = (session.nextLogicalSegment == 0) ? boost::endian::native_to_little(catalogEntry.bundleSizeBytes) : UINT64_MAX;
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

    const segment_id_t nextSegmentIdLittleEndian = (session.nextLogicalSegment == segmentIdChainVec.size()) ? UINT32_MAX : boost::endian::native_to_little(segmentIdChainVec[session.nextLogicalSegment]);
    const uint64_t custodyIdLittleEndian = boost::endian::native_to_little(custodyId);
    memcpy(dataCb, &bundleSizeBytesLittleEndian, sizeof(bundleSizeBytesLittleEndian));
    memcpy(dataCb + sizeof(bundleSizeBytesLittleEndian), &nextSegmentIdLittleEndian, sizeof(nextSegmentIdLittleEndian));
    memcpy(dataCb + (sizeof(bundleSizeBytesLittleEndian) + sizeof(nextSegmentIdLittleEndian)), &custodyIdLittleEndian, sizeof(custodyIdLittleEndian));
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

bool BundleStorageManagerBase::ReturnTop(BundleStorageManagerSession_ReadFromDisk & session) { //0 if empty, size if entry
    return ((session.catalogEntryPtr != NULL) && m_bundleStorageCatalog.ReturnEntryToAwaitingSend(*session.catalogEntryPtr, session.custodyId));
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

    uint64_t bundleSizeBytes;
    memcpy(&bundleSizeBytes, (void*)&session.readCache[session.cacheReadIndex * SEGMENT_SIZE + 0], sizeof(bundleSizeBytes));
    if ((session.nextLogicalSegment == 0) && (bundleSizeBytes != session.catalogEntryPtr->bundleSizeBytes)) {// ? chainInfo.first : UINT64_MAX;
        const std::string msg = "Error: read bundle size bytes = " + std::to_string(bundleSizeBytes) +
            " does not match catalog bundleSizeBytes = " + std::to_string(session.catalogEntryPtr->bundleSizeBytes);
        std::cout << msg << "\n";
        hdtn::Logger::getInstance()->logError("storage", msg);
    }
    else if (session.nextLogicalSegment != 0 && bundleSizeBytes != UINT64_MAX) {// ? chainInfo.first : UINT64_MAX;
        const std::string msg = "Error: read bundle size bytes = " + std::to_string(bundleSizeBytes) + " is not UINT64_MAX";
        std::cout << msg << "\n";
        hdtn::Logger::getInstance()->logError("storage", msg);
    }

    segment_id_t nextSegmentId;
    ++session.nextLogicalSegment;
    memcpy(&nextSegmentId, (void*)&session.readCache[session.cacheReadIndex * SEGMENT_SIZE + sizeof(bundleSizeBytes)], sizeof(nextSegmentId));
    if ((session.nextLogicalSegment != segments.size()) && (nextSegmentId != segments[session.nextLogicalSegment])) {
        const std::string msg = "Error: read nextSegmentId = " + std::to_string(nextSegmentId) +
            " does not match segment = " + std::to_string(segments[session.nextLogicalSegment]);
        std::cout << msg << "\n";
        hdtn::Logger::getInstance()->logError("storage", msg);
    }
    else if ((session.nextLogicalSegment == segments.size()) && (nextSegmentId != UINT32_MAX)) {
        const std::string msg = "Error: read nextSegmentId = " + std::to_string(nextSegmentId) + " is not UINT32_MAX";
        std::cout << msg << "\n";
        hdtn::Logger::getInstance()->logError("storage", msg);
    }

    std::size_t size = BUNDLE_STORAGE_PER_SEGMENT_SIZE;
    if (nextSegmentId == UINT32_MAX) {
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
            std::cout << "diskId " << diskId << " has file size of " << fileSizesVec[diskId] << "\n";
            hdtn::Logger::getInstance()->logInfo("storage", "Thread " + std::to_string(diskId) + " has file size of " +
                std::to_string(fileSizesVec[diskId]));
        }
        else {
            std::cout << "error: " << filePath << " does not exist\n";
            hdtn::Logger::getInstance()->logError("storage", "Error: " + std::string(filePath) + " does not exist");
            return false;
        }
        fileHandlesVec[diskId] = fopen(filePath, "rbR");
        if (fileHandlesVec[diskId] == NULL) {
            std::cout << "error opening file " << filePath << " for reading and restoring\n";
            hdtn::Logger::getInstance()->logError("storage", "Error opening file " + std::string(filePath) +
                " for reading and restoring");
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
                std::cout << "end of restore\n";
                hdtn::Logger::getInstance()->logNotification("storage", "End of restore");
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
                std::cout << "error reading at offset " << offsetBytes << " for disk " << diskIndex << " filesize " << fileSize << " logical segment " << session.nextLogicalSegment << " bytesread " << bytesReadFromFread << "\n";
                hdtn::Logger::getInstance()->logError("storage", "Error reading at offset " + std::to_string(offsetBytes) +
                    " for disk " + std::to_string(diskIndex) + " filesize " + std::to_string(fileSize) + " logical segment "
                    + std::to_string(session.nextLogicalSegment) + " bytesread " + std::to_string(bytesReadFromFread));
                return false;
            }
            bpv6_primary_block primary;
            uint64_t bundleSizeBytes; // = (session.nextLogicalSegment == 0) ? chainInfo.first : UINT64_MAX;
            segment_id_t nextSegmentId; // = (session.nextLogicalSegment == segmentIdChainVec.size()) ? UINT32_MAX : segmentIdChainVec[session.nextLogicalSegment];
            uint64_t custodyId;

            memcpy(&bundleSizeBytes, dataReadBuf, sizeof(bundleSizeBytes));
            memcpy(&nextSegmentId, dataReadBuf + sizeof(bundleSizeBytes), sizeof(nextSegmentId));
            memcpy(&custodyId, dataReadBuf + (sizeof(bundleSizeBytes) + sizeof(nextSegmentId)), sizeof(custodyId));

            boost::endian::little_to_native_inplace(bundleSizeBytes);
            boost::endian::little_to_native_inplace(nextSegmentId);
            boost::endian::little_to_native_inplace(custodyId);


            if ((session.nextLogicalSegment == 0) && (bundleSizeBytes != UINT64_MAX)) { //head segment
                headSegmentFound = true;
                custodyIdHeadSegment = custodyId;

                //copy bundle header and store to maps, push segmentId to chain vec
                std::size_t offset = cbhe_bpv6_primary_block_decode(&primary, (const char*)(dataReadBuf + SEGMENT_RESERVED_SPACE), 0, BUNDLE_STORAGE_PER_SEGMENT_SIZE);
                if (offset == 0) {
                    return false;//Malformed bundle
                }

                const uint64_t totalSegmentsRequired = (bundleSizeBytes / BUNDLE_STORAGE_PER_SEGMENT_SIZE) + ((bundleSizeBytes % BUNDLE_STORAGE_PER_SEGMENT_SIZE) == 0 ? 0 : 1);

                //std::cout << "tot segs req " << totalSegmentsRequired << "\n";
                *totalBytesRestored += bundleSizeBytes;
                *totalSegmentsRestored += totalSegmentsRequired;
                catalogEntry.Init(primary, bundleSizeBytes, totalSegmentsRequired, NULL); //NULL replaced later at CatalogIncomingBundleForStore
            }
            if (!headSegmentFound) break;
            if (custodyIdHeadSegment != custodyId) { //shall be the same across all segments
                std::cout << "error: custodyIdHeadSegment != custodyId\n";
                hdtn::Logger::getInstance()->logError("storage", "Error: custodyIdHeadSegment != custodyId");
                return false;
            }
            if ((session.nextLogicalSegment) >= segmentIdChainVec.size()) {
                std::cout << "error: logical segment exceeds total segments required\n";
                hdtn::Logger::getInstance()->logError("storage", "Error: logical segment exceeds total segments required");
                return false;
            }
            if (!m_memoryManager.IsSegmentFree(segmentId)) {
                std::cout << "error: segmentId is already allocated\n";
                hdtn::Logger::getInstance()->logError("storage", "Error: segmentId is already allocated");
                return false;
            }
            m_memoryManager.AllocateSegmentId_NoCheck_NotThreadSafe(segmentId);
            segmentIdChainVec[session.nextLogicalSegment] = segmentId;



            if ((session.nextLogicalSegment + 1) >= segmentIdChainVec.size()) { //==
                if (nextSegmentId != UINT32_MAX) { //there are more segments
                    std::cout << "error: at the last logical segment but nextSegmentId != UINT32_MAX\n";
                    hdtn::Logger::getInstance()->logError("storage", "Error: at the last logical segment but nextSegmentId != UINT32_MAX");
                    return false;
                }
                m_bundleStorageCatalog.CatalogIncomingBundleForStore(catalogEntry, primary, custodyId, BundleStorageCatalog::DUPLICATE_EXPIRY_ORDER::FIFO);
                *totalBundlesRestored += 1;
                break;
                //std::cout << "write complete\n";
            }

            if (nextSegmentId == UINT32_MAX) { //there are more segments
                std::cout << "error: there are more logical segments but nextSegmentId == UINT32_MAX\n";
                hdtn::Logger::getInstance()->logError("storage", "Error: there are more logical segments but nextSegmentId == UINT32_MAX");
                return false;
            }
            segmentId = nextSegmentId;

        }
    }


    for (unsigned int tId = 0; tId < M_NUM_STORAGE_DISKS; ++tId) {
        fclose(fileHandlesVec[tId]);
    }

    m_successfullyRestoredFromDisk = true;
    return true;
}


