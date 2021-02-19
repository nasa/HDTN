

#include <iostream>
#include <vector>

#include "store.hpp"
#include "message.hpp"
#include "SignalHandler.h"

static volatile bool g_running = true;

static void MonitorExitKeypressThreadFunction() {
    std::cout << "Keyboard Interrupt.. exiting\n";
    g_running = false; //do this first
}

static SignalHandler g_sigHandler(boost::bind(&MonitorExitKeypressThreadFunction));

static uint64_t _last_bytes;
static uint64_t _last_count;

int main(int argc, char *argv[]) {

    //scope to ensure clean exit before return 0
    {
        //double last = 0.0;
        //timeval tv;
        //gettimeofday(&tv, NULL);
        //last = (tv.tv_sec + (tv.tv_usec / 1000000.0));
        hdtn::storageConfig config;
        config.regsvr = HDTN_REG_SERVER_PATH;
        config.local = HDTN_RELEASE_PATH;
        config.releaseWorker = HDTN_SCHEDULER_PATH;
        config.storePath = "/home/hdtn/hdtn.store";
        hdtn::storage store;
        std::cout << "[store] Initializing storage manager ..." << std::endl;


        if (!store.init(config)) {
            return -1;
        }

        g_sigHandler.Start(false);
        while (g_running) {
            store.update();
            //gettimeofday(&tv, NULL);
            //double curr = (tv.tv_sec + (tv.tv_usec / 1000000.0));
            /*if (curr - last > 1) {
                last = curr;
                hdtn::StorageStats *stats = store.stats();
                uint64_t cbytes = stats->inBytes - _last_bytes;
                uint64_t ccount = stats->inMsg - _last_count;
                _last_bytes = stats->inBytes;
                _last_count = stats->inMsg;
                std::cout<< "Bytes read:"<< stats->worker.flow.disk_rbytes << " Bytes written: "<< stats->worker.flow.disk_wbytes <<std::endl;
                std::cout<<  "Mbps released: "<< stats->worker.flow.read_rate << "in " << stats->worker.flow.read_ts << " sec" <<std::endl;
                printf("[store] Received: %lu msg / %0.2f MB\n", ccount, cbytes / (1024.0 * 1024.0));
            }*/
            g_sigHandler.PollOnce();
        }
        std::cout<< "Storage main.cpp: exiting cleanly..\n";
    }
    std::cout<< "Storage main.cpp: exited cleanly\n";
    return 0;
}
