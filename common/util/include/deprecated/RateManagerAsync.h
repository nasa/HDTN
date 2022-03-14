#ifndef RATE_MANAGER_ASYNC_H
#define RATE_MANAGER_ASYNC_H 1

#include <boost/asio.hpp>
#include <boost/function.hpp>
#include <boost/thread.hpp>
#include <cstdint>
#include "CircularIndexBufferSingleProducerSingleConsumerConfigurable.h"
#include "hdtn_util_export.h"

typedef boost::function<void()> PacketsSentCallback_t;

class RateManagerAsync {
private:
    RateManagerAsync();
public:
    HDTN_UTIL_EXPORT RateManagerAsync(boost::asio::io_service & ioService, const uint64_t rateBitsPerSec, const uint64_t maxPacketsBeingSent);
    HDTN_UTIL_EXPORT ~RateManagerAsync();

    HDTN_UTIL_EXPORT void Reset();
    
    HDTN_UTIL_EXPORT std::size_t GetTotalPacketsCompletelySent();
    HDTN_UTIL_EXPORT std::size_t GetTotalPacketsDequeuedForSend();
    HDTN_UTIL_EXPORT std::size_t GetTotalPacketsBeingSent();
    HDTN_UTIL_EXPORT std::size_t GetTotalBytesCompletelySent();
    HDTN_UTIL_EXPORT std::size_t GetTotalBytesDequeuedForSend();
    HDTN_UTIL_EXPORT std::size_t GetTotalBytesBeingSent();

    HDTN_UTIL_EXPORT void WaitForAllDequeuedPacketsToFullySend_Blocking(unsigned int timeoutSeconds = 1000, bool printStats = false);
    HDTN_UTIL_EXPORT void WaitForAvailabilityToSendPacket_Blocking(unsigned int timeoutSeconds = 1000);
    HDTN_UTIL_EXPORT bool HasAvailabilityToSendPacket();
    HDTN_UTIL_EXPORT void SetPacketsSentCallback(const PacketsSentCallback_t & callback);
    HDTN_UTIL_EXPORT void SetRate(uint64_t rateBitsPerSec);
    HDTN_UTIL_EXPORT void NotifyPacketSentFromCallback_ThreadSafe(std::size_t bytes_transferred);
    HDTN_UTIL_EXPORT bool IoServiceThreadNotifyPacketSentCallback(std::size_t bytes_transferred);
    HDTN_UTIL_EXPORT bool SignalNewPacketDequeuedForSend(uint64_t packetSizeBytes);
private:
   
    HDTN_UTIL_EXPORT void TryRestartRateTimer();
    HDTN_UTIL_EXPORT void OnRate_TimerExpired(const boost::system::error_code& e);
private:
    boost::asio::io_service & m_ioServiceRef;
    boost::asio::deadline_timer m_rateTimer;
    uint64_t m_rateBitsPerSec;
    uint64_t m_maxPacketsBeingSent;

    CircularIndexBufferSingleProducerSingleConsumerConfigurable m_bytesToAckByRateCb;
    std::vector<std::size_t> m_bytesToAckByRateCbVec;
    std::vector<std::size_t> m_groupingOfBytesToAckByRateVec;
    CircularIndexBufferSingleProducerSingleConsumerConfigurable m_bytesToAckBySentCallbackCb;
    std::vector<std::size_t> m_bytesToAckBySentCallbackCbVec;
    boost::mutex m_cvMutex;
    boost::condition_variable m_conditionVariablePacketSent;

    PacketsSentCallback_t m_packetsSentCallback;

    std::size_t m_totalPacketsSentBySentCallback;
    std::size_t m_totalBytesSentBySentCallback;
    std::size_t m_totalPacketsSentByRate;
    std::size_t m_totalBytesSentByRate;
    std::size_t m_totalPacketsDequeuedForSend;
    std::size_t m_totalBytesDequeuedForSend;
    volatile bool m_rateTimerIsRunning;
};

#endif // RATE_MANAGER_ASYNC_H

