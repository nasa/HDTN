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
#ifdef USE_BITTEST
# include <immintrin.h>
# ifdef HAVE_INTRIN_H
#  include <intrin.h>
# endif
# ifdef HAVE_X86INTRIN_H
#  include <x86intrin.h>
# endif
#elif defined(USE_ANDN)
# include <immintrin.h>
# if defined(__GNUC__) && defined(HAVE_X86INTRIN_H)
#  include <x86intrin.h>
#  define _andn_u64(a, b)   (__andn_u64((a), (b)))
# elif defined(_MSC_VER)
#  include <ammintrin.h>
# endif
#endif //USE_BITTEST or USE_ANDN

MemoryManagerTreeArray::MemoryManagerTreeArray(const uint64_t maxSegments) : M_MAX_SEGMENTS(maxSegments), m_bitMasks(MAX_TREE_ARRAY_DEPTH) {
#if 0
    AllocateRowsMaxMemory();
#else
    AllocateRows(static_cast<segment_id_t>(M_MAX_SEGMENTS));
#endif
}
MemoryManagerTreeArray::~MemoryManagerTreeArray() {}


void MemoryManagerTreeArray::BackupDataToVector(memmanager_t & backup) const {
    backup = m_bitMasks;
}

const memmanager_t & MemoryManagerTreeArray::GetVectorsConstRef() const {
    return m_bitMasks;
}

bool MemoryManagerTreeArray::IsBackupEqual(const memmanager_t & backup) const {
    return (backup == m_bitMasks);
}


bool MemoryManagerTreeArray::GetAndSetFirstFreeSegmentId(const segment_id_t depthIndex, segment_id_t & segmentId) {
    uint64_t & longRef = m_bitMasks[depthIndex][segmentId];
    const unsigned int firstFreeBitIndex = boost::multiprecision::detail::find_lsb<uint64_t>(longRef);
    segmentId = (segmentId << 6) | firstFreeBitIndex; //same as: segmentId += firstFreeBitIndex * (1 << inverseDepthIndexTimes6); // 64^depth

    const bool isLeaf = (depthIndex == (MAX_TREE_ARRAY_DEPTH - 1));
    if (isLeaf || GetAndSetFirstFreeSegmentId(depthIndex + 1, segmentId)) {
        if (segmentId < M_MAX_SEGMENTS) {
#ifdef USE_BITTEST
            _bittestandreset64((int64_t*)&longRef, firstFreeBitIndex);
#else
            const uint64_t mask64 = (((uint64_t)1) << firstFreeBitIndex);
# if defined(USE_ANDN)
            longRef = _andn_u64(mask64, longRef);
# else
            longRef &= (~mask64);
# endif
#endif
        }
    }

    return (longRef == 0); //return isFull
}

segment_id_t MemoryManagerTreeArray::GetAndSetFirstFreeSegmentId_NotThreadSafe() {
    if (m_bitMasks[0][0] == 0) return SEGMENT_ID_FULL; //bitmask of zero means full (prevent undefined behavior in boost::multiprecision::detail::find_lsb)
    segment_id_t segmentId = 0;
    GetAndSetFirstFreeSegmentId(0, segmentId);
    if (segmentId >= M_MAX_SEGMENTS) return SEGMENT_ID_FULL;
    return segmentId;
}

bool MemoryManagerTreeArray::IsSegmentFree(const segment_id_t segmentId) const {
    if (segmentId >= M_MAX_SEGMENTS) return false;
    //just look at the leaf node
    segment_id_t longIndex = segmentId >> 6; //divide by 64 bits per ui64
    const segment_id_t bitIndex = segmentId & 63;
    const uint64_t leafLong = m_bitMasks[MAX_TREE_ARRAY_DEPTH - 1][longIndex];
#ifdef USE_BITTEST
    return _bittest64((const int64_t*)&leafLong, bitIndex);
#else
    const uint64_t mask64 = (((uint64_t)1) << bitIndex);
    return (leafLong & mask64); //leaf bit is 1 (empty)
#endif
}

bool MemoryManagerTreeArray::FreeSegmentId(const segment_id_t segmentId) {
    if (segmentId >= M_MAX_SEGMENTS) return false;
    //start at the leaf node
    segment_id_t longIndex = segmentId;
    {
        const segment_id_t bitIndex = longIndex & 63;
        longIndex >>= 6; //divide by 64 bits per ui64
        uint64_t & longRef = m_bitMasks[MAX_TREE_ARRAY_DEPTH-1][longIndex];
#ifdef USE_BITTEST
        const bool bitWasAlreadyOne = _bittestandset64((int64_t*)&longRef, bitIndex);
        const bool success = !bitWasAlreadyOne; //error if leaf bit is already 1 (empty)
#else
        const uint64_t mask64 = (((uint64_t)1) << bitIndex);
        const bool success = ((longRef & mask64) == 0);
        longRef |= mask64;
#endif
        if (!success) {
            return false;
        }
    }
    for (segment_id_t depth = MAX_TREE_ARRAY_DEPTH - 1; depth != 0; --depth) {
        const segment_id_t bitIndex = longIndex & 63;
        longIndex >>= 6; //divide by 64 bits per ui64
        const segment_id_t depthIndex = depth - 1;
        uint64_t & longRef = m_bitMasks[depthIndex][longIndex];
#ifdef USE_BITTEST
        _bittestandset64((int64_t*)&longRef, bitIndex);
#else
        const uint64_t mask64 = (((uint64_t)1) << bitIndex);
        longRef |= mask64;
#endif
    }
    return true;
}

void MemoryManagerTreeArray::AllocateRows(const segment_id_t largestSegmentId) {
    //start at the leaf node
    segment_id_t longIndex = largestSegmentId >> 6; //divide by 64 bits per ui64
    for (segment_id_t depth = MAX_TREE_ARRAY_DEPTH; depth != 0; --depth) {
        segment_id_t depthIndex = depth - 1;
        m_bitMasks[depthIndex].assign(longIndex + 1, UINT64_MAX);
        longIndex >>= 6; //same as: rowIndex += index * (1 << inverseDepthIndexTimes6); //rowIndex += index * (64^depthIndex)
    }
}

void MemoryManagerTreeArray::AllocateRowsMaxMemory() {
    for (segment_id_t depthIndex = 0; depthIndex < MAX_TREE_ARRAY_DEPTH; ++depthIndex) {
        const uint64_t arraySize64s = (((uint64_t)1) << (depthIndex * 6));
        m_bitMasks[depthIndex].assign(arraySize64s, UINT64_MAX);
    }
}

bool MemoryManagerTreeArray::AllocateSegmentId_NotThreadSafe(const segment_id_t segmentId) {
    if (segmentId >= M_MAX_SEGMENTS) return false;
    //start at the leaf node
    segment_id_t longIndex = segmentId;
    bool childIsFull;
    {
        const segment_id_t bitIndex = longIndex & 63;
        longIndex >>= 6; //divide by 64 bits per ui64
        uint64_t & longRef = m_bitMasks[MAX_TREE_ARRAY_DEPTH - 1][longIndex];
#ifdef USE_BITTEST
        const bool bitWasAlreadyOne = _bittestandreset64((int64_t*)&longRef, bitIndex);
        const bool success = bitWasAlreadyOne; //error if leaf bit was already 0 (already allocated)
#else
        const uint64_t mask64 = (((uint64_t)1) << bitIndex);
        const bool success = ((longRef & mask64) != 0);
# if defined(USE_ANDN)
        longRef = _andn_u64(mask64, longRef);
# else
        longRef &= (~mask64);
# endif
#endif
        if (!success) {
            return false;
        }
        childIsFull = (longRef == 0);
    }
    for (segment_id_t depth = MAX_TREE_ARRAY_DEPTH - 1; ((depth != 0) && (childIsFull)); --depth) {
        const segment_id_t bitIndex = longIndex & 63;
        longIndex >>= 6; //divide by 64 bits per ui64
        const segment_id_t depthIndex = depth - 1;
        uint64_t & longRef = m_bitMasks[depthIndex][longIndex];
#ifdef USE_BITTEST
        _bittestandreset64((int64_t*)&longRef, bitIndex);
#else
        const uint64_t mask64 = (((uint64_t)1) << bitIndex);
# if defined(USE_ANDN)
        longRef = _andn_u64(mask64, longRef);
# else
        longRef &= (~mask64);
# endif
#endif
        childIsFull = (longRef == 0);
    }
    return true;
}

bool MemoryManagerTreeArray::FreeSegmentId_NotThreadSafe(const segment_id_t segmentId) {
    return FreeSegmentId(segmentId);
}

bool MemoryManagerTreeArray::AllocateSegments_ThreadSafe(segment_id_chain_vec_t & segmentVec) { //number of segments should be the vector size
    boost::mutex::scoped_lock lock(m_mutex);
    const std::size_t size = segmentVec.size();
    for (std::size_t i = 0; i < size; ++i) {
        const segment_id_t segmentId = GetAndSetFirstFreeSegmentId_NotThreadSafe();
        if (segmentId != SEGMENT_ID_FULL) { //success
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


