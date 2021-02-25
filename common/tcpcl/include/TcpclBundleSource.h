#ifndef _TCPCL_BUNDLE_SOURCE_H
#define _TCPCL_BUNDLE_SOURCE_H 1

#include <string>
#include <boost/thread.hpp>
#include <boost/asio.hpp>
#include <map>
#include <queue>
#include "Tcpcl.h"

//tcpcl
class TcpclBundleSource {
private:
    TcpclBundleSource();
public:
    TcpclBundleSource(const uint16_t desiredKeeAliveIntervlSeconds, const std::string & thisEidString);

    ~TcpclBundleSource();
    bool Forward(const uint8_t* bundleData, const std::size_t size);
    void Connect(const std::string & hostname, const std::string & port);
    bool ReadyToForward();
private:
    void OnResolve(const boost::system::error_code & ec, boost::asio::ip::tcp::resolver::results_type results);
    void OnConnect(const boost::system::error_code & ec);
    void HandleTcpSend(boost::shared_ptr<std::vector<boost::uint8_t> > dataSentPtr, const boost::system::error_code& error, std::size_t bytes_transferred);
    void HandleTcpSendShutdown(boost::shared_ptr<std::vector<boost::uint8_t> > dataSentPtr, const boost::system::error_code& error, std::size_t bytes_transferred);
    void StartTcpReceive();
    void HandleTcpReceiveSome(const boost::system::error_code & error, std::size_t bytesTransferred);
    void OnNoKeepAlivePacketReceived_TimerExpired(const boost::system::error_code& e);
    void OnNeedToSendKeepAliveMessage_TimerExpired(const boost::system::error_code& e);
    void OnHandleSocketShutdown_TimerCancelled(const boost::system::error_code& e);
    void OnSendShutdownMessageTimeout_TimerExpired(const boost::system::error_code& e);
    void DoTcpclShutdown(bool sendShutdownMessage, bool reasonWasTimeOut);

    //tcpcl received data callback functions
    void ContactHeaderCallback(CONTACT_HEADER_FLAGS flags, uint16_t keepAliveIntervalSeconds, const std::string & localEid);
    void DataSegmentCallback(boost::shared_ptr<std::vector<uint8_t> > dataSegmentDataSharedPtr, bool isStartFlag, bool isEndFlag);
    void AckCallback(uint32_t totalBytesAcknowledged);
    void BundleRefusalCallback(BUNDLE_REFUSAL_CODES refusalCode);
    void NextBundleLengthCallback(uint32_t nextBundleLength);
    void KeepAliveCallback();
    void ShutdownCallback(bool hasReasonCode, SHUTDOWN_REASON_CODES shutdownReasonCode,
                                             bool hasReconnectionDelay, uint32_t reconnectionDelaySeconds);



    Tcpcl m_tcpcl;
    boost::asio::io_service m_ioService;
    boost::asio::io_service::work m_work;
    boost::asio::ip::tcp::resolver m_resolver;
    boost::asio::deadline_timer m_noKeepAlivePacketReceivedTimer;
    boost::asio::deadline_timer m_needToSendKeepAliveMessageTimer;
    boost::asio::deadline_timer m_handleSocketShutdownCancelOnlyTimer;
    boost::asio::deadline_timer m_sendShutdownMessageTimeoutTimer;
    boost::shared_ptr<boost::asio::ip::tcp::socket> m_tcpSocketPtr;
    boost::shared_ptr<boost::thread> m_ioServiceThreadPtr;

    //tcpcl vars
    CONTACT_HEADER_FLAGS m_contactHeaderFlags;
    std::string m_localEid;
    uint16_t m_keepAliveIntervalSeconds;
    std::queue<uint32_t> m_bytesToAckQueue;
    volatile bool m_readyToForward;
    volatile bool m_tcpclShutdownComplete;
    volatile bool m_sendShutdownMessage;
    volatile bool m_reasonWasTimeOut;
    const uint16_t M_DESIRED_KEEPALIVE_INTERVAL_SECONDS;
    const std::string M_THIS_EID_STRING;

    uint8_t m_tcpReadSomeBuffer[2000];

public:
    //tcpcl stats
    std::size_t m_totalDataSegmentsAcked;
    std::size_t m_totalBytesAcked;
    std::size_t m_totalDataSegmentsSent;
    std::size_t m_totalBundleBytesSent;
};



#endif //_TCPCL_BUNDLE_SOURCE_H
