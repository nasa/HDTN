#include <string.h>
#include <iostream>
#include "BpSendFile.h"
#include <boost/foreach.hpp>
#include <boost/make_unique.hpp>
#include <boost/filesystem.hpp>
#include <boost/endian/conversion.hpp>

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
    m_directoryScanner(fileOrFolderPath, uploadExistingFiles, uploadNewFiles, recurseDirectoriesDepth, m_ioService, 3000)
{

    if (uploadNewFiles) {
        m_ioServiceThreadPtr = boost::make_unique<boost::thread>(boost::bind(&boost::asio::io_service::run, &m_ioService));
    }
    
    if ((m_directoryScanner.GetNumberOfFilesToSend() == 0) && (!uploadNewFiles)) {
        std::cerr << "error no files to send\n";
    }
    else {
        std::cout << "sending " << m_directoryScanner.GetNumberOfFilesToSend() << " files now, monitoring "
            << m_directoryScanner.GetNumberOfCurrentlyMonitoredDirectories() << " directories\n";
    }
}

BpSendFile::~BpSendFile() {}

std::size_t BpSendFile::GetNumberOfFilesToSend() const {
    return m_directoryScanner.GetNumberOfFilesToSend();
}

uint64_t BpSendFile::GetNextPayloadLength_Step1() {
    if (m_currentFilePathAbsolute.empty()) {
        if (!m_directoryScanner.GetNextFilePath(m_currentFilePathAbsolute, m_currentFilePathRelative)) {
            if (m_directoryScanner.GetNumberOfCurrentlyMonitoredDirectories()) {
                return UINT64_MAX; //pending criteria
            }
            else {
                return 0; //stopping criteria
            }
        }
    }
    if (!m_currentIfstreamPtr) {
        //std::cout << "loading and sending " << p << "\n";
        m_currentIfstreamPtr = boost::make_unique<boost::filesystem::ifstream>(m_currentFilePathAbsolute, std::ifstream::in | std::ifstream::binary);
        if (m_currentIfstreamPtr->good()) {
            // get length of file:
            m_currentIfstreamPtr->seekg(0, m_currentIfstreamPtr->end);
            m_currentSendFileMetadata.totalFileSize = m_currentIfstreamPtr->tellg();
            m_currentIfstreamPtr->seekg(0, m_currentIfstreamPtr->beg);
            m_currentSendFileMetadata.fragmentOffset = 0;
            m_currentSendFileMetadata.pathLen = static_cast<uint8_t>(m_currentFilePathRelative.size());
            std::cout << "send " << m_currentFilePathRelative << std::endl;
        }
        else { //file error occurred.. stop
            std::cout << "error in BpSendFile::GetNextPayloadLength_Step1: failed to read " << m_currentFilePathAbsolute << " : error was : " << std::strerror(errno) << "\n";
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
    const std::string pRelativeStr = m_currentFilePathRelative.string();
    memcpy(destinationBuffer, pRelativeStr.data(), pRelativeStr.size());
    destinationBuffer += m_currentSendFileMetadata.pathLen;
    // read data as a block:
    m_currentIfstreamPtr->read((char*)destinationBuffer, m_currentSendFileMetadata.fragmentLength);
    if (*m_currentIfstreamPtr) {
        //std::cout << "all characters read successfully.";
    }
    else {
        std::cout << "error: only " << m_currentIfstreamPtr->gcount() << " out of " << m_currentSendFileMetadata.fragmentLength << " bytes could be read\n";
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
        return m_directoryScanner.GetNextFilePathTimeout(m_currentFilePathAbsolute, m_currentFilePathRelative, timeout);
    }
    return true;
}
