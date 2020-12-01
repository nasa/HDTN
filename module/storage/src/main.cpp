#include <sys/time.h>
#include <iostream>
#include "store.hpp"

static uint64_t _last_bytes;
static uint64_t _last_count;

int main(int argc, char* argv[]) {
    double last = 0.0;
    timeval tv;
    gettimeofday(&tv, NULL);
    last = (tv.tv_sec + (tv.tv_usec / 1000000.0));
    hdtn3::storage_config config;
    config.regsvr = "tcp://127.0.0.1:10140";
    config.local = "tcp://127.0.0.1:10145";
    config.store_path = "/var/lib/hdtn3.store";
    hdtn3::storage store;
    std::cout << "[store] Initializing storage manager ..." << std::endl;
    if(!store.init(config)) {
        return -1;
    }
    while(true) {
        store.update();
        gettimeofday(&tv, NULL);
        double curr = (tv.tv_sec + (tv.tv_usec / 1000000.0));
        if(curr - last > 1) {
            last = curr;
            hdtn3::storage_stats* stats = store.stats();
            uint64_t cbytes = stats->in_bytes - _last_bytes;
            uint64_t ccount = stats->in_msg - _last_count;
            _last_bytes = stats->in_bytes;
            _last_count = stats->in_msg;

            printf("[store] Received: %d msg / %0.2f MB\n", ccount, cbytes / (1024.0 * 1024.0));
        }
    }

    return 0;
}
