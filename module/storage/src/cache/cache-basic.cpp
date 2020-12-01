#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <iostream>
#include <sstream>
#include <sys/mman.h>
#include "cache.hpp"
#include "store.hpp"

hdtn3::flow_store_entry hdtn3::flow_store::load(int flow) {
    if(flow >= HDTN3_FLOWCOUNT_MAX || flow < 0) {
        errno = EINVAL;
        return {
            -1,
            NULL
        };
    }

    if(_flow.find(flow) == _flow.end()) {
        _flow[flow] = {
            -1, 
            NULL
        };
    }
    flow_store_entry res = _flow[flow];

    if(res.fd < 0) {
        int folder = (flow & 0x00FF0000) >> 16;
        int file = (flow & 0x0000FFFF);
        std::stringstream tstr;
        tstr << _root << "/" << folder << "/" << file;
        res.fd = open(tstr.str().c_str(), O_RDWR | O_CREAT, S_IWUSR | S_IRUSR | S_IRGRP);
        if(res.fd < 0) {
            std::cerr << "[flow-store:basic] Unable to open cache: " << tstr.str() << std::endl;
            perror("[flow-store:basic] Error");
            return res;
        }
        res.header = &_index[flow];
        if(NULL == res.header) {
            res.fd = -1;
            std::cerr << "[flow-store:basic] Failed to map cache header into address space: " << tstr.str() << std::endl;
            perror("[flow-store:basic] Error");
            return res;
        }

        _flow[flow] = res;
    }

    return res;
}

int hdtn3::flow_store::read(int flow, void* data, int maxsz) {
    flow_store_entry res = load(flow);

    if(res.fd < 0) {
        errno = EINVAL;
        return -1;
    }

    uint64_t to_read = res.header->end - res.header->begin;
    if(to_read > maxsz) {
        to_read = maxsz;
    }
    int retrieved = pread(res.fd, data, to_read, res.header->begin);
    if(retrieved >= 0) {
        res.header->begin += retrieved;
    }
    _stats.disk_rbytes += retrieved;
    _stats.disk_rcount ++;
    _stats.disk_used -= retrieved;
    return retrieved;
}

int hdtn3::flow_store::write(int flow, void* data, int sz) {
    flow_store_entry res = load(flow);

    if(res.fd < 0) {
        errno = EINVAL;
        return -1;
    }

    int written = pwrite(res.fd, data, sz, res.header->end);
    if(written >= 0) {
        res.header->end += written;
    }
    _stats.disk_wbytes += written;
    _stats.disk_wcount ++;
    _stats.disk_used += written;
    return written;
}

bool hdtn3::flow_store::init(std::string root) {
    _root = root;
    std::cout << "[flow-store:basic] Preparing cache for use (this could take some time)..." << std::endl;
    std::cout << "[flow-store:basic] Cache root directory is: " << _root << std::endl;
    for(int i = 0; i < 256; ++i) {
        std::stringstream tstr;
        tstr << _root << "/" << i;
        std::string path = tstr.str();
        struct stat cache_info;
        int res = stat(path.c_str(), &cache_info);
        if(res) {
            if(errno == ENOENT) {
                mkdir(path.c_str(), S_IRWXU | S_IXGRP | S_IRGRP);
                res = stat(path.c_str(), &cache_info);
                if(res) {
                    std::cerr << "[flow-store:basic] Cache initialization failed: " << path << std::endl;
                    perror("[flow-store:basic] Failed to prepare cache for application use");
                    return false;
                }
            }
            else {
                std::cerr << "[flow-store:basic] Cache initialization failed: " << path << std::endl;
                perror("[flow-store:basic] Failed to prepare cache for application use");
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
                std::cerr << "[flow-store:basic] Cache initialization failed: " << fpath << std::endl;
                perror("[flow-store:basic] Failed to prepare cache element: ");
                return false;
            }
        }
        */
    }

    std::stringstream tstr;
    tstr << _root << "/hdtn3.index";
    std::string ipath;
    bool build_index = false;
    _index_fd = open(tstr.str().c_str(), O_RDWR | O_CREAT, S_IWUSR | S_IRUSR | S_IRGRP);
    if(_index_fd < 0) {
        std::cerr << "[flow-store:basic] Failed to open hdtn3.index: " << tstr.str() << std::endl;
        perror("[flow-store:basic] Error");
        return false;
    }
    int res = fallocate(_index_fd, 0, 0, HDTN3_FLOWCOUNT_MAX * sizeof(hdtn3::flow_store_header));
    if(res) {
        std::cerr << "[flow-store:basic] Failed to allocate space for hdtn3.index." << std::endl;
        perror("[flow-store:basic] Error");
        return false;
    }

    _index = (flow_store_header*)mmap(0, HDTN3_FLOWCOUNT_MAX * sizeof(hdtn3::flow_store_header), 
                    PROT_READ | PROT_WRITE, MAP_SHARED, _index_fd, 0);
    if(NULL == _index) {
        std::cerr << "[flow-store:basic] Failed to map hdtn3.index: " << tstr.str() << std::endl;
        perror("[flow-store:basic] Error");
        return false;
    }
    for(int i = 0; i < HDTN3_FLOWCOUNT_MAX; ++i) {
        _stats.disk_used += (_index[i].end - _index[i].begin);
    }
    std::cout << "[flow-store:basic] Initialization completed." << std::endl;

    return true;
}
