#ifndef _HDTN_STORE_H
#define _HDTN_STORE_H

#include <map>
#include <string>
#include <vector>
#include <boost/shared_ptr.hpp>
#include <boost/thread.hpp>
#include "HdtnConfig.h"
//#include "cache.hpp"
#include "reg.hpp"
#include "stats.hpp"
#include "zmq.hpp"

#define HDTN_STORAGE_TELEM_FLOWCOUNT (4)
#define HDTN_STORAGE_PORT_DEFAULT (10425)
#define HDTN_BLOSC_MAXBLOCKSZ (1000000)
#define HDTN_FLOWCOUNT_MAX (16777216)

//addresses for ZMQ IPC transport
#define HDTN_STORAGE_BUNDLE_DATA_INPROC_PATH "inproc://hdtn.storage.bundledata"
#define HDTN_STORAGE_RELEASE_MESSAGES_INPROC_PATH "inproc://hdtn.storage.releasemessages"
#define HDTN_STORAGE_TELEM_PATH "tcp://127.0.0.1:10460"
#define HDTN_RELEASE_TELEM_PATH "tcp://127.0.0.1:10461"

namespace hdtn {

struct schedule_event {
    double ts;  // time at which this event will trigger
    uint32_t type;
    uint32_t flow;
    uint64_t rate;      //  bytes / sec
    uint64_t duration;  //  msec
};


class ZmqStorageInterface {
   public:
    ZmqStorageInterface();
    ~ZmqStorageInterface();
    void Stop();
    void Init(zmq::context_t *ctx, const HdtnConfig & hdtnConfig, zmq::context_t * hdtnOneProcessZmqInprocContextPtr = NULL);
    void launch();
    //pthread_t *thread() { return &storageThread; }

    WorkerStats stats() { return m_workerStats; }

    std::size_t m_totalBundlesErasedFromStorageNoCustodyTransfer;
    std::size_t m_totalBundlesErasedFromStorageWithCustodyTransfer;
    std::size_t m_totalBundlesSentToEgressFromStorage;
    uint64_t m_numRfc5050CustodyTransfers;
    uint64_t m_numAcsCustodyTransfers;
    uint64_t m_numAcsPacketsReceived;

private:
    void ThreadFunc();
    //void Write(hdtn::block_hdr *hdr, zmq::message_t *message);
    //void ReleaseData(uint32_t flow, uint64_t rate, uint64_t duration, zmq::socket_t *egressSock);

   private:
    zmq::context_t *m_zmqContextPtr;
    zmq::context_t * m_hdtnOneProcessZmqInprocContextPtr;
    std::unique_ptr<boost::thread> m_threadPtr;
    volatile bool m_running;
    WorkerStats m_workerStats;
    HdtnConfig m_hdtnConfig;
};


class storage {
public:
    storage();
    ~storage();
    void Stop();
    bool Init(const HdtnConfig & hdtnConfig, zmq::context_t * hdtnOneProcessZmqInprocContextPtr = NULL);
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
    std::unique_ptr<zmq::socket_t> m_inprocBundleDataSockPtr;
    std::unique_ptr<zmq::socket_t> m_inprocReleaseMessagesSockPtr;
    std::unique_ptr<zmq::socket_t> m_telemetrySockPtr;
    ZmqStorageInterface worker;
    StorageStats storageStats;
    HdtnConfig m_hdtnConfig;
    zmq::pollitem_t m_pollItems[3];
};

}  // namespace hdtn

#endif
