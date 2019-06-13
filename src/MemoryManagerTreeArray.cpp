#include "MemoryManagerTreeArray.h"
#include <boost/multiprecision/cpp_int.hpp>
#include <boost/multiprecision/detail/bitscan.hpp>
#include <iostream>
#include <string>
#include <inttypes.h>

//static boost::uint64_t g_numLeaves = 0;

MemoryManagerTreeArray::MemoryManagerTreeArray() {
	SetupTree();
}
MemoryManagerTreeArray::~MemoryManagerTreeArray() {
	FreeTree();
}

void MemoryManagerTreeArray::SetupTree() {
	for (unsigned int i = 0; i < MAX_TREE_ARRAY_DEPTH; ++i) {
		const boost::uint64_t arraySize64s = (((boost::uint64_t)1) << (i*6));
		//std::cout << i << " " << arraySize64s << "\n";
		m_bitMasks[i] = (boost::uint64_t*)malloc(arraySize64s * sizeof(boost::uint64_t));
		boost::uint64_t * const currentArrayPtr = m_bitMasks[i];
		for (boost::uint64_t j = 0; j < arraySize64s; ++j) {
			currentArrayPtr[j] = UINT64_MAX;
		}
	}
	

}


void MemoryManagerTreeArray::FreeTree() {
	for (unsigned int i = 0; i < MAX_TREE_ARRAY_DEPTH; ++i) {
		free(m_bitMasks[i]);
	}
}


bool MemoryManagerTreeArray::GetAndSetFirstFreeSegmentId(const boost::uint32_t depthIndex, const boost::uint32_t rowIndex, boost::uint32_t * segmentId) {

	boost::uint64_t * const currentArrayPtr = m_bitMasks[depthIndex];
	boost::uint64_t * const currentBit64Ptr = &currentArrayPtr[rowIndex];
	const unsigned int firstFreeIndex = boost::multiprecision::detail::find_lsb<boost::uint64_t>(*currentBit64Ptr);
	const boost::uint64_t mask64 = (((boost::uint64_t)1) << firstFreeIndex);	
	*segmentId += firstFreeIndex * (1 << (((MAX_TREE_ARRAY_DEPTH - 1) - depthIndex) * 6)); // 64^depth
	
	
	if ((depthIndex == MAX_TREE_ARRAY_DEPTH - 1) || GetAndSetFirstFreeSegmentId(depthIndex + 1, rowIndex + firstFreeIndex * (1 << (depthIndex * 6)), segmentId)) {
		if (*segmentId < MAX_SEGMENTS) *currentBit64Ptr &= ~mask64;
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
	boost::uint32_t segmentId = 0;
	GetAndSetFirstFreeSegmentId(0, 0, &segmentId);
	if (segmentId >= MAX_SEGMENTS) return UINT32_MAX;
	return segmentId;
}

bool MemoryManagerTreeArray::IsSegmentFree(const boost::uint32_t depthIndex, const boost::uint32_t rowIndex, boost::uint32_t segmentId) {

	const boost::uint64_t * const currentArrayPtr = m_bitMasks[depthIndex];
	const boost::uint64_t * const currentBit64Ptr = &currentArrayPtr[rowIndex];
	const unsigned int index = (segmentId >> (((MAX_TREE_ARRAY_DEPTH - 1) - depthIndex) * 6)) & 63;
	const boost::uint64_t mask64 = (((boost::uint64_t)1) << index);

	if (depthIndex == MAX_TREE_ARRAY_DEPTH - 1) { //leaf
		return (*currentBit64Ptr & mask64); //leaf bit is 1 (empty)			
	}
	else { //inner node
		return IsSegmentFree(depthIndex + 1, rowIndex + index * (1 << (depthIndex * 6)), segmentId);
	}
}

bool MemoryManagerTreeArray::IsSegmentFree(segment_id_t segmentId) {
	return IsSegmentFree(0, 0, segmentId);
}

void MemoryManagerTreeArray::FreeSegmentId(const boost::uint32_t depthIndex, const boost::uint32_t rowIndex, boost::uint32_t segmentId, bool *success) {

	boost::uint64_t * const currentArrayPtr = m_bitMasks[depthIndex];
	boost::uint64_t * const currentBit64Ptr = &currentArrayPtr[rowIndex];
	const unsigned int index = (segmentId >> (((MAX_TREE_ARRAY_DEPTH - 1) - depthIndex) * 6)) & 63;
	const boost::uint64_t mask64 = (((boost::uint64_t)1) << index);

	if (depthIndex == MAX_TREE_ARRAY_DEPTH - 1) { //leaf
		if (*currentBit64Ptr & mask64) { //leaf bit is already 1 (empty)
			*success = false; //error
		}
	}
	else { //inner node
		FreeSegmentId(depthIndex + 1, rowIndex + index * (1 << (depthIndex * 6)), segmentId, success);
	}
	*currentBit64Ptr |= mask64;
	

}

bool MemoryManagerTreeArray::AllocateSegmentId_NoCheck(const boost::uint32_t depthIndex, const boost::uint32_t rowIndex, boost::uint32_t segmentId) {

	boost::uint64_t * const currentArrayPtr = m_bitMasks[depthIndex];
	boost::uint64_t * const currentBit64Ptr = &currentArrayPtr[rowIndex];
	const unsigned int index = (segmentId >> (((MAX_TREE_ARRAY_DEPTH - 1) - depthIndex) * 6)) & 63;
	const boost::uint64_t mask64 = (((boost::uint64_t)1) << index);


	if ((depthIndex == MAX_TREE_ARRAY_DEPTH - 1) || AllocateSegmentId_NoCheck(depthIndex + 1, rowIndex + index * (1 << (depthIndex * 6)), segmentId)) {
		if (segmentId < MAX_SEGMENTS) *currentBit64Ptr &= ~mask64;
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

bool MemoryManagerTreeArray::FreeSegments_ThreadSafe(segment_id_chain_vec_t & segmentVec) {
	boost::mutex::scoped_lock lock(m_mutex);
	const std::size_t size = segmentVec.size();
	bool success = true;
	for (std::size_t i = 0; i < size; ++i) {
		if (!FreeSegmentId_NotThreadSafe(segmentVec[i])) {
			success = false;
		}
	}
	segmentVec.resize(0);
	return success;
}


