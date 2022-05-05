/**
 * @file UdpDelaySim.cpp
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

#include <iostream>


#include "UdpDelaySim.h"
#include <boost/make_unique.hpp>


UdpDelaySim::UdpDelaySim(uint16_t myBoundUdpPort,
    const std::string & remoteHostnameToForwardPacketsTo,
    const std::string & remotePortToForwardPacketsTo,
    const unsigned int numCircularBufferVectors,
    const unsigned int maxUdpPacketSizeBytes,
    const boost::posix_time::time_duration & sendDelay,
    const bool autoStart) :
    m_resolver(m_ioService),
    m_udpPacketSendDelayTimer(m_ioService),
    m_timerTransferRateStats(m_ioService),
    m_udpSocket(m_ioService),
    M_MY_BOUND_UDP_PORT(myBoundUdpPort),
    M_REMOTE_HOSTNAME_TO_FORWARD_PACKETS_TO(remoteHostnameToForwardPacketsTo),
    M_REMOTE_PORT_TO_FORWARD_PACKETS_TO(remotePortToForwardPacketsTo),
    M_NUM_CIRCULAR_BUFFER_VECTORS(numCircularBufferVectors),
    M_MAX_UDP_PACKET_SIZE_BYTES(maxUdpPacketSizeBytes),
    M_SEND_DELAY(sendDelay),
    m_udpReceiveBuffer(M_MAX_UDP_PACKET_SIZE_BYTES),
    m_circularIndexBuffer(M_NUM_CIRCULAR_BUFFER_VECTORS),
    m_udpReceiveBuffersCbVec(M_NUM_CIRCULAR_BUFFER_VECTORS),
    m_udpReceiveBytesTransferredCbVec(M_NUM_CIRCULAR_BUFFER_VECTORS),
    m_expiriesCbVec(M_NUM_CIRCULAR_BUFFER_VECTORS),
    m_printedCbTooSmallNotice(false),
    m_sendDelayTimerIsRunning(false),
    //stats
    m_lastTotalUdpPacketsReceived(0),
    m_lastTotalUdpBytesReceived(0),
    m_lastTotalUdpPacketsSent(0),
    m_lastTotalUdpBytesSent(0),
    m_countCircularBufferOverruns(0),
    m_countMaxCircularBufferSize(0),
    m_countTotalUdpPacketsReceived(0),
    m_countTotalUdpBytesReceived(0),
    m_countTotalUdpPacketsSent(0),
    m_countTotalUdpBytesSent(0)
{
    for (unsigned int i = 0; i < M_NUM_CIRCULAR_BUFFER_VECTORS; ++i) {
        m_udpReceiveBuffersCbVec[i].resize(M_MAX_UDP_PACKET_SIZE_BYTES);
    }

    if (autoStart) {
        StartIfNotAlreadyRunning(); //TODO EVALUATE IF AUTO START SAFE
    }
}

bool UdpDelaySim::StartIfNotAlreadyRunning() {
    if (!m_ioServiceThreadPtr) {
        //Receiver UDP
        try {
            m_udpSocket.open(boost::asio::ip::udp::v4());
            m_udpSocket.bind(boost::asio::ip::udp::endpoint(boost::asio::ip::udp::v4(), M_MY_BOUND_UDP_PORT)); // //if udpPort is 0 then bind to random ephemeral port
        }
        catch (const boost::system::system_error & e) {
            std::cerr << "Could not bind on UDP port " << M_MY_BOUND_UDP_PORT << std::endl;
            std::cerr << "  Error: " << e.what() << std::endl;

            return false;
        }
        printf("UdpDelaySim bound successfully on UDP port %d\n", m_udpSocket.local_endpoint().port());

        {
            //start timer
            m_lastPtime = boost::posix_time::microsec_clock::universal_time();
            m_timerTransferRateStats.expires_from_now(boost::posix_time::seconds(5));
            m_timerTransferRateStats.async_wait(boost::bind(&UdpDelaySim::TransferRate_TimerExpired, this, boost::asio::placeholders::error));
        }

        {
            //connect
            static const boost::asio::ip::resolver_query_base::flags UDP_RESOLVER_FLAGS = boost::asio::ip::resolver_query_base::canonical_name; //boost resolver flags
            std::cout << "udp resolving remote " << M_REMOTE_HOSTNAME_TO_FORWARD_PACKETS_TO << ":" << M_REMOTE_PORT_TO_FORWARD_PACKETS_TO << std::endl;
            boost::asio::ip::udp::resolver::query query(boost::asio::ip::udp::v4(), M_REMOTE_HOSTNAME_TO_FORWARD_PACKETS_TO, M_REMOTE_PORT_TO_FORWARD_PACKETS_TO, UDP_RESOLVER_FLAGS);
            m_resolver.async_resolve(query, boost::bind(&UdpDelaySim::OnResolve,
                this, boost::asio::placeholders::error,
                boost::asio::placeholders::results));
            m_ioServiceThreadPtr = boost::make_unique<boost::thread>(boost::bind(&boost::asio::io_service::run, &m_ioService)); //resolve serves as initial work
        }
    }
    return true;
}

UdpDelaySim::~UdpDelaySim() {
    Stop();
    std::cout << "stats:\n";
    std::cout << "m_countCircularBufferOverruns " << m_countCircularBufferOverruns << "\n";
    std::cout << "m_countMaxCircularBufferSize " << m_countMaxCircularBufferSize << "\n";
    std::cout << "m_countTotalUdpPacketsReceived " << m_countTotalUdpPacketsReceived << "\n";
    std::cout << "m_countTotalUdpPacketsSent " << m_countTotalUdpPacketsSent << "\n";
}

void UdpDelaySim::Stop() {

    DoUdpShutdown();
    while (m_udpSocket.is_open()) {
        boost::this_thread::sleep(boost::posix_time::milliseconds(250));
    }

    if (m_ioServiceThreadPtr) {
        m_ioServiceThreadPtr->join();
        m_ioServiceThreadPtr.reset(); //delete it
    }
    
}

void UdpDelaySim::OnResolve(const boost::system::error_code & ec, boost::asio::ip::udp::resolver::results_type results) { // Resolved endpoints as a range.
    if (ec) {
        std::cerr << "Error resolving: " << ec.message() << std::endl;
    }
    else {
        m_udpDestinationEndpoint = *results;
        std::cout << "resolved host to " << m_udpDestinationEndpoint.address() << ":" << m_udpDestinationEndpoint.port() << ".  Binding..." << std::endl;
        StartUdpReceive();
    }
}

void UdpDelaySim::StartUdpReceive() {
    m_udpSocket.async_receive_from(
        boost::asio::buffer(m_udpReceiveBuffer),
        m_remoteEndpointReceived,
        boost::bind(&UdpDelaySim::HandleUdpReceive, this,
            boost::asio::placeholders::error,
            boost::asio::placeholders::bytes_transferred));
}


void UdpDelaySim::HandleUdpReceive(const boost::system::error_code & error, std::size_t bytesTransferred) {
    if (!error) {
        const unsigned int writeIndex = m_circularIndexBuffer.GetIndexForWrite(); //store the volatile
        if (writeIndex == CIRCULAR_INDEX_BUFFER_FULL) {
            ++m_countCircularBufferOverruns;
            if (!m_printedCbTooSmallNotice) {
                m_printedCbTooSmallNotice = true;
                std::cout << "notice in UdpDelaySim::HandleUdpReceive(): buffers full.. you might want to increase the circular buffer size! This UDP packet will be dropped!" << std::endl;
            }
        }
        else { //not full.. swap packet in to circular buffer
            m_udpReceiveBuffer.swap(m_udpReceiveBuffersCbVec[writeIndex]);
            m_udpReceiveBytesTransferredCbVec[writeIndex] = bytesTransferred;
            m_expiriesCbVec[writeIndex] = boost::posix_time::microsec_clock::universal_time() + M_SEND_DELAY;
            m_circularIndexBuffer.CommitWrite(); //write complete at this point
            ++m_countTotalUdpPacketsReceived;
            m_countTotalUdpBytesReceived += bytesTransferred;
            const uint64_t cbSize = m_circularIndexBuffer.NumInBuffer();
            m_countMaxCircularBufferSize = std::max(m_countMaxCircularBufferSize, cbSize);
            TryRestartSendDelayTimer();
        }
        StartUdpReceive(); //restart operation only if there was no error
    }
    else if (error != boost::asio::error::operation_aborted) {
        std::cerr << "critical error in UdpDelaySim::HandleUdpReceive(): " << error.message() << std::endl;
        DoUdpShutdown();
    }
}

void UdpDelaySim::DoUdpShutdown() {
    //final code to shut down udp sockets
    m_udpPacketSendDelayTimer.cancel();
    m_timerTransferRateStats.cancel();
    if (m_udpSocket.is_open()) {
        try {
            std::cout << "closing UdpDelaySim UDP socket.." << std::endl;
            m_udpSocket.close();
        }
        catch (const boost::system::system_error & e) {
            std::cout << "notice in UdpDelaySim::DoUdpShutdown calling udpSocket.close: " << e.what() << std::endl;
        }
    }

}


//restarts the send delay timer if it is not running and there are packets available
void UdpDelaySim::TryRestartSendDelayTimer() {
    if (!m_sendDelayTimerIsRunning) {
        const unsigned int consumeIndex = m_circularIndexBuffer.GetIndexForRead(); //store the volatile
        if (consumeIndex != CIRCULAR_INDEX_BUFFER_EMPTY) { //if not empty
            m_udpPacketSendDelayTimer.expires_at(m_expiriesCbVec[consumeIndex]);
            m_udpPacketSendDelayTimer.async_wait(boost::bind(&UdpDelaySim::OnSendDelay_TimerExpired, this, boost::asio::placeholders::error, consumeIndex));
            m_sendDelayTimerIsRunning = true;
        }
    }
}

void UdpDelaySim::OnSendDelay_TimerExpired(const boost::system::error_code& e, const unsigned int consumeIndex) {
    //m_sendDelayTimerIsRunning = false; (HandleUdpSend shall set this false)
    if (e != boost::asio::error::operation_aborted) {
        // Timer was not cancelled, take necessary action.
        
        const std::vector<uint8_t> & vecDataToSend = m_udpReceiveBuffersCbVec[consumeIndex];
        const uint64_t bytesTransferred = m_udpReceiveBytesTransferredCbVec[consumeIndex];
        m_udpSocket.async_send_to(boost::asio::buffer(vecDataToSend.data(), bytesTransferred), m_udpDestinationEndpoint,
            boost::bind(&UdpDelaySim::HandleUdpSend, this,
                boost::asio::placeholders::error,
                boost::asio::placeholders::bytes_transferred));
    }
    else {
        //std::cout << "timer cancelled\n";
    }
}

void UdpDelaySim::HandleUdpSend(const boost::system::error_code& error, std::size_t bytes_transferred) {
    if (error) {
        std::cerr << "error in UdpDelaySim::HandleUdpSend: " << error.message() << std::endl;
        DoUdpShutdown();
    }
    else {
        ++m_countTotalUdpPacketsSent;
        m_countTotalUdpBytesSent += bytes_transferred;
        m_sendDelayTimerIsRunning = false;
        m_circularIndexBuffer.CommitRead();
        TryRestartSendDelayTimer();
    }
}

void UdpDelaySim::TransferRate_TimerExpired(const boost::system::error_code& e) {
    if (e != boost::asio::error::operation_aborted) {
        // Timer was not cancelled, take necessary action.
        boost::posix_time::ptime finishedTime = boost::posix_time::microsec_clock::universal_time();
        boost::posix_time::time_duration diff = finishedTime - m_lastPtime;
        const uint64_t diffTotalUdpPacketsReceived = m_countTotalUdpPacketsReceived - m_lastTotalUdpPacketsReceived;
        const uint64_t diffTotalUdpBytesReceived = m_countTotalUdpBytesReceived - m_lastTotalUdpBytesReceived;
        const uint64_t diffTotalUdpPacketsSent = m_countTotalUdpPacketsSent - m_lastTotalUdpPacketsSent;
        const uint64_t diffTotalUdpBytesSent = m_countTotalUdpBytesSent - m_lastTotalUdpBytesSent;
        if (diffTotalUdpPacketsReceived || diffTotalUdpPacketsSent) {
            const double diffTotalMicroseconds = static_cast<double>(diff.total_microseconds());
            double ratePacketsRxPerSecond = (diffTotalUdpPacketsReceived * 1e6) / diffTotalMicroseconds;
            double rateBytesRxMbps = (diffTotalUdpBytesReceived * 8.0) / diffTotalMicroseconds;
            double ratePacketsTxPerSecond = (diffTotalUdpPacketsSent * 1e6) / diffTotalMicroseconds;
            double rateBytesTxMbps = (diffTotalUdpBytesSent * 8.0) / diffTotalMicroseconds;
            const unsigned int cbSize = m_circularIndexBuffer.NumInBuffer();

            printf("RX: %0.4f Mbits/sec, %0.1f Packets/sec   TX: %0.4f Mbits/sec, %0.1f Packets/sec  Buffered: %d\n",
                rateBytesRxMbps, ratePacketsRxPerSecond, rateBytesTxMbps, ratePacketsTxPerSecond, cbSize);
        }

        m_lastTotalUdpPacketsReceived = m_countTotalUdpPacketsReceived;
        m_lastTotalUdpBytesReceived = m_countTotalUdpBytesReceived;
        m_lastTotalUdpPacketsSent = m_countTotalUdpPacketsSent;
        m_lastTotalUdpBytesSent = m_countTotalUdpBytesSent;
        m_lastPtime = finishedTime;
        m_timerTransferRateStats.expires_from_now(boost::posix_time::seconds(2));
        m_timerTransferRateStats.async_wait(boost::bind(&UdpDelaySim::TransferRate_TimerExpired, this, boost::asio::placeholders::error));
    }
    else {
        std::cout << "transfer rate timer stopped\n";
    }
}
