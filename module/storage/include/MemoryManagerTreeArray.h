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
typedef backup_memmanager_t memmanager_t;

class MemoryManagerTreeArray {
private:
    MemoryManagerTreeArray();
public:
    STORAGE_LIB_EXPORT MemoryManagerTreeArray(const uint64_t maxSegments);
    STORAGE_LIB_EXPORT ~MemoryManagerTreeArray();

    STORAGE_LIB_EXPORT bool AllocateSegments_ThreadSafe(segment_id_chain_vec_t & segmentVec); //number of segments should be the vector size
    STORAGE_LIB_EXPORT bool FreeSegments_ThreadSafe(const segment_id_chain_vec_t & segmentVec);
    STORAGE_LIB_EXPORT bool IsSegmentFree(const segment_id_t segmentId) const;
    STORAGE_LIB_EXPORT void AllocateSegmentId_NoCheck_NotThreadSafe(const segment_id_t segmentId);
    STORAGE_LIB_EXPORT void BackupDataToVector(backup_memmanager_t & backup) const;
    STORAGE_LIB_EXPORT const memmanager_t & GetVectorsConstRef() const;
    STORAGE_LIB_EXPORT bool IsBackupEqual(const backup_memmanager_t & backup) const;

    STORAGE_LIB_EXPORT bool FreeSegmentId_NotThreadSafe(const segment_id_t segmentId);
    STORAGE_LIB_EXPORT segment_id_t GetAndSetFirstFreeSegmentId_NotThreadSafe();

private:


    STORAGE_LIB_NO_EXPORT bool GetAndSetFirstFreeSegmentId(const segment_id_t depthIndex, const segment_id_t longIndex, segment_id_t & segmentId);
    STORAGE_LIB_NO_EXPORT bool FreeSegmentId(const segment_id_t segmentId);
    STORAGE_LIB_NO_EXPORT bool AllocateSegmentId_NoCheck(const segment_id_t depthIndex, const segment_id_t rowIndex, const segment_id_t segmentId);
    STORAGE_LIB_NO_EXPORT void AllocateRows(const segment_id_t largestSegmentId);
    STORAGE_LIB_NO_EXPORT void AllocateRowsMaxMemory();
private:
    const uint64_t M_MAX_SEGMENTS;
    std::vector<std::vector<uint64_t> > m_bitMasks;
    boost::mutex m_mutex;
};


#endif //_MEMORY_MANAGER_TREE_ARRAY_H
