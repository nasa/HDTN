#ifndef _MEMORY_MANAGER_TREE_H
#define _MEMORY_MANAGER_TREE_H

#include <boost/integer.hpp>
#include <stdint.h>
#include "BundleStorageConfig.h"


struct MemoryManagerLeafNode {
	boost::uint64_t m_bitMask;
};

struct MemoryManagerInnerNode {
	boost::uint64_t m_bitMask;
	void * m_childNodes; //array of 64 child nodes or leafnodes
};

class MemoryManagerTree {
public:
	void SetupTree();
	void FreeTree();
	boost::uint32_t GetAndSetFirstFreeSegmentId();
	bool FreeSegmentId(boost::uint32_t segmentId);
	static bool UnitTest();

private:
	void SetupTree(const int depth, void *node);
	void FreeTree(const int depth, void *node);
	void GetAndSetFirstFreeSegmentId(const int depth, void *node, boost::uint32_t * segmentId);
	void FreeSegmentId(const int depth, void *node, boost::uint32_t segmentId, bool *success);

private:

	MemoryManagerInnerNode m_rootNode;
};


#endif //_MEMORY_MANAGER_TREE_H
