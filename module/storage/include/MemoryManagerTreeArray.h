#ifndef _MEMORY_MANAGER_TREE_ARRAY_H
#define _MEMORY_MANAGER_TREE_ARRAY_H

#include <boost/integer.hpp>
#include <stdint.h>
#include "BundleStorageConfig.h"
#include <boost/thread.hpp>
#include <vector>

typedef boost::uint32_t segment_id_t;
typedef std::vector<segment_id_t> segment_id_chain_vec_t;

typedef std::vector< std::vector<boost::uint64_t> > backup_memmanager_t;

class MemoryManagerTreeArray {
private:
	MemoryManagerTreeArray();
public:
	MemoryManagerTreeArray(const boost::uint64_t maxSegments);
	~MemoryManagerTreeArray();
	
	bool AllocateSegments_ThreadSafe(segment_id_chain_vec_t & segmentVec); //number of segments should be the vector size
	bool FreeSegments_ThreadSafe(const segment_id_chain_vec_t & segmentVec);
	bool IsSegmentFree(segment_id_t segmentId);
	void AllocateSegmentId_NoCheck_NotThreadSafe(segment_id_t segmentId);
	void BackupDataToVector(backup_memmanager_t & backup) const;
	bool IsBackupEqual(const backup_memmanager_t & backup) const;
	
	bool FreeSegmentId_NotThreadSafe(segment_id_t segmentId);
	segment_id_t GetAndSetFirstFreeSegmentId_NotThreadSafe();

private:
	

	bool GetAndSetFirstFreeSegmentId(const boost::uint32_t depthIndex, const boost::uint32_t rowIndex, boost::uint32_t * segmentId);
	bool IsSegmentFree(const boost::uint32_t depthIndex, const boost::uint32_t rowIndex, boost::uint32_t segmentId);
	void FreeSegmentId(const boost::uint32_t depthIndex, const boost::uint32_t rowIndex, boost::uint32_t segmentId, bool *success);
	bool AllocateSegmentId_NoCheck(const boost::uint32_t depthIndex, const boost::uint32_t rowIndex, boost::uint32_t segmentId);
	void SetupTree();
	void FreeTree();
private:
	const boost::uint64_t M_MAX_SEGMENTS;
	boost::uint64_t * m_bitMasks[MAX_TREE_ARRAY_DEPTH];
	boost::mutex m_mutex;
};


#endif //_MEMORY_MANAGER_TREE_ARRAY_H
