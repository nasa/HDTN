/**
 * @file BundleStorageConfig.h
 * @author  Brian Tomko <brian.j.tomko@nasa.gov>
 *
 * @copyright Copyright (c) 2021 United States Government as represented by
 * the National Aeronautics and Space Administration.
 * No copyright is claimed in the United States under Title 17, U.S.Code.
 * All Other Rights Reserved.
 *
 * @section LICENSE
 * Released under the NASA Open Source Agreement (NOSA)
 * See LICENSE.md in the source root directory for more information.
 *
 * @section DESCRIPTION
 *
 * This file sets up all the compile time configuration settings for the storage module.
 */

#ifndef _BUNDLE_STORAGE_CONFIG_H
#define _BUNDLE_STORAGE_CONFIG_H 1

#include <cstdint>

///////////////////////////
//SEGMENT ALLOCATOR
///////////////////////////
//#define MAX_TREE_DEPTH 4 //obsolete for old segment allocator

//#define MAX_TREE_ARRAY_DEPTH 5
//#define MAX_MEMORY_MANAGER_SEGMENTS (1073741824 - 1) //64^5 = 1,073,741,824 (update this if you change MAX_TREE_ARRAY_DEPTH)
//(segment allocator using MAX_MEMORY_MANAGER_SEGMENTS uses about 130 MByte RAM)

#define MAX_TREE_ARRAY_DEPTH 6

#if !defined(STORAGE_SEGMENT_ID_SIZE_BITS)
#error "STORAGE_SEGMENT_ID_SIZE_BITS is not defined"
#elif (STORAGE_SEGMENT_ID_SIZE_BITS == 32)
typedef uint32_t segment_id_t;
#define SEGMENT_ID_FULL UINT32_MAX
#define MAX_MEMORY_MANAGER_SEGMENTS (UINT32_MAX - 3) //min(UINT32_MAX, 64^6) since segment_id_t is a uint32_t (update this if you change MAX_TREE_ARRAY_DEPTH)
//(segment allocator using MAX_MEMORY_MANAGER_SEGMENTS uses about 533 MByte RAM), and multiplying by 4KB gives ~17TB bundle storage capacity
#elif (STORAGE_SEGMENT_ID_SIZE_BITS == 64)
typedef uint64_t segment_id_t;
#define SEGMENT_ID_FULL UINT64_MAX
#define MAX_MEMORY_MANAGER_SEGMENTS (UINT32_MAX - 3) //min(UINT64_MAX, 64^6) = 68,719,476,736 since segment_id_t is a uint64_t (update this if you change MAX_TREE_ARRAY_DEPTH)
//multiplying by 4KB gives ~281TB
#else
#error "STORAGE_SEGMENT_ID_SIZE_BITS is defined but not set to 32 or 64"
#endif
#define SEGMENT_ID_LAST SEGMENT_ID_FULL






///////////////////////////
//BUNDLE STORAGE MANAGER
///////////////////////////
#if !defined(STORAGE_SEGMENT_SIZE_MULTIPLE_OF_4KB)
#error "STORAGE_SEGMENT_SIZE_MULTIPLE_OF_4KB is not defined"
#elif (STORAGE_SEGMENT_SIZE_MULTIPLE_OF_4KB == 0)
#error "STORAGE_SEGMENT_SIZE_MULTIPLE_OF_4KB must be an integer of at least 1"
#endif
#define SEGMENT_SIZE (4096 * STORAGE_SEGMENT_SIZE_MULTIPLE_OF_4KB)  
#define SEGMENT_RESERVED_SPACE (sizeof(uint64_t) + sizeof(uint64_t) + sizeof(segment_id_t) + sizeof(uint64_t))
#define BUNDLE_STORAGE_PER_SEGMENT_SIZE (SEGMENT_SIZE - SEGMENT_RESERVED_SPACE)
#define READ_CACHE_NUM_SEGMENTS_PER_SESSION 50

#ifdef _MSC_VER //Windows tests
//#define FILE_SIZE (1024000000ULL * 1) //1 GByte total of files, or file_size / num_threads size per file
////#define FILE_SIZE (1024000000ULL * 8) //8 GByte total of files, or file_size / num_threads size per file
#define NUM_SEGMENTS_PER_TEST 100000
#else //Lambda Linux tests
//#define FILE_SIZE (10240000000ULL * 64) //640 GByte files
////#define FILE_SIZE (10240000000ULL * 300) //3 TByte files
////#define NUM_SEGMENTS_PER_TEST 10000000
#define FILE_SIZE (1024000000ULL * 1) //1 GByte total of files, or file_size / num_threads size per file
//#define FILE_SIZE (1024000000ULL * 8) //8 GByte total of files, or file_size / num_threads size per file
#define NUM_SEGMENTS_PER_TEST 100000
#endif

////#define MAX_SEGMENTS (FILE_SIZE/SEGMENT_SIZE)

////#if MAX_SEGMENTS > MAX_MEMORY_MANAGER_SEGMENTS
////#error "MAX SEGMENTS GREATER THAN WHAT MEMORY MANAGER CAN HANDLE"
////#endif


#define NUMBER_OF_PRIORITIES (3)
//#define USE_MEMORY_MAPPED_FILES 1
////#define NUM_STORAGE_THREADS 4
#define MAX_NUM_STORAGE_THREADS 10

///////////////////////////
//CIRCULAR INDEX BUFFER
///////////////////////////
#define CIRCULAR_INDEX_BUFFER_SIZE 30

#endif //_BUNDLE_STORAGE_CONFIG_H
