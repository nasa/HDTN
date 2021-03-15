#ifndef _STCP_BUNDLE_SOURCE_H
#define _STCP_BUNDLE_SOURCE_H 1

#include <string>
#include <boost/thread.hpp>
#include <boost/asio.hpp>
#include <map>
#include <queue>
#include "TcpAsyncSender.h"
#include "CircularIndexBufferSingleProducerSingleConsumerConfigurable.h"

//tcpcl
class StcpBundleSource {
private:
    StcpBundleSource();
public:
    typedef boost::function<void()> OnSuccessfulAckCallback_t;
    StcpBundleSource(const uint16_t desiredKeeAliveIntervalSeconds, const uint64_t rateBps = 50, const unsigned int maxUnacked = 100);

    ~StcpBundleSource();
    bool Forward(const uint8_t* bundleData, const std::size_t size);
    bool Forward(zmq::message_t & dataZmq);
    bool Forward(std::vector<uint8_t> & dataVec);
    std::size_t GetTotalDataSegmentsAcked();
    std::size_t GetTotalDataSegmentsSent();
    std::size_t GetTotalDataSegmentsUnacked();
    std::size_t GetTotalBundleBytesAcked();
    std::size_t GetTotalBundleBytesSent();
    std::size_t GetTotalBundleBytesUnacked();
    void UpdateRate(uint64_t rateBitsPerSec);
    void Connect(const std::string & hostname, const std::string & port);
    bool ReadyToForward();
    void SetOnSuccessfulAckCallback(const OnSuccessfulAckCallback_t & callback);
private:
    static void GenerateDataUnit(std::vector<uint8_t> & dataUnit, const uint8_t * contents, uint32_t sizeContents);
    static void GenerateDataUnitHeaderOnly(std::vector<uint8_t> & dataUnit, uint32_t sizeContents);
    void OnResolve(const boost::system::error_code & ec, boost::asio::ip::tcp::resolver::results_type results);
    void OnConnect(const boost::system::error_code & ec);
    void HandleTcpSend(const boost::system::error_code& error, std::size_t bytes_transferred);
    void HandleTcpSendKeepAlive(const boost::system::error_code& error, std::size_t bytes_transferred);
    void StartTcpReceive();
    void HandleTcpReceiveSome(const boost::system::error_code & error, std::size_t bytesTransferred);

    void RestartNewDataSignaler();
    void SignalNewDataForwarded();
    void OnNewData_TimerCancelled(const boost::system::error_code& e);
    void TryRestartRateTimer();
    void OnRate_TimerExpired(const boost::system::error_code& e);
    void OnNeedToSendKeepAliveMessage_TimerExpired(const boost::system::error_code& e);
    void DoStcpShutdown();

    
    std::unique_ptr<TcpAsyncSender> m_tcpAsyncSenderPtr;
    TcpAsyncSenderElement::OnSuccessfulSendCallbackByIoServiceThread_t m_handleTcpSendCallback;
    TcpAsyncSenderElement::OnSuccessfulSendCallbackByIoServiceThread_t m_handleTcpSendKeepAliveCallback;


    
    boost::asio::io_service m_ioService;
    boost::asio::io_service::work m_work;
    boost::asio::ip::tcp::resolver m_resolver;
    boost::asio::deadline_timer m_needToSendKeepAliveMessageTimer;
    boost::asio::deadline_timer m_rateTimer;
    boost::asio::deadline_timer m_newDataSignalerTimer;
    boost::shared_ptr<boost::asio::ip::tcp::socket> m_tcpSocketPtr;
    boost::shared_ptr<boost::thread> m_ioServiceThreadPtr;

    const uint16_t M_KEEP_ALIVE_INTERVAL_SECONDS;
    uint64_t m_rateBitsPerSec;
    const unsigned int MAX_UNACKED;
    CircularIndexBufferSingleProducerSingleConsumerConfigurable m_bytesToAckByRateCb;
    std::vector<uint32_t> m_bytesToAckByRateCbVec;
    std::vector<uint32_t> m_groupingOfBytesToAckByRateVec;
    CircularIndexBufferSingleProducerSingleConsumerConfigurable m_bytesToAckByTcpSendCallbackCb;
    std::vector<uint32_t> m_bytesToAckByTcpSendCallbackCbVec;
    OnSuccessfulAckCallback_t m_onSuccessfulAckCallback;
    volatile bool m_readyToForward;
    volatile bool m_dataServedAsKeepAlive;
    volatile bool m_rateTimerIsRunning;

    uint8_t m_tcpReadSomeBuffer[10];

public:
    //stcp stats
    std::size_t m_totalDataSegmentsAckedByTcpSendCallback;
    std::size_t m_totalBytesAckedByTcpSendCallback;
    std::size_t m_totalDataSegmentsAckedByRate;
    std::size_t m_totalBytesAckedByRate;
    std::size_t m_totalDataSegmentsSent;
    std::size_t m_totalBundleBytesSent;
    std::size_t m_totalStcpBytesSent;
};



#endif //_STCP_BUNDLE_SOURCE_H
