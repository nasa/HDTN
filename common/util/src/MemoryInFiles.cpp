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
#include <boost/thread.hpp>
#include <memory>
#include <unordered_map>
#include "Logger.h"
#include <boost/make_unique.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/format.hpp>
#ifdef _WIN32
#include <windows.h> //must be included after boost
#endif

static constexpr hdtn::Logger::SubProcess subprocess = hdtn::Logger::SubProcess::none;

struct MemoryInFiles::Impl : private boost::noncopyable {

    Impl() = delete;
    Impl(boost::asio::io_service& ioServiceRef,
        const boost::filesystem::path& workingDirectory, const uint64_t newFileAggregationTimeMs);
    ~Impl();
    void Stop();
    uint64_t AllocateNewWriteMemoryBlock(std::size_t totalSize);
    bool WriteMemoryAsync(const uint64_t memoryBlockId, const uint64_t offset, const void* data, std::size_t size, write_memory_handler_t&& handler);
    bool ReadMemoryAsync(const uint64_t memoryBlockId, const uint64_t offset, void* data, std::size_t size, read_memory_handler_t&& handler);

private:

#ifdef _WIN32
    typedef boost::asio::windows::random_access_handle file_handle_t;
#else
    typedef boost::asio::posix::stream_descriptor file_handle_t;
#endif
    struct FileInfo : private boost::noncopyable {
        FileInfo() = delete;
        FileInfo(const boost::filesystem::path& filePath, boost::asio::io_service & ioServiceRef);
        ~FileInfo();
        bool WriteMemoryAsync(const uint64_t offset, const void* data, std::size_t size, write_memory_handler_t&& handler);
        bool ReadMemoryAsync(const uint64_t offset, void* data, std::size_t size, read_memory_handler_t&& handler);
    private:
        void HandleDiskWriteCompleted(const boost::system::error_code& error, std::size_t bytes_transferred,
            const write_memory_handler_t& handler);
        void HandleDiskReadCompleted(const boost::system::error_code& error, std::size_t bytes_transferred,
            const read_memory_handler_t& handler);

        std::unique_ptr<file_handle_t> m_fileHandlePtr;
        boost::filesystem::path m_filePath;
        bool m_diskOperationInProgress;
        bool m_valid;
    };
    struct MemoryBlockInfo : private boost::noncopyable {
        MemoryBlockInfo() = delete;
        MemoryBlockInfo(const std::shared_ptr<FileInfo>& fileInfoPtr, const uint64_t baseOffsetWithinFile, const std::size_t length);
        std::shared_ptr<FileInfo> m_fileInfoPtr;
        const uint64_t m_baseOffsetWithinFile;
        const std::size_t m_length;
    };
    typedef std::unordered_map<uint64_t, MemoryBlockInfo> id_to_memoryblockinfo_map_t;

    


    id_to_memoryblockinfo_map_t m_mapIdToMemoryBlockInfo;
    std::shared_ptr<FileInfo> m_currentWritingFileInfoPtr;

    boost::asio::io_service& m_ioServiceRef;
    const boost::filesystem::path m_rootStorageDirectory;
    const uint64_t m_newFileAggregationTimeMs;
    uint64_t m_nextMemoryBlockId;
    uint64_t m_nextFileId;
    uint64_t m_nextOffsetOfCurrentFile;
};

MemoryInFiles::Impl::FileInfo::FileInfo(const boost::filesystem::path& filePath, boost::asio::io_service& ioServiceRef) :
    m_filePath(filePath),
    m_diskOperationInProgress(false),
    m_valid(false)
{
    const boost::filesystem::path::value_type* filePathCstr = m_filePath.c_str();
#ifdef _WIN32
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
    
#else
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
    if (m_fileHandlePtr) {
        m_fileHandlePtr->close();
        m_fileHandlePtr.reset(); //delete it
    }
    if (!boost::filesystem::remove(m_filePath)) {
        LOG_ERROR(subprocess) << "error deleting file " << m_filePath;
    }
}
bool MemoryInFiles::Impl::FileInfo::WriteMemoryAsync(const uint64_t offset, const void* data, std::size_t size, write_memory_handler_t&& handler) {
    if ((!m_valid) || m_diskOperationInProgress) {
        return false;
    }
    m_diskOperationInProgress = true;


#ifdef _WIN32
    boost::asio::async_write_at(*m_fileHandlePtr, offset,
#else
    lseek64(m_asioHandlePtrsVec[diskId]->native_handle(), offset, SEEK_SET);
    boost::asio::async_write(*m_fileHandlePtr,
#endif
        boost::asio::buffer(data, size),
        boost::bind(&MemoryInFiles::Impl::FileInfo::HandleDiskWriteCompleted, this,
            boost::asio::placeholders::error,
            boost::asio::placeholders::bytes_transferred,
            std::move(handler)));
    return true;
}
void MemoryInFiles::Impl::FileInfo::HandleDiskWriteCompleted(const boost::system::error_code& error, std::size_t bytes_transferred,
    const write_memory_handler_t& handler)
{
    m_diskOperationInProgress = false;
    if (error) {
        LOG_ERROR(subprocess) << "HandleDiskWriteCompleted: " << error.message();
    }
    else {
        if (handler) {
            handler();
        }
    }
}

bool MemoryInFiles::Impl::FileInfo::ReadMemoryAsync(const uint64_t offset, void* data, std::size_t size, read_memory_handler_t&& handler)
{
    if ((!m_valid) || m_diskOperationInProgress) {
        return false;
    }
    m_diskOperationInProgress = true;


#ifdef _WIN32
    boost::asio::async_read_at(*m_fileHandlePtr, offset,
#else
    lseek64(m_asioHandlePtrsVec[diskId]->native_handle(), offset, SEEK_SET);
    boost::asio::async_read(*m_fileHandlePtr,
#endif
        boost::asio::buffer(data, size),
        boost::bind(&MemoryInFiles::Impl::FileInfo::HandleDiskReadCompleted, this,
            boost::asio::placeholders::error,
            boost::asio::placeholders::bytes_transferred,
            std::move(handler)));
    return true;
}
void MemoryInFiles::Impl::FileInfo::HandleDiskReadCompleted(const boost::system::error_code& error, std::size_t bytes_transferred,
    const read_memory_handler_t& handler)
{
    m_diskOperationInProgress = false;
    bool success = true;
    if (error) {
        LOG_ERROR(subprocess) << "HandleDiskWriteCompleted: " << error.message();
        success = false;
    }

    if (handler) {
        handler(success);
    }
}

MemoryInFiles::Impl::MemoryBlockInfo::MemoryBlockInfo(
    const std::shared_ptr<FileInfo>& fileInfoPtr,
    const uint64_t baseOffsetWithinFile, const std::size_t length) :
    m_fileInfoPtr(fileInfoPtr),
    m_baseOffsetWithinFile(baseOffsetWithinFile),
    m_length(length)
{

}

MemoryInFiles::Impl::Impl(boost::asio::io_service& ioServiceRef,
    const boost::filesystem::path& workingDirectory, const uint64_t newFileAggregationTimeMs) :
    m_ioServiceRef(ioServiceRef),
    m_rootStorageDirectory(workingDirectory / boost::filesystem::unique_path()),
    m_newFileAggregationTimeMs(newFileAggregationTimeMs),
    m_nextMemoryBlockId(1), //0 reserved for error
    m_nextFileId(0),
    m_nextOffsetOfCurrentFile(0)
{
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
    m_mapIdToMemoryBlockInfo.clear();
    m_currentWritingFileInfoPtr.reset();

    if (boost::filesystem::is_directory(m_rootStorageDirectory)) {
        if (!boost::filesystem::remove_all(m_rootStorageDirectory)) {
            LOG_ERROR(subprocess) << "Unable to remove directory " << m_rootStorageDirectory;
        }
    }
}

uint64_t MemoryInFiles::Impl::AllocateNewWriteMemoryBlock(std::size_t totalSize) {
    if (!m_currentWritingFileInfoPtr) {
        m_nextOffsetOfCurrentFile = 0;
        static const boost::format fmtTemplate("ltp_%09d.bin");
        boost::format fmt(fmtTemplate);
        fmt % m_nextFileId++;
        const boost::filesystem::path fullFilePath = m_rootStorageDirectory / boost::filesystem::path(fmt.str());

        if (boost::filesystem::exists(fullFilePath)) {
            LOG_ERROR(subprocess) << "MemoryInFiles::Impl::WriteMemoryAsync: " << fullFilePath << " already exists";
            return 0;
        }
        m_currentWritingFileInfoPtr = std::make_shared<FileInfo>(fullFilePath, m_ioServiceRef);
    }
    const uint64_t memoryBlockId = m_nextMemoryBlockId++;

    //try_emplace does not work with piecewise_construct
    std::pair<id_to_memoryblockinfo_map_t::iterator, bool> ret = m_mapIdToMemoryBlockInfo.emplace(
            std::piecewise_construct,
            std::forward_as_tuple(memoryBlockId),
            std::forward_as_tuple(m_currentWritingFileInfoPtr, m_nextOffsetOfCurrentFile, totalSize));
    //(true if insertion happened, false if it did not).
    if (ret.second) {
        m_nextOffsetOfCurrentFile += totalSize;
        return memoryBlockId;
    }
    else {
        //LOG_ERROR(subprocess) << "Unable to insert memoryBlockId " << memoryBlockId;
        return 0;
    }
}
bool MemoryInFiles::Impl::WriteMemoryAsync(const uint64_t memoryBlockId, const uint64_t offset, const void* data, std::size_t size, write_memory_handler_t&& handler) {
    
    id_to_memoryblockinfo_map_t::iterator it = m_mapIdToMemoryBlockInfo.find(memoryBlockId);
    if (it == m_mapIdToMemoryBlockInfo.end()) {
        return false;
    }
    MemoryBlockInfo& mbi = it->second;
    if ((offset + size) > mbi.m_length) {
        return false;
    }
    return mbi.m_fileInfoPtr->WriteMemoryAsync(offset, data, size, std::move(handler));
}
bool MemoryInFiles::Impl::ReadMemoryAsync(const uint64_t memoryBlockId, const uint64_t offset, void* data, std::size_t size, read_memory_handler_t&& handler) {
    id_to_memoryblockinfo_map_t::iterator it = m_mapIdToMemoryBlockInfo.find(memoryBlockId);
    if (it == m_mapIdToMemoryBlockInfo.end()) {
        return false;
    }
    MemoryBlockInfo& mbi = it->second;
    return mbi.m_fileInfoPtr->ReadMemoryAsync(offset, data, size, std::move(handler));
}


MemoryInFiles::MemoryInFiles(boost::asio::io_service& ioServiceRef,
    const boost::filesystem::path& rootStorageDirectory, const uint64_t newFileAggregationTimeMs) :
    m_pimpl(boost::make_unique<MemoryInFiles::Impl>(ioServiceRef, rootStorageDirectory, newFileAggregationTimeMs))
{}
MemoryInFiles::~MemoryInFiles() {}

uint64_t MemoryInFiles::AllocateNewWriteMemoryBlock(std::size_t totalSize) {
    return m_pimpl->AllocateNewWriteMemoryBlock(totalSize);
}

bool MemoryInFiles::WriteMemoryAsync(const uint64_t memoryBlockId, const uint64_t offset, const void* data, std::size_t size, const write_memory_handler_t& handler) {
    return m_pimpl->WriteMemoryAsync(memoryBlockId, offset, data, size, std::move(write_memory_handler_t(handler)));
}
bool MemoryInFiles::WriteMemoryAsync(const uint64_t memoryBlockId, const uint64_t offset, const void* data, std::size_t size, write_memory_handler_t&& handler) {
    return m_pimpl->WriteMemoryAsync(memoryBlockId, offset, data, size, std::move(handler));
}

bool MemoryInFiles::ReadMemoryAsync(const uint64_t memoryBlockId, const uint64_t offset, void* data, std::size_t size, const read_memory_handler_t& handler) {
    return m_pimpl->ReadMemoryAsync(memoryBlockId, offset, data, size, std::move(read_memory_handler_t(handler)));
}
bool MemoryInFiles::ReadMemoryAsync(const uint64_t memoryBlockId, const uint64_t offset, void* data, std::size_t size, read_memory_handler_t&& handler) {
    return m_pimpl->ReadMemoryAsync(memoryBlockId, offset, data, size, std::move(handler));
}

