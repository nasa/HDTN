#include <boost/bind/bind.hpp>
#include <boost/make_shared.hpp>
#include <iostream>
#include "TcpclBundleSink.h"
#include <boost/make_unique.hpp>
#include "Uri.h"

TcpclBundleSink::TcpclBundleSink(boost::shared_ptr<boost::asio::ip::tcp::socket> & tcpSocketPtr,
    boost::asio::io_service & tcpSocketIoServiceRef,
    const WholeBundleReadyCallback_t & wholeBundleReadyCallback,
    //ConnectionClosedCallback_t connectionClosedCallback,
    const unsigned int numCircularBufferVectors,
    const unsigned int circularBufferBytesPerVector,
    const uint64_t myNodeId,
    const uint64_t maxBundleSizeBytes,
    const NotifyReadyToDeleteCallback_t & notifyReadyToDeleteCallback,
    const OnContactHeaderCallback_t & onContactHeaderCallback,
    //const TryGetOpportunisticDataFunction_t & tryGetOpportunisticDataFunction,
    //const NotifyOpportunisticDataAckedCallback_t & notifyOpportunisticDataAckedCallback,
    const unsigned int maxUnacked, const uint64_t maxFragmentSize) :

    //ion 3.7.2 source code tcpcli.c line 1199 uses service number 0 for contact header:
    //isprintf(eid, sizeof eid, "ipn:" UVAST_FIELDSPEC ".0", getOwnNodeNbr());
    M_THIS_EID(Uri::GetIpnUriString(myNodeId, 0)),

    m_wholeBundleReadyCallback(wholeBundleReadyCallback),
    m_notifyReadyToDeleteCallback(notifyReadyToDeleteCallback),
    m_onContactHeaderCallback(onContactHeaderCallback),
    m_tcpSocketPtr(tcpSocketPtr),
    m_tcpSocketIoServiceRef(tcpSocketIoServiceRef),
    m_noKeepAlivePacketReceivedTimer(tcpSocketIoServiceRef),
    m_needToSendKeepAliveMessageTimer(tcpSocketIoServiceRef),
    m_sendShutdownMessageTimeoutTimer(tcpSocketIoServiceRef),
    M_NUM_CIRCULAR_BUFFER_VECTORS(numCircularBufferVectors),
    M_CIRCULAR_BUFFER_BYTES_PER_VECTOR(circularBufferBytesPerVector),
    m_circularIndexBuffer(M_NUM_CIRCULAR_BUFFER_VECTORS),
    m_tcpReceiveBuffersCbVec(M_NUM_CIRCULAR_BUFFER_VECTORS),
    m_tcpReceiveBytesTransferredCbVec(M_NUM_CIRCULAR_BUFFER_VECTORS),
    m_stateTcpReadActive(false),
    m_printedCbTooSmallNotice(false),
    m_running(false),
    m_safeToDelete(false),
    m_shutdownCalled(false),
    MAX_UNACKED(maxUnacked + 5),
    m_bytesToAckCb(MAX_UNACKED),
    m_bytesToAckCbVec(MAX_UNACKED),
    m_fragmentBytesToAckCbVec(MAX_UNACKED),
    m_fragmentVectorIndexCbVec(MAX_UNACKED),
    M_MAX_FRAGMENT_SIZE(maxFragmentSize),
    m_totalDataSegmentsAcked(0),
    m_totalBytesAcked(0),
    m_totalDataSegmentsSent(0),
    m_totalFragmentedAcked(0),
    m_totalFragmentedSent(0),
    m_totalBundleBytesSent(0)
{
    m_tcpcl.SetContactHeaderReadCallback(boost::bind(&TcpclBundleSink::ContactHeaderCallback, this, boost::placeholders::_1, boost::placeholders::_2, boost::placeholders::_3));
    m_tcpcl.SetDataSegmentContentsReadCallback(boost::bind(&TcpclBundleSink::DataSegmentCallback, this, boost::placeholders::_1, boost::placeholders::_2, boost::placeholders::_3));
    m_tcpcl.SetAckSegmentReadCallback(boost::bind(&TcpclBundleSink::AckCallback, this, boost::placeholders::_1));
    m_tcpcl.SetBundleRefusalCallback(boost::bind(&TcpclBundleSink::BundleRefusalCallback, this, boost::placeholders::_1));
    m_tcpcl.SetNextBundleLengthCallback(boost::bind(&TcpclBundleSink::NextBundleLengthCallback, this, boost::placeholders::_1));
    m_tcpcl.SetKeepAliveCallback(boost::bind(&TcpclBundleSink::KeepAliveCallback, this));
    m_tcpcl.SetShutdownMessageCallback(boost::bind(&TcpclBundleSink::ShutdownCallback, this, boost::placeholders::_1, boost::placeholders::_2, boost::placeholders::_3, boost::placeholders::_4));
    m_tcpcl.SetMaxReceiveBundleSizeBytes(maxBundleSizeBytes);

    for (unsigned int i = 0; i < M_NUM_CIRCULAR_BUFFER_VECTORS; ++i) {
        m_tcpReceiveBuffersCbVec[i].resize(M_CIRCULAR_BUFFER_BYTES_PER_VECTOR);
    }

    m_tcpAsyncSenderPtr = boost::make_unique<TcpAsyncSender>(m_tcpSocketPtr, m_tcpSocketIoServiceRef);
    m_handleTcpSendCallback = boost::bind(&TcpclBundleSink::HandleTcpSend, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred);
    m_handleTcpSendShutdownCallback = boost::bind(&TcpclBundleSink::HandleTcpSendShutdown, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred);

    m_running = true;
    m_threadCbReaderPtr = boost::make_unique<boost::thread>(
        boost::bind(&TcpclBundleSink::PopCbThreadFunc, this)); //create and start the worker thread

    TryStartTcpReceive();
}

TcpclBundleSink::~TcpclBundleSink() {
    if (!m_safeToDelete) {
        DoTcpclShutdown(true, false);
        while (!m_safeToDelete) {
            boost::this_thread::sleep(boost::posix_time::milliseconds(250));
        }
    }

    m_running = false; //thread stopping criteria

    if (m_threadCbReaderPtr) {
        m_threadCbReaderPtr->join();
        m_threadCbReaderPtr.reset(); //delete it
    }
    m_tcpAsyncSenderPtr.reset();
}




void TcpclBundleSink::TryStartTcpReceive() {
    if ((!m_stateTcpReadActive) && (m_tcpSocketPtr)) {
        const unsigned int writeIndex = m_circularIndexBuffer.GetIndexForWrite(); //store the volatile
        if (writeIndex == UINT32_MAX) {
            if (!m_printedCbTooSmallNotice) {
                m_printedCbTooSmallNotice = true;
                std::cout << "notice in TcpclBundleSink::StartTcpReceive(): buffers full.. you might want to increase the circular buffer size for better performance!" << std::endl;
            }
        }
        else {
            m_stateTcpReadActive = true;
            m_tcpSocketPtr->async_read_some(
                boost::asio::buffer(m_tcpReceiveBuffersCbVec[writeIndex]),
                boost::bind(&TcpclBundleSink::HandleTcpReceiveSome, this,
                    boost::asio::placeholders::error,
                    boost::asio::placeholders::bytes_transferred,
                    writeIndex));
        }
    }
}
void TcpclBundleSink::HandleTcpReceiveSome(const boost::system::error_code & error, std::size_t bytesTransferred, unsigned int writeIndex) {
    if (!error) {
        m_tcpReceiveBytesTransferredCbVec[writeIndex] = bytesTransferred;
        m_circularIndexBuffer.CommitWrite(); //write complete at this point
        m_stateTcpReadActive = false; //must be false before calling TryStartTcpReceive
        m_conditionVariableCb.notify_one();
        TryStartTcpReceive(); //restart operation only if there was no error
    }
    else if (error == boost::asio::error::eof) {
        std::cout << "Tcp connection closed cleanly by peer" << std::endl;
        DoTcpclShutdown(false, false);
    }
    else if (error != boost::asio::error::operation_aborted) {
        std::cerr << "Error in BpSinkAsync::HandleTcpReceiveSome: " << error.message() << std::endl;
    }
}

void TcpclBundleSink::PopCbThreadFunc() {

    boost::mutex localMutex;
    boost::mutex::scoped_lock lock(localMutex);

    while (m_running || (m_circularIndexBuffer.GetIndexForRead() != UINT32_MAX)) { //keep thread alive if running or cb not empty


        const unsigned int consumeIndex = m_circularIndexBuffer.GetIndexForRead(); //store the volatile
        boost::asio::post(m_tcpSocketIoServiceRef, boost::bind(&TcpclBundleSink::TryStartTcpReceive, this)); //keep this a thread safe operation by letting ioService thread run it
        if (consumeIndex == UINT32_MAX) { //if empty
            m_conditionVariableCb.timed_wait(lock, boost::posix_time::milliseconds(10)); // call lock.unlock() and blocks the current thread
            //thread is now unblocked, and the lock is reacquired by invoking lock.lock()
            continue;
        }

        m_tcpcl.HandleReceivedChars(m_tcpReceiveBuffersCbVec[consumeIndex].data(), m_tcpReceiveBytesTransferredCbVec[consumeIndex]);

        m_circularIndexBuffer.CommitRead();
    }

    std::cout << "TcpclBundleSink Circular buffer reader thread exiting\n";

}

void TcpclBundleSink::HandleTcpSend(const boost::system::error_code& error, std::size_t bytes_transferred) {
    if(error) {
        std::cerr << "error in TcpclBundleSink::HandleTcpSend: " << error.message() << std::endl;
        DoTcpclShutdown(true, false);
    }
    else {
        TrySendOpportunisticBundleIfAvailable_FromIoServiceThread();
    }
}

void TcpclBundleSink::HandleTcpSendShutdown(const boost::system::error_code& error, std::size_t bytes_transferred) {
    if (error) {
        std::cerr << "error in TcpclBundleSink::HandleTcpSendShutdown: " << error.message() << std::endl;
    }
    else {
        m_sendShutdownMessageTimeoutTimer.cancel();
    }
}

void TcpclBundleSink::ContactHeaderCallback(CONTACT_HEADER_FLAGS flags, uint16_t keepAliveIntervalSeconds, const std::string & localEid) {
    uint64_t remoteNodeId = UINT64_MAX;
    uint64_t remoteServiceId = UINT64_MAX;
    if (!Uri::ParseIpnUriString(localEid, remoteNodeId, remoteServiceId)) {
        std::cerr << "error in TcpclBundleSink::ContactHeaderCallback: error parsing remote EID URI string " << localEid
            << " .. TCPCL will not receive bundles." << std::endl;
        DoTcpclShutdown(false, false);
        return;
    }
    else if (remoteServiceId != 0) {
        //ion 3.7.2 source code tcpcli.c line 1199 uses service number 0 for contact header:
        //isprintf(eid, sizeof eid, "ipn:" UVAST_FIELDSPEC ".0", getOwnNodeNbr());
        std::cerr << "error in TcpclBundleSink::ContactHeaderCallback: remote EID URI string " << localEid
            << " does not use service number 0.. TCPCL will not receive bundles." << std::endl;
        DoTcpclShutdown(false, false);
        return;
    }
    m_contactHeaderFlags = flags;
    //The keepalive_interval parameter is set to the minimum value from
    //both contact headers.  If one or both contact headers contains the
    //value zero, then the keepalive feature (described in Section 5.6)
    //is disabled.
    m_keepAliveIntervalSeconds = keepAliveIntervalSeconds;
    m_remoteEid = localEid;
    m_remoteNodeId = remoteNodeId;
    std::cout << "received valid contact header from " << m_remoteEid << std::endl;

    //Since TcpclBundleSink was waiting for a contact header, it just got one.  Now it's time to reply with a contact header
    //use the same keepalive interval
    if(m_tcpSocketPtr) {
        TcpAsyncSenderElement * el = new TcpAsyncSenderElement();
        el->m_underlyingData.resize(1);
        Tcpcl::GenerateContactHeader(el->m_underlyingData[0], static_cast<CONTACT_HEADER_FLAGS>(0), keepAliveIntervalSeconds, M_THIS_EID);
        el->m_constBufferVec.emplace_back(boost::asio::buffer(el->m_underlyingData[0])); //only one element so resize not needed
        el->m_onSuccessfulSendCallbackByIoServiceThreadPtr = &m_handleTcpSendCallback;
        m_tcpAsyncSenderPtr->AsyncSend_ThreadSafe(el);

        if(m_keepAliveIntervalSeconds) { //non-zero
            std::cout << "using " << keepAliveIntervalSeconds << " seconds for keepalive\n";

            // * 2 =>
            //If no message (KEEPALIVE or other) has been received for at least
            //twice the keepalive_interval, then either party MAY terminate the
            //session by transmitting a one-byte SHUTDOWN message (as described in
            //Table 2) and by closing the TCP connection.
            m_noKeepAlivePacketReceivedTimer.expires_from_now(boost::posix_time::seconds(m_keepAliveIntervalSeconds * 2));
            m_noKeepAlivePacketReceivedTimer.async_wait(boost::bind(&TcpclBundleSink::OnNoKeepAlivePacketReceived_TimerExpired, this, boost::asio::placeholders::error));


            m_needToSendKeepAliveMessageTimer.expires_from_now(boost::posix_time::seconds(m_keepAliveIntervalSeconds));
            m_needToSendKeepAliveMessageTimer.async_wait(boost::bind(&TcpclBundleSink::OnNeedToSendKeepAliveMessage_TimerExpired, this, boost::asio::placeholders::error));
        }
    }
    if (m_onContactHeaderCallback) {
        m_onContactHeaderCallback(this);
    }
}

void TcpclBundleSink::DataSegmentCallback(std::vector<uint8_t> & dataSegmentDataVec, bool isStartFlag, bool isEndFlag) {

    uint64_t bytesToAck = 0;
    if(isStartFlag && isEndFlag) { //optimization for whole (non-fragmented) data
        bytesToAck = dataSegmentDataVec.size(); //grab the size now in case vector gets stolen in m_wholeBundleReadyCallback
        m_wholeBundleReadyCallback(dataSegmentDataVec);
        //std::cout << dataSegmentDataSharedPtr->size() << std::endl;
    }
    else {
        if (isStartFlag) {
            m_fragmentedBundleRxConcat.resize(0);
        }
        m_fragmentedBundleRxConcat.insert(m_fragmentedBundleRxConcat.end(), dataSegmentDataVec.begin(), dataSegmentDataVec.end()); //concatenate
        bytesToAck = m_fragmentedBundleRxConcat.size();
        if(isEndFlag) { //fragmentation complete
            m_wholeBundleReadyCallback(m_fragmentedBundleRxConcat);
        }
    }
    //send ack
    if((static_cast<unsigned int>(CONTACT_HEADER_FLAGS::REQUEST_ACK_OF_BUNDLE_SEGMENTS)) & (static_cast<unsigned int>(m_contactHeaderFlags))) {
        if(m_tcpSocketPtr) {
            TcpAsyncSenderElement * el = new TcpAsyncSenderElement();
            el->m_underlyingData.resize(1);
            Tcpcl::GenerateAckSegment(el->m_underlyingData[0], bytesToAck);
            el->m_constBufferVec.emplace_back(boost::asio::buffer(el->m_underlyingData[0])); //only one element so resize not needed
            el->m_onSuccessfulSendCallbackByIoServiceThreadPtr = &m_handleTcpSendCallback;
            m_tcpAsyncSenderPtr->AsyncSend_ThreadSafe(el);
        }
    }
}

void TcpclBundleSink::AckCallback(uint64_t totalBytesAcknowledged) {
    const unsigned int readIndex = m_bytesToAckCb.GetIndexForRead();
    if (readIndex == UINT32_MAX) { //empty
        std::cerr << "error: AckCallback called with empty queue" << std::endl;
    }
    else {
        std::vector<uint64_t> & currentFragmentBytesVec = m_fragmentBytesToAckCbVec[readIndex];
        if (currentFragmentBytesVec.size()) { //this was fragmented
            uint64_t & fragIdxRef = m_fragmentVectorIndexCbVec[readIndex];
            const uint64_t expectedFragByteToAck = currentFragmentBytesVec[fragIdxRef++];
            if (fragIdxRef == currentFragmentBytesVec.size()) {
                currentFragmentBytesVec.resize(0);
            }
            if (expectedFragByteToAck == totalBytesAcknowledged) {
                ++m_totalFragmentedAcked;
            }
            else {
                std::cerr << "error in TcpclBundleSource::AckCallback: wrong fragment bytes acked: expected " << expectedFragByteToAck << " but got " << totalBytesAcknowledged << std::endl;
            }
        }

        //now ack the entire bundle
        if (currentFragmentBytesVec.empty()) {
            if (m_bytesToAckCbVec[readIndex] == totalBytesAcknowledged) {
                ++m_totalDataSegmentsAcked;
                m_totalBytesAcked += m_bytesToAckCbVec[readIndex];
                m_bytesToAckCb.CommitRead();
                if (m_notifyOpportunisticDataAckedCallback) {
                    m_notifyOpportunisticDataAckedCallback();
                }
            }
            else {
                std::cerr << "error in TcpclBundleSink::AckCallback: wrong bytes acked: expected " << m_bytesToAckCbVec[readIndex] << " but got " << totalBytesAcknowledged << std::endl;
            }
        }
    }
}

void TcpclBundleSink::BundleRefusalCallback(BUNDLE_REFUSAL_CODES refusalCode) {
    std::cout << "TcpclBundleSink should never enter BundleRefusalCallback" << std::endl;
}

void TcpclBundleSink::NextBundleLengthCallback(uint64_t nextBundleLength) {
    std::cout << "next bundle length: " << nextBundleLength << std::endl;
}

void TcpclBundleSink::KeepAliveCallback() {
    std::cout << "In TcpclBundleSink::KeepAliveCallback, received keepalive packet" << std::endl;
    // * 2 =>
    //If no message (KEEPALIVE or other) has been received for at least
    //twice the keepalive_interval, then either party MAY terminate the
    //session by transmitting a one-byte SHUTDOWN message (as described in
    //Table 2) and by closing the TCP connection.
    m_noKeepAlivePacketReceivedTimer.expires_from_now(boost::posix_time::seconds(m_keepAliveIntervalSeconds * 2)); //cancels active timer with cancel flag in callback
    m_noKeepAlivePacketReceivedTimer.async_wait(boost::bind(&TcpclBundleSink::OnNoKeepAlivePacketReceived_TimerExpired, this, boost::asio::placeholders::error));
}

void TcpclBundleSink::ShutdownCallback(bool hasReasonCode, SHUTDOWN_REASON_CODES shutdownReasonCode,
                                         bool hasReconnectionDelay, uint64_t reconnectionDelaySeconds)
{
    std::cout << "remote has requested shutdown\n";
    if(hasReasonCode) {
        std::cout << "reason for shutdown: "
                  << ((shutdownReasonCode == SHUTDOWN_REASON_CODES::BUSY) ? "busy" :
                     (shutdownReasonCode == SHUTDOWN_REASON_CODES::IDLE_TIMEOUT) ? "idle timeout" :
                     (shutdownReasonCode == SHUTDOWN_REASON_CODES::VERSION_MISMATCH) ? "version mismatch" :  "unassigned")   << std::endl;
    }
    if(hasReconnectionDelay) {
        std::cout << "requested reconnection delay: " << reconnectionDelaySeconds << " seconds" << std::endl;
    }
    DoTcpclShutdown(false, false);
}

void TcpclBundleSink::OnNoKeepAlivePacketReceived_TimerExpired(const boost::system::error_code& e) {
    if (e != boost::asio::error::operation_aborted) {
        // Timer was not cancelled, take necessary action.
        DoTcpclShutdown(true, true);
    }
    else {
        //std::cout << "timer cancelled\n";
    }
}

void TcpclBundleSink::OnNeedToSendKeepAliveMessage_TimerExpired(const boost::system::error_code& e) {
    if (e != boost::asio::error::operation_aborted) {
        // Timer was not cancelled, take necessary action.
        //SEND KEEPALIVE PACKET
        if (m_tcpSocketPtr) {
            TcpAsyncSenderElement * el = new TcpAsyncSenderElement();
            el->m_underlyingData.resize(1);
            Tcpcl::GenerateKeepAliveMessage(el->m_underlyingData[0]);
            el->m_constBufferVec.emplace_back(boost::asio::buffer(el->m_underlyingData[0])); //only one element so resize not needed
            el->m_onSuccessfulSendCallbackByIoServiceThreadPtr = &m_handleTcpSendCallback;
            m_tcpAsyncSenderPtr->AsyncSend_NotThreadSafe(el); //timer runs in same thread as socket so special thread safety not needed


            m_needToSendKeepAliveMessageTimer.expires_from_now(boost::posix_time::seconds(m_keepAliveIntervalSeconds));
            m_needToSendKeepAliveMessageTimer.async_wait(boost::bind(&TcpclBundleSink::OnNeedToSendKeepAliveMessage_TimerExpired, this, boost::asio::placeholders::error));
        }
    }
    else {
        //std::cout << "timer cancelled\n";
    }
}

void TcpclBundleSink::DoTcpclShutdown(bool sendShutdownMessage, bool reasonWasTimeOut) {
    boost::asio::post(m_tcpSocketIoServiceRef, boost::bind(&TcpclBundleSink::HandleSocketShutdown, this, sendShutdownMessage, reasonWasTimeOut));
}

void TcpclBundleSink::HandleSocketShutdown(bool sendShutdownMessage, bool reasonWasTimeOut) {
    m_shutdownCalled = true;
    if (!m_safeToDelete) {
        if (sendShutdownMessage) {
            std::cout << "Sending shutdown packet to cleanly close tcpcl.. " << std::endl;
            TcpAsyncSenderElement * el = new TcpAsyncSenderElement();
            el->m_underlyingData.resize(1);
            //For the requested delay, in seconds, the value 0 SHALL be interpreted as an infinite delay,
            //i.e., that the connecting node MUST NOT re - establish the connection.
            if (reasonWasTimeOut) {
                Tcpcl::GenerateShutdownMessage(el->m_underlyingData[0], true, SHUTDOWN_REASON_CODES::IDLE_TIMEOUT, true, 3); //don't want to 0 to disable hdtn outduct reconnect, use 3 seconds
            }
            else {
                Tcpcl::GenerateShutdownMessage(el->m_underlyingData[0], false, SHUTDOWN_REASON_CODES::UNASSIGNED, true, 3); //don't want to 0 to disable hdtn outduct reconnect, use 3 seconds
            }

            el->m_constBufferVec.emplace_back(boost::asio::buffer(el->m_underlyingData[0])); //only one element so resize not needed
            el->m_onSuccessfulSendCallbackByIoServiceThreadPtr = &m_handleTcpSendShutdownCallback;
            m_tcpAsyncSenderPtr->AsyncSend_NotThreadSafe(el); //HandleSocketShutdown runs in same thread as socket so special thread safety not needed

            m_sendShutdownMessageTimeoutTimer.expires_from_now(boost::posix_time::seconds(3));
        }
        else {
            m_sendShutdownMessageTimeoutTimer.expires_from_now(boost::posix_time::seconds(0));
        }
        m_sendShutdownMessageTimeoutTimer.async_wait(boost::bind(&TcpclBundleSink::OnSendShutdownMessageTimeout_TimerExpired, this, boost::asio::placeholders::error));
    }
   
}

void TcpclBundleSink::OnSendShutdownMessageTimeout_TimerExpired(const boost::system::error_code& e) {
    if (e != boost::asio::error::operation_aborted) {
        // Timer was not cancelled, take necessary action.
        std::cout << "Notice: No TCPCL shutdown message was sent (not required)." << std::endl;
    }
    else {
        //std::cout << "timer cancelled\n";
        std::cout << "TCPCL shutdown message was sent." << std::endl;
    }

    //final code to shut down tcp sockets
    std::cout << "deleting TcpclBundleSink TCP Async Sender" << std::endl;
    m_tcpAsyncSenderPtr.reset();
    if (m_tcpSocketPtr) {
        if (m_tcpSocketPtr->is_open()) {
            try {
                std::cout << "shutting down TcpclBundleSink TCP socket.." << std::endl;
                m_tcpSocketPtr->shutdown(boost::asio::socket_base::shutdown_type::shutdown_both);
            }
            catch (const boost::system::system_error & e) {
                std::cerr << "error in TcpclBundleSink::OnSendShutdownMessageTimeout_TimerExpired: " << e.what() << std::endl;
            }
            try {
                std::cout << "closing TcpclBundleSink TCP socket socket.." << std::endl;
                m_tcpSocketPtr->close();
            }
            catch (const boost::system::system_error & e) {
                std::cerr << "error in TcpclBundleSink::OnSendShutdownMessageTimeout_TimerExpired: " << e.what() << std::endl;
            }
        }
        std::cout << "deleting TcpclBundleSink TCP Socket" << std::endl;
        if (m_tcpSocketPtr.use_count() != 1) {
            std::cerr << "error m_tcpSocketPtr.use_count() != 1" << std::endl;
        }
        m_tcpSocketPtr = boost::shared_ptr<boost::asio::ip::tcp::socket>();
    }
    m_needToSendKeepAliveMessageTimer.cancel();
    m_noKeepAlivePacketReceivedTimer.cancel();
    m_tcpcl.InitRx(); //reset states
    m_safeToDelete = true;
    if (m_notifyReadyToDeleteCallback) {
        m_notifyReadyToDeleteCallback();
    }
}


bool TcpclBundleSink::ReadyToBeDeleted() {
    return m_safeToDelete;
}


uint64_t TcpclBundleSink::GetRemoteNodeId() const {
    return m_remoteNodeId;
}

void TcpclBundleSink::TrySendOpportunisticBundleIfAvailable_FromIoServiceThread() {
    if (m_shutdownCalled || m_safeToDelete) {
        std::cerr << "opportunistic link unavailable" << std::endl;
        return;
    }
    std::pair<std::unique_ptr<zmq::message_t>, std::vector<uint8_t> > bundleDataPair;
    const std::size_t totalDataSegmentsUnacked = m_totalDataSegmentsSent - m_totalDataSegmentsAcked;
    const uint64_t bundlePipelineLimit = MAX_UNACKED - 5;
    if ((totalDataSegmentsUnacked <= bundlePipelineLimit) && m_tryGetOpportunisticDataFunction && m_tryGetOpportunisticDataFunction(bundleDataPair)) {
        std::size_t dataSize;
        const uint8_t * dataToSendPtr;
        bool usingZmqData;
        if (bundleDataPair.first) { //this is zmq data
            dataSize = bundleDataPair.first->size();
            dataToSendPtr = (const uint8_t *) bundleDataPair.first->data();
            usingZmqData = true;
        }
        else { //this is std::vector<uint8_t>
            dataSize = bundleDataPair.second.size();
            dataToSendPtr = bundleDataPair.second.data();
            usingZmqData = false;
        }

        const unsigned int writeIndex = m_bytesToAckCb.GetIndexForWrite(); //don't put this in tcp async write callback
        if (writeIndex == UINT32_MAX) { //push check
            std::cerr << "Unexpected Error in TcpclBundleSink::TrySendOpportunisticBundleIfAvailable.. cannot get cb write index" << std::endl;
            return;
        }
        m_bytesToAckCbVec[writeIndex] = dataSize;

        ++m_totalDataSegmentsSent;
        m_totalBundleBytesSent += dataSize;

        std::vector<uint64_t> & currentFragmentBytesVec = m_fragmentBytesToAckCbVec[writeIndex];
        currentFragmentBytesVec.resize(0); //will be zero size if not fragmented
        m_fragmentVectorIndexCbVec[writeIndex] = 0; //used by the ack callback
        std::vector<TcpAsyncSenderElement*> elements;

        if (M_MAX_FRAGMENT_SIZE && (dataSize > M_MAX_FRAGMENT_SIZE)) {
            elements.reserve((dataSize / M_MAX_FRAGMENT_SIZE) + 2);
            uint64_t dataIndex = 0;
            while (true) {
                uint64_t bytesToSend = std::min(dataSize - dataIndex, M_MAX_FRAGMENT_SIZE);
                const bool isStartSegment = (dataIndex == 0);
                const bool isEndSegment = ((bytesToSend + dataIndex) == dataSize);

                TcpAsyncSenderElement * el = new TcpAsyncSenderElement();
                el->m_constBufferVec.resize(2);
                if (usingZmqData) {
                    el->m_underlyingData.resize(1);
                    if (isEndSegment) {
                        el->m_underlyingDataZmq = std::move(bundleDataPair.first);
                    }
                }
                else {
                    el->m_underlyingData.resize(1 + isEndSegment);
                    if (isEndSegment) {
                        el->m_underlyingData[1] = std::move(bundleDataPair.second);
                    }
                }
                Tcpcl::GenerateDataSegmentHeaderOnly(el->m_underlyingData[0], isStartSegment, isEndSegment, bytesToSend);
                el->m_constBufferVec[0] = boost::asio::buffer(el->m_underlyingData[0]);
                el->m_constBufferVec[1] = boost::asio::buffer(dataToSendPtr + dataIndex, bytesToSend);
                el->m_onSuccessfulSendCallbackByIoServiceThreadPtr = &m_handleTcpSendCallback;
                elements.push_back(el);


                dataIndex += bytesToSend;
                currentFragmentBytesVec.push_back(dataIndex); //bytes to ack must be cumulative of fragments

                if (isEndSegment) {
                    break;
                }
            }
        }

        m_bytesToAckCb.CommitWrite(); //pushed

        if (elements.size()) { //is fragmented
            m_totalFragmentedSent += elements.size();
            for (std::size_t i = 0; i < elements.size(); ++i) {
                m_tcpAsyncSenderPtr->AsyncSend_ThreadSafe(elements[i]);
            }
        }
        else {
            TcpAsyncSenderElement * el = new TcpAsyncSenderElement();
            if (usingZmqData) {
                el->m_underlyingData.resize(1);
                el->m_underlyingDataZmq = std::move(bundleDataPair.first);
            }
            else {
                el->m_underlyingData.resize(2);
                el->m_underlyingData[1] = std::move(bundleDataPair.second);
            }
            Tcpcl::GenerateDataSegmentHeaderOnly(el->m_underlyingData[0], true, true, dataSize);
            el->m_constBufferVec.resize(2);
            el->m_constBufferVec[0] = boost::asio::buffer(el->m_underlyingData[0]);
            el->m_constBufferVec[1] = boost::asio::buffer(dataToSendPtr, dataSize);
            el->m_onSuccessfulSendCallbackByIoServiceThreadPtr = &m_handleTcpSendCallback;
            m_tcpAsyncSenderPtr->AsyncSend_ThreadSafe(el);
        }
    }
}

void TcpclBundleSink::SetTryGetOpportunisticDataFunction(const TryGetOpportunisticDataFunction_t & tryGetOpportunisticDataFunction) {
    m_tryGetOpportunisticDataFunction = tryGetOpportunisticDataFunction;
}
void TcpclBundleSink::SetNotifyOpportunisticDataAckedCallback(const NotifyOpportunisticDataAckedCallback_t & notifyOpportunisticDataAckedCallback) {
    m_notifyOpportunisticDataAckedCallback = notifyOpportunisticDataAckedCallback;
}
