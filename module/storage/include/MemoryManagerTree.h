#ifndef _MEMORY_MANAGER_TREE_H
#define _MEMORY_MANAGER_TREE_H

#include <boost/integer.hpp>
#include <stdint.h>
#include "BundleStorageConfig.h"
#include "storage_lib_export.h"

struct MemoryManagerLeafNode {
    uint64_t m_bitMask;
};

struct MemoryManagerInnerNode {
    uint64_t m_bitMask;
    void * m_childNodes; //array of 64 child nodes or leafnodes
};

class MemoryManagerTree {
public:
    STORAGE_LIB_EXPORT void SetupTree();
    STORAGE_LIB_EXPORT void FreeTree();
    STORAGE_LIB_EXPORT boost::uint32_t GetAndSetFirstFreeSegmentId();
    STORAGE_LIB_EXPORT bool FreeSegmentId(boost::uint32_t segmentId);

private:
    STORAGE_LIB_NO_EXPORT void SetupTree(const int depth, void *node);
    STORAGE_LIB_NO_EXPORT void FreeTree(const int depth, void *node);
    STORAGE_LIB_NO_EXPORT void GetAndSetFirstFreeSegmentId(const int depth, void *node, boost::uint32_t * segmentId);
    STORAGE_LIB_NO_EXPORT void FreeSegmentId(const int depth, void *node, boost::uint32_t segmentId, bool *success);

private:

    MemoryManagerInnerNode m_rootNode;
};


#endif //_MEMORY_MANAGER_TREE_H
