#ifndef _BUNDLE_STORAGE_MANAGER_MT_H
#define _BUNDLE_STORAGE_MANAGER_MT_H

#include <boost/integer.hpp>
#include <stdint.h>
#include <map>
#include <vector>
#include <utility>
#include <string>
#include <cstdio>
#include <boost/iostreams/device/mapped_file.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/thread.hpp>
#include "CircularIndexBufferSingleProducerSingleConsumerConfigurable.h"
#include "BundleStorageConfig.h"
#include "MemoryManagerTreeArray.h"
#include "StorageConfig.h"


typedef boost::uint64_t abs_expiration_t;
typedef std::pair<boost::uint64_t, segment_id_chain_vec_t> chain_info_t; //bundleSizeBytes, segment_id_chain_vec_t

typedef std::vector<chain_info_t> chain_info_vec_t;
typedef std::map<abs_expiration_t, chain_info_vec_t> expiration_map_t;
typedef std::vector<expiration_map_t> priority_vec_t;
typedef std::map<uint64_t, priority_vec_t> destination_map_t; //dst_node, priority_vec

typedef struct bp_primary_if_base_t {
	uint8_t version;
	uint8_t type;
	uint16_t blocklen;
	uint32_t flags;
	uint64_t framelen;
	uint64_t creation; // creation time in usec
	uint64_t sequence; // sequence
	uint64_t lifetime; // lifetime of this bundle in usec
	uint64_t offset; // only used for fragments
	uint64_t length;

	uint64_t dst_node;
	uint64_t dst_svc;
	uint64_t src_node;
	uint64_t src_svc;
	uint64_t custodian_node;
	uint64_t custodian_svc;
	uint64_t report_node;
	uint64_t report_svc;

	uint64_t flowid;
} bp_primary_if_base_t;


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
	chain_info_vec_t * chainInfoVecPtr;
	expiration_map_t::iterator expirationMapIterator;

	uint64_t destLinkId;
	unsigned int priorityIndex;
	abs_expiration_t absExpiration;
	volatile uint8_t readCache[READ_CACHE_NUM_SEGMENTS_PER_SESSION * SEGMENT_SIZE];
	volatile bool readCacheIsSegmentReady[READ_CACHE_NUM_SEGMENTS_PER_SESSION];
};

class BundleStorageManagerMT {
public:
	BundleStorageManagerMT();
    BundleStorageManagerMT(const std::string & jsonConfigFileName);
	~BundleStorageManagerMT();
	void Start(bool autoDeleteFilesOnExit = true);

	//write
	boost::uint64_t Push(BundleStorageManagerSession_WriteToDisk & session, bp_primary_if_base_t & bundleMetaData); //return totalSegmentsRequired
	int PushSegment(BundleStorageManagerSession_WriteToDisk & session, void * buf, std::size_t size);

	//Read
	uint64_t PopTop(BundleStorageManagerSession_ReadFromDisk & session, const std::vector<uint64_t> & availableDestLinks); //0 if empty, size if entry
	bool ReturnTop(BundleStorageManagerSession_ReadFromDisk & session);
	std::size_t TopSegment(BundleStorageManagerSession_ReadFromDisk & session, void * buf);
	bool RemoveReadBundleFromDisk(BundleStorageManagerSession_ReadFromDisk & session, bool forceRemove = false);
	
	
	

	bool RestoreFromDisk(uint64_t * totalBundlesRestored, uint64_t * totalBytesRestored, uint64_t * totalSegmentsRestored);

	const MemoryManagerTreeArray & GetMemoryManagerConstRef();
	
	static bool TestSpeed();

private:
	void ThreadFunc(unsigned int threadIndex);
	void AddLink(boost::uint64_t linkName);

private:
	StorageConfig_ptr m_storageConfigPtr;
	const unsigned int M_NUM_STORAGE_THREADS;
	const boost::uint64_t M_TOTAL_STORAGE_CAPACITY_BYTES; //old FILE_SIZE
	const boost::uint64_t M_MAX_SEGMENTS;
	MemoryManagerTreeArray m_memoryManager;
	destination_map_t m_destMap;
	boost::mutex m_mutexMainThread;
	boost::mutex::scoped_lock m_lockMainThread;
	boost::condition_variable m_conditionVariableMainThread;
	//boost::condition_variable m_conditionVariables[NUM_STORAGE_THREADS];
	//boost::shared_ptr<boost::thread> m_threadPtrs[NUM_STORAGE_THREADS];
	//CircularIndexBufferSingleProducerSingleConsumer m_circularIndexBuffers[NUM_STORAGE_THREADS];
	std::vector<boost::condition_variable> m_conditionVariablesVec;
	std::vector<boost::shared_ptr<boost::thread> > m_threadPtrsVec;
	std::vector<CircularIndexBufferSingleProducerSingleConsumerConfigurable> m_circularIndexBuffersVec;
	
	volatile bool m_running;
	boost::uint8_t * m_circularBufferBlockDataPtr;
	segment_id_t * m_circularBufferSegmentIdsPtr;
	//volatile bool * volatile m_circularBufferIsReadCompletedPointers[CIRCULAR_INDEX_BUFFER_SIZE * NUM_STORAGE_THREADS];
	//volatile boost::uint8_t * volatile m_circularBufferReadFromStoragePointers[CIRCULAR_INDEX_BUFFER_SIZE * NUM_STORAGE_THREADS];
	volatile bool * volatile m_circularBufferIsReadCompletedPointers[CIRCULAR_INDEX_BUFFER_SIZE * MAX_NUM_STORAGE_THREADS];
	volatile boost::uint8_t * volatile m_circularBufferReadFromStoragePointers[CIRCULAR_INDEX_BUFFER_SIZE * MAX_NUM_STORAGE_THREADS];
	volatile bool m_successfullyRestoredFromDisk;
	volatile bool m_autoDeleteFilesOnExit;
	
};


#endif //_BUNDLE_STORAGE_MANAGER_MT_H
