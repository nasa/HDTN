/**
 * @file LtpSessionReceiver.h
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
 * This LtpSessionReceiver class encapsulates one LTP receiving session.
 * It uses its own asynchronous timer which uses/shares the user's provided boost::asio::io_service.
 */

#ifndef LTP_SESSION_RECEIVER_H
#define LTP_SESSION_RECEIVER_H 1

#include "LtpFragmentSet.h"
#include "Ltp.h"
#include "LtpRandomNumberGenerator.h"
#include "LtpTimerManager.h"
#include "MemoryInFiles.h"
#include "ForwardListQueue.h"
#include <set>
#include <map>
#include <boost/asio.hpp>
#include "LtpNoticesToClientService.h"
#include "LtpClientServiceDataToSend.h"
#include <boost/core/noncopyable.hpp>

/**
 * @typedef NotifyEngineThatThisReceiverNeedsDeletedCallback_t Type of callback to be invoked when this receiver should be queued for deletion.
 */
typedef boost::function<void(const Ltp::session_id_t & sessionId, bool wasCancelled, CANCEL_SEGMENT_REASON_CODES reasonCode)> NotifyEngineThatThisReceiverNeedsDeletedCallback_t;

/**
 * @typedef NotifyEngineThatThisReceiversTimersHasProducibleDataFunction_t Type of callback to be invoked when this receiver has data to send.
 */
typedef boost::function<void(const Ltp::session_id_t & sessionId)> NotifyEngineThatThisReceiversTimersHasProducibleDataFunction_t;

/**
 * @typedef NotifyEngineThatThisReceiverCompletedDeferredOperationFunction_t Type of callback to be invoked when this receiver has completed a deferred disk operation.
 */
typedef boost::function<void()> NotifyEngineThatThisReceiverCompletedDeferredOperationFunction_t;

class LtpSessionReceiver : private boost::noncopyable {
private:
    LtpSessionReceiver() = delete;

    /** Generate and queue a report for transmission.
     *
     * Calls LtpFragmentSet::PopulateReportSegment() to populate a single report segment from the currently received data fragments.
     * If the reception claims exceed the maximum number of reception claims per segment, calls LtpFragmentSet::SplitReportSegment() to split into
     * multiple smaller report segments.
     * For each resulting report segment, attaches an increasing report serial number, queues for transmission and calls m_notifyEngineThatThisReceiversTimersHasProducibleDataFunctionRef()
     * to notify the associated LtpEngine that there is data to send.
     * @param checkpointSerialNumber The associated checkpoint serial number.
     * @param lowerBound The lower bound.
     * @param upperBound The upper bound.
     * @param checkpointIsResponseToReportSegment Whether this is a secondary reception report (if False, is primary).
     */
    LTP_LIB_NO_EXPORT void HandleGenerateAndSendReportSegment(const uint64_t checkpointSerialNumber,
        const uint64_t lowerBound, const uint64_t upperBound, const bool checkpointIsResponseToReportSegment);
    
    /** Handle deferred disk write completion.
     *
     * Decrements the number of active disk I/O operations.
     * If NO disk I/O operations are currently in progress AND we have fully received the red part data, queues a deferred disk read to read back the
     * red part into m_dataReceivedRed (in-memory) with LtpSessionReceiver::OnRedDataRecoveredFromDisk() as a completion handler.
     * If the deferred read operation is queued successfully, increments the number of active disk I/O operations.
     * Calls m_notifyEngineThatThisReceiverCompletedDeferredOperationFunctionRef() to notify the associated LtpEngine that a deferred disk operation
     * has been completed.
     * @param clientServiceDataReceivedSharedPtr The client service data.
     * @param isEndOfBlock Whether the data contain the EOB segment.
     * @post The number of active disk I/O operations is modified (see above for details).
     */
    LTP_LIB_NO_EXPORT void OnDataSegmentWrittenToDisk(std::shared_ptr<std::vector<uint8_t> >& clientServiceDataReceivedSharedPtr, bool isEndOfBlock);
    
    /** Handle deferred red part read completion.
     *
     * Decrements the number of active disk I/O operations.
     * Initiates a deferred memory block deletion for our memory block.
     * If the red part reception callback is set, calls m_redPartReceptionCallbackRef() to invoke the red part reception callback.
     * Clears the in-memory red data store.
     * @param success Whether the read operation was successful.
     * @param isEndOfBlock Whether the data contain the EOB segment.
     * @post The number of active disk I/O operations is decremented.
     */
    LTP_LIB_NO_EXPORT void OnRedDataRecoveredFromDisk(bool success, bool isEndOfBlock);
public:
    /** Handle pending checkpoint delayed report transmission timer expiry.
     *
     * Calls LtpSessionReceiver::HandleGenerateAndSendReportSegment() to generate and queue a report for transmission.
     * Removes the checkpoint from the pending checkpoints for delayed report transmission.
     * @param checkpointSerialNumberPlusSessionNumber The associated checkpoint serial number.
     * @param userData The pending checkpoint delayed report transmission timer context data.
     */
    LTP_LIB_EXPORT void LtpDelaySendReportSegmentTimerExpiredCallback(const Ltp::session_id_t& checkpointSerialNumberPlusSessionNumber, std::vector<uint8_t>& userData);
    
    //these two are public now because they are invoked by LtpEngine (eliminates need for each session to have its own expensive boost::function)
    /** Handle report retransmission timer expiry.
     *
     * Removes the report from the reports with active retransmission timers.
     * If the transmission retry count is within the report retransmission limit, queues the report back for transmission, then calls m_notifyEngineThatThisReceiversTimersHasProducibleDataFunctionRef()
     * to notify the associated LtpEngine that there is data to send.
     * Else, If not already marked, marks the receiver for deferred deletion then calls m_notifyEngineThatThisReceiverNeedsDeletedCallbackRef()
     * with a cancel code of RLEXC to notify the associated LtpEngine for receiver deletion.
     * @param reportSerialNumberPlusSessionNumber The report serial number.
     * @param userData The report retransmission timer context data.
     */
    LTP_LIB_EXPORT void LtpReportSegmentTimerExpiredCallback(const Ltp::session_id_t& reportSerialNumberPlusSessionNumber, std::vector<uint8_t>& userData);

    /// Type of map holding report segments, mapped by report serial number
    typedef std::map<uint64_t, Ltp::report_segment_t,
        std::less<uint64_t>,
        FreeListAllocator<std::pair<const uint64_t, Ltp::report_segment_t> > > report_segments_sent_map_t;

    typedef std::set<uint64_t,
        std::less<uint64_t>,
        FreeListAllocator<uint64_t> > checkpoint_serial_numbers_received_set_t;

    /// Type of pair holding retries per report segment sent, (Report segment sent iterator TO number of retries)
    typedef std::pair<report_segments_sent_map_t::const_iterator, uint32_t> it_retrycount_pair_t; //pair<iterator from m_mapAllReportSegmentsSent, retryCount>

    typedef std::list<uint64_t, FreeListAllocator<uint64_t> > report_serial_number_active_timers_list_t;

    /// Type of pair holding checkpoint type metadata, (checkpoint serial number TO whether this is a secondary checkpoint).
    /// (checkpointSerialNumberToWhichRsPertains, checkpointIsResponseToReportSegment).
    typedef std::pair<uint64_t, bool> csn_issecondary_pair_t;
    /// Type of map holding checkpoint type metadata, mapped by data segment bounds (rsLowerBound, rsUpperBound)
    typedef std::map<FragmentSet::data_fragment_no_overlap_allow_abut_t, csn_issecondary_pair_t,
        std::less<FragmentSet::data_fragment_no_overlap_allow_abut_t>,
        FreeListAllocator<std::pair<const FragmentSet::data_fragment_no_overlap_allow_abut_t, csn_issecondary_pair_t> > > rs_pending_map_t;
    

    /// Recycle this data because it contains containers with their own allocators that have recycled elements.
    struct LtpSessionReceiverRecycledData : private boost::noncopyable {

        LTP_LIB_EXPORT void ClearAll();
        
        /// Received data fragments
        LtpFragmentSet::data_fragment_set_t m_receivedDataFragmentsSet;
        
        /// Report segments sent, mapped by report serial number
        report_segments_sent_map_t m_mapAllReportSegmentsSent;

        /// Received checkpoint serial numbers
        checkpoint_serial_numbers_received_set_t m_checkpointSerialNumbersReceivedSet;
        
        /// Reports needing-transmitted queue
        ForwardListQueue<it_retrycount_pair_t, FreeListAllocator<it_retrycount_pair_t> > m_reportsToSendFlistQueue;

        /// Report serial numbers with active retransmission timers
        report_serial_number_active_timers_list_t m_reportSerialNumberActiveTimersList;

        /// Pending checkpoint fragments, mapped by data segment bounds.
        /// When (m_mapReportSegmentsPendingGeneration.empty()) indicates no active pending checkpoint delayed report transmission timers.
        /// Used to recalculate gaps from received data fragments for pending checkpoint delayed report generation.
        rs_pending_map_t m_mapReportSegmentsPendingGeneration;

        //temporary vector data for HandleGenerateAndSendReportSegment
        std::vector<Ltp::report_segment_t> m_tempReportSegmentsVec;
        std::vector<Ltp::report_segment_t> m_tempReportSegmentsSplitVec;
        
    };
    typedef std::unique_ptr<LtpSessionReceiverRecycledData> LtpSessionReceiverRecycledDataUniquePtr;
    typedef UserDataRecycler<LtpSessionReceiverRecycledDataUniquePtr> LtpSessionReceiverRecycler;

    /// Receiver common data, referenced across all receivers associated with the same LtpEngine
    struct LtpSessionReceiverCommonData : private boost::noncopyable {
        LtpSessionReceiverCommonData() = delete;
        /// Start all stat counters from 0
        LTP_LIB_EXPORT LtpSessionReceiverCommonData(
            const uint64_t clientServiceId,
            uint64_t maxReceptionClaims,
            uint64_t estimatedBytesToReceive,
            uint64_t maxRedRxBytes,
            uint32_t& maxRetriesPerSerialNumberRef,
            LtpTimerManager<Ltp::session_id_t, Ltp::hash_session_id_t>& timeManagerOfReportSerialNumbersRef,
            const LtpTimerManager<Ltp::session_id_t, Ltp::hash_session_id_t>::LtpTimerExpiredCallback_t& rsnTimerExpiredCallbackRef,
            LtpTimerManager<Ltp::session_id_t, Ltp::hash_session_id_t>& timeManagerOfSendingDelayedReceptionReportsRef,
            const LtpTimerManager<Ltp::session_id_t, Ltp::hash_session_id_t>::LtpTimerExpiredCallback_t& delayedReceptionReportTimerExpiredCallbackRef,
            const NotifyEngineThatThisReceiverNeedsDeletedCallback_t& notifyEngineThatThisReceiverNeedsDeletedCallbackRef,
            const NotifyEngineThatThisReceiversTimersHasProducibleDataFunction_t& notifyEngineThatThisReceiversTimersHasProducibleDataFunctionRef,
            const NotifyEngineThatThisReceiverCompletedDeferredOperationFunction_t& notifyEngineThatThisReceiverCompletedDeferredOperationFunctionRef,
            const RedPartReceptionCallback_t& redPartReceptionCallbackRef,
            const GreenPartSegmentArrivalCallback_t& greenPartSegmentArrivalCallbackRef,
            std::unique_ptr<MemoryInFiles>& memoryInFilesPtrRef,
            LtpSessionReceiverRecycler& ltpSessionReceiverRecyclerRef);

        /// Local client service ID
        const uint64_t m_clientServiceId;
        /// Maximum number of reception claims per report segment
        uint64_t m_maxReceptionClaims;
        /// Estimated maximum number of bytes to reverse space for (both in-memory and for disk storage).
        /// This is a soft cap to lessen instances of reallocation, the actual space will be expanded if needed.
        uint64_t m_estimatedBytesToReceive;
        /// Maximum number of red data allowed in bytes per red data part
        uint64_t m_maxRedRxBytes;
        /// Maximum retries allowed per report
        uint32_t& m_maxRetriesPerSerialNumberRef;

        /// Report retransmission timer manager, timer mapped by session ID, hashed by session ID
        LtpTimerManager<Ltp::session_id_t, Ltp::hash_session_id_t>& m_timeManagerOfReportSerialNumbersRef;
        /// Report retransmission timer expiry callback
        const LtpTimerManager<Ltp::session_id_t, Ltp::hash_session_id_t>::LtpTimerExpiredCallback_t& m_rsnTimerExpiredCallbackRef;
        /// Pending checkpoint delayed report transmission timer manager, timer mapped by session ID, hashed by session ID
        LtpTimerManager<Ltp::session_id_t, Ltp::hash_session_id_t>& m_timeManagerOfSendingDelayedReceptionReportsRef;
        /// Pending checkpoint delayed report transmission timer expiry callback
        const LtpTimerManager<Ltp::session_id_t, Ltp::hash_session_id_t>::LtpTimerExpiredCallback_t& m_delayedReceptionReportTimerExpiredCallbackRef;

        /// LtpEngine this receiver should be queued for deletion notice function
        const NotifyEngineThatThisReceiverNeedsDeletedCallback_t& m_notifyEngineThatThisReceiverNeedsDeletedCallbackRef;
        /// LtpEngine this receiver has data to send notice function
        const NotifyEngineThatThisReceiversTimersHasProducibleDataFunction_t& m_notifyEngineThatThisReceiversTimersHasProducibleDataFunctionRef;
        /// LtpEngine this receiver has completed a deferred disk operation notice function
        const NotifyEngineThatThisReceiverCompletedDeferredOperationFunction_t& m_notifyEngineThatThisReceiverCompletedDeferredOperationFunctionRef;
        /// Red data part reception callback, invoked only on full red data part reception
        const RedPartReceptionCallback_t& m_redPartReceptionCallbackRef;
        /// Green data segment reception callback, invoked for any partial segment
        const GreenPartSegmentArrivalCallback_t& m_greenPartSegmentArrivalCallbackRef;
        /// Disk memory manager
        std::unique_ptr<MemoryInFiles>& m_memoryInFilesPtrRef;
        /// Recycled data structure manager
        LtpSessionReceiverRecycler& m_ltpSessionReceiverRecyclerRef;

        //session receiver stats
        /// Total number of report segment timer expiry callback invocations
        uint64_t m_numReportSegmentTimerExpiredCallbacks;
        /// Total number of report segments unable to be issued
        uint64_t m_numReportSegmentsUnableToBeIssued;
        /// Total number of reports too large needing-fragmented (when report claims > m_maxReceptionClaims)
        uint64_t m_numReportSegmentsTooLargeAndNeedingSplit;
        /// Total number of report segments produced from too large needing-fragmented reports
        uint64_t m_numReportSegmentsCreatedViaSplit;
        /// Total number of gaps filled by out-of-order data segments
        uint64_t m_numGapsFilledByOutOfOrderDataSegments;
        /// Total number of whole primary report segments sent (only reports when no gaps)
        uint64_t m_numDelayedFullyClaimedPrimaryReportSegmentsSent;
        /// Total number of whole secondary report segments sent (only reports when no gaps)
        uint64_t m_numDelayedFullyClaimedSecondaryReportSegmentsSent;
        /// Total number of out-of-order partial primary report segments
        uint64_t m_numDelayedPartiallyClaimedPrimaryReportSegmentsSent;
        /// Total number of out-of-order partial secondary report segments
        uint64_t m_numDelayedPartiallyClaimedSecondaryReportSegmentsSent;
    };
    
    
    /**
     * Start all stat counters from 0 and initialize flags.
     * Set m_lengthOfRedPart and m_lowestGreenOffsetReceived to UINT16_MAX.
     * Reserve m_estimatedBytesToReceive bytes of space on disk (if using the disk for intermediate storage) or in-memory as a fallback.
     */
    LTP_LIB_EXPORT LtpSessionReceiver(uint64_t randomNextReportSegmentReportSerialNumber,
        const Ltp::session_id_t & sessionId,
        LtpSessionReceiverCommonData& ltpSessionReceiverCommonDataRef);

    /// Clean up active report and pending checkpoint delayed report transmission timers from the shared timer manager
    LTP_LIB_EXPORT ~LtpSessionReceiver();
    
    /** Load next report segment to send.
     *
     * If the reports needing-transmitted is empty, returns immediately with a value of False.
     * Else, the first queued segment is popped from the queue and loaded into the send operation data context, then a report retransmission timer
     * is attempted to be started.
     * @param udpSendPacketInfo The send operation context data to modify.
     * @return True if there is a segment to send and it could be loaded successfully (and thus the send operation context data are modified), or False otherwise.
     * @post If returns True, the argument to udpSendPacketInfo is modified accordingly (see above for details).
     */
    LTP_LIB_EXPORT bool NextDataToSend(UdpSendPacketInfo& udpSendPacketInfo);
    
    /** Get number of currently active timers.
     *.
     * Used by LtpEngine housekeeping timer to detect idle open sessions.
     * @return The number of currently active timers (report retransmission timers PLUS pending checkpoint delayed report transmission timers).
     */
    LTP_LIB_EXPORT std::size_t GetNumActiveTimers() const;
    
    /** Whether it is safe to delete this receiver.
     *
     * A receiver is considered safe for deletion if there are NO disk I/O operations in progress.
     * @return True if it is safe to delete this receiver, or False otherwise.
     */
    LTP_LIB_EXPORT bool IsSafeToDelete() const noexcept;
    
    /** Handle report acknowledgment segment reception.
     *
     * Updates the last segment received timestamp to refresh the idleness status for this receiver.
     * Deletes the report retransmission timer.
     * 1. If the reports needing-transmitted queue is empty AND the there are no report retransmission timers active:
     *   A. If the red or green EOB segment is received AND the red data part reception callback has already been invoked.
     *     If not already marked, marks the receiver for deferred deletion then calls m_notifyEngineThatThisReceiverNeedsDeletedCallbackRef()
     *     with a cancel code of RESERVED to notify the associated LtpEngine for receiver deletion.
     *   B. If the green EOB segment is lost:
     *     ??? (Unspecified)
     * @param reportSerialNumberBeingAcknowledged The associated report serial number.
     * @param headerExtensions The LTP header extensions.
     * @param trailerExtensions The LTP trailer extensions.
     */
    LTP_LIB_EXPORT void ReportAcknowledgementSegmentReceivedCallback(uint64_t reportSerialNumberBeingAcknowledged,
        Ltp::ltp_extensions_t & headerExtensions, Ltp::ltp_extensions_t & trailerExtensions);

    /** Handle data segment reception.
     *
     * Updates the last segment received timestamp to refresh the idleness status for this receiver.
     * 1. If the segment is EOB (either red of green):
     *   Marks reception of an EOB segment.
     * 2. If this is a red segment:
     *   Advances the currently received red data length appropriately.
     *   A. If this is a miscolored segment (m_currentRedLength > m_lowestGreenOffsetReceived):
     *     If not already marked, marks the receiver for deferred deletion then calls m_notifyEngineThatThisReceiverNeedsDeletedCallbackRef()
     *     with a cancel code of MISCOLORED to notify the associated LtpEngine for receiver deletion.
     *   B. If the red data part reception callback has already been invoked:
     *     No further processing is required.
     *   C. If the currently received red data length exceeds the maximum red part length limit:
     *     If not already marked, marks the receiver for deferred deletion then calls m_notifyEngineThatThisReceiverNeedsDeletedCallbackRef()
     *     with a cancel code of SYSTEM_CANCELLED to notify the associated LtpEngine for receiver deletion.
     *   D. If this data segment contains data not previously received (still pending arrival):
     *     a. If using the disk for intermediate storage:
     *       Initiates a deferred disk write operation with LtpSessionReceiver::OnDataSegmentWrittenToDisk() as a completion handler,
     *       resizing our memory block appropriately, the client service data are MOVED for this deferred write.
     *       If the disk I/O operation is queued successfully, the number of active disk I/O operations is incremented.
     *       this branch eventually leads to this function returning True to indicate a pending operation in progress (the deferred disk write).
     *     b. If storing the data in-memory:
     *       The resizing takes place in our in-memory data store but the client service data are COPIED and NOT MOVED.
     *     c. If this data segment JUST NOW filled all reception gaps for a pending checkpoint for delayed report transmission:
     *       Calls LtpSessionReceiver::HandleGenerateAndSendReportSegment() to generate and queue for transmission the appropriate report, then
     *       the pending checkpoint delayed report transmission timer is deleted.
     *   E. If this is a red checkpoint segment:
     *     If the checkpoint serial number OR the associated report serial number are NULL, this segment is invalid no further processing is required.
     *     If this is a redundant segment, no further processing is required.
     *     Sets the lower bound of the associated report (and any subsequent reports if necessary) appropriately depending on the type of
     *     the checkpoint (primary or secondary).
     *     If this is a discretionary checkpoint for which a report should NOT be issued, no further processing is required.
     *     Else, (if this is a discretionary checkpoint for which a report should be issued):
     *     a. [UNIT TESTING]: If we are NOT handling possibly out-of-order reception of data segments:
     *       Calls LtpSessionReceiver::HandleGenerateAndSendReportSegment() to generate and queue for transmission the appropriate report.
     *     b. If we are handling possibly out-of-order reception of data segments:
     *       The segment is added to the pending checkpoint for delayed report transmission and the pending checkpoint delayed report transmission
     *       timer is attempted to be started.
     *   F. If this is a red segment that JUST NOW filled all reception gaps for the entire red part and the red part reception callback
     *   has NOT already been invoked:
     *     If this is NOT a checkpoint AND a report has NOT already been issued (by branch 2.D.c), calls LtpSessionReceiver::HandleGenerateAndSendReportSegment()
     *     to generate and queue for transmission a report covering the entire range of the red data part.
     *     Marks the red part reception callback as completed.
     *     If using the disk for intermediate storage, does nothing and lets the callback be naturally deferred to the asynchronous disk read handler.
     *     Else, calls m_redPartReceptionCallbackRef() to invoke the red part reception callback with data loaded from the in-memory data store.
     * 3. If this is a green segment:
     *   Advances the currently received lowest green offset appropriately.
     *   A. If this is a miscolored segment (m_currentRedLength > m_lowestGreenOffsetReceived):
     *     If not already marked, marks the receiver for deferred deletion then calls m_notifyEngineThatThisReceiverNeedsDeletedCallbackRef()
     *     with a cancel code of MISCOLORED to notify the associated LtpEngine for receiver deletion.
     *   B. If the green data reception callback is set:
     *     Calls m_greenPartSegmentArrivalCallbackRef() to invoke the green data reception callback for this data segment.
     *   C. If this is a green EOB segment:
     *     If this is a fully green session OR the red part reception callback has already been invoked (which indicates full reception
     *     of the red part), if not already marked, marks the receiver for deferred deletion then calls m_notifyEngineThatThisReceiverNeedsDeletedCallbackRef()
     *     with a cancel code of RESERVED to notify the associated LtpEngine for receiver deletion.
     * @param segmentTypeFlags The segment type flags.
     * @param clientServiceDataVec The client service data.
     * @param dataSegmentMetadata The data segment metadata.
     * @param headerExtensions The LTP header extensions.
     * @param trailerExtensions The LTP trailer extensions.
     * @return True if the operation is still in progress on function exit (currently only on asynchronous disk writes), or False otherwise.
     * @post The argument to clientServiceDataVec MIGHT be left in a moved-from state (see above for details) (branch 2.D.a).
     * @post The number of active disk I/O operations MIGHT be incremented (see above for details) (branch 2.D.a).
     * @post If returns False, indicates that the UDP circular index buffer can reduce its size.
     */
    LTP_LIB_EXPORT bool DataSegmentReceivedCallback(uint8_t segmentTypeFlags,
        Ltp::client_service_raw_data_t& clientServiceRawData, const Ltp::data_segment_metadata_t & dataSegmentMetadata,
        Ltp::ltp_extensions_t & headerExtensions, Ltp::ltp_extensions_t & trailerExtensions);
private:
    
    /// Last primary report segment sent iterator
    report_segments_sent_map_t::const_iterator m_itLastPrimaryReportSegmentSent; //std::map<uint64_t, Ltp::report_segment_t> m_mapPrimaryReportSegmentsSent;
    
    //LtpFragmentSet::data_fragment_set_t m_receivedDataFragmentsThatSenderKnowsAboutSet;
    
    
    
    
    
    /// Report retransmission timer context data
    struct rsntimer_userdata_t {
        /// Report segment sent iterator
        report_segments_sent_map_t::const_iterator itMapAllReportSegmentsSent;
        /// Report serial number with active retransmission timer iterator
        std::list<uint64_t>::iterator itReportSerialNumberActiveTimersList;
        /// Number of retries
        uint32_t retryCount;
    };

    
    
    /// Next report segment serial number
    uint64_t m_nextReportSegmentReportSerialNumber;
    /// Currently received red data stored in-memory, if using the disk for intermediate storage see below how to handle loading and accessing data
    padded_vector_uint8_t m_dataReceivedRed;
    /// Allocation size in bytes of our memory block, if 0 the block is invalid, handled by MemoryInFiles::Resize()
    uint64_t m_memoryBlockIdReservedSize; //since its resize rounds up
    /// Our session ID
    const Ltp::session_id_t M_SESSION_ID;
    uint64_t m_lengthOfRedPart;
    /// Currently received lowest green data offset, when (m_currentRedLength > m_lowestGreenOffsetReceived) we have received a miscolored segment
    uint64_t m_lowestGreenOffsetReceived;
    /// Currently received red data length in bytes, when (m_currentRedLength > m_lowestGreenOffsetReceived) we have received a miscolored segment.
    /// Cannot just use variable in (m_dataReceivedRed.size()) because may be using disk instead.
    uint64_t m_currentRedLength;
    /// Receiver common data reference, the data shared by all receivers of the associated LtpEngine
    LtpSessionReceiverCommonData& m_ltpSessionReceiverCommonDataRef;
    /// Our memory block ID, if using the disk for intermediate storage the ID MUST be non-zero, data are loaded in-memory before invocation of completion callbacks
    uint64_t m_memoryBlockId;
    /// Recycled data structures for this session
    LtpSessionReceiverRecycledDataUniquePtr m_ltpSessionReceiverRecycledDataUniquePtr;
public:
    /// Last segment (either data or report acknowledgment) received timestamp, used by LtpEngine housekeeping timer to detect idle open sessions
    boost::posix_time::ptime m_lastSegmentReceivedTimestamp;
private:
    /// Number of system read or write I/O operations currently in progress, for graceful cleanup wait until there are no active disk I/O operations before deleting this receiver
    uint32_t m_numActiveAsyncDiskOperations;
    /// Whether the red part fully received callback has been called, indicates receive completion for the red part data of this session
    bool m_didRedPartReceptionCallback;
    /// Whether deferred deletion of this receiver has been requested (typically on session completed), used to notify the associated LtpEngine
    bool m_didNotifyForDeletion;
    /// Whether we have received an EOB segment (either red or green)
    bool m_receivedEobFromGreenOrRed;
public:
    /// Whether the cancellation callback has been invoked, used to prevent against multiple executions of the session completion procedure
    bool m_calledCancelledCallback;
};

#endif // LTP_SESSION_RECEIVER_H

