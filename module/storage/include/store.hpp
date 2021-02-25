#ifndef _HDTN_STORE_H
#define _HDTN_STORE_H

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
#define HDTN_BLOSC_MAXBLOCKSZ (1 << 26)
#define HDTN_FLOWCOUNT_MAX (16777216)

namespace hdtn {

struct ScheduleEvent {
    double ts;  // time at which this event will trigger
    uint32_t type;
    uint32_t flow;
    uint64_t rate;      //  bytes / sec
    uint64_t duration;  //  msec
};

typedef std::vector<ScheduleEvent> ReleaseList;

class Scheduler {
public:
    void Init();
    void Add(CscheduleHdr *hdr);
    ScheduleEvent *Next();

private:
    ReleaseList m_schedule;
};

struct StorageConfig {
    StorageConfig() : telem(HDTN_STORAGE_TELEM_PATH), worker(HDTN_STORAGE_WORKER_PATH) {}

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
};

class StorageWorker {
public:
    ~StorageWorker();
    void Init(zmq::context_t *ctx, StorageConfig config);
    void Launch();
    void *Execute(void *arg);
    pthread_t *Thread() { return &m_thread; }
    void Write(hdtn::BlockHdr *hdr, zmq::message_t *message);

private:
    zmq::context_t *m_ctx;
    pthread_t m_thread;
    std::string m_root;
    std::string m_queue;
    char *m_outBuf;
    hdtn::FlowStore m_store;
};

class Storage {
public:
    bool Init(StorageConfig config);
    void Update();
    void Dispatch();
    void C2telem();
    void Release(uint32_t flow, uint64_t rate, uint64_t duration);
    bool Ingress(std::string remote);
    StorageStats *Stats() { return &m_stats; }

private:
    zmq::context_t *m_ctx;
    zmq::socket_t *m_ingressSock;
    HdtnRegsvr m_storeReg;
    HdtnRegsvr m_telemReg;
    uint16_t m_port;
    zmq::socket_t *m_egressSock;
    zmq::socket_t *m_workerSock;
    zmq::socket_t *m_telemetrySock;
    StorageWorker m_worker;
    StorageStats m_stats;
};
}  // namespace hdtn

#endif
