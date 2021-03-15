#ifndef _HDTN_INGRESS_H
#define _HDTN_INGRESS_H

#include <stdint.h>

#include "message.hpp"
#include "paths.hpp"
//#include "util/tsc.h"
#include "zmq.hpp"

#include "CircularIndexBufferSingleProducerSingleConsumerConfigurable.h"
#include <boost/asio.hpp>
#include <boost/thread.hpp>
#include "TcpclBundleSink.h"
#include "StcpBundleSink.h"
#include "UdpBundleSink.h"
#include <list>
#include <queue>

// Used to receive multiple datagrams (e.g. recvmmsg)
#define BP_INGRESS_STRBUF_SZ (8192)
#define BP_INGRESS_MSG_NBUF (32)
#define BP_INGRESS_MSG_BUFSZ (65536)
#define BP_INGRESS_USE_SYSCALL (1)
#define BP_INGRESS_TYPE_UDP (0x01)
#define BP_INGRESS_TYPE_STCP (0x02)

namespace hdtn {
std::string Datetime();

typedef struct BpMmsgbuf {
    uint32_t nbuf;
    uint32_t bufsz;
    struct mmsghdr *hdr;
    struct iovec *io;
    char *srcbuf;
} BpMmsgbuf;

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

class BpIngressSyscall {
public:
    BpIngressSyscall();  // initialize message buffers
    ~BpIngressSyscall();
    void Stop();
    int Init(uint32_t type);
    int Netstart(uint16_t port, bool useTcpcl, bool useStcp, bool alwaysSendToStorage);
    int send_telemetry();
    void RemoveInactiveTcpConnections();
private:
    int Process(std::vector<uint8_t> && rxBuf);
    void ReadZmqAcksThreadFunc();

    void UdpWholeBundleReadyCallback(std::vector<uint8_t> & wholeBundleVec);
    void TcpclWholeBundleReadyCallback(boost::shared_ptr<std::vector<uint8_t> > wholeBundleSharedPtr);
    void StartTcpAccept();
    void HandleTcpAccept(boost::shared_ptr<boost::asio::ip::tcp::socket> newTcpSocketPtr, const boost::system::error_code& error);

public:
    uint64_t m_bundleCountStorage = 0;
    uint64_t m_bundleCountEgress = 0;
    uint64_t m_bundleCount = 0;
    uint64_t m_bundleData = 0;
    uint64_t m_zmsgsIn = 0;
    uint64_t m_zmsgsOut = 0;
    uint64_t m_ingSequenceNum = 0;
    double m_elapsed = 0;
    bool m_forceStorage = false;

private:
    struct EgressToIngressAckingQueue {
        EgressToIngressAckingQueue() {

        }
        std::size_t GetQueueSize() {
            return m_blockHdrQueue.size();
        }
        void Push_ThreadSafe(const BlockHdr & blk) {
            boost::mutex::scoped_lock lock(m_mutex);
            m_blockHdrQueue.push(blk);
        }
        bool CompareAndPop_ThreadSafe(const BlockHdr & blk) {
            boost::mutex::scoped_lock lock(m_mutex);
            if (m_blockHdrQueue.empty()) {
                return false;
            }
            else if (m_blockHdrQueue.front() == blk) {
                m_blockHdrQueue.pop();
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
        std::queue<BlockHdr> m_blockHdrQueue;
    };

    std::unique_ptr<zmq::context_t> m_zmqCtx_ingressEgressPtr;
    std::unique_ptr<zmq::socket_t> m_zmqPushSock_boundIngressToConnectingEgressPtr;
    std::unique_ptr<zmq::socket_t> m_zmqPullSock_connectingEgressToBoundIngressPtr;
    std::unique_ptr<zmq::context_t> m_zmqCtx_ingressStoragePtr;
    std::unique_ptr<zmq::socket_t> m_zmqPushSock_boundIngressToConnectingStoragePtr;
    std::unique_ptr<zmq::socket_t> m_zmqPullSock_connectingStorageToBoundIngressPtr;
    //boost::shared_ptr<zmq::context_t> m_zmqTelemCtx;
    //boost::shared_ptr<zmq::socket_t> m_zmqTelemSock;
    int m_type;
    boost::asio::io_service m_ioService;
    std::unique_ptr<boost::asio::ip::tcp::acceptor> m_tcpAcceptorPtr;

    std::list<std::unique_ptr<TcpclBundleSink> > m_listTcpclBundleSinkPtrs;
    std::list<std::unique_ptr<StcpBundleSink> > m_listStcpBundleSinkPtrs;
    std::unique_ptr<UdpBundleSink> m_udpBundleSinkPtr;
    
    std::unique_ptr<boost::thread> m_threadZmqAckReaderPtr;
    std::unique_ptr<boost::thread> m_ioServiceThreadPtr;
    std::queue<BlockHdr> m_storageAckQueue;
    boost::mutex m_storageAckQueueMutex;
    boost::condition_variable m_conditionVariableStorageAckReceived;
    std::map<uint64_t, EgressToIngressAckingQueue> m_egressAckMapQueue; //flow id to queue
    boost::mutex m_egressAckMapQueueMutex;
    boost::mutex m_ingressToEgressZmqSocketMutex;
    std::size_t m_eventsTooManyInStorageQueue;
    std::size_t m_eventsTooManyInEgressQueue;
    volatile bool m_running;
    bool m_useTcpcl;
    bool m_useStcp;
    bool m_alwaysSendToStorage;
};

// use an explicit typedef to avoid runtime vcall overhead
#ifdef BP_INGRESS_USE_SYSCALL
typedef BpIngressSyscall BpIngress;
#endif

}  // namespace hdtn

#endif  //_HDTN_INGRESS_H
