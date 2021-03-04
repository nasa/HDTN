#ifndef _STCP_BUNDLE_SOURCE_H
#define _STCP_BUNDLE_SOURCE_H 1

#include <string>
#include <boost/thread.hpp>
#include <boost/asio.hpp>
#include <map>
#include <queue>

//tcpcl
class StcpBundleSource {
private:
    StcpBundleSource();
public:
    StcpBundleSource(const uint16_t desiredKeeAliveIntervalSeconds);

    ~StcpBundleSource();
    bool Forward(const uint8_t* bundleData, const std::size_t size, unsigned int & numUnackedBundles);
    void Connect(const std::string & hostname, const std::string & port);
    bool ReadyToForward();
private:
    static void GenerateDataUnit(std::vector<uint8_t> & dataUnit, const uint8_t * contents, uint32_t sizeContents);
    void OnResolve(const boost::system::error_code & ec, boost::asio::ip::tcp::resolver::results_type results);
    void OnConnect(const boost::system::error_code & ec);
    void HandleTcpSend(boost::shared_ptr<std::vector<boost::uint8_t> > dataSentPtr, const boost::system::error_code& error, std::size_t bytes_transferred);
    void HandleTcpSendKeepAlive(const boost::system::error_code& error, std::size_t bytes_transferred);
    void StartTcpReceive();
    void HandleTcpReceiveSome(const boost::system::error_code & error, std::size_t bytesTransferred);
    void OnNeedToSendKeepAliveMessage_TimerExpired(const boost::system::error_code& e);
    void DoStcpShutdown();

    



    
    boost::asio::io_service m_ioService;
    boost::asio::io_service::work m_work;
    boost::asio::ip::tcp::resolver m_resolver;
    boost::asio::deadline_timer m_needToSendKeepAliveMessageTimer;
    boost::shared_ptr<boost::asio::ip::tcp::socket> m_tcpSocketPtr;
    boost::shared_ptr<boost::thread> m_ioServiceThreadPtr;

    const uint16_t M_KEEP_ALIVE_INTERVAL_SECONDS;
    volatile bool m_readyToForward;
    volatile bool m_dataServedAsKeepAlive;

    uint8_t m_tcpReadSomeBuffer[10];

public:
    //stcp stats
    std::size_t m_totalDataSegmentsSent;
    std::size_t m_totalBundleBytesSent;
};



#endif //_STCP_BUNDLE_SOURCE_H
