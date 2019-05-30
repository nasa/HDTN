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
		std::cout << i << " " << arraySize64s << "\n";
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


bool MemoryManagerTreeArray::UnitTest() {
	
	MemoryManagerTreeArray t;
	//std::cout << "ready\n";
	//getchar();
	//t.SetupTree();
	//std::cout << "done\n";
	//getchar();

	


	//unit tests
	boost::uint64_t prevRootBitmask = 5;
	//for (boost::uint32_t i = 0; i < 16777216 * 64; ++i) {
	for (boost::uint32_t i = 0; i < MAX_SEGMENTS; ++i) {
		const boost::uint32_t segmentId = t.GetAndSetFirstFreeSegmentId_NotThreadSafe();
		if (segmentId != i) {
			std::cout << "error " << segmentId << " " << i << "\n";
			return false;
		}
		if (prevRootBitmask != t.m_bitMasks[0][0]) {
			prevRootBitmask = t.m_bitMasks[0][0];
			printf("%d 0x%" PRIx64 "\n", segmentId, t.m_bitMasks[0][0]);
			//printf("0 0x%I64x\n", ((InnerNode*)t.m_rootNode.m_childNodes)[0].m_bitMask);
			//printf("1 0x%I64x\n", ((InnerNode*)t.m_rootNode.m_childNodes)[1].m_bitMask);
			//printf("2 0x%I64x\n", ((InnerNode*)t.m_rootNode.m_childNodes)[2].m_bitMask);
		}
		//std::cout << "sid:" << segmentId << "\n";
	}
	std::cout << "testing max\n";
	{
		const boost::uint32_t segmentId = t.GetAndSetFirstFreeSegmentId_NotThreadSafe();
		if (segmentId != UINT32_MAX) {
			std::cout << "error " << segmentId << "\n";
			printf("0x%" PRIx64 "\n", t.m_bitMasks[0][0]);
			return false;
		}
	}

	{
		const boost::uint32_t segmentIds[11] = {
		123,
		12345,
		16777 - 43,
		16777,
		16777 + 53,
		16777 + 1234,
		16777 * 2 + 5,
		16777 * 3 + 9,
		16777 * 5 + 2,
		16777 * 9 + 6,
		16777 * 12 + 8
		};

		for (int i = 0; i < 11; ++i) {
			const boost::uint32_t segmentId = segmentIds[i];
			if (t.FreeSegmentId_NotThreadSafe(segmentId)) {
				std::cout << "freed segId " << segmentId << "\n";
			}
			else {
				std::cout << "error FreeSegment\n";
				return false;
			}
		}
		for (int i = 0; i < 11; ++i) {
			const boost::uint32_t segmentId = segmentIds[i];
			const boost::uint32_t newSegmentId = t.GetAndSetFirstFreeSegmentId_NotThreadSafe();
			if (newSegmentId != segmentId) {
				std::cout << "error " << segmentId << " " << newSegmentId << "\n";
				return false;
			}
			else {
				std::cout << "reacquired segId " << newSegmentId << "\n";
			}

		}
	}
	{
		const boost::uint32_t segmentId = t.GetAndSetFirstFreeSegmentId_NotThreadSafe();
		if (segmentId != UINT32_MAX) {
			std::cout << "error " << segmentId << "\n";
			printf("0x%" PRIx64 "\n", t.m_bitMasks[0][0]);
			return false;
		}
	}
	
	//t.FreeTree();
	std::cout << "done\n";
	//getchar();
	return true;
}
