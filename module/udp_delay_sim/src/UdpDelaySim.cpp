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


#include "UdpDelaySim.h"
#include "Logger.h"
#include <boost/make_unique.hpp>
#include <boost/format.hpp>
#include "ThreadNamer.h"

static constexpr hdtn::Logger::SubProcess subprocess = hdtn::Logger::SubProcess::none;

UdpDelaySim::UdpDelaySim(uint16_t myBoundUdpPort,
    const std::string & remoteHostnameToForwardPacketsTo,
    const std::string & remotePortToForwardPacketsTo,
    const unsigned int numCircularBufferVectors,
    const unsigned int maxUdpPacketSizeBytes,
    const boost::posix_time::time_duration & sendDelay,
    uint64_t lossOfSignalStartMs,
    uint64_t lossOfSignalDurationMs,
    const bool autoStart) :
    m_resolver(m_ioService),
    m_udpPacketSendDelayTimer(m_ioService),
    m_timerTransferRateStats(m_ioService),
    m_timerLossOfSignal(m_ioService),
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
    m_isStateLossOfSignal(false),
    m_setUdpDropSimulatorFunctionInProgress(false),
    m_lossOfSignalStartMs(lossOfSignalStartMs),
    m_lossOfSignalDuration(boost::posix_time::milliseconds(lossOfSignalDurationMs)),
    m_countLossOfSignalTimerStarts(0),
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
            LOG_ERROR(subprocess) << "Could not bind on UDP port " << M_MY_BOUND_UDP_PORT;
            LOG_ERROR(subprocess) << "  Error: " << e.what();

            return false;
        }
        LOG_INFO(subprocess) << "UdpDelaySim bound successfully on UDP port " << m_udpSocket.local_endpoint().port();

        {
            //start timer
            m_lastPtime = boost::posix_time::microsec_clock::universal_time();
            m_timerTransferRateStats.expires_from_now(boost::posix_time::seconds(5));
            m_timerTransferRateStats.async_wait(boost::bind(&UdpDelaySim::TransferRate_TimerExpired, this, boost::asio::placeholders::error));
        }


        {
            //connect
            static const boost::asio::ip::resolver_query_base::flags UDP_RESOLVER_FLAGS = boost::asio::ip::resolver_query_base::canonical_name; //boost resolver flags
            LOG_INFO(subprocess) << "udp resolving remote " << M_REMOTE_HOSTNAME_TO_FORWARD_PACKETS_TO << ":" << M_REMOTE_PORT_TO_FORWARD_PACKETS_TO;
            boost::asio::ip::udp::resolver::query query(boost::asio::ip::udp::v4(), M_REMOTE_HOSTNAME_TO_FORWARD_PACKETS_TO, M_REMOTE_PORT_TO_FORWARD_PACKETS_TO, UDP_RESOLVER_FLAGS);
            m_resolver.async_resolve(query, boost::bind(&UdpDelaySim::OnResolve,
                this, boost::asio::placeholders::error,
                boost::asio::placeholders::results));
            m_ioServiceThreadPtr = boost::make_unique<boost::thread>(boost::bind(&boost::asio::io_service::run, &m_ioService)); //resolve serves as initial work
            ThreadNamer::SetIoServiceThreadName(m_ioService, "ioServiceUdpDelaySim");
        }
    }
    return true;
}

UdpDelaySim::~UdpDelaySim() {
    Stop();
    LOG_INFO(subprocess) << "stats:";
    LOG_INFO(subprocess) << "m_countCircularBufferOverruns " << m_countCircularBufferOverruns;
    LOG_INFO(subprocess) << "m_countMaxCircularBufferSize " << m_countMaxCircularBufferSize;
    LOG_INFO(subprocess) << "m_countTotalUdpPacketsReceived " << m_countTotalUdpPacketsReceived;
    LOG_INFO(subprocess) << "m_countTotalUdpPacketsSent " << m_countTotalUdpPacketsSent;
}

void UdpDelaySim::Stop() {

    DoUdpShutdown();
    while (m_udpSocket.is_open()) {
        try {
            boost::this_thread::sleep(boost::posix_time::milliseconds(250));
        }
        catch (const boost::thread_resource_error&) {}
        catch (const boost::thread_interrupted&) {}
        catch (const boost::condition_error&) {}
        catch (const boost::lock_error&) {}
    }

    if (m_ioServiceThreadPtr) {
        try {
            m_ioServiceThreadPtr->join();
            m_ioServiceThreadPtr.reset(); //delete it
        }
        catch (const boost::thread_resource_error&) {
            LOG_ERROR(subprocess) << "error stopping UdpDelaySim io_service";
        }
    }
    
}

void UdpDelaySim::OnResolve(const boost::system::error_code & ec, boost::asio::ip::udp::resolver::results_type results) { // Resolved endpoints as a range.
    if (ec) {
        LOG_ERROR(subprocess) << "Error resolving: " << ec.message();
    }
    else {
        m_udpDestinationEndpoint = *results;
        LOG_INFO(subprocess) << "resolved host to " << m_udpDestinationEndpoint.address() << ":" << m_udpDestinationEndpoint.port() << ".  Binding...";
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
        if (m_lossOfSignalStartMs && (m_countLossOfSignalTimerStarts == 0)) {
            //start timer for start of LOS (after first udp packet received)
            LOG_INFO(subprocess) << "LOS starting in " << m_lossOfSignalStartMs << "ms";
            ++m_countLossOfSignalTimerStarts;
            m_timerLossOfSignal.expires_from_now(boost::posix_time::milliseconds(m_lossOfSignalStartMs));
            m_timerLossOfSignal.async_wait(boost::bind(&UdpDelaySim::LossOfSignal_TimerExpired, this,
                boost::asio::placeholders::error, true));
        }

        if (m_udpDropSimulatorFunction && m_udpDropSimulatorFunction(m_udpReceiveBuffer, bytesTransferred)) {
            //dropped
        }
        else if (m_isStateLossOfSignal) {
            //dropped because in LOS
        }
        else {
            QueuePacketForDelayedSend_NotThreadSafe(m_udpReceiveBuffer, bytesTransferred);
        }
        StartUdpReceive(); //restart operation only if there was no error
    }
    else if (error != boost::asio::error::operation_aborted) {
        LOG_FATAL(subprocess) << "critical error in UdpDelaySim::HandleUdpReceive(): " << error.message();
        DoUdpShutdown();
    }
}

//udpPacketToSwapIn.size() is not the size of the udp packet but rather bytesTransferred is (see docs)
void UdpDelaySim::QueuePacketForDelayedSend_NotThreadSafe(std::vector<uint8_t>& udpPacketToSwapIn, std::size_t bytesTransferred) {
    const unsigned int writeIndex = m_circularIndexBuffer.GetIndexForWrite(); //store the volatile
    if (writeIndex == CIRCULAR_INDEX_BUFFER_FULL) {
        ++m_countCircularBufferOverruns;
        if (!m_printedCbTooSmallNotice) {
            m_printedCbTooSmallNotice = true;
            LOG_WARNING(subprocess) << "notice in UdpDelaySim::HandleUdpReceive(): buffers full.. you might want to increase the circular buffer size! This UDP packet will be dropped!";
        }
    }
    else { //not full.. swap packet in to circular buffer
        udpPacketToSwapIn.swap(m_udpReceiveBuffersCbVec[writeIndex]);
        m_udpReceiveBytesTransferredCbVec[writeIndex] = bytesTransferred;
        m_expiriesCbVec[writeIndex] = boost::posix_time::microsec_clock::universal_time() + M_SEND_DELAY;
        m_circularIndexBuffer.CommitWrite(); //write complete at this point
        ++m_countTotalUdpPacketsReceived;
        m_countTotalUdpBytesReceived += bytesTransferred;
        const uint64_t cbSize = m_circularIndexBuffer.NumInBuffer();
        m_countMaxCircularBufferSize = std::max(m_countMaxCircularBufferSize, cbSize);
        TryRestartSendDelayTimer();
    }
}

void UdpDelaySim::DoUdpShutdown() {
    //final code to shut down udp sockets
    try {
        m_udpPacketSendDelayTimer.cancel();
    }
    catch (const boost::system::system_error& e) {
        LOG_WARNING(subprocess) << "UdpDelaySim::DoUdpShutdown calling udpPacketSendDelayTimer.cancel(): " << e.what();
    }
    try {
        m_timerLossOfSignal.cancel();
    }
    catch (const boost::system::system_error& e) {
        LOG_WARNING(subprocess) << "UdpDelaySim::DoUdpShutdown calling timerLossOfSignal.cancel(): " << e.what();
    }
    try {
        m_timerTransferRateStats.cancel();
    }
    catch (const boost::system::system_error& e) {
        LOG_WARNING(subprocess) << "UdpDelaySim::DoUdpShutdown calling timerTransferRateStats.cancel(): " << e.what();
    }
    if (m_udpSocket.is_open()) {
        try {
            LOG_INFO(subprocess) << "closing UdpDelaySim UDP socket..";
            m_udpSocket.close();
        }
        catch (const boost::system::system_error & e) {
            LOG_WARNING(subprocess) << "notice in UdpDelaySim::DoUdpShutdown calling udpSocket.close: " << e.what();
        }
    }

}

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
}

void UdpDelaySim::HandleUdpSend(const boost::system::error_code& error, std::size_t bytes_transferred) {
    if (error) {
        LOG_ERROR(subprocess) << "UdpDelaySim::HandleUdpSend: " << error.message();
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

            static const boost::format fmtTemplate("RX: %0.4f Mbits/sec, %0.1f Packets/sec   TX: %0.4f Mbits/sec, %0.1f Packets/sec  Buffered: %d");
            boost::format fmt(fmtTemplate);
            fmt % rateBytesRxMbps % ratePacketsRxPerSecond % rateBytesTxMbps % ratePacketsTxPerSecond % cbSize;
            LOG_INFO(subprocess) << fmt.str();
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
        LOG_INFO(subprocess) << "transfer rate timer stopped";
    }
}

void UdpDelaySim::LossOfSignal_TimerExpired(const boost::system::error_code& e, bool isStartOfLos) {
    if (e != boost::asio::error::operation_aborted) {
        // Timer was not cancelled, take necessary action.
        m_isStateLossOfSignal = isStartOfLos;
        if (isStartOfLos) {
            LOG_INFO(subprocess) << "Entering LOS for " << m_lossOfSignalDuration.total_milliseconds() << "ms";

            //start timer for end of LOS
            m_timerLossOfSignal.expires_from_now(m_lossOfSignalDuration);
            m_timerLossOfSignal.async_wait(boost::bind(&UdpDelaySim::LossOfSignal_TimerExpired, this,
                boost::asio::placeholders::error, false));
        }
        else {
            LOG_INFO(subprocess) << "Entering AOS";
        }
    }
    else {
        LOG_INFO(subprocess) << "loss of signal timer cancelled";
    }
}

void UdpDelaySim::SetUdpDropSimulatorFunction_ThreadSafe(const UdpDropSimulatorFunction_t & udpDropSimulatorFunction) {
    boost::mutex::scoped_lock cvLock(m_mutexSetUdpDropSimulatorFunction);
    m_setUdpDropSimulatorFunctionInProgress = true;
    boost::asio::post(m_ioService, boost::bind(&UdpDelaySim::SetUdpDropSimulatorFunction, this, udpDropSimulatorFunction));
    while (m_setUdpDropSimulatorFunctionInProgress) { //lock mutex (above) before checking condition
        m_cvSetUdpDropSimulatorFunction.wait(cvLock);
    }    
}
void UdpDelaySim::SetUdpDropSimulatorFunction(const UdpDropSimulatorFunction_t & udpDropSimulatorFunction) {
    m_udpDropSimulatorFunction = udpDropSimulatorFunction;
    m_mutexSetUdpDropSimulatorFunction.lock();
    m_setUdpDropSimulatorFunctionInProgress = false;
    m_mutexSetUdpDropSimulatorFunction.unlock();
    m_cvSetUdpDropSimulatorFunction.notify_one();
}
