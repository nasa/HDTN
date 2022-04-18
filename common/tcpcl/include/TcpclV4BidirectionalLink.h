/**
 * @file TcpclV4BidirectionalLink.h
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
 * for version 4 of the TCP Convergence-Layer Protocol for the bidirectional
 * nature of the TCPCL protocol, such that any "bundle source" must be prepared
 * to receive bundles, and any "bundle sink" must be prepared to send bundles,
 * and in both cases they must share the same underlying TCP socket/connection.
 * This class can be compiled with or without OpenSSL support.
 */

#ifndef _TCPCLV4_BIDIRECTIONAL_LINK_H
#define _TCPCLV4_BIDIRECTIONAL_LINK_H 1

#include <string>
#include <boost/thread.hpp>
#include <boost/asio.hpp>
#include <map>
#include <vector>
#include "TcpclV4.h"
#include "TcpAsyncSender.h"
#include "CircularIndexBufferSingleProducerSingleConsumerConfigurable.h"
#include "BidirectionalLink.h"
#ifdef OPENSSL_SUPPORT_ENABLED
#include <boost/asio/ssl.hpp>

//DO NOT USE THE FOLLOWING from earlier TCPCLv4:
//generate an x509 version 3 key with an IPN URI subjectAltName:
//C:\openssl-1.1.1e_msvc2017\bin\openssl.exe req -x509 -newkey rsa:4096 -nodes -keyout privatekey.pem -out cert.pem -sha256 -days 365 -extensions v3_req -extensions v3_ca -subj "/C=US/ST=Ohio/L=Cleveland/O=NASA/OU=HDTN/CN=localhost" -addext "subjectAltName = URI:ipn:10.0" -config C:\Users\btomko\Downloads\openssl-1.1.1e\apps\openssl.cnf

//INSTEAD, USE THE RFC 9174 certificate profile:
//generate an x509 version 3 key with an otherName subjectAltName:
//C:\openssl-1.1.1e_msvc2017\bin\openssl.exe req -x509 -newkey rsa:4096 -nodes -keyout privatekey.pem -out cert.pem -sha256 -days 365 -extensions v3_req -extensions v3_ca -subj "/C=US/ST=Ohio/L=Cleveland/O=NASA/OU=HDTN/CN=localhost" -addext "subjectAltName = otherName:1.3.6.1.5.5.7.8.11;IA5:ipn:10.0" -config C:\Users\btomko\Downloads\openssl-1.1.1e\apps\openssl.cnf


//generate the dh file:
//C:\openssl-1.1.1e_msvc2017\bin\openssl.exe dhparam -outform PEM -out dh4096.pem 4096

#endif

class CLASS_VISIBILITY_TCPCL_LIB TcpclV4BidirectionalLink : public BidirectionalLink {
public:
    /*
    //sink
    typedef boost::function<void(std::vector<uint8_t> & wholeBundleVec)> WholeBundleReadyCallback_t;
    typedef boost::function<void()> NotifyReadyToDeleteCallback_t;
    typedef boost::function<bool(std::pair<std::unique_ptr<zmq::message_t>, std::vector<uint8_t> > & bundleDataPair)> TryGetOpportunisticDataFunction_t;
    typedef boost::function<void()> NotifyOpportunisticDataAckedCallback_t;
    typedef boost::function<void(TcpclV4BidirectionalLink * thisTcpclBundleSinkPtr)> OnContactHeaderCallback_t;
    //source
    typedef boost::function<void(std::vector<uint8_t> & movableBundle)> OutductOpportunisticProcessReceivedBundleCallback_t;
    typedef boost::function<void()> OnSuccessfulAckCallback_t;*/
private:
    TcpclV4BidirectionalLink();
public:


    TCPCL_LIB_EXPORT TcpclV4BidirectionalLink(
        const std::string & implementationStringForCout,
        const uint64_t reconnectionDelaySecondsIfNotZero, //source
        const bool deleteSocketAfterShutdown,
        const bool isActiveEntity,
        const uint16_t desiredKeepAliveIntervalSeconds,
        boost::asio::io_service * externalIoServicePtr,
        const unsigned int myMaxTxUnackedBundles,
        const uint64_t myMaxRxSegmentSizeBytes,
        const uint64_t myMaxRxBundleSizeBytes,
        const uint64_t myNodeId,
        const std::string & expectedRemoteEidUriStringIfNotEmpty,
        const bool tryUseTls,
        const bool tlsIsRequired
    );

    TCPCL_LIB_EXPORT virtual ~TcpclV4BidirectionalLink();
    TCPCL_LIB_EXPORT bool BaseClass_Forward(const uint8_t* bundleData, const std::size_t size);
    TCPCL_LIB_EXPORT bool BaseClass_Forward(std::vector<uint8_t> & dataVec);
    TCPCL_LIB_EXPORT bool BaseClass_Forward(zmq::message_t & dataZmq);
    TCPCL_LIB_EXPORT bool BaseClass_Forward(std::unique_ptr<zmq::message_t> & zmqMessageUniquePtr, std::vector<uint8_t> & vecMessage, const bool usingZmqData);

    TCPCL_LIB_EXPORT virtual std::size_t Virtual_GetTotalBundlesAcked();
    TCPCL_LIB_EXPORT virtual std::size_t Virtual_GetTotalBundlesSent();
    TCPCL_LIB_EXPORT virtual std::size_t Virtual_GetTotalBundlesUnacked();
    TCPCL_LIB_EXPORT virtual std::size_t Virtual_GetTotalBundleBytesAcked();
    TCPCL_LIB_EXPORT virtual std::size_t Virtual_GetTotalBundleBytesSent();
    TCPCL_LIB_EXPORT virtual std::size_t Virtual_GetTotalBundleBytesUnacked();

    TCPCL_LIB_EXPORT virtual unsigned int Virtual_GetMaxTxBundlesInPipeline();

protected:
    TCPCL_LIB_EXPORT void BaseClass_SendContactHeader();
    TCPCL_LIB_EXPORT void BaseClass_SendSessionInit();
    TCPCL_LIB_EXPORT void BaseClass_TryToWaitForAllBundlesToFinishSending();
    TCPCL_LIB_EXPORT void BaseClass_DoTcpclShutdown(bool doCleanShutdown, TCPCLV4_SESSION_TERMINATION_REASON_CODES sessionTerminationReasonCode, bool isAckOfAnEarlierSessionTerminationMessage);
    virtual void Virtual_OnTcpclShutdownComplete_CalledFromIoServiceThread() = 0;
    virtual void Virtual_OnSuccessfulWholeBundleAcknowledged() = 0;
    virtual void Virtual_WholeBundleReady(padded_vector_uint8_t & wholeBundleVec) = 0;
    TCPCL_LIB_EXPORT virtual void Virtual_OnTcpSendSuccessful_CalledFromIoServiceThread();
    TCPCL_LIB_EXPORT virtual void Virtual_OnTcpSendContactHeaderSuccessful_CalledFromIoServiceThread();
    TCPCL_LIB_EXPORT virtual void Virtual_OnSessionInitReceivedAndProcessedSuccessfully();

private:
    TCPCL_LIB_NO_EXPORT void BaseClass_DataSegmentCallback(padded_vector_uint8_t & dataSegmentDataVec, bool isStartFlag, bool isEndFlag,
        uint64_t transferId, const TcpclV4::tcpclv4_extensions_t & transferExtensions);
    TCPCL_LIB_NO_EXPORT void BaseClass_AckCallback(const TcpclV4::tcpclv4_ack_t & ack);
    TCPCL_LIB_NO_EXPORT void BaseClass_KeepAliveCallback();
    TCPCL_LIB_NO_EXPORT void BaseClass_ContactHeaderCallback(bool remoteHasEnabledTlsSecurity);
    TCPCL_LIB_NO_EXPORT void BaseClass_SessionInitCallback(uint16_t keepAliveIntervalSeconds, uint64_t segmentMru, uint64_t transferMru,
        const std::string & remoteNodeEidUri, const TcpclV4::tcpclv4_extensions_t & sessionExtensions);
    TCPCL_LIB_NO_EXPORT void BaseClass_SessionTerminationMessageCallback(TCPCLV4_SESSION_TERMINATION_REASON_CODES terminationReasonCode, bool isAckOfAnEarlierSessionTerminationMessage);
    TCPCL_LIB_NO_EXPORT void BaseClass_MessageRejectCallback(TCPCLV4_MESSAGE_REJECT_REASON_CODES refusalCode, uint8_t rejectedMessageHeader);
    TCPCL_LIB_NO_EXPORT void BaseClass_BundleRefusalCallback(TCPCLV4_TRANSFER_REFUSE_REASON_CODES refusalCode, uint64_t transferId);

    
    TCPCL_LIB_NO_EXPORT void BaseClass_DoHandleSocketShutdown(bool doCleanShutdown, TCPCLV4_SESSION_TERMINATION_REASON_CODES sessionTerminationReasonCode, bool isAckOfAnEarlierSessionTerminationMessage);
    TCPCL_LIB_NO_EXPORT void BaseClass_OnSendShutdownMessageTimeout_TimerExpired(const boost::system::error_code& e, bool isAckOfAnEarlierSessionTerminationMessage);
    TCPCL_LIB_NO_EXPORT void BaseClass_OnWaitForSessionTerminationAckTimeout_TimerExpired(const boost::system::error_code& e);
    TCPCL_LIB_NO_EXPORT void BaseClass_RemainInEndingState_TimerExpired(const boost::system::error_code& e);
    TCPCL_LIB_NO_EXPORT void BaseClass_RestartNoKeepaliveReceivedTimer();
    TCPCL_LIB_NO_EXPORT void BaseClass_RestartNeedToSendKeepAliveMessageTimer();
    TCPCL_LIB_NO_EXPORT void BaseClass_OnNoKeepAlivePacketReceived_TimerExpired(const boost::system::error_code& e);
    TCPCL_LIB_NO_EXPORT void BaseClass_OnNeedToSendKeepAliveMessage_TimerExpired(const boost::system::error_code& e);
    TCPCL_LIB_NO_EXPORT void BaseClass_HandleTcpSend(const boost::system::error_code& error, std::size_t bytes_transferred);
    TCPCL_LIB_NO_EXPORT void BaseClass_HandleTcpSendContactHeader(const boost::system::error_code& error, std::size_t bytes_transferred);
    TCPCL_LIB_NO_EXPORT void BaseClass_HandleTcpSendShutdown(const boost::system::error_code& error, std::size_t bytes_transferred);
    
    TCPCL_LIB_NO_EXPORT void BaseClass_CloseAndDeleteSockets();

protected:
    const std::string M_BASE_IMPLEMENTATION_STRING_FOR_COUT;
    const uint64_t M_BASE_SHUTDOWN_MESSAGE_RECONNECTION_DELAY_SECONDS_TO_SEND;
    const uint16_t M_BASE_DESIRED_KEEPALIVE_INTERVAL_SECONDS;
    const bool M_BASE_DELETE_SOCKET_AFTER_SHUTDOWN;
    const bool M_BASE_IS_ACTIVE_ENTITY;
    const std::string M_BASE_THIS_TCPCL_EID_STRING;
    const bool M_BASE_TRY_USE_TLS;
    const bool M_BASE_TLS_IS_REQUIRED;
    bool m_base_usingTls;
    std::string M_BASE_EXPECTED_REMOTE_CONTACT_HEADER_EID_STRING_IF_NOT_EMPTY;
    uint16_t m_base_keepAliveIntervalSeconds;
    std::unique_ptr<boost::asio::io_service> m_base_localIoServiceUniquePtr; //if an external one is not provided, create it here and set the ioServiceRef below to it
    boost::asio::io_service & m_base_ioServiceRef;
    boost::asio::deadline_timer m_base_noKeepAlivePacketReceivedTimer;
    boost::asio::deadline_timer m_base_needToSendKeepAliveMessageTimer;
    boost::asio::deadline_timer m_base_sendSessionTerminationMessageTimeoutTimer;
    boost::asio::deadline_timer m_base_waitForSessionTerminationAckTimeoutTimer;
    boost::asio::deadline_timer m_base_remainInEndingStateTimer;
    bool m_base_shutdownCalled;
    volatile bool m_base_readyToForward; //bundleSource
    volatile bool m_base_sinkIsSafeToDelete; //bundleSink
    volatile bool m_base_tcpclShutdownComplete; //bundleSource
    volatile bool m_base_useLocalConditionVariableAckReceived; //bundleSource
    volatile bool m_base_dataReceivedServedAsKeepaliveReceived;
    volatile bool m_base_dataSentServedAsKeepaliveSent;
    bool m_base_doUpgradeSocketToSsl;
    bool m_base_didSuccessfulSslHandshake;
    boost::condition_variable m_base_localConditionVariableAckReceived;
    uint64_t m_base_reconnectionDelaySecondsIfNotZero; //bundle source only, increases with exponential back-off mechanism

    TcpclV4 m_base_tcpclV4RxStateMachine;
    uint64_t m_base_myNextTransferId;
    std::string m_base_tcpclRemoteEidString;
    uint64_t m_base_tcpclRemoteNodeId;
#ifdef OPENSSL_SUPPORT_ENABLED
    boost::shared_ptr< boost::asio::ssl::stream<boost::asio::ip::tcp::socket> > m_base_sslStreamSharedPtr;
    std::unique_ptr<TcpAsyncSenderSsl> m_base_tcpAsyncSenderSslPtr;
#else
    boost::shared_ptr<boost::asio::ip::tcp::socket> m_base_tcpSocketPtr;
    std::unique_ptr<TcpAsyncSender> m_base_tcpAsyncSenderPtr;
#endif
    TcpAsyncSenderElement::OnSuccessfulSendCallbackByIoServiceThread_t m_base_handleTcpSendCallback;
    TcpAsyncSenderElement::OnSuccessfulSendCallbackByIoServiceThread_t m_base_handleTcpSendContactHeaderCallback;
    TcpAsyncSenderElement::OnSuccessfulSendCallbackByIoServiceThread_t m_base_handleTcpSendShutdownCallback;
    padded_vector_uint8_t m_base_fragmentedBundleRxConcat;

    const unsigned int M_BASE_MY_MAX_TX_UNACKED_BUNDLES;
    std::unique_ptr<CircularIndexBufferSingleProducerSingleConsumerConfigurable> m_base_segmentsToAckCbPtr; //CircularIndexBufferSingleProducerSingleConsumerConfigurable m_base_bytesToAckCb;
    std::vector<TcpclV4::tcpclv4_ack_t> m_base_segmentsToAckCbVec; //std::vector<uint64_t> m_base_bytesToAckCbVec;
    std::vector<std::vector<uint64_t> > m_base_fragmentBytesToAckCbVec;
    std::vector<uint64_t> m_base_fragmentVectorIndexCbVec;
    const uint64_t M_BASE_MY_MAX_RX_SEGMENT_SIZE_BYTES;
    const uint64_t M_BASE_MY_MAX_RX_BUNDLE_SIZE_BYTES;
    uint64_t m_base_remoteMaxRxSegmentSizeBytes;
    uint64_t m_base_remoteMaxRxBundleSizeBytes;
    uint64_t m_base_remoteMaxRxSegmentsPerBundle;
    uint64_t m_base_maxUnackedSegments;
    uint64_t m_base_ackCbSize;

public:
    //tcpcl stats
    std::size_t m_base_totalBundlesAcked;
    std::size_t m_base_totalBytesAcked;
    std::size_t m_base_totalBundlesSent;
    std::size_t m_base_totalFragmentedAcked;
    std::size_t m_base_totalFragmentedSent;
    std::size_t m_base_totalBundleBytesSent;
};



#endif  //_TCPCLV4_BIDIRECTIONAL_LINK_H
