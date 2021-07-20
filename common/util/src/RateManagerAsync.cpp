#include "RateManagerAsync.h"
#include <iostream>
#include <boost/bind.hpp>

RateManagerAsync::RateManagerAsync(boost::asio::io_service & ioService, const uint64_t rateBitsPerSec, const uint64_t maxPacketsBeingSent) :
    m_ioServiceRef(ioService),
    m_rateTimer(ioService),
    m_rateBitsPerSec(rateBitsPerSec),
    m_maxPacketsBeingSent(maxPacketsBeingSent),
    m_bytesToAckByRateCb(static_cast<uint32_t>(m_maxPacketsBeingSent + 10)),
    m_bytesToAckByRateCbVec(m_maxPacketsBeingSent + 10),
    m_bytesToAckBySentCallbackCb(static_cast<uint32_t>(m_maxPacketsBeingSent + 10)),
    m_bytesToAckBySentCallbackCbVec(m_maxPacketsBeingSent + 10)
    
{

    Reset();
}

RateManagerAsync::~RateManagerAsync() {
    //Reset();

    
    //print stats
    std::cout << "m_totalPacketsSentBySentCallback " << m_totalPacketsSentBySentCallback << std::endl;
    std::cout << "m_totalBytesSentBySentCallback " << m_totalBytesSentBySentCallback << std::endl;
    std::cout << "m_totalPacketsSentByRate " << m_totalPacketsSentByRate << std::endl;
    std::cout << "m_totalBytesSentByRate " << m_totalBytesSentByRate << std::endl;
    std::cout << "m_totalPacketsDequeuedForSend " << m_totalPacketsDequeuedForSend << std::endl;
    std::cout << "m_totalBytesDequeuedForSend " << m_totalBytesDequeuedForSend << std::endl;
}

void RateManagerAsync::Reset() {
    m_totalPacketsSentBySentCallback = 0;
    m_totalBytesSentBySentCallback = 0;
    m_totalPacketsSentByRate = 0;
    m_totalBytesSentByRate = 0;
    m_totalPacketsDequeuedForSend = 0;
    m_totalBytesDequeuedForSend = 0;
    m_rateTimerIsRunning = false;
}
void RateManagerAsync::SetRate(uint64_t rateBitsPerSec) {
    m_rateBitsPerSec = rateBitsPerSec;
}

void RateManagerAsync::SetPacketsSentCallback(const PacketsSentCallback_t & callback) {
    m_packetsSentCallback = callback;
}

std::size_t RateManagerAsync::GetTotalPacketsCompletelySent() {
    const std::size_t totalSentBySentCallback = m_totalPacketsSentBySentCallback;
    const std::size_t totalSentByRate = m_totalPacketsSentByRate;
    return std::min(totalSentBySentCallback, totalSentByRate);
}

std::size_t RateManagerAsync::GetTotalPacketsDequeuedForSend() {
    return m_totalPacketsDequeuedForSend;
}

std::size_t RateManagerAsync::GetTotalPacketsBeingSent() {
    return GetTotalPacketsDequeuedForSend() - GetTotalPacketsCompletelySent();
}

std::size_t RateManagerAsync::GetTotalBytesCompletelySent() {
    const std::size_t totalSentBySentCallback = m_totalBytesSentBySentCallback;
    const std::size_t totalSentByRate = m_totalBytesSentByRate;
    return std::min(totalSentBySentCallback, totalSentByRate);
}

std::size_t RateManagerAsync::GetTotalBytesDequeuedForSend() {
    return m_totalBytesDequeuedForSend;
}

std::size_t RateManagerAsync::GetTotalBytesBeingSent() {
    return GetTotalBytesDequeuedForSend() - GetTotalBytesCompletelySent();
}

bool RateManagerAsync::SignalNewPacketDequeuedForSend(uint64_t packetSizeBytes) {
    const unsigned int writeIndexRate = m_bytesToAckByRateCb.GetIndexForWrite(); //don't put this in tcp async write callback
    if (writeIndexRate == UINT32_MAX) { //push check
        std::cerr << "Error in RateManagerAsync::SignalNewPacketDequeuedForSend.. too many unacked packets by rate" << std::endl;
        return false;
    }

    const unsigned int writeIndexSentCallback = m_bytesToAckBySentCallbackCb.GetIndexForWrite(); //don't put this in tcp async write callback
    if (writeIndexSentCallback == UINT32_MAX) { //push check
        std::cerr << "Error in RateManagerAsync::SignalNewPacketDequeuedForSend.. too many unacked packets by tcp send callback" << std::endl;
        return false;
    }


    ++m_totalPacketsDequeuedForSend;

    m_bytesToAckByRateCbVec[writeIndexRate] = packetSizeBytes;
    m_bytesToAckByRateCb.CommitWrite(); //pushed

    m_bytesToAckBySentCallbackCbVec[writeIndexSentCallback] = packetSizeBytes;
    m_bytesToAckBySentCallbackCb.CommitWrite(); //pushed

    boost::asio::post(m_ioServiceRef, boost::bind(&RateManagerAsync::TryRestartRateTimer, this)); //keep TryRestartRateTimer within ioservice thread
    return true;
}

//restarts the rate timer if there is a pending ack in the cb
void RateManagerAsync::TryRestartRateTimer() {
    if (!m_rateTimerIsRunning && (m_groupingOfBytesToAckByRateVec.size() == 0)) {
        uint64_t delayMicroSec = 0;
        for (unsigned int readIndex = m_bytesToAckByRateCb.GetIndexForRead(); readIndex != UINT32_MAX; readIndex = m_bytesToAckByRateCb.GetIndexForRead()) { //notempty
            const double numBitsDouble = static_cast<double>(m_bytesToAckByRateCbVec[readIndex]) * 8.0;
            const double delayMicroSecDouble = (1e6 / m_rateBitsPerSec) * numBitsDouble;
            delayMicroSec += static_cast<uint64_t>(delayMicroSecDouble);
            m_groupingOfBytesToAckByRateVec.push_back(m_bytesToAckByRateCbVec[readIndex]);
            m_bytesToAckByRateCb.CommitRead();
            if (delayMicroSec >= 10000) { //try to avoid sleeping for any time smaller than 10 milliseconds
                break;
            }
        }
        if (m_groupingOfBytesToAckByRateVec.size()) {
            //std::cout << "d " << delayMicroSec << " sz " << m_groupingOfBytesToAckByRateVec.size() << std::endl;
            m_rateTimer.expires_from_now(boost::posix_time::microseconds(delayMicroSec));
            m_rateTimer.async_wait(boost::bind(&RateManagerAsync::OnRate_TimerExpired, this, boost::asio::placeholders::error));
            m_rateTimerIsRunning = true;
        }
    }
}

void RateManagerAsync::OnRate_TimerExpired(const boost::system::error_code& e) {
    m_rateTimerIsRunning = false;
    if (e != boost::asio::error::operation_aborted) {
        // Timer was not cancelled, take necessary action.
        if (m_groupingOfBytesToAckByRateVec.size() > 0) {
            m_totalPacketsSentByRate += m_groupingOfBytesToAckByRateVec.size();
            for (std::size_t i = 0; i < m_groupingOfBytesToAckByRateVec.size(); ++i) {
                m_totalBytesSentByRate += m_groupingOfBytesToAckByRateVec[i];
            }
            m_groupingOfBytesToAckByRateVec.clear();

            if (m_totalPacketsSentByRate <= m_totalPacketsSentBySentCallback) { //tcp send callback segments ahead
                if (m_packetsSentCallback) {
                    m_packetsSentCallback();
                }
                
                m_conditionVariablePacketSent.notify_one();
                
            }
            TryRestartRateTimer(); //must be called after commit read
        }
        else {
            std::cerr << "error in RateManagerAsync::OnRate_TimerExpired: m_groupingOfBytesToAckByRateVec is size 0" << std::endl;
        }
    }
    else {
        //std::cout << "timer cancelled\n";
    }
}

void RateManagerAsync::NotifyPacketSentFromCallback_ThreadSafe(std::size_t bytes_transferred) {
    boost::asio::post(m_ioServiceRef, boost::bind(&RateManagerAsync::IoServiceThreadNotifyPacketSentCallback, this, bytes_transferred));
}

bool RateManagerAsync::IoServiceThreadNotifyPacketSentCallback(std::size_t bytes_transferred) {
   
    const unsigned int readIndex = m_bytesToAckBySentCallbackCb.GetIndexForRead();
    if (readIndex == UINT32_MAX) { //empty
        std::cerr << "error in RateManagerAsync::IoServiceThreadNotifyPacketSentCallback: AckCallback called with empty queue" << std::endl;
        return false;
    }
    else if (m_bytesToAckBySentCallbackCbVec[readIndex] == bytes_transferred) {
        ++m_totalPacketsSentBySentCallback;
        m_totalBytesSentBySentCallback += m_bytesToAckBySentCallbackCbVec[readIndex];
        m_bytesToAckBySentCallbackCb.CommitRead();
        if (m_totalPacketsSentBySentCallback <= m_totalPacketsSentByRate) { //rate segments ahead
            if (m_packetsSentCallback) {
                m_packetsSentCallback();
            }
            m_conditionVariablePacketSent.notify_one();
        }
        return true;
    }
    else {
        std::cerr << "error in RateManagerAsync::IoServiceThreadNotifyPacketSentCallback: wrong bytes acked: expected " << m_bytesToAckBySentCallbackCbVec[readIndex] << " but got " << bytes_transferred << std::endl;
        return false;
    }
}

void RateManagerAsync::WaitForAllDequeuedPacketsToFullySend_Blocking(unsigned int timeoutSeconds, bool printStats) {
    std::size_t previousUnacked = std::numeric_limits<std::size_t>::max();
    const unsigned int maxAttempts = (timeoutSeconds * 2);
    for (unsigned int attempt = 0; attempt < maxAttempts; ++attempt) {
        const std::size_t numUnacked = GetTotalPacketsBeingSent();
        if (numUnacked) {
            if (printStats) {
                std::cout << "notice: waiting on " << numUnacked << " unacked bundles" << std::endl;
            }
            if (previousUnacked > numUnacked) {
                previousUnacked = numUnacked;
                attempt = 0;
            }
            boost::mutex::scoped_lock lock(m_cvMutex);
            m_conditionVariablePacketSent.timed_wait(lock, boost::posix_time::milliseconds(500)); // call lock.unlock() and blocks the current thread
            //thread is now unblocked, and the lock is reacquired by invoking lock.lock()
            continue;
        }
        break;
    }
}

void RateManagerAsync::WaitForAvailabilityToSendPacket_Blocking(unsigned int timeoutSeconds) {

    const unsigned int maxAttempts = (timeoutSeconds * 2);
    for (unsigned int attempt = 0; attempt < maxAttempts; ++attempt) {
        const std::size_t numPacketsBeingSent = GetTotalPacketsBeingSent();
        
        if (!HasAvailabilityToSendPacket()) {
            boost::mutex::scoped_lock lock(m_cvMutex);
            m_conditionVariablePacketSent.timed_wait(lock, boost::posix_time::milliseconds(500)); // call lock.unlock() and blocks the current thread
            //thread is now unblocked, and the lock is reacquired by invoking lock.lock()
            continue;
        }
        break;
    }
}

bool RateManagerAsync::HasAvailabilityToSendPacket() {
    return (GetTotalPacketsBeingSent() <= m_maxPacketsBeingSent);
}