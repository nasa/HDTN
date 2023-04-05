/**
 * @file BundleStorageManagerBase.h
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
 *
 * @section DESCRIPTION
 *
 * This BundleStorageManagerBase class implements the basic methods for
 * writing and reading bundles to and from solid state disk drive(s).
 */

#ifndef _BUNDLE_STORAGE_MANAGER_BASE_H
#define _BUNDLE_STORAGE_MANAGER_BASE_H 1
#include "storage_lib_export.h"
#ifndef CLASS_VISIBILITY_STORAGE_LIB
#  ifdef _WIN32
#    define CLASS_VISIBILITY_STORAGE_LIB
#  else
#    define CLASS_VISIBILITY_STORAGE_LIB STORAGE_LIB_EXPORT
#  endif
#endif
#include <boost/integer.hpp>
#include <stdint.h>
#include <map>
#include <array>
#include <vector>
#include <utility>
#include <string>
#include <cstdio>
#include <memory>
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

    STORAGE_LIB_EXPORT BundleStorageManagerSession_ReadFromDisk();
    STORAGE_LIB_EXPORT ~BundleStorageManagerSession_ReadFromDisk();
};

class CLASS_VISIBILITY_STORAGE_LIB BundleStorageManagerBase {
protected:
    STORAGE_LIB_EXPORT BundleStorageManagerBase();
    STORAGE_LIB_EXPORT BundleStorageManagerBase(const boost::filesystem::path& jsonConfigFilePath);
    STORAGE_LIB_EXPORT BundleStorageManagerBase(const StorageConfig_ptr & storageConfigPtr);
public:

    STORAGE_LIB_EXPORT virtual ~BundleStorageManagerBase();
    virtual void Start() = 0;

    //write
    STORAGE_LIB_EXPORT uint64_t Push(BundleStorageManagerSession_WriteToDisk & session,
        const PrimaryBlock & bundlePrimaryBlock, const uint64_t bundleSizeBytes); //return totalSegmentsRequired
    STORAGE_LIB_EXPORT int PushSegment(BundleStorageManagerSession_WriteToDisk & session,
        const PrimaryBlock & bundlePrimaryBlock, const uint64_t custodyId,
        const uint8_t * buf, std::size_t size);
    STORAGE_LIB_EXPORT uint64_t PushAllSegments(BundleStorageManagerSession_WriteToDisk & session,
        const PrimaryBlock & bundlePrimaryBlock,
        const uint64_t custodyId, const uint8_t * allData, const std::size_t allDataSize); //return total bytes pushed

    //Read
    STORAGE_LIB_EXPORT uint64_t PopTop(BundleStorageManagerSession_ReadFromDisk & session, const std::vector<cbhe_eid_t> & availableDestinationEids); //0 if empty, size if entry
    STORAGE_LIB_EXPORT uint64_t PopTop(BundleStorageManagerSession_ReadFromDisk & session, const std::vector<uint64_t> & availableDestNodeIds); //0 if empty, size if entry
    STORAGE_LIB_EXPORT uint64_t PopTop(BundleStorageManagerSession_ReadFromDisk & session, const std::vector<std::pair<cbhe_eid_t, bool> > & availableDests); //0 if empty, size if entry
    STORAGE_LIB_EXPORT bool ReturnTop(BundleStorageManagerSession_ReadFromDisk & session);
    STORAGE_LIB_EXPORT bool ReturnCustodyIdToAwaitingSend(const uint64_t custodyId); //for expired custody timers
    STORAGE_LIB_EXPORT catalog_entry_t * GetCatalogEntryPtrFromCustodyId(const uint64_t custodyId); //for deletion of custody timer
    STORAGE_LIB_EXPORT std::size_t TopSegment(BundleStorageManagerSession_ReadFromDisk & session, void * buf);
    STORAGE_LIB_EXPORT bool ReadAllSegments(BundleStorageManagerSession_ReadFromDisk & session, std::vector<uint8_t> & buf);
    STORAGE_LIB_EXPORT bool RemoveBundleFromDisk(const catalog_entry_t * catalogEntryPtr,const uint64_t custodyId);
    STORAGE_LIB_EXPORT bool RemoveReadBundleFromDisk(const uint64_t custodyId);
    STORAGE_LIB_EXPORT bool RemoveReadBundleFromDisk(BundleStorageManagerSession_ReadFromDisk & sessionRead);
    STORAGE_LIB_EXPORT bool RemoveReadBundleFromDisk(const catalog_entry_t * catalogEntryPtr, const uint64_t custodyId);
    STORAGE_LIB_EXPORT uint64_t * GetCustodyIdFromUuid(const cbhe_bundle_uuid_t & bundleUuid);
    STORAGE_LIB_EXPORT uint64_t * GetCustodyIdFromUuid(const cbhe_bundle_uuid_nofragment_t & bundleUuid);

    STORAGE_LIB_EXPORT bool GetStorageExpiringBeforeThresholdTelemetry(StorageExpiringBeforeThresholdTelemetry_t& telem);
    STORAGE_LIB_EXPORT void GetExpiredBundleIds(const uint64_t expiry, const uint64_t maxNumberToFind, std::vector<uint64_t> & returnedIds);

    STORAGE_LIB_EXPORT bool RestoreFromDisk(uint64_t * totalBundlesRestored, uint64_t * totalBytesRestored, uint64_t * totalSegmentsRestored);

    STORAGE_LIB_EXPORT const MemoryManagerTreeArray& GetMemoryManagerConstRef() const;
    STORAGE_LIB_EXPORT const BundleStorageCatalog& GetBundleStorageCatalogConstRef() const;

    STORAGE_LIB_EXPORT uint64_t GetFreeSpaceBytes() const noexcept;
    STORAGE_LIB_EXPORT uint64_t GetUsedSpaceBytes() const noexcept;
    STORAGE_LIB_EXPORT uint64_t GetTotalCapacityBytes() const noexcept;


protected:

    
    virtual void CommitWriteAndNotifyDiskOfWorkToDo_ThreadSafe(const unsigned int diskId) = 0;

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
    boost::condition_variable m_conditionVariableMainThread;
    std::vector<boost::filesystem::path> m_filePathsVec;
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
