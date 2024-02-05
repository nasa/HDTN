/**
 * @file MemoryInFiles.cpp
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

#ifndef _WIN32
#define _LARGEFILE64_SOURCE
#define _FILE_OFFSET_BITS 64
#include <fcntl.h>
#endif

#include "MemoryInFiles.h"
#include "ForwardListQueue.h"
#include "FragmentSet.h"
#include <boost/thread.hpp>
#include <memory>
#include <unordered_map>
#include <vector>
#include <queue>
#include "Logger.h"
#include <boost/make_unique.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/format.hpp>
#ifdef _WIN32
#include <windows.h> //must be included after boost
#endif

static constexpr hdtn::Logger::SubProcess subprocess = hdtn::Logger::SubProcess::none;

/// Memory block unit size in bytes
static constexpr uint64_t BLOCK_SIZE_MULTIPLE = 4096; // 4 KiB
/// Memory block mask
static constexpr uint64_t BLOCK_SIZE_MULTIPLE_MASK = BLOCK_SIZE_MULTIPLE - 1;
/// Memory block shift
static constexpr uint64_t BLOCK_SIZE_MULTIPLE_SHIFT = 12; //(1<<12) == 4096

uint64_t MemoryInFiles::CeilToNearestBlockMultiple(const uint64_t minimumDesiredBytes) {
    //const uint64_t totalBlocksRequired = (minimumDesiredBytes / BLOCK_SIZE_MULTIPLE) + ((minimumDesiredBytes % BLOCK_SIZE_MULTIPLE) == 0 ? 0 : 1);
    const bool addOne = ((minimumDesiredBytes & BLOCK_SIZE_MULTIPLE_MASK) != 0); //((minimumDesiredBytes % BLOCK_SIZE_MULTIPLE) == 0 ? 0 : 1);
    const uint64_t totalBlocksRequired = (minimumDesiredBytes >> BLOCK_SIZE_MULTIPLE_SHIFT) + addOne;
    const uint64_t allocationSize = (totalBlocksRequired << BLOCK_SIZE_MULTIPLE_SHIFT);
    return allocationSize;
}

struct MemoryInFiles::Impl : private boost::noncopyable {

    

    Impl() = delete;
    
    /**
     * Set counters to 0.
     * Start memory block IDs from 1 since 0 is reversed for error reporting.
     * Reserve space for estimatedMaxAllocatedBlocks number of memory blocks.
     * Create the working directory on disk if it does not exist.
     * @param ioServiceRef The I/O execution context.
     * @param workingDirectory The working directory for file I/O.
     * @param newFileAggregationTimeMs The time window in milliseconds for which write allocations will be redirected to the currently active write file.
     * @param estimatedMaxAllocatedBlocks The number of memory blocks to reverse space for, this is a soft cap to lessen instances of reallocation, the actual space will be expanded if needed.
     */
    Impl(boost::asio::io_service& ioServiceRef,
        const boost::filesystem::path& workingDirectory,
        const uint64_t newFileAggregationTimeMs,
        const uint64_t estimatedMaxAllocatedBlocks);
    
    /**
     * Stop timers.
     * Clear allocated memory blocks.
     * Delete the working directory from disk.
     */
    ~Impl();
    
    /// ...
    void Stop();
    
    /** Allocate new memory block of at least given size.
     *
     * Tries to insert memory block metadata for the memory block, if succeeds then calls MemoryBlockInfo::Resize() to perform the allocation.
     * @param totalSize The minimum number of bytes to fit
     * @return The memory block ID to the newly created memory block on successful allocation, or 0 if the allocation failed.
     */
    uint64_t AllocateNewWriteMemoryBlock(uint64_t totalSize);
    
    /** Resize the given memory block.
     *
     * If the given memory block ID does not reference a valid memory block, returns immediately with a value of 0.
     * Else, calls MemoryBlockInfo::Resize() to perform the resize.
     * @param memoryBlockId The memory block ID.
     * @param newSize The minimum number of bytes to fit.
     * @return The total size in bytes of the newly resized allocation on successful resize, or 0 if the resize failed.
     */
    uint64_t Resize(const uint64_t memoryBlockId, uint64_t newSize);
    
    /** Get the size of the given memory block.
     *
     * If the given memory block ID does not reference a valid memory block, returns immediately with a value of 0.
     * Else, returns the allocation size in bytes of the memory block.
     * @param memoryBlockId The memory block ID.
     * @return The allocation size of the memory block on successful retrieval, or 0 if retrieval failed.
     */
    uint64_t GetSizeOfMemoryBlock(const uint64_t memoryBlockId) const noexcept;
    
    /** Handle deferred deletion of the given memory block.
     *
     * Deletes the memory block.
     * @param memoryBlockId The memory block ID.
     */
    void ForceDeleteMemoryBlock(const uint64_t memoryBlockId); //intended only for boost::asio::post calls to defer delete when io operations in progress
    
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
    bool DeleteMemoryBlock(const uint64_t memoryBlockId);
    
    /** Initiate a request to delete the given memory block.
     *
     * Initiates an asynchronous request to Impl::HandleAsyncDeleteMemoryBlock() to delete the memory block.
     * @param memoryBlockId The memory block ID.
     */
    void AsyncDeleteMemoryBlock(const uint64_t memoryBlockId);
    
    /** Initiate deferred single-write operation.
     *
     * If the given memory block ID does not reference a valid memory block, returns immediately.
     * Else, calls MemoryBlockInfo::DoWriteOrRead() to initiate the deferred write operation.
     * @param deferredWrite The deferred write context data.
     * @param handlerPtr The completion handler pointer.
     * @return True if the write operation was queued successfully, or False otherwise.
     */
    bool WriteMemoryAsync(const deferred_write_t& deferredWrite, std::shared_ptr<write_memory_handler_t>& handlerPtr);
    
    /** Initiate deferred multi-write operation.
     *
     * Calls Impl::WriteMemoryAsync() for every operation in the vector.
     * @param deferredWritesVec The deferred write context data vector.
     * @param handlerPtr The completion handler pointer.
     * @return True if ALL the write operations in the vector could be queued successfully, or False otherwise.
     */
    bool WriteMemoryAsync(const std::vector<deferred_write_t>& deferredWritesVec, std::shared_ptr<write_memory_handler_t>& handlerPtr);
    
    /** Initiate deferred single-read operation.
     *
     * If the given memory block ID does not reference a valid memory block, returns immediately.
     * Else, calls MemoryBlockInfo::DoWriteOrRead() to initiate the deferred read operation.
     * @param deferredRead The deferred read context data.
     * @param handlerPtr The completion handler pointer.
     * @return True if the read operation was queued successfully, or False otherwise.
     */
    bool ReadMemoryAsync(const deferred_read_t& deferredRead, std::shared_ptr<read_memory_handler_t>& handlerPtr);
    
    /** Initiate deferred multi-read operation.
     *
     * Calls Impl::ReadMemoryAsync() for every operation in the vector.
     * @param deferredReadsVec The deferred read context data vector.
     * @param handlerPtr The completion handler pointer.
     * @return True if ALL the read operations in the vector could be queued successfully, or False otherwise.
     */
    bool ReadMemoryAsync(const std::vector<deferred_read_t>& deferredReadsVec, std::shared_ptr<read_memory_handler_t>& handlerPtr);

private:
    /** handle deletion of the given memory block.
     *
     * Used for logging purposes.
     * @param memoryBlockId The memory block ID.
     */
    void HandleAsyncDeleteMemoryBlock(const uint64_t memoryBlockId);

    /// Forward declaration
    struct MemoryBlockInfo;
    
    /** Resize the given memory block.
     *
     * If the currently active write file is not operational, returns immediately.
     * Else, calls MemoryInFiles::CeilToNearestBlockMultiple() to get the new allocation length and then calls MemoryBlockInfo::Resize() to perform the resize.
     * @param mbi The associated memory block metadata.
     * @param newSize The minimum number of bytes to fit.
     * @return The increase in size (NOT the total size of the new allocation) if resizing to a larger length, or 0 if the new length is smaller than the current length.
     */
    uint64_t Resize(MemoryBlockInfo& mbi, uint64_t newSize);
    
    /** Setup next active file if none is currently active.
     *
     * If there is an active write file (we are inside an active write window), returns immediately.
     * Else, creates a new write file on disk, then starts the active write window timer.
     * @return True if A write file (either existing or newly created) is now active and ready to be operated on, or False otherwise.
     * @post If returns True, the active write file is ready to be operated on.
     */
    bool SetupNextFileIfNeeded();

#ifdef _WIN32
    /// System file handle
    typedef boost::asio::windows::random_access_handle file_handle_t;
#else
    /// System file handle
    typedef boost::asio::posix::stream_descriptor file_handle_t;
#endif

    

    /// I/O operation context data
    struct io_operation_t : private boost::noncopyable {
        io_operation_t() = delete;
        /**
         * Set m_readToThisLocationPtr to NULL.
         * @param memoryBlockInfoRef The associated memory block.
         * @param offsetWithinFile The file offset to begin operating on.
         * @param length The data length to operate on.
         * @param writeLocationPtr The input begin pointer to read from.
         * @param writeHandlerPtr The I/O write operation completion handler.
         */
        io_operation_t(
            MemoryBlockInfo& memoryBlockInfoRef,
            uint64_t offsetWithinFile,
            uint64_t length,
            const void* writeLocationPtr,
            std::shared_ptr<write_memory_handler_t>& writeHandlerPtr);
        /**
         * Set m_writeFromThisLocationPtr to NULL.
         * @param memoryBlockInfoRef The associated memory block.
         * @param readHandlerPtr The I/O read operation completion handler.
         * @param offsetWithinFile The file offset to begin operating on.
         * @param length The data length to operate on.
         * @param readLocationPtr The output begin pointer to read into.
         */
        io_operation_t(
            MemoryBlockInfo& memoryBlockInfoRef,
            std::shared_ptr<read_memory_handler_t>& readHandlerPtr,
            uint64_t offsetWithinFile,
            uint64_t length,
            void* readLocationPtr);

        /// Associated memory block reference
        MemoryBlockInfo& m_memoryBlockInfoRef; //references to unordered_map never get invalidated, only iterators
        /// File offset to begin operating on
        uint64_t m_offsetWithinFile;
        /// Data length in bytes to operate on
        uint64_t m_length;
        /// Type-erased output begin pointer in case of read, NULL on write
        void* m_readToThisLocationPtr;
        /// Type-erased input begin pointer in case of read, NULL on read
        const void* m_writeFromThisLocationPtr;
        /// Read operation completion handler shared pointer, one copy per operation for multi-read operations, nullptr on write
        std::shared_ptr<read_memory_handler_t> m_readHandlerPtr;
        /// Write operation completion handler shared pointer, one copy per operation for multi-write operations, nullptr on read
        std::shared_ptr<write_memory_handler_t> m_writeHandlerPtr;
    };

    struct FileInfo : private boost::noncopyable {
        FileInfo() = delete;
        /**
         * @param filePath The filesystem path to the associated file.
         * @param ioServiceRef The I/O execution context.
         * @param implRef PIMPL reference.
         */
        FileInfo(const boost::filesystem::path& filePath, boost::asio::io_service & ioServiceRef, MemoryInFiles::Impl& implRef);
        /**
         * Cleans up any open system file resources and deletes the associated file from disk.
         * @post Decrements the pending I/O operation reference counter (m_queuedOperationsReferenceCount).
         */
        ~FileInfo();
        /** Try queuing an asynchronous I/O write operation.
         * If the file is NOT open and ready to be operated on, returns immediately.
         * Else, queues a new I/O operation, increments the pending I/O operation reference counter, then calls FileInfo::TryStartNextQueuedIoOperation()
         * to trigger processing of the next queued I/O operation.
         * @param memoryBlockInfoRef The associated memory block.
         * @param offsetWithinFile The file offset to begin writing to.
         * @param data Type-erased input begin pointer to read from.
         * @param length The data length in bytes to write.
         * @param handlerPtr Type-erased I/O completion handler pointer.
         * @return True if the file is open and ready to be operated on, or False otherwise.
         * @post MIGHT increment the pending I/O operation reference counter (m_queuedOperationsReferenceCount).
         */
        bool WriteMemoryAsync(MemoryBlockInfo& memoryBlockInfoRef, const uint64_t offsetWithinFile, const void* data, uint64_t length, std::shared_ptr<write_memory_handler_t>& handlerPtr);
        /** Try queuing an asynchronous I/O read operation.
         * If the file is NOT open and ready to be operated on, returns immediately.
         * Else, queues a new I/O operation, increments the pending I/O operation reference counter, then calls FileInfo::TryStartNextQueuedIoOperation()
         * to trigger processing of the next queued I/O operation.
         * @param memoryBlockInfoRef The associated memory block.
         * @param offsetWithinFile The file offset to begin reading from.
         * @param data Type-erased output begin pointer to read into.
         * @param length The data length in bytes to read.
         * @param handlerPtr Type-erased I/O completion handler pointer.
         * @return True if the file is open and ready to be operated on, or False otherwise.
         * @post MIGHT increment the pending I/O operation reference counter (m_queuedOperationsReferenceCount).
         */
        bool ReadMemoryAsync(MemoryBlockInfo& memoryBlockInfoRef, const uint64_t offsetWithinFile, void* data, uint64_t length, std::shared_ptr<read_memory_handler_t>& handlerPtr);
    private:
        /** Handle completion of a system write operation.
         * If this is the last component of a multi-write operation, calls the final I/O operation handler (io_operation_t::m_writeHandlerPtr) (if any).
         * ALWAYS calls FileInfo::FinishCompletionHandler() to finalize the completion of the I/O operation.
         * @param error The error code.
         * @param bytes_transferred The number of bytes written.
         */
        void HandleDiskWriteCompleted(const boost::system::error_code& error, std::size_t bytes_transferred);
        /** Handle completion of a system read operation.
         * If this is the last component of a multi-read operation, calls the final I/O operation handler (io_operation_t::m_readHandlerPtr) (if any).
         * ALWAYS calls FileInfo::FinishCompletionHandler() to finalize the completion of the I/O operation.
         * @param error The error code.
         * @param bytes_transferred The number of bytes read.
         */
        void HandleDiskReadCompleted(const boost::system::error_code& error, std::size_t bytes_transferred);
        /** Finalize completion of the given I/O operation.
         * If the associated memory block is due deletion and there are no pending I/O operations left, initiates an asynchronous request to Impl::ForceDeleteMemoryBlock()
         * to clean up the memory block.
         * Calls FileInfo::TryStartNextQueuedIoOperation() to trigger processing of the next queued I/O operation.
         * @param op The completed I/O operation.
         * @post Decrements the pending I/O operation reference counter (m_queuedOperationsReferenceCount).
         */
        void FinishCompletionHandler(io_operation_t& op);
        /** Try start next I/O operation.
         * If there are no queued operations left or an operation is currently in progress, returns immediately.
         * Else, acts accordingly to the following cases:
         * 1. For read operations, initiates an asynchronous system read operation with FileInfo::HandleDiskReadCompleted() as a completion handler.
         * 2. For write operations, initiates an asynchronous system write operation with FileInfo::HandleDiskWriteCompleted() as a completion handler.
         */
        void TryStartNextQueuedIoOperation();
        
        
        /// I/O operation queue
        std::queue<io_operation_t> m_queueIoOperations;
        /// Pointer to the underlying system file handle
        std::unique_ptr<file_handle_t> m_fileHandlePtr;
        /// Filesystem path to the associated file
        boost::filesystem::path m_filePath;
        /// PIMPL reference
        MemoryInFiles::Impl& m_implRef;
        /// Whether an I/O operation is currently in progress
        bool m_diskOperationInProgress;
        /// Whether the associated file is open and ready to be operated on
        bool m_valid;
    };

    /// Memory block fragment metadata
    struct MemoryBlockFragmentInfo : private boost::noncopyable {
        MemoryBlockFragmentInfo() = delete;
        /**
         * Initialize metadata.
         * @param fileInfoPtr The file shared pointer.
         * @param baseOffsetWithinFile The memory block fragment file offset.
         * @param lengthAlignedToBlockSize The memory block fragment length in bytes.
         */
        MemoryBlockFragmentInfo(const std::shared_ptr<FileInfo>& fileInfoPtr, const uint64_t baseOffsetWithinFile, const uint64_t lengthAlignedToBlockSize);
        /// File metadata shared pointer, when all fragments in a file go out of scope the file can then be safely deleted to reclaim disk space
        std::shared_ptr<FileInfo> m_fileInfoPtr;
        /// Memory block fragment file offset
        const uint64_t m_baseOffsetWithinFile;
        /// Memory block fragment length in bytes
        const uint64_t m_length;
    };

    /// Memory block metadata
    struct MemoryBlockInfo : private boost::noncopyable {
        MemoryBlockInfo() = delete;
        /**
         * Set the memory block ID.
         * @param memoryBlockId The memory block ID.
         */
        MemoryBlockInfo(const uint64_t memoryBlockId);
        /** Resize this memory block.
         * If new length is smaller than the current length, returns immediately with a value of 0.
         * Else, on the current active write file creates a new fragment that covers the length difference, thus expanding this memory block by the desired amount.
         * @param currentFileInfoPtr The current active write file.
         * @param currentBaseOffsetWithinFile The current active file offset to begin writing from.
         * @param newLengthAlignedToBlockSize The target memory block length in bytes.
         * @return The increase in size (NOT the total size of the new allocation) if resizing to a larger length, or 0 if the new length is smaller than the current length.
         */
        uint64_t Resize(const std::shared_ptr<FileInfo>& currentFileInfoPtr, //returns the increase in size
            const uint64_t currentBaseOffsetWithinFile, const uint64_t newLengthAlignedToBlockSize);
        /** Initiate a read or write operation.
         * If the block is due deletion or the data length to operate on exceeds the allocated size of the block, returns immediately.
         * Else, acts accordingly to the following cases:
         * 1. When (readToThisLocationPtr == NULL), we are attempting to queue a write operation, calls FileInfo::WriteMemoryAsync() on each eligible fragment.
         * 2. When (writeFromThisLocationPtr == NULL), we are attempting to queue a read operation, calls FileInfo::ReadMemoryAsync() on each eligible fragment.
         * Eligible is any fragment that overlaps with the range we are operating on [deferredOffset, (deferredOffset + deferredLength)], as such
         * if N fragments are eligible, this function will queue N individual I/O operations.
         * @param deferredOffset The target memory block offset.
         * @param deferredLength The data length in bytes to read or write.
         * @param readToThisLocationPtr Type-erased output begin pointer in case of read, NULL on write.
         * @param writeFromThisLocationPtr Type-erased input begin pointer in case of write, NULL on read.
         * @param handlerPtrPtr Type-erased I/O completion handler pointer.
         * @return True if the operation could be queued successfully, or False otherwise.
         */
        bool DoWriteOrRead(const uint64_t deferredOffset, const uint64_t deferredLength,
            void* readToThisLocationPtr, const void* writeFromThisLocationPtr, void* handlerPtrPtr);
        /// Memory block ID
        const uint64_t m_memoryBlockId;
        /// Memory block fragment metadata for all fragments that make up this memory block, the same file may contain multiple fragments and fragments may span multiple files
        ForwardListQueue<MemoryBlockFragmentInfo> m_memoryBlockFragmentInfoFlistQueue;
        /// Memory block size in bytes, this is the size of the allocation and may differ from the size of the application data currently stored in the block
        uint64_t m_lengthAlignedToBlockSize;
        /// Pending I/O operations reference counter
        unsigned int m_queuedOperationsReferenceCount;
        /// Whether the block is due deletion, for graceful cleanup wait until there are no pending I/O operations (m_queuedOperationsReferenceCount == 0)
        bool m_markedForDeletion;
    };
    
    /** Try restart the active write window timer.
     *
     * If the active write window timer is already running, returns immediately.
     * Else, starts the active write window timer asynchronously with MemoryBlockInfo::OnNewFileAggregation_TimerExpired() as a completion handler.
     */
    void TryRestartNewFileAggregationTimer();
    
    /** Handle active write window timer expiry.
     *
     * If the expiry occurred due to the timer being manually cancelled, returns immediately.
     * Else, resets the currently active write file pointer to prepare for the next write window, it does NOT start the next write window automatically
     * to prevent periodic empty files from being created during periods where no writes are taking place.
     * @param e The error code.
     */
    void OnNewFileAggregation_TimerExpired(const boost::system::error_code& e);

    /// Type of map holding memory block metadata, mapped by memory block ID
    typedef std::unordered_map<uint64_t, MemoryBlockInfo> id_to_memoryblockinfo_map_t;

    

    /// Memory block metadata for currently allocated memory blocks, mapped by memory block ID
    id_to_memoryblockinfo_map_t m_mapIdToMemoryBlockInfo;
    /// Currently active write file pointer, the file that will be used during the current active write window
    std::shared_ptr<FileInfo> m_currentWritingFileInfoPtr;

    /// I/O execution context reference
    boost::asio::io_service& m_ioServiceRef;
    /// Active write window timer, the time window for which write allocations will be redirected to the currently active write file, on expiry (and after the next write) a new file will be created
    boost::asio::deadline_timer m_newFileAggregationTimer;
    /// Our working directory for file I/O
    const boost::filesystem::path m_rootStorageDirectory;
    /// Active write window interval in milliseconds, used by m_newFileAggregationTimeDuration
    const uint64_t m_newFileAggregationTimeMs;
    /// Active write window duration, used by m_newFileAggregationTimer
    const boost::posix_time::time_duration m_newFileAggregationTimeDuration;
    /// Whether a write window is currently active, will be False on m_newFileAggregationTimer expiry until the next time we queue a write allocation for processing
    bool m_newFileAggregationTimerIsRunning;
    /// Memory block ID to assign to the next memory block allocated, starts from 1 since memory block ID of 0 is reserved for error reporting
    uint64_t m_nextMemoryBlockId;
    /// File ID to assign to the next file created
    uint64_t m_nextFileId;
    /// File offset to begin the next write for the current active write file
    uint64_t m_nextOffsetOfCurrentFile;
public: //stats
    /// Total number of files created in the lifetime of the instance
    uint64_t m_countTotalFilesCreated;
    /// Number of currently active files, active files are ones storing data that still await further processing or are currently in use, when files become inactive they get deleted
    uint64_t m_countTotalFilesActive;
};

MemoryInFiles::Impl::io_operation_t::io_operation_t(
    MemoryBlockInfo& memoryBlockInfoRef,
    uint64_t offsetWithinFile,
    uint64_t length,
    const void* writeLocationPtr,
    std::shared_ptr<write_memory_handler_t>& writeHandlerPtr) :
    ///
    m_memoryBlockInfoRef(memoryBlockInfoRef),
    m_offsetWithinFile(offsetWithinFile),
    m_length(length),
    m_readToThisLocationPtr(NULL),
    m_writeFromThisLocationPtr(writeLocationPtr),
    m_writeHandlerPtr(writeHandlerPtr) {}

MemoryInFiles::Impl::io_operation_t::io_operation_t(
    MemoryBlockInfo& memoryBlockInfoRef,
    std::shared_ptr<read_memory_handler_t>& readHandlerPtr,
    uint64_t offsetWithinFile,
    uint64_t length,
    void* readLocationPtr) :
    ///
    m_memoryBlockInfoRef(memoryBlockInfoRef),
    m_offsetWithinFile(offsetWithinFile),
    m_length(length),
    m_readToThisLocationPtr(readLocationPtr),
    m_writeFromThisLocationPtr(NULL),
    m_readHandlerPtr(readHandlerPtr) {}

MemoryInFiles::Impl::FileInfo::FileInfo(const boost::filesystem::path& filePath, boost::asio::io_service& ioServiceRef, MemoryInFiles::Impl& implRef) :
    m_filePath(filePath),
    m_implRef(implRef),
    m_diskOperationInProgress(false),
    m_valid(false)
{
    const boost::filesystem::path::value_type* filePathCstr = m_filePath.c_str();
#if defined(_WIN32)
    //
    //https://docs.microsoft.com/en-us/windows/win32/fileio/synchronous-and-asynchronous-i-o
    //In synchronous file I/O, a thread starts an I/O operation and immediately enters a wait state until the I/O request has completed.
    //A thread performing asynchronous file I/O sends an I/O request to the kernel by calling an appropriate function.
    //If the request is accepted by the kernel, the calling thread continues processing another job until the kernel signals to
    //the thread that the I/O operation is complete. It then interrupts its current job and processes the data from the I/O operation as necessary.
    //Asynchronous I/O is also referred to as overlapped I/O.
    HANDLE hFile = CreateFileW(filePathCstr,                // name of the file
        GENERIC_READ | GENERIC_WRITE,          // open for reading and writing
        0,                      // do not share
        NULL,                   // default security
        //CREATE_ALWAYS : Creates a new file, always. If the specified file exists and is writable, the function overwrites the file
        //OPEN_EXISTING : Opens a file or device, only if it exists.  If the specified file or device does not exist, the function fails and the last - error code is set to ERROR_FILE_NOT_FOUND(2).
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED,  // normal file
        NULL);                  // no attr. template

    if (hFile == INVALID_HANDLE_VALUE) {
        LOG_ERROR(subprocess) << "error opening " << m_filePath;
        return;
    } 
#elif defined(__APPLE__)
    int hFile = open(filePathCstr, (O_CREAT | O_RDWR | O_TRUNC), DEFFILEMODE);
    if (hFile < 0) {
        LOG_ERROR(subprocess) << "error opening " << m_filePath;
        return;
    }
#else // Linux (not WIN32 or APPLE)
    int hFile = open(filePathCstr, (O_CREAT | O_RDWR | O_TRUNC | O_LARGEFILE), DEFFILEMODE);
    if (hFile < 0) {
        LOG_ERROR(subprocess) << "error opening " << m_filePath;
        return;
    }
#endif
    m_fileHandlePtr = boost::make_unique<file_handle_t>(ioServiceRef, hFile);
    m_valid = true;
}
MemoryInFiles::Impl::FileInfo::~FileInfo() {
    //last shared_ptr to delete calls this destructor
    --m_implRef.m_countTotalFilesActive;
    if (m_fileHandlePtr) {
        m_fileHandlePtr->close();
        m_fileHandlePtr.reset(); //delete it
    }
    if (!boost::filesystem::remove(m_filePath)) {
        LOG_ERROR(subprocess) << "error deleting file " << m_filePath;
    }
}
void MemoryInFiles::Impl::FileInfo::TryStartNextQueuedIoOperation() {
    if (m_queueIoOperations.size() && (!m_diskOperationInProgress)) {
        io_operation_t& op = m_queueIoOperations.front();
        m_diskOperationInProgress = true;
        if (op.m_readToThisLocationPtr) {
#if defined(_WIN32)
            boost::asio::async_read_at(*m_fileHandlePtr, op.m_offsetWithinFile,
#elif defined(__APPLE__)
    lseek(m_fileHandlePtr->native_handle(), op.m_offsetWithinFile, SEEK_SET);
            boost::asio::async_read(*m_fileHandlePtr,
#else // Linux (not WIN32 or APPLE)
            lseek64(m_fileHandlePtr->native_handle(), op.m_offsetWithinFile, SEEK_SET);
            boost::asio::async_read(*m_fileHandlePtr,
#endif
                boost::asio::buffer(op.m_readToThisLocationPtr, op.m_length),
                boost::bind(&MemoryInFiles::Impl::FileInfo::HandleDiskReadCompleted, this,
                    boost::asio::placeholders::error,
                    boost::asio::placeholders::bytes_transferred));
        }
        else { //write operation
#if defined(_WIN32)
            boost::asio::async_write_at(*m_fileHandlePtr, op.m_offsetWithinFile,
#elif defined(__APPLE__)
            lseek(m_fileHandlePtr->native_handle(), op.m_offsetWithinFile, SEEK_SET);
            boost::asio::async_write(*m_fileHandlePtr,
#else // Linux (not WIN32 or APPLE)
            lseek64(m_fileHandlePtr->native_handle(), op.m_offsetWithinFile, SEEK_SET);
            boost::asio::async_write(*m_fileHandlePtr,
#endif
                boost::asio::const_buffer(op.m_writeFromThisLocationPtr, op.m_length),
                boost::bind(&MemoryInFiles::Impl::FileInfo::HandleDiskWriteCompleted, this,
                    boost::asio::placeholders::error,
                    boost::asio::placeholders::bytes_transferred));
        }
    }
}
bool MemoryInFiles::Impl::FileInfo::WriteMemoryAsync(MemoryBlockInfo& memoryBlockInfoRef, const uint64_t offsetWithinFile, const void* data, uint64_t length, std::shared_ptr<write_memory_handler_t>& handlerPtr) {
    if (m_valid) {
        m_queueIoOperations.emplace(memoryBlockInfoRef, offsetWithinFile, length, data, handlerPtr);
        ++memoryBlockInfoRef.m_queuedOperationsReferenceCount;
        TryStartNextQueuedIoOperation();
    }
    return m_valid;
}
void MemoryInFiles::Impl::FileInfo::HandleDiskWriteCompleted(const boost::system::error_code& error, std::size_t bytes_transferred) {
    (void)bytes_transferred;
    io_operation_t& op = m_queueIoOperations.front();
    if (error) {
        LOG_ERROR(subprocess) << "HandleDiskWriteCompleted: " << error.message();
    }
    else {
        if ((op.m_writeHandlerPtr) && (op.m_writeHandlerPtr.use_count() == 1)) { //only the last reference to a handler should be called to complete a multi-operation
            write_memory_handler_t& handler = *op.m_writeHandlerPtr;
            if (handler) {
                handler();
            }
        }
    }
    FinishCompletionHandler(op);
}

bool MemoryInFiles::Impl::FileInfo::ReadMemoryAsync(MemoryBlockInfo& memoryBlockInfoRef, const uint64_t offsetWithinFile, void* data, uint64_t length, std::shared_ptr<read_memory_handler_t>& handlerPtr) {
    if (m_valid) {
        m_queueIoOperations.emplace(memoryBlockInfoRef, handlerPtr, offsetWithinFile, length, data);
        ++memoryBlockInfoRef.m_queuedOperationsReferenceCount;
        TryStartNextQueuedIoOperation();
    }
    return m_valid;
}
void MemoryInFiles::Impl::FileInfo::HandleDiskReadCompleted(const boost::system::error_code& error, std::size_t bytes_transferred) {
    (void)bytes_transferred;
    io_operation_t& op = m_queueIoOperations.front();
    bool success = true;
    if (error) {
        LOG_ERROR(subprocess) << "HandleDiskReadCompleted: " << error.message();
        success = false;
    }
    if ((op.m_readHandlerPtr) && (op.m_readHandlerPtr.use_count() == 1)) { //only the last reference to a handler should be called to complete a multi-operation
        read_memory_handler_t& handler = *op.m_readHandlerPtr;
        if (handler) {
            handler(success);
        }
    }
    FinishCompletionHandler(op);
}
void MemoryInFiles::Impl::FileInfo::FinishCompletionHandler(io_operation_t& op) {
    --op.m_memoryBlockInfoRef.m_queuedOperationsReferenceCount;
    if (op.m_memoryBlockInfoRef.m_markedForDeletion && (op.m_memoryBlockInfoRef.m_queuedOperationsReferenceCount == 0)) {
        //don't potentially delete "this" (FileInfo) while in a FileInfo function when deleting a memoryBlockInfo which has a shared_ptr to the FileInfo
        boost::asio::post(m_implRef.m_ioServiceRef, boost::bind(&MemoryInFiles::Impl::ForceDeleteMemoryBlock, &m_implRef, op.m_memoryBlockInfoRef.m_memoryBlockId));
    }
    m_queueIoOperations.pop();
    m_diskOperationInProgress = false;
    TryStartNextQueuedIoOperation();
}

void MemoryInFiles::Impl::TryRestartNewFileAggregationTimer() {
    if (!m_newFileAggregationTimerIsRunning) {
        const boost::posix_time::ptime nowPtime = boost::posix_time::microsec_clock::universal_time();
        m_newFileAggregationTimer.expires_at(nowPtime + m_newFileAggregationTimeDuration);
        m_newFileAggregationTimer.async_wait(boost::bind(&MemoryInFiles::Impl::OnNewFileAggregation_TimerExpired, this, boost::asio::placeholders::error));
        m_newFileAggregationTimerIsRunning = true;
    }
}
void MemoryInFiles::Impl::OnNewFileAggregation_TimerExpired(const boost::system::error_code& e) {
    if (e != boost::asio::error::operation_aborted) {
        // Timer was not cancelled, take necessary action.
        m_currentWritingFileInfoPtr.reset();
        m_newFileAggregationTimerIsRunning = false;
    }
}

MemoryInFiles::Impl::MemoryBlockFragmentInfo::MemoryBlockFragmentInfo(
    const std::shared_ptr<FileInfo>& fileInfoPtr,
    const uint64_t baseOffsetWithinFile,
    const uint64_t lengthAlignedToBlockSize) :
    //
    m_fileInfoPtr(fileInfoPtr),
    m_baseOffsetWithinFile(baseOffsetWithinFile),
    m_length(lengthAlignedToBlockSize) {}


MemoryInFiles::Impl::MemoryBlockInfo::MemoryBlockInfo(const uint64_t memoryBlockId) :
    m_memoryBlockId(memoryBlockId),
    m_lengthAlignedToBlockSize(0),
    m_queuedOperationsReferenceCount(0),
    m_markedForDeletion(false)
{

}

//returns the increase in size
uint64_t MemoryInFiles::Impl::MemoryBlockInfo::Resize(const std::shared_ptr<FileInfo>& currentFileInfoPtr,
    const uint64_t currentBaseOffsetWithinFile, const uint64_t newLengthAlignedToBlockSize)
{
    if (newLengthAlignedToBlockSize <= m_lengthAlignedToBlockSize) {
        return 0; //0-byte increase in file size
    }
    const uint64_t diffSize = newLengthAlignedToBlockSize - m_lengthAlignedToBlockSize;
    m_lengthAlignedToBlockSize = newLengthAlignedToBlockSize;

    //insert (order by FIFO, so newest elements will be last)
    m_memoryBlockFragmentInfoFlistQueue.emplace_back(currentFileInfoPtr, currentBaseOffsetWithinFile, diffSize);

    return diffSize;
}

MemoryInFiles::Impl::Impl(boost::asio::io_service& ioServiceRef,
    const boost::filesystem::path& workingDirectory,
    const uint64_t newFileAggregationTimeMs, const uint64_t estimatedMaxAllocatedBlocks) :
    m_ioServiceRef(ioServiceRef),
    m_newFileAggregationTimer(ioServiceRef),
    m_rootStorageDirectory(workingDirectory / boost::filesystem::unique_path()),
    m_newFileAggregationTimeMs(newFileAggregationTimeMs),
    m_newFileAggregationTimeDuration(boost::posix_time::milliseconds(newFileAggregationTimeMs)),
    m_newFileAggregationTimerIsRunning(false),
    m_nextMemoryBlockId(1), //0 reserved for error
    m_nextFileId(0),
    m_nextOffsetOfCurrentFile(0),
    m_countTotalFilesCreated(0),
    m_countTotalFilesActive(0)
{
    m_mapIdToMemoryBlockInfo.reserve(estimatedMaxAllocatedBlocks);
    if (boost::filesystem::is_directory(workingDirectory)) {
        if (!boost::filesystem::is_directory(m_rootStorageDirectory)) {
            if (!boost::filesystem::create_directory(m_rootStorageDirectory)) {
                LOG_ERROR(subprocess) << "Unable to create MemoryInFiles storage directory: " << m_rootStorageDirectory;
            }
            else {
                LOG_INFO(subprocess) << "Created MemoryInFiles storage directory: " << m_rootStorageDirectory;
            }
        }
    }
    else {
        LOG_ERROR(subprocess) << "MemoryInFiles working directory does not exist: " << workingDirectory;
    }
}
MemoryInFiles::Impl::~Impl() {
    try {
        m_newFileAggregationTimer.cancel();
    }
    catch (const boost::system::system_error& e) {
        LOG_WARNING(subprocess) << "MemoryInFiles ~Impl calling newFileAggregationTimer.cancel(): " << e.what();
    }
    m_mapIdToMemoryBlockInfo.clear();
    m_currentWritingFileInfoPtr.reset();

    if (boost::filesystem::is_directory(m_rootStorageDirectory)) {
        if (!boost::filesystem::remove_all(m_rootStorageDirectory)) {
            LOG_ERROR(subprocess) << "Unable to remove directory " << m_rootStorageDirectory;
        }
    }
}

bool MemoryInFiles::Impl::SetupNextFileIfNeeded() {
    if (!m_currentWritingFileInfoPtr) {
        m_nextOffsetOfCurrentFile = 0;
        static const boost::format fmtTemplate("ltp_%09d.bin");
        boost::format fmt(fmtTemplate);
        fmt% m_nextFileId++;
        const boost::filesystem::path fullFilePath = m_rootStorageDirectory / boost::filesystem::path(fmt.str());

        if (boost::filesystem::exists(fullFilePath)) {
            LOG_ERROR(subprocess) << "MemoryInFiles::Impl::WriteMemoryAsync: " << fullFilePath << " already exists";
            return false;
        }
        m_currentWritingFileInfoPtr = std::make_shared<FileInfo>(fullFilePath, m_ioServiceRef, *this);
        TryRestartNewFileAggregationTimer(); //only start timer on new write allocation to prevent periodic empty files from being created
        ++m_countTotalFilesCreated;
        ++m_countTotalFilesActive;
    }
    return true;
}

uint64_t MemoryInFiles::Impl::AllocateNewWriteMemoryBlock(uint64_t totalSize) {
    const uint64_t memoryBlockId = m_nextMemoryBlockId;
    //try_emplace does not work with piecewise_construct
    std::pair<id_to_memoryblockinfo_map_t::iterator, bool> ret = m_mapIdToMemoryBlockInfo.emplace(
            std::piecewise_construct,
            std::forward_as_tuple(memoryBlockId),
            std::forward_as_tuple(memoryBlockId));
    //(true if insertion happened, false if it did not).
    if (ret.second) {
        ++m_nextMemoryBlockId;
        MemoryBlockInfo& mbi = ret.first->second;
        if (Resize(mbi, totalSize) == 0) {
            return 0; //fail
        }
        return memoryBlockId;
    }
    else {
        //LOG_ERROR(subprocess) << "Unable to insert memoryBlockId " << memoryBlockId;
        return 0;
    }
}
uint64_t MemoryInFiles::Impl::Resize(const uint64_t memoryBlockId, uint64_t newSize) { //returns the new size
    id_to_memoryblockinfo_map_t::iterator it = m_mapIdToMemoryBlockInfo.find(memoryBlockId);
    if (it == m_mapIdToMemoryBlockInfo.end()) {
        return 0; //fail
    }
    MemoryBlockInfo& mbi = it->second;
    return Resize(mbi, newSize);
}
uint64_t MemoryInFiles::Impl::Resize(MemoryBlockInfo& mbi, uint64_t newSize) { //returns the new size
    if (!SetupNextFileIfNeeded()) {
        return 0;
    }
    newSize = CeilToNearestBlockMultiple(newSize);
    const uint64_t fileSizeIncrease = mbi.Resize(m_currentWritingFileInfoPtr, m_nextOffsetOfCurrentFile, newSize);
    m_nextOffsetOfCurrentFile += fileSizeIncrease;
    return newSize;
}
uint64_t MemoryInFiles::Impl::GetSizeOfMemoryBlock(const uint64_t memoryBlockId) const noexcept {
    id_to_memoryblockinfo_map_t::const_iterator it = m_mapIdToMemoryBlockInfo.find(memoryBlockId);
    if (it == m_mapIdToMemoryBlockInfo.cend()) {
        return false;
    }
    const MemoryBlockInfo& mbi = it->second;
    return mbi.m_lengthAlignedToBlockSize;
}
void MemoryInFiles::Impl::ForceDeleteMemoryBlock(const uint64_t memoryBlockId) {
    if (m_mapIdToMemoryBlockInfo.erase(memoryBlockId)) {
        LOG_DEBUG(subprocess) << "Deferred delete successful of memoryBlockId=" << memoryBlockId;
    }
    else {
        LOG_ERROR(subprocess) << "Deferred delete failed of memoryBlockId=" << memoryBlockId;
    }
}
bool MemoryInFiles::Impl::DeleteMemoryBlock(const uint64_t memoryBlockId) {
    id_to_memoryblockinfo_map_t::iterator it = m_mapIdToMemoryBlockInfo.find(memoryBlockId);
    if (it == m_mapIdToMemoryBlockInfo.end()) {
        return false;
    }
    MemoryBlockInfo& mbi = it->second;
    if (mbi.m_queuedOperationsReferenceCount) {
        if (mbi.m_markedForDeletion) { //already marked for deletion (double call to DeleteMemoryBlock)
            return false;
        }
        mbi.m_markedForDeletion = true;
        LOG_DEBUG(subprocess) << "DeleteMemoryBlock called while i/o operations in progress. Deferring deletion of memoryBlockId=" << memoryBlockId;
        return true;
    }
    m_mapIdToMemoryBlockInfo.erase(it);
    return true;
}
void MemoryInFiles::Impl::AsyncDeleteMemoryBlock(const uint64_t memoryBlockId) {
    boost::asio::post(m_ioServiceRef, boost::bind(&MemoryInFiles::Impl::HandleAsyncDeleteMemoryBlock, this, memoryBlockId));
}
void MemoryInFiles::Impl::HandleAsyncDeleteMemoryBlock(const uint64_t memoryBlockId) {
    if (!DeleteMemoryBlock(memoryBlockId)) {
        LOG_ERROR(subprocess) << "Unable to async delete memoryBlockId=" << memoryBlockId;
    }
    else {
        //LOG_DEBUG(subprocess) << "Async delete successful of memoryBlockId=" << memoryBlockId;
    }
}
bool MemoryInFiles::Impl::MemoryBlockInfo::DoWriteOrRead(
    const uint64_t deferredOffset, const uint64_t deferredLength, void* readToThisLocationPtr, const void* writeFromThisLocationPtr,
    void * handlerPtrPtr)
{
    if (m_markedForDeletion) {
        LOG_ERROR(subprocess) << ((readToThisLocationPtr != NULL) ? "ReadMemoryAsync" : "WriteMemoryAsync")
            << " called on marked for deletion block with memoryBlockId = " << m_memoryBlockId;
        return false;
    }

    if ((deferredOffset + deferredLength) > m_lengthAlignedToBlockSize) {
        //LOG_ERROR(subprocess) << "out of bounds " << (deferredOffset + deferredLength) << " > " << m_lengthAlignedToBlockSize;
        return false;
    }

    //endIndex shall be treated as endIndexPlus1
    const FragmentSet::data_fragment_t fullBlock(deferredOffset, deferredOffset + deferredLength);
    FragmentSet::data_fragment_t thisFragment(0, 0);
    //LOG_DEBUG(subprocess) << "DoWriteOrRead";
    uint64_t operationPtrOffset = 0;
    for (ForwardListQueue<MemoryBlockFragmentInfo>::const_iterator it = m_memoryBlockFragmentInfoFlistQueue.cbegin(); it != m_memoryBlockFragmentInfoFlistQueue.cend(); ++it) {
        const MemoryBlockFragmentInfo& frag = *it;
        thisFragment.endIndex += frag.m_length;
        //LOG_DEBUG(subprocess) << "Frag owf=" << frag.m_baseOffsetWithinFile << " len=" << frag.m_length;
        FragmentSet::data_fragment_t overlap;
        if (thisFragment.GetOverlap(fullBlock, overlap)) {
            //LOG_DEBUG(subprocess) << "overlap fb[" << fullBlock.beginIndex << "," << fullBlock.endIndex << "] frag[" << thisFragment.beginIndex << "," << thisFragment.endIndex 
            //    << "] overlap[" << overlap.beginIndex << "," << overlap.endIndex << "]";
            const uint64_t offsetWithinFragment = (overlap.beginIndex - thisFragment.beginIndex);
            const uint64_t offsetWithinFile = frag.m_baseOffsetWithinFile + offsetWithinFragment;
            const uint64_t thisFragmentOperationLength = overlap.endIndex - overlap.beginIndex;
            if (writeFromThisLocationPtr) {
                std::shared_ptr<write_memory_handler_t>& handlerPtr = *(reinterpret_cast<std::shared_ptr<write_memory_handler_t> *>(handlerPtrPtr));
                //LOG_DEBUG(subprocess) << "Write owfile=" << offsetWithinFile << " owfrag=" << offsetWithinFragment
                //    << " oplen=" << thisFragmentOperationLength << " bi=" << overlap.beginIndex << " ei=" << overlap.endIndex;
                if (!frag.m_fileInfoPtr->WriteMemoryAsync(*this, offsetWithinFile,
                    ((const uint8_t*)writeFromThisLocationPtr) + operationPtrOffset, thisFragmentOperationLength, handlerPtr))
                {
                    return false;
                }
            }
            else {
                std::shared_ptr<read_memory_handler_t>& handlerPtr = *(reinterpret_cast<std::shared_ptr<read_memory_handler_t> *>(handlerPtrPtr));
                //LOG_DEBUG(subprocess) << "read owf=" << offsetWithinFile << " oplen=" << thisFragmentOperationLength
                //    << " bi=" << overlap.beginIndex << " ei=" << overlap.endIndex;
                if (!frag.m_fileInfoPtr->ReadMemoryAsync(*this, offsetWithinFile,
                    ((uint8_t*)readToThisLocationPtr) + operationPtrOffset, thisFragmentOperationLength, handlerPtr))
                {
                    return false;
                }
            }
            operationPtrOffset += thisFragmentOperationLength;
        }
        //else {
            //LOG_DEBUG(subprocess) << "no overlap fb[" << fullBlock.beginIndex << "," << fullBlock.endIndex << "] frag[" << thisFragment.beginIndex << "," << thisFragment.endIndex << "]";
        //}
        
        thisFragment.beginIndex = thisFragment.endIndex;
    }
  
    //old behavior before resize capability and fragment list were added:
    //const uint64_t offsetWithinFile = mbi.m_baseOffsetWithinFile + deferredWrite.offset;
    //return mbi.m_fileInfoPtr->WriteMemoryAsync(mbi, offsetWithinFile, deferredWrite.writeFromThisLocationPtr, deferredWrite.length, handlerPtr);

    return true;
}
bool MemoryInFiles::Impl::WriteMemoryAsync(const deferred_write_t& deferredWrite, std::shared_ptr<write_memory_handler_t>& handlerPtr) {
    
    id_to_memoryblockinfo_map_t::iterator it = m_mapIdToMemoryBlockInfo.find(deferredWrite.memoryBlockId);
    if (it == m_mapIdToMemoryBlockInfo.end()) {
        return false;
    }
    MemoryBlockInfo& mbi = it->second;
    return mbi.DoWriteOrRead(deferredWrite.offset, deferredWrite.length, NULL, deferredWrite.writeFromThisLocationPtr, &handlerPtr);
}
bool MemoryInFiles::Impl::WriteMemoryAsync(const std::vector<deferred_write_t>& deferredWritesVec, std::shared_ptr<write_memory_handler_t>& handlerPtr) {
    for (std::size_t i = 0; i < deferredWritesVec.size(); ++i) {
        if (!WriteMemoryAsync(deferredWritesVec[i], handlerPtr)) {
            return false;
        }
    }
    return true;
}
bool MemoryInFiles::Impl::ReadMemoryAsync(const deferred_read_t& deferredRead, std::shared_ptr<read_memory_handler_t>& handlerPtr) {
    id_to_memoryblockinfo_map_t::iterator it = m_mapIdToMemoryBlockInfo.find(deferredRead.memoryBlockId);
    if (it == m_mapIdToMemoryBlockInfo.end()) {
        return false;
    }
    MemoryBlockInfo& mbi = it->second;
    return mbi.DoWriteOrRead(deferredRead.offset, deferredRead.length, deferredRead.readToThisLocationPtr, NULL, &handlerPtr);
}
bool MemoryInFiles::Impl::ReadMemoryAsync(const std::vector<deferred_read_t>& deferredReadsVec, std::shared_ptr<read_memory_handler_t>& handlerPtr) {
    for (std::size_t i = 0; i < deferredReadsVec.size(); ++i) {
        if (!ReadMemoryAsync(deferredReadsVec[i], handlerPtr)) {
            return false;
        }
    }
    return true;
}


MemoryInFiles::MemoryInFiles(boost::asio::io_service& ioServiceRef,
    const boost::filesystem::path& rootStorageDirectory,
    const uint64_t newFileAggregationTimeMs, const uint64_t estimatedMaxAllocatedBlocks) :
    m_pimpl(boost::make_unique<MemoryInFiles::Impl>(ioServiceRef, rootStorageDirectory, newFileAggregationTimeMs, estimatedMaxAllocatedBlocks))
{}
MemoryInFiles::~MemoryInFiles() {}

uint64_t MemoryInFiles::AllocateNewWriteMemoryBlock(uint64_t totalSize) {
    return m_pimpl->AllocateNewWriteMemoryBlock(totalSize);
}
uint64_t MemoryInFiles::GetSizeOfMemoryBlock(const uint64_t memoryBlockId) const noexcept {
    return m_pimpl->GetSizeOfMemoryBlock(memoryBlockId);
}
uint64_t MemoryInFiles::Resize(const uint64_t memoryBlockId, uint64_t newSize) {
    return m_pimpl->Resize(memoryBlockId, newSize);
}
bool MemoryInFiles::DeleteMemoryBlock(const uint64_t memoryBlockId) {
    return m_pimpl->DeleteMemoryBlock(memoryBlockId);
}
void MemoryInFiles::AsyncDeleteMemoryBlock(const uint64_t memoryBlockId) {
    return m_pimpl->AsyncDeleteMemoryBlock(memoryBlockId);
}

bool MemoryInFiles::WriteMemoryAsync(const deferred_write_t& deferredWrite, const write_memory_handler_t& handler) {
    std::shared_ptr<write_memory_handler_t> hPtr = std::make_shared<write_memory_handler_t>(handler);
    return m_pimpl->WriteMemoryAsync(deferredWrite, hPtr);
}
bool MemoryInFiles::WriteMemoryAsync(const deferred_write_t& deferredWrite, write_memory_handler_t&& handler) {
    std::shared_ptr<write_memory_handler_t> hPtr = std::make_shared<write_memory_handler_t>(std::move(handler));
    return m_pimpl->WriteMemoryAsync(deferredWrite, hPtr);
}
bool MemoryInFiles::WriteMemoryAsync(const std::vector<deferred_write_t>& deferredWritesVec, const write_memory_handler_t& handler) {
    std::shared_ptr<write_memory_handler_t> hPtr = std::make_shared<write_memory_handler_t>(handler);
    return m_pimpl->WriteMemoryAsync(deferredWritesVec, hPtr);
}
bool MemoryInFiles::WriteMemoryAsync(const std::vector<deferred_write_t>& deferredWritesVec, write_memory_handler_t&& handler) {
    std::shared_ptr<write_memory_handler_t> hPtr = std::make_shared<write_memory_handler_t>(std::move(handler));
    return m_pimpl->WriteMemoryAsync(deferredWritesVec, hPtr);
}

bool MemoryInFiles::ReadMemoryAsync(const deferred_read_t& deferredRead, const read_memory_handler_t& handler) {
    std::shared_ptr<read_memory_handler_t> hPtr = std::make_shared<read_memory_handler_t>(handler);
    return m_pimpl->ReadMemoryAsync(deferredRead, hPtr);
}
bool MemoryInFiles::ReadMemoryAsync(const deferred_read_t& deferredRead, read_memory_handler_t&& handler) {
    std::shared_ptr<read_memory_handler_t> hPtr = std::make_shared<read_memory_handler_t>(std::move(handler));
    return m_pimpl->ReadMemoryAsync(deferredRead, hPtr);
}
bool MemoryInFiles::ReadMemoryAsync(const std::vector<deferred_read_t>& deferredReadsVec, const read_memory_handler_t& handler) {
    std::shared_ptr<read_memory_handler_t> hPtr = std::make_shared<read_memory_handler_t>(handler);
    return m_pimpl->ReadMemoryAsync(deferredReadsVec, hPtr);
}
bool MemoryInFiles::ReadMemoryAsync(const std::vector<deferred_read_t>& deferredReadsVec, read_memory_handler_t&& handler) {
    std::shared_ptr<read_memory_handler_t> hPtr = std::make_shared<read_memory_handler_t>(std::move(handler));
    return m_pimpl->ReadMemoryAsync(deferredReadsVec, hPtr);
}

uint64_t MemoryInFiles::GetCountTotalFilesCreated() const {
    return m_pimpl->m_countTotalFilesCreated;
}
uint64_t MemoryInFiles::GetCountTotalFilesActive() const {
    return m_pimpl->m_countTotalFilesActive;
}
