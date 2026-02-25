/**
 * @file BpSendFile.h
 * @author  Brian Tomko <brian.j.tomko@nasa.gov>
 *
 * @copyright Copyright (c) 2026 United States Government as represented by
 * the National Aeronautics and Space Administration.
 * No copyright is claimed in the United States under Title 17, U.S.Code.
 * All Other Rights Reserved.
 *
 * @section LICENSE
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * @section DESCRIPTION
 *
 * The BpSendFile class is a child class of BpSourcePattern.  It is an app used for
 * bundling up existing files (or optionally monitoring for new files) within a directory
 * into bundles up to a specified maximum size.
 * Bundles are sent either at a defined rate or as fast as possible.
 * The app copies a tiny meta-data payload to the beginning of the bundle
 * payload block used for preserving relative path names.
 * The remaining data in the bundle payload block is the file data (or fragment of file data).
 * This app is intended to be used with the BpReceiveFile app.
 * It is acceptable if the bundles arrive to the destination out-of-order.
 */

#ifndef _BP_SEND_FILE_H
#define _BP_SEND_FILE_H 1

#include <string>
#include "app_patterns/BpSourcePattern.h"
#include "DirectoryScanner.h"
#include <boost/filesystem/fstream.hpp>
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
    virtual ~BpSendFile() override;
    std::size_t GetNumberOfFilesToSend() const;

protected:
    virtual bool TryWaitForDataAvailable(const boost::posix_time::time_duration& timeout) override;
    virtual uint64_t GetNextPayloadLength_Step1() override;
    virtual bool CopyPayload_Step2(uint8_t * destinationBuffer) override;
private:
    void Shutdown_NotThreadSafe();
private:
    const uint64_t M_MAX_BUNDLE_PAYLOAD_SIZE_BYTES;

    //unbuffered mode
    std::unique_ptr<boost::filesystem::ifstream> m_currentIfstreamPtr;
    boost::filesystem::path m_currentFilePathAbsolute;
    boost::filesystem::path m_currentFilePathRelative;
    std::string m_currentFilePathRelativeAsUtf8String;
    std::string m_currentFilePathRelativeAsPrintableString;
    SendFileMetadata m_currentSendFileMetadata;
    boost::asio::io_service m_ioService; //for DirectoryScanner
    std::unique_ptr<boost::thread> m_ioServiceThreadPtr;
    std::unique_ptr<DirectoryScanner> m_directoryScannerPtr;
};

#endif //_BP_SEND_FILE_H
