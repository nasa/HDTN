#include <string.h>
#include "BpSendFile.h"
#include "Logger.h"
#include <boost/foreach.hpp>
#include <boost/make_unique.hpp>
#include <boost/filesystem.hpp>
#include <boost/endian/conversion.hpp>

static constexpr hdtn::Logger::SubProcess subprocess = hdtn::Logger::SubProcess::none;

BpSendFile::SendFileMetadata::SendFileMetadata() : totalFileSize(0), fragmentOffset(0), fragmentLength(0), pathLen(0) {}
void BpSendFile::SendFileMetadata::ToLittleEndianInplace() {
    boost::endian::native_to_little_inplace(totalFileSize);
    boost::endian::native_to_little_inplace(fragmentOffset);
    boost::endian::native_to_little_inplace(fragmentLength);
}
void BpSendFile::SendFileMetadata::ToNativeEndianInplace() {
    boost::endian::little_to_native_inplace(totalFileSize);
    boost::endian::little_to_native_inplace(fragmentOffset);
    boost::endian::little_to_native_inplace(fragmentLength);
}

BpSendFile::BpSendFile(const boost::filesystem::path & fileOrFolderPath, uint64_t maxBundleSizeBytes,
    bool uploadExistingFiles, bool uploadNewFiles, unsigned int recurseDirectoriesDepth) :
    BpSourcePattern(),
    M_MAX_BUNDLE_PAYLOAD_SIZE_BYTES(maxBundleSizeBytes),
    m_directoryScannerPtr(boost::make_unique<DirectoryScanner>(fileOrFolderPath, uploadExistingFiles, uploadNewFiles, recurseDirectoriesDepth, m_ioService, 3000))
{

    if (uploadNewFiles) {
        m_ioServiceThreadPtr = boost::make_unique<boost::thread>(boost::bind(&boost::asio::io_service::run, &m_ioService));
    }
    
    if ((m_directoryScannerPtr->GetNumberOfFilesToSend() == 0) && (!uploadNewFiles)) {
        LOG_ERROR(subprocess) << "no files to send";
    }
    else {
        LOG_INFO(subprocess) << "sending " << m_directoryScannerPtr->GetNumberOfFilesToSend() << " files now, monitoring "
            << m_directoryScannerPtr->GetNumberOfCurrentlyMonitoredDirectories() << " directories";
    }
}

BpSendFile::~BpSendFile() {
    //m_ioService.stop(); //will hang ~m_ioService, delete the directory scanner object to stop it instead
    if (m_ioServiceThreadPtr) {
        boost::asio::post(m_ioService, boost::bind(&BpSendFile::Shutdown_NotThreadSafe, this));
        m_ioServiceThreadPtr->join();
        m_ioServiceThreadPtr.reset(); //delete it
    }
}
void BpSendFile::Shutdown_NotThreadSafe() { //call within m_ioService thread
    m_directoryScannerPtr.reset(); //delete it
}

std::size_t BpSendFile::GetNumberOfFilesToSend() const {
    return m_directoryScannerPtr->GetNumberOfFilesToSend();
}

uint64_t BpSendFile::GetNextPayloadLength_Step1() {
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
            // get length of file:
            m_currentIfstreamPtr->seekg(0, m_currentIfstreamPtr->end);
            m_currentSendFileMetadata.totalFileSize = m_currentIfstreamPtr->tellg();
            m_currentIfstreamPtr->seekg(0, m_currentIfstreamPtr->beg);
            m_currentSendFileMetadata.fragmentOffset = 0;
            m_currentSendFileMetadata.pathLen = static_cast<uint8_t>(m_currentFilePathRelative.size());
            LOG_INFO(subprocess) << "send " << m_currentFilePathRelative;
        }
        else { //file error occurred.. stop
            LOG_ERROR(subprocess) << "Failed to read " << m_currentFilePathAbsolute << " : error was : " << std::strerror(errno);
            return 0;
        }
    }
    m_currentSendFileMetadata.fragmentLength = static_cast<uint32_t>(std::min(m_currentSendFileMetadata.totalFileSize - m_currentSendFileMetadata.fragmentOffset, M_MAX_BUNDLE_PAYLOAD_SIZE_BYTES));
    return m_currentSendFileMetadata.fragmentLength + sizeof(SendFileMetadata) + m_currentSendFileMetadata.pathLen;
}
bool BpSendFile::CopyPayload_Step2(uint8_t * destinationBuffer) {
    m_currentSendFileMetadata.ToLittleEndianInplace();
    memcpy(destinationBuffer, &m_currentSendFileMetadata, sizeof(m_currentSendFileMetadata));
    m_currentSendFileMetadata.ToNativeEndianInplace();
    destinationBuffer += sizeof(m_currentSendFileMetadata);
    const std::string pRelativeStr = m_currentFilePathRelative.string(); //TODO ALLOW NON US CHARACTERS IN FILENAME
    memcpy(destinationBuffer, pRelativeStr.data(), pRelativeStr.size());
    destinationBuffer += m_currentSendFileMetadata.pathLen;
    // read data as a block:
    m_currentIfstreamPtr->read((char*)destinationBuffer, m_currentSendFileMetadata.fragmentLength);
    if (!*m_currentIfstreamPtr) {
        LOG_ERROR(subprocess) << "only " << m_currentIfstreamPtr->gcount() << " out of " << m_currentSendFileMetadata.fragmentLength << " bytes could be read";
        return false;
    }
    const uint64_t nextOffset = m_currentSendFileMetadata.fragmentLength + m_currentSendFileMetadata.fragmentOffset;
    const bool isEndOfThisFile = (nextOffset == m_currentSendFileMetadata.totalFileSize);
    if (isEndOfThisFile) {
        m_currentIfstreamPtr->close();
        m_currentIfstreamPtr.reset();
        m_currentFilePathAbsolute.clear();
        m_currentFilePathRelative.clear();
    }
    else {
        m_currentSendFileMetadata.fragmentOffset = nextOffset;
    }
    
    return true;
}

bool BpSendFile::TryWaitForDataAvailable(const boost::posix_time::time_duration& timeout) {
    if (m_currentFilePathAbsolute.empty()) {
        return m_directoryScannerPtr->GetNextFilePathTimeout(m_currentFilePathAbsolute, m_currentFilePathRelative, timeout);
    }
    return true;
}
