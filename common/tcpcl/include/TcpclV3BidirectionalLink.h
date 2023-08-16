/**
 * @file TcpclV3BidirectionalLink.h
 * @author  Brian Tomko <brian.j.tomko@nasa.gov>
 *
 * @copyright Copyright © 2021 United States Government as represented by
 * the National Aeronautics and Space Administration.
 * No copyright is claimed in the United States under Title 17, U.S.Code.
 * All Other Rights Reserved.
 *
 * @section LICENSE
 * Released under the NASA Open Source Agreement (NOSA)
 * See LICENSE.md in the source root directory for more information.
 *
 * @section DESCRIPTION
 *
 * This TcpclV3BidirectionalLink virtual base class defines common functionality
 * for version 3 of the TCP Convergence-Layer Protocol for the bidirectional
 * nature of the TCPCL protocol, such that any "bundle source" must be prepared
 * to receive bundles, and any "bundle sink" must be prepared to send bundles,
 * and in both cases they must share the same underlying TCP socket/connection.
 */

#ifndef _TCPCLV3_BIDIRECTIONAL_LINK_H
#define _TCPCLV3_BIDIRECTIONAL_LINK_H 1

#include <string>
#include <boost/thread.hpp>
#include <boost/asio.hpp>
#include <map>
#include <vector>
#include <memory>
#include "Tcpcl.h"
#include "TcpAsyncSender.h"
#include "TelemetryDefinitions.h"
#include "CircularIndexBufferSingleProducerSingleConsumerConfigurable.h"
#include "BidirectionalLink.h"
#include "BundleCallbackFunctionDefines.h"
#include <atomic>

class CLASS_VISIBILITY_TCPCL_LIB TcpclV3BidirectionalLink : public BidirectionalLink {
public:
    TCPCL_LIB_EXPORT TcpclV3BidirectionalLink(
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
    TCPCL_LIB_EXPORT virtual ~TcpclV3BidirectionalLink();

    TCPCL_LIB_EXPORT bool BaseClass_Forward(const uint8_t* bundleData, const std::size_t size, std::vector<uint8_t>&& userData);
    TCPCL_LIB_EXPORT bool BaseClass_Forward(padded_vector_uint8_t& dataVec, std::vector<uint8_t>&& userData);
    TCPCL_LIB_EXPORT bool BaseClass_Forward(zmq::message_t & dataZmq, std::vector<uint8_t>&& userData);
    TCPCL_LIB_EXPORT bool BaseClass_Forward(std::unique_ptr<zmq::message_t> & zmqMessageUniquePtr, padded_vector_uint8_t& vecMessage, const bool usingZmqData, std::vector<uint8_t>&& userData);

    TCPCL_LIB_EXPORT virtual unsigned int Virtual_GetMaxTxBundlesInPipeline() override;

    TCPCL_LIB_EXPORT void BaseClass_SetOnFailedBundleVecSendCallback(const OnFailedBundleVecSendCallback_t& callback);
    TCPCL_LIB_EXPORT void BaseClass_SetOnFailedBundleZmqSendCallback(const OnFailedBundleZmqSendCallback_t& callback);
    TCPCL_LIB_EXPORT void BaseClass_SetOnSuccessfulBundleSendCallback(const OnSuccessfulBundleSendCallback_t& callback);
    TCPCL_LIB_EXPORT void BaseClass_SetOnOutductLinkStatusChangedCallback(const OnOutductLinkStatusChangedCallback_t& callback);
    TCPCL_LIB_EXPORT void BaseClass_SetUserAssignedUuid(uint64_t userAssignedUuid);
    TCPCL_LIB_EXPORT void BaseClass_GetTelemetry(TcpclV3InductConnectionTelemetry_t& telem) const;
    TCPCL_LIB_EXPORT void BaseClass_GetTelemetry(TcpclV3OutductTelemetry_t& telem) const;

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
    std::atomic<bool> m_base_readyToForward; //bundleSource
    std::atomic<bool> m_base_sinkIsSafeToDelete; //bundleSink
    std::atomic<bool> m_base_tcpclShutdownComplete; //bundleSource
    std::atomic<bool> m_base_useLocalConditionVariableAckReceived; //bundleSource
    std::atomic<bool> m_base_dataReceivedServedAsKeepaliveReceived;
    std::atomic<bool> m_base_dataSentServedAsKeepaliveSent;
    boost::condition_variable m_base_localConditionVariableAckReceived;
    uint64_t m_base_reconnectionDelaySecondsIfNotZero;

    Tcpcl m_base_tcpclV3RxStateMachine;
    CONTACT_HEADER_FLAGS m_base_contactHeaderFlags;
    std::string m_base_tcpclRemoteEidString;
    uint64_t m_base_tcpclRemoteNodeId;
    std::shared_ptr<boost::asio::ip::tcp::socket> m_base_tcpSocketPtr;
    std::unique_ptr<TcpAsyncSender> m_base_tcpAsyncSenderPtr;
    TcpAsyncSenderElement::OnSuccessfulSendCallbackByIoServiceThread_t m_base_handleTcpSendCallback;
    TcpAsyncSenderElement::OnSuccessfulSendCallbackByIoServiceThread_t m_base_handleTcpSendShutdownCallback;
    padded_vector_uint8_t m_base_fragmentedBundleRxConcat;

    const unsigned int M_BASE_MAX_UNACKED_BUNDLES_IN_PIPELINE;
    const unsigned int M_BASE_UNACKED_BUNDLE_CB_SIZE;
    CircularIndexBufferSingleProducerSingleConsumerConfigurable m_base_bytesToAckCb;
    std::vector<uint64_t> m_base_bytesToAckCbVec;
    std::vector<std::vector<uint64_t> > m_base_fragmentBytesToAckCbVec;
    std::vector<uint64_t> m_base_fragmentVectorIndexCbVec;
    std::vector<std::vector<uint8_t> > m_base_userDataCbVec;
    const uint64_t M_BASE_MAX_FRAGMENT_SIZE;

    OnFailedBundleVecSendCallback_t m_base_onFailedBundleVecSendCallback;
    OnFailedBundleZmqSendCallback_t m_base_onFailedBundleZmqSendCallback;
    OnSuccessfulBundleSendCallback_t m_base_onSuccessfulBundleSendCallback;
    OnOutductLinkStatusChangedCallback_t m_base_onOutductLinkStatusChangedCallback;
    uint64_t m_base_userAssignedUuid;

    std::string m_base_inductConnectionName;
    std::string m_base_inductInputName;

protected:
    
    TCPCL_LIB_EXPORT void BaseClass_TryToWaitForAllBundlesToFinishSending();
    TCPCL_LIB_EXPORT void BaseClass_DoTcpclShutdown(bool sendShutdownMessage, bool reasonWasTimeOut);
    virtual void Virtual_OnTcpclShutdownComplete_CalledFromIoServiceThread() = 0;
    virtual void Virtual_OnSuccessfulWholeBundleAcknowledged() = 0;
    virtual void Virtual_WholeBundleReady(padded_vector_uint8_t & wholeBundleVec) = 0;
    TCPCL_LIB_EXPORT virtual void Virtual_OnTcpSendSuccessful_CalledFromIoServiceThread();
    TCPCL_LIB_EXPORT virtual void Virtual_OnContactHeaderCompletedSuccessfully();

private:
    TCPCL_LIB_NO_EXPORT void BaseClass_ContactHeaderCallback(CONTACT_HEADER_FLAGS flags, uint16_t keepAliveIntervalSeconds, const std::string & localEid);
    TCPCL_LIB_NO_EXPORT void BaseClass_DataSegmentCallback(padded_vector_uint8_t & dataSegmentDataVec, bool isStartFlag, bool isEndFlag);
    TCPCL_LIB_NO_EXPORT void BaseClass_AckCallback(uint64_t totalBytesAcknowledged);
    TCPCL_LIB_NO_EXPORT void BaseClass_RestartNoKeepaliveReceivedTimer();
    TCPCL_LIB_NO_EXPORT void BaseClass_RestartNeedToSendKeepAliveMessageTimer();
    TCPCL_LIB_NO_EXPORT void BaseClass_KeepAliveCallback();
    TCPCL_LIB_NO_EXPORT void BaseClass_ShutdownCallback(bool hasReasonCode, SHUTDOWN_REASON_CODES shutdownReasonCode,
        bool hasReconnectionDelay, uint64_t reconnectionDelaySeconds);
    TCPCL_LIB_NO_EXPORT void BaseClass_BundleRefusalCallback(BUNDLE_REFUSAL_CODES refusalCode);
    TCPCL_LIB_NO_EXPORT void BaseClass_NextBundleLengthCallback(uint64_t nextBundleLength);

    TCPCL_LIB_NO_EXPORT void BaseClass_HandleTcpSend(const boost::system::error_code& error, std::size_t bytes_transferred, TcpAsyncSenderElement* elPtr);
    TCPCL_LIB_NO_EXPORT void BaseClass_HandleTcpSendShutdown(const boost::system::error_code& error, std::size_t bytes_transferred, TcpAsyncSenderElement* elPtr);
    TCPCL_LIB_NO_EXPORT void BaseClass_OnNoKeepAlivePacketReceived_TimerExpired(const boost::system::error_code& e);
    TCPCL_LIB_NO_EXPORT void BaseClass_OnNeedToSendKeepAliveMessage_TimerExpired(const boost::system::error_code& e);
    TCPCL_LIB_NO_EXPORT void BaseClass_DoHandleSocketShutdown(bool sendShutdownMessage, bool reasonWasTimeOut);
    TCPCL_LIB_NO_EXPORT void BaseClass_OnSendShutdownMessageTimeout_TimerExpired(const boost::system::error_code& e);

    
};



#endif  //_TCPCLV3_BIDIRECTIONAL_LINK_H
