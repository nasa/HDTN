/**
 * @file MemoryManagerTree.h
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

#ifndef _MEMORY_MANAGER_TREE_H
#define _MEMORY_MANAGER_TREE_H 1

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
