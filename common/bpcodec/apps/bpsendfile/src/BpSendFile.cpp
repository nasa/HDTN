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

BpSendFile::BpSendFile(const std::string & fileOrFolderPathString, uint64_t maxBundleSizeBytes) :
    BpSourcePattern(),
    M_MAX_BUNDLE_PAYLOAD_SIZE_BYTES(maxBundleSizeBytes),
    m_currentFileIndex(0)
{

    const boost::filesystem::path fileOrFolderPath(fileOrFolderPathString);
    if (boost::filesystem::is_directory(fileOrFolderPath)) {
        boost::filesystem::directory_iterator dirIt(fileOrFolderPath), eod;
        BOOST_FOREACH(const boost::filesystem::path &p, std::make_pair(dirIt, eod)) {
            if (boost::filesystem::is_regular_file(p) && (p.extension().string().size() > 1)) {
                std::string pathStr = p.string();
                if (pathStr.size() <= 255) {
                    m_sortedPathStringsOfFiles.push_back(std::move(pathStr));
                }
                else {
                    std::cout << "skipping " << pathStr << std::endl;
                }
            }
        }
        std::sort(m_sortedPathStringsOfFiles.begin(), m_sortedPathStringsOfFiles.end());
    }
    else if (boost::filesystem::is_regular_file(fileOrFolderPath) && (fileOrFolderPath.extension().string().size() > 1)) { //just one file
        std::string pathStr = fileOrFolderPath.string();
        if (pathStr.size() <= 255) {
            m_sortedPathStringsOfFiles.push_back(std::move(pathStr));
        }
        else {
            std::cout << "error " << pathStr << " is too long" << std::endl;
        }
    }

    if (m_sortedPathStringsOfFiles.size() == 0) {
        std::cerr << "error no files to send\n";
    }
    else {
        std::cout << "sending " << m_sortedPathStringsOfFiles.size() << " files\n";
    }
}

BpSendFile::~BpSendFile() {}

std::size_t BpSendFile::GetNumberOfFilesToSend() {
    return m_sortedPathStringsOfFiles.size();
}

uint64_t BpSendFile::GetNextPayloadLength_Step1() {
    if (m_currentFileIndex >= m_sortedPathStringsOfFiles.size()) {
        return 0; //stopping criteria
    }
    const std::string & p = m_sortedPathStringsOfFiles[m_currentFileIndex];
    if (!m_currentIfstreamPtr) {
        //std::cout << "loading and sending " << p << "\n";
        m_currentIfstreamPtr = boost::make_unique<std::ifstream>(p, std::ifstream::in | std::ifstream::binary);
        if (m_currentIfstreamPtr->good()) {
            // get length of file:
            m_currentIfstreamPtr->seekg(0, m_currentIfstreamPtr->end);
            m_currentSendFileMetadata.totalFileSize = m_currentIfstreamPtr->tellg();
            m_currentIfstreamPtr->seekg(0, m_currentIfstreamPtr->beg);
            m_currentSendFileMetadata.fragmentOffset = 0;
            m_currentSendFileMetadata.pathLen = static_cast<uint8_t>(p.size());
            std::cout << "send " << p << std::endl;
        }
        else { //file error occurred.. stop
            return 0;
        }
        
    }
    m_currentSendFileMetadata.fragmentLength = static_cast<uint32_t>(std::min(m_currentSendFileMetadata.totalFileSize - m_currentSendFileMetadata.fragmentOffset, M_MAX_BUNDLE_PAYLOAD_SIZE_BYTES));
    return m_currentSendFileMetadata.fragmentLength + sizeof(SendFileMetadata) + p.size();
}
bool BpSendFile::CopyPayload_Step2(uint8_t * destinationBuffer) {
    m_currentSendFileMetadata.ToLittleEndianInplace();
    memcpy(destinationBuffer, &m_currentSendFileMetadata, sizeof(m_currentSendFileMetadata));
    m_currentSendFileMetadata.ToNativeEndianInplace();
    destinationBuffer += sizeof(m_currentSendFileMetadata);
    const std::string & p = m_sortedPathStringsOfFiles[m_currentFileIndex];
    memcpy(destinationBuffer, p.data(), p.size());
    destinationBuffer += p.size();
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
        ++m_currentFileIndex;
    }
    else {
        m_currentSendFileMetadata.fragmentOffset = nextOffset;
    }
    
    return true;
}
