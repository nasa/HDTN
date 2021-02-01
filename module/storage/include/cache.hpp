#ifndef _HDTN_CACHE_H
#define _HDTN_CACHE_H

#include <map>
#include <string>

#include "message.hpp"
#include "stats.hpp"

#define HDTN_RECLAIM_THRESHOLD (1 << 28)

namespace hdtn {
struct FlowStoreHeader {
    uint64_t begin;
    uint64_t end;
};

struct FlowStoreEntry {
    int fd;
    FlowStoreHeader *header;  // mapped from the first N bytes of the corresponding file
};

typedef std::map<int, FlowStoreEntry> FlowMap;

class FlowStore {
public:
    ~FlowStore();
    bool Init(std::string root);
    FlowStoreEntry Load(int flow);
    int Write(int flow, void *data, int sz);
    int Read(int flow, void *data, int maxsz);
    FlowStats Stats() { return m_stats; }

private:
    FlowMap m_flow;
    std::string m_root;
    FlowStoreHeader *m_index;
    int m_indexFd;
    FlowStats m_stats;
};
}  // namespace hdtn

#endif
