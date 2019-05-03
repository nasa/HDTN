#include <iostream>
#include <string>
#include <stdint.h>

#include <boost/multiprecision/cpp_int.hpp>
#include <boost/multiprecision/detail/bitscan.hpp>

#define MAX_TREE_DEPTH 4
struct LeafNode {
	//LeafNode() : m_bitMask(0) {}

	boost::uint64_t m_bitMask;
};

struct InnerNode {
	

	boost::uint64_t m_bitMask;
	void * m_childNodes; //array of 64 child nodes or leafnodes
};
static boost::uint64_t g_numLeaves = 0;
struct MemoryManagerTree {
	
	void SetupTree(const int depth, void *node) {
		InnerNode * const innerNode = (InnerNode*)node;	
		innerNode->m_bitMask = UINT64_MAX;
		if (depth > 1) { //inner node
			
			InnerNode * const childInnerNodes = (InnerNode*) malloc(64 * sizeof(InnerNode));
			innerNode->m_childNodes = childInnerNodes;
			for (unsigned int i = 0; i < 64; ++i) {				
				SetupTree(depth-1, &childInnerNodes[i]);
			}
		}
		else { //depth == 1 so setup leaf node
			LeafNode * const childLeafNodes = (LeafNode*)malloc(64 * sizeof(LeafNode));
			g_numLeaves += 64;
			innerNode->m_childNodes = childLeafNodes;
			for (unsigned int i = 0; i < 64; ++i) {
				childLeafNodes[i].m_bitMask = UINT64_MAX;
			}
		}
		
	}

	void SetupTree() {
		SetupTree(MAX_TREE_DEPTH, &m_rootNode);
	}

	void FreeTree(const int depth, void *node) {
		InnerNode * const innerNode = (InnerNode*)node;
		if (depth > 1) { //inner node
			InnerNode * const childInnerNodes = (InnerNode*)innerNode->m_childNodes;
			for (unsigned int i = 0; i < 64; ++i) {
				FreeTree(depth - 1, &childInnerNodes[i]);
			}
			free(childInnerNodes);
		}
		else { //depth == 1 so free leaf node
			LeafNode * const childLeafNodes = (LeafNode*)innerNode->m_childNodes;
			g_numLeaves -= 64;			
			free(childLeafNodes);
		}

	}

	void FreeTree() {
		FreeTree(MAX_TREE_DEPTH, &m_rootNode);
	}

	
	void GetAndSetFirstFreeSegmentId(const int depth, void *node, boost::uint32_t * segmentId) {

		InnerNode * const innerNode = (InnerNode*)node;
		if (depth > 1) { // this is an inner node that has children that are also inner nodes		
			if (innerNode->m_bitMask != 0) { //bitmask not zero means space available
				InnerNode * const childInnerNodes = (InnerNode*)innerNode->m_childNodes;
				const unsigned int firstFreeIndex = boost::multiprecision::detail::find_lsb<boost::uint64_t>(innerNode->m_bitMask);
				*segmentId += firstFreeIndex * (64 << ((depth-1)*6)); // 64^depth
				//std::cout << "d" << depth << " i" << firstFreeIndex << " ";
				InnerNode * const targetChildInnerNode = &childInnerNodes[firstFreeIndex];
				GetAndSetFirstFreeSegmentId(depth-1, targetChildInnerNode, segmentId);
				if (targetChildInnerNode->m_bitMask == 0) { //target child now full, so mark the parent's bit full (0)
					const boost::uint64_t mask64 = (((boost::uint64_t)1) << firstFreeIndex);
					innerNode->m_bitMask &= ~mask64;
				}
			}	
		}
		else { //depth == 1 , this is an inner node that has children that are leaf nodes
			LeafNode * const childLeafNodes = (LeafNode*)innerNode->m_childNodes;
			const unsigned int firstFreeIndexInnerNode = boost::multiprecision::detail::find_lsb<boost::uint64_t>(innerNode->m_bitMask);
			*segmentId += firstFreeIndexInnerNode * 64; // 64^1 where 1 is depth
			//std::cout << "d" << depth << " i" << firstFreeIndexInnerNode << " ";
			LeafNode * const targetChildLeafNode = &childLeafNodes[firstFreeIndexInnerNode];
			const unsigned int firstFreeIndexLeaf = boost::multiprecision::detail::find_lsb<boost::uint64_t>(targetChildLeafNode->m_bitMask);
			*segmentId += firstFreeIndexLeaf;
			//std::cout << "d0 i" << firstFreeIndexLeaf << " ";
			const boost::uint64_t mask64InnerNode = ( ((boost::uint64_t)1) << firstFreeIndexInnerNode);
			const boost::uint64_t mask64LeafNode = (((boost::uint64_t)1) << firstFreeIndexLeaf);
			targetChildLeafNode->m_bitMask &= ~mask64LeafNode;
			if (targetChildLeafNode->m_bitMask == 0) { //target child now full, so mark the parent's bit full (0)
				innerNode->m_bitMask &= ~mask64InnerNode;
			}
		}

	}

	boost::uint32_t GetAndSetFirstFreeSegmentId() {
		if (m_rootNode.m_bitMask == 0) return UINT32_MAX; //bitmask of zero means full
		boost::uint32_t segmentId = 0;
		GetAndSetFirstFreeSegmentId(MAX_TREE_DEPTH, &m_rootNode, &segmentId);
		return segmentId;
	}

	
	void FreeSegmentId(const int depth, void *node, boost::uint32_t segmentId, bool *success) {

		InnerNode * const innerNode = (InnerNode*)node;
		if (depth > 1) { // this is an inner node that has children that are also inner nodes		
			
			const unsigned int index = (segmentId >> (depth * 6)) & 63;
			const boost::uint64_t mask64 = (((boost::uint64_t)1) << index);			
			InnerNode * const childInnerNodes = (InnerNode*)innerNode->m_childNodes;				
			InnerNode * const targetChildInnerNode = &childInnerNodes[index];
			FreeSegmentId(depth - 1, targetChildInnerNode, segmentId, success);	
			//target child is definitely not full, so mark the parent's bit not full (1)
			//0=full, 1=empty or partially filled (not full)
			innerNode->m_bitMask |= mask64; //set bit to 1 (not full)
		}
		else { //depth == 1 , this is an inner node that has children that are leaf nodes
			LeafNode * const childLeafNodes = (LeafNode*)innerNode->m_childNodes;
			const unsigned int indexInnerNode = (segmentId >> 6) & 63; //depth=1
			LeafNode * const targetChildLeafNode = &childLeafNodes[indexInnerNode];
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

	bool FreeSegmentId(boost::uint32_t segmentId) {
		bool success = true;
		FreeSegmentId(MAX_TREE_DEPTH, &m_rootNode, segmentId, &success);
		return success;
	}
	
	InnerNode m_rootNode;
};

int main() {
	boost::uint64_t n = 128;
	int a = boost::multiprecision::detail::find_lsb<boost::uint64_t>(n);
	std::cout << a << "\n";

	MemoryManagerTree t;
	std::cout << "ready\n";
	//getchar();
	t.SetupTree();
	std::cout << "done " << g_numLeaves << "\n";
	//getchar();

	/*
	{
		const boost::uint32_t segmentId = 66;// 16777217;
		for (int d = 4; d > 0; --d) {			
			const unsigned int index = (segmentId >> ((d) * 6)) & 63;
			std::cout << d << " " << index << "\n";
		}
		{
			const unsigned int index = segmentId & 63;
			std::cout << "0" << " " << index << "\n";
		}
	}	
	return 0;
	*/


	//unit tests
	boost::uint64_t prevRootBitmask = 5;
	for (boost::uint32_t i = 0; i < 16777216*64; ++i) {
		const boost::uint32_t segmentId = t.GetAndSetFirstFreeSegmentId();
		if (segmentId != i) {
			std::cout << "error " << segmentId << " " << i << "\n";
			break;
		}
		if (prevRootBitmask != t.m_rootNode.m_bitMask) {
			prevRootBitmask = t.m_rootNode.m_bitMask;
			printf("%d 0x%I64x\n", segmentId, t.m_rootNode.m_bitMask);
			//printf("0 0x%I64x\n", ((InnerNode*)t.m_rootNode.m_childNodes)[0].m_bitMask);
			//printf("1 0x%I64x\n", ((InnerNode*)t.m_rootNode.m_childNodes)[1].m_bitMask);
			//printf("2 0x%I64x\n", ((InnerNode*)t.m_rootNode.m_childNodes)[2].m_bitMask);
		}
		//std::cout << "sid:" << segmentId << "\n";
	}
	{
		const boost::uint32_t segmentId = t.GetAndSetFirstFreeSegmentId();
		if (segmentId != UINT32_MAX) {
			std::cout << "error " << segmentId <<  "\n";
			printf("0x%I64x\n", t.m_rootNode.m_bitMask);
		}
	}

	{
		const boost::uint32_t segmentIds[11] = {
			123,
			12345,
			16777216 - 43,
			16777216,			
			16777216 + 53,
			16777216 + 1234567,			
			16777216 * 2 + 5,
			16777216 * 3 + 9,
			16777216 * 5 + 2,
			16777216 * 9 + 6,
			16777216 * 12 + 8
		};

		for (int i = 0; i < 11; ++i) {
			const boost::uint32_t segmentId = segmentIds[i];
			if (t.FreeSegmentId(segmentId)) {
				std::cout << "freed segId " << segmentId << "\n";				
			}
		}
		for (int i = 0; i < 11; ++i) {
			const boost::uint32_t segmentId = segmentIds[i];			
			const boost::uint32_t newSegmentId = t.GetAndSetFirstFreeSegmentId();
			if (newSegmentId != segmentId) {
				std::cout << "error " << segmentId << " " << newSegmentId << "\n";
			}
			else {
				std::cout << "reacquired segId " << newSegmentId << "\n";
			}
			
		}
	}
	{
		const boost::uint32_t segmentId = t.GetAndSetFirstFreeSegmentId();
		if (segmentId != UINT32_MAX) {
			std::cout << "error " << segmentId << "\n";
			printf("0x%I64x\n", t.m_rootNode.m_bitMask);
		}
	}
	t.FreeTree();
	std::cout << "done " << g_numLeaves << "\n";
	//getchar();
	return 0;
}
