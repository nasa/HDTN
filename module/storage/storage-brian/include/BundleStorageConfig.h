#ifndef _BUNDLE_STORAGE_CONFIG_H
#define _BUNDLE_STORAGE_CONFIG_H

#include <boost/integer.hpp>
#include <stdint.h>

///////////////////////////
//SEGMENT ALLOCATOR
///////////////////////////
#define MAX_TREE_DEPTH 4 //obsolete for old segment allocator

#define MAX_TREE_ARRAY_DEPTH 5
#define MAX_MEMORY_MANAGER_SEGMENTS 1073741824 //64^5 = 1,073,741,824 (update this if you change MAX_TREE_ARRAY_DEPTH)

///////////////////////////
//BUNDLE STORAGE MANAGER
///////////////////////////

#define SEGMENT_SIZE 4096  
#define SEGMENT_RESERVED_SPACE (sizeof(boost::uint64_t) + sizeof(boost::uint32_t))
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

//two days
#define NUMBER_OF_EXPIRATIONS (86400*2)
#define NUMBER_OF_PRIORITIES (3)
//#define USE_MEMORY_MAPPED_FILES 1
////#define NUM_STORAGE_THREADS 4
#define MAX_NUM_STORAGE_THREADS 10

///////////////////////////
//CIRCULAR INDEX BUFFER
///////////////////////////
#define CIRCULAR_INDEX_BUFFER_SIZE 30

#endif //_BUNDLE_STORAGE_CONFIG_H
