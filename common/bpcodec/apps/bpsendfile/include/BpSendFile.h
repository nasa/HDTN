#ifndef _BP_SEND_FILE_H
#define _BP_SEND_FILE_H 1

#include <string>
#include "app_patterns/BpSourcePattern.h"


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
    BpSendFile(const std::string & fileOrFolderPathString, uint64_t maxBundleSizeBytes);
    virtual ~BpSendFile();
    std::size_t GetNumberOfFilesToSend();

protected:
    virtual uint64_t GetNextPayloadLength_Step1();
    virtual bool CopyPayload_Step2(uint8_t * destinationBuffer);
private:
    std::vector<std::string> m_sortedPathStringsOfFiles;
    const uint64_t M_MAX_BUNDLE_PAYLOAD_SIZE_BYTES;

    //unbuffered mode
    std::unique_ptr<std::ifstream> m_currentIfstreamPtr;
    uint64_t m_currentFileIndex;
    SendFileMetadata m_currentSendFileMetadata;
};

#endif //_BP_SEND_FILE_H
