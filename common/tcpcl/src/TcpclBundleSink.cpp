#include <boost/bind.hpp>
#include <boost/make_shared.hpp>
#include <iostream>
#include "TcpclBundleSink.h"


TcpclBundleSink::TcpclBundleSink(boost::shared_ptr<boost::asio::ip::tcp::socket> tcpSocketPtr,
                                 WholeBundleReadyCallback_t wholeBundleReadyCallback,
                                 //ConnectionClosedCallback_t connectionClosedCallback,
                                 const unsigned int numCircularBufferVectors,
                                 const unsigned int circularBufferBytesPerVector) :
    m_wholeBundleReadyCallback(wholeBundleReadyCallback),
    //m_connectionClosedCallback(connectionClosedCallback),
    m_tcpSocketPtr(tcpSocketPtr),
    m_noKeepAlivePacketReceivedTimer(tcpSocketPtr->get_executor()),
    m_needToSendKeepAliveMessageTimer(tcpSocketPtr->get_executor()),
    M_NUM_CIRCULAR_BUFFER_VECTORS(numCircularBufferVectors),
    M_CIRCULAR_BUFFER_BYTES_PER_VECTOR(circularBufferBytesPerVector),
    m_circularIndexBuffer(M_NUM_CIRCULAR_BUFFER_VECTORS),
    m_tcpReceiveBuffersCbVec(M_NUM_CIRCULAR_BUFFER_VECTORS),
    m_tcpReceiveBytesTransferredCbVec(M_NUM_CIRCULAR_BUFFER_VECTORS),
    m_running(false),
    m_safeToDelete(false)
{

    m_tcpcl.SetContactHeaderReadCallback(boost::bind(&TcpclBundleSink::ContactHeaderCallback, this, boost::placeholders::_1, boost::placeholders::_2, boost::placeholders::_3));
    m_tcpcl.SetDataSegmentContentsReadCallback(boost::bind(&TcpclBundleSink::DataSegmentCallback, this, boost::placeholders::_1, boost::placeholders::_2, boost::placeholders::_3));
    m_tcpcl.SetAckSegmentReadCallback(boost::bind(&TcpclBundleSink::AckCallback, this, boost::placeholders::_1));
    m_tcpcl.SetBundleRefusalCallback(boost::bind(&TcpclBundleSink::BundleRefusalCallback, this, boost::placeholders::_1));
    m_tcpcl.SetNextBundleLengthCallback(boost::bind(&TcpclBundleSink::NextBundleLengthCallback, this, boost::placeholders::_1));
    m_tcpcl.SetKeepAliveCallback(boost::bind(&TcpclBundleSink::KeepAliveCallback, this));
    m_tcpcl.SetShutdownMessageCallback(boost::bind(&TcpclBundleSink::ShutdownCallback, this, boost::placeholders::_1, boost::placeholders::_2, boost::placeholders::_3, boost::placeholders::_4));

    for (unsigned int i = 0; i < M_NUM_CIRCULAR_BUFFER_VECTORS; ++i) {
        m_tcpReceiveBuffersCbVec[i].resize(M_CIRCULAR_BUFFER_BYTES_PER_VECTOR);
    }

    m_running = true;
    m_threadCbReaderPtr = boost::make_shared<boost::thread>(
        boost::bind(&TcpclBundleSink::PopCbThreadFunc, this)); //create and start the worker thread


    StartTcpReceive();
}

TcpclBundleSink::~TcpclBundleSink() {

    ShutdownAndCloseTcpSocket();

    m_running = false; //thread stopping criteria

    if (m_threadCbReaderPtr) {
        m_threadCbReaderPtr->join();
        m_threadCbReaderPtr = boost::shared_ptr<boost::thread>();
    }
}




void TcpclBundleSink::StartTcpReceive() {
    const unsigned int writeIndex = m_circularIndexBuffer.GetIndexForWrite(); //store the volatile
    if (writeIndex == UINT32_MAX) {
        std::cerr << "critical error in TcpclBundleSink::StartTcpReceive(): buffers full.. TCP receiving on TcpclBundleSink will now stop!\n";
        return;
    }
    if(m_tcpSocketPtr) {
        m_tcpSocketPtr->async_read_some(
            boost::asio::buffer(m_tcpReceiveBuffersCbVec[writeIndex]),
            boost::bind(&TcpclBundleSink::HandleTcpReceiveSome, this,
                boost::asio::placeholders::error,
                boost::asio::placeholders::bytes_transferred,
                writeIndex));
    }
}
void TcpclBundleSink::HandleTcpReceiveSome(const boost::system::error_code & error, std::size_t bytesTransferred, unsigned int writeIndex) {
    if (!error) {
        m_tcpReceiveBytesTransferredCbVec[writeIndex] = bytesTransferred;
        m_circularIndexBuffer.CommitWrite(); //write complete at this point
        m_conditionVariableCb.notify_one();
        StartTcpReceive(); //restart operation only if there was no error
    }
    else if (error == boost::asio::error::eof) {
        std::cout << "Tcp connection closed cleanly by peer" << std::endl;
        ShutdownAndCloseTcpSocket();
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

void TcpclBundleSink::HandleTcpSend(boost::shared_ptr<std::vector<boost::uint8_t> > dataSentPtr, const boost::system::error_code& error, std::size_t bytes_transferred, bool closeSocket) {
    if(error) {
        std::cerr << "error in TcpclBundleSink::HandleTcpSend: " << error.message() << std::endl;
        ShutdownAndCloseTcpSocket();
    }
    else if(closeSocket) {
        ShutdownAndCloseTcpSocket();
    }
}

void TcpclBundleSink::ContactHeaderCallback(CONTACT_HEADER_FLAGS flags, uint16_t keepAliveIntervalSeconds, const std::string & localEid) {
    m_contactHeaderFlags = flags;
    //The keepalive_interval parameter is set to the minimum value from
    //both contact headers.  If one or both contact headers contains the
    //value zero, then the keepalive feature (described in Section 5.6)
    //is disabled.
    m_keepAliveIntervalSeconds = keepAliveIntervalSeconds;
    m_localEid = localEid;
    std::cout << "received contact header from " << m_localEid << std::endl;

    //Since TcpclBundleSink was waiting for a contact header, it just got one.  Now it's time to reply with a contact header
    //use the same keepalive interval
    if(m_tcpSocketPtr) {
        boost::shared_ptr<std::vector<uint8_t> > contactHeaderPtr = boost::make_shared<std::vector<uint8_t> >();
        Tcpcl::GenerateContactHeader(*contactHeaderPtr, static_cast<CONTACT_HEADER_FLAGS>(0), keepAliveIntervalSeconds, "bpsink");
        boost::asio::async_write(*m_tcpSocketPtr, boost::asio::buffer(*contactHeaderPtr),
                                         boost::bind(&TcpclBundleSink::HandleTcpSend, this, contactHeaderPtr,
                                                     boost::asio::placeholders::error,
                                                     boost::asio::placeholders::bytes_transferred, false));
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
}

void TcpclBundleSink::DataSegmentCallback(boost::shared_ptr<std::vector<uint8_t> > dataSegmentDataSharedPtr, bool isStartFlag, bool isEndFlag) {

    uint32_t bytesToAck = 0;
    if(isStartFlag && isEndFlag) { //optimization for whole (non-fragmented) data
        m_wholeBundleReadyCallback(dataSegmentDataSharedPtr);
        bytesToAck = static_cast<uint32_t>(dataSegmentDataSharedPtr->size());
    }
    else {
        if (isStartFlag) {
            m_fragmentedBundleRxConcat.resize(0);
        }
        m_fragmentedBundleRxConcat.insert(m_fragmentedBundleRxConcat.end(), dataSegmentDataSharedPtr->begin(), dataSegmentDataSharedPtr->end()); //concatenate
        bytesToAck = static_cast<uint32_t>(m_fragmentedBundleRxConcat.size());
        if(isEndFlag) { //fragmentation complete
            *dataSegmentDataSharedPtr = std::move(m_fragmentedBundleRxConcat);
            m_wholeBundleReadyCallback(dataSegmentDataSharedPtr);
        }
    }
    //send ack
    if((static_cast<unsigned int>(CONTACT_HEADER_FLAGS::REQUEST_ACK_OF_BUNDLE_SEGMENTS)) & (static_cast<unsigned int>(m_contactHeaderFlags))) {
        if(m_tcpSocketPtr) {
            boost::shared_ptr<std::vector<uint8_t> > ackPtr = boost::make_shared<std::vector<uint8_t> >();
            Tcpcl::GenerateAckSegment(*ackPtr, bytesToAck);
            boost::asio::async_write(*m_tcpSocketPtr, boost::asio::buffer(*ackPtr),
                                             boost::bind(&TcpclBundleSink::HandleTcpSend, this, ackPtr,
                                                         boost::asio::placeholders::error,
                                                         boost::asio::placeholders::bytes_transferred, false));
        }
    }
}

void TcpclBundleSink::AckCallback(uint32_t totalBytesAcknowledged) {
    std::cout << "TcpclBundleSink should never enter AckCallback" << std::endl;
}

void TcpclBundleSink::BundleRefusalCallback(BUNDLE_REFUSAL_CODES refusalCode) {
    std::cout << "TcpclBundleSink should never enter BundleRefusalCallback" << std::endl;
}

void TcpclBundleSink::NextBundleLengthCallback(uint32_t nextBundleLength) {
    std::cout << "next bundle length: " << nextBundleLength << std::endl;
}

void TcpclBundleSink::KeepAliveCallback() {
    std::cout << "received keepalive packet" << std::endl;
    // * 2 =>
    //If no message (KEEPALIVE or other) has been received for at least
    //twice the keepalive_interval, then either party MAY terminate the
    //session by transmitting a one-byte SHUTDOWN message (as described in
    //Table 2) and by closing the TCP connection.
    m_noKeepAlivePacketReceivedTimer.expires_from_now(boost::posix_time::seconds(m_keepAliveIntervalSeconds * 2)); //cancels active timer with cancel flag in callback
    m_noKeepAlivePacketReceivedTimer.async_wait(boost::bind(&TcpclBundleSink::OnNoKeepAlivePacketReceived_TimerExpired, this, boost::asio::placeholders::error));
}

void TcpclBundleSink::ShutdownCallback(bool hasReasonCode, SHUTDOWN_REASON_CODES shutdownReasonCode,
                                         bool hasReconnectionDelay, uint32_t reconnectionDelaySeconds)
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
    ShutdownAndCloseTcpSocket();
}

void TcpclBundleSink::OnNoKeepAlivePacketReceived_TimerExpired(const boost::system::error_code& e) {
    if (e != boost::asio::error::operation_aborted) {
        // Timer was not cancelled, take necessary action.
        if(m_tcpSocketPtr) {
            std::cout << "error: tcp keepalive timed out.. closing socket\n";
            boost::shared_ptr<std::vector<uint8_t> > shutdownPtr = boost::make_shared<std::vector<uint8_t> >();
            Tcpcl::GenerateShutdownMessage(*shutdownPtr, true, SHUTDOWN_REASON_CODES::IDLE_TIMEOUT, false, 0);
            boost::asio::async_write(*m_tcpSocketPtr, boost::asio::buffer(*shutdownPtr),
                                             boost::bind(&TcpclBundleSink::HandleTcpSend, this, shutdownPtr,
                                                         boost::asio::placeholders::error,
                                                         boost::asio::placeholders::bytes_transferred, true)); //true => shutdown after data sent
        }

    }
    else {
        //std::cout << "timer cancelled\n";
    }
}

void TcpclBundleSink::OnNeedToSendKeepAliveMessage_TimerExpired(const boost::system::error_code& e) {
    if (e != boost::asio::error::operation_aborted) {
        // Timer was not cancelled, take necessary action.
        //SEND KEEPALIVE PACKET
        if(m_tcpSocketPtr) {
            boost::shared_ptr<std::vector<uint8_t> > keepAlivePtr = boost::make_shared<std::vector<uint8_t> >();
            Tcpcl::GenerateKeepAliveMessage(*keepAlivePtr);
            boost::asio::async_write(*m_tcpSocketPtr, boost::asio::buffer(*keepAlivePtr),
                                             boost::bind(&TcpclBundleSink::HandleTcpSend, this, keepAlivePtr,
                                                         boost::asio::placeholders::error,
                                                         boost::asio::placeholders::bytes_transferred, false));

            m_needToSendKeepAliveMessageTimer.expires_from_now(boost::posix_time::seconds(m_keepAliveIntervalSeconds));
            m_needToSendKeepAliveMessageTimer.async_wait(boost::bind(&TcpclBundleSink::OnNeedToSendKeepAliveMessage_TimerExpired, this, boost::asio::placeholders::error));
        }
    }
    else {
        //std::cout << "timer cancelled\n";
    }
}

void TcpclBundleSink::ShutdownAndCloseTcpSocket() {
    if(m_tcpSocketPtr) {
        std::cout << "shutting down connection\n";
        m_tcpSocketPtr = boost::shared_ptr<boost::asio::ip::tcp::socket>();
    }
    m_needToSendKeepAliveMessageTimer.cancel();
    m_noKeepAlivePacketReceivedTimer.cancel();
    m_tcpcl.InitRx(); //reset states
    m_safeToDelete = true;
}

bool TcpclBundleSink::ReadyToBeDeleted() {
    return m_safeToDelete;
}
