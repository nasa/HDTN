#ifndef _UDP_BUNDLE_SOURCE_H
#define _UDP_BUNDLE_SOURCE_H 1

#include <string>
#include <boost/thread.hpp>
#include <boost/asio.hpp>
#include <map>
#include <queue>
#include "CircularIndexBufferSingleProducerSingleConsumerConfigurable.h"
#include <zmq.hpp>

class UdpBundleSource {
private:
    UdpBundleSource();
public:
    typedef boost::function<void()> OnSuccessfulAckCallback_t;
    UdpBundleSource(const uint64_t rateBps = 50, const unsigned int maxUnacked = 100);

    ~UdpBundleSource();
    void Stop();
    bool Forward(const uint8_t* bundleData, const std::size_t size);
    bool Forward(zmq::message_t & dataZmq);
    bool Forward(std::vector<uint8_t> & dataVec);
    std::size_t GetTotalUdpPacketsAcked();
    std::size_t GetTotalUdpPacketsSent();
    std::size_t GetTotalUdpPacketsUnacked();
    std::size_t GetTotalBundleBytesAcked();
    std::size_t GetTotalBundleBytesSent();
    std::size_t GetTotalBundleBytesUnacked();
    void UpdateRate(uint64_t rateBitsPerSec);
    void Connect(const std::string & hostname, const std::string & port);
    bool ReadyToForward();
    void SetOnSuccessfulAckCallback(const OnSuccessfulAckCallback_t & callback);
private:
    void OnResolve(const boost::system::error_code & ec, boost::asio::ip::udp::resolver::results_type results);
    void OnConnect(const boost::system::error_code & ec);
    void HandleUdpSend(boost::shared_ptr<std::vector<boost::uint8_t> > dataSentPtr, const boost::system::error_code& error, std::size_t bytes_transferred);
    void HandleUdpSendZmqMessage(boost::shared_ptr<zmq::message_t> dataZmqSentPtr, const boost::system::error_code& error, std::size_t bytes_transferred);

    void RestartNewDataSignaler();
    void SignalNewDataForwarded();
    void OnNewData_TimerCancelled(const boost::system::error_code& e);
    void TryRestartRateTimer();
    void OnRate_TimerExpired(const boost::system::error_code& e);
    void DoUdpShutdown();

    



    
    boost::asio::io_service m_ioService;
    boost::asio::io_service::work m_work;
    boost::asio::ip::udp::resolver m_resolver;
    boost::asio::deadline_timer m_rateTimer;
    boost::asio::deadline_timer m_newDataSignalerTimer;
    boost::asio::ip::udp::socket m_udpSocket;
    boost::asio::ip::udp::endpoint m_udpDestinationEndpoint;
    std::unique_ptr<boost::thread> m_ioServiceThreadPtr;
    boost::condition_variable m_localConditionVariableAckReceived;

    uint64_t m_rateBitsPerSec;
    const unsigned int MAX_UNACKED;
    CircularIndexBufferSingleProducerSingleConsumerConfigurable m_bytesToAckByRateCb;
    std::vector<uint32_t> m_bytesToAckByRateCbVec;
    std::vector<uint32_t> m_groupingOfBytesToAckByRateVec;
    CircularIndexBufferSingleProducerSingleConsumerConfigurable m_bytesToAckByUdpSendCallbackCb;
    std::vector<uint32_t> m_bytesToAckByUdpSendCallbackCbVec;
    OnSuccessfulAckCallback_t m_onSuccessfulAckCallback;
    volatile bool m_readyToForward;
    volatile bool m_rateTimerIsRunning;
    volatile bool m_newDataSignalerTimerIsRunning;
    volatile bool m_useLocalConditionVariableAckReceived;


public:
    //udp stats
    std::size_t m_totalUdpPacketsAckedByUdpSendCallback;
    std::size_t m_totalBytesAckedByUdpSendCallback;
    std::size_t m_totalUdpPacketsAckedByRate;
    std::size_t m_totalBytesAckedByRate;
    std::size_t m_totalUdpPacketsSent;
    std::size_t m_totalBundleBytesSent;
};



#endif //_UDP_BUNDLE_SOURCE_H
