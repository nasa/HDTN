

#include <iostream>
#include <vector>

#include "store.hpp"
#include "message.hpp"

static uint64_t _last_bytes;
static uint64_t _last_count;

int main(int argc, char *argv[]) {
    double last = 0.0;
    timeval tv;
    gettimeofday(&tv, NULL);
    last = (tv.tv_sec + (tv.tv_usec / 1000000.0));
    hdtn::storageConfig config;
    config.regsvr = HDTN_REG_SERVER_PATH;
    config.local = HDTN_RELEASE_PATH;
    config.storePath = "/home/hdtn/hdtn.store";
    hdtn::storage store;
    std::cout << "[store] Initializing storage manager ..." << std::endl;
    

    if (!store.init(config)) {
        return -1;
    }

    while (true) {
        store.update();
        gettimeofday(&tv, NULL);
        double curr = (tv.tv_sec + (tv.tv_usec / 1000000.0));
        if (curr - last > 1) {
            last = curr;
            hdtn::storage_stats *stats = store.stats();
            uint64_t cbytes = stats->in_bytes - _last_bytes;
            uint64_t ccount = stats->in_msg - _last_count;
            _last_bytes = stats->in_bytes;
            _last_count = stats->in_msg;
           // std::cout<< "Bytes read:"<< stats->worker.flow.disk_rbytes << " Bytes written: "<< stats->worker.flow.disk_wbytes <<std::endl;
            //std::cout<<  "Mbps released: "<< stats->worker.flow.read_rate << "in " << stats->worker.flow.read_ts << " sec" <<std::endl;
            //printf("[store] Received: %d msg / %0.2f MB\n", ccount, cbytes / (1024.0 * 1024.0));
        }
    }

    return 0;
}
