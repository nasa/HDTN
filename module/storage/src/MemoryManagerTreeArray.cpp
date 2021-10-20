/***************************************************************************
 * NASA Glenn Research Center, Cleveland, OH
 * Released under the NASA Open Source Agreement (NOSA)
 * May  2021
 *
 ****************************************************************************
 */
#include "MemoryManagerTreeArray.h"
#include <boost/multiprecision/cpp_int.hpp>
#include <boost/multiprecision/detail/bitscan.hpp>
#include <iostream>
#include <string>
#include <inttypes.h>
#ifdef USE_X86_HARDWARE_ACCELERATION
#include <immintrin.h>
#endif

 //static uint64_t g_numLeaves = 0;

MemoryManagerTreeArray::MemoryManagerTreeArray(const uint64_t maxSegments) : M_MAX_SEGMENTS(maxSegments) {
    SetupTree();
}
MemoryManagerTreeArray::~MemoryManagerTreeArray() {
    FreeTree();
}

void MemoryManagerTreeArray::SetupTree() {
    for (unsigned int i = 0; i < MAX_TREE_ARRAY_DEPTH; ++i) {
        const uint64_t arraySize64s = (((uint64_t)1) << (i * 6));
        //std::cout << i << " " << arraySize64s << "\n";
        m_bitMasks[i] = (uint64_t*)malloc(arraySize64s * sizeof(uint64_t));
        uint64_t * const currentArrayPtr = m_bitMasks[i];
        for (uint64_t j = 0; j < arraySize64s; ++j) {
            currentArrayPtr[j] = UINT64_MAX;
        }
    }


}


void MemoryManagerTreeArray::FreeTree() {
    for (unsigned int i = 0; i < MAX_TREE_ARRAY_DEPTH; ++i) {
        free(m_bitMasks[i]);
    }
}

void MemoryManagerTreeArray::BackupDataToVector(backup_memmanager_t & backup) const {
    backup.resize(MAX_TREE_ARRAY_DEPTH);
    for (unsigned int i = 0; i < MAX_TREE_ARRAY_DEPTH; ++i) {
        const uint64_t arraySize64s = (((uint64_t)1) << (i * 6));
        std::vector<uint64_t> & row = backup[i];
        row.resize(arraySize64s);
        const uint64_t * const currentArrayPtr = m_bitMasks[i];
        for (uint64_t j = 0; j < arraySize64s; ++j) {
            row[j] = currentArrayPtr[j];
        }
    }
}

bool MemoryManagerTreeArray::IsBackupEqual(const backup_memmanager_t & backup) const {
    for (unsigned int i = 0; i < MAX_TREE_ARRAY_DEPTH; ++i) {
        const uint64_t arraySize64s = (((uint64_t)1) << (i * 6));
        const std::vector<uint64_t> & row = backup[i];
        const uint64_t * const currentArrayPtr = m_bitMasks[i];
        for (uint64_t j = 0; j < arraySize64s; ++j) {
            if (row[j] != currentArrayPtr[j]) return false;
        }
    }
    return true;
}


bool MemoryManagerTreeArray::GetAndSetFirstFreeSegmentId(const uint32_t depthIndex, const uint32_t rowIndex, uint32_t * segmentId) {

    uint64_t * const currentArrayPtr = m_bitMasks[depthIndex];
    uint64_t * const currentBit64Ptr = &currentArrayPtr[rowIndex];
    const unsigned int firstFreeIndex = boost::multiprecision::detail::find_lsb<uint64_t>(*currentBit64Ptr);
    *segmentId += firstFreeIndex * (1 << (((MAX_TREE_ARRAY_DEPTH - 1) - depthIndex) * 6)); // 64^depth


    if ((depthIndex == MAX_TREE_ARRAY_DEPTH - 1) || GetAndSetFirstFreeSegmentId(depthIndex + 1, rowIndex + firstFreeIndex * (1 << (depthIndex * 6)), segmentId)) {
        if (*segmentId < M_MAX_SEGMENTS) {
#ifdef USE_X86_HARDWARE_ACCELERATION
#if !defined(__GNUC__)
            _bittestandreset64((int64_t*)currentBit64Ptr, firstFreeIndex);
#else
            const uint64_t mask64 = (((uint64_t)1) << firstFreeIndex);
            *currentBit64Ptr = _andn_u64(mask64, *currentBit64Ptr);
#endif
#else
            const uint64_t mask64 = (((uint64_t)1) << firstFreeIndex);
            *currentBit64Ptr &= ~mask64;
#endif
        }
    }
    /*

    if (depthIndex == MAX_TREE_ARRAY_DEPTH - 1) { //at the child leaf
        *currentBit64Ptr &= ~mask64;

    }
    else { //at an inner node
        const unsigned int nextRowIndex = rowIndex + firstFreeIndex * (1 << (depthIndex * 6));
        if (GetAndSetFirstFreeSegmentId(depthIndex + 1, nextRowIndex, segmentId)) {
            *currentBit64Ptr &= ~mask64;
        }
    }
    */

    return (*currentBit64Ptr == 0); //return isFull




}

segment_id_t MemoryManagerTreeArray::GetAndSetFirstFreeSegmentId_NotThreadSafe() {
    if (m_bitMasks[0][0] == 0) return UINT32_MAX; //bitmask of zero means full
    uint32_t segmentId = 0;
    GetAndSetFirstFreeSegmentId(0, 0, &segmentId);
    if (segmentId >= M_MAX_SEGMENTS) return UINT32_MAX;
    return segmentId;
}

bool MemoryManagerTreeArray::IsSegmentFree(const uint32_t depthIndex, const uint32_t rowIndex, uint32_t segmentId) {

    const unsigned int index = (segmentId >> (((MAX_TREE_ARRAY_DEPTH - 1) - depthIndex) * 6)) & 63;

    if (depthIndex == MAX_TREE_ARRAY_DEPTH - 1) { //leaf
        const uint64_t * const currentArrayPtr = m_bitMasks[depthIndex];
        const uint64_t * const currentBit64Ptr = &currentArrayPtr[rowIndex];
#if defined(USE_X86_HARDWARE_ACCELERATION) && !defined(__GNUC__)
        return _bittest64((const int64_t*)currentBit64Ptr, index);
#else
        const uint64_t mask64 = (((uint64_t)1) << index);
        return (*currentBit64Ptr & mask64); //leaf bit is 1 (empty)
#endif
    }
    else { //inner node
        return IsSegmentFree(depthIndex + 1, rowIndex + index * (1 << (depthIndex * 6)), segmentId);
    }
}

bool MemoryManagerTreeArray::IsSegmentFree(segment_id_t segmentId) {
    return IsSegmentFree(0, 0, segmentId);
}

void MemoryManagerTreeArray::FreeSegmentId(const uint32_t depthIndex, const uint32_t rowIndex, uint32_t segmentId, bool *success) {

    uint64_t * const currentArrayPtr = m_bitMasks[depthIndex];
    uint64_t * const currentBit64Ptr = &currentArrayPtr[rowIndex];
    const unsigned int index = (segmentId >> (((MAX_TREE_ARRAY_DEPTH - 1) - depthIndex) * 6)) & 63;
#if defined(USE_X86_HARDWARE_ACCELERATION) && !defined(__GNUC__)
    const bool bitWasAlreadyOne = _bittestandset64((int64_t*)currentBit64Ptr, index);
    if (depthIndex == MAX_TREE_ARRAY_DEPTH - 1) { //leaf
        *success = !bitWasAlreadyOne; //error if leaf bit is already 1 (empty)
    }
    else { //inner node
        FreeSegmentId(depthIndex + 1, rowIndex + index * (1 << (depthIndex * 6)), segmentId, success);
    }
#else
    const uint64_t mask64 = (((uint64_t)1) << index);

    if (depthIndex == MAX_TREE_ARRAY_DEPTH - 1) { //leaf
        //if (*currentBit64Ptr & mask64) { //leaf bit is already 1 (empty)
        //    *success = false; //error
        //}
        *success = ((*currentBit64Ptr & mask64) == 0);
    }
    else { //inner node
        FreeSegmentId(depthIndex + 1, rowIndex + index * (1 << (depthIndex * 6)), segmentId, success);
    }
    *currentBit64Ptr |= mask64;
#endif
}

bool MemoryManagerTreeArray::AllocateSegmentId_NoCheck(const uint32_t depthIndex, const uint32_t rowIndex, uint32_t segmentId) {

    uint64_t * const currentArrayPtr = m_bitMasks[depthIndex];
    uint64_t * const currentBit64Ptr = &currentArrayPtr[rowIndex];
    const unsigned int index = (segmentId >> (((MAX_TREE_ARRAY_DEPTH - 1) - depthIndex) * 6)) & 63;

    if ((depthIndex == MAX_TREE_ARRAY_DEPTH - 1) || AllocateSegmentId_NoCheck(depthIndex + 1, rowIndex + index * (1 << (depthIndex * 6)), segmentId)) {
        if (segmentId < M_MAX_SEGMENTS) {
#ifdef USE_X86_HARDWARE_ACCELERATION
#if !defined(__GNUC__)
            _bittestandreset64((int64_t*)currentBit64Ptr, index);
#else
            const uint64_t mask64 = (((uint64_t)1) << index);
            *currentBit64Ptr = _andn_u64(mask64, *currentBit64Ptr);
#endif
#else
            const uint64_t mask64 = (((uint64_t)1) << index);
            *currentBit64Ptr &= ~mask64;
#endif
        }
    }


    return (*currentBit64Ptr == 0); //return isFull


}

void MemoryManagerTreeArray::AllocateSegmentId_NoCheck_NotThreadSafe(segment_id_t segmentId) {
    AllocateSegmentId_NoCheck(0, 0, segmentId);
}

bool MemoryManagerTreeArray::FreeSegmentId_NotThreadSafe(segment_id_t segmentId) {
    bool success = true;
    FreeSegmentId(0, 0, segmentId, &success);
    return success;
}

bool MemoryManagerTreeArray::AllocateSegments_ThreadSafe(segment_id_chain_vec_t & segmentVec) { //number of segments should be the vector size
    boost::mutex::scoped_lock lock(m_mutex);
    const std::size_t size = segmentVec.size();
    for (std::size_t i = 0; i < size; ++i) {
        const segment_id_t segmentId = GetAndSetFirstFreeSegmentId_NotThreadSafe();
        if (segmentId != UINT32_MAX) { //success
            segmentVec[i] = segmentId;
        }
        else { //fail
            for (std::size_t j = 0; j < i; ++j) {
                FreeSegmentId_NotThreadSafe(segmentVec[j]);
            }
            segmentVec.resize(0);
            return false;
        }
    }
    return true;
}

bool MemoryManagerTreeArray::FreeSegments_ThreadSafe(const segment_id_chain_vec_t & segmentVec) {
    boost::mutex::scoped_lock lock(m_mutex);
    const std::size_t size = segmentVec.size();
    bool success = true;
    for (std::size_t i = 0; i < size; ++i) {
        if (!FreeSegmentId_NotThreadSafe(segmentVec[i])) {
            success = false;
        }
    }
    //segmentVec.resize(0);
    return success;
}


