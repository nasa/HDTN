#ifndef _BP_SEND_FILE_H
#define _BP_SEND_FILE_H 1

#include <string>
#include "app_patterns/BpSourcePattern.h"
#include "DirectoryScanner.h"

class BpSendFile : public BpSourcePattern {
private:
    BpSendFile();
public:
    struct SendFileMetadata {
        SendFileMetadata();
        void ToLittleEndianInplace();
        void ToNativeEndianInplace();
        uint64_t totalFileSize;
        uint64_t fragmentOffset;
        uint32_t fragmentLength;
        uint8_t pathLen;
        uint8_t unused1;
        uint8_t unused2;
        uint8_t unused3;
    };
    BpSendFile(const boost::filesystem::path & fileOrFolderPath, uint64_t maxBundleSizeBytes, bool uploadExistingFiles, bool uploadNewFiles, unsigned int recurseDirectoriesDepth);
    virtual ~BpSendFile();
    std::size_t GetNumberOfFilesToSend() const;

protected:
    virtual bool TryWaitForDataAvailable(const boost::posix_time::time_duration& timeout);
    virtual uint64_t GetNextPayloadLength_Step1();
    virtual bool CopyPayload_Step2(uint8_t * destinationBuffer);
private:
    void Shutdown_NotThreadSafe();
private:
    const uint64_t M_MAX_BUNDLE_PAYLOAD_SIZE_BYTES;

    //unbuffered mode
    std::unique_ptr<boost::filesystem::ifstream> m_currentIfstreamPtr;
    boost::filesystem::path m_currentFilePathAbsolute;
    boost::filesystem::path m_currentFilePathRelative;
    SendFileMetadata m_currentSendFileMetadata;
    boost::asio::io_service m_ioService; //for DirectoryScanner
    std::unique_ptr<boost::thread> m_ioServiceThreadPtr;
    std::unique_ptr<DirectoryScanner> m_directoryScannerPtr;
};

#endif //_BP_SEND_FILE_H
