#include "LtpTimerManager.h"
#include <iostream>
#include <boost/bind.hpp>
#include "Ltp.h"

template <class idType>
LtpTimerManager<idType>::LtpTimerManager(boost::asio::io_service & ioService, const boost::posix_time::time_duration & oneWayLightTime,
    const boost::posix_time::time_duration & oneWayMarginTime, const LtpTimerExpiredCallback_t & callback) :
    m_deadlineTimer(ioService),
    M_ONE_WAY_LIGHT_TIME(oneWayLightTime),
    M_ONE_WAY_MARGIN_TIME(oneWayMarginTime),
    M_TRANSMISSION_TO_ACK_RECEIVED_TIME((oneWayLightTime * 2) + (oneWayMarginTime * 2)),
    m_ltpTimerExpiredCallbackFunction(callback)
{

    Reset();
}

template <class idType>
LtpTimerManager<idType>::~LtpTimerManager() {
    Reset();
}

template <class idType>
void LtpTimerManager<idType>::Reset() {
    m_bimapCheckpointSerialNumberToExpiry.clear(); //clear first so cancel doesnt restart the next one
    m_mapSerialNumberToUserData.clear();
    m_deadlineTimer.cancel();
    m_activeSerialNumberBeingTimed = 0;
    m_isTimerActive = false;
}


template <class idType>
bool LtpTimerManager<idType>::StartTimer(const idType serialNumber, std::vector<uint8_t> userData) {
    boost::posix_time::ptime expiry;
    do { //loop to make sure expiry is unique in case multiple calls to StartTimer are too close
        expiry = boost::posix_time::microsec_clock::universal_time() + M_TRANSMISSION_TO_ACK_RECEIVED_TIME;
    } while (m_bimapCheckpointSerialNumberToExpiry.right.count(expiry));

    if (m_bimapCheckpointSerialNumberToExpiry.left.insert(boost::bimap<idType, boost::posix_time::ptime>::left_value_type(serialNumber, expiry)).second) {
        //value was inserted
        m_mapSerialNumberToUserData[serialNumber] = std::move(userData);
        //std::cout << "StartTimer inserted " << serialNumber << std::endl;
        if (!m_isTimerActive) { //timer is not running
            //std::cout << "StartTimer started timer for " << serialNumber << std::endl;
            m_activeSerialNumberBeingTimed = serialNumber;
            m_deadlineTimer.expires_at(expiry);
            m_deadlineTimer.async_wait(boost::bind(&LtpTimerManager::OnTimerExpired, this, boost::asio::placeholders::error));
            m_isTimerActive = true;
        }
        return true;
    }
    return false;
}

template <class idType>
bool LtpTimerManager<idType>::DeleteTimer(const idType serialNumber) {
    
    std::map<idType, std::vector<uint8_t> >::iterator itUserData = m_mapSerialNumberToUserData.find(serialNumber);
    if (itUserData != m_mapSerialNumberToUserData.end()) {
        m_mapSerialNumberToUserData.erase(itUserData);
    }
    
    boost::bimap<idType, boost::posix_time::ptime>::left_iterator it = m_bimapCheckpointSerialNumberToExpiry.left.find(serialNumber);
    if (it != m_bimapCheckpointSerialNumberToExpiry.left.end()) {
        //std::cout << "DeleteTimer found and erasing " << serialNumber << std::endl;
        m_bimapCheckpointSerialNumberToExpiry.left.erase(it);
        if (m_isTimerActive && (m_activeSerialNumberBeingTimed == serialNumber)) { //if this is the one running, cancel it (which will automatically start the next)
            //std::cout << "delete is cancelling " << serialNumber << std::endl;
            m_deadlineTimer.cancel(); 
        }
        return true;
    }
    return false;
}

template <class idType>
void LtpTimerManager<idType>::OnTimerExpired(const boost::system::error_code& e) {
    if (e != boost::asio::error::operation_aborted) {
        // Timer was not cancelled, take necessary action.
        const idType serialNumberThatExpired = m_activeSerialNumberBeingTimed;
        std::vector<uint8_t> userData(std::move(m_mapSerialNumberToUserData[serialNumberThatExpired])); //grab any user data before DeleteTimer deletes it
        //std::cout << "OnTimerExpired expired sn " << serialNumberThatExpired << std::endl;
        m_activeSerialNumberBeingTimed = 0; //so DeleteTimer does not try to cancel timer that already expired
        DeleteTimer(serialNumberThatExpired); //callback function can choose to readd it later

        m_ltpTimerExpiredCallbackFunction(serialNumberThatExpired, userData); //called after DeleteTimer in case callback readds it
    }
    else {
        //std::cout << "timer cancelled\n";
    }

    //regardless of cancelled or expired:
    //start the next timer (if most recent ptime exists)
    boost::bimap<idType, boost::posix_time::ptime>::right_iterator it = m_bimapCheckpointSerialNumberToExpiry.right.begin();
    if (it != m_bimapCheckpointSerialNumberToExpiry.right.end()) {
        //most recent ptime exists
        
        m_activeSerialNumberBeingTimed = it->second;
        //std::cout << "OnTimerExpired starting next ptime for " << m_activeSerialNumberBeingTimed << std::endl;
        m_deadlineTimer.expires_at(it->first);
        m_deadlineTimer.async_wait(boost::bind(&LtpTimerManager::OnTimerExpired, this, boost::asio::placeholders::error));
        m_isTimerActive = true;
    }
    else {
        m_isTimerActive = false;
    }
}

template <class idType>
bool LtpTimerManager<idType>::Empty() const {
    return m_bimapCheckpointSerialNumberToExpiry.left.empty();
}

// Explicit template instantiation
template class LtpTimerManager<uint64_t>;
template class LtpTimerManager<Ltp::session_id_t>;