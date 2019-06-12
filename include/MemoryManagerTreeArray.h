#ifndef _MEMORY_MANAGER_TREE_ARRAY_H
#define _MEMORY_MANAGER_TREE_ARRAY_H

#include <boost/integer.hpp>
#include <stdint.h>
#include "BundleStorageConfig.h"
#include <boost/thread.hpp>
#include <vector>

typedef boost::uint32_t segment_id_t;
typedef std::vector<segment_id_t> segment_id_chain_vec_t;

class MemoryManagerTreeArray {
public:
	MemoryManagerTreeArray();
	~MemoryManagerTreeArray();
	
	bool AllocateSegments_ThreadSafe(segment_id_chain_vec_t & segmentVec); //number of segments should be the vector size
	bool FreeSegments_ThreadSafe(segment_id_chain_vec_t & segmentVec);
	
	bool FreeSegmentId_NotThreadSafe(segment_id_t segmentId);
	segment_id_t GetAndSetFirstFreeSegmentId_NotThreadSafe();

private:
	

	bool GetAndSetFirstFreeSegmentId(const boost::uint32_t depthIndex, const boost::uint32_t rowIndex, boost::uint32_t * segmentId);
	void FreeSegmentId(const boost::uint32_t depthIndex, const boost::uint32_t rowIndex, boost::uint32_t segmentId, bool *success);
	void SetupTree();
	void FreeTree();
private:

	boost::uint64_t * m_bitMasks[MAX_TREE_ARRAY_DEPTH];
	boost::mutex m_mutex;
};


#endif //_MEMORY_MANAGER_TREE_ARRAY_H
