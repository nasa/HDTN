/**
 * @file TcpclV3BidirectionalLink.cpp
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
 */

#include <string>
#include <iostream>
#include "TcpclV3BidirectionalLink.h"
#include <boost/lexical_cast.hpp>
#include <memory>
#include <boost/make_unique.hpp>
#include "Uri.h"
#include <boost/endian/conversion.hpp>

TcpclV3BidirectionalLink::TcpclV3BidirectionalLink(
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
    ) :
    M_BASE_IMPLEMENTATION_STRING_FOR_COUT(implementationStringForCout),
    M_BASE_SHUTDOWN_MESSAGE_RECONNECTION_DELAY_SECONDS_TO_SEND(shutdownMessageReconnectionDelaySecondsToSend),
    M_BASE_DESIRED_KEEPALIVE_INTERVAL_SECONDS(desiredKeepAliveIntervalSeconds),
    M_BASE_DELETE_SOCKET_AFTER_SHUTDOWN(deleteSocketAfterShutdown),
    M_BASE_CONTACT_HEADER_MUST_REPLY(contactHeaderMustReply),
    //ion 3.7.2 source code tcpcli.c line 1199 uses service number 0 for contact header:
    //isprintf(eid, sizeof eid, "ipn:" UVAST_FIELDSPEC ".0", getOwnNodeNbr());
    M_BASE_THIS_TCPCL_EID_STRING(Uri::GetIpnUriString(myNodeId, 0)),
    m_base_keepAliveIntervalSeconds(desiredKeepAliveIntervalSeconds),
    m_base_localIoServiceUniquePtr((externalIoServicePtr == NULL) ? boost::make_unique<boost::asio::io_service>() : std::unique_ptr<boost::asio::io_service>()),
    m_base_ioServiceRef((externalIoServicePtr == NULL) ? (*m_base_localIoServiceUniquePtr) : (*externalIoServicePtr)),
    m_base_noKeepAlivePacketReceivedTimer(m_base_ioServiceRef),
    m_base_needToSendKeepAliveMessageTimer(m_base_ioServiceRef),
    m_base_sendShutdownMessageTimeoutTimer(m_base_ioServiceRef),

    m_base_shutdownCalled(false),
    m_base_readyToForward(false), //bundleSource
    m_base_sinkIsSafeToDelete(false), //bundleSink
    m_base_tcpclShutdownComplete(true), //bundleSource
    m_base_useLocalConditionVariableAckReceived(false), //for bundleSource destructor only
    m_base_dataReceivedServedAsKeepaliveReceived(false),
    m_base_dataSentServedAsKeepaliveSent(false),
    m_base_reconnectionDelaySecondsIfNotZero(3), //bundle source only, default 3 unless remote says 0 in shutdown message

    M_BASE_MAX_UNACKED_BUNDLES_IN_PIPELINE(maxUnacked), //bundle sink has MAX_UNACKED(maxUnacked + 5),
    M_BASE_UNACKED_BUNDLE_CB_SIZE(maxUnacked + 5),
    m_base_bytesToAckCb(M_BASE_UNACKED_BUNDLE_CB_SIZE),
    m_base_bytesToAckCbVec(M_BASE_UNACKED_BUNDLE_CB_SIZE),
    m_base_fragmentBytesToAckCbVec(M_BASE_UNACKED_BUNDLE_CB_SIZE),
    m_base_fragmentVectorIndexCbVec(M_BASE_UNACKED_BUNDLE_CB_SIZE),
    M_BASE_MAX_FRAGMENT_SIZE(maxFragmentSize),

    m_base_userAssignedUuid(0),

    //stats
    m_base_totalBundlesAcked(0),
    m_base_totalBytesAcked(0),
    m_base_totalBundlesSent(0),
    m_base_totalFragmentedAcked(0),
    m_base_totalFragmentedSent(0),
    m_base_totalBundleBytesSent(0)
{
    m_base_tcpclV3RxStateMachine.SetMaxReceiveBundleSizeBytes(maxBundleSizeBytes);

    m_base_tcpclV3RxStateMachine.SetContactHeaderReadCallback(boost::bind(&TcpclV3BidirectionalLink::BaseClass_ContactHeaderCallback, this, boost::placeholders::_1, boost::placeholders::_2, boost::placeholders::_3));
    m_base_tcpclV3RxStateMachine.SetDataSegmentContentsReadCallback(boost::bind(&TcpclV3BidirectionalLink::BaseClass_DataSegmentCallback, this, boost::placeholders::_1, boost::placeholders::_2, boost::placeholders::_3));
    m_base_tcpclV3RxStateMachine.SetAckSegmentReadCallback(boost::bind(&TcpclV3BidirectionalLink::BaseClass_AckCallback, this, boost::placeholders::_1));
    m_base_tcpclV3RxStateMachine.SetBundleRefusalCallback(boost::bind(&TcpclV3BidirectionalLink::BaseClass_BundleRefusalCallback, this, boost::placeholders::_1));
    m_base_tcpclV3RxStateMachine.SetNextBundleLengthCallback(boost::bind(&TcpclV3BidirectionalLink::BaseClass_NextBundleLengthCallback, this, boost::placeholders::_1));
    m_base_tcpclV3RxStateMachine.SetKeepAliveCallback(boost::bind(&TcpclV3BidirectionalLink::BaseClass_KeepAliveCallback, this));
    m_base_tcpclV3RxStateMachine.SetShutdownMessageCallback(boost::bind(&TcpclV3BidirectionalLink::BaseClass_ShutdownCallback, this, boost::placeholders::_1, boost::placeholders::_2, boost::placeholders::_3, boost::placeholders::_4));


    m_base_handleTcpSendCallback = boost::bind(&TcpclV3BidirectionalLink::BaseClass_HandleTcpSend, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred);
    m_base_handleTcpSendShutdownCallback = boost::bind(&TcpclV3BidirectionalLink::BaseClass_HandleTcpSendShutdown, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred);

    for (unsigned int i = 0; i < M_BASE_UNACKED_BUNDLE_CB_SIZE; ++i) {
        m_base_fragmentBytesToAckCbVec[i].reserve(100);
    }

    if (expectedRemoteEidUriStringIfNotEmpty.empty()) {
        M_BASE_EXPECTED_REMOTE_CONTACT_HEADER_EID_STRING_IF_NOT_EMPTY = "";
    }
    else {
        uint64_t remoteNodeId, remoteServiceId;
        if (!Uri::ParseIpnUriString(expectedRemoteEidUriStringIfNotEmpty, remoteNodeId, remoteServiceId)) {
            std::cerr << M_BASE_IMPLEMENTATION_STRING_FOR_COUT << ": error in constructor: error parsing remote EID URI string " << expectedRemoteEidUriStringIfNotEmpty
                << " .. TCPCL will fail the Contact Header Callback.  Correct the \"nextHopEndpointId\" field in the outducts config." << std::endl;
        }
        else {
            //ion 3.7.2 source code tcpcli.c line 1199 uses service number 0 for contact header:
            //isprintf(eid, sizeof eid, "ipn:" UVAST_FIELDSPEC ".0", getOwnNodeNbr());
            M_BASE_EXPECTED_REMOTE_CONTACT_HEADER_EID_STRING_IF_NOT_EMPTY = Uri::GetIpnUriString(remoteNodeId, 0);
        }
    }
}

TcpclV3BidirectionalLink::~TcpclV3BidirectionalLink() {

}

void TcpclV3BidirectionalLink::BaseClass_TryToWaitForAllBundlesToFinishSending() {
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

std::size_t TcpclV3BidirectionalLink::Virtual_GetTotalBundlesAcked() {
    return m_base_totalBundlesAcked;
}

std::size_t TcpclV3BidirectionalLink::Virtual_GetTotalBundlesSent() {
    return m_base_totalBundlesSent;
}

std::size_t TcpclV3BidirectionalLink::Virtual_GetTotalBundlesUnacked() {
    return m_base_totalBundlesSent - m_base_totalBundlesAcked;
}

std::size_t TcpclV3BidirectionalLink::Virtual_GetTotalBundleBytesAcked() {
    return m_base_totalBytesAcked;
}

std::size_t TcpclV3BidirectionalLink::Virtual_GetTotalBundleBytesSent() {
    return m_base_totalBundleBytesSent;
}

std::size_t TcpclV3BidirectionalLink::Virtual_GetTotalBundleBytesUnacked() {
    return m_base_totalBundleBytesSent - m_base_totalBytesAcked;
}

unsigned int TcpclV3BidirectionalLink::Virtual_GetMaxTxBundlesInPipeline() {
    return M_BASE_MAX_UNACKED_BUNDLES_IN_PIPELINE;
}

void TcpclV3BidirectionalLink::BaseClass_HandleTcpSend(const boost::system::error_code& error, std::size_t bytes_transferred) {
    if (error) {
        std::cerr << M_BASE_IMPLEMENTATION_STRING_FOR_COUT << ": error in BaseClass_HandleTcpSend: " << error.message() << std::endl;
        BaseClass_DoTcpclShutdown(true, false);
    }
    else {
        Virtual_OnTcpSendSuccessful_CalledFromIoServiceThread();
    }
}

void TcpclV3BidirectionalLink::BaseClass_HandleTcpSendShutdown(const boost::system::error_code& error, std::size_t bytes_transferred) {
    if (error) {
        std::cerr << M_BASE_IMPLEMENTATION_STRING_FOR_COUT << ": error in BaseClass_HandleTcpSendShutdown: " << error.message() << std::endl;
    }
    else {
        m_base_sendShutdownMessageTimeoutTimer.cancel();
    }
}

void TcpclV3BidirectionalLink::Virtual_OnTcpSendSuccessful_CalledFromIoServiceThread() {}

void TcpclV3BidirectionalLink::BaseClass_DataSegmentCallback(padded_vector_uint8_t & dataSegmentDataVec, bool isStartFlag, bool isEndFlag) {

    uint64_t bytesToAck = 0;
    if (isStartFlag && isEndFlag) { //optimization for whole (non-fragmented) data
        bytesToAck = dataSegmentDataVec.size(); //grab the size now in case vector gets stolen in m_wholeBundleReadyCallback
        Virtual_WholeBundleReady(dataSegmentDataVec);
        //std::cout << dataSegmentDataSharedPtr->size() << std::endl;
    }
    else {
        if (isStartFlag) {
            m_base_fragmentedBundleRxConcat.resize(0);
        }
        m_base_fragmentedBundleRxConcat.insert(m_base_fragmentedBundleRxConcat.end(), dataSegmentDataVec.begin(), dataSegmentDataVec.end()); //concatenate
        bytesToAck = m_base_fragmentedBundleRxConcat.size();
        if (isEndFlag) { //fragmentation complete
            Virtual_WholeBundleReady(m_base_fragmentedBundleRxConcat);
        }
    }
    //send ack
    if ((static_cast<unsigned int>(CONTACT_HEADER_FLAGS::REQUEST_ACK_OF_BUNDLE_SEGMENTS)) & (static_cast<unsigned int>(m_base_contactHeaderFlags))) {
        if (m_base_tcpSocketPtr) {
            TcpAsyncSenderElement * el = new TcpAsyncSenderElement();
            el->m_underlyingDataVecHeaders.resize(1);
            Tcpcl::GenerateAckSegment(el->m_underlyingDataVecHeaders[0], bytesToAck);
            el->m_constBufferVec.emplace_back(boost::asio::buffer(el->m_underlyingDataVecHeaders[0])); //only one element so resize not needed
            el->m_onSuccessfulSendCallbackByIoServiceThreadPtr = &m_base_handleTcpSendCallback;
            m_base_dataSentServedAsKeepaliveSent = true; //sending acks can also be used in lieu of keepalives
            m_base_tcpAsyncSenderPtr->AsyncSend_ThreadSafe(el);
        }
    }
}




void TcpclV3BidirectionalLink::BaseClass_AckCallback(uint64_t totalBytesAcknowledged) {
    const unsigned int readIndex = m_base_bytesToAckCb.GetIndexForRead();
    if (readIndex == CIRCULAR_INDEX_BUFFER_EMPTY) { //empty
        std::cerr << M_BASE_IMPLEMENTATION_STRING_FOR_COUT << ": error: AckCallback called with empty queue" << std::endl;
    }
    else {
        std::vector<uint64_t> & currentFragmentBytesVec = m_base_fragmentBytesToAckCbVec[readIndex];
        if (currentFragmentBytesVec.size()) { //this was fragmented
            uint64_t & fragIdxRef = m_base_fragmentVectorIndexCbVec[readIndex];
            const uint64_t expectedFragByteToAck = currentFragmentBytesVec[fragIdxRef++];
            if (fragIdxRef == currentFragmentBytesVec.size()) {
                currentFragmentBytesVec.resize(0);
            }
            if (expectedFragByteToAck == totalBytesAcknowledged) {
                ++m_base_totalFragmentedAcked;
            }
            else {
                std::cerr << M_BASE_IMPLEMENTATION_STRING_FOR_COUT << ": error in BaseClass_AckCallback: wrong fragment bytes acked: expected "
                    << expectedFragByteToAck << " but got " << totalBytesAcknowledged << std::endl;
            }
        }

        //now ack the entire bundle
        if (currentFragmentBytesVec.empty()) {
            if (m_base_bytesToAckCbVec[readIndex] == totalBytesAcknowledged) {
                ++m_base_totalBundlesAcked;
                m_base_totalBytesAcked += m_base_bytesToAckCbVec[readIndex];
                m_base_bytesToAckCb.CommitRead();
                Virtual_OnSuccessfulWholeBundleAcknowledged();
                if (m_base_useLocalConditionVariableAckReceived) {
                    m_base_localConditionVariableAckReceived.notify_one();
                }
            }
            else {
                std::cerr << M_BASE_IMPLEMENTATION_STRING_FOR_COUT << ": error in BaseClass_AckCallback: wrong bytes acked: expected "
                    << m_base_bytesToAckCbVec[readIndex] << " but got " << totalBytesAcknowledged << std::endl;
            }
        }
    }
}

void TcpclV3BidirectionalLink::BaseClass_RestartNoKeepaliveReceivedTimer() {
    // m_base_keepAliveIntervalSeconds * 2.5 seconds =>
    //If no message (KEEPALIVE or other) has been received for at least
    //twice the keepalive_interval, then either party MAY terminate the
    //session by transmitting a one-byte SHUTDOWN message (as described in
    //Table 2) and by closing the TCP connection.
    m_base_noKeepAlivePacketReceivedTimer.expires_from_now(boost::posix_time::milliseconds(m_base_keepAliveIntervalSeconds * 2500)); //cancels active timer with cancel flag in callback
    m_base_noKeepAlivePacketReceivedTimer.async_wait(boost::bind(&TcpclV3BidirectionalLink::BaseClass_OnNoKeepAlivePacketReceived_TimerExpired, this, boost::asio::placeholders::error));
    m_base_dataReceivedServedAsKeepaliveReceived = false;
}

void TcpclV3BidirectionalLink::BaseClass_KeepAliveCallback() {
    std::cout << M_BASE_IMPLEMENTATION_STRING_FOR_COUT << ": received keepalive packet\n";
    BaseClass_RestartNoKeepaliveReceivedTimer(); //cancels and restarts timer
}

void TcpclV3BidirectionalLink::BaseClass_OnNoKeepAlivePacketReceived_TimerExpired(const boost::system::error_code& e) {
    if (e != boost::asio::error::operation_aborted) {
        // Timer was not cancelled, take necessary action.
        if (m_base_dataReceivedServedAsKeepaliveReceived) {
            std::cout << M_BASE_IMPLEMENTATION_STRING_FOR_COUT << ": data received served as keepalive\n";
            BaseClass_RestartNoKeepaliveReceivedTimer();
        }
        else {
            std::cout << M_BASE_IMPLEMENTATION_STRING_FOR_COUT << ": shutting down tcpcl session due to inactivity or missing keepalive\n";
            BaseClass_DoTcpclShutdown(true, true);
        }
    }
    else {
        //std::cout << "timer cancelled\n";
    }
}

void TcpclV3BidirectionalLink::BaseClass_RestartNeedToSendKeepAliveMessageTimer() {
    //Shift right 1 (divide by 2) if we are omitting sending a keepalive, so for example,
    //if our keep alive interval is 10 seconds, but we are omitting sending a keepalive due to a Forward() function call,
    //then evaluate if sending a keepalive is necessary 5 seconds from now.
    //But if there is no data being sent from us, do not omit keepalives every 10 seconds. 
    const unsigned int shift = static_cast<unsigned int>(m_base_dataSentServedAsKeepaliveSent);
    const unsigned int millisecondMultiplier = 1000u >> shift;
    const boost::posix_time::time_duration expiresFromNowDuration = boost::posix_time::milliseconds(m_base_keepAliveIntervalSeconds * millisecondMultiplier);
    //std::cout << "try send keepalive in " << expiresFromNowDuration.total_milliseconds() << " millisec\n";
    m_base_needToSendKeepAliveMessageTimer.expires_from_now(expiresFromNowDuration);
    m_base_needToSendKeepAliveMessageTimer.async_wait(boost::bind(&TcpclV3BidirectionalLink::BaseClass_OnNeedToSendKeepAliveMessage_TimerExpired, this, boost::asio::placeholders::error));
    m_base_dataSentServedAsKeepaliveSent = false;
}

void TcpclV3BidirectionalLink::BaseClass_OnNeedToSendKeepAliveMessage_TimerExpired(const boost::system::error_code& e) {
    if (e != boost::asio::error::operation_aborted) {
        // Timer was not cancelled, take necessary action.
        //SEND KEEPALIVE PACKET
        if (m_base_tcpSocketPtr) {
            if (!m_base_dataSentServedAsKeepaliveSent) {
                TcpAsyncSenderElement * el = new TcpAsyncSenderElement();
                el->m_underlyingDataVecHeaders.resize(1);
                Tcpcl::GenerateKeepAliveMessage(el->m_underlyingDataVecHeaders[0]);
                el->m_constBufferVec.emplace_back(boost::asio::buffer(el->m_underlyingDataVecHeaders[0])); //only one element so resize not needed
                el->m_onSuccessfulSendCallbackByIoServiceThreadPtr = &m_base_handleTcpSendCallback;
                m_base_tcpAsyncSenderPtr->AsyncSend_NotThreadSafe(el); //timer runs in same thread as socket so special thread safety not needed
            }
            BaseClass_RestartNeedToSendKeepAliveMessageTimer();
        }
    }
    else {
        //std::cout << "timer cancelled\n";
    }
}


void TcpclV3BidirectionalLink::BaseClass_ShutdownCallback(bool hasReasonCode, SHUTDOWN_REASON_CODES shutdownReasonCode,
    bool hasReconnectionDelay, uint64_t reconnectionDelaySeconds)
{
    std::cout << M_BASE_IMPLEMENTATION_STRING_FOR_COUT << ": remote has requested shutdown\n";
    if (hasReasonCode) {
        std::cout << M_BASE_IMPLEMENTATION_STRING_FOR_COUT << " reason provided by remote for shutdown: "
            << ((shutdownReasonCode == SHUTDOWN_REASON_CODES::BUSY) ? "busy" :
            (shutdownReasonCode == SHUTDOWN_REASON_CODES::IDLE_TIMEOUT) ? "idle timeout" :
                (shutdownReasonCode == SHUTDOWN_REASON_CODES::VERSION_MISMATCH) ? "version mismatch" : "unassigned") << std::endl;
    }
    if (hasReconnectionDelay) {
        m_base_reconnectionDelaySecondsIfNotZero = reconnectionDelaySeconds; //for bundle source only
        std::cout << M_BASE_IMPLEMENTATION_STRING_FOR_COUT << ": remote has requested reconnection delay of " << reconnectionDelaySeconds << " seconds" << std::endl;
    }
    BaseClass_DoTcpclShutdown(false, false);
}

void TcpclV3BidirectionalLink::BaseClass_DoTcpclShutdown(bool sendShutdownMessage, bool reasonWasTimeOut) {
    boost::asio::post(m_base_ioServiceRef, boost::bind(&TcpclV3BidirectionalLink::BaseClass_DoHandleSocketShutdown, this, sendShutdownMessage, reasonWasTimeOut));
}

void TcpclV3BidirectionalLink::BaseClass_DoHandleSocketShutdown(bool sendShutdownMessage, bool reasonWasTimeOut) {
    if (!m_base_shutdownCalled) {
        m_base_shutdownCalled = true;
        // Called from post() to keep socket shutdown within io_service thread.

        m_base_readyToForward = false;
        //if (!m_base_sinkIsSafeToDelete) { this is unnecessary if statement
        if (sendShutdownMessage && m_base_tcpAsyncSenderPtr && m_base_tcpSocketPtr) {
            std::cout << M_BASE_IMPLEMENTATION_STRING_FOR_COUT << " Sending shutdown packet to cleanly close tcpcl.. " << std::endl;
            TcpAsyncSenderElement * el = new TcpAsyncSenderElement();
            el->m_underlyingDataVecHeaders.resize(1);
            //For the requested delay, in seconds, the value 0 SHALL be interpreted as an infinite delay,
            //i.e., that the connecting node MUST NOT re - establish the connection.
            if (reasonWasTimeOut) {
                Tcpcl::GenerateShutdownMessage(el->m_underlyingDataVecHeaders[0], true, SHUTDOWN_REASON_CODES::IDLE_TIMEOUT, true, M_BASE_SHUTDOWN_MESSAGE_RECONNECTION_DELAY_SECONDS_TO_SEND);
            }
            else {
                Tcpcl::GenerateShutdownMessage(el->m_underlyingDataVecHeaders[0], false, SHUTDOWN_REASON_CODES::UNASSIGNED, true, M_BASE_SHUTDOWN_MESSAGE_RECONNECTION_DELAY_SECONDS_TO_SEND);
            }

            el->m_constBufferVec.emplace_back(boost::asio::buffer(el->m_underlyingDataVecHeaders[0])); //only one element so resize not needed
            el->m_onSuccessfulSendCallbackByIoServiceThreadPtr = &m_base_handleTcpSendShutdownCallback;
            m_base_tcpAsyncSenderPtr->AsyncSend_NotThreadSafe(el); //HandleSocketShutdown runs in same thread as socket so special thread safety not needed

            m_base_sendShutdownMessageTimeoutTimer.expires_from_now(boost::posix_time::seconds(3));
        }
        else {
            m_base_sendShutdownMessageTimeoutTimer.expires_from_now(boost::posix_time::seconds(0));
        }
        m_base_sendShutdownMessageTimeoutTimer.async_wait(boost::bind(&TcpclV3BidirectionalLink::BaseClass_OnSendShutdownMessageTimeout_TimerExpired, this, boost::asio::placeholders::error));
        
    }
}

void TcpclV3BidirectionalLink::BaseClass_OnSendShutdownMessageTimeout_TimerExpired(const boost::system::error_code& e) {
    if (e != boost::asio::error::operation_aborted) {
        // Timer was not cancelled, take necessary action.
        std::cout << M_BASE_IMPLEMENTATION_STRING_FOR_COUT << " Notice: No TCPCL shutdown message was sent (not required)." << std::endl;
    }
    else {
        //std::cout << "timer cancelled\n";
        std::cout << M_BASE_IMPLEMENTATION_STRING_FOR_COUT << " TCPCL shutdown message was sent." << std::endl;
    }

    //final code to shut down tcp sockets
    if (M_BASE_DELETE_SOCKET_AFTER_SHUTDOWN) { //for bundle sink
        std::cout << M_BASE_IMPLEMENTATION_STRING_FOR_COUT << " deleting TCP Async Sender" << std::endl;
        m_base_tcpAsyncSenderPtr.reset();
    }
    if (m_base_tcpSocketPtr) {
        if (m_base_tcpSocketPtr->is_open()) {
            try {
                std::cout << M_BASE_IMPLEMENTATION_STRING_FOR_COUT << " shutting down TCP socket.." << std::endl;
                m_base_tcpSocketPtr->shutdown(boost::asio::socket_base::shutdown_type::shutdown_both);
            }
            catch (const boost::system::system_error & e) {
                std::cerr << M_BASE_IMPLEMENTATION_STRING_FOR_COUT << " error in BaseClass_OnSendShutdownMessageTimeout_TimerExpired: " << e.what() << std::endl;
            }
            try {
                std::cout << M_BASE_IMPLEMENTATION_STRING_FOR_COUT << " closing TCP socket socket.." << std::endl;
                m_base_tcpSocketPtr->close();
            }
            catch (const boost::system::system_error & e) {
                std::cerr << M_BASE_IMPLEMENTATION_STRING_FOR_COUT << " error in BaseClass_OnSendShutdownMessageTimeout_TimerExpired: " << e.what() << std::endl;
            }
        }
        if (M_BASE_DELETE_SOCKET_AFTER_SHUTDOWN) { //for bundle sink
            std::cout << M_BASE_IMPLEMENTATION_STRING_FOR_COUT << " deleting TCP Socket" << std::endl;
            if (m_base_tcpSocketPtr.use_count() != 1) {
                std::cerr << "error m_base_tcpSocketPtr.use_count() != 1" << std::endl;
            }
            m_base_tcpSocketPtr = std::shared_ptr<boost::asio::ip::tcp::socket>();
        }
        else {
            //don't delete the tcp socket or async sender because the Forward function is multi-threaded without a mutex to
            //increase speed, so prevent a race condition that would cause a null pointer exception
        }
    }
    m_base_needToSendKeepAliveMessageTimer.cancel();
    m_base_noKeepAlivePacketReceivedTimer.cancel();
    m_base_tcpclV3RxStateMachine.InitRx(); //reset states
    m_base_tcpclShutdownComplete = true;
    m_base_sinkIsSafeToDelete = true;
    Virtual_OnTcpclShutdownComplete_CalledFromIoServiceThread();
    
}

bool TcpclV3BidirectionalLink::BaseClass_Forward(const uint8_t* bundleData, const std::size_t size) {
    std::vector<uint8_t> vec(bundleData, bundleData + size);
    return BaseClass_Forward(vec);
}
bool TcpclV3BidirectionalLink::BaseClass_Forward(std::vector<uint8_t> & dataVec) {
    static std::unique_ptr<zmq::message_t> nullZmqMessagePtr;
    return BaseClass_Forward(nullZmqMessagePtr, dataVec, false);
}
bool TcpclV3BidirectionalLink::BaseClass_Forward(zmq::message_t & dataZmq) {
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
bool TcpclV3BidirectionalLink::BaseClass_Forward(std::unique_ptr<zmq::message_t> & zmqMessageUniquePtr, std::vector<uint8_t> & vecMessage, const bool usingZmqData) {

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

    


    const unsigned int writeIndex = m_base_bytesToAckCb.GetIndexForWrite(); //don't put this in tcp async write callback
    if (writeIndex == CIRCULAR_INDEX_BUFFER_FULL) { //push check
        std::cerr << M_BASE_IMPLEMENTATION_STRING_FOR_COUT << ": Error in BaseClass_Forward.. too many unacked packets" << std::endl;
        return false;
    }
    m_base_dataSentServedAsKeepaliveSent = true;
    m_base_bytesToAckCbVec[writeIndex] = dataSize;


    ++m_base_totalBundlesSent;
    m_base_totalBundleBytesSent += dataSize;

    std::vector<uint64_t> & currentFragmentBytesVec = m_base_fragmentBytesToAckCbVec[writeIndex];
    currentFragmentBytesVec.resize(0); //will be zero size if not fragmented
    m_base_fragmentVectorIndexCbVec[writeIndex] = 0; //used by the ack callback
    std::vector<TcpAsyncSenderElement*> elements;

    if (M_BASE_MAX_FRAGMENT_SIZE && (dataSize > M_BASE_MAX_FRAGMENT_SIZE)) {
        const uint64_t reservedSize = (dataSize / M_BASE_MAX_FRAGMENT_SIZE) + 2;
        elements.reserve(reservedSize);
        currentFragmentBytesVec.reserve(reservedSize);
        uint64_t dataIndex = 0;
        while (true) {
            uint64_t bytesToSend = std::min(dataSize - dataIndex, M_BASE_MAX_FRAGMENT_SIZE);
            const bool isStartSegment = (dataIndex == 0);
            const bool isEndSegment = ((bytesToSend + dataIndex) == dataSize);

            TcpAsyncSenderElement * el = new TcpAsyncSenderElement();
            el->m_underlyingDataVecHeaders.resize(1);
            if (usingZmqData) {
                //el->m_underlyingDataVecHeaders.resize(1);
                if (isEndSegment) {
                    el->m_underlyingDataZmqBundle = std::move(zmqMessageUniquePtr);
                }
            }
            else {
                //el->m_underlyingDataVecHeaders.resize(1 + isEndSegment);
                if (isEndSegment) {
                    el->m_underlyingDataVecBundle = std::move(vecMessage);
                }
            }
            Tcpcl::GenerateDataSegmentHeaderOnly(el->m_underlyingDataVecHeaders[0], isStartSegment, isEndSegment, bytesToSend);
            el->m_constBufferVec.resize(2);
            el->m_constBufferVec[0] = boost::asio::buffer(el->m_underlyingDataVecHeaders[0]);
            el->m_constBufferVec[1] = boost::asio::buffer(dataToSendPtr + dataIndex, bytesToSend);
            el->m_onSuccessfulSendCallbackByIoServiceThreadPtr = &m_base_handleTcpSendCallback;
            elements.push_back(el);


            dataIndex += bytesToSend;
            currentFragmentBytesVec.push_back(dataIndex); //bytes to ack must be cumulative of fragments

            if (isEndSegment) {
                break;
            }
        }
    }

    m_base_bytesToAckCb.CommitWrite(); //pushed

    //send length if requested
    //LENGTH messages MUST NOT be sent unless the corresponding flag bit is
    //set in the contact header.  If the flag bit is set, LENGTH messages
    //MAY be sent at the sender's discretion.  LENGTH messages MUST NOT be
    //sent unless the next DATA_SEGMENT message has the 'S' bit set to "1"
    //(i.e., just before the start of a new bundle).

    //TODO:
    //A receiver MAY send a BUNDLE_REFUSE message as soon as it receives a
    //LENGTH message without waiting for the next DATA_SEGMENT message.
    //The sender MUST be prepared for this and MUST associate the refusal
    //with the right bundle.
    if ((static_cast<unsigned int>(CONTACT_HEADER_FLAGS::REQUEST_SENDING_OF_LENGTH_MESSAGES)) & (static_cast<unsigned int>(m_base_contactHeaderFlags))) {
        TcpAsyncSenderElement * el = new TcpAsyncSenderElement();
        el->m_underlyingDataVecHeaders.resize(1);
        Tcpcl::GenerateBundleLength(el->m_underlyingDataVecHeaders[0], dataSize);
        el->m_constBufferVec.emplace_back(boost::asio::buffer(el->m_underlyingDataVecHeaders[0])); //only one element so resize not needed
        el->m_onSuccessfulSendCallbackByIoServiceThreadPtr = &m_base_handleTcpSendCallback;
        m_base_tcpAsyncSenderPtr->AsyncSend_ThreadSafe(el);
    }

    if (elements.size()) { //is fragmented
        m_base_totalFragmentedSent += elements.size();
        for (std::size_t i = 0; i < elements.size(); ++i) {
            m_base_tcpAsyncSenderPtr->AsyncSend_ThreadSafe(elements[i]);
        }
    }
    else {
        TcpAsyncSenderElement * el = new TcpAsyncSenderElement();
        el->m_underlyingDataVecHeaders.resize(1);
        if (usingZmqData) {
            el->m_underlyingDataZmqBundle = std::move(zmqMessageUniquePtr);
        }
        else {
            el->m_underlyingDataVecBundle = std::move(vecMessage);
        }
        Tcpcl::GenerateDataSegmentHeaderOnly(el->m_underlyingDataVecHeaders[0], true, true, dataSize);
        el->m_constBufferVec.resize(2);
        el->m_constBufferVec[0] = boost::asio::buffer(el->m_underlyingDataVecHeaders[0]);
        el->m_constBufferVec[1] = boost::asio::buffer(dataToSendPtr, dataSize);
        el->m_onSuccessfulSendCallbackByIoServiceThreadPtr = &m_base_handleTcpSendCallback;
        m_base_tcpAsyncSenderPtr->AsyncSend_ThreadSafe(el);
    }

    return true;
    
}

void TcpclV3BidirectionalLink::BaseClass_BundleRefusalCallback(BUNDLE_REFUSAL_CODES refusalCode) {
    std::cout << M_BASE_IMPLEMENTATION_STRING_FOR_COUT << ": error: BundleRefusalCallback not implemented yet" << std::endl;
}

void TcpclV3BidirectionalLink::BaseClass_NextBundleLengthCallback(uint64_t nextBundleLength) {
    if (nextBundleLength > m_base_tcpclV3RxStateMachine.GetMaxReceiveBundleSizeBytes()) {
        std::cout << M_BASE_IMPLEMENTATION_STRING_FOR_COUT << ": error: BaseClass_NextBundleLengthCallback received next bundle length of " << nextBundleLength
            << " which is greater than the bundle receive limit of " << m_base_tcpclV3RxStateMachine.GetMaxReceiveBundleSizeBytes()  << " bytes" << std::endl;
        //todo refuse bundle message?
    }
    else {
        std::cout << "next bundle length\n";
        m_base_fragmentedBundleRxConcat.reserve(nextBundleLength); //in case the bundle is fragmented
    }
}

void TcpclV3BidirectionalLink::BaseClass_ContactHeaderCallback(CONTACT_HEADER_FLAGS flags, uint16_t keepAliveIntervalSeconds, const std::string & localEid) {
    
    uint64_t remoteNodeId = UINT64_MAX;
    uint64_t remoteServiceId = UINT64_MAX;
    if (!Uri::ParseIpnUriString(localEid, remoteNodeId, remoteServiceId)) {
        std::cerr << M_BASE_IMPLEMENTATION_STRING_FOR_COUT << ": error in BaseClass_ProcessContactHeader: error parsing remote EID URI string " << localEid
            << " .. TCPCL will not receive bundles." << std::endl;
        BaseClass_DoTcpclShutdown(false, false);
        return;
    }
    else if (remoteServiceId != 0) {
        //ion 3.7.2 source code tcpcli.c line 1199 uses service number 0 for contact header:
        //isprintf(eid, sizeof eid, "ipn:" UVAST_FIELDSPEC ".0", getOwnNodeNbr());
        std::cerr << M_BASE_IMPLEMENTATION_STRING_FOR_COUT << ": error in BaseClass_ProcessContactHeader: remote EID URI string " << localEid
            << " does not use service number 0.. TCPCL will not receive bundles." << std::endl;
        BaseClass_DoTcpclShutdown(false, false);
        return;
    }
    else if ((!M_BASE_EXPECTED_REMOTE_CONTACT_HEADER_EID_STRING_IF_NOT_EMPTY.empty()) && (localEid != M_BASE_EXPECTED_REMOTE_CONTACT_HEADER_EID_STRING_IF_NOT_EMPTY)) {
        std::cout << M_BASE_IMPLEMENTATION_STRING_FOR_COUT << ": error in BaseClass_ProcessContactHeader: received wrong contact header back from "
            << localEid << " but expected " << M_BASE_EXPECTED_REMOTE_CONTACT_HEADER_EID_STRING_IF_NOT_EMPTY
            << " .. TCPCL will not forward bundles.  Correct the \"nextHopEndpointId\" field in the outducts config." << std::endl;
        BaseClass_DoTcpclShutdown(false, false);
        return;
    }

    m_base_tcpclRemoteEidString = localEid;
    m_base_tcpclRemoteNodeId = remoteNodeId;
    std::cout << M_BASE_IMPLEMENTATION_STRING_FOR_COUT << " received valid contact header from remote with EID " << m_base_tcpclRemoteEidString << std::endl;

    m_base_contactHeaderFlags = flags;

    //The keepalive_interval parameter is set to the minimum value from
    //both contact headers.  If one or both contact headers contains the
    //value zero, then the keepalive feature (described in Section 5.6)
    //is disabled.
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

    if (M_BASE_CONTACT_HEADER_MUST_REPLY) {
        //Since TcpclBundleSink was waiting for a contact header, it just got one.  Now it's time to reply with a contact header
        //use the same keepalive interval
        if (m_base_tcpSocketPtr) {
            TcpAsyncSenderElement * el = new TcpAsyncSenderElement();
            el->m_underlyingDataVecHeaders.resize(1);
            Tcpcl::GenerateContactHeader(el->m_underlyingDataVecHeaders[0], CONTACT_HEADER_FLAGS::REQUEST_ACK_OF_BUNDLE_SEGMENTS, m_base_keepAliveIntervalSeconds, M_BASE_THIS_TCPCL_EID_STRING);
            el->m_constBufferVec.emplace_back(boost::asio::buffer(el->m_underlyingDataVecHeaders[0])); //only one element so resize not needed
            el->m_onSuccessfulSendCallbackByIoServiceThreadPtr = &m_base_handleTcpSendCallback;
            m_base_tcpAsyncSenderPtr->AsyncSend_ThreadSafe(el);
        }
    }
    
    if (m_base_keepAliveIntervalSeconds) { //non-zero
        std::cout << M_BASE_IMPLEMENTATION_STRING_FOR_COUT << " using " << m_base_keepAliveIntervalSeconds << " seconds for keepalive\n";

        BaseClass_RestartNoKeepaliveReceivedTimer();
        BaseClass_RestartNeedToSendKeepAliveMessageTimer();
    }

    m_base_readyToForward = true;
    Virtual_OnContactHeaderCompletedSuccessfully();
}

void TcpclV3BidirectionalLink::Virtual_OnContactHeaderCompletedSuccessfully() {}

void TcpclV3BidirectionalLink::BaseClass_SetOnFailedBundleVecSendCallback(const OnFailedBundleVecSendCallback_t& callback) {
    m_base_onFailedBundleVecSendCallback = callback;
}
void TcpclV3BidirectionalLink::BaseClass_SetOnFailedBundleZmqSendCallback(const OnFailedBundleZmqSendCallback_t& callback) {
    m_base_onFailedBundleZmqSendCallback = callback;
}
void TcpclV3BidirectionalLink::BaseClass_SetUserAssignedUuid(uint64_t userAssignedUuid) {
    m_base_userAssignedUuid = userAssignedUuid;
}
