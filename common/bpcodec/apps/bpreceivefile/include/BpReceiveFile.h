#ifndef _BP_RECEIVE_FILE_H
#define _BP_RECEIVE_FILE_H

#include "app_patterns/BpSinkPattern.h"
#include <set>
#include <map>
#include "FragmentSet.h"
#include <boost/filesystem.hpp>

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
    typedef std::pair<std::set<FragmentSet::data_fragment_t>, std::unique_ptr<boost::filesystem::ofstream> > fragments_ofstream_pair_t;
    typedef std::map<boost::filesystem::path, fragments_ofstream_pair_t> filename_to_writeinfo_map_t;

    BpReceiveFile(const boost::filesystem::path & saveDirectory);
    virtual ~BpReceiveFile();

protected:
    virtual bool ProcessPayload(const uint8_t * data, const uint64_t size);
public:
    boost::filesystem::path m_saveDirectory;
    filename_to_writeinfo_map_t m_filenameToWriteInfoMap;
};



#endif  //_BP_RECEIVE_FILE_H
