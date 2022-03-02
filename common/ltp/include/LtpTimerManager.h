#ifndef LTP_TIMER_MANAGER_H
#define LTP_TIMER_MANAGER_H 1

#include <map>
#include <list>
#include <boost/asio.hpp>
#include <boost/function.hpp>
#include "ltp_lib_export.h"

//Single threaded class designed to run and be called from ioService thread only


template <typename idType>
class LtpTimerManager {
private:
    LtpTimerManager();
public:
    typedef boost::function<void(idType serialNumber, std::vector<uint8_t> & userData)> LtpTimerExpiredCallback_t;
    LTP_LIB_EXPORT LtpTimerManager(boost::asio::io_service & ioService, const boost::posix_time::time_duration & oneWayLightTime, const boost::posix_time::time_duration & oneWayMarginTime, const LtpTimerExpiredCallback_t & callback);
    LTP_LIB_EXPORT ~LtpTimerManager();
    LTP_LIB_EXPORT void Reset();
       
    LTP_LIB_EXPORT bool StartTimer(const idType serialNumber, std::vector<uint8_t> userData = std::vector<uint8_t>());
    LTP_LIB_EXPORT bool DeleteTimer(const idType serialNumber);
    LTP_LIB_EXPORT bool DeleteTimer(const idType serialNumber, std::vector<uint8_t> & userDataReturned);
    LTP_LIB_EXPORT bool Empty() const;
    //std::vector<uint8_t> & GetUserDataRef(const uint64_t serialNumber);
private:
    LTP_LIB_NO_EXPORT void OnTimerExpired(const boost::system::error_code& e, bool * isTimerDeleted);
private:
    boost::asio::deadline_timer m_deadlineTimer;
    const boost::posix_time::time_duration M_ONE_WAY_LIGHT_TIME;
    const boost::posix_time::time_duration M_ONE_WAY_MARGIN_TIME;
    const boost::posix_time::time_duration M_TRANSMISSION_TO_ACK_RECEIVED_TIME;
    const LtpTimerExpiredCallback_t m_ltpTimerExpiredCallbackFunction;
    //boost::bimap<idType, boost::posix_time::ptime> m_bimapCheckpointSerialNumberToExpiry;
    //std::map<idType, std::vector<uint8_t> > m_mapSerialNumberToUserData;
    typedef std::pair<idType, boost::posix_time::ptime> id_ptime_pair_t;
    typedef std::list<id_ptime_pair_t> id_ptime_list_t;
    typedef std::pair<typename id_ptime_list_t::iterator, std::vector<uint8_t> > listiterator_userdata_pair_t;
    typedef std::map<idType, listiterator_userdata_pair_t> id_to_listiteratorplususerdata_map_t;
    typedef std::pair<idType, listiterator_userdata_pair_t> id_to_listiteratorplususerdata_map_insertion_element_t;
    id_ptime_list_t m_listCheckpointSerialNumberPlusExpiry;
    id_to_listiteratorplususerdata_map_t m_mapCheckpointSerialNumberToExpiryListIteratorPlusUserData;
    

    idType m_activeSerialNumberBeingTimed;
    bool m_isTimerActive;
    bool * m_timerIsDeletedPtr;
};

#endif // LTP_TIMER_MANAGER_H

