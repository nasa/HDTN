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

	//return false if full, true if space was available
	void GetAndSetFirstFreeSegmentId(const int depth, void *node, boost::uint32_t * segmentId) {

		InnerNode * const innerNode = (InnerNode*)node;
		if (depth > 1) { //inner node			
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

				//return true;
			}
			//else return false; //full.. bitmask of zero means full
			
		}
		else { //depth == 1 so check leaf node
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

			//return true;
		}

	}

	boost::uint32_t GetAndSetFirstFreeSegmentId() {
		boost::uint32_t segmentId = 0;
		GetAndSetFirstFreeSegmentId(MAX_TREE_DEPTH, &m_rootNode, &segmentId);
		return segmentId;
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
	for (int i = 0; i < 16777216; ++i) {
		boost::uint32_t segmentId = t.GetAndSetFirstFreeSegmentId();
		if (segmentId != i) {
			std::cout << "error " << segmentId << " " << i << "\n";
			break;
		}
		//std::cout << "sid:" << segmentId << "\n";
	}
	t.FreeTree();
	std::cout << "done " << g_numLeaves << "\n";
	//getchar();
	return 0;
}
