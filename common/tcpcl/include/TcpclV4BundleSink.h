#ifndef _TCPCLV4_BUNDLE_SINK_H
#define _TCPCLV4_BUNDLE_SINK_H 1

#include "TcpclV4BidirectionalLink.h"

class TcpclV4BundleSink : public TcpclV4BidirectionalLink {
private:
    TcpclV4BundleSink();
public:
    typedef boost::function<void(std::vector<uint8_t> & wholeBundleVec)> WholeBundleReadyCallback_t;
    typedef boost::function<void()> NotifyReadyToDeleteCallback_t;
    typedef boost::function<bool(std::pair<std::unique_ptr<zmq::message_t>, std::vector<uint8_t> > & bundleDataPair)> TryGetOpportunisticDataFunction_t;
    typedef boost::function<void()> NotifyOpportunisticDataAckedCallback_t;
    typedef boost::function<void(TcpclV4BundleSink * thisTcpclBundleSinkPtr)> OnContactHeaderCallback_t;

    TcpclV4BundleSink(
#ifdef OPENSSL_SUPPORT_ENABLED
        boost::shared_ptr<boost::asio::ssl::stream<boost::asio::ip::tcp::socket> > & sslStreamSharedPtr,
#else
        boost::shared_ptr<boost::asio::ip::tcp::socket> & tcpSocketPtr,
#endif
        const uint16_t desiredKeepAliveIntervalSeconds,
        boost::asio::io_service & tcpSocketIoServiceRef,
        const WholeBundleReadyCallback_t & wholeBundleReadyCallback,
        const unsigned int numCircularBufferVectors,
        const unsigned int circularBufferBytesPerVector,
        const uint64_t myNodeId,
        const uint64_t maxBundleSizeBytes,
        const NotifyReadyToDeleteCallback_t & notifyReadyToDeleteCallback = NotifyReadyToDeleteCallback_t(),
        const OnContactHeaderCallback_t & onContactHeaderCallback = OnContactHeaderCallback_t(),
        //const TryGetOpportunisticDataFunction_t & tryGetOpportunisticDataFunction = TryGetOpportunisticDataFunction_t(),
        //const NotifyOpportunisticDataAckedCallback_t & notifyOpportunisticDataAckedCallback = NotifyOpportunisticDataAckedCallback_t(),
        const unsigned int maxUnacked = 10, const uint64_t maxFragmentSize = 100000000 ); //todo
    ~TcpclV4BundleSink();
    bool ReadyToBeDeleted();
    uint64_t GetRemoteNodeId() const;
    void TrySendOpportunisticBundleIfAvailable_FromIoServiceThread();
    void SetTryGetOpportunisticDataFunction(const TryGetOpportunisticDataFunction_t & tryGetOpportunisticDataFunction);
    void SetNotifyOpportunisticDataAckedCallback(const NotifyOpportunisticDataAckedCallback_t & notifyOpportunisticDataAckedCallback);
private:

    
#ifdef OPENSSL_SUPPORT_ENABLED
    void DoSslUpgrade();
    void HandleSslHandshake(const boost::system::error_code & error);
    void TryStartTcpReceiveSecure();
    void HandleTcpReceiveSomeSecure(const boost::system::error_code & error, std::size_t bytesTransferred, unsigned int writeIndex);
#endif
    void TryStartTcpReceiveUnsecure();
    void HandleTcpReceiveSomeUnsecure(const boost::system::error_code & error, std::size_t bytesTransferred, unsigned int writeIndex);
    void HandleTcpSend(const boost::system::error_code& error, std::size_t bytes_transferred);
    void HandleTcpSendShutdown(const boost::system::error_code& error, std::size_t bytes_transferred);
    void OnSendShutdownMessageTimeout_TimerExpired(const boost::system::error_code& e);
    void PopCbThreadFunc();
    
    virtual void Virtual_OnTcpclShutdownComplete_CalledFromIoServiceThread();
    virtual void Virtual_OnSuccessfulWholeBundleAcknowledged();
    virtual void Virtual_WholeBundleReady(std::vector<uint8_t> & wholeBundleVec);
    virtual void Virtual_OnTcpSendSuccessful_CalledFromIoServiceThread();
    virtual void Virtual_OnTcpSendContactHeaderSuccessful_CalledFromIoServiceThread();
    virtual void Virtual_OnSessionInitReceivedAndProcessedSuccessfully();

    

    const WholeBundleReadyCallback_t m_wholeBundleReadyCallback;
    const NotifyReadyToDeleteCallback_t m_notifyReadyToDeleteCallback;
    TryGetOpportunisticDataFunction_t m_tryGetOpportunisticDataFunction;
    NotifyOpportunisticDataAckedCallback_t m_notifyOpportunisticDataAckedCallback;
    const OnContactHeaderCallback_t m_onContactHeaderCallback;

    
    boost::asio::io_service & m_tcpSocketIoServiceRef;
    

    const unsigned int M_NUM_CIRCULAR_BUFFER_VECTORS;
    const unsigned int M_CIRCULAR_BUFFER_BYTES_PER_VECTOR;
    CircularIndexBufferSingleProducerSingleConsumerConfigurable m_circularIndexBuffer;
    std::vector<std::vector<boost::uint8_t> > m_tcpReceiveBuffersCbVec;
    std::vector<std::size_t> m_tcpReceiveBytesTransferredCbVec;
    boost::condition_variable m_conditionVariableCb;
    std::unique_ptr<boost::thread> m_threadCbReaderPtr;
    bool m_stateTcpReadActive;
    bool m_printedCbTooSmallNotice;
    volatile bool m_running;
    

    
};



#endif  //_TCPCLV4_BUNDLE_SINK_H
