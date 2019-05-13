#ifndef _BUNDLE_STORAGE_MANAGER_MT_H
#define _BUNDLE_STORAGE_MANAGER_MT_H

#include <boost/integer.hpp>
#include <stdint.h>
#include <map>
#include <vector>
#include <string>
#include <cstdio>
#include <boost/iostreams/device/mapped_file.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/thread.hpp>
#include "CircularIndexBufferSingleProducerSingleConsumer.h"
#include "BundleStorageConfig.h"

typedef boost::uint32_t segment_id_t;
typedef boost::uint64_t abs_expiration_t;
typedef std::vector<segment_id_t> segment_id_vec_t;
typedef std::map<abs_expiration_t, segment_id_vec_t> expiration_map_t;
typedef std::vector<expiration_map_t> priority_vec_t;
typedef std::map<std::string, priority_vec_t> destination_map_t;



class BundleStorageManagerMT {
public:
	BundleStorageManagerMT();
	~BundleStorageManagerMT();

	
	
	void AddLink(const std::string & linkName);
	void StoreBundle(const std::string & linkName, const unsigned int priorityIndex, const abs_expiration_t absExpiration, const segment_id_t segmentId, const unsigned char * const data);
	segment_id_t GetBundle(const std::vector<std::string> & availableDestLinks);
	
	static bool TimeRandomReadsAndWrites();

private:
	void ThreadFunc(unsigned int threadIndex);

private:
	destination_map_t m_destMap;
	boost::mutex m_mutexMainThread;
	boost::mutex::scoped_lock m_lockMainThread;
	boost::condition_variable m_conditionVariableMainThread;
	boost::condition_variable m_conditionVariables[NUM_STORAGE_THREADS];
	boost::shared_ptr<boost::thread> m_threadPtrs[NUM_STORAGE_THREADS];
	CircularIndexBufferSingleProducerSingleConsumer m_circularIndexBuffers[NUM_STORAGE_THREADS];
	volatile bool m_running;
	boost::uint8_t * m_circularBufferBlockDataPtr;
	segment_id_t * m_circularBufferSegmentIdsPtr;
	bool * m_circularBufferReadWriteBoolsPtr;
};


#endif //_BUNDLE_STORAGE_MANAGER_MT_H
