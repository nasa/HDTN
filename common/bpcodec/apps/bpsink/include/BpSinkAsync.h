#ifndef _BP_SINK_ASYNC_H
#define _BP_SINK_ASYNC_H

#include <stdint.h>
#include <sys/time.h>

//#include "message.hpp"
//#include "paths.hpp"



#include "CircularIndexBufferSingleProducerSingleConsumerConfigurable.h"
#include <boost/asio.hpp>
#include <boost/thread.hpp>
#include "Tcpcl.h"

namespace hdtn {


class BpSinkAsync {
private:
    BpSinkAsync();
public:
    BpSinkAsync(uint16_t port, bool useTcp);  // initialize message buffers
    ~BpSinkAsync();
    int Init(uint32_t type);
    int Netstart();
    //int send_telemetry();
private:
    int Process(const std::vector<uint8_t> & rxBuf, const std::size_t messageSize);
    void StartUdpReceive();
    void HandleUdpReceive(const boost::system::error_code & error, std::size_t bytesTransferred, unsigned int writeIndex);
    void PopCbThreadFunc();

    void StartTcpAccept();
    void HandleTcpAccept(boost::shared_ptr<boost::asio::ip::tcp::socket> newTcpSocketPtr, const boost::system::error_code& error);
    void StartTcpReceive();
    void HandleTcpReceiveSome(const boost::system::error_code & error, std::size_t bytesTransferred, unsigned int writeIndex);
    void HandleTcpSend(boost::shared_ptr<std::vector<boost::uint8_t> > dataSentPtr, const boost::system::error_code& error, std::size_t bytes_transferred, bool closeSocket);
    void OnNoKeepAlivePacketReceived_TimerExpired(const boost::system::error_code& e);
    void OnNeedToSendKeepAliveMessage_TimerExpired(const boost::system::error_code& e);
    void ShutdownAndCloseTcpSocket();

    //tcpcl received data callback functions
    void ContactHeaderCallback(CONTACT_HEADER_FLAGS flags, uint16_t keepAliveIntervalSeconds, const std::string & localEid);
    void DataSegmentCallback(boost::shared_ptr<std::vector<uint8_t> > dataSegmentDataSharedPtr, bool isStartFlag, bool isEndFlag);
    void AckCallback(uint32_t totalBytesAcknowledged);
    void BundleRefusalCallback(BUNDLE_REFUSAL_CODES refusalCode);
    void NextBundleLengthCallback(uint32_t nextBundleLength);
    void KeepAliveCallback();
    void ShutdownCallback(bool hasReasonCode, SHUTDOWN_REASON_CODES shutdownReasonCode,
                                             bool hasReconnectionDelay, uint32_t reconnectionDelaySeconds);

    //tcpcl vars
    CONTACT_HEADER_FLAGS m_contactHeaderFlags;
    uint16_t m_keepAliveIntervalSeconds;
    std::string m_localEid;
    std::vector<uint8_t> m_fragmentedBundleRxConcat;

public:
    uint32_t m_batch;

    uint64_t m_tscTotal;
    int64_t m_rtTotal;
    uint64_t m_totalBytesRx;

    uint64_t m_receivedCount;
    uint64_t m_duplicateCount;
    uint64_t m_seqHval;
    uint64_t m_seqBase;

private:
    const uint16_t m_rxPortUdpOrTcp;
    const bool m_useTcp;
    Tcpcl m_tcpcl;

    int m_type;
    boost::asio::io_service m_ioService;
    boost::asio::ip::udp::socket m_udpSocket;
    boost::asio::ip::tcp::acceptor m_tcpAcceptor;
    boost::asio::deadline_timer m_noKeepAlivePacketReceivedTimer;
    boost::asio::deadline_timer m_needToSendKeepAliveMessageTimer;
    boost::shared_ptr<boost::asio::ip::tcp::socket> m_tcpSocketPtr;
    CircularIndexBufferSingleProducerSingleConsumerConfigurable m_circularIndexBuffer;
    std::vector<std::vector<boost::uint8_t> > m_udpReceiveBuffersCbVec;
    std::vector<boost::asio::ip::udp::endpoint> m_remoteEndpointsCbVec;
    std::vector<std::size_t> m_udpReceiveBytesTransferredCbVec;
    boost::condition_variable m_conditionVariableCb;
    boost::shared_ptr<boost::thread> m_threadCbReaderPtr;
    boost::shared_ptr<boost::thread> m_ioServiceThreadPtr;
    volatile bool m_running;
};


}  // namespace hdtn

#endif  //_BP_SINK_ASYNC_H
