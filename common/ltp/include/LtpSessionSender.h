/**
 * @file LtpSessionSender.h
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
 * This LtpSessionSender class encapsulates one LTP sending session.
 * It uses its own asynchronous timer which uses/shares the user's provided boost::asio::io_service.
 */

#ifndef LTP_SESSION_SENDER_H
#define LTP_SESSION_SENDER_H 1

#include "LtpFragmentSet.h"
#include "Ltp.h"
#include "ForwardListQueue.h"
#include <set>
#include <boost/asio.hpp>
#include <memory>
#include "LtpTimerManager.h"
#include "MemoryInFiles.h"
#include "LtpNoticesToClientService.h"
#include "LtpClientServiceDataToSend.h"
#include <boost/core/noncopyable.hpp>

/**
 * @typedef NotifyEngineThatThisSenderNeedsDeletedCallback_t Type of callback to be invoked when this sender should be queued for deletion.
 */
typedef boost::function<void(const Ltp::session_id_t & sessionId, bool wasCancelled, CANCEL_SEGMENT_REASON_CODES reasonCode, std::shared_ptr<LtpTransmissionRequestUserData> & userDataPtr)> NotifyEngineThatThisSenderNeedsDeletedCallback_t;

/**
 * @typedef NotifyEngineThatThisSenderHasProducibleDataFunction_t Type of callback to be invoked when this sender has data to send.
 */
typedef boost::function<void(const uint64_t sessionNumber)> NotifyEngineThatThisSenderHasProducibleDataFunction_t;

class LtpSessionSender : private boost::noncopyable {
private:
    LtpSessionSender() = delete;
public:
    /// Checkpoint fragment retransmission context data
    struct resend_fragment_t {
        /// Default constructor
        resend_fragment_t() {}
        /// Start number of retries from 1
        resend_fragment_t(uint64_t paramOffset, uint64_t paramLength, uint64_t paramCheckpointSerialNumber, uint64_t paramReportSerialNumber, LTP_DATA_SEGMENT_TYPE_FLAGS paramFlags) :
            offset(paramOffset), length(paramLength), checkpointSerialNumber(paramCheckpointSerialNumber), reportSerialNumber(paramReportSerialNumber), flags(paramFlags), retryCount(1) {}
        /// Fragment offset
        uint64_t offset;
        /// Fragment length
        uint64_t length;
        /// Checkpoint serial number
        uint64_t checkpointSerialNumber;
        /// Associated report serial number
        uint64_t reportSerialNumber;
        /// Data segment type
        LTP_DATA_SEGMENT_TYPE_FLAGS flags;
        /// Number of retries
        uint32_t retryCount;
    };

    typedef std::set<uint64_t,
        std::less<uint64_t>,
        FreeListAllocator<uint64_t> > report_segment_serial_numbers_received_set_t;

    typedef std::list<uint64_t, FreeListAllocator<uint64_t> > checkpoint_serial_number_active_timers_list_t;

    /// Recycle this data because it contains containers with their own allocators that have recycled elements.
    struct LtpSessionSenderRecycledData : private boost::noncopyable {

        LTP_LIB_EXPORT void ClearAll();

        /// Data fragments reported received
        LtpFragmentSet::data_fragment_set_t m_dataFragmentsAckedByReceiver;

        /// Internal operations queue, includes report acknowledgment segments
        ForwardListQueue<std::vector<uint8_t>, FreeListAllocator<std::vector<uint8_t> > > m_nonDataToSendFlistQueue;

        /// Data fragments needing-retransmitted queue
        ForwardListQueue<resend_fragment_t, FreeListAllocator<resend_fragment_t> > m_resendFragmentsFlistQueue;

        /// Received report serial numbers
        report_segment_serial_numbers_received_set_t m_reportSegmentSerialNumbersReceivedSet;

        /// Checkpoint serial numbers with active retransmission timers, a timer stops being active either on reported receive or on RLEXC triggered by checkpoint retransmission limit
        checkpoint_serial_number_active_timers_list_t m_checkpointSerialNumberActiveTimersList;

        /// Map holding report serial numbers, mapped by report scope bounds (rsLowerBound, rsUpperBound)
        /// Pending report serial numbers, mapped by report scope bounds, when (m_mapRsBoundsToRsnPendingGeneration.empty()) indicates no active data segment retransmission timers
        /// Used to recalculate gaps in reception claims for data segment retransmission.
        LtpFragmentSet::ds_pending_map_t m_mapRsBoundsToRsnPendingGeneration;

        /// Temporary for LtpDelaySendDataSegmentsTimerExpiredCallback
        LtpFragmentSet::list_fragment_set_needing_resent_for_each_report_t m_tempListFragmentSetNeedingResentForEachReport;

        /// Temporary for ReportSegmentReceivedCallback
        LtpFragmentSet::data_fragment_set_t m_tempFragmentsNeedingResent;
    };
    typedef std::unique_ptr<LtpSessionSenderRecycledData> LtpSessionSenderRecycledDataUniquePtr;
    typedef UserDataRecycler<LtpSessionSenderRecycledDataUniquePtr> LtpSessionSenderRecycler;

    /// Sender common data, referenced across all senders associated with the same LtpEngine
    struct LtpSessionSenderCommonData : private boost::noncopyable {
        LtpSessionSenderCommonData() = delete;
        /// Start all stat counters from 0
        LTP_LIB_EXPORT LtpSessionSenderCommonData(
            uint64_t mtuClientServiceData,
            uint64_t checkpointEveryNthDataPacket,
            uint32_t & maxRetriesPerSerialNumberRef,
            LtpTimerManager<Ltp::session_id_t, Ltp::hash_session_id_t>& timeManagerOfCheckpointSerialNumbersRef,
            const LtpTimerManager<Ltp::session_id_t, Ltp::hash_session_id_t>::LtpTimerExpiredCallback_t& csnTimerExpiredCallbackRef,
            LtpTimerManager<uint64_t, std::hash<uint64_t> >& timeManagerOfSendingDelayedDataSegmentsRef,
            const LtpTimerManager<uint64_t, std::hash<uint64_t> >::LtpTimerExpiredCallback_t& delayedDataSegmentsTimerExpiredCallbackRef,
            const NotifyEngineThatThisSenderNeedsDeletedCallback_t& notifyEngineThatThisSenderNeedsDeletedCallbackRef,
            const NotifyEngineThatThisSenderHasProducibleDataFunction_t& notifyEngineThatThisSenderHasProducibleDataFunctionRef,
            const InitialTransmissionCompletedCallback_t& initialTransmissionCompletedCallbackRef,
            LtpSessionSenderRecycler& ltpSessionSenderRecyclerRef);

        /// The max size of the data portion (excluding LTP headers and UDP headers and IP headers) of a red data segment
        uint64_t m_mtuClientServiceData;
        /// Enables accelerated retransmission for an LTP sender by making every Nth UDP packet a checkpoint (0 disables)
        uint64_t m_checkpointEveryNthDataPacket;
        /// The max number of retries/resends of a single LTP packet with a serial number before the session is terminated
        uint32_t & m_maxRetriesPerSerialNumberRef;
        /// Checkpoint retransmission timer manager, timer mapped by session ID, hashed by session ID
        LtpTimerManager<Ltp::session_id_t, Ltp::hash_session_id_t>& m_timeManagerOfCheckpointSerialNumbersRef;
        /// Checkpoint retransmission timer expiry callback
        const LtpTimerManager<Ltp::session_id_t, Ltp::hash_session_id_t>::LtpTimerExpiredCallback_t& m_csnTimerExpiredCallbackRef;
        /// Data segment retransmission timer manager, timer mapped by session number, hashed by session number
        LtpTimerManager<uint64_t, std::hash<uint64_t> >& m_timeManagerOfSendingDelayedDataSegmentsRef;
        /// Data segment retransmission timer expiry callback
        const LtpTimerManager<uint64_t, std::hash<uint64_t> >::LtpTimerExpiredCallback_t& m_delayedDataSegmentsTimerExpiredCallbackRef;
        
        /// LtpEngine this sender should be queued for deletion notice function
        const NotifyEngineThatThisSenderNeedsDeletedCallback_t& m_notifyEngineThatThisSenderNeedsDeletedCallbackRef;
        /// LtpEngine this sender has data to send notice function
        const NotifyEngineThatThisSenderHasProducibleDataFunction_t& m_notifyEngineThatThisSenderHasProducibleDataFunctionRef;
        /// LtpEngine this sender has completed initial data transmission (first pass) notice function
        const InitialTransmissionCompletedCallback_t& m_initialTransmissionCompletedCallbackRef;
        /// Recycled data structure manager
        LtpSessionSenderRecycler& m_ltpSessionSenderRecyclerRef;

        //session sender stats
        /// Total number of checkpoint retransmission timer expiry callback invocations
        std::atomic<uint64_t> m_numCheckpointTimerExpiredCallbacks;
        /// Total number of discretionary checkpoints reported received
        std::atomic<uint64_t> m_numDiscretionaryCheckpointsNotResent;
        /// Total number of reports deleted after claiming reception of their entire scope
        std::atomic<uint64_t> m_numDeletedFullyClaimedPendingReports;
    };

    
    /// Clean up active checkpoint and data segment retransmission timers from the shared timer manager
    LTP_LIB_EXPORT ~LtpSessionSender();
    
    /**
     * Start counters from 0 and initialize flags.
     * @post The arguments to dataToSend and userDataPtrToTake are left in a moved-from state.
     */
    LTP_LIB_EXPORT LtpSessionSender(uint64_t randomInitialSenderCheckpointSerialNumber, LtpClientServiceDataToSend && dataToSend,
        std::shared_ptr<LtpTransmissionRequestUserData> && userDataPtrToTake, uint64_t lengthOfRedPart,
        const Ltp::session_id_t & sessionId, const uint64_t clientServiceId,
        const uint64_t memoryBlockId, LtpSessionSenderCommonData& ltpSessionSenderCommonDataRef);
    
    /** Load next critical data segment to send.
     *
     * Critical data priority (in descending order, 1 is highest priority):
     * 1. Internal operation queue segments (includes report acknowledgment segments):
     *   If the queue is NOT empty, the first queued segment is popped from the queue and loaded into the send operation data context.
     * 2. Data fragments needing-retransmitted queue segments:
     *   If the red data part of this session has already been reported received, clears the data fragment needing-retransmitted queue and returns
     *   with a value of False.
     *   Else, if the queue is NOT empty, the first queued segment is popped from the queue and the send operation context data are modified accordingly:
     *   A. If the segment is a checkpoint:
     *     A checkpoint retransmission timer is attempted to be started.
     *   B. If the data need to be read from disk:
     *     The deferred disk read context data are updated for an eventual deferred read NOT initiated from this function, the data are NOT loaded in-memory.
     *   C. If the data need to be read from memory:
     *     The data are loaded from the in-memory client service data to send, then the send operation data context is updated to hold a copy
     *     of the shared pointer to the in-memory client service data to send, so the data are not deleted before the send operation is completed.
     * @param udpSendPacketInfo The send operation context data to modify.
     * @return True if there is a segment to send and it could be loaded successfully (and thus the send operation context data are modified), or False otherwise.
     * @post If returns True, the argument to udpSendPacketInfo is modified accordingly (see above for details).
     */
    LTP_LIB_EXPORT bool NextTimeCriticalDataToSend(UdpSendPacketInfo & udpSendPacketInfo);
    
    /** Load next first-pass data segment to send.
     *
     * If there are no first-pass data left to send, returns immediately with a value of False.
     * Else, the send operation context data are modified accordingly:
     * 1. If we are sending red data:
     *   A. If the segment is a checkpoint (periodic, EORP or EOB):
     *     The segment checkpoint type is updated appropriately, then a checkpoint retransmission timer is attempted to be started.
     *   B. If the data need to be read from disk:
     *     The deferred disk read context data are updated for an eventual deferred read NOT initiated from this function, the data are NOT loaded in-memory.
     *   C. If the data need to be read from memory:
     *     The data are loaded from the in-memory client service data to send.
     *   D. Finally:
     *     Advances the next first-pass data offset.
     * 2. If we are sending green data:
     *   A. If the segment is a checkpoint (EOB):
     *     The segment checkpoint type is updated appropriately, green data do NOT use checkpoint retransmission timers.
     *   B. If the data need to be read from disk:
     *     The deferred disk read context data are updated for an eventual deferred read NOT initiated from this function, the data are NOT loaded in-memory.
     *   C. If the data need to be read from memory:
     *     The data are loaded from the in-memory client service data to send.
     *   D. Finally:
     *     Advances the next first-pass data offset.
     * 3. Finally:
     *   A. If the data were loaded from memory:
     *     The send operation data context is updated to hold a copy of the shared pointer to the in-memory client service data to send,
     *     so the data are not deleted before the send operation is completed.
     *   B. [SYSTEM]: If execution reaches this point:
     *     Calls m_initialTransmissionCompletedCallbackRef() to notify the associated LtpEngine that initial data transmission (first pass) has been completed.
     *   C. If this is a fully green send session OR if all the red data have already been reported-received:
     *     If not already marked, marks the sender for deferred deletion then calls m_notifyEngineThatThisSenderNeedsDeletedCallbackRef()
     *     with a cancel code of RESERVED to notify the associated LtpEngine for sender deletion.
     * @param udpSendPacketInfo The send operation context data to modify.
     * @return True if there is a segment to send and it could be loaded successfully (and thus the send operation context data are modified), or False otherwise.
     * @post If returns True, the argument to udpSendPacketInfo is modified accordingly (see above for details).
     */
    LTP_LIB_EXPORT bool NextFirstPassDataToSend(UdpSendPacketInfo& udpSendPacketInfo);

    
    /** Handle report segment reception.
     *
     * Appends a report acknowledgment segment with the same serial number as the report segment to the internal operations queue.
     * Regardless of processing (if any) of the segment, if the sender is not marked for deletion, calls m_notifyEngineThatThisSenderHasProducibleDataFunctionRef()
     * to notify the associated LtpEngine that there is data to send.
     * If the report segment is redundant, no processing is required.
     * Else, processing goes through the following steps:
     * 1. If the report segment has a non-zero checkpoint serial number.
     *   The active checkpoint timer associated with the report is deleted.
     * 2. If scope of the report segment has already been processed by earlier reports:
     *   No further processing is required.
     * 3. If this report covers still pending data segments:
     *   A. If all red data have JUST NOW been reported received by the addition of this report (first time only):
     *     Set the all red data reported received flag.
     *   B. If all red and green data have already been sent AND all red data have been reported received:
     *     If not already marked, marks the sender for deferred deletion then calls m_notifyEngineThatThisSenderNeedsDeletedCallbackRef()
     *     with a cancel code of RESERVED to notify the associated LtpEngine for sender deletion.
     *   C. [UNIT TESTING]: If we are NOT handling possibly out-of-order reception of report segments:
     *     Calls LtpSessionSender::ResendDataFromReport() to begin retransmission for the unaccounted-for needing-retransmitted data fragments
     *     with only the last fragment marked as a checkpoint.
     *   C. If we are handling possibly out-of-order reception of report segments:
     *     If the data segment retransmission timer is NOT currently running AND there are data fragments needing-retransmitted, the data segment
     *     retransmission timer is attempted to be started.
     *     If the data segment retransmission timer is currently running AND this report JUST NOW filled all reception claim gaps for the
     *     data fragments needing-retransmitted, the timer is stopped and the data fragments needing-retransmitted are cleared.
     * @param reportSegment The report segment.
     * @param headerExtensions The LTP header extensions.
     * @param trailerExtensions The LTP trailer extensions.
     */
    LTP_LIB_EXPORT void ReportSegmentReceivedCallback(const Ltp::report_segment_t & reportSegment,
        Ltp::ltp_extensions_t & headerExtensions, Ltp::ltp_extensions_t & trailerExtensions);
    
    //these two are public now because they are invoked by LtpEngine (eliminates need for each session to have its own expensive boost::function)
    /** Handle checkpoint retransmission timer expiry.
     *
     * Deletes the active checkpoint retransmission timer.
     * 1. If the transmission retry count is within the checkpoint retransmission limit:
     *   If this is a discretionary checkpoint, the checkpoint is NOT retransmitted and is thus dropped.
     *   Else, the checkpoint is queued back for retransmission, the retry count is incremented, then calls m_notifyEngineThatThisSenderHasProducibleDataFunctionRef()
     *   to notify the associated LtpEngine that there is data to send.
     * 2. If the checkpoint retransmission limit has been reached:
     *   If not already marked, marks the sender for deferred deletion then calls m_notifyEngineThatThisSenderNeedsDeletedCallbackRef()
     *   with a cancel code of RLEXC to notify the associated LtpEngine for sender deletion, also marks this session as failed.
     * @param checkpointSerialNumberPlusSessionNumber The checkpoint session ID.
     * @param userData The checkpoint retransmission timer context data.
     */
    LTP_LIB_EXPORT void LtpCheckpointTimerExpiredCallback(const Ltp::session_id_t& checkpointSerialNumberPlusSessionNumber, std::vector<uint8_t>& userData);

    //these two are public now because they are invoked by LtpEngine (eliminates need for each session to have its own expensive boost::function)
    /** Handle data segment retransmission timer expiry.
     *
     * Calculates the data fragments needing-retransmitted, then on each resulting data fragment calls LtpSessionSender::ResendDataFromReport()
     * to begin retransmission,
     * Clears the pending report serial numbers.
     * If the sender is not marked for deletion, calls m_notifyEngineThatThisSenderHasProducibleDataFunctionRef() to notify the associated LtpEngine
     * that there is data to send.
     * @param sessionNumber Our session number.
     * @param userData The data segment retransmission timer context data.
     */
    LTP_LIB_EXPORT void LtpDelaySendDataSegmentsTimerExpiredCallback(const uint64_t& sessionNumber, std::vector<uint8_t>& userData);

private:
    /** Queue for retransmission data fragments needing-retransmitted for the given report.
     *
     * Queue each data fragment for retransmission in the data fragments needing-retransmitted queue, setting only the last data fragment
     * as a checkpoint and setting EORP and EOB appropriately.
     * @param fragmentsNeedingResent The data fragments needing-retransmitted.
     * @param reportSerialNumber The report serial number.
     */
    LTP_LIB_NO_EXPORT void ResendDataFromReport(const LtpFragmentSet::data_fragment_set_t& fragmentsNeedingResent, const uint64_t reportSerialNumber);
    
    
    
    /// Checkpoint retransmission timer context data
    struct csntimer_userdata_t {
        /// Checkpoint serial number with active retransmission timer iterator
        std::list<uint64_t>::iterator itCheckpointSerialNumberActiveTimersList;
        /// Checkpoint fragment retransmission context data
        resend_fragment_t resendFragment;
    };

    
    /// Upper bound of received report with the largest scope span, used to recalculate gaps in reception claims for data segment retransmission
    uint64_t m_largestEndIndexPendingGeneration;

    /// ...
    uint64_t m_receptionClaimIndex;
    /// Next checkpoint serial number
    uint64_t m_nextCheckpointSerialNumber;
public:
    /// Client service data to send (red prefix and green suffix), when (m_dataToSendSharedPtr->data() == NULL) we MUST read the data from disk instead
    std::shared_ptr<LtpClientServiceDataToSend> m_dataToSendSharedPtr;
    /// Session attached client service data
    std::shared_ptr<LtpTransmissionRequestUserData> m_userDataPtr;
    /// Red part data length in bytes
    const uint64_t M_LENGTH_OF_RED_PART;
private:
    /// Next first pass data offset, used for initial transmission, when (m_dataIndexFirstPass >= M_LENGTH_OF_RED_PART) the rest of the data pending initial transmission are all green data
    uint64_t m_dataIndexFirstPass;
    /// Our session ID
    const Ltp::session_id_t M_SESSION_ID;
    /// Remote client service ID
    const uint64_t M_CLIENT_SERVICE_ID;
    /// Periodic checkpoint counter, if using periodic checkpoints for every Nth packet, when the counter reaches zero the next packet MUST be a checkpoint and the counter reset
    uint64_t m_checkpointEveryNthDataPacketCounter;
public:
    /// Our memory block ID, if using the disk for intermediate storage the ID MUST be non-zero, the lifetime of the memory block is managed by the associated LtpEngine
    const uint64_t MEMORY_BLOCK_ID;
private:
    /// Recycled data structures for this session
    LtpSessionSenderRecycledDataUniquePtr m_ltpSessionSenderRecycledDataUniquePtr;
    /// Sender common data reference, the data shared by all senders of the associated LtpEngine
    LtpSessionSenderCommonData& m_ltpSessionSenderCommonDataRef;
    /// Whether deferred deletion of this sender has been requested (typically on session completed), used to notify the associated LtpEngine
    bool m_didNotifyForDeletion;
    /// Whether the receiver has received all the red data, if True we can safely delete all the currently stored red data segments
    bool m_allRedDataReceivedByRemote;
public:
    //stats
    /// Whether the send session has been completed due to a fatal error, currently only used on RLEXC triggered by checkpoint retransmission limit
    bool m_isFailedSession;
    /// Whether the send session has been completed, used to prevent against multiple executions of the session completion procedure
    bool m_calledCancelledOrCompletedCallback;
};

#endif // LTP_SESSION_SENDER_H

