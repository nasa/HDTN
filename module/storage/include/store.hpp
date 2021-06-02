#ifndef _HDTN_STORE_H
#define _HDTN_STORE_H

#include <map>
#include <string>
#include <vector>
#include <boost/shared_ptr.hpp>
#include <boost/thread.hpp>

//#include "cache.hpp"
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
        : telem(HDTN_STORAGE_TELEM_PATH), worker(HDTN_STORAGE_WORKER_PATH), releaseWorker(HDTN_BOUND_SCHEDULER_PUBSUB_PATH) {}

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

class ZmqStorageInterface {
   public:
    ZmqStorageInterface();
    ~ZmqStorageInterface();
    void Stop();
    void init(zmq::context_t *ctx, const storageConfig & config);
    void launch();
    //pthread_t *thread() { return &storageThread; }

    WorkerStats stats() { return m_workerStats; }

    std::size_t m_totalBundlesErasedFromStorage = 0;
    std::size_t m_totalBundlesSentToEgressFromStorage = 0;

private:
    void ThreadFunc();
    //void Write(hdtn::block_hdr *hdr, zmq::message_t *message);
    //void ReleaseData(uint32_t flow, uint64_t rate, uint64_t duration, zmq::socket_t *egressSock);

   private:
    zmq::context_t *m_zmqContextPtr;
    std::unique_ptr<boost::thread> m_threadPtr;
    std::string m_storageConfigFilePath;
    std::string m_queue;
    volatile bool m_running;
    WorkerStats m_workerStats;
};


class storage {
public:
    storage();
    ~storage();
    void Stop();
    bool init(const storageConfig & config);
    void update();
    void dispatch();
    void scheduleRelease();
    void c2telem();
    StorageStats *stats() { return &storageStats; }
    std::size_t GetCurrentNumberOfBundlesDeletedFromStorage();
    std::size_t m_totalBundlesErasedFromStorage = 0;
    std::size_t m_totalBundlesSentToEgressFromStorage = 0;

private:
    std::unique_ptr<zmq::context_t> m_zmqContextPtr;
    std::unique_ptr<zmq::socket_t> m_zmqPullSock_boundIngressToConnectingStoragePtr;
    std::unique_ptr<zmq::socket_t> m_zmqSubSock_boundReleaseToConnectingStoragePtr;
    uint16_t port;
    std::unique_ptr<zmq::socket_t> m_workerSockPtr;
    std::unique_ptr<zmq::socket_t> m_telemetrySockPtr;
    ZmqStorageInterface worker;
    StorageStats storageStats;
};

}  // namespace hdtn

#endif
