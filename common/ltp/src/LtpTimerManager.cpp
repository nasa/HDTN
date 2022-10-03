/**
 * @file LtpTimerManager.cpp
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

#include "LtpTimerManager.h"
#include <iostream>
#include <boost/bind/bind.hpp>
#include "Ltp.h"

template <typename idType, typename hashType>
LtpTimerManager<idType, hashType>::LtpTimerManager(boost::asio::deadline_timer& deadlineTimerRef,
    boost::posix_time::time_duration& transmissionToAckReceivedTimeRef,
    const uint64_t hashMapNumBuckets) :
    m_deadlineTimerRef(deadlineTimerRef),
    m_transmissionToAckReceivedTimeRef(transmissionToAckReceivedTimeRef),
    m_timerIsDeletedPtr(new bool(false))
{
    m_mapIdToTimerData.reserve(hashMapNumBuckets);
    Reset();
}

template <typename idType, typename hashType>
LtpTimerManager<idType, hashType>::~LtpTimerManager() {
    //std::cout << "~LtpTimerManager " << m_isTimerActive << std::endl;
    //this destructor is single threaded
    if (!m_isTimerActive) {
        delete m_timerIsDeletedPtr;
        m_timerIsDeletedPtr = NULL;
    }
    else { //timer is active
        *m_timerIsDeletedPtr = true;
    }
    Reset();
}

template <typename idType, typename hashType>
void LtpTimerManager<idType, hashType>::Reset() {
    m_listTimerData.clear(); //clear first so cancel doesnt restart the next one
    m_mapIdToTimerData.clear();
    m_deadlineTimerRef.cancel();
    m_activeSerialNumberBeingTimed = 0;
    m_isTimerActive = false;
}


template <typename idType, typename hashType>
bool LtpTimerManager<idType, hashType>::StartTimer(const idType serialNumber, const LtpTimerExpiredCallback_t* callbackPtr, std::vector<uint8_t> userData) {
    //expiry will always be appended to list (always greater than previous) (duplicate expiries ok)
    const boost::posix_time::ptime expiry = boost::posix_time::microsec_clock::universal_time() + m_transmissionToAckReceivedTimeRef;
    
    std::pair<typename id_to_data_map_t::iterator, bool> retVal = m_mapIdToTimerData.emplace(serialNumber, timer_data_list_t::iterator());
    if (retVal.second) {
        //value was inserted
        m_listTimerData.emplace_back(serialNumber, expiry, callbackPtr, std::move(userData));
        retVal.first->second = std::prev(m_listTimerData.end()); //For a non-empty container c, the expression c.back() is equivalent to *std::prev(c.end())
        //std::cout << "StartTimer inserted " << serialNumber << std::endl;
        if (!m_isTimerActive) { //timer is not running
            //std::cout << "StartTimer started timer for " << serialNumber << std::endl;
            m_activeSerialNumberBeingTimed = serialNumber;
            m_deadlineTimerRef.expires_at(expiry);
            m_deadlineTimerRef.async_wait(boost::bind(&LtpTimerManager::OnTimerExpired, this, boost::asio::placeholders::error, m_timerIsDeletedPtr));
            m_isTimerActive = true;
        }
        return true;
    }
    return false;
}

template <typename idType, typename hashType>
bool LtpTimerManager<idType, hashType>::DeleteTimer(const idType serialNumber) {
    std::vector<uint8_t> userDataToDiscard;
    const LtpTimerExpiredCallback_t* callbackPtrToDiscard;
    return DeleteTimer(serialNumber, userDataToDiscard, callbackPtrToDiscard);
}

template <typename idType, typename hashType>
bool LtpTimerManager<idType, hashType>::DeleteTimer(const idType serialNumber, std::vector<uint8_t> & userDataReturned, const LtpTimerExpiredCallback_t *& callbackPtrReturned) {
    
    typename id_to_data_map_t::iterator mapIt = m_mapIdToTimerData.find(serialNumber);
    if (mapIt != m_mapIdToTimerData.end()) {
        typename timer_data_list_t::iterator& timerDataListIt = mapIt->second;
        timer_data_t& timerDataRef = *timerDataListIt;
        //std::cout << "DeleteTimer found and erasing " << serialNumber << std::endl;
        userDataReturned = std::move(timerDataRef.m_userData);
        callbackPtrReturned = timerDataRef.m_callbackPtr;
        m_listTimerData.erase(timerDataListIt);
        m_mapIdToTimerData.erase(mapIt);
        if (m_isTimerActive && (m_activeSerialNumberBeingTimed == serialNumber)) { //if this is the one running, cancel it (which will automatically start the next)
            //std::cout << "delete is cancelling " << serialNumber << std::endl;
            m_activeSerialNumberBeingTimed = 0; //prevent double delete and callback when cancelled externally after expiration
            m_deadlineTimerRef.cancel(); 
        }
        return true;
    }
    return false;
}

template <typename idType, typename hashType>
void LtpTimerManager<idType, hashType>::OnTimerExpired(const boost::system::error_code& e, bool * isTimerDeleted) {

    //for that timer that got cancelled by the destructor, it's still going to enter this function.. prevent it from using the deleted member variables
    if (*isTimerDeleted) {
        //std::cout << "safely returning from OnTimerExpired\n";
        delete isTimerDeleted;
        return;
    }

    if (e != boost::asio::error::operation_aborted) {
        // Timer was not cancelled, take necessary action.
        const idType serialNumberThatExpired = m_activeSerialNumberBeingTimed;
        if (!(serialNumberThatExpired == 0)) { //if not deleted externally when active after expiration
            std::vector<uint8_t> userData; //grab any user data before DeleteTimer deletes it
            const LtpTimerExpiredCallback_t * callbackPtr; //grab before DeleteTimer deletes it
            //std::cout << "OnTimerExpired expired sn " << serialNumberThatExpired << std::endl;
            m_activeSerialNumberBeingTimed = 0; //so DeleteTimer does not try to cancel timer that already expired
            DeleteTimer(serialNumberThatExpired, userData, callbackPtr); //callback function can choose to readd it later

            const LtpTimerExpiredCallback_t& callbackRef = *callbackPtr;
            callbackRef(serialNumberThatExpired, userData); //called after DeleteTimer in case callback readds it
        }
    }
    else {
        //std::cout << "timer cancelled\n";
    }

    //regardless of cancelled or expired:
    //start the next timer (if most recent ptime exists)
    typename timer_data_list_t::iterator it = m_listTimerData.begin();
    if (it != m_listTimerData.end()) {
        //most recent ptime exists
        
        m_activeSerialNumberBeingTimed = it->m_idType;
        //std::cout << "OnTimerExpired starting next ptime for " << m_activeSerialNumberBeingTimed << std::endl;
        m_deadlineTimerRef.expires_at(it->m_expiry);
        m_deadlineTimerRef.async_wait(boost::bind(&LtpTimerManager::OnTimerExpired, this, boost::asio::placeholders::error, m_timerIsDeletedPtr));
        m_isTimerActive = true;
    }
    else {
        m_isTimerActive = false;
    }
}

template <typename idType, typename hashType>
bool LtpTimerManager<idType, hashType>::Empty() const {
    return m_listTimerData.empty();
}

// Explicit template instantiation
template class LtpTimerManager<uint64_t, std::hash<uint64_t> >;

//template<> struct std::hash<Ltp::session_id_t> {
//    std::size_t operator()(const Ltp::session_id_t& o) const noexcept {
//        return static_cast<std::size_t>(o.sessionOriginatorEngineId ^ o.sessionNumber);
//    }
//};
template class LtpTimerManager<Ltp::session_id_t, Ltp::hash_session_id_t>;
