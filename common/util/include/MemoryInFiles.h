/**
 * @file MemoryInFiles.h
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
 *
 * @section DESCRIPTION
 *
 * This MemoryInFiles class is used for asynchronous reads of memory from storage
 * and writes of memory to storage.  The intention is to allow LTP to use this as
 * a storage mechanism for long delays and high rates which would require too
 * much RAM otherwise.  It is single threaded and must be run by the thread running ioServiceRef.
 */

#ifndef _MEMORY_IN_FILES_H
#define _MEMORY_IN_FILES_H 1

#include <boost/filesystem.hpp>
#include <boost/asio.hpp>
#include <boost/function.hpp>
#include <cstdint>
#include <memory>
#include <boost/core/noncopyable.hpp>
#include "hdtn_util_export.h"

class MemoryInFiles : private boost::noncopyable {
private:
    MemoryInFiles() = delete;
public:
    typedef boost::function<void()> write_memory_handler_t;
    typedef boost::function<void(bool success)> read_memory_handler_t;

    struct deferred_read_t {
        deferred_read_t() : memoryBlockId(0) {}
        uint64_t memoryBlockId;
        uint64_t offset;
        std::size_t length;
        void* readToThisLocationPtr;
    };
    struct deferred_write_t {
        uint64_t memoryBlockId;
        uint64_t offset;
        std::size_t length;
        const void* writeFromThisLocationPtr;
    };
    
    HDTN_UTIL_EXPORT MemoryInFiles(boost::asio::io_service & ioServiceRef,
        const boost::filesystem::path & rootStorageDirectory, const uint64_t newFileAggregationTimeMs);
    HDTN_UTIL_EXPORT ~MemoryInFiles();

    HDTN_UTIL_EXPORT uint64_t AllocateNewWriteMemoryBlock(std::size_t totalSize);
    HDTN_UTIL_EXPORT bool DeleteMemoryBlock(const uint64_t memoryBlockId);
    //returns memoryBlockId (key) assigned to the written data, use that key to read it back
    HDTN_UTIL_EXPORT bool WriteMemoryAsync(const deferred_write_t& deferredWrite, const write_memory_handler_t& handler);
    HDTN_UTIL_EXPORT bool WriteMemoryAsync(const deferred_write_t& deferredWrite, write_memory_handler_t&& handler);
    HDTN_UTIL_EXPORT bool WriteMemoryAsync(const std::vector<deferred_write_t>& deferredWritesVec, const write_memory_handler_t& handler);
    HDTN_UTIL_EXPORT bool WriteMemoryAsync(const std::vector<deferred_write_t>& deferredWritesVec, write_memory_handler_t&& handler);

    HDTN_UTIL_EXPORT bool ReadMemoryAsync(deferred_read_t& deferredRead, const read_memory_handler_t& handler);
    HDTN_UTIL_EXPORT bool ReadMemoryAsync(deferred_read_t& deferredRead, read_memory_handler_t&& handler);
    HDTN_UTIL_EXPORT bool ReadMemoryAsync(std::vector<deferred_read_t>& deferredReadsVec, const read_memory_handler_t& handler);
    HDTN_UTIL_EXPORT bool ReadMemoryAsync(std::vector<deferred_read_t>& deferredReadsVec, read_memory_handler_t&& handler);

    HDTN_UTIL_EXPORT uint64_t GetCountTotalFilesCreated() const;
    HDTN_UTIL_EXPORT uint64_t GetCountTotalFilesActive() const;
    
private:
    // Internal implementation class
    struct Impl;
    // Pointer to the internal implementation
    std::unique_ptr<Impl> m_pimpl;
};

#endif //_MEMORY_IN_FILES_H
