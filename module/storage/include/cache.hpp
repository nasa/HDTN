#ifndef _HDTN3_CACHE_H
#define _HDTN3_CACHE_H

#include <string>
#include <map>
#include "stats.hpp"
#include "message.hpp"

#define HDTN3_RECLAIM_THRESHOLD  (1 << 28)

namespace hdtn3 {
    struct flow_store_header {
        uint64_t  begin;
        uint64_t  end;
    };

    struct flow_store_entry {
        int                    fd;
        flow_store_header*     header;  // mapped from the first N bytes of the corresponding file
    };

    typedef std::map<int, flow_store_entry> flow_map;

    class flow_store {
    public:
        ~flow_store();
        bool                init(std::string root);
        flow_store_entry    load(int flow);
        int                 write(int flow, void* data, int sz);
        int                 read(int flow, void* data, int maxsz);
        flow_stats          stats() { return _stats; }

    private:
        flow_map                  _flow;
        std::string               _root;
        flow_store_header*        _index;
        int                       _index_fd;
        flow_stats                _stats;
    };
}

#endif
