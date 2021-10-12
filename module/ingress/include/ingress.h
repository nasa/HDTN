#ifndef _HDTN_INGRESS_H
#define _HDTN_INGRESS_H

#include <stdint.h>

#include "message.hpp"
//#include "util/tsc.h"
#include "zmq.hpp"

#include "CircularIndexBufferSingleProducerSingleConsumerConfigurable.h"
#include <boost/asio.hpp>
#include <boost/thread.hpp>
#include "HdtnConfig.h"
#include "InductManager.h"
#include <list>
#include <queue>
#include <boost/atomic.hpp>

// Used to receive multiple datagrams (e.g. recvmmsg)
#define BP_INGRESS_STRBUF_SZ (8192)
#define BP_INGRESS_MSG_NBUF (32)
#define BP_INGRESS_MSG_BUFSZ (65536)
#define BP_INGRESS_USE_SYSCALL (1)
#define BP_INGRESS_TYPE_UDP (0x01)
#define BP_INGRESS_TYPE_STCP (0x02)

namespace hdtn {

typedef struct IngressTelemetry {
    uint64_t totalBundles;
    uint64_t totalBytes;
    uint64_t totalZmsgsIn;
    uint64_t totalZmsgsOut;
    uint64_t bundlesSecIn;
    uint64_t mBitsSecIn;
    uint64_t zmsgsSecIn;
    uint64_t zmsgsSecOut;
    double elapsed;
} IngressTelemetry;

class Ingress {
public:
    Ingress();  // initialize message buffers
    ~Ingress();
    void Stop();
    void SchedulerEventHandler();
    int Init(const HdtnConfig & hdtnConfig, const bool isCutThroughOnlyTest,
             zmq::context_t * hdtnOneProcessZmqInprocContextPtr = NULL);
private:
    bool Process(std::vector<uint8_t> && rxBuf);
    void ReadZmqAcksThreadFunc();
    void WholeBundleReadyCallback(std::vector<uint8_t> & wholeBundleVec);

public:
    uint64_t m_bundleCountStorage;
    boost::atomic_uint64_t m_bundleCountEgress;
    uint64_t m_bundleCount;
    boost::atomic_uint64_t m_bundleData;
    double m_elapsed;

private:
    struct EgressToIngressAckingQueue {
        EgressToIngressAckingQueue() {

        }
        std::size_t GetQueueSize() {
            return m_ingressToEgressCustodyIdQueue.size();
        }
        void PushMove_ThreadSafe(const uint64_t ingressToEgressCustody) {
            boost::mutex::scoped_lock lock(m_mutex);
            m_ingressToEgressCustodyIdQueue.push(ingressToEgressCustody);
        }
        bool CompareAndPop_ThreadSafe(const uint64_t ingressToEgressCustody) {
            boost::mutex::scoped_lock lock(m_mutex);
            if (m_ingressToEgressCustodyIdQueue.empty()) {
                return false;
            }
            else if (m_ingressToEgressCustodyIdQueue.front() == ingressToEgressCustody) {
                m_ingressToEgressCustodyIdQueue.pop();
                return true;
            }
            return false;
        }
        void WaitUntilNotifiedOr250MsTimeout() {
            boost::mutex::scoped_lock lock(m_mutex);
            m_conditionVariable.timed_wait(lock, boost::posix_time::milliseconds(250)); // call lock.unlock() and blocks the current thread
        }
        void NotifyAll() {            
            m_conditionVariable.notify_all();
        }
        boost::mutex m_mutex;
        boost::condition_variable m_conditionVariable;
        std::queue<uint64_t> m_ingressToEgressCustodyIdQueue;
    };

    std::unique_ptr<zmq::context_t> m_zmqCtx_ingressEgressPtr;
    std::unique_ptr<zmq::socket_t> m_zmqPushSock_boundIngressToConnectingEgressPtr;
    std::unique_ptr<zmq::socket_t> m_zmqPullSock_connectingEgressToBoundIngressPtr;
    std::unique_ptr<zmq::context_t> m_zmqCtx_ingressStoragePtr;
    std::unique_ptr<zmq::socket_t> m_zmqPushSock_boundIngressToConnectingStoragePtr;
    std::unique_ptr<zmq::socket_t> m_zmqPullSock_connectingStorageToBoundIngressPtr;
   
     std::unique_ptr<zmq::context_t> m_zmqCtx_schedulerIngressPtr;
     std::unique_ptr<zmq::socket_t> m_zmqSubSock_boundSchedulerToConnectingIngressPtr;

    //boost::shared_ptr<zmq::context_t> m_zmqTelemCtx;
    //boost::shared_ptr<zmq::socket_t> m_zmqTelemSock;

    InductManager m_inductManager;
    HdtnConfig m_hdtnConfig;
    cbhe_eid_t M_HDTN_EID_CUSTODY;
    boost::posix_time::time_duration M_MAX_INGRESS_BUNDLE_WAIT_ON_EGRESS_TIME_DURATION;
    
    std::unique_ptr<boost::thread> m_threadZmqAckReaderPtr;
    std::queue<uint64_t> m_storageAckQueue;
    boost::mutex m_storageAckQueueMutex;
    boost::condition_variable m_conditionVariableStorageAckReceived;
    std::map<cbhe_eid_t, EgressToIngressAckingQueue> m_egressAckMapQueue; //final dest id to queue
    boost::mutex m_egressAckMapQueueMutex;
    boost::mutex m_ingressToEgressZmqSocketMutex;
    boost::mutex m_eidAvailableSetMutex;
    std::size_t m_eventsTooManyInStorageQueue;
    std::size_t m_eventsTooManyInEgressQueue;
    volatile bool m_running;
    bool m_isCutThroughOnlyTest;
    boost::atomic_uint64_t m_ingressToEgressNextUniqueIdAtomic;
    uint64_t m_ingressToStorageNextUniqueId;
    std::set<cbhe_eid_t> m_finalDestEidAvailableSet;
    std::vector<uint64_t> m_schedulerRxBufPtrToStdVec64;
};


}  // namespace hdtn

#endif  //_HDTN_INGRESS_H
