#ifndef _MEMORY_MANAGER_TREE_ARRAY_H
#define _MEMORY_MANAGER_TREE_ARRAY_H

#include <boost/integer.hpp>
#include <stdint.h>
#include "BundleStorageConfig.h"
#include <boost/thread.hpp>
#include <vector>
#include "storage_lib_export.h"

typedef uint32_t segment_id_t;
typedef std::vector<segment_id_t> segment_id_chain_vec_t;

typedef std::vector< std::vector<uint64_t> > backup_memmanager_t;

class MemoryManagerTreeArray {
private:
    MemoryManagerTreeArray();
public:
    STORAGE_LIB_EXPORT MemoryManagerTreeArray(const boost::uint64_t maxSegments);
    STORAGE_LIB_EXPORT ~MemoryManagerTreeArray();

    STORAGE_LIB_EXPORT bool AllocateSegments_ThreadSafe(segment_id_chain_vec_t & segmentVec); //number of segments should be the vector size
    STORAGE_LIB_EXPORT bool FreeSegments_ThreadSafe(const segment_id_chain_vec_t & segmentVec);
    STORAGE_LIB_EXPORT bool IsSegmentFree(segment_id_t segmentId);
    STORAGE_LIB_EXPORT void AllocateSegmentId_NoCheck_NotThreadSafe(segment_id_t segmentId);
    STORAGE_LIB_EXPORT void BackupDataToVector(backup_memmanager_t & backup) const;
    STORAGE_LIB_EXPORT bool IsBackupEqual(const backup_memmanager_t & backup) const;

    STORAGE_LIB_EXPORT bool FreeSegmentId_NotThreadSafe(segment_id_t segmentId);
    STORAGE_LIB_EXPORT segment_id_t GetAndSetFirstFreeSegmentId_NotThreadSafe();

private:


    STORAGE_LIB_EXPORT bool GetAndSetFirstFreeSegmentId(const boost::uint32_t depthIndex, const boost::uint32_t rowIndex, boost::uint32_t * segmentId);
    STORAGE_LIB_EXPORT bool IsSegmentFree(const boost::uint32_t depthIndex, const boost::uint32_t rowIndex, boost::uint32_t segmentId);
    STORAGE_LIB_EXPORT void FreeSegmentId(const boost::uint32_t depthIndex, const boost::uint32_t rowIndex, boost::uint32_t segmentId, bool *success);
    STORAGE_LIB_EXPORT bool AllocateSegmentId_NoCheck(const boost::uint32_t depthIndex, const boost::uint32_t rowIndex, boost::uint32_t segmentId);
    STORAGE_LIB_EXPORT void SetupTree();
    STORAGE_LIB_EXPORT void FreeTree();
private:
    const boost::uint64_t M_MAX_SEGMENTS;
    boost::uint64_t * m_bitMasks[MAX_TREE_ARRAY_DEPTH];
    boost::mutex m_mutex;
};


#endif //_MEMORY_MANAGER_TREE_ARRAY_H
