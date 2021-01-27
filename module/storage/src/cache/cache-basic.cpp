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
  close(index_fd_);
}

FlowStoreEntry FlowStore::Load(int flow) {
  if (flow >= HDTN_FLOWCOUNT_MAX || flow < 0) {
    errno = EINVAL;
    return {-1, NULL};
  }

  if (flow_.find(flow) == flow_.end()) {
    flow_[flow] = {-1, NULL};
  }
  FlowStoreEntry res = flow_[flow];

  if (res.fd < 0) {
    int folder = (flow & 0x00FF0000) >> 16;
    int file = (flow & 0x0000FFFF);
    std::stringstream tstr;
    tstr << root_ << "/" << folder << "/" << file;

    // JCF
    std::cout << "In cache-basic, flow_store::load.  tstr.str(): " << tstr.str()
              << std::endl;

    res.fd =
        open(tstr.str().c_str(), O_RDWR | O_CREAT, S_IWUSR | S_IRUSR | S_IRGRP);
    if (res.fd < 0) {
      std::cerr << "[flow-store:basic] Unable to open cache: " << tstr.str()
                << std::endl;
      perror("[flow-store:basic] Error");
      return res;
    }
    res.header = &index_[flow];
    if (NULL == res.header) {
      res.fd = -1;
      std::cerr << "[flow-store:basic] Failed to map cache header into address "
                   "space: "
                << tstr.str() << std::endl;
      perror("[flow-store:basic] Error");
      return res;
    }

    flow_[flow] = res;
  }

  return res;
}

int FlowStore::Read(int flow, void *data, int maxsz) {
  FlowStoreEntry res = Load(flow);

  if (res.fd < 0) {
    errno = EINVAL;
    return -1;
  }

  uint64_t to_read = res.header->end - res.header->begin;
  if (to_read > maxsz) {
    to_read = maxsz;
  }
  int retrieved = pread(res.fd, data, to_read, res.header->begin);
  if (retrieved >= 0) {
    res.header->begin += retrieved;
  }
  stats_.disk_rbytes += retrieved;
  stats_.disk_rcount++;
  stats_.disk_used -= retrieved;
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
  stats_.disk_wbytes += written;
  stats_.disk_wcount++;
  stats_.disk_used += written;
  close(res.fd);
  return written;
}

bool FlowStore::Init(std::string root) {
  root_ = root;
  std::cout << "[flow-store:basic] Preparing cache for use (this could take "
               "some time)..."
            << std::endl;
  std::cout << "[flow-store:basic] Cache root directory is: " << root_
            << std::endl;
  for (int i = 0; i < 256; ++i) {
    std::stringstream tstr;
    tstr << root_ << "/" << i;
    std::string path = tstr.str();
    struct stat cache_info;
    int res = stat(path.c_str(), &cache_info);
    if (res) {
      if (errno == ENOENT) {
        mkdir(path.c_str(), S_IRWXU | S_IXGRP | S_IRGRP);
        res = stat(path.c_str(), &cache_info);
        if (res) {
          std::cerr << "[flow-store:basic] Cache initialization failed: "
                    << path << std::endl;
          perror(
              "[flow-store:basic] Failed to prepare cache for application use");
          return false;
        }
      } else {
        std::cerr << "[flow-store:basic] Cache initialization failed: " << path
                  << std::endl;
        perror(
            "[flow-store:basic] Failed to prepare cache for application use");
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
  tstr << root_ << "/hdtn.index";
  std::string ipath;
  bool build_index = false;
  index_fd_ =
      open(tstr.str().c_str(), O_RDWR | O_CREAT, S_IWUSR | S_IRUSR | S_IRGRP);
  if (index_fd_ < 0) {
    std::cerr << "[flow-store:basic] Failed to open hdtn.index: " << tstr.str()
              << std::endl;
    perror("[flow-store:basic] Error");
    return false;
  }
  int res = fallocate(index_fd_, 0, 0,
                      HDTN_FLOWCOUNT_MAX * sizeof(hdtn::FlowStoreHeader));
  if (res) {
    std::cerr << "[flow-store:basic] Failed to allocate space for hdtn.index."
              << std::endl;
    perror("[flow-store:basic] Error");
    return false;
  }

  index_ = (FlowStoreHeader *)mmap(
      0, HDTN_FLOWCOUNT_MAX * sizeof(hdtn::FlowStoreHeader),
      PROT_READ | PROT_WRITE, MAP_SHARED, index_fd_, 0);
  if (NULL == index_) {
    std::cerr << "[flow-store:basic] Failed to map hdtn.index: " << tstr.str()
              << std::endl;
    perror("[flow-store:basic] Error");
    return false;
  }
  for (int i = 0; i < HDTN_FLOWCOUNT_MAX; ++i) {
    stats_.disk_used += (index_[i].end - index_[i].begin);
  }
  std::cout << "[flow-store:basic] Initialization completed." << std::endl;

  return true;
}
}  // namespace hdtn
