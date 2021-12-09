#ifndef _TCPCL_BUNDLE_SOURCE_H
#define _TCPCL_BUNDLE_SOURCE_H 1

#include "TcpclV3BidirectionalLink.h"

typedef boost::function<void(std::vector<uint8_t> & movableBundle)> OutductOpportunisticProcessReceivedBundleCallback_t;

//tcpcl
class TcpclBundleSource : public TcpclV3BidirectionalLink {
private:
    TcpclBundleSource();
public:
    typedef boost::function<void()> OnSuccessfulAckCallback_t;
    TcpclBundleSource(const uint16_t desiredKeepAliveIntervalSeconds, const uint64_t myNodeId,
        const std::string & expectedRemoteEidUri, const unsigned int maxUnacked, const uint64_t maxFragmentSize,
        const OutductOpportunisticProcessReceivedBundleCallback_t & outductOpportunisticProcessReceivedBundleCallback = OutductOpportunisticProcessReceivedBundleCallback_t());

    ~TcpclBundleSource();
    void Stop();
    
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
    void OnReconnectAfterOnConnectError_TimerExpired(const boost::system::error_code& e);
    void HandleTcpSend(const boost::system::error_code& error, std::size_t bytes_transferred);
    void HandleTcpSendShutdown(const boost::system::error_code& error, std::size_t bytes_transferred);
    void StartTcpReceive();
    void HandleTcpReceiveSome(const boost::system::error_code & error, std::size_t bytesTransferred);
    void OnNeedToReconnectAfterShutdown_TimerExpired(const boost::system::error_code& e);
    void OnSendShutdownMessageTimeout_TimerExpired(const boost::system::error_code& e);

    virtual void Virtual_OnTcpclShutdownComplete_CalledFromIoServiceThread();
    virtual void Virtual_OnSuccessfulWholeBundleAcknowledged();
    virtual void Virtual_WholeBundleReady(std::vector<uint8_t> & wholeBundleVec);

    
    
    
    boost::asio::io_service::work m_work;
    boost::asio::ip::tcp::resolver m_resolver;
    boost::asio::deadline_timer m_reconnectAfterShutdownTimer;
    boost::asio::deadline_timer m_reconnectAfterOnConnectErrorTimer;
    
    boost::asio::ip::tcp::resolver::results_type m_resolverResults;
    std::unique_ptr<boost::thread> m_ioServiceThreadPtr;
    

    //tcpcl vars

    
    
    
    
    
    
    
    
    
    
    OnSuccessfulAckCallback_t m_onSuccessfulAckCallback;

    //opportunistic receive bundles
    const OutductOpportunisticProcessReceivedBundleCallback_t m_outductOpportunisticProcessReceivedBundleCallback;


    std::vector<uint8_t> m_tcpReadSomeBufferVec;

};



#endif //_TCPCL_BUNDLE_SOURCE_H
