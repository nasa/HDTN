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
    /**
     * @typedef write_memory_handler_t Type of I/O write operation completion handler.
     */
    typedef boost::function<void()> write_memory_handler_t;
    
    /**
     * @typedef read_memory_handler_t Type of I/O read operation completion handler.
     */
    typedef boost::function<void(bool success)> read_memory_handler_t;

    /// Deferred read context data
    struct deferred_read_t {
        /// Set target memory block ID to 0, an ID of 0 is unreachable by normal blocks and reserved for failure reporting
        deferred_read_t() : memoryBlockId(0) {}
        /// Set target memory block ID to 0
        void Reset() {
            memoryBlockId = 0;
        }
        /// Target memory block ID
        uint64_t memoryBlockId;
        /// Target memory block offset
        uint64_t offset;
        /// Output length in bytes
        uint64_t length;
        /// Type-erased output begin write pointer
        void* readToThisLocationPtr;
    };
    
    /// Deferred write context data
    struct deferred_write_t {
        /// Target memory block ID
        uint64_t memoryBlockId;
        /// Target memory block offset
        uint64_t offset;
        /// Input length in bytes
        uint64_t length;
        /// Type-erased input begin read pointer
        const void* writeFromThisLocationPtr;
    };
    
    /**
     * Initialize PIMPL.
     * @param ioServiceRef I/O execution context reference.
     * @param rootStorageDirectory The filesystem root directory, a randomly generated directory will be created inside the root that will be the working directory for this instance.
     * @param newFileAggregationTimeMs The time window in milliseconds for which write allocations will be redirected to the currently active write file.
     * @param estimatedMaxAllocatedBlocks The number of memory blocks to reverse space for, this is a soft cap to lessen instances of reallocation, the actual space will be expanded if needed.
     */
    HDTN_UTIL_EXPORT MemoryInFiles(boost::asio::io_service & ioServiceRef,
        const boost::filesystem::path & rootStorageDirectory, const uint64_t newFileAggregationTimeMs,
        const uint64_t estimatedMaxAllocatedBlocks);
    
    /// Default destructor
    HDTN_UTIL_EXPORT ~MemoryInFiles();

    /** Allocate new memory block of at least given size.
     *
     * Tries to insert memory block metadata for the memory block, if succeeds then calls MemoryBlockInfo::Resize() to perform the allocation.
     * @param totalSize The minimum number of bytes to fit
     * @return The memory block ID to the newly created memory block on successful allocation, or 0 if the allocation failed.
     */
    HDTN_UTIL_EXPORT uint64_t AllocateNewWriteMemoryBlock(uint64_t totalSize);
    
    /** Get the size of the given memory block.
     *
     * If the given memory block ID does not reference a valid memory block, returns immediately with a value of 0.
     * Else, returns the allocation size in bytes of the memory block.
     * @param memoryBlockId The memory block ID.
     * @return The allocation size of the memory block on successful retrieval, or 0 if retrieval failed.
     */
    HDTN_UTIL_EXPORT uint64_t GetSizeOfMemoryBlock(const uint64_t memoryBlockId) const noexcept;
    
    /** Resize the given memory block.
     *
     * If the given memory block ID does not reference a valid memory block, returns immediately with a value of 0.
     * Else, calls MemoryBlockInfo::Resize() to perform the resize.
     * @param memoryBlockId The memory block ID.
     * @param newSize The minimum number of bytes to fit.
     * @return The total size in bytes of the newly resized allocation on successful resize, or 0 if the resize failed.
     */
    HDTN_UTIL_EXPORT uint64_t Resize(const uint64_t memoryBlockId, uint64_t newSize);
    
    /** Delete the given memory block.
     *
     * If the given memory block ID does not reference a valid memory block, returns immediately.
     *
     * if the memory block has pending I/O operations queued, then if not already marked, marks the block as due deletion (deferred for after the I/O
     * operations have been completed, deferred deletion is eventually handled by Impl::ForceDeleteMemoryBlock()).
     * Else, deletes the memory block.
     * @param memoryBlockId The memory block ID.
     * @return True if the memory block was JUST NOW marked as due deletion or was JUST NOW deleted, or False otherwise.
     */
    HDTN_UTIL_EXPORT bool DeleteMemoryBlock(const uint64_t memoryBlockId);
    
    /** Initiate a request to delete the given memory block.
     *
     * Initiates an asynchronous request to Impl::HandleAsyncDeleteMemoryBlock() to delete the memory block.
     * @param memoryBlockId The memory block ID.
     */
    HDTN_UTIL_EXPORT void AsyncDeleteMemoryBlock(const uint64_t memoryBlockId);
    
    //returns memoryBlockId (key) assigned to the written data, use that key to read it back
    
    /** Initiate deferred single-write operation.
     *
     * If the given memory block ID does not reference a valid memory block, returns immediately.
     * Else, calls MemoryBlockInfo::DoWriteOrRead() to initiate the deferred write operation.
     * @param deferredWrite The deferred write context data.
     * @param handlerPtr The completion handler pointer.
     * @return True if the write operation was queued successfully, or False otherwise.
     */
    HDTN_UTIL_EXPORT bool WriteMemoryAsync(const deferred_write_t& deferredWrite, const write_memory_handler_t& handler);
    
    /** Initiate deferred single-write operation.
     *
     * If the given memory block ID does not reference a valid memory block, returns immediately.
     * Else, calls MemoryBlockInfo::DoWriteOrRead() to initiate the deferred write operation.
     * @param deferredWrite The deferred write context data.
     * @param handlerPtr The completion handler pointer.
     * @return True if the write operation was queued successfully, or False otherwise.
     * @post The argument to handler is left in a moved-from state.
     */
    HDTN_UTIL_EXPORT bool WriteMemoryAsync(const deferred_write_t& deferredWrite, write_memory_handler_t&& handler);
    
    /** Initiate deferred multi-write operation.
     *
     * Calls Impl::WriteMemoryAsync() for every operation in the vector.
     * @param deferredWritesVec The deferred write context data vector.
     * @param handlerPtr The completion handler pointer.
     * @return True if ALL the write operations in the vector could be queued successfully, or False otherwise.
     */
    HDTN_UTIL_EXPORT bool WriteMemoryAsync(const std::vector<deferred_write_t>& deferredWritesVec, const write_memory_handler_t& handler);
    
    /** Initiate deferred multi-write operation.
     *
     * Calls Impl::WriteMemoryAsync() for every operation in the vector.
     * @param deferredWritesVec The deferred write context data vector.
     * @param handlerPtr The completion handler pointer.
     * @return True if ALL the write operations in the vector could be queued successfully, or False otherwise.
     * @post The argument to handler is left in a moved-from state.
     */
    HDTN_UTIL_EXPORT bool WriteMemoryAsync(const std::vector<deferred_write_t>& deferredWritesVec, write_memory_handler_t&& handler);

    /** Initiate deferred single-read operation.
     *
     * If the given memory block ID does not reference a valid memory block, returns immediately.
     * Else, calls MemoryBlockInfo::DoWriteOrRead() to initiate the deferred read operation.
     * @param deferredRead The deferred read context data.
     * @param handlerPtr The completion handler pointer.
     * @return True if the read operation was queued successfully, or False otherwise.
     */
    HDTN_UTIL_EXPORT bool ReadMemoryAsync(const deferred_read_t& deferredRead, const read_memory_handler_t& handler);
    
    /** Initiate deferred single-read operation.
     *
     * If the given memory block ID does not reference a valid memory block, returns immediately.
     * Else, calls MemoryBlockInfo::DoWriteOrRead() to initiate the deferred read operation.
     * @param deferredRead The deferred read context data.
     * @param handlerPtr The completion handler pointer.
     * @return True if the read operation was queued successfully, or False otherwise.
     * @post The argument to handler is left in a moved-from state.
     */
    HDTN_UTIL_EXPORT bool ReadMemoryAsync(const deferred_read_t& deferredRead, read_memory_handler_t&& handler);
    
    /** Initiate deferred multi-read operation.
     *
     * Calls Impl::ReadMemoryAsync() for every operation in the vector.
     * @param deferredReadsVec The deferred read context data vector.
     * @param handlerPtr The completion handler pointer.
     * @return True if ALL the read operations in the vector could be queued successfully, or False otherwise.
     */
    HDTN_UTIL_EXPORT bool ReadMemoryAsync(const std::vector<deferred_read_t>& deferredReadsVec, const read_memory_handler_t& handler);
    
    /** Initiate deferred multi-read operation.
     *
     * Calls Impl::ReadMemoryAsync() for every operation in the vector.
     * @param deferredReadsVec The deferred read context data vector.
     * @param handlerPtr The completion handler pointer.
     * @return True if ALL the read operations in the vector could be queued successfully, or False otherwise.
     * @post The argument to handler is left in a moved-from state.
     */
    HDTN_UTIL_EXPORT bool ReadMemoryAsync(const std::vector<deferred_read_t>& deferredReadsVec, read_memory_handler_t&& handler);

    /** Get the total number of files created by this instance.
     *
     * @return The total number of files created by this instance.
     */
    HDTN_UTIL_EXPORT uint64_t GetCountTotalFilesCreated() const;
    
    /** Get the number of currently active files.
     *
     * Active files are ones storing data that still await further processing or are currently in use, when files become inactive they get deleted.
     * @return The number of currently active files.
     */
    HDTN_UTIL_EXPORT uint64_t GetCountTotalFilesActive() const;

    /** Get total number of bytes required for smallest memory block to fit given number of bytes.
     *
     * @param minimumDesiredBytes The minimum number of bytes to fit.
     * @return The total number of bytes required for smallest memory block to fit minimumDesiredBytes number of bytes.
     */
    HDTN_UTIL_EXPORT static uint64_t CeilToNearestBlockMultiple(const uint64_t minimumDesiredBytes);
    
private:
    /// PIMPL idiom
    struct Impl;
    /// Pointer to the internal implementation
    std::unique_ptr<Impl> m_pimpl;
};

#endif //_MEMORY_IN_FILES_H
