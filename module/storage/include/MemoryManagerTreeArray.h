/**
 * @file MemoryManagerTreeArray.h
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
 * The MemoryManagerTreeArray class is used for fast allocation/deallocation
 * of 4KByte segments for bundle storage.
 */

#ifndef _MEMORY_MANAGER_TREE_ARRAY_H
#define _MEMORY_MANAGER_TREE_ARRAY_H 1

#include <boost/integer.hpp>
#include <stdint.h>
#include "BundleStorageConfig.h"
#include <boost/thread.hpp>
#include <vector>
#include "storage_lib_export.h"


typedef std::vector<segment_id_t> segment_id_chain_vec_t;

typedef std::vector< std::vector<uint64_t> > memmanager_t;

class MemoryManagerTreeArray {
private:
    MemoryManagerTreeArray();
public:
    /**
    * Constructor that sets the max number of segments that can be allocated
    * and sets up internal data structures.
    *
    * @param maxSegments The max number of segments that can be allocated.
    *        Memory requirements for this class is approximately 1 bit per segment.
    */
    STORAGE_LIB_EXPORT MemoryManagerTreeArray(const uint64_t maxSegments);
    STORAGE_LIB_EXPORT ~MemoryManagerTreeArray();

    /** Thread safe method to allocate a vector of the first available free segment numbers in numerical order.
     * The desired number of segments must be set prior to the call by calling segmentVec.resize() (i.e. number of desired segments should be the vector size).
     *
     * @param segmentVec The preallocated vector of segments to be filled.  Will be resized to zero on failure.
     * @return True if the segmentVec was fully populated (the MemoryManagerTreeArray was not full prior to the last segment being allocated), or False otherwise.
     *         The segmentVec is also resized to zero on a return of False.
     * @post The internal data structures are updated if and only if the MemoryManagerTreeArray was able to allocate the entire vector of segment IDs.
     */
    STORAGE_LIB_EXPORT bool AllocateSegments_ThreadSafe(segment_id_chain_vec_t & segmentVec);

    /** Thread safe method to free a vector of segment numbers.
     *
     * @param segmentVec The vector of segments to mark as free in the internal data structure.
     * @return True if all the segment numbers in the segmentVec were freed, or False otherwise.
     * @post The internal data structures are updated for only the segment IDs that were allocated (now they are marked free).  Segments that were already free remain unchanged (with False returned).
     */
    STORAGE_LIB_EXPORT bool FreeSegments_ThreadSafe(const segment_id_chain_vec_t & segmentVec);

    /** Test if the specified segment is free.
     *
     * @param segmentId The segment to be tested.
     * @return True if and only if the segmentId is available/free, or False otherwise.
     */
    STORAGE_LIB_EXPORT bool IsSegmentFree(const segment_id_t segmentId) const;

    /** Backup the internal data structure to the given reference parameter, useful for equality comparision in unit testing.
     *
     * @param backup The data to copy the internal data structure to.
     */
    STORAGE_LIB_EXPORT void BackupDataToVector(memmanager_t & backup) const;

    /** Get a const reference to the internal data structure, useful for equality comparision in unit testing.
     *
     * @return A const reference to the internal data structure.
     */
    STORAGE_LIB_EXPORT const memmanager_t & GetVectorsConstRef() const;

    /** Compare the internal data structure to the given const reference parameter, useful for equality comparision in unit testing.
     *
     * @param backup The data to compare the internal data structure to.
     * @return True if the data structures are equal, or False otherwise.
     */
    STORAGE_LIB_EXPORT bool IsBackupEqual(const memmanager_t & backup) const;

    /** Free the given segment Id.
     *
     * @param segmentId The segment to be freed from the internal data structure.
     * @return True if the given segment number was freed, or False otherwise.
     * @post The internal data structures are updated if and only if the given segment Id was allocated prior to the call.
     */
    STORAGE_LIB_EXPORT bool FreeSegmentId_NotThreadSafe(const segment_id_t segmentId);

    /** Allocate and get the first available free segment number in numerical order.  Requires 6 recursive calls.
     *
     * @return The first available free segment Id if the MemoryManagerTreeArray is not full, or SEGMENT_ID_FULL otherwise.
     * @post The internal data structures are updated if and only if the MemoryManagerTreeArray is not full.
     */
    STORAGE_LIB_EXPORT segment_id_t GetAndSetFirstFreeSegmentId_NotThreadSafe();

    /** Manually allocate the specified segment (if free), useful for restore from disk (rebuilding the MemoryManagerTreeArray after power loss) operations.
     *
     * @param segmentId The segment to be set as allocated.
     * @return True if and only if the segmentId is available and marked as allocated, or False otherwise.
     * @post The internal data structures are updated if and only if the segmentId is available.
     */
    STORAGE_LIB_EXPORT bool AllocateSegmentId_NotThreadSafe(const segment_id_t segmentId);

private:


    STORAGE_LIB_NO_EXPORT bool GetAndSetFirstFreeSegmentId(const segment_id_t depthIndex, segment_id_t & segmentId);
    STORAGE_LIB_NO_EXPORT void AllocateRows(const segment_id_t largestSegmentId);
    STORAGE_LIB_NO_EXPORT void AllocateRowsMaxMemory();
private:
    const uint64_t M_MAX_SEGMENTS;
    std::vector<std::vector<uint64_t> > m_bitMasks;
    boost::mutex m_mutex;
};


#endif //_MEMORY_MANAGER_TREE_ARRAY_H
