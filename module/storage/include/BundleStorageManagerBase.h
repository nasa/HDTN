#ifndef _BUNDLE_STORAGE_MANAGER_BASE_H
#define _BUNDLE_STORAGE_MANAGER_BASE_H

#include <boost/integer.hpp>
#include <stdint.h>
#include <map>
#include <forward_list>
#include <array>
#include <vector>
#include <utility>
#include <string>
#include <cstdio>
#include <boost/shared_ptr.hpp>
#include <boost/thread.hpp>
#include <boost/bimap.hpp>
#include "CircularIndexBufferSingleProducerSingleConsumerConfigurable.h"
#include "BundleStorageConfig.h"
#include "Logger.h"
#include "MemoryManagerTreeArray.h"
#include "StorageConfig.h"
#include "codec/bpv6.h"
#include "BundleStorageCatalog.h"




struct BundleStorageManagerSession_WriteToDisk {
    catalog_entry_t catalogEntry;
    uint32_t nextLogicalSegment;
};

struct BundleStorageManagerSession_ReadFromDisk {
    catalog_entry_t * catalogEntryPtr;
    uint64_t custodyId;

    uint32_t nextLogicalSegment;
    uint32_t nextLogicalSegmentToCache;
    uint32_t cacheReadIndex;
    uint32_t cacheWriteIndex;

    std::unique_ptr<volatile uint8_t[]> readCache;// [READ_CACHE_NUM_SEGMENTS_PER_SESSION * SEGMENT_SIZE]; //may overflow stack, create on heap
    volatile bool readCacheIsSegmentReady[READ_CACHE_NUM_SEGMENTS_PER_SESSION];

    BundleStorageManagerSession_ReadFromDisk();
    ~BundleStorageManagerSession_ReadFromDisk();
};

class BundleStorageManagerBase {
protected:
    BundleStorageManagerBase();
    BundleStorageManagerBase(const std::string & jsonConfigFileName);
    BundleStorageManagerBase(const StorageConfig_ptr & storageConfigPtr);
public:

    virtual ~BundleStorageManagerBase();
    virtual void Start() = 0;

    //write
    uint64_t Push(BundleStorageManagerSession_WriteToDisk & session,
        const bpv6_primary_block & bundlePrimaryBlock, const uint64_t bundleSizeBytes); //return totalSegmentsRequired
    int PushSegment(BundleStorageManagerSession_WriteToDisk & session,
        const bpv6_primary_block & bundlePrimaryBlock, const uint64_t custodyId,
        const uint8_t * buf, std::size_t size);
    uint64_t PushAllSegments(BundleStorageManagerSession_WriteToDisk & session,
        const bpv6_primary_block & bundlePrimaryBlock,
        const uint64_t custodyId, const uint8_t * allData, const std::size_t allDataSize); //return total bytes pushed

    //Read
    uint64_t PopTop(BundleStorageManagerSession_ReadFromDisk & session, const std::vector<cbhe_eid_t> & availableDestinationEids); //0 if empty, size if entry
    bool ReturnTop(BundleStorageManagerSession_ReadFromDisk & session);
    bool ReturnCustodyIdToAwaitingSend(const uint64_t custodyId); //for expired custody timers
    catalog_entry_t * GetCatalogEntryPtrFromCustodyId(const uint64_t custodyId); //for deletion of custody timer
    std::size_t TopSegment(BundleStorageManagerSession_ReadFromDisk & session, void * buf);
    bool ReadAllSegments(BundleStorageManagerSession_ReadFromDisk & session, std::vector<uint8_t> & buf);
    bool RemoveReadBundleFromDisk(const uint64_t custodyId);
    bool RemoveReadBundleFromDisk(BundleStorageManagerSession_ReadFromDisk & sessionRead);
    bool RemoveReadBundleFromDisk(const catalog_entry_t * catalogEntryPtr, const uint64_t custodyId);
    uint64_t * GetCustodyIdFromUuid(const cbhe_bundle_uuid_t & bundleUuid);
    uint64_t * GetCustodyIdFromUuid(const cbhe_bundle_uuid_nofragment_t & bundleUuid);



    bool RestoreFromDisk(uint64_t * totalBundlesRestored, uint64_t * totalBytesRestored, uint64_t * totalSegmentsRestored);

    const MemoryManagerTreeArray & GetMemoryManagerConstRef();


protected:

    
    virtual void NotifyDiskOfWorkToDo_ThreadSafe(const unsigned int diskId) = 0;

protected:
    StorageConfig_ptr m_storageConfigPtr;
public:
    const unsigned int M_NUM_STORAGE_DISKS;
    const uint64_t M_TOTAL_STORAGE_CAPACITY_BYTES; //old FILE_SIZE
    const uint64_t M_MAX_SEGMENTS;
protected:
    MemoryManagerTreeArray m_memoryManager;
    BundleStorageCatalog m_bundleStorageCatalog;
    boost::mutex m_mutexMainThread;
    boost::mutex::scoped_lock m_lockMainThread;
    boost::condition_variable m_conditionVariableMainThread;
    std::vector<boost::filesystem::path> m_filePathsVec;
    std::vector<std::string> m_filePathsAsStringVec;
    std::vector<CircularIndexBufferSingleProducerSingleConsumerConfigurable> m_circularIndexBuffersVec;

    uint8_t * m_circularBufferBlockDataPtr;
    segment_id_t * m_circularBufferSegmentIdsPtr;
    //volatile bool * volatile m_circularBufferIsReadCompletedPointers[CIRCULAR_INDEX_BUFFER_SIZE * NUM_STORAGE_THREADS];
    //volatile boost::uint8_t * volatile m_circularBufferReadFromStoragePointers[CIRCULAR_INDEX_BUFFER_SIZE * NUM_STORAGE_THREADS];
    volatile bool * volatile m_circularBufferIsReadCompletedPointers[CIRCULAR_INDEX_BUFFER_SIZE * MAX_NUM_STORAGE_THREADS];
    volatile uint8_t * volatile m_circularBufferReadFromStoragePointers[CIRCULAR_INDEX_BUFFER_SIZE * MAX_NUM_STORAGE_THREADS];
    volatile bool m_autoDeleteFilesOnExit;
    
public:
    bool m_successfullyRestoredFromDisk;
    uint64_t m_totalBundlesRestored;
    uint64_t m_totalBytesRestored;
    uint64_t m_totalSegmentsRestored;
};


#endif //_BUNDLE_STORAGE_MANAGER_BASE_H
