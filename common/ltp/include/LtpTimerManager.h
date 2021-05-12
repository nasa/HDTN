#ifndef LTP_TIMER_MANAGER_H
#define LTP_TIMER_MANAGER_H 1

#include <boost/bimap.hpp>
#include <boost/asio.hpp>
#include <boost/function.hpp>

//Single threaded class designed to run and be called from ioService thread only

typedef boost::function<void(uint64_t serialNumber)> LtpTimerExpiredCallback_t;

class LtpTimerManager {
private:
    LtpTimerManager();
public:
    LtpTimerManager(boost::asio::io_service & ioService, const boost::posix_time::time_duration & oneWayLightTime, const boost::posix_time::time_duration & oneWayMarginTime, const LtpTimerExpiredCallback_t & callback);
    ~LtpTimerManager();
    void Reset();
       
    bool StartTimer(const uint64_t serialNumber);
    bool DeleteTimer(const uint64_t serialNumber);
private:
    void OnTimerExpired(const boost::system::error_code& e);
private:
    boost::asio::deadline_timer m_deadlineTimer;
    const boost::posix_time::time_duration M_ONE_WAY_LIGHT_TIME;
    const boost::posix_time::time_duration M_ONE_WAY_MARGIN_TIME;
    const boost::posix_time::time_duration M_TRANSMISSION_TO_ACK_RECEIVED_TIME;
    const LtpTimerExpiredCallback_t m_ltpTimerExpiredCallbackFunction;
    boost::bimap<uint64_t, boost::posix_time::ptime> m_bimapCheckpointSerialNumberToExpiry;
    uint64_t m_activeSerialNumberBeingTimed;
    bool m_isTimerActive;
};

#endif // LTP_TIMER_MANAGER_H

