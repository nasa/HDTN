/**
 * @file MemoryManagerTree.cpp
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
 */

#include "MemoryManagerTree.h"
#include <boost/multiprecision/cpp_int.hpp>
#include <boost/multiprecision/detail/bitscan.hpp>
#include <iostream>
#include <string>

//static boost::uint64_t g_numLeaves = 0;

void MemoryManagerTree::SetupTree(const int depth, void *node) {
	MemoryManagerInnerNode * const innerNode = (MemoryManagerInnerNode*)node;
	innerNode->m_bitMask = UINT64_MAX;
	if (depth > 1) { //inner node

		MemoryManagerInnerNode * const childInnerNodes = (MemoryManagerInnerNode*)malloc(64 * sizeof(MemoryManagerInnerNode));
		innerNode->m_childNodes = childInnerNodes;
		for (unsigned int i = 0; i < 64; ++i) {
			SetupTree(depth - 1, &childInnerNodes[i]);
		}
	}
	else { //depth == 1 so setup leaf node
		MemoryManagerLeafNode * const childLeafNodes = (MemoryManagerLeafNode*)malloc(64 * sizeof(MemoryManagerLeafNode));
		//g_numLeaves += 64;
		innerNode->m_childNodes = childLeafNodes;
		for (unsigned int i = 0; i < 64; ++i) {
			childLeafNodes[i].m_bitMask = UINT64_MAX;
		}
	}

}

void MemoryManagerTree::SetupTree() {
	SetupTree(MAX_TREE_DEPTH, &m_rootNode);
}

void MemoryManagerTree::FreeTree(const int depth, void *node) {
	MemoryManagerInnerNode * const innerNode = (MemoryManagerInnerNode*)node;
	if (depth > 1) { //inner node
		MemoryManagerInnerNode * const childInnerNodes = (MemoryManagerInnerNode*)innerNode->m_childNodes;
		for (unsigned int i = 0; i < 64; ++i) {
			FreeTree(depth - 1, &childInnerNodes[i]);
		}
		free(childInnerNodes);
	}
	else { //depth == 1 so free leaf node
		MemoryManagerLeafNode * const childLeafNodes = (MemoryManagerLeafNode*)innerNode->m_childNodes;
		//g_numLeaves -= 64;
		free(childLeafNodes);
	}

}

void MemoryManagerTree::FreeTree() {
	FreeTree(MAX_TREE_DEPTH, &m_rootNode);
}


void MemoryManagerTree::GetAndSetFirstFreeSegmentId(const int depth, void *node, boost::uint32_t * segmentId) {

	MemoryManagerInnerNode * const innerNode = (MemoryManagerInnerNode*)node;
	if (depth > 1) { // this is an inner node that has children that are also inner nodes
		if (innerNode->m_bitMask != 0) { //bitmask not zero means space available
			MemoryManagerInnerNode * const childInnerNodes = (MemoryManagerInnerNode*)innerNode->m_childNodes;
			const unsigned int firstFreeIndex = boost::multiprecision::detail::find_lsb<boost::uint64_t>(innerNode->m_bitMask);
			*segmentId += firstFreeIndex * (64 << ((depth - 1) * 6)); // 64^depth
			//std::cout << "d" << depth << " i" << firstFreeIndex << " ";
			MemoryManagerInnerNode * const targetChildInnerNode = &childInnerNodes[firstFreeIndex];
			GetAndSetFirstFreeSegmentId(depth - 1, targetChildInnerNode, segmentId);
			if (targetChildInnerNode->m_bitMask == 0) { //target child now full, so mark the parent's bit full (0)
				const boost::uint64_t mask64 = (((boost::uint64_t)1) << firstFreeIndex);
				innerNode->m_bitMask &= ~mask64;
			}
		}
	}
	else { //depth == 1 , this is an inner node that has children that are leaf nodes
		MemoryManagerLeafNode * const childLeafNodes = (MemoryManagerLeafNode*)innerNode->m_childNodes;
		const unsigned int firstFreeIndexInnerNode = boost::multiprecision::detail::find_lsb<boost::uint64_t>(innerNode->m_bitMask);
		*segmentId += firstFreeIndexInnerNode * 64; // 64^1 where 1 is depth
		//std::cout << "d" << depth << " i" << firstFreeIndexInnerNode << " ";
		MemoryManagerLeafNode * const targetChildLeafNode = &childLeafNodes[firstFreeIndexInnerNode];
		const unsigned int firstFreeIndexLeaf = boost::multiprecision::detail::find_lsb<boost::uint64_t>(targetChildLeafNode->m_bitMask);
		*segmentId += firstFreeIndexLeaf;
		//std::cout << "d0 i" << firstFreeIndexLeaf << " ";
		const boost::uint64_t mask64InnerNode = (((boost::uint64_t)1) << firstFreeIndexInnerNode);
		const boost::uint64_t mask64LeafNode = (((boost::uint64_t)1) << firstFreeIndexLeaf);
		targetChildLeafNode->m_bitMask &= ~mask64LeafNode;
		if (targetChildLeafNode->m_bitMask == 0) { //target child now full, so mark the parent's bit full (0)
			innerNode->m_bitMask &= ~mask64InnerNode;
		}
	}

}

boost::uint32_t MemoryManagerTree::GetAndSetFirstFreeSegmentId() {
	if (m_rootNode.m_bitMask == 0) return UINT32_MAX; //bitmask of zero means full
	boost::uint32_t segmentId = 0;
	GetAndSetFirstFreeSegmentId(MAX_TREE_DEPTH, &m_rootNode, &segmentId);
	return segmentId;
}


void MemoryManagerTree::FreeSegmentId(const int depth, void *node, boost::uint32_t segmentId, bool *success) {

	MemoryManagerInnerNode * const innerNode = (MemoryManagerInnerNode*)node;
	if (depth > 1) { // this is an inner node that has children that are also inner nodes

		const unsigned int index = (segmentId >> (depth * 6)) & 63;
		const boost::uint64_t mask64 = (((boost::uint64_t)1) << index);
		MemoryManagerInnerNode * const childInnerNodes = (MemoryManagerInnerNode*)innerNode->m_childNodes;
		MemoryManagerInnerNode * const targetChildInnerNode = &childInnerNodes[index];
		FreeSegmentId(depth - 1, targetChildInnerNode, segmentId, success);
		//target child is definitely not full, so mark the parent's bit not full (1)
		//0=full, 1=empty or partially filled (not full)
		innerNode->m_bitMask |= mask64; //set bit to 1 (not full)
	}
	else { //depth == 1 , this is an inner node that has children that are leaf nodes
		MemoryManagerLeafNode * const childLeafNodes = (MemoryManagerLeafNode*)innerNode->m_childNodes;
		const unsigned int indexInnerNode = (segmentId >> 6) & 63; //depth=1
		MemoryManagerLeafNode * const targetChildLeafNode = &childLeafNodes[indexInnerNode];
		const unsigned int indexLeafNode = segmentId & 63; //depth=0
		const boost::uint64_t mask64LeafNode = (((boost::uint64_t)1) << indexLeafNode);
		if (targetChildLeafNode->m_bitMask & mask64LeafNode) { //leaf bit is already 1 (empty)
			*success = false; //error
		}
		else { //leaf bit 0 so delete it by setting it to 1
			targetChildLeafNode->m_bitMask |= mask64LeafNode;
			//target child is now definitely not full, so mark the parent's bit not full (1)
			//0=full, 1=empty or partially filled (not full)
			const boost::uint64_t mask64InnerNode = (((boost::uint64_t)1) << indexInnerNode);
			innerNode->m_bitMask |= mask64InnerNode; //set bit to 1 (not full)

		}
	}

}

bool MemoryManagerTree::FreeSegmentId(boost::uint32_t segmentId) {
	bool success = true;
	FreeSegmentId(MAX_TREE_DEPTH, &m_rootNode, segmentId, &success);
	return success;
}


