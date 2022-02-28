#ifndef _TCPCL_BUNDLE_SINK_H
#define _TCPCL_BUNDLE_SINK_H 1

#include "TcpclV3BidirectionalLink.h"

class TcpclBundleSink : public TcpclV3BidirectionalLink {
private:
    TcpclBundleSink();
public:
    typedef boost::function<void(padded_vector_uint8_t & wholeBundleVec)> WholeBundleReadyCallback_t;
    typedef boost::function<void()> NotifyReadyToDeleteCallback_t;
    typedef boost::function<bool(std::pair<std::unique_ptr<zmq::message_t>, std::vector<uint8_t> > & bundleDataPair)> TryGetOpportunisticDataFunction_t;
    typedef boost::function<void()> NotifyOpportunisticDataAckedCallback_t;
    typedef boost::function<void(TcpclBundleSink * thisTcpclBundleSinkPtr)> OnContactHeaderCallback_t;

    TCPCL_LIB_EXPORT TcpclBundleSink(
        const uint16_t desiredKeepAliveIntervalSeconds,
        boost::shared_ptr<boost::asio::ip::tcp::socket> & tcpSocketPtr,
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
    TCPCL_LIB_EXPORT ~TcpclBundleSink();
    TCPCL_LIB_EXPORT bool ReadyToBeDeleted();
    TCPCL_LIB_EXPORT uint64_t GetRemoteNodeId() const;
    TCPCL_LIB_EXPORT void TrySendOpportunisticBundleIfAvailable_FromIoServiceThread();
    TCPCL_LIB_EXPORT void SetTryGetOpportunisticDataFunction(const TryGetOpportunisticDataFunction_t & tryGetOpportunisticDataFunction);
    TCPCL_LIB_EXPORT void SetNotifyOpportunisticDataAckedCallback(const NotifyOpportunisticDataAckedCallback_t & notifyOpportunisticDataAckedCallback);
private:

    TCPCL_LIB_EXPORT void TryStartTcpReceive();
    TCPCL_LIB_EXPORT void HandleTcpReceiveSome(const boost::system::error_code & error, std::size_t bytesTransferred, unsigned int writeIndex);
    TCPCL_LIB_EXPORT void PopCbThreadFunc();
    
    TCPCL_LIB_EXPORT virtual void Virtual_OnTcpclShutdownComplete_CalledFromIoServiceThread();
    TCPCL_LIB_EXPORT virtual void Virtual_OnSuccessfulWholeBundleAcknowledged();
    TCPCL_LIB_EXPORT virtual void Virtual_WholeBundleReady(padded_vector_uint8_t & wholeBundleVec);
    TCPCL_LIB_EXPORT virtual void Virtual_OnTcpSendSuccessful_CalledFromIoServiceThread();
    TCPCL_LIB_EXPORT virtual void Virtual_OnContactHeaderCompletedSuccessfully();

    

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



#endif  //_TCPCL_BUNDLE_SINK_H
