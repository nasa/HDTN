#ifndef _HDTN_STORE_H
#define _HDTN_STORE_H

#include <sys/time.h>

#include <map>
#include <string>
#include <vector>

#include "cache.hpp"
#include "paths.hpp"
#include "reg.hpp"
#include "stats.hpp"
#include "zmq.hpp"

#define HDTN_STORAGE_TELEM_FLOWCOUNT (4)
#define HDTN_STORAGE_PORT_DEFAULT (10425)
#define HDTN_BLOSC_MAXBLOCKSZ (1000000)
#define HDTN_FLOWCOUNT_MAX (16777216)

namespace hdtn {

struct schedule_event {
    double ts;  // time at which this event will trigger
    uint32_t type;
    uint32_t flow;
    uint64_t rate;      //  bytes / sec
    uint64_t duration;  //  msec
};

struct storageConfig {
    storageConfig()
        : telem(HDTN_STORAGE_TELEM_PATH), worker(HDTN_STORAGE_WORKER_PATH), releaseWorker(HDTN_SCHEDULER_PATH) {}

    /**
         * 0mq endpoint for registration server
         */
    std::string regsvr;
    /**
         * 0mq endpoint for storage service
         */
    std::string local;
    /**
         * Filesystem location for flow / data storage
         */
    std::string storePath;
    /**
         * 0mq endpoint for local telemetry service
         */
    std::string telem;
    /**
         * 0mq inproc endpoint for worker's use
         */
    std::string worker;
    std::string releaseWorker;
};

class storage_worker {
   public:
    ~storage_worker();
    void init(zmq::context_t *ctx, storageConfig config);
    void launch();
    void *execute(void *arg);
    pthread_t *thread() { return &storageThread; }
    void write(hdtn::block_hdr *hdr, zmq::message_t *message);
    void releaseData(uint32_t flow, uint64_t rate, uint64_t duration, zmq::socket_t *egressSock);
    hdtn::worker_stats stats() { return workerStats; }

   private:
    zmq::context_t *zmqContext;
    pthread_t storageThread;
    std::string root;
    std::string queue;
    char *outBuf;
    hdtn::flow_store storeFlow;
    hdtn::worker_stats workerStats;
};

class storage {
   public:
    bool init(storageConfig config);
    void update();
    void dispatch();
    void scheduleRelease();
    void c2telem();
    bool ingress(std::string remote);
    storage_stats *stats() { return &storageStats; }

   private:
    zmq::context_t *zmqContext;
    zmq::socket_t *ingressSock;
    zmq::socket_t *releaseSock;
    hdtn_regsvr storeReg;
    hdtn_regsvr telemReg;
    uint16_t port;
    zmq::socket_t *workerSock;
    zmq::socket_t *telemetrySock;
    storage_worker worker;

    storage_stats storageStats;
};

}  // namespace hdtn

#endif
