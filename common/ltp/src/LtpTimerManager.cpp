#include "LtpTimerManager.h"
#include <iostream>
#include <boost/bind/bind.hpp>
#include "Ltp.h"

template <class idType>
LtpTimerManager<idType>::LtpTimerManager(boost::asio::io_service & ioService, const boost::posix_time::time_duration & oneWayLightTime,
    const boost::posix_time::time_duration & oneWayMarginTime, const LtpTimerExpiredCallback_t & callback) :
    m_deadlineTimer(ioService),
    M_ONE_WAY_LIGHT_TIME(oneWayLightTime),
    M_ONE_WAY_MARGIN_TIME(oneWayMarginTime),
    M_TRANSMISSION_TO_ACK_RECEIVED_TIME((oneWayLightTime * 2) + (oneWayMarginTime * 2)),
    m_ltpTimerExpiredCallbackFunction(callback),
    m_timerIsDeletedPtr(new bool(false))
{

    Reset();
}

template <class idType>
LtpTimerManager<idType>::~LtpTimerManager() {
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

template <class idType>
void LtpTimerManager<idType>::Reset() {
    m_listCheckpointSerialNumberPlusExpiry.clear(); //clear first so cancel doesnt restart the next one
    m_mapCheckpointSerialNumberToExpiryListIteratorPlusUserData.clear();
    m_deadlineTimer.cancel();
    m_activeSerialNumberBeingTimed = 0;
    m_isTimerActive = false;
}


template <class idType>
bool LtpTimerManager<idType>::StartTimer(const idType serialNumber, std::vector<uint8_t> userData) {
    //expiry will always be appended to list (always greater than previous) (duplicate expiries ok)
    const boost::posix_time::ptime expiry = boost::posix_time::microsec_clock::universal_time() + M_TRANSMISSION_TO_ACK_RECEIVED_TIME;
    
    std::pair<typename id_to_listiteratorplususerdata_map_t::iterator, bool> retVal =
        m_mapCheckpointSerialNumberToExpiryListIteratorPlusUserData.insert(
            id_to_listiteratorplususerdata_map_insertion_element_t(serialNumber, listiterator_userdata_pair_t()));
    if (retVal.second) {
        //value was inserted
        retVal.first->second.first = m_listCheckpointSerialNumberPlusExpiry.insert(m_listCheckpointSerialNumberPlusExpiry.end(), id_ptime_pair_t(serialNumber,expiry));
        retVal.first->second.second = std::move(userData);
        //std::cout << "StartTimer inserted " << serialNumber << std::endl;
        if (!m_isTimerActive) { //timer is not running
            //std::cout << "StartTimer started timer for " << serialNumber << std::endl;
            m_activeSerialNumberBeingTimed = serialNumber;
            m_deadlineTimer.expires_at(expiry);
            m_deadlineTimer.async_wait(boost::bind(&LtpTimerManager::OnTimerExpired, this, boost::asio::placeholders::error, m_timerIsDeletedPtr));
            m_isTimerActive = true;
        }
        return true;
    }
    return false;
}

template <class idType>
bool LtpTimerManager<idType>::DeleteTimer(const idType serialNumber) {
    std::vector<uint8_t> userDataToDiscard;
    return DeleteTimer(serialNumber, userDataToDiscard);
}

template <class idType>
bool LtpTimerManager<idType>::DeleteTimer(const idType serialNumber, std::vector<uint8_t> & userDataReturned) {
    
    typename id_to_listiteratorplususerdata_map_t::iterator it = m_mapCheckpointSerialNumberToExpiryListIteratorPlusUserData.find(serialNumber);
    if (it != m_mapCheckpointSerialNumberToExpiryListIteratorPlusUserData.end()) {
        //std::cout << "DeleteTimer found and erasing " << serialNumber << std::endl;
        userDataReturned = std::move(it->second.second);
        m_listCheckpointSerialNumberPlusExpiry.erase(it->second.first);
        m_mapCheckpointSerialNumberToExpiryListIteratorPlusUserData.erase(it);
        if (m_isTimerActive && (m_activeSerialNumberBeingTimed == serialNumber)) { //if this is the one running, cancel it (which will automatically start the next)
            //std::cout << "delete is cancelling " << serialNumber << std::endl;
            m_activeSerialNumberBeingTimed = 0; //prevent double delete and callback when cancelled externally after expiration
            m_deadlineTimer.cancel(); 
        }
        return true;
    }
    return false;
}

template <class idType>
void LtpTimerManager<idType>::OnTimerExpired(const boost::system::error_code& e, bool * isTimerDeleted) {

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
            //std::cout << "OnTimerExpired expired sn " << serialNumberThatExpired << std::endl;
            m_activeSerialNumberBeingTimed = 0; //so DeleteTimer does not try to cancel timer that already expired
            DeleteTimer(serialNumberThatExpired, userData); //callback function can choose to readd it later

            m_ltpTimerExpiredCallbackFunction(serialNumberThatExpired, userData); //called after DeleteTimer in case callback readds it
        }
    }
    else {
        //std::cout << "timer cancelled\n";
    }

    //regardless of cancelled or expired:
    //start the next timer (if most recent ptime exists)
    typename id_ptime_list_t::iterator it = m_listCheckpointSerialNumberPlusExpiry.begin();
    if (it != m_listCheckpointSerialNumberPlusExpiry.end()) {
        //most recent ptime exists
        
        m_activeSerialNumberBeingTimed = it->first;
        //std::cout << "OnTimerExpired starting next ptime for " << m_activeSerialNumberBeingTimed << std::endl;
        m_deadlineTimer.expires_at(it->second);
        m_deadlineTimer.async_wait(boost::bind(&LtpTimerManager::OnTimerExpired, this, boost::asio::placeholders::error, m_timerIsDeletedPtr));
        m_isTimerActive = true;
    }
    else {
        m_isTimerActive = false;
    }
}

template <class idType>
bool LtpTimerManager<idType>::Empty() const {
    return m_listCheckpointSerialNumberPlusExpiry.empty();
}

// Explicit template instantiation
template class LtpTimerManager<uint64_t>;
template class LtpTimerManager<Ltp::session_id_t>;
