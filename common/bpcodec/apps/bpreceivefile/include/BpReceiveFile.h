/**
 * @file BpReceiveFile.h
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
 * The BpReceiveFile class is a child class of BpSinkPattern.
 * It is an app used for receiving file-fragment bundles from the BpSendFile app
 * and writing those files to disk within a directory and preserving
 * the sender's relative path names.
 * This app is intended to be used with the BpSendFile app.
 * It is acceptable if the bundles arrive out-of-order.
 */

#ifndef _BP_RECEIVE_FILE_H
#define _BP_RECEIVE_FILE_H

#include "app_patterns/BpSinkPattern.h"
#include <set>
#include <map>
#include "FragmentSet.h"
#include <boost/filesystem/path.hpp>
#include <boost/filesystem/fstream.hpp>
class BpReceiveFile : public BpSinkPattern {
private:
    BpReceiveFile();
public:
    struct SendFileMetadata {
        SendFileMetadata() : totalFileSize(0), fragmentOffset(0), fragmentLength(0), pathLen(0) {}
        uint64_t totalFileSize;
        uint64_t fragmentOffset;
        uint32_t fragmentLength;
        uint8_t pathLen;
        uint8_t unused1;
        uint8_t unused2;
        uint8_t unused3;
    };
    typedef std::pair<FragmentSet::data_fragment_set_t, std::unique_ptr<boost::filesystem::ofstream> > fragments_ofstream_pair_t;
    typedef std::map<boost::filesystem::path, fragments_ofstream_pair_t> filename_to_writeinfo_map_t;

    BpReceiveFile(const boost::filesystem::path & saveDirectory);
    virtual ~BpReceiveFile() override;

protected:
    virtual bool ProcessPayload(const uint8_t * data, const uint64_t size) override;
public:
    boost::filesystem::path m_saveDirectory;
    filename_to_writeinfo_map_t m_filenameToWriteInfoMap;
};



#endif  //_BP_RECEIVE_FILE_H
