/**
 * @file BpFileTransfer.cpp
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
 */

#include <string.h>
#include "BpFileTransfer.h"
#include "Logger.h"
#include <boost/foreach.hpp>
#include <boost/make_unique.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/endian/conversion.hpp>
#include "ThreadNamer.h"
#include "Utf8Paths.h"
#include "DirectoryScanner.h"
#include <queue>
#include "FragmentSet.h"
#include "codec/Bpv7Crc.h"

//static const uint64_t MAX_FILE_SIZE_BYTES = 8000000000; //8GB

static constexpr hdtn::Logger::SubProcess subprocess = hdtn::Logger::SubProcess::none;

static bool CreateDirectoryRecursivelyVerboseIfNotExist(const boost::filesystem::path& path) {
    if (!boost::filesystem::is_directory(path)) {
        LOG_INFO(subprocess) << "directory does not exist.. creating directory recursively..";
        try {
            if (boost::filesystem::create_directories(path)) {
                LOG_INFO(subprocess) << "successfully created directory";
            }
            else {
                LOG_ERROR(subprocess) << "unable to create directory";
                return false;
            }
        }
        catch (const boost::system::system_error& e) {
            LOG_ERROR(subprocess) << e.what() << "..unable to create directory";
            return false;
        }
    }
    return true;
}

enum class SendFileMessageType : uint8_t {
    FILE_DATA = 0,
    FILE_ACK
};

struct SendFileMetadata {
    SendFileMetadata();
    void ToLittleEndianInplace();
    void ToNativeEndianInplace();
    uint64_t totalFileSize;
    uint64_t fragmentOffset;
    uint32_t fragmentLength;
    uint32_t crc32c; //0 if not the last fragment
    uint8_t pathLen;
    static constexpr std::size_t SendFileMetadataLength = 25;
};

enum class FileAckResponseCodes : uint8_t {
    FILE_RECEIVED_CRC_MATCH = 0,
    FILE_RECEIVED_ZERO_LENGTH,
    FILE_RECEIVED_CRC_MISMATCH,
    DUPLICATE_FILENAME,
    SAVE_ERROR,
    FILE_TOO_LARGE_ERROR,
    FILE_FRAGMENT_MALFORMED_ERROR,
    NUM_FILE_ACK_RESPONSE_CODES
};
static std::string FileAckResponseCodesToString(FileAckResponseCodes code) {
    static const std::string ToString[(std::size_t)FileAckResponseCodes::NUM_FILE_ACK_RESPONSE_CODES] = {
        "file received successfully (crc matched)",
        "file received successfully (zero length)",
        "error: file had a crc mismatch",
        "error: duplicate filename",
        "error: file cannot be saved to disk",
        "error: file is too large",
        "error: file fragment is malformed"
    };
    if (code < FileAckResponseCodes::NUM_FILE_ACK_RESPONSE_CODES) {
        return ToString[(std::size_t)code];
    }
    return "invalid FileAckResponseCode received";
}
static bool FileAckResponseCodeIsSuccess(FileAckResponseCodes code) {
    return (code <= FileAckResponseCodes::FILE_RECEIVED_ZERO_LENGTH);
}


struct FileAckMetadata {
    FileAckResponseCodes responseCode;
    uint8_t pathLen;
    static constexpr std::size_t FileAckMetadataLength = 2;
};

class BpFileTransfer::Impl : public BpSourcePattern {

public:
    Impl(const boost::filesystem::path& fileOrFolderPathToSend,
        const boost::filesystem::path& saveDirectory,
        uint64_t maxBundleSizeBytes,
        bool uploadExistingFiles,
        bool uploadNewFiles,
        unsigned int recurseDirectoriesDepth,
        const uint64_t maxRxFileSizeBytes);
    virtual ~Impl() override;

    bool InitWebServer(const WebsocketServer::ProgramOptions& websocketServerProgramOptions);

    
    struct write_info_t {
        write_info_t() : expectedCrc(0), expectedFileSize(0), errorOccurred(false) {}
        FragmentSet::data_fragment_set_t fragmentSet;
        std::unique_ptr<boost::filesystem::ofstream> ofstreamPtr;
        Crc32c_ReceiveOutOfOrderChunks crc32cRxOutOfOrderChunks;
        uint32_t expectedCrc;
        uint64_t expectedFileSize;
        bool errorOccurred;
    };
    typedef std::map<boost::filesystem::path, write_info_t> filename_to_writeinfo_map_t;

    std::size_t GetNumberOfFilesToSend() const;

protected:
    virtual uint64_t GetNextPayloadLength_Step1() override;
    virtual bool CopyPayload_Step2(uint8_t* destinationBuffer) override;
    virtual bool TryWaitForDataAvailable(const boost::posix_time::time_duration& timeout) override;
    virtual bool ProcessNonAdminRecordBundlePayload(const uint8_t* data, const uint64_t size) override;
private:
    void QueueFileAckToSend(FileAckResponseCodes responseCode, const std::string& encodedRelativePathNameAsUtf8String);
    //void TryDequeueFileAckToSend(FileAckResponseCodes& responseCode, std::string& encodedRelativePathNameAsUtf8String);
    void TryDequeueFileAckToSend(std::vector<uint8_t>& fileAckVecToSend);
    void Shutdown_NotThreadSafe();
    void OnNewWebsocketConnectionCallback(WebsocketServer::Connection& conn);
    bool OnNewWebsocketTextDataReceivedCallback(WebsocketServer::Connection& conn, std::string& receivedString);
private:
    const uint64_t M_MAX_BUNDLE_PAYLOAD_SIZE_BYTES;
    const uint64_t M_MAX_RX_FILE_SIZE_BYTES;

    //send file (unbuffered mode)
    std::unique_ptr<boost::filesystem::ifstream> m_currentIfstreamPtr;
    boost::filesystem::path m_currentFilePathAbsolute;
    boost::filesystem::path m_currentFilePathRelative;
    std::string m_currentFilePathRelativeAsUtf8String;
    std::string m_currentFilePathRelativeAsPrintableString;
    SendFileMetadata m_currentSendFileMetadata;
    Crc32c_InOrderChunks m_currentSendFileCrc32c;
    boost::asio::io_service m_ioService; //for DirectoryScanner
    std::unique_ptr<boost::thread> m_ioServiceThreadPtr;
    std::unique_ptr<DirectoryScanner> m_directoryScannerPtr;

    //receive file
    boost::filesystem::path m_saveDirectory;
    filename_to_writeinfo_map_t m_filenameToWriteInfoMap;
    std::queue<std::vector<uint8_t> > m_fileAcksToSendQueue;
    boost::mutex m_fileAcksToSendMutex;
    boost::condition_variable m_fileAcksToSendCv;
    std::vector<uint8_t> m_currentFileAckVecToSend;


    WebsocketServer m_websocketServer;

    std::queue<std::vector<uint8_t> > m_commandsToSendQueue;
    boost::mutex m_commandsToSendMutex;
    boost::condition_variable m_commandsToSendCv;
};

BpFileTransfer::BpFileTransfer(const boost::filesystem::path& fileOrFolderPathToSend,
    const boost::filesystem::path& saveDirectory,
    uint64_t maxBundleSizeBytes,
    bool uploadExistingFiles,
    bool uploadNewFiles,
    unsigned int recurseDirectoriesDepth,
    const uint64_t maxRxFileSizeBytes) :
    m_pimpl(boost::make_unique<BpFileTransfer::Impl>(fileOrFolderPathToSend, saveDirectory,
        maxBundleSizeBytes, uploadExistingFiles, uploadNewFiles, recurseDirectoriesDepth, maxRxFileSizeBytes)) {}

BpFileTransfer::~BpFileTransfer() {
    Stop();
}
void BpFileTransfer::Stop() {
    m_pimpl->Stop();

    // stop websocket after thread
    //if (m_websocketServerPtr) {
    //    m_websocketServerPtr->Stop();
    //    m_websocketServerPtr.reset();
    //}
}
void BpFileTransfer::Start(OutductsConfig_ptr& outductsConfigPtr, InductsConfig_ptr& inductsConfigPtr,
    const boost::filesystem::path& bpSecConfigFilePath,
    bool custodyTransferUseAcs,
    const cbhe_eid_t& myEid, double bundleRate, const cbhe_eid_t& finalDestEid, const uint64_t myCustodianServiceId,
    const unsigned int bundleSendTimeoutSeconds, const uint64_t bundleLifetimeMilliseconds, const uint64_t bundlePriority,
    const bool requireRxBundleBeforeNextTx, const bool forceDisableCustody, const bool useBpVersion7,
    const uint64_t claRate, const WebsocketServer::ProgramOptions& websocketServerProgramOptions)
{

    m_pimpl->InitWebServer(websocketServerProgramOptions);
    m_pimpl->Start(outductsConfigPtr, inductsConfigPtr,
        bpSecConfigFilePath,
        custodyTransferUseAcs,
        myEid, bundleRate, finalDestEid, myCustodianServiceId,
        bundleSendTimeoutSeconds, bundleLifetimeMilliseconds, bundlePriority,
        requireRxBundleBeforeNextTx, forceDisableCustody, useBpVersion7,
        claRate);
}

bool BpFileTransfer::Impl::InitWebServer(const WebsocketServer::ProgramOptions& websocketServerProgramOptions) {
    if (!m_websocketServer.Init(websocketServerProgramOptions,
        boost::bind(&BpFileTransfer::Impl::OnNewWebsocketConnectionCallback, this, boost::placeholders::_1),
        boost::bind(&BpFileTransfer::Impl::OnNewWebsocketTextDataReceivedCallback, this,
            boost::placeholders::_1, boost::placeholders::_2)))
    {
        LOG_FATAL(subprocess) << "Error in BpFileTransfer Init: cannot init websocket server";
        return false;
    }
    return true;
}



SendFileMetadata::SendFileMetadata() : totalFileSize(0), fragmentOffset(0), fragmentLength(0), crc32c(0), pathLen(0) {}
void SendFileMetadata::ToLittleEndianInplace() {
    boost::endian::native_to_little_inplace(totalFileSize);
    boost::endian::native_to_little_inplace(fragmentOffset);
    boost::endian::native_to_little_inplace(fragmentLength);
    boost::endian::native_to_little_inplace(crc32c);
}
void SendFileMetadata::ToNativeEndianInplace() {
    boost::endian::little_to_native_inplace(totalFileSize);
    boost::endian::little_to_native_inplace(fragmentOffset);
    boost::endian::little_to_native_inplace(fragmentLength);
    boost::endian::little_to_native_inplace(crc32c);
}

BpFileTransfer::Impl::Impl(const boost::filesystem::path& fileOrFolderPathToSend,
    const boost::filesystem::path& saveDirectory,
    uint64_t maxBundleSizeBytes,
    bool uploadExistingFiles,
    bool uploadNewFiles,
    unsigned int recurseDirectoriesDepth,
    const uint64_t maxRxFileSizeBytes) :
    BpSourcePattern(),
    M_MAX_BUNDLE_PAYLOAD_SIZE_BYTES(maxBundleSizeBytes),
    M_MAX_RX_FILE_SIZE_BYTES(maxRxFileSizeBytes),
    m_directoryScannerPtr(fileOrFolderPathToSend.empty() ?
        std::unique_ptr<DirectoryScanner>() :
        boost::make_unique<DirectoryScanner>(fileOrFolderPathToSend, uploadExistingFiles, uploadNewFiles, recurseDirectoriesDepth, m_ioService, 3000)),
    m_saveDirectory(saveDirectory)
{
    //sending files
    if (!m_directoryScannerPtr) {
        LOG_INFO(subprocess) << "not sending files";
    }
    else {
        if (uploadNewFiles) {
            m_ioServiceThreadPtr = boost::make_unique<boost::thread>(boost::bind(&boost::asio::io_service::run, &m_ioService));
            ThreadNamer::SetIoServiceThreadName(m_ioService, "ioServiceBpSendFile");
        }

        if ((m_directoryScannerPtr->GetNumberOfFilesToSend() == 0) && (!uploadNewFiles)) {
            LOG_WARNING(subprocess) << "no files to send";
        }
        else {
            LOG_INFO(subprocess) << "sending " << m_directoryScannerPtr->GetNumberOfFilesToSend() << " files now, monitoring "
                << m_directoryScannerPtr->GetNumberOfCurrentlyMonitoredDirectories() << " directories";
        }
    }

    //receiving files
    if (m_saveDirectory.empty()) {
        LOG_INFO(subprocess) << "not saving received files";
    }
    else {
        LOG_INFO(subprocess) << "saving files to directory: " << m_saveDirectory;
        if (!CreateDirectoryRecursivelyVerboseIfNotExist(m_saveDirectory)) {
            LOG_ERROR(subprocess) << "cannot create save directory: " << m_saveDirectory << " ..not saving files";
            m_saveDirectory.clear();
        }
    }
}

BpFileTransfer::Impl::~Impl() {
    //m_ioService.stop(); //will hang ~m_ioService, delete the directory scanner object to stop it instead
    if (m_ioServiceThreadPtr) {
        boost::asio::post(m_ioService, boost::bind(&BpFileTransfer::Impl::Shutdown_NotThreadSafe, this));
        m_ioServiceThreadPtr->join();
        m_ioServiceThreadPtr.reset(); //delete it
    }
}
void BpFileTransfer::Impl::Shutdown_NotThreadSafe() { //call within m_ioService thread
    m_directoryScannerPtr.reset(); //delete it
}

std::size_t BpFileTransfer::Impl::GetNumberOfFilesToSend() const {
    return (m_directoryScannerPtr) ? m_directoryScannerPtr->GetNumberOfFilesToSend() : 0;
}

uint64_t BpFileTransfer::Impl::GetNextPayloadLength_Step1() {
    //acks highest priority
    TryDequeueFileAckToSend(m_currentFileAckVecToSend);
    if (m_currentFileAckVecToSend.size()) {
        return m_currentFileAckVecToSend.size();
    }

    if (!m_directoryScannerPtr) {
        return UINT64_MAX; //pending criteria
    }
    if (m_currentFilePathAbsolute.empty()) {
        if (!m_directoryScannerPtr->GetNextFilePath(m_currentFilePathAbsolute, m_currentFilePathRelative)) {
            if (m_directoryScannerPtr->GetNumberOfCurrentlyMonitoredDirectories()) {
                return UINT64_MAX; //pending criteria
            }
            else {
                return 0; //stopping criteria
            }
        }
    }
    if (!m_currentIfstreamPtr) {
        m_currentIfstreamPtr = boost::make_unique<boost::filesystem::ifstream>(m_currentFilePathAbsolute, std::ifstream::in | std::ifstream::binary);
        if (m_currentIfstreamPtr->good()) {
            //path name shall be utf-8 encoded
            m_currentFilePathRelativeAsUtf8String = Utf8Paths::PathToUtf8String(m_currentFilePathRelative);
            m_currentFilePathRelativeAsPrintableString = (Utf8Paths::IsAscii(m_currentFilePathRelativeAsUtf8String)) ?
                m_currentFilePathRelativeAsUtf8String : std::string("UTF-8-non-printable-file-name");
            if (m_currentFilePathRelativeAsUtf8String.size() > std::numeric_limits<uint8_t>::max()) {
                LOG_ERROR(subprocess) << "Path " << m_currentFilePathRelativeAsPrintableString
                    << " exceeds max length of " << (int)std::numeric_limits<uint8_t>::max();
                return 0;
            }
            // get length of file:
            m_currentIfstreamPtr->seekg(0, m_currentIfstreamPtr->end);
            m_currentSendFileMetadata.totalFileSize = m_currentIfstreamPtr->tellg();
            m_currentIfstreamPtr->seekg(0, m_currentIfstreamPtr->beg);
            m_currentSendFileMetadata.fragmentOffset = 0;
            m_currentSendFileMetadata.pathLen = static_cast<uint8_t>(m_currentFilePathRelativeAsUtf8String.size());
            m_currentSendFileCrc32c.Reset();
            LOG_INFO(subprocess) << "Sending: " << m_currentFilePathRelativeAsPrintableString;
        }
        else { //file error occurred.. stop
            LOG_ERROR(subprocess) << "Failed to read " << m_currentFilePathRelativeAsPrintableString << " : error was : " << std::strerror(errno);
            return 0;
        }
    }
    m_currentSendFileMetadata.fragmentLength = static_cast<uint32_t>(std::min(m_currentSendFileMetadata.totalFileSize - m_currentSendFileMetadata.fragmentOffset, M_MAX_BUNDLE_PAYLOAD_SIZE_BYTES));
    return m_currentSendFileMetadata.fragmentLength + SendFileMetadata::SendFileMetadataLength + sizeof(SendFileMessageType) + m_currentSendFileMetadata.pathLen;
}
bool BpFileTransfer::Impl::CopyPayload_Step2(uint8_t * destinationBuffer) {
    //acks highest priority
    if (m_currentFileAckVecToSend.size()) {
        //already included in m_currentFileAckVecToSend: *destinationBuffer++ = (uint8_t)SendFileMessageType::FILE_ACK;
        memcpy(destinationBuffer, m_currentFileAckVecToSend.data(), m_currentFileAckVecToSend.size());
        return true;
    }

    *destinationBuffer++ = (uint8_t)SendFileMessageType::FILE_DATA;
    m_currentSendFileMetadata.ToLittleEndianInplace();
    memcpy(destinationBuffer, &m_currentSendFileMetadata, SendFileMetadata::SendFileMetadataLength);
    m_currentSendFileMetadata.ToNativeEndianInplace();
    SendFileMetadata* metaPtr = (SendFileMetadata*)destinationBuffer;
    destinationBuffer += SendFileMetadata::SendFileMetadataLength;
    memcpy(destinationBuffer, m_currentFilePathRelativeAsUtf8String.data(), m_currentFilePathRelativeAsUtf8String.size());
    destinationBuffer += m_currentSendFileMetadata.pathLen;
    // read data as a block:
    m_currentIfstreamPtr->read((char*)destinationBuffer, m_currentSendFileMetadata.fragmentLength);
    if (!*m_currentIfstreamPtr) {
        LOG_ERROR(subprocess) << "only " << m_currentIfstreamPtr->gcount() << " out of " << m_currentSendFileMetadata.fragmentLength << " bytes could be read";
        return false;
    }
    m_currentSendFileCrc32c.AddUnalignedBytes(destinationBuffer, m_currentSendFileMetadata.fragmentLength);
    const uint64_t nextOffset = m_currentSendFileMetadata.fragmentLength + m_currentSendFileMetadata.fragmentOffset;
    const bool isEndOfThisFile = (nextOffset == m_currentSendFileMetadata.totalFileSize);
    if (isEndOfThisFile) {
        m_currentIfstreamPtr->close();
        m_currentIfstreamPtr.reset();
        m_currentFilePathAbsolute.clear();
        m_currentFilePathRelative.clear();
        uint32_t crcFinal = m_currentSendFileCrc32c.FinalizeAndGet();
        boost::endian::native_to_little_inplace(crcFinal);
        memcpy(&metaPtr->crc32c, &crcFinal, sizeof(crcFinal));
    }
    else {
        m_currentSendFileMetadata.fragmentOffset = nextOffset;
    }
    
    return true;
}

bool BpFileTransfer::Impl::TryWaitForDataAvailable(const boost::posix_time::time_duration& timeout) {
    {
        boost::mutex::scoped_lock lock(m_fileAcksToSendMutex);
        if (!m_fileAcksToSendQueue.empty()) {
            return true;
        }
        if (!m_directoryScannerPtr) { //is null
            m_fileAcksToSendCv.timed_wait(lock, timeout);
            return !m_fileAcksToSendQueue.empty();
        }
    }
    if (m_currentFilePathAbsolute.empty()) {
        if (m_directoryScannerPtr->GetNextFilePathTimeout(m_currentFilePathAbsolute, m_currentFilePathRelative, timeout)) {
            return true;
        }
        boost::mutex::scoped_lock lock(m_fileAcksToSendMutex);
        return !m_fileAcksToSendQueue.empty();
    }
    return true;
}

static bool IsFileFullyReceived(FragmentSet::data_fragment_set_t& fragmentSet, const uint64_t totalFileSize) {
    if (fragmentSet.size() != 1) {
        return false;
    }
    const FragmentSet::data_fragment_t& df = *fragmentSet.begin();
    return ((df.beginIndex == 0) && (df.endIndex == (totalFileSize - 1)));
}

bool BpFileTransfer::Impl::ProcessNonAdminRecordBundlePayload(const uint8_t* data, const uint64_t size) {
    uint64_t bufSizeRemaining = size;
    if (bufSizeRemaining == 0) {
        return false;
    }
    SendFileMessageType type = (SendFileMessageType)(*data++);
    --bufSizeRemaining;
    if (type == SendFileMessageType::FILE_DATA) {
        SendFileMetadata sendFileMetadata;
        if (bufSizeRemaining < SendFileMetadata::SendFileMetadataLength) {
            LOG_ERROR(subprocess) << "not enough data in bundle for file metadata";
            return false;
        }
        memcpy(&sendFileMetadata, data, SendFileMetadata::SendFileMetadataLength);
        data += SendFileMetadata::SendFileMetadataLength;
        bufSizeRemaining -= SendFileMetadata::SendFileMetadataLength;
        sendFileMetadata.ToNativeEndianInplace();
        
        const uint64_t fragmentOffsetPlusFragmentLength = sendFileMetadata.fragmentOffset + sendFileMetadata.fragmentLength;
        
        if (bufSizeRemaining < sendFileMetadata.pathLen) {
            LOG_ERROR(subprocess) << "not enough data in bundle for file path length";
            return false;
        }
        
        const std::string encodedRelativePathNameAsUtf8String(data, data + sendFileMetadata.pathLen);
        const boost::filesystem::path relativeFilePath(Utf8Paths::Utf8StringToPath(encodedRelativePathNameAsUtf8String));
        boost::filesystem::path fullFilePath = m_saveDirectory / relativeFilePath;
        const std::string encodedFullPathNameAsUtf8String(Utf8Paths::PathToUtf8String(fullFilePath));
        const std::string printableFullPathString = (Utf8Paths::IsAscii(encodedFullPathNameAsUtf8String)) ?
            encodedFullPathNameAsUtf8String : std::string("UTF-8-non-printable-file-name");
        data += sendFileMetadata.pathLen;
        bufSizeRemaining -= sendFileMetadata.pathLen;
        
        write_info_t& writeInfo = m_filenameToWriteInfoMap[relativeFilePath];
        if (writeInfo.errorOccurred) {
            return false; //prevent duplicate prints and error acks and trying to operate on this failed file
        }
        bool doWriteFragment = false;
        if (bufSizeRemaining != sendFileMetadata.fragmentLength) {
            if (!writeInfo.errorOccurred) {
                writeInfo.errorOccurred = true;
                QueueFileAckToSend(FileAckResponseCodes::FILE_FRAGMENT_MALFORMED_ERROR, encodedRelativePathNameAsUtf8String);
                LOG_ERROR(subprocess) << "malformed fragment: bundle payload length not match file fragmentLength";
            }
            return false;
        }
        if (sendFileMetadata.totalFileSize > M_MAX_RX_FILE_SIZE_BYTES) {
            if (!writeInfo.errorOccurred) {
                writeInfo.errorOccurred = true;
                QueueFileAckToSend(FileAckResponseCodes::FILE_TOO_LARGE_ERROR, encodedRelativePathNameAsUtf8String);
                LOG_ERROR(subprocess) << "file exceeds " << M_MAX_RX_FILE_SIZE_BYTES << " bytes";
            }
            return false;
        }
        if (fragmentOffsetPlusFragmentLength > sendFileMetadata.totalFileSize) {
            if (!writeInfo.errorOccurred) {
                writeInfo.errorOccurred = true;
                QueueFileAckToSend(FileAckResponseCodes::FILE_FRAGMENT_MALFORMED_ERROR, encodedRelativePathNameAsUtf8String);
                LOG_ERROR(subprocess) << "malformed fragment: fragmentOffsetPlusFragmentLength exceeds " << sendFileMetadata.totalFileSize << " bytes";
            }
            return false;
        }

        if (sendFileMetadata.totalFileSize == 0) { //0 length file, create an empty file
            if (writeInfo.fragmentSet.empty() && m_saveDirectory.size()) {

                if (!CreateDirectoryRecursivelyVerboseIfNotExist(fullFilePath.parent_path())) {
                    if (!writeInfo.errorOccurred) {
                        writeInfo.errorOccurred = true;
                        QueueFileAckToSend(FileAckResponseCodes::SAVE_ERROR, encodedRelativePathNameAsUtf8String);
                        LOG_ERROR(subprocess) << "save error: unable to create directory structure for saving file " << printableFullPathString;
                    }
                    return false;
                }
                if (boost::filesystem::is_regular_file(fullFilePath)) {
                    if (!writeInfo.errorOccurred) {
                        writeInfo.errorOccurred = true;
                        QueueFileAckToSend(FileAckResponseCodes::DUPLICATE_FILENAME, encodedRelativePathNameAsUtf8String);
                        LOG_INFO(subprocess) << "skipping writing zero-length file " << printableFullPathString << " because it already exists";
                    }
                    return true;
                }
                boost::filesystem::ofstream ofs(fullFilePath, boost::filesystem::ofstream::out | boost::filesystem::ofstream::binary);
                if (!ofs.good()) {
                    if (!writeInfo.errorOccurred) {
                        writeInfo.errorOccurred = true;
                        QueueFileAckToSend(FileAckResponseCodes::SAVE_ERROR, encodedRelativePathNameAsUtf8String);
                        LOG_ERROR(subprocess) << "unable to open file " << printableFullPathString << " for writing";
                    }
                    return false;
                }
                ofs.close();
                if (ofs.fail()) { //check close() worked
                    if (!writeInfo.errorOccurred) {
                        writeInfo.errorOccurred = true;
                        QueueFileAckToSend(FileAckResponseCodes::SAVE_ERROR, encodedRelativePathNameAsUtf8String);
                        LOG_ERROR(subprocess) << "save error: unable to close file " << printableFullPathString;
                    }
                    return false;
                }
                QueueFileAckToSend(FileAckResponseCodes::FILE_RECEIVED_ZERO_LENGTH, encodedRelativePathNameAsUtf8String);
            }
        }
        else if (sendFileMetadata.fragmentLength == 0) { //0 length fragment.. ignore
            LOG_INFO(subprocess) << "ignoring 0 length fragment";
        }
        else if (writeInfo.fragmentSet.empty()) { //first reception of this file
            writeInfo.expectedFileSize = sendFileMetadata.totalFileSize;
            if (m_saveDirectory.size()) { //if we are actually saving the files
                if (!CreateDirectoryRecursivelyVerboseIfNotExist(fullFilePath.parent_path())) {
                    if (!writeInfo.errorOccurred) {
                        writeInfo.errorOccurred = true;
                        QueueFileAckToSend(FileAckResponseCodes::SAVE_ERROR, encodedRelativePathNameAsUtf8String);
                        LOG_ERROR(subprocess) << "save error: unable to create directory structure for saving file " << printableFullPathString;
                    }
                    return false;
                }
                if (boost::filesystem::is_regular_file(fullFilePath)) {
                    if (!writeInfo.errorOccurred) {
                        writeInfo.errorOccurred = true;
                        QueueFileAckToSend(FileAckResponseCodes::DUPLICATE_FILENAME, encodedRelativePathNameAsUtf8String);
                        LOG_INFO(subprocess) << "skipping writing file " << printableFullPathString << " because it already exists";
                    }
                    return true;
                }
                
                LOG_INFO(subprocess) << "creating new file " << printableFullPathString;
                writeInfo.ofstreamPtr = boost::make_unique<boost::filesystem::ofstream>(fullFilePath, boost::filesystem::ofstream::out | boost::filesystem::ofstream::binary);
                if (!writeInfo.ofstreamPtr->good()) {
                    if (!writeInfo.errorOccurred) {
                        writeInfo.errorOccurred = true;
                        QueueFileAckToSend(FileAckResponseCodes::SAVE_ERROR, encodedRelativePathNameAsUtf8String);
                        LOG_ERROR(subprocess) << "save error: unable to open file " << printableFullPathString << " for writing";
                    }
                    return false;
                }
            }
            else {
                LOG_INFO(subprocess) << "not creating new file " << printableFullPathString;
            }
            doWriteFragment = true;
        }
        else if (IsFileFullyReceived(writeInfo.fragmentSet, sendFileMetadata.totalFileSize)) { //file was already received.. ignore duplicate fragment
            LOG_INFO(subprocess) << "ignoring duplicate fragment";
        }
        else { //subsequent reception of file fragment 
            doWriteFragment = true;
        }

        

        if (doWriteFragment) {
            if (writeInfo.expectedFileSize != sendFileMetadata.totalFileSize) {
                if (!writeInfo.errorOccurred) {
                    writeInfo.errorOccurred = true;
                    QueueFileAckToSend(FileAckResponseCodes::FILE_FRAGMENT_MALFORMED_ERROR, encodedRelativePathNameAsUtf8String);
                    LOG_ERROR(subprocess) << "malformed fragment: fragment totalFileSize mismatch for file " << printableFullPathString;
                }
                return false;
            }
            writeInfo.crc32cRxOutOfOrderChunks.AddUnalignedBytes(data, sendFileMetadata.fragmentOffset, sendFileMetadata.fragmentLength);
            if (fragmentOffsetPlusFragmentLength == sendFileMetadata.totalFileSize) { //last fragment, crc in this fragment's metadata is the valid crc of the file
                writeInfo.expectedCrc = sendFileMetadata.crc32c;
            }
            FragmentSet::InsertFragment(writeInfo.fragmentSet, FragmentSet::data_fragment_t(sendFileMetadata.fragmentOffset, fragmentOffsetPlusFragmentLength - 1));
            const bool fileIsFullyReceived = IsFileFullyReceived(writeInfo.fragmentSet, sendFileMetadata.totalFileSize);
            if (m_saveDirectory.size()) { //if we are actually saving the files
                writeInfo.ofstreamPtr->seekp(sendFileMetadata.fragmentOffset); //absolute position releative to beginning
                if (!writeInfo.ofstreamPtr->good()) { //check seekp() worked
                    if (!writeInfo.errorOccurred) {
                        writeInfo.errorOccurred = true;
                        QueueFileAckToSend(FileAckResponseCodes::SAVE_ERROR, encodedRelativePathNameAsUtf8String);
                        LOG_ERROR(subprocess) << "save error during call to seekp for " << printableFullPathString;
                    }
                    return false;
                }
                writeInfo.ofstreamPtr->write((char*)data, sendFileMetadata.fragmentLength);
                if (!writeInfo.ofstreamPtr->good()) { //check write() worked
                    if (!writeInfo.errorOccurred) {
                        writeInfo.errorOccurred = true;
                        QueueFileAckToSend(FileAckResponseCodes::SAVE_ERROR, encodedRelativePathNameAsUtf8String);
                        LOG_ERROR(subprocess) << "save error: unable to write fragment to file " << printableFullPathString;
                    }
                    return false;
                }
                if (fileIsFullyReceived) {
                    writeInfo.ofstreamPtr->close();
                    if (writeInfo.ofstreamPtr->fail()) { //check close() worked
                        if (!writeInfo.errorOccurred) {
                            writeInfo.errorOccurred = true;
                            QueueFileAckToSend(FileAckResponseCodes::SAVE_ERROR, encodedRelativePathNameAsUtf8String);
                            LOG_ERROR(subprocess) << "save error: unable to close file " << printableFullPathString;
                        }
                        return false;
                    }
                    writeInfo.ofstreamPtr.reset();
                }
            }
            if (fileIsFullyReceived) {
                const uint32_t computedCrc = writeInfo.crc32cRxOutOfOrderChunks.FinalizeAndGet();
                if (computedCrc == writeInfo.expectedCrc) {
                    QueueFileAckToSend(FileAckResponseCodes::FILE_RECEIVED_CRC_MATCH, encodedRelativePathNameAsUtf8String);
                    LOG_INFO(subprocess) << "successfully received " << printableFullPathString << " crc32c=" << computedCrc;
                }
                else {
                    QueueFileAckToSend(FileAckResponseCodes::FILE_RECEIVED_CRC_MISMATCH, encodedRelativePathNameAsUtf8String);
                    LOG_ERROR(subprocess) << "failed received " << printableFullPathString
                        << " sender_crc32c=" << writeInfo.expectedCrc << " receiver_crc32c=" << computedCrc;
                }
            }
        }
    }
    else if (type == SendFileMessageType::FILE_ACK) {
        if (bufSizeRemaining < FileAckMetadata::FileAckMetadataLength) {
            LOG_ERROR(subprocess) << "not enough data in bundle for file ack metadata";
            return false;
        }
        FileAckMetadata* fileAckMetadata = (FileAckMetadata*) data;
        data += FileAckMetadata::FileAckMetadataLength;
        bufSizeRemaining -= FileAckMetadata::FileAckMetadataLength;

        if (bufSizeRemaining != fileAckMetadata->pathLen) {
            LOG_ERROR(subprocess) << "bundle payload length not match file ack path length";
            return false;
        }
        const std::string encodedRelativePathNameAsUtf8String(data, data + fileAckMetadata->pathLen);
        const std::string printableRelativePathString = (Utf8Paths::IsAscii(encodedRelativePathNameAsUtf8String)) ?
            encodedRelativePathNameAsUtf8String : std::string("UTF-8-non-printable-file-name");
        const bool wasSuccessful = FileAckResponseCodeIsSuccess(fileAckMetadata->responseCode);
        LOG_INFO(subprocess) << "received " << ((wasSuccessful) ? "success" : "failure") << " ack from file " << printableRelativePathString 
            << "\n  ..response was " << FileAckResponseCodesToString(fileAckMetadata->responseCode);
    }
    else {
        LOG_ERROR(subprocess) << "invalid SendFileMessageType";
        return false;
    }

    return true;
}

void BpFileTransfer::Impl::QueueFileAckToSend(FileAckResponseCodes responseCode, const std::string& encodedRelativePathNameAsUtf8String) {
    std::vector<uint8_t> ack(sizeof(SendFileMessageType) + FileAckMetadata::FileAckMetadataLength + encodedRelativePathNameAsUtf8String.size());
    ack[0] = (uint8_t)SendFileMessageType::FILE_ACK;
    FileAckMetadata* meta = (FileAckMetadata*) &ack[1];
    meta->pathLen = static_cast<uint8_t>(encodedRelativePathNameAsUtf8String.size());
    meta->responseCode = responseCode;
    memcpy(&ack[sizeof(SendFileMessageType) + FileAckMetadata::FileAckMetadataLength], encodedRelativePathNameAsUtf8String.data(), encodedRelativePathNameAsUtf8String.size());
    {
        boost::mutex::scoped_lock lock(m_fileAcksToSendMutex);
        m_fileAcksToSendQueue.emplace(std::move(ack));
    }
    if (m_directoryScannerPtr) {
        m_directoryScannerPtr->InterruptTimedWait();
    }
    else {
        m_fileAcksToSendCv.notify_one();
    }
}

void BpFileTransfer::Impl::TryDequeueFileAckToSend(std::vector<uint8_t>& fileAckVecToSend) {
    fileAckVecToSend.clear();
    {
        boost::mutex::scoped_lock lock(m_fileAcksToSendMutex);
        if (!m_fileAcksToSendQueue.empty()) {
            fileAckVecToSend = std::move(m_fileAcksToSendQueue.front());
            m_fileAcksToSendQueue.pop();
        }
    }
}

void BpFileTransfer::Impl::OnNewWebsocketConnectionCallback(WebsocketServer::Connection& conn) {
    (void)conn;
    std::cout << "newconn\n";
    /*std::string jsonStr;
    jsonStr.reserve(1000);
    jsonStr = "{\"telemChannelDescriptions\":[";
    for (std::size_t i = 0; i < NUM_TELEM_CHANNELS; ++i) {
        const telem_channel_description_t& t = TELEM_CHANNEL_DESCRIPTIONS[i];
        if (i) {
            jsonStr += ',';
        }
        jsonStr += "{\"name\":\"" + t.name
            + "\", \"units\":\"" + t.units
            + "\", \"channelIndex\":" + boost::lexical_cast<std::string>(i) + "}";
    }
    jsonStr += "]}";
    //std::cout << jsonStr << "\n";
    conn.AsyncSendTextData(std::make_shared<std::string>(std::move(jsonStr)));*/
}

bool BpFileTransfer::Impl::OnNewWebsocketTextDataReceivedCallback(WebsocketServer::Connection& conn, std::string& receivedString) {
    (void)conn;
    std::cout << receivedString << "\n";
    return true;
}
