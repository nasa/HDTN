#ifndef _TCPCL_BUNDLE_SOURCE_H
#define _TCPCL_BUNDLE_SOURCE_H 1

#include <string>
#include <boost/thread.hpp>
#include <boost/asio.hpp>
#include <map>
#include <vector>
#include "Tcpcl.h"
#include "TcpAsyncSender.h"
#include "CircularIndexBufferSingleProducerSingleConsumerConfigurable.h"

//tcpcl
class TcpclBundleSource {
private:
    TcpclBundleSource();
public:
    typedef boost::function<void()> OnSuccessfulAckCallback_t;
    TcpclBundleSource(const uint16_t desiredKeeAliveIntervlSeconds, const std::string & thisEidString, const unsigned int maxUnacked = 100);

    ~TcpclBundleSource();
    void Stop();
    bool Forward(const uint8_t* bundleData, const std::size_t size);
    bool Forward(zmq::message_t & dataZmq);
    bool Forward(std::vector<uint8_t> & dataVec);
    std::size_t GetTotalDataSegmentsAcked();
    std::size_t GetTotalDataSegmentsSent();
    std::size_t GetTotalDataSegmentsUnacked();
    std::size_t GetTotalBundleBytesAcked();
    std::size_t GetTotalBundleBytesSent();
    std::size_t GetTotalBundleBytesUnacked();
    void Connect(const std::string & hostname, const std::string & port);
    bool ReadyToForward();
    void SetOnSuccessfulAckCallback(const OnSuccessfulAckCallback_t & callback);
private:
    void OnResolve(const boost::system::error_code & ec, boost::asio::ip::tcp::resolver::results_type results);
    void OnConnect(const boost::system::error_code & ec);
    void HandleTcpSend(const boost::system::error_code& error, std::size_t bytes_transferred);
    void HandleTcpSendShutdown(const boost::system::error_code& error, std::size_t bytes_transferred);
    void StartTcpReceive();
    void HandleTcpReceiveSome(const boost::system::error_code & error, std::size_t bytesTransferred);
    void OnNoKeepAlivePacketReceived_TimerExpired(const boost::system::error_code& e);
    void OnNeedToSendKeepAliveMessage_TimerExpired(const boost::system::error_code& e);
    void OnHandleSocketShutdown_TimerCancelled(const boost::system::error_code& e);
    void OnSendShutdownMessageTimeout_TimerExpired(const boost::system::error_code& e);
    void DoTcpclShutdown(bool sendShutdownMessage, bool reasonWasTimeOut);

    //tcpcl received data callback functions
    void ContactHeaderCallback(CONTACT_HEADER_FLAGS flags, uint16_t keepAliveIntervalSeconds, const std::string & localEid);
    void DataSegmentCallback(std::vector<uint8_t> & dataSegmentDataVec, bool isStartFlag, bool isEndFlag);
    void AckCallback(uint64_t totalBytesAcknowledged);
    void BundleRefusalCallback(BUNDLE_REFUSAL_CODES refusalCode);
    void NextBundleLengthCallback(uint64_t nextBundleLength);
    void KeepAliveCallback();
    void ShutdownCallback(bool hasReasonCode, SHUTDOWN_REASON_CODES shutdownReasonCode,
                                             bool hasReconnectionDelay, uint64_t reconnectionDelaySeconds);


    std::unique_ptr<TcpAsyncSender> m_tcpAsyncSenderPtr;
    TcpAsyncSenderElement::OnSuccessfulSendCallbackByIoServiceThread_t m_handleTcpSendCallback;
    TcpAsyncSenderElement::OnSuccessfulSendCallbackByIoServiceThread_t m_handleTcpSendShutdownCallback;

    Tcpcl m_tcpcl;
    boost::asio::io_service m_ioService;
    boost::asio::io_service::work m_work;
    boost::asio::ip::tcp::resolver m_resolver;
    boost::asio::deadline_timer m_noKeepAlivePacketReceivedTimer;
    boost::asio::deadline_timer m_needToSendKeepAliveMessageTimer;
    boost::asio::deadline_timer m_handleSocketShutdownCancelOnlyTimer;
    boost::asio::deadline_timer m_sendShutdownMessageTimeoutTimer;
    boost::shared_ptr<boost::asio::ip::tcp::socket> m_tcpSocketPtr;
    std::unique_ptr<boost::thread> m_ioServiceThreadPtr;
    boost::condition_variable m_localConditionVariableAckReceived;

    //tcpcl vars
    CONTACT_HEADER_FLAGS m_contactHeaderFlags;
    std::string m_localEid;
    uint16_t m_keepAliveIntervalSeconds;
    const unsigned int MAX_UNACKED;
    CircularIndexBufferSingleProducerSingleConsumerConfigurable m_bytesToAckCb;
    std::vector<uint64_t> m_bytesToAckCbVec;
    volatile bool m_readyToForward;
    volatile bool m_tcpclShutdownComplete;
    volatile bool m_sendShutdownMessage;
    volatile bool m_reasonWasTimeOut;
    volatile bool m_useLocalConditionVariableAckReceived;
    const uint16_t M_DESIRED_KEEPALIVE_INTERVAL_SECONDS;
    const std::string M_THIS_EID_STRING;
    OnSuccessfulAckCallback_t m_onSuccessfulAckCallback;

    uint8_t m_tcpReadSomeBuffer[2000];

public:
    //tcpcl stats
    std::size_t m_totalDataSegmentsAcked;
    std::size_t m_totalBytesAcked;
    std::size_t m_totalDataSegmentsSent;
    std::size_t m_totalBundleBytesSent;
};



#endif //_TCPCL_BUNDLE_SOURCE_H
