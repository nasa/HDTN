#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <iostream>
#include <sstream>

#include "cache.hpp"
#include "store.hpp"

namespace hdtn {

FlowStore::~FlowStore() {
    munmap(0, HDTN_FLOWCOUNT_MAX * sizeof(hdtn::FlowStoreHeader));
    close(m_indexFd);
}

FlowStoreEntry FlowStore::Load(int flow) {
    if (flow >= HDTN_FLOWCOUNT_MAX || flow < 0) {
        errno = EINVAL;
        return {-1, NULL};
    }

    if (m_flow.find(flow) == m_flow.end()) {
        m_flow[flow] = {-1, NULL};
    }
    FlowStoreEntry res = m_flow[flow];

    if (res.fd < 0) {
        int folder = (flow & 0x00FF0000) >> 16;
        int file = (flow & 0x0000FFFF);
        std::stringstream tstr;
        tstr << m_root << "/" << folder << "/" << file;

        // JCF
        std::cout << "In cache-basic, flow_store::load.  tstr.str(): " << tstr.str() << std::endl;

        res.fd = open(tstr.str().c_str(), O_RDWR | O_CREAT, S_IWUSR | S_IRUSR | S_IRGRP);
        if (res.fd < 0) {
            std::cerr << "[flow-store:basic] Unable to open cache: " << tstr.str() << std::endl;
            perror("[flow-store:basic] Error");
            return res;
        }
        res.header = &m_index[flow];
        if (NULL == res.header) {
            res.fd = -1;
            std::cerr << "[flow-store:basic] Failed to map cache header into address "
                         "space: "
                      << tstr.str() << std::endl;
            perror("[flow-store:basic] Error");
            return res;
        }

        m_flow[flow] = res;
    }

    return res;
}

int FlowStore::Read(int flow, void *data, int maxsz) {
    FlowStoreEntry res = Load(flow);

    if (res.fd < 0) {
        errno = EINVAL;
        return -1;
    }

    uint64_t toRead = res.header->end - res.header->begin;
    if (toRead > maxsz) {
        toRead = maxsz;
    }
    int retrieved = pread(res.fd, data, toRead, res.header->begin);
    if (retrieved >= 0) {
        res.header->begin += retrieved;
    }
    m_stats.diskRbytes += retrieved;
    m_stats.diskRcount++;
    m_stats.diskUsed -= retrieved;
    close(res.fd);
    return retrieved;
}

int FlowStore::Write(int flow, void *data, int sz) {
    FlowStoreEntry res = Load(flow);

    if (res.fd < 0) {
        errno = EINVAL;
        return -1;
    }

    int written = pwrite(res.fd, data, sz, res.header->end);
    if (written >= 0) {
        res.header->end += written;
    }
    m_stats.diskWbytes += written;
    m_stats.diskWcount++;
    m_stats.diskUsed += written;
    close(res.fd);
    return written;
}

bool FlowStore::Init(std::string root) {
    m_root = root;
    std::cout << "[flow-store:basic] Preparing cache for use (this could take "
                 "some time)..."
              << std::endl;
    std::cout << "[flow-store:basic] Cache root directory is: " << m_root << std::endl;
    for (int i = 0; i < 256; ++i) {
        std::stringstream tstr;
        tstr << m_root << "/" << i;
        std::string path = tstr.str();
        struct stat cacheInfo;
        int res = stat(path.c_str(), &cacheInfo);
        if (res) {
            if (errno == ENOENT) {
                mkdir(path.c_str(), S_IRWXU | S_IXGRP | S_IRGRP);
                res = stat(path.c_str(), &cacheInfo);
                if (res) {
                    std::cerr << "[flow-store:basic] Cache initialization failed: " << path << std::endl;
                    perror(
                        "[flow-store:basic] Failed to prepare cache for "
                        "application use");
                    return false;
                }
            }
            else {
                std::cerr << "[flow-store:basic] Cache initialization failed: " << path << std::endl;
                perror(
                    "[flow-store:basic] Failed to prepare cache for "
                    "application use");
                return false;
            }
        }

        /*
    for(int j = 0; j < 65536; ++j) {
        tstr = std::stringstream();
        tstr << _root << "/" << i << "/" << j;
        std::string fpath = tstr.str();
        int tfd = open(fpath.c_str(), O_CREAT, S_IWUSR | S_IRUSR | S_IRGRP);
        close(tfd);
        if(access(fpath.c_str(), F_OK)) {
            std::cerr << "[flow-store:basic] Cache initialization failed: " <<
    fpath << std::endl; perror("[flow-store:basic] Failed to prepare cache
    element: "); return false;
        }
    }
    */
    }

    std::stringstream tstr;
    tstr << m_root << "/hdtn.index";
    std::string ipath;
    bool buildIndex = false;
    m_indexFd = open(tstr.str().c_str(), O_RDWR | O_CREAT, S_IWUSR | S_IRUSR | S_IRGRP);
    if (m_indexFd < 0) {
        std::cerr << "[flow-store:basic] Failed to open hdtn.index: " << tstr.str() << std::endl;
        perror("[flow-store:basic] Error");
        return false;
    }
    int res = fallocate(m_indexFd, 0, 0, HDTN_FLOWCOUNT_MAX * sizeof(hdtn::FlowStoreHeader));
    if (res) {
        std::cerr << "[flow-store:basic] Failed to allocate space for hdtn.index." << std::endl;
        perror("[flow-store:basic] Error");
        return false;
    }

    m_index = (FlowStoreHeader *)mmap(0, HDTN_FLOWCOUNT_MAX * sizeof(hdtn::FlowStoreHeader), PROT_READ | PROT_WRITE,
                                     MAP_SHARED, m_indexFd, 0);
    if (NULL == m_index) {
        std::cerr << "[flow-store:basic] Failed to map hdtn.index: " << tstr.str() << std::endl;
        perror("[flow-store:basic] Error");
        return false;
    }
    for (int i = 0; i < HDTN_FLOWCOUNT_MAX; ++i) {
        m_stats.diskUsed += (m_index[i].end - m_index[i].begin);
    }
    std::cout << "[flow-store:basic] Initialization completed." << std::endl;

    return true;
}
}  // namespace hdtn
