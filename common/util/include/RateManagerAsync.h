#ifndef RATE_MANAGER_ASYNC_H
#define RATE_MANAGER_ASYNC_H 1

#include <boost/asio.hpp>
#include <boost/function.hpp>
#include <boost/thread.hpp>
#include <cstdint>
#include "CircularIndexBufferSingleProducerSingleConsumerConfigurable.h"

typedef boost::function<void()> PacketsSentCallback_t;

class RateManagerAsync {
private:
    RateManagerAsync();
public:
    RateManagerAsync(boost::asio::io_service & ioService, const uint64_t rateBitsPerSec, const uint64_t maxPacketsBeingSent);
    ~RateManagerAsync();

    void Reset();
    
    std::size_t GetTotalPacketsCompletelySent();
    std::size_t GetTotalPacketsDequeuedForSend();
    std::size_t GetTotalPacketsBeingSent();
    std::size_t GetTotalBytesCompletelySent();
    std::size_t GetTotalBytesDequeuedForSend();
    std::size_t GetTotalBytesBeingSent();

    void WaitForAllDequeuedPacketsToFullySend_Blocking(unsigned int timeoutSeconds = 1000, bool printStats = false);
    void WaitForAvailabilityToSendPacket_Blocking(unsigned int timeoutSeconds = 1000);
    bool HasAvailabilityToSendPacket();
    void SetPacketsSentCallback(const PacketsSentCallback_t & callback);
    void SetRate(uint64_t rateBitsPerSec);
    void NotifyPacketSentFromCallback_ThreadSafe(std::size_t bytes_transferred);
    bool IoServiceThreadNotifyPacketSentCallback(std::size_t bytes_transferred);
    bool SignalNewPacketDequeuedForSend(uint64_t packetSizeBytes);
private:
   
    void TryRestartRateTimer();
    void OnRate_TimerExpired(const boost::system::error_code& e);
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

