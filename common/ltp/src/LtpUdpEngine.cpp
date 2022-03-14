#include "LtpUdpEngine.h"
#include <boost/make_unique.hpp>

static const boost::posix_time::time_duration static_tokenMaxLimitDurationWindow(boost::posix_time::milliseconds(100));
static const boost::posix_time::time_duration static_tokenRefreshTimeDurationWindow(boost::posix_time::milliseconds(20));

LtpUdpEngine::LtpUdpEngine(boost::asio::io_service & ioServiceUdpRef, boost::asio::ip::udp::socket & udpSocketRef,
    const uint64_t thisEngineId, const uint8_t engineIndexForEncodingIntoRandomSessionNumber,
    const uint64_t mtuClientServiceData, uint64_t mtuReportSegment,
    const boost::posix_time::time_duration & oneWayLightTime, const boost::posix_time::time_duration & oneWayMarginTime,
    const boost::asio::ip::udp::endpoint & remoteEndpoint, const unsigned int numUdpRxCircularBufferVectors,
    const uint64_t ESTIMATED_BYTES_TO_RECEIVE_PER_SESSION, const uint64_t maxRedRxBytesPerSession, uint32_t checkpointEveryNthDataPacketSender,
    uint32_t maxRetriesPerSerialNumber, const bool force32BitRandomNumbers, const uint64_t maxUdpRxPacketSizeBytes, const uint64_t maxSendRateBitsPerSecOrZeroToDisable) :
    LtpEngine(thisEngineId, engineIndexForEncodingIntoRandomSessionNumber, mtuClientServiceData, mtuReportSegment, oneWayLightTime, oneWayMarginTime,
        ESTIMATED_BYTES_TO_RECEIVE_PER_SESSION, maxRedRxBytesPerSession, true, checkpointEveryNthDataPacketSender, maxRetriesPerSerialNumber, force32BitRandomNumbers),
    m_ioServiceUdpRef(ioServiceUdpRef),
    m_udpSocketRef(udpSocketRef),
    m_tokenRefreshTimer(m_ioServiceUdpRef),
    m_maxSendRateBitsPerSecOrZeroToDisable(maxSendRateBitsPerSecOrZeroToDisable),
    m_tokenRefreshTimerIsRunning(false),
    m_lastTimeTokensWereRefreshed(boost::posix_time::special_values::neg_infin),
    m_remoteEndpoint(remoteEndpoint),
    M_NUM_CIRCULAR_BUFFER_VECTORS(numUdpRxCircularBufferVectors),
    M_MAX_UDP_RX_PACKET_SIZE_BYTES(maxUdpRxPacketSizeBytes),
    m_circularIndexBuffer(M_NUM_CIRCULAR_BUFFER_VECTORS),
    m_udpReceiveBuffersCbVec(M_NUM_CIRCULAR_BUFFER_VECTORS),
    m_printedCbTooSmallNotice(false),
    m_countAsyncSendCalls(0),
    m_countAsyncSendCallbackCalls(0),
    m_countAsyncSendsLimitedByRate(0),
    m_countCircularBufferOverruns(0)
{
    for (unsigned int i = 0; i < M_NUM_CIRCULAR_BUFFER_VECTORS; ++i) {
        m_udpReceiveBuffersCbVec[i].resize(maxUdpRxPacketSizeBytes);
    }

    UpdateRate(m_maxSendRateBitsPerSecOrZeroToDisable);
    
    if (maxSendRateBitsPerSecOrZeroToDisable) {
        const uint64_t tokenLimit = m_tokenRateLimiter.GetRemainingTokens();
        std::cout << "LtpUdpEngine: rate bitsPerSec = " << m_maxSendRateBitsPerSecOrZeroToDisable << "  token limit = " << tokenLimit << "\n";
    }
}

LtpUdpEngine::~LtpUdpEngine() {
    //std::cout << "end of ~LtpUdpEngine with port " << M_MY_BOUND_UDP_PORT << std::endl;
    std::cout << "~LtpUdpEngine: m_countAsyncSendCalls " << m_countAsyncSendCalls
        << "\n     m_countCircularBufferOverruns " << m_countCircularBufferOverruns
        << "\n     m_countAsyncSendsLimitedByRate " << m_countAsyncSendsLimitedByRate << std::endl;
}


void LtpUdpEngine::Reset() {
    LtpEngine::Reset();
    m_countAsyncSendCalls = 0;
    m_countAsyncSendCallbackCalls = 0;
    m_countCircularBufferOverruns = 0;
}


void LtpUdpEngine::PostPacketFromManager_ThreadSafe(std::vector<uint8_t> & packetIn_thenSwappedForAnotherSameSizeVector, std::size_t size) {
    const unsigned int writeIndex = m_circularIndexBuffer.GetIndexForWrite(); //store the volatile
    if (writeIndex == UINT32_MAX) {
        ++m_countCircularBufferOverruns;
        if (!m_printedCbTooSmallNotice) {
            m_printedCbTooSmallNotice = true;
            std::cout << "notice in LtpUdpEngine::StartUdpReceive(): buffers full.. you might want to increase the circular buffer size! Next UDP packet will be dropped!" << std::endl;
        }
    }
    else {
        packetIn_thenSwappedForAnotherSameSizeVector.swap(m_udpReceiveBuffersCbVec[writeIndex]);
        m_circularIndexBuffer.CommitWrite(); //write complete at this point
        PacketIn_ThreadSafe(m_udpReceiveBuffersCbVec[writeIndex].data(), size); //Post to the LtpEngine IoService so its thread will process
    }
}

void LtpUdpEngine::SendPacket(std::vector<boost::asio::const_buffer> & constBufferVec, boost::shared_ptr<std::vector<std::vector<uint8_t> > > & underlyingDataToDeleteOnSentCallback, const uint64_t sessionOriginatorEngineId) {
    //called by LtpEngine Thread
    ++m_countAsyncSendCalls;
    if (m_udpDropSimulatorFunction && m_udpDropSimulatorFunction(*((uint8_t*)constBufferVec[0].data()))) {
        boost::asio::post(m_ioServiceUdpRef, boost::bind(&LtpUdpEngine::HandleUdpSend, this, underlyingDataToDeleteOnSentCallback, boost::system::error_code(), 0));
    }
    else {
        m_udpSocketRef.async_send_to(constBufferVec, m_remoteEndpoint,
            boost::bind(&LtpUdpEngine::HandleUdpSend, this, std::move(underlyingDataToDeleteOnSentCallback),
                boost::asio::placeholders::error,
                boost::asio::placeholders::bytes_transferred));
    }
}


void LtpUdpEngine::PacketInFullyProcessedCallback(bool success) {
    //Called by LTP Engine thread
    //std::cout << "PacketInFullyProcessedCallback " << std::endl;
    m_circularIndexBuffer.CommitRead(); //LtpEngine IoService thread will CommitRead
}



void LtpUdpEngine::HandleUdpSend(boost::shared_ptr<std::vector<std::vector<uint8_t> > > & underlyingDataToDeleteOnSentCallback, const boost::system::error_code& error, std::size_t bytes_transferred) {
    ++m_countAsyncSendCallbackCalls;
    if (error) {
        std::cerr << "error in LtpUdpEngine::HandleUdpSend: " << error.message() << std::endl;
        //DoUdpShutdown();
    }
    else {
        //RATE STUFF (the HandleUdpSend and OnTokenRefresh_TimerExpired run in the same thread because the udp socket and token refresh timer share the same io_service)
        if (m_maxSendRateBitsPerSecOrZeroToDisable) {
            TryRestartTokenRefreshTimer(); //make sure this is running so that tokens can be replenished
            if (!m_tokenRateLimiter.TakeTokens(bytes_transferred)) { //no tokens available for next send, empty the rate limited bytes queue at the next m_tokenRefreshTimer expiration
                m_queueRateLimitedBytes.emplace(bytes_transferred);
                return;
            }
        }
        //std::cout << "sent " << bytes_transferred << std::endl;

        if (m_countAsyncSendCallbackCalls == m_countAsyncSendCalls) { //prevent too many sends from stacking up in ioService queue
            SignalReadyForSend_ThreadSafe();
        }
    }
}

void LtpUdpEngine::UpdateRate(const uint64_t maxSendRateBitsPerSecOrZeroToDisable) {
    m_maxSendRateBitsPerSecOrZeroToDisable = maxSendRateBitsPerSecOrZeroToDisable;
    if (maxSendRateBitsPerSecOrZeroToDisable) {
        const uint64_t rateBytesPerSecond = m_maxSendRateBitsPerSecOrZeroToDisable >> 3;
        m_tokenRateLimiter.SetRate(
            rateBytesPerSecond,
            boost::posix_time::seconds(1),
            static_tokenMaxLimitDurationWindow //token limit of rateBytesPerSecond / (1000ms/100ms) = rateBytesPerSecond / 10
        );
    }
}

//restarts the token refresh timer if it is not running from now
void LtpUdpEngine::TryRestartTokenRefreshTimer() {
    if (!m_tokenRefreshTimerIsRunning) {
        const boost::posix_time::ptime nowPtime = boost::posix_time::microsec_clock::universal_time();
        if (m_lastTimeTokensWereRefreshed.is_neg_infinity()) {
            m_lastTimeTokensWereRefreshed = nowPtime;
        }
        m_tokenRefreshTimer.expires_at(nowPtime + static_tokenRefreshTimeDurationWindow);
        m_tokenRefreshTimer.async_wait(boost::bind(&LtpUdpEngine::OnTokenRefresh_TimerExpired, this, boost::asio::placeholders::error));
        m_tokenRefreshTimerIsRunning = true;
    }
}
//restarts the token refresh timer if it is not running from the given ptime
void LtpUdpEngine::TryRestartTokenRefreshTimer(const boost::posix_time::ptime & nowPtime) {
    if (!m_tokenRefreshTimerIsRunning) {
        if (m_lastTimeTokensWereRefreshed.is_neg_infinity()) {
            m_lastTimeTokensWereRefreshed = nowPtime;
        }
        m_tokenRefreshTimer.expires_at(nowPtime + static_tokenRefreshTimeDurationWindow);
        m_tokenRefreshTimer.async_wait(boost::bind(&LtpUdpEngine::OnTokenRefresh_TimerExpired, this, boost::asio::placeholders::error));
        m_tokenRefreshTimerIsRunning = true;
    }
}

void LtpUdpEngine::OnTokenRefresh_TimerExpired(const boost::system::error_code& e) {
    const boost::posix_time::ptime nowPtime = boost::posix_time::microsec_clock::universal_time();
    const boost::posix_time::time_duration diff = nowPtime - m_lastTimeTokensWereRefreshed;
    m_tokenRateLimiter.AddTime(diff);
    m_lastTimeTokensWereRefreshed = nowPtime;
    m_tokenRefreshTimerIsRunning = false;
    if (e != boost::asio::error::operation_aborted) {
        // Timer was not cancelled, take necessary action.
        while (!m_queueRateLimitedBytes.empty()) {
            const std::size_t rateLimitedBytes = m_queueRateLimitedBytes.front();
            //empty the queue of rate limited packets
            if (m_tokenRateLimiter.TakeTokens(rateLimitedBytes)) { //there are tokens available, send this now
                m_queueRateLimitedBytes.pop();
                ++m_countAsyncSendsLimitedByRate;
            }
            else { //no tokens available, empty the rate limited bytes queue at the next m_tokenRefreshTimer expiration
                TryRestartTokenRefreshTimer(nowPtime);
                return;
            }
        }
        //If more tokens can be added, restart the timer so more tokens will be added at the next timer expiration.
        //Otherwise, if full, don't restart the timer and the next send packet operation will start it.
        if (!m_tokenRateLimiter.HasFullBucketOfTokens()) {
            TryRestartTokenRefreshTimer(nowPtime);
        }

        //queue is empty at this point, now safe to signal the ltp engine thread that more data can be sent
        if (m_countAsyncSendCallbackCalls == m_countAsyncSendCalls) { //prevent too many sends from stacking up in ioService queue
            SignalReadyForSend_ThreadSafe();
        }
    }
    else {
        //std::cout << "timer cancelled\n";
    }
}
