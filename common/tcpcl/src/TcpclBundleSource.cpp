#include <string>
#include <iostream>
#include "TcpclBundleSource.h"
#include <boost/lexical_cast.hpp>
#include <boost/make_shared.hpp>


TcpclBundleSource::TcpclBundleSource(const uint16_t desiredKeeAliveIntervlSeconds, const std::string & thisEidString) :
m_work(m_ioService), //prevent stopping of ioservice until destructor
m_resolver(m_ioService),
m_noKeepAlivePacketReceivedTimer(m_ioService),
m_needToSendKeepAliveMessageTimer(m_ioService),
m_handleSocketShutdownCancelOnlyTimer(m_ioService),
m_sendShutdownMessageTimeoutTimer(m_ioService),
m_keepAliveIntervalSeconds(desiredKeeAliveIntervlSeconds),
m_readyToForward(false),
m_tcpclShutdownComplete(true),
m_sendShutdownMessage(false),
m_reasonWasTimeOut(false),
M_DESIRED_KEEPALIVE_INTERVAL_SECONDS(desiredKeeAliveIntervlSeconds),
M_THIS_EID_STRING(thisEidString),

m_totalDataSegmentsAcked(0),
m_totalBytesAcked(0),
m_totalDataSegmentsSent(0),
m_totalBundleBytesSent(0)
{
    m_ioServiceThreadPtr = boost::make_shared<boost::thread>(boost::bind(&boost::asio::io_service::run, &m_ioService));

    m_handleSocketShutdownCancelOnlyTimer.expires_from_now(boost::posix_time::pos_infin);
    m_handleSocketShutdownCancelOnlyTimer.async_wait(boost::bind(&TcpclBundleSource::OnHandleSocketShutdown_TimerCancelled, this, boost::asio::placeholders::error));


    m_tcpcl.SetContactHeaderReadCallback(boost::bind(&TcpclBundleSource::ContactHeaderCallback, this, boost::placeholders::_1, boost::placeholders::_2, boost::placeholders::_3));
    m_tcpcl.SetDataSegmentContentsReadCallback(boost::bind(&TcpclBundleSource::DataSegmentCallback, this, boost::placeholders::_1, boost::placeholders::_2, boost::placeholders::_3));
    m_tcpcl.SetAckSegmentReadCallback(boost::bind(&TcpclBundleSource::AckCallback, this, boost::placeholders::_1));
    m_tcpcl.SetBundleRefusalCallback(boost::bind(&TcpclBundleSource::BundleRefusalCallback, this, boost::placeholders::_1));
    m_tcpcl.SetNextBundleLengthCallback(boost::bind(&TcpclBundleSource::NextBundleLengthCallback, this, boost::placeholders::_1));
    m_tcpcl.SetKeepAliveCallback(boost::bind(&TcpclBundleSource::KeepAliveCallback, this));
    m_tcpcl.SetShutdownMessageCallback(boost::bind(&TcpclBundleSource::ShutdownCallback, this, boost::placeholders::_1, boost::placeholders::_2, boost::placeholders::_3, boost::placeholders::_4));

}

TcpclBundleSource::~TcpclBundleSource() {
    
    DoTcpclShutdown(true, false);
    while (!m_tcpclShutdownComplete) {
        boost::this_thread::sleep(boost::posix_time::milliseconds(250));
    }
    m_ioService.stop(); //ioservice requires stopping before join because of the m_work object

    if(m_ioServiceThreadPtr) {
        m_ioServiceThreadPtr->join();
        m_ioServiceThreadPtr = boost::shared_ptr<boost::thread>();
    }

    //print stats
    std::cout << "m_totalDataSegmentsAcked " << m_totalDataSegmentsAcked << std::endl;
    std::cout << "m_totalBytesAcked " << m_totalBytesAcked << std::endl;
    std::cout << "m_totalDataSegmentsSent " << m_totalDataSegmentsSent << std::endl;
    std::cout << "m_totalBundleBytesSent " << m_totalBundleBytesSent << std::endl;
}


bool TcpclBundleSource::Forward(const uint8_t* bundleData, const std::size_t size) {

    if(!m_readyToForward) {
        std::cerr << "link not ready to forward yet" << std::endl;
        return false;
    }
    ++m_totalDataSegmentsSent;
    m_totalBundleBytesSent += size;
    m_bytesToAckQueue.push(static_cast<uint32_t>(size));

    boost::shared_ptr<std::vector<uint8_t> > bundleSegmentPtr = boost::make_shared<std::vector<uint8_t> >();
    Tcpcl::GenerateDataSegment(*bundleSegmentPtr, true, true, bundleData, static_cast<uint32_t>(size));

    boost::asio::async_write(*m_tcpSocketPtr, boost::asio::buffer(*bundleSegmentPtr),
                                     boost::bind(&TcpclBundleSource::HandleTcpSend, this, bundleSegmentPtr,
                                                 boost::asio::placeholders::error,
                                                 boost::asio::placeholders::bytes_transferred));
    return true;
}



void TcpclBundleSource::Connect(const std::string & hostname, const std::string & port) {

    boost::asio::ip::tcp::resolver::query query(hostname, port);
    m_resolver.async_resolve(query, boost::bind(&TcpclBundleSource::OnResolve,
                                                this, boost::asio::placeholders::error,
                                                boost::asio::placeholders::results));
}

void TcpclBundleSource::OnResolve(const boost::system::error_code & ec, boost::asio::ip::tcp::resolver::results_type results) { // Resolved endpoints as a range.
    if(ec) {
        std::cerr << "Error resolving: " << ec.message() << std::endl;
    }
    else {
        std::cout << "resolved host to " << results->endpoint().address() << ":" << results->endpoint().port() << ".  Connecting..." << std::endl;
        m_tcpSocketPtr = boost::make_shared<boost::asio::ip::tcp::socket>(m_ioService);
        boost::asio::async_connect(
            *m_tcpSocketPtr,
            results,
            boost::bind(
                &TcpclBundleSource::OnConnect,
                this,
                boost::asio::placeholders::error));
    }
}

void TcpclBundleSource::OnConnect(const boost::system::error_code & ec) {

    if (ec) {
        if (ec != boost::asio::error::operation_aborted) {
            std::cerr << "Error in OnConnect: " << ec.value() << " " << ec.message() << "\n";
        }
        return;
    }

    std::cout << "connected.. sending contact header..\n";
    m_tcpclShutdownComplete = false;

    if(m_tcpSocketPtr) {
        boost::shared_ptr<std::vector<uint8_t> > contactHeaderPtr = boost::make_shared<std::vector<uint8_t> >();
        Tcpcl::GenerateContactHeader(*contactHeaderPtr, CONTACT_HEADER_FLAGS::REQUEST_ACK_OF_BUNDLE_SEGMENTS, m_keepAliveIntervalSeconds, M_THIS_EID_STRING);
        boost::asio::async_write(*m_tcpSocketPtr, boost::asio::buffer(*contactHeaderPtr),
                                         boost::bind(&TcpclBundleSource::HandleTcpSend, this, contactHeaderPtr,
                                                     boost::asio::placeholders::error,
                                                     boost::asio::placeholders::bytes_transferred));

        StartTcpReceive();
    }
}



void TcpclBundleSource::HandleTcpSend(boost::shared_ptr<std::vector<boost::uint8_t> > dataSentPtr, const boost::system::error_code& error, std::size_t bytes_transferred) {
    if (error) {
        std::cerr << "error in TcpclBundleSource::HandleTcpSend: " << error.message() << std::endl;
        DoTcpclShutdown(true, false);
    }
}

void TcpclBundleSource::HandleTcpSendShutdown(boost::shared_ptr<std::vector<boost::uint8_t> > dataSentPtr, const boost::system::error_code& error, std::size_t bytes_transferred) {
    if (error) {
        std::cerr << "error in TcpclBundleSource::HandleTcpSendShutdown: " << error.message() << std::endl;
    }
    else {
        m_sendShutdownMessageTimeoutTimer.cancel();
    }
}

void TcpclBundleSource::StartTcpReceive() {
    m_tcpSocketPtr->async_read_some(
        boost::asio::buffer(m_tcpReadSomeBuffer, 2000),
        boost::bind(&TcpclBundleSource::HandleTcpReceiveSome, this,
                    boost::asio::placeholders::error,
                    boost::asio::placeholders::bytes_transferred));
}
void TcpclBundleSource::HandleTcpReceiveSome(const boost::system::error_code & error, std::size_t bytesTransferred) {
    if (!error) {
        //std::cout << "received " << bytesTransferred << "\n";

        //because TcpclBundleSource will not receive much data from the destination,
        //a separate thread is not needed to process it, but rather this
        //io_service thread will do the processing
        m_tcpcl.HandleReceivedChars(m_tcpReadSomeBuffer, bytesTransferred);
        StartTcpReceive(); //restart operation only if there was no error
    }
    else if (error == boost::asio::error::eof) {
        std::cout << "Tcp connection closed cleanly by peer" << std::endl;
        DoTcpclShutdown(false, false);
    }
    else if (error != boost::asio::error::operation_aborted) {
        std::cerr << "Error in UartMcu::HandleTcpReceiveSome: " << error.message() << std::endl;
    }
}




void TcpclBundleSource::ContactHeaderCallback(CONTACT_HEADER_FLAGS flags, uint16_t keepAliveIntervalSeconds, const std::string & localEid) {
    m_contactHeaderFlags = flags;
    //The keepalive_interval parameter is set to the minimum value from
    //both contact headers.  If one or both contact headers contains the
    //value zero, then the keepalive feature (described in Section 5.6)
    //is disabled.
    if(m_keepAliveIntervalSeconds > keepAliveIntervalSeconds) {
        std::cout << "notice: host has requested a smaller keepalive interval of " << keepAliveIntervalSeconds << " seconds." << std::endl;
        m_keepAliveIntervalSeconds = keepAliveIntervalSeconds;
    }
    m_localEid = localEid;
    std::cout << "received contact header back from " << m_localEid << std::endl;
    m_readyToForward = true;


    if(m_keepAliveIntervalSeconds) { //non-zero
        std::cout << "using " << keepAliveIntervalSeconds << " seconds for keepalive\n";

        // * 2 =>
        //If no message (KEEPALIVE or other) has been received for at least
        //twice the keepalive_interval, then either party MAY terminate the
        //session by transmitting a one-byte SHUTDOWN message (as described in
        //Table 2) and by closing the TCP connection.
        m_noKeepAlivePacketReceivedTimer.expires_from_now(boost::posix_time::seconds(m_keepAliveIntervalSeconds * 2));
        m_noKeepAlivePacketReceivedTimer.async_wait(boost::bind(&TcpclBundleSource::OnNoKeepAlivePacketReceived_TimerExpired, this, boost::asio::placeholders::error));


        m_needToSendKeepAliveMessageTimer.expires_from_now(boost::posix_time::seconds(m_keepAliveIntervalSeconds));
        m_needToSendKeepAliveMessageTimer.async_wait(boost::bind(&TcpclBundleSource::OnNeedToSendKeepAliveMessage_TimerExpired, this, boost::asio::placeholders::error));
    }

}

void TcpclBundleSource::DataSegmentCallback(boost::shared_ptr<std::vector<uint8_t> > dataSegmentDataSharedPtr, bool isStartFlag, bool isEndFlag) {

    std::cout << "TcpclBundleSource should never enter DataSegmentCallback" << std::endl;
}

void TcpclBundleSource::AckCallback(uint32_t totalBytesAcknowledged) {
    if(m_bytesToAckQueue.empty()) {
        std::cerr << "error: AckCallback called with empty queue" << std::endl;
    }
    else if(m_bytesToAckQueue.front() == totalBytesAcknowledged) {
        ++m_totalDataSegmentsAcked;
        m_totalBytesAcked += m_bytesToAckQueue.front();
        m_bytesToAckQueue.pop();
    }
    else {
        std::cerr << "error: wrong bytes acked: expected " << m_bytesToAckQueue.front() << " but got " << totalBytesAcknowledged << std::endl;
    }
}

void TcpclBundleSource::BundleRefusalCallback(BUNDLE_REFUSAL_CODES refusalCode) {
    std::cout << "error: BundleRefusalCallback not implemented yet in TcpclBundleSource" << std::endl;
}

void TcpclBundleSource::NextBundleLengthCallback(uint32_t nextBundleLength) {
    std::cout << "TcpclBundleSource should never enter NextBundleLengthCallback" << std::endl;
}

void TcpclBundleSource::KeepAliveCallback() {
    std::cout << "received keepalive packet\n";
    // * 2 =>
    //If no message (KEEPALIVE or other) has been received for at least
    //twice the keepalive_interval, then either party MAY terminate the
    //session by transmitting a one-byte SHUTDOWN message (as described in
    //Table 2) and by closing the TCP connection.
    m_noKeepAlivePacketReceivedTimer.expires_from_now(boost::posix_time::seconds(m_keepAliveIntervalSeconds * 2)); //cancels active timer with cancel flag in callback
    m_noKeepAlivePacketReceivedTimer.async_wait(boost::bind(&TcpclBundleSource::OnNoKeepAlivePacketReceived_TimerExpired, this, boost::asio::placeholders::error));
}

void TcpclBundleSource::ShutdownCallback(bool hasReasonCode, SHUTDOWN_REASON_CODES shutdownReasonCode,
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
    DoTcpclShutdown(false, false);
}

void TcpclBundleSource::OnNoKeepAlivePacketReceived_TimerExpired(const boost::system::error_code& e) {
    if (e != boost::asio::error::operation_aborted) {
        // Timer was not cancelled, take necessary action.
        DoTcpclShutdown(true, true);
    }
    else {
        //std::cout << "timer cancelled\n";
    }
}

void TcpclBundleSource::OnNeedToSendKeepAliveMessage_TimerExpired(const boost::system::error_code& e) {
    if (e != boost::asio::error::operation_aborted) {
        // Timer was not cancelled, take necessary action.
        //SEND KEEPALIVE PACKET
        if(m_tcpSocketPtr) {
            boost::shared_ptr<std::vector<uint8_t> > keepAlivePtr = boost::make_shared<std::vector<uint8_t> >();
            Tcpcl::GenerateKeepAliveMessage(*keepAlivePtr);
            boost::asio::async_write(*m_tcpSocketPtr, boost::asio::buffer(*keepAlivePtr),
                                             boost::bind(&TcpclBundleSource::HandleTcpSend, this, keepAlivePtr,
                                                         boost::asio::placeholders::error,
                                                         boost::asio::placeholders::bytes_transferred));

            m_needToSendKeepAliveMessageTimer.expires_from_now(boost::posix_time::seconds(m_keepAliveIntervalSeconds));
            m_needToSendKeepAliveMessageTimer.async_wait(boost::bind(&TcpclBundleSource::OnNeedToSendKeepAliveMessage_TimerExpired, this, boost::asio::placeholders::error));
        }
    }
    else {
        //std::cout << "timer cancelled\n";
    }
}

void TcpclBundleSource::DoTcpclShutdown(bool sendShutdownMessage, bool reasonWasTimeOut) {
    m_sendShutdownMessage = sendShutdownMessage;
    m_reasonWasTimeOut = reasonWasTimeOut;
    m_handleSocketShutdownCancelOnlyTimer.cancel();
}

void TcpclBundleSource::OnHandleSocketShutdown_TimerCancelled(const boost::system::error_code& e) {
    if (e == boost::asio::error::operation_aborted) {
        // Timer was cancelled as expected.  This method keeps socket shutdown within io_service thread.

        m_readyToForward = false;
        if (m_sendShutdownMessage) {
            std::cout << "Sending shutdown packet to cleanly close tcpcl.. " << std::endl;
            boost::shared_ptr<std::vector<uint8_t> > shutdownPtr = boost::make_shared<std::vector<uint8_t> >();
            if (m_reasonWasTimeOut) {
                Tcpcl::GenerateShutdownMessage(*shutdownPtr, true, SHUTDOWN_REASON_CODES::IDLE_TIMEOUT, false, 0);
            }
            else {
                Tcpcl::GenerateShutdownMessage(*shutdownPtr, false, SHUTDOWN_REASON_CODES::UNASSIGNED, false, 0);
            }

            boost::asio::async_write(*m_tcpSocketPtr, boost::asio::buffer(*shutdownPtr),
                                     boost::bind(&TcpclBundleSource::HandleTcpSendShutdown, this, shutdownPtr,
                                                 boost::asio::placeholders::error,
                                                 boost::asio::placeholders::bytes_transferred));

            m_sendShutdownMessageTimeoutTimer.expires_from_now(boost::posix_time::seconds(3));
        }
        else {
            m_sendShutdownMessageTimeoutTimer.expires_from_now(boost::posix_time::seconds(0));
        }
        m_sendShutdownMessageTimeoutTimer.async_wait(boost::bind(&TcpclBundleSource::OnSendShutdownMessageTimeout_TimerExpired, this, boost::asio::placeholders::error));
    }
    else {
        std::cerr << "Critical error in OnHandleSocketShutdown_TimerCancelled: timer was not cancelled" << std::endl;
    }
}

void TcpclBundleSource::OnSendShutdownMessageTimeout_TimerExpired(const boost::system::error_code& e) {
    if (e != boost::asio::error::operation_aborted) {
        // Timer was not cancelled, take necessary action.
        std::cout << "Notice: No TCPCL shutdown message was sent (not required)." << std::endl;
    }
    else {
        //std::cout << "timer cancelled\n";
        std::cout << "TCPCL shutdown message was sent." << std::endl;
    }

    //final code to shut down tcp sockets
    if (m_tcpSocketPtr) {
        try {
            std::cout << "shutting down tcp socket.." << std::endl;
            m_tcpSocketPtr->shutdown(boost::asio::socket_base::shutdown_type::shutdown_both);
            std::cout << "closing tcp socket.." << std::endl;
            m_tcpSocketPtr->close();
        }
        catch (const boost::system::system_error & e) {
            std::cerr << "error in OnSendShutdownMessageTimeout_TimerExpired: " << e.what() << std::endl;
        }
        //don't delete the tcp socket because the Forward function is multi-threaded without a mutex to
        //increase speed, so prevent a race condition that would cause a null pointer exception
        //std::cout << "deleting tcp socket" << std::endl;
        //m_tcpSocketPtr = boost::shared_ptr<boost::asio::ip::tcp::socket>();
    }
    m_needToSendKeepAliveMessageTimer.cancel();
    m_noKeepAlivePacketReceivedTimer.cancel();
    m_tcpcl.InitRx(); //reset states
    m_tcpclShutdownComplete = true;
}

bool TcpclBundleSource::ReadyToForward() {
    return m_readyToForward;
}
