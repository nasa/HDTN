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
#include <list>

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
    int Init(uint32_t type);
    int Netstart(uint16_t port, bool useTcpcl, bool useStcp, bool alwaysSendToStorage);
    int send_telemetry();
    void RemoveInactiveTcpConnections();
private:
    int Process(const std::vector<uint8_t> & rxBuf, const std::size_t messageSize);
    void StartUdpReceive();
    void HandleUdpReceive(const boost::system::error_code & error, std::size_t bytesTransferred, unsigned int writeIndex);
    void PopCbThreadFunc();

    void TcpclWholeBundleReadyCallback(boost::shared_ptr<std::vector<uint8_t> > wholeBundleSharedPtr);
    void StartTcpAccept();
    void HandleTcpAccept(boost::shared_ptr<boost::asio::ip::tcp::socket> newTcpSocketPtr, const boost::system::error_code& error);

public:

    uint64_t m_bundleCount = 0;
    uint64_t m_bundleData = 0;
    uint64_t m_zmsgsIn = 0;
    uint64_t m_zmsgsOut = 0;
    uint64_t m_ingSequenceNum = 0;
    double m_elapsed = 0;
    bool m_forceStorage = false;

private:

    boost::shared_ptr<zmq::context_t> m_zmqCtx_boundIngressToConnectingEgressPtr;
    boost::shared_ptr<zmq::socket_t> m_zmqPushSock_boundIngressToConnectingEgressPtr;
    boost::shared_ptr<zmq::context_t> m_zmqCtx_boundIngressToConnectingStoragePtr;
    boost::shared_ptr<zmq::socket_t> m_zmqPushSock_boundIngressToConnectingStoragePtr;
    //boost::shared_ptr<zmq::context_t> m_zmqTelemCtx;
    //boost::shared_ptr<zmq::socket_t> m_zmqTelemSock;
    int m_type;
    boost::asio::io_service m_ioService;
    boost::asio::ip::udp::socket m_udpSocket;
    boost::shared_ptr<boost::asio::ip::tcp::acceptor> m_tcpAcceptorPtr;

    std::list<boost::shared_ptr<TcpclBundleSink> > m_listTcpclBundleSinkPtrs;
    std::list<boost::shared_ptr<StcpBundleSink> > m_listStcpBundleSinkPtrs;
    CircularIndexBufferSingleProducerSingleConsumerConfigurable m_circularIndexBuffer;
    std::vector<std::vector<boost::uint8_t> > m_udpReceiveBuffersCbVec;
    std::vector<boost::asio::ip::udp::endpoint> m_remoteEndpointsCbVec;
    std::vector<std::size_t> m_udpReceiveBytesTransferredCbVec;
    boost::condition_variable m_conditionVariableCb;
    boost::shared_ptr<boost::thread> m_threadCbReaderPtr;
    boost::shared_ptr<boost::thread> m_ioServiceThreadPtr;
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
