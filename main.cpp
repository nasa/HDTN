#include <iostream>
#include <string>

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
				
		if (depth > 1) { //inner node
			InnerNode * const innerNode = (InnerNode*)node;
			InnerNode * const childInnerNodes = (InnerNode*) malloc(64 * sizeof(InnerNode));
			innerNode->m_childNodes = childInnerNodes;
			for (unsigned int i = 0; i < 64; ++i) {
				childInnerNodes[i].m_bitMask = 0;
				SetupTree(depth-1, &childInnerNodes[i]);
			}
		}
		else { //depth == 1 so setup leaf node
			InnerNode * const innerNode = (InnerNode*)node;
			LeafNode * const childLeafNodes = (LeafNode*)malloc(64 * sizeof(LeafNode));
			g_numLeaves += 64;
			innerNode->m_childNodes = childLeafNodes;
			for (unsigned int i = 0; i < 64; ++i) {
				childLeafNodes[i].m_bitMask = 0;
			}
		}
		
	}

	void SetupTree() {
		SetupTree(MAX_TREE_DEPTH, &m_rootNode);
	}

	void FreeTree(const int depth, void *node) {

		if (depth > 1) { //inner node

			InnerNode * const innerNode = (InnerNode*)node;
			InnerNode * const childInnerNodes = (InnerNode*)innerNode->m_childNodes;
			for (unsigned int i = 0; i < 64; ++i) {
				FreeTree(depth - 1, &childInnerNodes[i]);
			}
			free(childInnerNodes);
		}
		else { //depth == 1 so free leaf node
			InnerNode * const innerNode = (InnerNode*)node;
			LeafNode * const childLeafNodes = (LeafNode*)innerNode->m_childNodes;
			g_numLeaves -= 64;			
			free(childLeafNodes);
		}

	}

	void FreeTree() {
		FreeTree(MAX_TREE_DEPTH, &m_rootNode);
	}
	
	InnerNode m_rootNode;
};

int main() {
	boost::uint64_t n = 128;
	int a = boost::multiprecision::detail::find_lsb<boost::uint64_t>(n);
	std::cout << a << "\n";

	MemoryManagerTree t;
	std::cout << "ready\n";
	getchar();
	t.SetupTree();
	std::cout << "done " << g_numLeaves << "\n";
	getchar();
	t.FreeTree();
	std::cout << "done " << g_numLeaves << "\n";
	getchar();
	return 0;
}
