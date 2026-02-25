/**
 * @file MemoryManagerTree.h
 * @author  Brian Tomko <brian.j.tomko@nasa.gov>
 *
 * @copyright Copyright (c) 2026 United States Government as represented by
 * the National Aeronautics and Space Administration.
 * No copyright is claimed in the United States under Title 17, U.S.Code.
 * All Other Rights Reserved.
 *
 * @section LICENSE
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
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
