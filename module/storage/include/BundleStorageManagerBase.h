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



typedef boost::uint64_t abs_expiration_t;
typedef std::pair<boost::uint64_t, segment_id_chain_vec_t> chain_info_t; //bundleSizeBytes, segment_id_chain_vec_t

typedef std::forward_list<chain_info_t> chain_info_flist_t;
typedef std::map<abs_expiration_t, chain_info_flist_t> expiration_map_t;
typedef std::array<expiration_map_t, NUMBER_OF_PRIORITIES> priority_array_t;
typedef std::map<uint64_t, priority_array_t> destination_map_t; //dst_node, priority_vec
/*
struct cbhe_bundle_uuid_storage_t {
    cbhe_bundle_uuid_t bundleUuid;

    uint64_t creationSeconds;
    uint64_t sequence;
    cbhe_eid_t destEid;
    //below if isFragment (default 0 if not a fragment)
    uint64_t fragmentOffset;
    uint64_t priority;      // 64 bytes



    cbhe_bundle_uuid_t(); //a default constructor: X()
    cbhe_bundle_uuid_t(uint64_t paramCreationSeconds, uint64_t paramSequence,
        uint64_t paramSrcNodeId, uint64_t paramSrcServiceId, uint64_t paramFragmentOffset, uint64_t paramDataLength);
    cbhe_bundle_uuid_t(const bpv6_primary_block & primary);
    ~cbhe_bundle_uuid_t(); //a destructor: ~X()
    cbhe_bundle_uuid_t(const cbhe_bundle_uuid_t& o); //a copy constructor: X(const X&)
    cbhe_bundle_uuid_t(cbhe_bundle_uuid_t&& o); //a move constructor: X(X&&)
    cbhe_bundle_uuid_t& operator=(const cbhe_bundle_uuid_t& o); //a copy assignment: operator=(const X&)
    cbhe_bundle_uuid_t& operator=(cbhe_bundle_uuid_t&& o); //a move assignment: operator=(X&&)
    bool operator==(const cbhe_bundle_uuid_t & o) const; //operator ==
    bool operator!=(const cbhe_bundle_uuid_t & o) const; //operator !=
    bool operator<(const cbhe_bundle_uuid_t & o) const; //operator < so it can be used as a map key
};*/
typedef boost::bimap<cbhe_bundle_uuid_t, uint64_t> uuid_to_custid_bimap_t; //get the cteb custody id

struct BundleStorageManagerSession_WriteToDisk {
    chain_info_t chainInfo;
    boost::uint32_t nextLogicalSegment;

    uint64_t destLinkId;
    unsigned int priorityIndex;
    abs_expiration_t absExpiration;
};

struct BundleStorageManagerSession_ReadFromDisk {
    chain_info_t chainInfo;
    boost::uint32_t nextLogicalSegment;
    boost::uint32_t nextLogicalSegmentToCache;
    boost::uint32_t cacheReadIndex;
    boost::uint32_t cacheWriteIndex;

    expiration_map_t * expirationMapPtr;
    chain_info_flist_t * chainInfoFlistPtr;
    expiration_map_t::iterator expirationMapIterator;

    uint64_t destLinkId;
    unsigned int priorityIndex;
    abs_expiration_t absExpiration;
    volatile uint8_t readCache[READ_CACHE_NUM_SEGMENTS_PER_SESSION * SEGMENT_SIZE];
    volatile bool readCacheIsSegmentReady[READ_CACHE_NUM_SEGMENTS_PER_SESSION];
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
    boost::uint64_t Push(BundleStorageManagerSession_WriteToDisk & session, bpv6_primary_block & bundlePrimaryBlock, const boost::uint64_t bundleSizeBytes); //return totalSegmentsRequired
    int PushSegment(BundleStorageManagerSession_WriteToDisk & session, void * buf, std::size_t size);

    //Read
    uint64_t PopTop(BundleStorageManagerSession_ReadFromDisk & session, const std::vector<uint64_t> & availableDestLinks); //0 if empty, size if entry
    bool ReturnTop(BundleStorageManagerSession_ReadFromDisk & session);
    std::size_t TopSegment(BundleStorageManagerSession_ReadFromDisk & session, void * buf);
    bool RemoveReadBundleFromDisk(BundleStorageManagerSession_ReadFromDisk & session, bool forceRemove = false);




    bool RestoreFromDisk(uint64_t * totalBundlesRestored, uint64_t * totalBytesRestored, uint64_t * totalSegmentsRestored);

    const MemoryManagerTreeArray & GetMemoryManagerConstRef();


protected:

    
    virtual void NotifyDiskOfWorkToDo_ThreadSafe(const unsigned int diskId) = 0;

protected:
    StorageConfig_ptr m_storageConfigPtr;
public:
    const unsigned int M_NUM_STORAGE_DISKS;
    const boost::uint64_t M_TOTAL_STORAGE_CAPACITY_BYTES; //old FILE_SIZE
    const boost::uint64_t M_MAX_SEGMENTS;
protected:
    MemoryManagerTreeArray m_memoryManager;
    destination_map_t m_destMap;
    boost::mutex m_mutexMainThread;
    boost::mutex::scoped_lock m_lockMainThread;
    boost::condition_variable m_conditionVariableMainThread;
    std::vector<boost::filesystem::path> m_filePathsVec;
    std::vector<std::string> m_filePathsAsStringVec;
    std::vector<CircularIndexBufferSingleProducerSingleConsumerConfigurable> m_circularIndexBuffersVec;

    boost::uint8_t * m_circularBufferBlockDataPtr;
    segment_id_t * m_circularBufferSegmentIdsPtr;
    //volatile bool * volatile m_circularBufferIsReadCompletedPointers[CIRCULAR_INDEX_BUFFER_SIZE * NUM_STORAGE_THREADS];
    //volatile boost::uint8_t * volatile m_circularBufferReadFromStoragePointers[CIRCULAR_INDEX_BUFFER_SIZE * NUM_STORAGE_THREADS];
    volatile bool * volatile m_circularBufferIsReadCompletedPointers[CIRCULAR_INDEX_BUFFER_SIZE * MAX_NUM_STORAGE_THREADS];
    volatile boost::uint8_t * volatile m_circularBufferReadFromStoragePointers[CIRCULAR_INDEX_BUFFER_SIZE * MAX_NUM_STORAGE_THREADS];
    volatile bool m_autoDeleteFilesOnExit;
    
public:
    bool m_successfullyRestoredFromDisk;
    uint64_t m_totalBundlesRestored;
    uint64_t m_totalBytesRestored;
    uint64_t m_totalSegmentsRestored;
};


#endif //_BUNDLE_STORAGE_MANAGER_BASE_H
