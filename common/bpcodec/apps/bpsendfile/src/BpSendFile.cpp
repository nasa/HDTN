/**
 * @file BpSendFile.cpp
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

#include <string.h>
#include "BpSendFile.h"
#include "Logger.h"
#include <boost/foreach.hpp>
#include <boost/make_unique.hpp>
#include <boost/filesystem.hpp>
#include <boost/endian/conversion.hpp>
#include "ThreadNamer.h"
#include "Utf8Paths.h"

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
        ThreadNamer::SetIoServiceThreadName(m_ioService, "ioServiceBpSendFile");
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
            LOG_INFO(subprocess) << "Sending: " << m_currentFilePathRelativeAsPrintableString;
        }
        else { //file error occurred.. stop
            LOG_ERROR(subprocess) << "Failed to read " << m_currentFilePathRelativeAsPrintableString << " : error was : " << std::strerror(errno);
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
    memcpy(destinationBuffer, m_currentFilePathRelativeAsUtf8String.data(), m_currentFilePathRelativeAsUtf8String.size());
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
