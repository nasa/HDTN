#ifndef _TCPCL_BUNDLE_SINK_H
#define _TCPCL_BUNDLE_SINK_H 1

#include <stdint.h>
#include <boost/asio.hpp>
#include <boost/thread.hpp>
#include <boost/function.hpp>
#include <boost/shared_ptr.hpp>
#include "CircularIndexBufferSingleProducerSingleConsumerConfigurable.h"
#include "Tcpcl.h"
#include "TcpAsyncSender.h"

class TcpclBundleSink {
private:
    TcpclBundleSink();
public:
    typedef boost::function<void(std::vector<uint8_t> & wholeBundleVec)> WholeBundleReadyCallback_t;
    typedef boost::function<void()> NotifyReadyToDeleteCallback_t;
    typedef boost::function<bool(std::pair<std::unique_ptr<zmq::message_t>, std::vector<uint8_t> > & bundleDataPair)> TryGetOpportunisticDataFunction_t;
    typedef boost::function<void()> NotifyOpportunisticDataAckedCallback_t;
    typedef boost::function<void(TcpclBundleSink * thisTcpclBundleSinkPtr)> OnContactHeaderCallback_t;

    TcpclBundleSink(
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
    ~TcpclBundleSink();
    bool ReadyToBeDeleted();
    uint64_t GetRemoteNodeId() const;
    void TrySendOpportunisticBundleIfAvailable_FromIoServiceThread();
    void SetTryGetOpportunisticDataFunction(const TryGetOpportunisticDataFunction_t & tryGetOpportunisticDataFunction);
    void SetNotifyOpportunisticDataAckedCallback(const NotifyOpportunisticDataAckedCallback_t & notifyOpportunisticDataAckedCallback);
private:

    void TryStartTcpReceive();
    void HandleTcpReceiveSome(const boost::system::error_code & error, std::size_t bytesTransferred, unsigned int writeIndex);
    void HandleTcpSend(const boost::system::error_code& error, std::size_t bytes_transferred);
    void HandleTcpSendShutdown(const boost::system::error_code& error, std::size_t bytes_transferred);
    void OnNoKeepAlivePacketReceived_TimerExpired(const boost::system::error_code& e);
    void OnNeedToSendKeepAliveMessage_TimerExpired(const boost::system::error_code& e);
    void HandleSocketShutdown(bool sendShutdownMessage, bool reasonWasTimeOut);
    void OnSendShutdownMessageTimeout_TimerExpired(const boost::system::error_code& e);
    void PopCbThreadFunc();
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

    //tcpcl vars
    Tcpcl m_tcpcl;
    CONTACT_HEADER_FLAGS m_contactHeaderFlags;
    uint16_t m_keepAliveIntervalSeconds;
    std::string m_remoteEid;
    uint64_t m_remoteNodeId;
    const std::string M_THIS_EID;
    std::vector<uint8_t> m_fragmentedBundleRxConcat;

    const WholeBundleReadyCallback_t m_wholeBundleReadyCallback;
    const NotifyReadyToDeleteCallback_t m_notifyReadyToDeleteCallback;
    TryGetOpportunisticDataFunction_t m_tryGetOpportunisticDataFunction;
    NotifyOpportunisticDataAckedCallback_t m_notifyOpportunisticDataAckedCallback;
    const OnContactHeaderCallback_t m_onContactHeaderCallback;

    boost::shared_ptr<boost::asio::ip::tcp::socket> m_tcpSocketPtr;
    boost::asio::io_service & m_tcpSocketIoServiceRef;
    boost::asio::deadline_timer m_noKeepAlivePacketReceivedTimer;
    boost::asio::deadline_timer m_needToSendKeepAliveMessageTimer;
    boost::asio::deadline_timer m_sendShutdownMessageTimeoutTimer;

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
    volatile bool m_safeToDelete;
    volatile bool m_shutdownCalled;

    const unsigned int MAX_UNACKED;
    CircularIndexBufferSingleProducerSingleConsumerConfigurable m_bytesToAckCb;
    std::vector<uint64_t> m_bytesToAckCbVec;
    std::vector<std::vector<uint64_t> > m_fragmentBytesToAckCbVec;
    std::vector<uint64_t> m_fragmentVectorIndexCbVec;
    const uint64_t M_MAX_FRAGMENT_SIZE;

    //tcpcl stats
    std::size_t m_totalDataSegmentsAcked;
    std::size_t m_totalBytesAcked;
    std::size_t m_totalDataSegmentsSent;
    std::size_t m_totalFragmentedAcked;
    std::size_t m_totalFragmentedSent;
    std::size_t m_totalBundleBytesSent;
};



#endif  //_TCPCL_BUNDLE_SINK_H
