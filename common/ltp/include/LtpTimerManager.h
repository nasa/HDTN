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
#include "FreeListAllocator.h"
#include "ltp_lib_export.h"
#include <boost/core/noncopyable.hpp>

template <typename idType, typename hashType>
class LtpTimerManager : private boost::noncopyable {
private:
    LtpTimerManager() = delete;
public:
    /**
     * @typdef LtpTimerExpiredCallback_t Type of callback to be invoked on timer expiry.
     */
    typedef boost::function<void(void * classPtr, const idType & serialNumber, std::vector<uint8_t> & userData)> LtpTimerExpiredCallback_t;
    
    /**
     * Reserve space for hashMapNumBuckets timers.
     * Call LtpTimerManager::Reset() to prepare the timer manager.
     * @param deadlineTimerRef Our managed timer.
     * @param transmissionToAckReceivedTimeRef The timer wait duration.
     * @param hashMapNumBuckets The number of queued timers to reserve.
     */
    LTP_LIB_EXPORT LtpTimerManager(boost::asio::deadline_timer & deadlineTimerRef,
        boost::posix_time::time_duration & transmissionToAckReceivedTimeRef,
        const uint64_t hashMapNumBuckets);
    
    /**
     * If timer manager is NOT active, delete m_timerIsDeletedPtr and set to NULL.
     * Else, set *m_timerIsDeletedPtr to True and let the current active timer completion handler delete it.
     *
     * Call LtpTimerManager::Reset() to release all managed resources.
     * @post Assumes external code will handle deleting m_timerIsDeletedPtr to prevent a memory leak.
     */
    LTP_LIB_EXPORT ~LtpTimerManager();
    
    /** Perform timer manager reset.
     *
     * Clears queued timers, cancels current active timer, then sets timer manager state to inactive.
     * @post The object is ready to be reused.
     */
    LTP_LIB_EXPORT void Reset();
    
    /** Queue a new timer to manage.
     *
     * If a timer associated with serialNumber already exists, returns immediately.
     * Else, queues a new timer for processing, and if the timer manager is inactive (thus timer queue empty) sets the manager to active and actively
     * waits on the newly queued timer by starting the managed timer asynchronously with LtpTimerManager::OnTimerExpired() as a completion handler.
     * @param classPtr Type-erased data pointer.
     * @param serialNumber The serial number associated with the timer.
     * @param callbackPtr The callback to invoke on timer expiry.
     * @param userData The attached user data.
     * @return True if a NEW timer was queued successfully, or False otherwise.
     */
    LTP_LIB_EXPORT bool StartTimer(void* classPtr, const idType serialNumber, const LtpTimerExpiredCallback_t* callbackPtr, std::vector<uint8_t> userData = std::vector<uint8_t>());
    
    /** Delete a queued timer.
     *
     * Calls DeleteTimer(serialNumber, userDataReturned, callbackPtrReturned, classPtrReturned) to delete the queued timer.
     * On successful deletion, discards all of the timer context data.
     * @param serialNumber The serial number associated with the timer to delete.
     * @return True if the timer exists and could be deleted successfully, or False otherwise.
     */
    LTP_LIB_EXPORT bool DeleteTimer(const idType serialNumber);
    
    /** Delete a queued timer.
     *
     * Calls DeleteTimer(serialNumber, userDataReturned, callbackPtrReturned, classPtrReturned) to delete the queued timer.
     * On successful deletion, returns the attached user data to the caller from the timer context data.
     * @param serialNumber The serial number associated with the timer to delete.
     * @param userDataReturned The attached user data of the timer.
     * @return True if the timer exists and could be deleted successfully, or False otherwise.
     */
    LTP_LIB_EXPORT bool DeleteTimer(const idType serialNumber, std::vector<uint8_t> & userDataReturned);
    
    /** Delete a queued timer.
     *
     * If a timer associated with serialNumber does NOT exist, returns immediately.
     * Else, deletes the queued timer from the queue (without cancelling for performance reasons), then sets m_activeSerialNumberBeingTimed to 0.
     *
     * On successful deletion, returns the attached user data and the callback and the type-erased data pointer from the timer context data.
     * @param serialNumber The serial number associated with the timer to delete.
     * @param userDataReturned The attached user data of the timer.
     * @param callbackPtrReturned The callback of the timer.
     * @param classPtrReturned The type-erased data pointer of the timer.
     * @return True if the timer exists and could be deleted successfully, or False otherwise.
     */
    LTP_LIB_EXPORT bool DeleteTimer(const idType serialNumber, std::vector<uint8_t> & userDataReturned,
        const LtpTimerExpiredCallback_t*& callbackPtrReturned, void*& classPtrReturned);
    
    /** Adjust timer wait duration across all current and future queued timers.
     *
     * If the timer manager is NOT active, returns immediately.
     * Else, adjusts all queued timers by the given difference.
     * @param diffNewMinusOld The difference from the current timer wait duration, should be a negative duration when new duration < current duration.
     */
    LTP_LIB_EXPORT void AdjustRunningTimers(const boost::posix_time::time_duration & diffNewMinusOld);
    
    /** Query whether the timer queue is empty.
     *
     * @return True if the timer queue is empty, or False otherwise.
     */
    LTP_LIB_EXPORT bool Empty() const;
    
    /** Get the timer wait duration.
     *
     * @return The timer wait duration.
     */
    LTP_LIB_EXPORT const boost::posix_time::time_duration& GetTimeDurationRef() const;
    //std::vector<uint8_t> & GetUserDataRef(const uint64_t serialNumber);
private:
    /** Handle managed timer expiry.
     *
     * If the timer manager is due deletion, deletes isTimerDeleted and returns immediately.
     * If the expiry did NOT occur due to the timer being manually cancelled, caches the timer context data, calls LtpTimerManager::DeleteTimer() to delete
     * the expired timer and retain any data the caller might want back, then invokes the timer callback.
     *
     * Regardless of the reason the expiration occurred:
     * If there are any more timers left in the queue, starts the managed timer, for the next timer in the queue, asynchronously with itself
     * as a completion handler to achieve a processing loop.
     * Else, sets the timer manager as inactive since there is no more work left to do.
     * @param e The error code.
     * @param isTimerDeleted Pointer indicating whether the time manager has been deleted (see m_timerIsDeletedPtr docs for how to handle).
     */
    LTP_LIB_NO_EXPORT void OnTimerExpired(const boost::system::error_code& e, bool * isTimerDeleted);
private:
    /// Our managed timer
    boost::asio::deadline_timer & m_deadlineTimerRef;
    /// Timer wait duration, can be adjusted from outside this class
    boost::posix_time::time_duration & m_transmissionToAckReceivedTimeRef;
    //boost::bimap<idType, boost::posix_time::ptime> m_bimapCheckpointSerialNumberToExpiry;
    //std::map<idType, std::vector<uint8_t> > m_mapSerialNumberToUserData;
    /// Timer context data
    struct timer_data_t {
        /// Type-erased data pointer, fed to m_callbackPtr
        void* m_classPtr;
        /// Associated timer ID
        idType m_id;
        /// Associated timer expiry
        boost::posix_time::ptime m_expiry;
        /// Associated timer callback
        const LtpTimerExpiredCallback_t * m_callbackPtr;
        /// Attached user data
        std::vector<uint8_t> m_userData;

        timer_data_t() = delete;
        /// Move constructor
        timer_data_t(void* classPtr, idType id, const boost::posix_time::ptime& expiry, const LtpTimerExpiredCallback_t* callbackPtr, std::vector<uint8_t>&& userData) : //a move constructor: X(X&&)
            m_classPtr(classPtr),
            m_id(id),
            m_expiry(expiry),
            m_callbackPtr(callbackPtr),
            m_userData(std::move(userData)) {}
    };
    /// Type of list holding timer context data
    typedef std::list<timer_data_t, FreeListAllocatorDynamic<timer_data_t> > timer_data_list_t;
    /// Type of map holding timer context data iterators, mapped by idType, hashed by hashType
    typedef std::unordered_map<idType, typename timer_data_list_t::iterator,
        hashType,
        std::equal_to<idType>,
        FreeListAllocatorDynamic<std::pair<const idType, typename timer_data_list_t::iterator> > > id_to_data_map_t;
    /// Timer context data queue
    timer_data_list_t m_listTimerData;
    /// Timer context data iterators referencing m_listTimerData, mapped by idType
    id_to_data_map_t m_mapIdToTimerData;
    
    
    /// Serial number of current active timer, when set to 0 indicates no current active timer since a zero serial number is unreachable
    idType m_activeSerialNumberBeingTimed;
    /// Whether the timer manager is active
    bool m_isTimerActive;
    /// Timer manager deleted pointer, whether the timer manager has been destructed.
    /// 1. NULL -> Timer manager has been destructed while NOT active, no further action needed.
    /// 2. non-NULL AND (*m_timerIsDeletedPtr == true) -> Timer manager has been destructed while active, if observed through timer callback delete pointer and return immediately.
    bool * m_timerIsDeletedPtr;
};

#endif // LTP_TIMER_MANAGER_H

