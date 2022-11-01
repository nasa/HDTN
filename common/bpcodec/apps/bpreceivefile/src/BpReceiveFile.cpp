#include "BpReceiveFile.h"
#include "Logger.h"
#include <boost/make_unique.hpp>
#include <boost/endian/conversion.hpp>
#include <boost/filesystem.hpp>

static constexpr hdtn::Logger::SubProcess subprocess = hdtn::Logger::SubProcess::none;

static bool CreateDirectoryRecursivelyVerboseIfNotExist(const boost::filesystem::path & path) {
    if (!boost::filesystem::is_directory(path)) {
        LOG_INFO(subprocess) << "directory does not exist.. creating directory recursively..";
        try {
            if (boost::filesystem::create_directories(path)) {
                LOG_INFO(subprocess) << "successfully created directory";
            }
            else {
                LOG_ERROR(subprocess) << "error: unable to create directory";
                return false;
            }
        }
        catch (const boost::system::system_error & e) {
            LOG_ERROR(subprocess) << "error: " << e.what() << "..unable to create directory";
            return false;
        }
    }
    return true;
}

BpReceiveFile::BpReceiveFile(const boost::filesystem::path& saveDirectory) :
    BpSinkPattern(),
    m_saveDirectory(saveDirectory)
{
    if (m_saveDirectory.empty()) {
        LOG_INFO(subprocess) << "not saving files";
    }
    else {
        LOG_INFO(subprocess) << "saving files to directory: " << m_saveDirectory;
        if (!CreateDirectoryRecursivelyVerboseIfNotExist(m_saveDirectory)) {
            LOG_INFO(subprocess) << "not saving files";
            m_saveDirectory.clear();
        }
    }
}

BpReceiveFile::~BpReceiveFile() {}

static bool IsFileFullyReceived(std::set<FragmentSet::data_fragment_t> & fragmentSet, const uint64_t totalFileSize) {
    if (fragmentSet.size() != 1) {
        return false;
    }
    const FragmentSet::data_fragment_t & df = *fragmentSet.begin();
    return ((df.beginIndex == 0) && (df.endIndex == (totalFileSize - 1)));
}

bool BpReceiveFile::ProcessPayload(const uint8_t * data, const uint64_t size) {
    SendFileMetadata sendFileMetadata;
    if (size < sizeof(sendFileMetadata)) {
        return false;
    }
    memcpy(&sendFileMetadata, data, sizeof(sendFileMetadata));
    data += sizeof(sendFileMetadata);
    boost::endian::little_to_native_inplace(sendFileMetadata.totalFileSize);
    boost::endian::little_to_native_inplace(sendFileMetadata.fragmentOffset);
    boost::endian::little_to_native_inplace(sendFileMetadata.fragmentLength);
    //safety checks
    if (sendFileMetadata.fragmentOffset > 8000000000) { //8GB ignore
        LOG_ERROR(subprocess) << "error fragmentOffset > 8GB";
        return false;
    }
    if (sendFileMetadata.fragmentLength > 2000000000) { //2GB ignore
        LOG_ERROR(subprocess) << "error fragmentLength > 2GB";
        return false;
    }
    const uint64_t fragmentOffsetPlusFragmentLength = sendFileMetadata.fragmentOffset + sendFileMetadata.fragmentLength;
    if (fragmentOffsetPlusFragmentLength > sendFileMetadata.totalFileSize) {
        LOG_ERROR(subprocess) << "error fragment exceeds total file size";
        return false;
    }
    const boost::filesystem::path fileName(std::string(data, data + sendFileMetadata.pathLen));
    data += sendFileMetadata.pathLen;
    fragments_ofstream_pair_t & fragmentsOfstreamPair = m_filenameToWriteInfoMap[fileName];
    std::set<FragmentSet::data_fragment_t> & fragmentSet = fragmentsOfstreamPair.first;
    std::unique_ptr<boost::filesystem::ofstream> & ofstreamPtr = fragmentsOfstreamPair.second;
    bool doWriteFragment = false;
    if (sendFileMetadata.totalFileSize == 0) { //0 length file, create an empty file
        if (fragmentSet.empty() && m_saveDirectory.size()) {
            boost::filesystem::path fullPathFileName = boost::filesystem::path(m_saveDirectory) / boost::filesystem::path(fileName);
            if (!CreateDirectoryRecursivelyVerboseIfNotExist(fullPathFileName.parent_path())) {
                return false;
            }
            if (boost::filesystem::is_regular_file(fullPathFileName)) {
                LOG_INFO(subprocess) << "skipping writing zero-length file " << fullPathFileName << " because it already exists";
                return true;
            }
            boost::filesystem::ofstream ofs(fullPathFileName, boost::filesystem::ofstream::out | boost::filesystem::ofstream::binary);
            if (!ofs.good()) {
                LOG_ERROR(subprocess) << "error, unable to open file " << fullPathFileName << " for writing";
                return false;
            }
            ofs.close();
        }
    }
    else if (sendFileMetadata.fragmentLength == 0) { //0 length fragment.. ignore
        LOG_INFO(subprocess) << "ignoring 0 length fragment";
    }
    else if (fragmentSet.empty()) { //first reception of this file
        boost::filesystem::path fullPathFileName = boost::filesystem::path(m_saveDirectory) / boost::filesystem::path(fileName);
        if (m_saveDirectory.size()) { //if we are actually saving the files
            if (!CreateDirectoryRecursivelyVerboseIfNotExist(fullPathFileName.parent_path())) {
                return false;
            }
            if (boost::filesystem::is_regular_file(fullPathFileName)) {
                if (sendFileMetadata.fragmentOffset == 0) {
                    LOG_INFO(subprocess) << "skipping writing file " << fullPathFileName << " because it already exists";
                }
                else {
                    LOG_INFO(subprocess) << "ignoring fragment for " << fullPathFileName << " because file already exists";
                }
                return true;
            }
            LOG_INFO(subprocess) << "creating new file " << fullPathFileName;
            ofstreamPtr = boost::make_unique<boost::filesystem::ofstream>(fullPathFileName, boost::filesystem::ofstream::out | boost::filesystem::ofstream::binary);
            if (!ofstreamPtr->good()) {
                LOG_ERROR(subprocess) << "unable to open file " << fullPathFileName << " for writing";
                return false;
            }
        }
        else {
            LOG_INFO(subprocess) << "not creating new file " << fullPathFileName;
        }
        doWriteFragment = true;
    }
    else if (IsFileFullyReceived(fragmentSet, sendFileMetadata.totalFileSize)) { //file was already received.. ignore duplicate fragment
        LOG_INFO(subprocess) << "ignoring duplicate fragment";
    }
    else { //subsequent reception of file fragment 
        doWriteFragment = true;
    }

    if (doWriteFragment) {
        FragmentSet::InsertFragment(fragmentSet, FragmentSet::data_fragment_t(sendFileMetadata.fragmentOffset, fragmentOffsetPlusFragmentLength - 1));
        const bool fileIsFullyReceived = IsFileFullyReceived(fragmentSet, sendFileMetadata.totalFileSize);
        if (m_saveDirectory.size()) { //if we are actually saving the files
            ofstreamPtr->seekp(sendFileMetadata.fragmentOffset); //absolute position releative to beginning
            ofstreamPtr->write((char*)data, sendFileMetadata.fragmentLength);
            if (fileIsFullyReceived) {
                ofstreamPtr->close();
                ofstreamPtr.reset();
            }
        }
        if (fileIsFullyReceived) {
            LOG_INFO(subprocess) << "closed " << fileName;
        }
    }


    return true;
}
