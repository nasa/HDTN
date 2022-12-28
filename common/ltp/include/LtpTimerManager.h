/**
 * @file LtpTimerManager.h
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
 *
 * @section DESCRIPTION
 *
 * This LtpTimerManager templated class encapsulates a shared boost::asio::deadline_timer
 * for use with all LTP sessions within a single-threaded LTP sender "exclusive or" receiver engine.
 * The class uses/shares the user's provided boost::asio::deadline_timer
 * and hence uses/shares the user's provided boost::asio::io_service.
 * This is a single threaded class designed to run and be called from one ioService thread only.
 * Time expiration is based on 2*(one_way_light_time + one_way_margin_time)
 * Explicit template instantiation is defined in its .cpp file for idType of Ltp::session_id_t and uint64_t.
 * The idType is a "serial number" used to associate an expiry time with. 
 */

#ifndef LTP_TIMER_MANAGER_H
#define LTP_TIMER_MANAGER_H 1

#include <unordered_map>
#include <list>
#include <boost/asio.hpp>
#include <boost/function.hpp>
#include "ltp_lib_export.h"
#include <boost/core/noncopyable.hpp>

template <typename idType, typename hashType>
class LtpTimerManager : private boost::noncopyable {
private:
    LtpTimerManager() = delete;
public:
    typedef boost::function<void(void * classPtr, const idType & serialNumber, std::vector<uint8_t> & userData)> LtpTimerExpiredCallback_t;
    LTP_LIB_EXPORT LtpTimerManager(boost::asio::deadline_timer & deadlineTimerRef,
        boost::posix_time::time_duration & transmissionToAckReceivedTimeRef,
        const uint64_t hashMapNumBuckets);
    LTP_LIB_EXPORT ~LtpTimerManager();
    LTP_LIB_EXPORT void Reset();
       
    LTP_LIB_EXPORT bool StartTimer(void* classPtr, const idType serialNumber, const LtpTimerExpiredCallback_t* callbackPtr, std::vector<uint8_t> userData = std::vector<uint8_t>());
    LTP_LIB_EXPORT bool DeleteTimer(const idType serialNumber);
    LTP_LIB_EXPORT bool DeleteTimer(const idType serialNumber, std::vector<uint8_t> & userDataReturned);
    LTP_LIB_EXPORT bool DeleteTimer(const idType serialNumber, std::vector<uint8_t> & userDataReturned,
        const LtpTimerExpiredCallback_t*& callbackPtrReturned, void*& classPtrReturned);
    LTP_LIB_EXPORT void AdjustRunningTimers(const boost::posix_time::time_duration & diffNewMinusOld);
    LTP_LIB_EXPORT bool Empty() const;
    LTP_LIB_EXPORT const boost::posix_time::time_duration& GetTimeDurationRef() const;
    //std::vector<uint8_t> & GetUserDataRef(const uint64_t serialNumber);
private:
    LTP_LIB_NO_EXPORT void OnTimerExpired(const boost::system::error_code& e, bool * isTimerDeleted);
private:
    boost::asio::deadline_timer & m_deadlineTimerRef;
    boost::posix_time::time_duration & m_transmissionToAckReceivedTimeRef; //may be changed from outside
    //boost::bimap<idType, boost::posix_time::ptime> m_bimapCheckpointSerialNumberToExpiry;
    //std::map<idType, std::vector<uint8_t> > m_mapSerialNumberToUserData;
    struct timer_data_t {
        void* m_classPtr;
        idType m_id;
        boost::posix_time::ptime m_expiry;
        const LtpTimerExpiredCallback_t * m_callbackPtr;
        std::vector<uint8_t> m_userData;

        timer_data_t() = delete;
        //a move constructor: X(X&&)
        timer_data_t(void* classPtr, idType id, const boost::posix_time::ptime& expiry, const LtpTimerExpiredCallback_t* callbackPtr, std::vector<uint8_t>&& userData) :
            m_classPtr(classPtr),
            m_id(id),
            m_expiry(expiry),
            m_callbackPtr(callbackPtr),
            m_userData(std::move(userData)) {}
    };
    typedef std::list<timer_data_t> timer_data_list_t;
    typedef std::unordered_map<idType, typename timer_data_list_t::iterator, hashType> id_to_data_map_t;
    timer_data_list_t m_listTimerData;
    id_to_data_map_t m_mapIdToTimerData;
    

    idType m_activeSerialNumberBeingTimed;
    bool m_isTimerActive;
    bool * m_timerIsDeletedPtr;
};

#endif // LTP_TIMER_MANAGER_H

