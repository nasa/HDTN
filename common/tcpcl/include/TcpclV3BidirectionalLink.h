#ifndef _TCPCLV3_BIDIRECTIONAL_LINK_H
#define _TCPCLV3_BIDIRECTIONAL_LINK_H 1

#include <string>
#include <boost/thread.hpp>
#include <boost/asio.hpp>
#include <map>
#include <vector>
#include "Tcpcl.h"
#include "TcpAsyncSender.h"
#include "CircularIndexBufferSingleProducerSingleConsumerConfigurable.h"

class TcpclV3BidirectionalLink {
public:
    TcpclV3BidirectionalLink(
        const std::string & implementationStringForCout,
        const uint64_t shutdownMessageReconnectionDelaySecondsToSend,
        const bool deleteSocketAfterShutdown,
        const bool contactHeaderMustReply,
        const uint16_t desiredKeepAliveIntervalSeconds,
        boost::asio::io_service * externalIoServicePtr,
        const unsigned int maxUnacked,
        const uint64_t maxBundleSizeBytes,
        const uint64_t maxFragmentSize,
        const uint64_t myNodeId,
        const std::string & expectedRemoteEidUriStringIfNotEmpty
    );
    bool BaseClass_Forward(const uint8_t* bundleData, const std::size_t size);
    bool BaseClass_Forward(std::vector<uint8_t> & dataVec);
    bool BaseClass_Forward(zmq::message_t & dataZmq);
    bool BaseClass_Forward(std::unique_ptr<zmq::message_t> & zmqMessageUniquePtr, std::vector<uint8_t> & vecMessage, const bool usingZmqData);
    

protected:
    const std::string M_BASE_IMPLEMENTATION_STRING_FOR_COUT;
    const uint64_t M_BASE_SHUTDOWN_MESSAGE_RECONNECTION_DELAY_SECONDS_TO_SEND;
    const uint16_t M_BASE_DESIRED_KEEPALIVE_INTERVAL_SECONDS;
    const bool M_BASE_DELETE_SOCKET_AFTER_SHUTDOWN;
    const bool M_BASE_CONTACT_HEADER_MUST_REPLY;
    const std::string M_BASE_THIS_TCPCL_EID_STRING;
    std::string M_BASE_EXPECTED_REMOTE_CONTACT_HEADER_EID_STRING_IF_NOT_EMPTY;
    uint16_t m_base_keepAliveIntervalSeconds;
    std::unique_ptr<boost::asio::io_service> m_base_localIoServiceUniquePtr; //if an external one is not provided, create it here and set the ioServiceRef below to it
    boost::asio::io_service & m_base_ioServiceRef;
    boost::asio::deadline_timer m_base_noKeepAlivePacketReceivedTimer;
    boost::asio::deadline_timer m_base_needToSendKeepAliveMessageTimer;
    boost::asio::deadline_timer m_base_sendShutdownMessageTimeoutTimer;
    bool m_base_shutdownCalled;
    volatile bool m_base_readyToForward; //bundleSource
    volatile bool m_base_sinkIsSafeToDelete; //bundleSink
    volatile bool m_base_tcpclShutdownComplete; //bundleSource
    volatile bool m_base_useLocalConditionVariableAckReceived; //bundleSource
    boost::condition_variable m_base_localConditionVariableAckReceived;
    uint64_t m_base_reconnectionDelaySecondsIfNotZero;

    Tcpcl m_base_tcpclV3RxStateMachine;
    CONTACT_HEADER_FLAGS m_base_contactHeaderFlags;
    std::string m_base_tcpclRemoteEidString;
    uint64_t m_base_tcpclRemoteNodeId;
    boost::shared_ptr<boost::asio::ip::tcp::socket> m_base_tcpSocketPtr;
    std::unique_ptr<TcpAsyncSender> m_base_tcpAsyncSenderPtr;
    TcpAsyncSenderElement::OnSuccessfulSendCallbackByIoServiceThread_t m_base_handleTcpSendCallback;
    TcpAsyncSenderElement::OnSuccessfulSendCallbackByIoServiceThread_t m_base_handleTcpSendShutdownCallback;
    std::vector<uint8_t> m_base_fragmentedBundleRxConcat;

    const unsigned int M_BASE_MAX_UNACKED;
    CircularIndexBufferSingleProducerSingleConsumerConfigurable m_base_bytesToAckCb;
    std::vector<uint64_t> m_base_bytesToAckCbVec;
    std::vector<std::vector<uint64_t> > m_base_fragmentBytesToAckCbVec;
    std::vector<uint64_t> m_base_fragmentVectorIndexCbVec;
    const uint64_t M_BASE_MAX_FRAGMENT_SIZE;

    

protected:
    
    void BaseClass_DoTcpclShutdown(bool sendShutdownMessage, bool reasonWasTimeOut);
    virtual void Virtual_OnTcpclShutdownComplete_CalledFromIoServiceThread() = 0;
    virtual void Virtual_OnSuccessfulWholeBundleAcknowledged() = 0;
    virtual void Virtual_WholeBundleReady(std::vector<uint8_t> & wholeBundleVec) = 0;
    virtual void Virtual_OnTcpSendSuccessful_CalledFromIoServiceThread();
    virtual void Virtual_OnContactHeaderCompletedSuccessfully();

private:
    void BaseClass_ContactHeaderCallback(CONTACT_HEADER_FLAGS flags, uint16_t keepAliveIntervalSeconds, const std::string & localEid);
    void BaseClass_DataSegmentCallback(std::vector<uint8_t> & dataSegmentDataVec, bool isStartFlag, bool isEndFlag);
    void BaseClass_AckCallback(uint64_t totalBytesAcknowledged);
    void BaseClass_KeepAliveCallback();
    void BaseClass_ShutdownCallback(bool hasReasonCode, SHUTDOWN_REASON_CODES shutdownReasonCode,
        bool hasReconnectionDelay, uint64_t reconnectionDelaySeconds);
    void BaseClass_BundleRefusalCallback(BUNDLE_REFUSAL_CODES refusalCode);
    void BaseClass_NextBundleLengthCallback(uint64_t nextBundleLength);

    void BaseClass_HandleTcpSend(const boost::system::error_code& error, std::size_t bytes_transferred);
    void BaseClass_HandleTcpSendShutdown(const boost::system::error_code& error, std::size_t bytes_transferred);
    void BaseClass_OnNoKeepAlivePacketReceived_TimerExpired(const boost::system::error_code& e);
    void BaseClass_OnNeedToSendKeepAliveMessage_TimerExpired(const boost::system::error_code& e);    
    void BaseClass_DoHandleSocketShutdown(bool sendShutdownMessage, bool reasonWasTimeOut);
    void BaseClass_OnSendShutdownMessageTimeout_TimerExpired(const boost::system::error_code& e);

public:
    //tcpcl stats
    std::size_t m_base_totalBundlesAcked;
    std::size_t m_base_totalBytesAcked;
    std::size_t m_base_totalBundlesSent;
    std::size_t m_base_totalFragmentedAcked;
    std::size_t m_base_totalFragmentedSent;
    std::size_t m_base_totalBundleBytesSent;
    
};



#endif  //_TCPCLV3_BIDIRECTIONAL_LINK_H
