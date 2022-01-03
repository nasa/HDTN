#include <string>
#include <iostream>
#include "TcpclV4BidirectionalLink.h"
#include <boost/lexical_cast.hpp>
#include <boost/make_shared.hpp>
#include <boost/make_unique.hpp>
#include "Uri.h"
#include <boost/endian/conversion.hpp>

TcpclV4BidirectionalLink::TcpclV4BidirectionalLink(
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
) :
    M_BASE_IMPLEMENTATION_STRING_FOR_COUT(implementationStringForCout),
    M_BASE_SHUTDOWN_MESSAGE_RECONNECTION_DELAY_SECONDS_TO_SEND(reconnectionDelaySecondsIfNotZero),
    M_BASE_DESIRED_KEEPALIVE_INTERVAL_SECONDS(desiredKeepAliveIntervalSeconds),
    M_BASE_DELETE_SOCKET_AFTER_SHUTDOWN(deleteSocketAfterShutdown),
    M_BASE_IS_ACTIVE_ENTITY(isActiveEntity),
    //ion 3.7.2 source code tcpcli.c line 1199 uses service number 0 for contact header:
    //isprintf(eid, sizeof eid, "ipn:" UVAST_FIELDSPEC ".0", getOwnNodeNbr());
    M_BASE_THIS_TCPCL_EID_STRING(Uri::GetIpnUriString(myNodeId, 0)),
    M_BASE_TRY_USE_TLS(tryUseTls),
    M_BASE_TLS_IS_REQUIRED(tlsIsRequired),
    m_base_usingTls(false), //initialization not needed
    m_base_keepAliveIntervalSeconds(desiredKeepAliveIntervalSeconds),
    m_base_localIoServiceUniquePtr((externalIoServicePtr == NULL) ? boost::make_unique<boost::asio::io_service>() : std::unique_ptr<boost::asio::io_service>()),
    m_base_ioServiceRef((externalIoServicePtr == NULL) ? (*m_base_localIoServiceUniquePtr) : (*externalIoServicePtr)),
    m_base_noKeepAlivePacketReceivedTimer(m_base_ioServiceRef),
    m_base_needToSendKeepAliveMessageTimer(m_base_ioServiceRef),
    m_base_sendSessionTerminationMessageTimeoutTimer(m_base_ioServiceRef),
    m_base_waitForSessionTerminationAckTimeoutTimer(m_base_ioServiceRef),
    m_base_remainInEndingStateTimer(m_base_ioServiceRef),

    m_base_shutdownCalled(false),
    m_base_readyToForward(false), //bundleSource
    m_base_sinkIsSafeToDelete(false), //bundleSink
    m_base_tcpclShutdownComplete(true), //bundleSource
    m_base_useLocalConditionVariableAckReceived(false), //for bundleSource destructor only
    m_base_doUpgradeSocketToSsl(false),
    m_base_didSuccessfulSslHandshake(false),
    m_base_reconnectionDelaySecondsIfNotZero(3), //bundle source only, start at 3, increases with exponential back-off mechanism

    m_base_myNextTransferId(0),

    M_BASE_MY_MAX_TX_UNACKED_BUNDLES(myMaxTxUnackedBundles), //bundle sink has MAX_UNACKED(maxUnacked + 5),
    M_BASE_MY_MAX_RX_SEGMENT_SIZE_BYTES(myMaxRxSegmentSizeBytes),
    M_BASE_MY_MAX_RX_BUNDLE_SIZE_BYTES(myMaxRxBundleSizeBytes),
    /*m_base_bytesToAckCb(M_BASE_MAX_UNACKED),
    m_base_bytesToAckCbVec(M_BASE_MAX_UNACKED),
    m_base_fragmentBytesToAckCbVec(M_BASE_MAX_UNACKED),
    m_base_fragmentVectorIndexCbVec(M_BASE_MAX_UNACKED),
    M_BASE_MAX_FRAGMENT_SIZE(maxFragmentSize),
    */

    //stats
    m_base_totalBundlesAcked(0),
    m_base_totalBytesAcked(0),
    m_base_totalBundlesSent(0),
    m_base_totalFragmentedAcked(0),
    m_base_totalFragmentedSent(0),
    m_base_totalBundleBytesSent(0)
{
    
    m_base_tcpclV4RxStateMachine.SetMaxReceiveBundleSizeBytes(myMaxRxBundleSizeBytes);

    
    m_base_tcpclV4RxStateMachine.SetContactHeaderReadCallback(boost::bind(&TcpclV4BidirectionalLink::BaseClass_ContactHeaderCallback, this, boost::placeholders::_1));
    m_base_tcpclV4RxStateMachine.SetSessionInitReadCallback(boost::bind(&TcpclV4BidirectionalLink::BaseClass_SessionInitCallback, this, boost::placeholders::_1, boost::placeholders::_2, boost::placeholders::_3,
        boost::placeholders::_4, boost::placeholders::_5));
    m_base_tcpclV4RxStateMachine.SetDataSegmentContentsReadCallback(boost::bind(&TcpclV4BidirectionalLink::BaseClass_DataSegmentCallback, this, boost::placeholders::_1, boost::placeholders::_2, boost::placeholders::_3,
        boost::placeholders::_4, boost::placeholders::_5));
    m_base_tcpclV4RxStateMachine.SetAckSegmentReadCallback(boost::bind(&TcpclV4BidirectionalLink::BaseClass_AckCallback, this, boost::placeholders::_1));
    m_base_tcpclV4RxStateMachine.SetBundleRefusalCallback(boost::bind(&TcpclV4BidirectionalLink::BaseClass_BundleRefusalCallback, this, boost::placeholders::_1, boost::placeholders::_2));
    m_base_tcpclV4RxStateMachine.SetMessageRejectCallback(boost::bind(&TcpclV4BidirectionalLink::BaseClass_MessageRejectCallback, this, boost::placeholders::_1, boost::placeholders::_2));
    m_base_tcpclV4RxStateMachine.SetKeepAliveCallback(boost::bind(&TcpclV4BidirectionalLink::BaseClass_KeepAliveCallback, this));
    m_base_tcpclV4RxStateMachine.SetSessionTerminationMessageCallback(boost::bind(&TcpclV4BidirectionalLink::BaseClass_SessionTerminationMessageCallback, this, boost::placeholders::_1, boost::placeholders::_2));
    

    m_base_handleTcpSendCallback = boost::bind(&TcpclV4BidirectionalLink::BaseClass_HandleTcpSend, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred);
    m_base_handleTcpSendContactHeaderCallback = boost::bind(&TcpclV4BidirectionalLink::BaseClass_HandleTcpSendContactHeader, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred);
    m_base_handleTcpSendShutdownCallback = boost::bind(&TcpclV4BidirectionalLink::BaseClass_HandleTcpSendShutdown, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred);


    if (expectedRemoteEidUriStringIfNotEmpty.empty()) {
        M_BASE_EXPECTED_REMOTE_CONTACT_HEADER_EID_STRING_IF_NOT_EMPTY = "";
    }
    else {
        uint64_t remoteNodeId, remoteServiceId;
        if (!Uri::ParseIpnUriString(expectedRemoteEidUriStringIfNotEmpty, remoteNodeId, remoteServiceId)) {
            std::cerr << M_BASE_IMPLEMENTATION_STRING_FOR_COUT << ": error in constructor: error parsing remote EID URI string " << expectedRemoteEidUriStringIfNotEmpty
                << " .. TCPCLV4 will fail the Session Init Callback.  Correct the \"nextHopEndpointId\" field in the outducts config." << std::endl;
        }
        else {
            //ion 3.7.2 source code tcpcli.c line 1199 uses service number 0 for contact header:
            //isprintf(eid, sizeof eid, "ipn:" UVAST_FIELDSPEC ".0", getOwnNodeNbr());
            M_BASE_EXPECTED_REMOTE_CONTACT_HEADER_EID_STRING_IF_NOT_EMPTY = Uri::GetIpnUriString(remoteNodeId, 0);
        }
    }
}

TcpclV4BidirectionalLink::~TcpclV4BidirectionalLink() {

}

void TcpclV4BidirectionalLink::BaseClass_TryToWaitForAllBundlesToFinishSending() {
    boost::mutex localMutex;
    boost::mutex::scoped_lock lock(localMutex);
    m_base_useLocalConditionVariableAckReceived = true;
    std::size_t previousUnacked = std::numeric_limits<std::size_t>::max();
    for (unsigned int attempt = 0; attempt < 10; ++attempt) {
        const std::size_t numUnacked = Virtual_GetTotalBundlesUnacked();
        if (numUnacked) {
            std::cout << M_BASE_IMPLEMENTATION_STRING_FOR_COUT << ": notice: destructor waiting on " << numUnacked << " unacked bundles" << std::endl;

            std::cout << "   acked: " << m_base_totalBundlesAcked << std::endl;
            std::cout << "   total sent: " << m_base_totalBundlesSent << std::endl;

            if (previousUnacked > numUnacked) {
                previousUnacked = numUnacked;
                attempt = 0;
            }
            m_base_localConditionVariableAckReceived.timed_wait(lock, boost::posix_time::milliseconds(250)); // call lock.unlock() and blocks the current thread
            //thread is now unblocked, and the lock is reacquired by invoking lock.lock()
            continue;
        }
        break;
    }
    m_base_useLocalConditionVariableAckReceived = false;
}

std::size_t TcpclV4BidirectionalLink::Virtual_GetTotalBundlesAcked() {
    return m_base_totalBundlesAcked;
}

std::size_t TcpclV4BidirectionalLink::Virtual_GetTotalBundlesSent() {
    return m_base_totalBundlesSent;
}

std::size_t TcpclV4BidirectionalLink::Virtual_GetTotalBundlesUnacked() {
    return m_base_totalBundlesSent - m_base_totalBundlesAcked;
}

std::size_t TcpclV4BidirectionalLink::Virtual_GetTotalBundleBytesAcked() {
    return m_base_totalBytesAcked;
}

std::size_t TcpclV4BidirectionalLink::Virtual_GetTotalBundleBytesSent() {
    return m_base_totalBundleBytesSent;
}

std::size_t TcpclV4BidirectionalLink::Virtual_GetTotalBundleBytesUnacked() {
    return m_base_totalBundleBytesSent - m_base_totalBytesAcked;
}

unsigned int TcpclV4BidirectionalLink::Virtual_GetMaxTxBundlesInPipeline() {
    return M_BASE_MY_MAX_TX_UNACKED_BUNDLES;
}

void TcpclV4BidirectionalLink::BaseClass_HandleTcpSend(const boost::system::error_code& error, std::size_t bytes_transferred) {
    if (error) {
        std::cerr << M_BASE_IMPLEMENTATION_STRING_FOR_COUT << ": error in BaseClass_HandleTcpSend: " << error.message() << std::endl;
        BaseClass_DoTcpclShutdown(true, TCPCLV4_SESSION_TERMINATION_REASON_CODES::UNKNOWN, false);
    }
    else {
        Virtual_OnTcpSendSuccessful_CalledFromIoServiceThread();
    }
}

void TcpclV4BidirectionalLink::BaseClass_HandleTcpSendContactHeader(const boost::system::error_code& error, std::size_t bytes_transferred) {
    if (error) {
        std::cerr << M_BASE_IMPLEMENTATION_STRING_FOR_COUT << ": error in BaseClass_HandleTcpSendContactHeader: " << error.message() << std::endl;
        BaseClass_DoTcpclShutdown(true, TCPCLV4_SESSION_TERMINATION_REASON_CODES::UNKNOWN, false);
    }
    else {
        Virtual_OnTcpSendContactHeaderSuccessful_CalledFromIoServiceThread();
    }
}

void TcpclV4BidirectionalLink::BaseClass_HandleTcpSendShutdown(const boost::system::error_code& error, std::size_t bytes_transferred) {
    if (error) {
        std::cerr << M_BASE_IMPLEMENTATION_STRING_FOR_COUT << ": error in BaseClass_HandleTcpSendShutdown: " << error.message() << std::endl;
    }
    else {
        m_base_sendSessionTerminationMessageTimeoutTimer.cancel();
    }
}

void TcpclV4BidirectionalLink::Virtual_OnTcpSendSuccessful_CalledFromIoServiceThread() {}
void TcpclV4BidirectionalLink::Virtual_OnTcpSendContactHeaderSuccessful_CalledFromIoServiceThread() {}

void TcpclV4BidirectionalLink::BaseClass_DataSegmentCallback(std::vector<uint8_t> & dataSegmentDataVec, bool isStartFlag, bool isEndFlag,
    uint64_t transferId, const TcpclV4::tcpclv4_extensions_t & transferExtensions)
{
    bool detectedLengthExtension = false;
    uint64_t bundleLength;
    if (transferExtensions.extensionsVec.size()) {
        for (std::size_t i = 0; i < transferExtensions.extensionsVec.size(); ++i) {
            const TcpclV4::tcpclv4_extension_t & ext = transferExtensions.extensionsVec[i];

            if (ext.type == 0x0001) { //The Transfer Length extension SHALL be assigned transfer extension type ID 0x0001.
                if (ext.valueVec.size() == sizeof(bundleLength)) { //length is 64 bit
                    memcpy(&bundleLength, ext.valueVec.data(), sizeof(bundleLength));
                    boost::endian::big_to_native_inplace(bundleLength);
                    if (bundleLength > M_BASE_MY_MAX_RX_BUNDLE_SIZE_BYTES) {
                        std::cout << M_BASE_IMPLEMENTATION_STRING_FOR_COUT << ": error in BaseClass_DataSegmentCallback: received length transfer extension with bundle length ("
                            << bundleLength << ") greater than my bundle capacity of " << M_BASE_MY_MAX_RX_BUNDLE_SIZE_BYTES << " bytes\n";
                        //todo refuse bundle
                    }
                    else {
                        detectedLengthExtension = true;
                    }
                }
                else {
                    std::cout << "error in TcpclV4BidirectionalLink::BaseClass_DataSegmentCallback: received length transfer extension with invalid payload size\n";
                }
            }
            else if (ext.IsCritical()) {
                //If a TCPCL entity receives a
                //Transfer Extension Item with an unknown Item Type and the CRITICAL
                //flag is 1, the entity SHALL refuse the transfer with an
                //XFER_REFUSE reason code of "Extension Failure".  If the CRITICAL
                //flag is 0, an entity SHALL skip over and ignore any item with an
                //unknown Item Type.
                std::cout << "error in TcpclV4BidirectionalLink::BaseClass_DataSegmentCallback: received unknown critical transfer extension type " << ext.type
                    << " ..terminating session.\n";
            }
            else {
                //std::cout << "notice: TcpclV4BundleSource::SessionInitCallback: ignoring unknown non-critical session extension type " << ext.type << std::endl;
            }
        }
    }

    uint64_t bytesToAck = 0;
    if (isStartFlag && isEndFlag) { //optimization for whole (non-fragmented) data
        bytesToAck = dataSegmentDataVec.size(); //grab the size now in case vector gets stolen in m_wholeBundleReadyCallback
        Virtual_WholeBundleReady(dataSegmentDataVec);
        //std::cout << dataSegmentDataSharedPtr->size() << std::endl;
    }
    else {
        if (isStartFlag) {
            m_base_fragmentedBundleRxConcat.resize(0);
            //(!isEndFlag) is assumed from if else
            if (detectedLengthExtension) {
                m_base_fragmentedBundleRxConcat.reserve(bundleLength);
            }
            else {
                std::cout << "warning in TcpclV4BidirectionalLink::BaseClass_DataSegmentCallback: received fragmented start segment with no length extension\n";
            }
        }
        m_base_fragmentedBundleRxConcat.insert(m_base_fragmentedBundleRxConcat.end(), dataSegmentDataVec.begin(), dataSegmentDataVec.end()); //concatenate
        bytesToAck = m_base_fragmentedBundleRxConcat.size();
        if (isEndFlag) { //fragmentation complete
            Virtual_WholeBundleReady(m_base_fragmentedBundleRxConcat);
        }
    }
    //always send ack in tcpclv4
    //A receiving TCPCL entity SHALL send an XFER_ACK message in response
    //to each received XFER_SEGMENT message after the segment has been
    //fully processed.  The flags portion of the XFER_ACK header SHALL be
    //set to match the corresponding XFER_SEGMENT message being
    //acknowledged (including flags not decodable to the entity).  The
    //acknowledged length of each XFER_ACK contains the sum of the data
    //length fields of all XFER_SEGMENT messages received so far in the
    //course of the indicated transfer.  The sending entity SHOULD transmit
    //multiple XFER_SEGMENT messages without waiting for the corresponding
    //XFER_ACK responses.  This enables pipelining of messages on a
    //transfer stream.
#ifdef OPENSSL_SUPPORT_ENABLED
    if (m_base_sslStreamSharedPtr) {
#else
    if (m_base_tcpSocketPtr) {
#endif
        TcpAsyncSenderElement * el = new TcpAsyncSenderElement();
        el->m_underlyingData.resize(1);
        TcpclV4::GenerateAckSegment(el->m_underlyingData[0], TcpclV4::tcpclv4_ack_t(isStartFlag, isEndFlag, transferId, bytesToAck));
        el->m_constBufferVec.emplace_back(boost::asio::buffer(el->m_underlyingData[0])); //only one element so resize not needed
        el->m_onSuccessfulSendCallbackByIoServiceThreadPtr = &m_base_handleTcpSendCallback;
#ifdef OPENSSL_SUPPORT_ENABLED
        if (m_base_usingTls) {
            m_base_tcpAsyncSenderSslPtr->AsyncSendSecure_ThreadSafe(el);
        }
        else {
            m_base_tcpAsyncSenderSslPtr->AsyncSendUnsecure_ThreadSafe(el);
        }
#else
        m_base_tcpAsyncSenderPtr->AsyncSend_ThreadSafe(el);
#endif
    }

}

void TcpclV4BidirectionalLink::BaseClass_AckCallback(const TcpclV4::tcpclv4_ack_t & ack) {
    const unsigned int readIndex = m_base_segmentsToAckCbPtr->GetIndexForRead();
    if (readIndex == UINT32_MAX) { //empty
        std::cerr << M_BASE_IMPLEMENTATION_STRING_FOR_COUT << ": error: BaseClass_AckCallback called with empty queue" << std::endl;
        return;
    }

    const TcpclV4::tcpclv4_ack_t & toAckFromQueue = m_base_segmentsToAckCbVec[readIndex];
    if (toAckFromQueue == ack) {
        m_base_segmentsToAckCbPtr->CommitRead();
        const bool isFragment = !(ack.isStartSegment && ack.isEndSegment);
        m_base_totalFragmentedAcked += isFragment;
        if (ack.isEndSegment) { //entire bundle
            ++m_base_totalBundlesAcked;
            m_base_totalBytesAcked += ack.totalBytesAcknowledged;
            Virtual_OnSuccessfulWholeBundleAcknowledged();
            if (m_base_useLocalConditionVariableAckReceived) {
                m_base_localConditionVariableAckReceived.notify_one();
            }
        }
    }
    else {
        std::cerr << M_BASE_IMPLEMENTATION_STRING_FOR_COUT << ": error in BaseClass_AckCallback: wrong ack: expected " << toAckFromQueue << " but got " << ack << std::endl;
    }
}

void TcpclV4BidirectionalLink::BaseClass_KeepAliveCallback() {
    std::cout << M_BASE_IMPLEMENTATION_STRING_FOR_COUT << ": received keepalive packet\n";
    // * 2 =>
    //If no message (KEEPALIVE or other) has been received for at least
    //twice the keepalive_interval, then either party MAY terminate the
    //session by transmitting a one-byte SHUTDOWN message (as described in
    //Table 2) and by closing the TCP connection.
    m_base_noKeepAlivePacketReceivedTimer.expires_from_now(boost::posix_time::seconds(m_base_keepAliveIntervalSeconds * 2)); //cancels active timer with cancel flag in callback
    m_base_noKeepAlivePacketReceivedTimer.async_wait(boost::bind(&TcpclV4BidirectionalLink::BaseClass_OnNoKeepAlivePacketReceived_TimerExpired, this, boost::asio::placeholders::error));
}

void TcpclV4BidirectionalLink::BaseClass_OnNoKeepAlivePacketReceived_TimerExpired(const boost::system::error_code& e) {
    if (e != boost::asio::error::operation_aborted) {
        // Timer was not cancelled, take necessary action.
        BaseClass_DoTcpclShutdown(true, TCPCLV4_SESSION_TERMINATION_REASON_CODES::IDLE_TIMEOUT, false);
    }
    else {
        //std::cout << "timer cancelled\n";
    }
}

void TcpclV4BidirectionalLink::BaseClass_OnNeedToSendKeepAliveMessage_TimerExpired(const boost::system::error_code& e) {
    if (e != boost::asio::error::operation_aborted) {
        // Timer was not cancelled, take necessary action.
        //SEND KEEPALIVE PACKET
#ifdef OPENSSL_SUPPORT_ENABLED
        if (m_base_sslStreamSharedPtr) {
#else
        if (m_base_tcpSocketPtr) {
#endif
            TcpAsyncSenderElement * el = new TcpAsyncSenderElement();
            el->m_underlyingData.resize(1);
            TcpclV4::GenerateKeepAliveMessage(el->m_underlyingData[0]);
            el->m_constBufferVec.emplace_back(boost::asio::buffer(el->m_underlyingData[0])); //only one element so resize not needed
            el->m_onSuccessfulSendCallbackByIoServiceThreadPtr = &m_base_handleTcpSendCallback;
#ifdef OPENSSL_SUPPORT_ENABLED
            if (m_base_usingTls) {
                m_base_tcpAsyncSenderSslPtr->AsyncSendSecure_NotThreadSafe(el); //timer runs in same thread as socket so special thread safety not needed
            }
            else {
                m_base_tcpAsyncSenderSslPtr->AsyncSendUnsecure_NotThreadSafe(el); //timer runs in same thread as socket so special thread safety not needed
            }
#else
            m_base_tcpAsyncSenderPtr->AsyncSend_NotThreadSafe(el); //timer runs in same thread as socket so special thread safety not needed
#endif
            m_base_needToSendKeepAliveMessageTimer.expires_from_now(boost::posix_time::seconds(m_base_keepAliveIntervalSeconds));
            m_base_needToSendKeepAliveMessageTimer.async_wait(boost::bind(&TcpclV4BidirectionalLink::BaseClass_OnNeedToSendKeepAliveMessage_TimerExpired, this, boost::asio::placeholders::error));
        }

    }
    else {
        //std::cout << "timer cancelled\n";
    }
}

void TcpclV4BidirectionalLink::BaseClass_SessionTerminationMessageCallback(TCPCLV4_SESSION_TERMINATION_REASON_CODES terminationReasonCode, bool isAckOfAnEarlierSessionTerminationMessage) {


    if (isAckOfAnEarlierSessionTerminationMessage) {
        std::cout << M_BASE_IMPLEMENTATION_STRING_FOR_COUT << ": remote acknowledged my termination request\n";
        m_base_waitForSessionTerminationAckTimeoutTimer.cancel();
    }
    else {
        std::cout << M_BASE_IMPLEMENTATION_STRING_FOR_COUT << ": remote has requested session termination\n";
        //send ack
        BaseClass_DoTcpclShutdown(true, terminationReasonCode, true);
    }

    std::cout << M_BASE_IMPLEMENTATION_STRING_FOR_COUT << ": reason for shutdown: "
        << ((terminationReasonCode == TCPCLV4_SESSION_TERMINATION_REASON_CODES::UNKNOWN) ? "unknown" :
        (terminationReasonCode == TCPCLV4_SESSION_TERMINATION_REASON_CODES::IDLE_TIMEOUT) ? "idle timeout" :
            (terminationReasonCode == TCPCLV4_SESSION_TERMINATION_REASON_CODES::VERSION_MISMATCH) ? "version mismatch" :
            (terminationReasonCode == TCPCLV4_SESSION_TERMINATION_REASON_CODES::BUSY) ? "busy" :
            (terminationReasonCode == TCPCLV4_SESSION_TERMINATION_REASON_CODES::CONTACT_FAILURE) ? "contact failure" :
            (terminationReasonCode == TCPCLV4_SESSION_TERMINATION_REASON_CODES::RESOURCE_EXHAUSTION) ? "resource exhaustion" : "unassigned") << std::endl;
}

void TcpclV4BidirectionalLink::BaseClass_DoTcpclShutdown(bool doCleanShutdown, TCPCLV4_SESSION_TERMINATION_REASON_CODES sessionTerminationReasonCode, bool isAckOfAnEarlierSessionTerminationMessage) {
    boost::asio::post(m_base_ioServiceRef, boost::bind(&TcpclV4BidirectionalLink::BaseClass_DoHandleSocketShutdown, this, doCleanShutdown, sessionTerminationReasonCode, isAckOfAnEarlierSessionTerminationMessage));
}


void TcpclV4BidirectionalLink::BaseClass_DoHandleSocketShutdown(bool doCleanShutdown, TCPCLV4_SESSION_TERMINATION_REASON_CODES sessionTerminationReasonCode, bool isAckOfAnEarlierSessionTerminationMessage) {
    if (!m_base_shutdownCalled) {
        m_base_shutdownCalled = true; //the ending state
        // Timer was cancelled as expected.  This method keeps socket shutdown within io_service thread.

        m_base_readyToForward = false;
#ifdef OPENSSL_SUPPORT_ENABLED
        if (doCleanShutdown && m_base_tcpAsyncSenderSslPtr && m_base_sslStreamSharedPtr) {
#else
        if (doCleanShutdown && m_base_tcpAsyncSenderPtr && m_base_tcpSocketPtr) {
#endif
        
            std::cout << M_BASE_IMPLEMENTATION_STRING_FOR_COUT << " Sending session terminination packet to cleanly close tcpcl.. " << std::endl;
            TcpAsyncSenderElement * el = new TcpAsyncSenderElement();
            el->m_underlyingData.resize(1);

            //To cleanly terminate a session, a SESS_TERM message SHALL be
            //transmitted by either entity at any point following complete
            //transmission of any other message.  When sent to initiate a
            //termination, the REPLY flag of a SESS_TERM message SHALL be 0.  Upon
            //receiving a SESS_TERM message after not sending a SESS_TERM message
            //in the same session, an entity SHALL send an acknowledging SESS_TERM
            //message.  When sent to acknowledge a termination, a SESS_TERM message
            //SHALL have identical data content from the message being acknowledged
            //except for the REPLY flag, which is set to 1 to indicate
            //acknowledgement.

            //Once a SESS_TERM message is sent the state of that TCPCL session
            //changes to Ending.  While the session is in the Ending state, an
            //entity MAY finish an in-progress transfer in either direction.  While
            //the session is in the Ending state, an entity SHALL NOT begin any new
            //outgoing transfer for the remainder of the session.  While the
            //session is in the Ending state, an entity SHALL NOT accept any new
            //incoming transfer for the remainder of the session.  If a new
            //incoming transfer is attempted while in the Ending state, the
            //receiving entity SHALL send an XFER_REFUSE with a Reason Code of
            //"Session Terminating".
            TcpclV4::GenerateSessionTerminationMessage(el->m_underlyingData[0], sessionTerminationReasonCode, isAckOfAnEarlierSessionTerminationMessage);


            el->m_constBufferVec.emplace_back(boost::asio::buffer(el->m_underlyingData[0])); //only one element so resize not needed
            el->m_onSuccessfulSendCallbackByIoServiceThreadPtr = &m_base_handleTcpSendShutdownCallback;
#ifdef OPENSSL_SUPPORT_ENABLED
            if (m_base_usingTls) {
                m_base_tcpAsyncSenderSslPtr->AsyncSendSecure_NotThreadSafe(el); //HandleSocketShutdown runs in same thread as socket so special thread safety not needed
            }
            else {
                m_base_tcpAsyncSenderSslPtr->AsyncSendUnsecure_NotThreadSafe(el); //HandleSocketShutdown runs in same thread as socket so special thread safety not needed
            }
#else
            m_base_tcpAsyncSenderPtr->AsyncSend_NotThreadSafe(el); //HandleSocketShutdown runs in same thread as socket so special thread safety not needed
#endif

            m_base_sendSessionTerminationMessageTimeoutTimer.expires_from_now(boost::posix_time::seconds(3));
            m_base_sendSessionTerminationMessageTimeoutTimer.async_wait(boost::bind(
                &TcpclV4BidirectionalLink::BaseClass_OnSendShutdownMessageTimeout_TimerExpired, this, boost::asio::placeholders::error, isAckOfAnEarlierSessionTerminationMessage));
        }
        else {
            BaseClass_CloseAndDeleteSockets();
        }
    }
}

void TcpclV4BidirectionalLink::BaseClass_OnSendShutdownMessageTimeout_TimerExpired(const boost::system::error_code& e, bool isAckOfAnEarlierSessionTerminationMessage) {
    if (e != boost::asio::error::operation_aborted) {
        // Timer was not cancelled, take necessary action.
        std::cout << M_BASE_IMPLEMENTATION_STRING_FOR_COUT << ": Error: TCPCL session termination message could not be sent." << std::endl;
        BaseClass_CloseAndDeleteSockets();
    }
    else { //operation cancelled by m_base_handleTcpSendShutdownCallback (as expected)
        //std::cout << "timer cancelled\n";
        if (isAckOfAnEarlierSessionTerminationMessage) {
            std::cout << M_BASE_IMPLEMENTATION_STRING_FOR_COUT << ": TCPCL session termination ACK message was sent.  Keeping socket open for 1 second." << std::endl;
            m_base_remainInEndingStateTimer.expires_from_now(boost::posix_time::seconds(1));
            m_base_remainInEndingStateTimer.async_wait(boost::bind(
                &TcpclV4BidirectionalLink::BaseClass_RemainInEndingState_TimerExpired, this, boost::asio::placeholders::error));
        }
        else {
            std::cout << M_BASE_IMPLEMENTATION_STRING_FOR_COUT << ": TCPCL session termination message was sent (initiated by me)." << std::endl;
            m_base_waitForSessionTerminationAckTimeoutTimer.expires_from_now(boost::posix_time::seconds(3));
            m_base_waitForSessionTerminationAckTimeoutTimer.async_wait(boost::bind(
                &TcpclV4BidirectionalLink::BaseClass_OnWaitForSessionTerminationAckTimeout_TimerExpired, this, boost::asio::placeholders::error));
        }
    }
}

void TcpclV4BidirectionalLink::BaseClass_OnWaitForSessionTerminationAckTimeout_TimerExpired(const boost::system::error_code& e) {
    if (e != boost::asio::error::operation_aborted) {
        // Timer was not cancelled, take necessary action.
        std::cout << M_BASE_IMPLEMENTATION_STRING_FOR_COUT << ": Error: TCPCL session termination Ack message was not received." << std::endl;
        BaseClass_CloseAndDeleteSockets();
    }
    else { //operation cancelled by On Session Termination Message Received (as expected)
        //std::cout << "timer cancelled\n";
        std::cout << M_BASE_IMPLEMENTATION_STRING_FOR_COUT << ": TCPCL session termination Ack message was received.  Keeping socket open for 1 second." << std::endl;
        m_base_remainInEndingStateTimer.expires_from_now(boost::posix_time::seconds(1));
        m_base_remainInEndingStateTimer.async_wait(boost::bind(
            &TcpclV4BidirectionalLink::BaseClass_RemainInEndingState_TimerExpired, this, boost::asio::placeholders::error));
    }
}

void TcpclV4BidirectionalLink::BaseClass_RemainInEndingState_TimerExpired(const boost::system::error_code& e) {
    if (e != boost::asio::error::operation_aborted) {
        // Timer was not cancelled, take necessary action.
        std::cout << M_BASE_IMPLEMENTATION_STRING_FOR_COUT << ": Closing socket..." << std::endl;
    }
    BaseClass_CloseAndDeleteSockets();
}

void TcpclV4BidirectionalLink::BaseClass_CloseAndDeleteSockets() {
    //final code to shut down tcp sockets
    if (M_BASE_DELETE_SOCKET_AFTER_SHUTDOWN) { //for bundle sink
        std::cout << M_BASE_IMPLEMENTATION_STRING_FOR_COUT << " deleting TCP Async Sender" << std::endl;
#ifdef OPENSSL_SUPPORT_ENABLED
        m_base_tcpAsyncSenderSslPtr.reset();
#else
        m_base_tcpAsyncSenderPtr.reset();
#endif
    }

#ifdef OPENSSL_SUPPORT_ENABLED
    boost::asio::ip::tcp::socket & socketRef = m_base_sslStreamSharedPtr->next_layer();
    if (m_base_sslStreamSharedPtr) {
#else
    boost::asio::ip::tcp::socket & socketRef = *m_base_tcpSocketPtr;
    if (m_base_tcpSocketPtr) {
#endif
    
        if (socketRef.is_open()) {
            try {
                std::cout << M_BASE_IMPLEMENTATION_STRING_FOR_COUT << " cancelling TCP socket operations.." << std::endl;
                socketRef.cancel();
            }
            catch (const boost::system::system_error & e) {
                std::cerr << M_BASE_IMPLEMENTATION_STRING_FOR_COUT << " error in BaseClass_CloseAndDeleteSockets: " << e.what() << std::endl;
            }
            try {
                std::cout << M_BASE_IMPLEMENTATION_STRING_FOR_COUT << " shutting down TCP socket.." << std::endl;
                socketRef.shutdown(boost::asio::socket_base::shutdown_type::shutdown_both);
            }
            catch (const boost::system::system_error & e) {
                std::cerr << M_BASE_IMPLEMENTATION_STRING_FOR_COUT << " error in BaseClass_CloseAndDeleteSockets: " << e.what() << std::endl;
            }
            try {
                std::cout << M_BASE_IMPLEMENTATION_STRING_FOR_COUT << " closing TCP socket socket.." << std::endl;
                socketRef.close();
            }
            catch (const boost::system::system_error & e) {
                std::cerr << M_BASE_IMPLEMENTATION_STRING_FOR_COUT << " error in BaseClass_CloseAndDeleteSockets: " << e.what() << std::endl;
            }
        }
        if (M_BASE_DELETE_SOCKET_AFTER_SHUTDOWN) { //for bundle sink
            std::cout << M_BASE_IMPLEMENTATION_STRING_FOR_COUT << " deleting TCP Socket" << std::endl;
#ifdef OPENSSL_SUPPORT_ENABLED
            if (m_base_sslStreamSharedPtr.use_count() != 1) {
                std::cerr << "error m_base_sslStreamSharedPtr.use_count() != 1" << std::endl;
            }
            m_base_sslStreamSharedPtr = boost::shared_ptr< boost::asio::ssl::stream<boost::asio::ip::tcp::socket> >();
#else
            if (m_base_tcpSocketPtr.use_count() != 1) {
                std::cerr << "error m_base_tcpSocketPtr.use_count() != 1" << std::endl;
        }
            m_base_tcpSocketPtr = boost::shared_ptr<boost::asio::ip::tcp::socket>();
#endif
        }
        else {
            //don't delete the tcp socket or async sender because the Forward function is multi-threaded without a mutex to
            //increase speed, so prevent a race condition that would cause a null pointer exception
        }
    }
    m_base_needToSendKeepAliveMessageTimer.cancel();
    m_base_noKeepAlivePacketReceivedTimer.cancel();
    m_base_tcpclV4RxStateMachine.InitRx(); //reset states
    m_base_tcpclShutdownComplete = true; //bundlesource
    m_base_sinkIsSafeToDelete = true; //bundlesink
    Virtual_OnTcpclShutdownComplete_CalledFromIoServiceThread();
}

bool TcpclV4BidirectionalLink::BaseClass_Forward(const uint8_t* bundleData, const std::size_t size) {
    std::vector<uint8_t> vec(bundleData, bundleData + size);
    return BaseClass_Forward(vec);
}
bool TcpclV4BidirectionalLink::BaseClass_Forward(std::vector<uint8_t> & dataVec) {
    static std::unique_ptr<zmq::message_t> nullZmqMessagePtr;
    return BaseClass_Forward(nullZmqMessagePtr, dataVec, false);
}
bool TcpclV4BidirectionalLink::BaseClass_Forward(zmq::message_t & dataZmq) {
    static std::vector<uint8_t> unusedVecMessage;
    std::unique_ptr<zmq::message_t> zmqMessageUniquePtr = boost::make_unique<zmq::message_t>(std::move(dataZmq));
    const bool success = BaseClass_Forward(zmqMessageUniquePtr, unusedVecMessage, true);
    if (!success) { //if failure
        //move message back to param
        if (zmqMessageUniquePtr) {
            dataZmq = std::move(*zmqMessageUniquePtr);
        }
    }
    return success;
}
bool TcpclV4BidirectionalLink::BaseClass_Forward(std::unique_ptr<zmq::message_t> & zmqMessageUniquePtr, std::vector<uint8_t> & vecMessage, const bool usingZmqData) {
    
    if (!m_base_readyToForward) {
        std::cerr << "link not ready to forward yet" << std::endl;
        return false;
    }

    std::size_t dataSize;
    const uint8_t * dataToSendPtr;
    if (usingZmqData) { //this is zmq data
        dataSize = zmqMessageUniquePtr->size();
        dataToSendPtr = (const uint8_t *)zmqMessageUniquePtr->data();
    }
    else { //this is std::vector<uint8_t>
        dataSize = vecMessage.size();
        dataToSendPtr = vecMessage.data();
    }

        

    ++m_base_totalBundlesSent;
    m_base_totalBundleBytesSent += dataSize;

        
    std::vector<TcpAsyncSenderElement*> elements;

    /*
    Each of the bundle transfer messages contains a Transfer ID which is
    used to correlate messages (from both sides of a transfer) for each
    bundle.  A Transfer ID does not attempt to address uniqueness of the
    bundle data itself and has no relation to concepts such as bundle
    fragmentation.  Each invocation of TCPCL by the bundle protocol
    agent, requesting transmission of a bundle (fragmentary or
    otherwise), results in the initiation of a single TCPCL transfer.
    Each transfer entails the sending of a sequence of some number of
    XFER_SEGMENT and XFER_ACK messages; all are correlated by the same
    Transfer ID.  The sending entity originates a transfer ID and the
    receiving entity uses that same Transfer ID in acknowledgements.

    Transfer IDs from each entity SHALL be unique within a single TCPCL
    session.  Upon exhaustion of the entire 64-bit Transfer ID space, the
    sending entity SHALL terminate the session with SESS_TERM reason code
    "Resource Exhaustion".  For bidirectional bundle transfers, a TCPCL
    entity SHOULD NOT rely on any relation between Transfer IDs
    originating from each side of the TCPCL session.

    Although there is not a strict requirement for Transfer ID initial
    values or ordering (see Section 8.13), in the absence of any other
    mechanism for generating Transfer IDs an entity SHALL use the
    following algorithm: The initial Transfer ID from each entity is zero
    and subsequent Transfer ID values are incremented from the prior
    Transfer ID value by one.

        The only requirement on Transfer IDs is that they be unique with each
    session from the sending peer only.  The trivial algorithm of the
    first transfer starting at zero and later transfers incrementing by
    one causes absolutely predictable Transfer IDs.  Even when a TCPCL
    session is not TLS secured and there is a on-path attacker causing
    denial of service with XFER_REFUSE messages, it is not possible to
    preemptively refuse a transfer so there is no benefit in having
    unpredictable Transfer IDs within a session.*/
    const uint64_t transferId = m_base_myNextTransferId++;


    if (m_base_remoteMaxRxSegmentSizeBytes && (dataSize > m_base_remoteMaxRxSegmentSizeBytes)) {
        //fragmenting a bundle into multiple tcpcl segments
        elements.reserve((dataSize / m_base_remoteMaxRxSegmentSizeBytes) + 2);
        uint64_t dataIndex = 0;
        while (true) {
            uint64_t bytesToSend = std::min(dataSize - dataIndex, m_base_remoteMaxRxSegmentSizeBytes);
            const bool isStartSegment = (dataIndex == 0);
            const bool isEndSegment = ((bytesToSend + dataIndex) == dataSize);

            TcpAsyncSenderElement * el = new TcpAsyncSenderElement();
            el->m_constBufferVec.resize(2);
            if (usingZmqData) {
                el->m_underlyingData.resize(1);
                if (isEndSegment) {
                    el->m_underlyingDataZmq = std::move(zmqMessageUniquePtr);
                }
            }
            else {
                el->m_underlyingData.resize(1 + isEndSegment);
                if (isEndSegment) {
                    el->m_underlyingData[1] = std::move(vecMessage);
                }
            }
                
            //The Transfer Extension Length and Transfer
            //Extension Item fields SHALL only be present when the 'START' flag
            //is set to 1 on the message.
            //If a transfer occupies exactly one segment (i.e., both START and END
            //flags are 1) the Transfer Length extension SHOULD NOT be present.
            //The extension does not provide any additional information for single-
            //segment transfers.
            if (isStartSegment) {
                TcpclV4::GenerateFragmentedStartDataSegmentWithLengthExtensionHeaderOnly(el->m_underlyingData[0], transferId, bytesToSend, dataSize);
            }
            else {
                TcpclV4::GenerateNonStartDataSegmentHeaderOnly(el->m_underlyingData[0], isEndSegment, transferId, bytesToSend);
            }
            el->m_constBufferVec[0] = boost::asio::buffer(el->m_underlyingData[0]);
            el->m_constBufferVec[1] = boost::asio::buffer(dataToSendPtr + dataIndex, bytesToSend);
            el->m_onSuccessfulSendCallbackByIoServiceThreadPtr = &m_base_handleTcpSendCallback;
            elements.push_back(el);


            dataIndex += bytesToSend;//bytes to ack must be cumulative of fragments

            const unsigned int writeIndex = m_base_segmentsToAckCbPtr->GetIndexForWrite(); //don't put this in tcp async write callback
            if (writeIndex == UINT32_MAX) { //push check
                std::cerr << M_BASE_IMPLEMENTATION_STRING_FOR_COUT << ": Unexpected Error in Base_Forward.. cannot get cb write index" << std::endl;
                return false;
            }
            m_base_segmentsToAckCbVec[writeIndex] = TcpclV4::tcpclv4_ack_t(isStartSegment, isEndSegment, transferId, dataIndex);
            m_base_segmentsToAckCbPtr->CommitWrite(); //pushed

            if (isEndSegment) {
                break;
            }
        }
    }

        

    if (elements.size()) { //is fragmented
        m_base_totalFragmentedSent += elements.size();
        for (std::size_t i = 0; i < elements.size(); ++i) {
#ifdef OPENSSL_SUPPORT_ENABLED
            if (m_base_usingTls) {
                m_base_tcpAsyncSenderSslPtr->AsyncSendSecure_ThreadSafe(elements[i]);
            }
            else {
                m_base_tcpAsyncSenderSslPtr->AsyncSendUnsecure_ThreadSafe(elements[i]);
            }
#else
            m_base_tcpAsyncSenderPtr->AsyncSend_ThreadSafe(elements[i]);
#endif
        }
    }
    else {
        TcpAsyncSenderElement * el = new TcpAsyncSenderElement();
        if (usingZmqData) {
            el->m_underlyingData.resize(1);
            el->m_underlyingDataZmq = std::move(zmqMessageUniquePtr);
        }
        else {
            el->m_underlyingData.resize(2);
            el->m_underlyingData[1] = std::move(vecMessage);
        }
        TcpclV4::GenerateNonFragmentedDataSegmentHeaderOnly(el->m_underlyingData[0], transferId, dataSize);
        el->m_constBufferVec.resize(2);
        el->m_constBufferVec[0] = boost::asio::buffer(el->m_underlyingData[0]);
        el->m_constBufferVec[1] = boost::asio::buffer(dataToSendPtr, dataSize);
        el->m_onSuccessfulSendCallbackByIoServiceThreadPtr = &m_base_handleTcpSendCallback;

        const unsigned int writeIndex = m_base_segmentsToAckCbPtr->GetIndexForWrite(); //don't put this in tcp async write callback
        if (writeIndex == UINT32_MAX) { //push check
            std::cerr << M_BASE_IMPLEMENTATION_STRING_FOR_COUT << ": Unexpected Error in Base_Forward.. cannot get cb write index" << std::endl;
            return false;
        }
        m_base_segmentsToAckCbVec[writeIndex] = TcpclV4::tcpclv4_ack_t(true, true, transferId, dataSize);
        m_base_segmentsToAckCbPtr->CommitWrite(); //pushed

#ifdef OPENSSL_SUPPORT_ENABLED
        if (m_base_usingTls) {
            m_base_tcpAsyncSenderSslPtr->AsyncSendSecure_ThreadSafe(el);
        }
        else {
            m_base_tcpAsyncSenderSslPtr->AsyncSendUnsecure_ThreadSafe(el);
        }
#else
        m_base_tcpAsyncSenderPtr->AsyncSend_ThreadSafe(el);
#endif
    }
    return true;
}

void TcpclV4BidirectionalLink::BaseClass_MessageRejectCallback(TCPCLV4_MESSAGE_REJECT_REASON_CODES refusalCode, uint8_t rejectedMessageHeader) {
    std::cout << M_BASE_IMPLEMENTATION_STRING_FOR_COUT << ": remote rejected message (header=" << (int)rejectedMessageHeader << ") with reason code: ";
    std::cout <<
        ((refusalCode == TCPCLV4_MESSAGE_REJECT_REASON_CODES::MESSAGE_TYPE_UNKNOWN) ? "MESSAGE_TYPE_UNKNOWN" :
        (refusalCode == TCPCLV4_MESSAGE_REJECT_REASON_CODES::MESSAGE_UNEXPECTED) ? "MESSAGE_UNEXPECTED" :
        (refusalCode == TCPCLV4_MESSAGE_REJECT_REASON_CODES::MESSAGE_UNSUPPORTED) ? "MESSAGE_UNSUPPORTED" : "unassigned") << std::endl;
}

void TcpclV4BidirectionalLink::BaseClass_BundleRefusalCallback(TCPCLV4_TRANSFER_REFUSE_REASON_CODES refusalCode, uint64_t transferId) {
    std::cout << M_BASE_IMPLEMENTATION_STRING_FOR_COUT << ": remote refused bundle transfer (transferId=" << transferId << ") with reason code: ";
    std::cout <<
        ((refusalCode == TCPCLV4_TRANSFER_REFUSE_REASON_CODES::REFUSAL_REASON_ALREADY_COMPLETED) ? "ALREADY_COMPLETED" :
        (refusalCode == TCPCLV4_TRANSFER_REFUSE_REASON_CODES::REFUSAL_REASON_EXTENSION_FAILURE) ? "EXTENSION_FAILURE" :
        (refusalCode == TCPCLV4_TRANSFER_REFUSE_REASON_CODES::REFUSAL_REASON_NOT_ACCEPTABLE) ? "NOT_ACCEPTABLE" :
        (refusalCode == TCPCLV4_TRANSFER_REFUSE_REASON_CODES::REFUSAL_REASON_NO_RESOURCES) ? "NO_RESOURCES" :
        (refusalCode == TCPCLV4_TRANSFER_REFUSE_REASON_CODES::REFUSAL_REASON_RETRANSMIT) ? "RETRANSMIT" :
        (refusalCode == TCPCLV4_TRANSFER_REFUSE_REASON_CODES::REFUSAL_REASON_SESSION_TERMINATING) ? "SESSION_TERMINATING" :
        (refusalCode == TCPCLV4_TRANSFER_REFUSE_REASON_CODES::REFUSAL_REASON_UNKNOWN) ? "UNKNOWN" :"unassigned") << std::endl;
}

void TcpclV4BidirectionalLink::BaseClass_ContactHeaderCallback(bool remoteHasEnabledTlsSecurity) {
    //We are the active entity
    std::cout << "rx contact header\n";
    //The first negotiation is on the TCPCL protocol version to use.  The
    //active entity always sends its Contact Header first and waits for a
    //response from the passive entity.  During contact initiation, the
    //active TCPCL entity SHALL send the highest TCPCL protocol version on
    //a first session attempt for a TCPCL peer.  If the active entity
    //receives a Contact Header with a lower protocol version than the one
    //sent earlier on the TCP connection, the TCP connection SHALL be
    //closed.  If the active entity receives a SESS_TERM message with
    //reason of "Version Mismatch", that entity MAY attempt further TCPCL
    //sessions with the peer using earlier protocol version numbers in
    //decreasing order.  Managing multi-TCPCL-session state such as this is
    //an implementation matter.
    //however, the tcpclv4 rx state machine won't let any traffic through unless it's version 4


    //Enable TLS:  Negotiation of the Enable TLS parameter is performed by
    //taking the logical AND of the two Contact Headers' CAN_TLS flags.
    m_base_usingTls = (M_BASE_TRY_USE_TLS && remoteHasEnabledTlsSecurity);
    //A local security policy is then applied to determine of the
    //negotiated value of Enable TLS is acceptable.  It can be a
    //reasonable security policy to require or disallow the use of TLS
    //depending upon the desired network flows.  The RECOMMENDED policy
    //is to require TLS for all sessions, even if security policy does
    //not allow or require authentication.  Because this state is
    //negotiated over an unsecured medium, there is a risk of a TLS
    //Stripping as described in Section 8.4.

    //If the Enable TLS state is unacceptable, the entity SHALL
    //terminate the session with a reason code of "Contact Failure".
    //Note that this contact failure reason is different than a failure
    //of TLS handshake or TLS authentication after an agreed-upon and
    //acceptable Enable TLS state.  If the negotiated Enable TLS value
    //is true and acceptable then TLS negotiation feature (described in
    //Section 4.4) begins immediately following the Contact Header
    //exchange.
    if (M_BASE_TLS_IS_REQUIRED && (!m_base_usingTls)) {
        //contact failure
        std::cerr << M_BASE_IMPLEMENTATION_STRING_FOR_COUT << ": error in BaseClass_ContactHeaderCallback: tls is required by this entity." << std::endl;
        BaseClass_DoTcpclShutdown(true, TCPCLV4_SESSION_TERMINATION_REASON_CODES::CONTACT_FAILURE, false); //will send shutdown with tls disabled
        return;
    }

    m_base_doUpgradeSocketToSsl = m_base_usingTls;
    if (!M_BASE_IS_ACTIVE_ENTITY) {
        //Since TcpclBundleSink was waiting for a contact header, it just got one.  Now it's time to reply with a contact header (contact headers are sent without tls)
        BaseClass_SendContactHeader();
    }
    else {
        //Since TcpclBundleSource sent the first contact header, it just got the reply back from the sink.  Now it's time to reply with a session init
        if (m_base_usingTls) {
            std::cout << "upgrading tcp socket to tls tcp socket..\n";
        }
        else {
            BaseClass_SendSessionInit();
        }
    }
}

void TcpclV4BidirectionalLink::BaseClass_SendSessionInit() {
#ifdef OPENSSL_SUPPORT_ENABLED
    if (m_base_sslStreamSharedPtr) {
#else
    if (m_base_tcpSocketPtr) {
#endif
        TcpAsyncSenderElement * el = new TcpAsyncSenderElement();
        el->m_underlyingData.resize(1);
        TcpclV4::GenerateSessionInitMessage(el->m_underlyingData[0], M_BASE_DESIRED_KEEPALIVE_INTERVAL_SECONDS, M_BASE_MY_MAX_RX_SEGMENT_SIZE_BYTES,
            M_BASE_MY_MAX_RX_BUNDLE_SIZE_BYTES, M_BASE_THIS_TCPCL_EID_STRING, TcpclV4::tcpclv4_extensions_t());
        el->m_constBufferVec.emplace_back(boost::asio::buffer(el->m_underlyingData[0])); //only one element so resize not needed
        el->m_onSuccessfulSendCallbackByIoServiceThreadPtr = &m_base_handleTcpSendCallback;

#ifdef OPENSSL_SUPPORT_ENABLED
        if (m_base_usingTls) {
            m_base_tcpAsyncSenderSslPtr->AsyncSendSecure_NotThreadSafe(el); //OnConnect runs in ioService thread so no thread safety needed
        }
        else {
            m_base_tcpAsyncSenderSslPtr->AsyncSendUnsecure_NotThreadSafe(el); //OnConnect runs in ioService thread so no thread safety needed
        }
#else
        m_base_tcpAsyncSenderPtr->AsyncSend_NotThreadSafe(el); //OnConnect runs in ioService thread so no thread safety needed
#endif
    }
}

void TcpclV4BidirectionalLink::BaseClass_SendContactHeader() {
#ifdef OPENSSL_SUPPORT_ENABLED
    if (m_base_sslStreamSharedPtr) {
#else
    if (m_base_tcpSocketPtr) {
#endif
        TcpAsyncSenderElement * el = new TcpAsyncSenderElement();
        el->m_underlyingData.resize(1);
        TcpclV4::GenerateContactHeader(el->m_underlyingData[0], M_BASE_TRY_USE_TLS);
        el->m_constBufferVec.emplace_back(boost::asio::buffer(el->m_underlyingData[0])); //only one element so resize not needed
        el->m_onSuccessfulSendCallbackByIoServiceThreadPtr = &m_base_handleTcpSendContactHeaderCallback;
#ifdef OPENSSL_SUPPORT_ENABLED
        m_base_tcpAsyncSenderSslPtr->AsyncSendUnsecure_ThreadSafe(el);
#else
        m_base_tcpAsyncSenderPtr->AsyncSend_ThreadSafe(el);
#endif
    }
}

void TcpclV4BidirectionalLink::BaseClass_SessionInitCallback(uint16_t keepAliveIntervalSeconds, uint64_t segmentMru, uint64_t transferMru,
    const std::string & remoteNodeEidUri, const TcpclV4::tcpclv4_extensions_t & sessionExtensions)
{
    std::cout << "rx session init\n";
#ifdef OPENSSL_SUPPORT_ENABLED
    if (m_base_didSuccessfulSslHandshake) {
        const std::string sslVersionString(SSL_get_version(m_base_sslStreamSharedPtr->native_handle()));
        std::cout << "tcpclv4 using secure protocol: " << sslVersionString << std::endl;
    }
    else {
        std::cout << "notice: TLS is disabled\n";
    }
#endif
    uint64_t remoteNodeId = UINT64_MAX;
    uint64_t remoteServiceId = UINT64_MAX;
    if (!Uri::ParseIpnUriString(remoteNodeEidUri, remoteNodeId, remoteServiceId)) {
        std::cerr << M_BASE_IMPLEMENTATION_STRING_FOR_COUT << ": error in BaseClass_SessionInitCallback: error parsing remote EID URI string " << remoteNodeEidUri
            << " .. TCPCL will not receive bundles." << std::endl;
        BaseClass_DoTcpclShutdown(true, TCPCLV4_SESSION_TERMINATION_REASON_CODES::CONTACT_FAILURE, false);
        return;
    }
    else if (remoteServiceId != 0) {
        //ion 3.7.2 source code tcpcli.c line 1199 uses service number 0 for contact header:
        //isprintf(eid, sizeof eid, "ipn:" UVAST_FIELDSPEC ".0", getOwnNodeNbr());
        std::cerr << M_BASE_IMPLEMENTATION_STRING_FOR_COUT << ": error in BaseClass_SessionInitCallback: remote EID URI string " << remoteNodeEidUri
            << " does not use service number 0.. TCPCL will not receive bundles." << std::endl;
        BaseClass_DoTcpclShutdown(true, TCPCLV4_SESSION_TERMINATION_REASON_CODES::CONTACT_FAILURE, false);
        return;
    }
    else if ((!M_BASE_EXPECTED_REMOTE_CONTACT_HEADER_EID_STRING_IF_NOT_EMPTY.empty()) && (remoteNodeEidUri != M_BASE_EXPECTED_REMOTE_CONTACT_HEADER_EID_STRING_IF_NOT_EMPTY)) {
        std::cout << M_BASE_IMPLEMENTATION_STRING_FOR_COUT << ": error in BaseClass_SessionInitCallback: received wrong remote node eid "
            << remoteNodeEidUri << " but expected " << M_BASE_EXPECTED_REMOTE_CONTACT_HEADER_EID_STRING_IF_NOT_EMPTY
            << " .. TCPCL will not forward bundles.  Correct the \"nextHopEndpointId\" field in the outducts config." << std::endl;
        BaseClass_DoTcpclShutdown(true, TCPCLV4_SESSION_TERMINATION_REASON_CODES::CONTACT_FAILURE, false);
        return;
    }

    m_base_tcpclRemoteEidString = remoteNodeEidUri;
    m_base_tcpclRemoteNodeId = remoteNodeId;
    std::cout << M_BASE_IMPLEMENTATION_STRING_FOR_COUT << " received valid SessionInit from remote with EID " << m_base_tcpclRemoteEidString << std::endl;

    m_base_remoteMaxRxSegmentSizeBytes = segmentMru;
    m_base_remoteMaxRxBundleSizeBytes = transferMru;
    if (m_base_remoteMaxRxSegmentSizeBytes == 0) {
        std::cout << M_BASE_IMPLEMENTATION_STRING_FOR_COUT << ": error in BaseClass_SessionInitCallback: remote has 0 for segment MRU\n";
        //DoTcpclShutdown(false, false);
        return;
    }
    if (m_base_remoteMaxRxSegmentSizeBytes > m_base_remoteMaxRxBundleSizeBytes) {
        std::cout << M_BASE_IMPLEMENTATION_STRING_FOR_COUT << ": notice in BaseClass_SessionInitCallback: remote has segment MRU (" << m_base_remoteMaxRxSegmentSizeBytes 
            << ") greater than transfer/bundle MRU( " << m_base_remoteMaxRxBundleSizeBytes << ").. reducing segment mru to transfer mru\n";
        m_base_remoteMaxRxSegmentSizeBytes = m_base_remoteMaxRxBundleSizeBytes;
    }

    m_base_remoteMaxRxSegmentsPerBundle = ((m_base_remoteMaxRxBundleSizeBytes / m_base_remoteMaxRxSegmentSizeBytes) + 1);
    m_base_maxUnackedSegments = (m_base_remoteMaxRxSegmentsPerBundle * M_BASE_MY_MAX_TX_UNACKED_BUNDLES);
    m_base_ackCbSize = m_base_maxUnackedSegments + 10;
    m_base_segmentsToAckCbPtr = boost::make_unique<CircularIndexBufferSingleProducerSingleConsumerConfigurable>(m_base_ackCbSize);
    m_base_segmentsToAckCbVec.resize(m_base_ackCbSize);

    if (sessionExtensions.extensionsVec.size()) {
        std::cout << M_BASE_IMPLEMENTATION_STRING_FOR_COUT << ": received " << sessionExtensions.extensionsVec.size() << " session extensions\n";
        for (std::size_t i = 0; i < sessionExtensions.extensionsVec.size(); ++i) {
            const TcpclV4::tcpclv4_extension_t & ext = sessionExtensions.extensionsVec[i];
            //If a TCPCL entity receives a
            //Session Extension Item with an unknown Item Type and the CRITICAL
            //flag of 1, the entity SHALL terminate the TCPCL session with
            //SESS_TERM reason code of "Contact Failure".  If the CRITICAL flag
            //is 0, an entity SHALL skip over and ignore any item with an
            //unknown Item Type.
            if (ext.IsCritical()) {
                std::cout << M_BASE_IMPLEMENTATION_STRING_FOR_COUT << ": error in BaseClass_SessionInitCallback: received unknown critical session extension type " << ext.type
                    << " ..terminating session.\n";
            }
            else {
                std::cout << M_BASE_IMPLEMENTATION_STRING_FOR_COUT << ": notice: BaseClass_SessionInitCallback: ignoring unknown non-critical session extension type " << ext.type << std::endl;
            }
        }
    }


    //Session Keepalive:  Negotiation of the Session Keepalive parameter is
    //performed by taking the minimum of the two Keepalive Interval
    //values from the two SESS_INIT messages.  The Session Keepalive
    //interval is a parameter for the behavior described in
    //Section 5.1.1.  If the Session Keepalive interval is unacceptable,
    //the entity SHALL terminate the session with a reason code of
    //"Contact Failure".  Note: a negotiated Session Keepalive of zero
    //indicates that KEEPALIVEs are disabled.
    if (M_BASE_DESIRED_KEEPALIVE_INTERVAL_SECONDS == 0) {
        std::cout << M_BASE_IMPLEMENTATION_STRING_FOR_COUT << " notice: we have disabled the keepalive feature\n";
        m_base_keepAliveIntervalSeconds = 0;
    }
    else if (keepAliveIntervalSeconds == 0) {
        std::cout << M_BASE_IMPLEMENTATION_STRING_FOR_COUT << " notice: remote host has disabled the keepalive feature\n";
        m_base_keepAliveIntervalSeconds = 0;
    }
    else if (M_BASE_DESIRED_KEEPALIVE_INTERVAL_SECONDS > keepAliveIntervalSeconds) {
        std::cout << M_BASE_IMPLEMENTATION_STRING_FOR_COUT << " notice: remote host has requested a smaller keepalive interval of " << keepAliveIntervalSeconds
            << " seconds than ours of " << M_BASE_DESIRED_KEEPALIVE_INTERVAL_SECONDS << "." << std::endl;
        m_base_keepAliveIntervalSeconds = keepAliveIntervalSeconds; //use the remote's smaller one
    }
    else if (M_BASE_DESIRED_KEEPALIVE_INTERVAL_SECONDS < keepAliveIntervalSeconds) { //bundle source should never enter here as the response should be smaller
        std::cout << M_BASE_IMPLEMENTATION_STRING_FOR_COUT << " notice: we have requested a smaller keepalive interval of " << M_BASE_DESIRED_KEEPALIVE_INTERVAL_SECONDS
            << " seconds than the remote host's of " << keepAliveIntervalSeconds << "." << std::endl;
        m_base_keepAliveIntervalSeconds = M_BASE_DESIRED_KEEPALIVE_INTERVAL_SECONDS; //use our smaller one
    }
    else {
        m_base_keepAliveIntervalSeconds = M_BASE_DESIRED_KEEPALIVE_INTERVAL_SECONDS; //use
    }

    if (!M_BASE_IS_ACTIVE_ENTITY) {
        //Since TcpclBundleSink was waiting for a session init, it just got one.  Now it's time to reply with a session init
        //use the negotiated keepalive interval
        BaseClass_SendSessionInit();
    }

    if (m_base_keepAliveIntervalSeconds) { //non-zero
        std::cout << M_BASE_IMPLEMENTATION_STRING_FOR_COUT << " using " << m_base_keepAliveIntervalSeconds << " seconds for keepalive\n";

        // * 2 =>
        //If no message (KEEPALIVE or other) has been received for at least
        //twice the keepalive_interval, then either party MAY terminate the
        //session by transmitting a one-byte SHUTDOWN message (as described in
        //Table 2) and by closing the TCP connection.
        m_base_noKeepAlivePacketReceivedTimer.expires_from_now(boost::posix_time::seconds(m_base_keepAliveIntervalSeconds * 2));
        m_base_noKeepAlivePacketReceivedTimer.async_wait(boost::bind(&TcpclV4BidirectionalLink::BaseClass_OnNoKeepAlivePacketReceived_TimerExpired, this, boost::asio::placeholders::error));


        m_base_needToSendKeepAliveMessageTimer.expires_from_now(boost::posix_time::seconds(m_base_keepAliveIntervalSeconds));
        m_base_needToSendKeepAliveMessageTimer.async_wait(boost::bind(&TcpclV4BidirectionalLink::BaseClass_OnNeedToSendKeepAliveMessage_TimerExpired, this, boost::asio::placeholders::error));
    }

    m_base_readyToForward = true;
    Virtual_OnSessionInitReceivedAndProcessedSuccessfully();
}

void TcpclV4BidirectionalLink::Virtual_OnSessionInitReceivedAndProcessedSuccessfully() {}
