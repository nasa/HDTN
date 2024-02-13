/**
 * @file LtpEngine.h
 * @author  Brian Tomko <brian.j.tomko@nasa.gov>
 *
 * @copyright Copyright (c) 2021 United States Government as represented by
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
 * This LtpEngine class manages all the active LTP sending or receiving sessions.
 */

#ifndef LTP_ENGINE_H
#define LTP_ENGINE_H 1

#include <boost/thread.hpp>
#include "LtpFragmentSet.h"
#include "Ltp.h"
#include "LtpRandomNumberGenerator.h"
#include "LtpSessionReceiver.h"
#include "LtpSessionSender.h"
#include "LtpNoticesToClientService.h"
#include "LtpClientServiceDataToSend.h"
#include "LtpSessionRecreationPreventer.h"
#include "LtpEngineConfig.h"
#include "TokenRateLimiter.h"
#include "BundleCallbackFunctionDefines.h"
#include "MemoryInFiles.h"
#include "FreeListAllocator.h"
#include <unordered_map>
#include <queue>
#include <memory>
#include <atomic>
#include <boost/core/noncopyable.hpp>

class CLASS_VISIBILITY_LTP_LIB LtpEngine : private boost::noncopyable {
private:
    LtpEngine() = delete;
public:
    /// Transmission request context data
    struct transmission_request_t {
        /// Remote client service ID
        uint64_t destinationClientServiceId;
        /// Remove LTP engine ID
        uint64_t destinationLtpEngineId;
        /// Client service data to send
        LtpClientServiceDataToSend clientServiceDataToSend;
        /// Red part data length in bytes
        uint64_t lengthOfRedPart;
        /// Session attached client service data
        std::shared_ptr<LtpTransmissionRequestUserData> userDataPtr;
    };
    
    /// Session cancellation segment context data
    struct cancel_segment_timer_info_t {
        /// Session ID
        Ltp::session_id_t sessionId;
        /// Reason code
        CANCEL_SEGMENT_REASON_CODES reasonCode;
        /// Whether the cancellation segment was issued by the sender (if False, issued by receiver)
        bool isFromSender;
        /// Number of retries
        uint8_t retryCount;
    };
    
    /**
     * Set RTT to (ltpRxOrTxCfg.oneWayLightTime * 2) + (ltpRxOrTxCfg.oneWayMarginTime * 2).
     * Set housekeeping timer interval to 1000ms.
     * initialize sender and receiver common data.
     * Bind necessary callbacks.
     * Call LtpEngine::SetMtuReportSegment().
     * Call LtpEngine::UpdateRate().
     * Call LtpEngine::Reset().
     * Start the housekeeping timer asynchronously with LtpEngine::OnHousekeeping_TimerExpired() as a completion handler.
     * If using the disk for intermediate storage AND we are not unit testing (if using a dedicated thread), initialize the disk memory manager.
     * if using a dedicated thread, initialize the dedicated I/O thread.
     */
    LTP_LIB_EXPORT LtpEngine(const LtpEngineConfig& ltpRxOrTxCfg, const uint8_t engineIndexForEncodingIntoRandomSessionNumber, bool startIoServiceThread);

    /**
     * If the dedicated I/O thread is active, clear the housekeeping timer, initiate an asynchronous reset to LtpEngine::Reset(), reset the
     * explicit work object, then join and cleanup the dedicated I/O thread.
     */
    LTP_LIB_EXPORT virtual ~LtpEngine();

    /** Perform reset.
     *
     * Clear all active transmission and reception sessions and reserve space for (M_MAX_SIMULTANEOUS_SESSIONS << 1) sessions for both.
     * Reset the Rx state machine.
     * Reset all timer managers.
     * Clear all segment queues.
     * Start all stat counters back from 0.
     */
    LTP_LIB_EXPORT virtual void Reset();
    
    /** Set checkpoint every Nth data packet across all senders.
     *
     * (see docs for LtpEngineConfig::checkpointEveryNthDataPacketSender).
     * @param checkpointEveryNthDataPacketSender The value to set.
     */
    LTP_LIB_EXPORT void SetCheckpointEveryNthDataPacketForSenders(uint64_t checkpointEveryNthDataPacketSender);
    
    /** Get the engine index.
     *
     * @return The engine index.
     */
    LTP_LIB_EXPORT uint8_t GetEngineIndex();

    /** Issue a transmission request.
     *
     * Calls LtpEngine::TransmissionRequest(trcd->destinationClientServiceId, trcd->destinationLtpEngineId, trcd->clientServiceDataToSend,
     *                                      trcd->userDataPtr, trcd->lengthOfRedPart).
     * @param transmissionRequest The transmission request context data.
     * @post The arguments to (trcd->clientServiceDataToSend) and (trcd->userDataPtr) are left in a moved-from state.
     */
    LTP_LIB_EXPORT void TransmissionRequest(std::shared_ptr<transmission_request_t> & transmissionRequest);
    
    /** Initiate a request to issue a transmission request (thread-safe).
     *
     * Initiates an asynchronous request to LtpEngine::TransmissionRequest(transmissionRequest).
     * @param transmissionRequest The transmission request context data.
     * @post The argument to transmissionRequest is left in a moved-from state.
     */
    LTP_LIB_EXPORT void TransmissionRequest_ThreadSafe(std::shared_ptr<transmission_request_t> && transmissionRequest);
    
    /** Issue a transmission request.
     *
     * If using the disk for intermediate storage, calls MemoryInFiles::AllocateNewWriteMemoryBlock() to allocate a new memory block, if the
     * new block could be created successfully, calls MemoryInFiles::WriteMemoryAsync() with LtpEngine::OnTransmissionRequestDataWrittenToDisk()
     * as a completion handler, attempting to initiate a deferred disk write, writing the client service data to send before the transmission request
     * is initiated.
     *
     * Else, if NOT writing to disk, calls LtpEngine::DoTransmissionRequest() directly.
     * @param destinationClientServiceId The remote client service ID.
     * @param destinationLtpEngineId The remote LTP engine ID.
     * @param clientServiceDataToSend The client service data to send.
     * @param userDataPtrToTake The attached user data.
     * @param lengthOfRedPart The red part data length in bytes.
     * @post The arguments to clientServiceDataToSend and userDataPtrToTake are left in a moved-from state.
     */
    LTP_LIB_EXPORT void TransmissionRequest(uint64_t destinationClientServiceId, uint64_t destinationLtpEngineId,
        LtpClientServiceDataToSend && clientServiceDataToSend, std::shared_ptr<LtpTransmissionRequestUserData> && userDataPtrToTake, uint64_t lengthOfRedPart);
    
    /** Issue a cancellation request.
     *
     * Copies the client service data to send.
     * Calls LtpEngine::TransmissionRequest(destinationClientServiceId, destinationLtpEngineId,
     *                                      std::vector<uint8_t>(clientServiceDataToCopyAndSend, clientServiceDataToCopyAndSend + length),
     *                                      userDataPtrToTake, lengthOfRedPart).
     * @param destinationClientServiceId The remote client service ID.
     * @param destinationLtpEngineId The remote LTP engine ID.
     * @param clientServiceDataToCopyAndSend The client service data to send.
     * @param length The client service data length in bytes.
     * @param userDataPtrToTake The attached user data.
     * @param lengthOfRedPart The red part data length in bytes.
     * @post The argument to userDataPtrToTake is left in a moved-from state.
     */
    LTP_LIB_EXPORT void TransmissionRequest(uint64_t destinationClientServiceId, uint64_t destinationLtpEngineId,
        const uint8_t * clientServiceDataToCopyAndSend, uint64_t length, std::shared_ptr<LtpTransmissionRequestUserData> && userDataPtrToTake, uint64_t lengthOfRedPart);
    
    /** Issue a transmission request.
     *
     * Copies the client service data to send.
     * Calls LtpEngine::TransmissionRequest(destinationClientServiceId, destinationLtpEngineId,
     *                                      std::vector<uint8_t>(clientServiceDataToCopyAndSend, clientServiceDataToCopyAndSend + length),
     *                                      nullptr, lengthOfRedPart).
     * @param destinationClientServiceId The remote client service ID.
     * @param destinationLtpEngineId The remote LTP engine ID.
     * @param clientServiceDataToCopyAndSend The client service data to send.
     * @param length The client service data length in bytes.
     * @param lengthOfRedPart The red part data length in bytes.
     */
    LTP_LIB_EXPORT void TransmissionRequest(uint64_t destinationClientServiceId, uint64_t destinationLtpEngineId,
        const uint8_t * clientServiceDataToCopyAndSend, uint64_t length, uint64_t lengthOfRedPart);
private:
    /** Issue a transmission request.
     *
     * Marks that transmission request should serve as a ping.
     * Attempts to append a new transmission session to the active transmission sessions queue, if the insertion operation fails returns immediately.
     * Else, appends the newly created sender to the senders needing first-pass data sent queue, then if the session start callback
     * is set, calls LtpEngine::m_sessionStartCallback() to invoke the session start callback.
     * Finally calls LtpEngine::TrySaturateSendPacketPipeline() to resume processing.
     * @param destinationClientServiceId The remote client service ID.
     * @param destinationLtpEngineId The remote LTP engine ID.
     * @param clientServiceDataToSend The client service data to send.
     * @param userDataPtrToTake The attached user data.
     * @param lengthOfRedPart The red part data length in bytes.
     * @param memoryBlockId The memory block ID.
     */
    LTP_LIB_NO_EXPORT void DoTransmissionRequest(uint64_t destinationClientServiceId, uint64_t destinationLtpEngineId,
        LtpClientServiceDataToSend&& clientServiceDataToSend, std::shared_ptr<LtpTransmissionRequestUserData>&& userDataPtrToTake, uint64_t lengthOfRedPart, uint64_t memoryBlockId);
    
    /** Handle transmission request disk write completion.
     *
     * Clears the client service data to send from memory to free up space since the data have been written to disk.
     * Calls DoTransmissionRequest(destinationClientServiceId, destinationLtpEngineId,
     *                             *clientServiceDataToSendPtrToTake, userDataPtrToTake,
     *                             lengthOfRedPart, memoryBlockId).
     *
     * If the successful session data disk write completion callback is set AND we are within the maximum number of queued disk operation
     * completion callbacks, calls LtpEngine::m_onSuccessfulBundleSendCallback() to invoke the successful session data disk write completion callback.
     * Else, queues the callback context data to the pending successful bundle send callback context data queue.
     * @param destinationClientServiceId The remote client service ID.
     * @param destinationLtpEngineId The remote LTP engine ID.
     * @param clientServiceDataToSendPtrToTake The client service data to send.
     * @param userDataPtrToTake The attached user data.
     * @param lengthOfRedPart The red part data length in bytes.
     * @param memoryBlockId The memory block ID.
     * @post The arguments to (clientServiceDataToSendPtrToTake->m_userData) and userDataPtrToTake are left in a moved-from state.
     */
    LTP_LIB_NO_EXPORT void OnTransmissionRequestDataWrittenToDisk(uint64_t destinationClientServiceId, uint64_t destinationLtpEngineId,
        std::shared_ptr<LtpClientServiceDataToSend>& clientServiceDataToSendPtrToTake, std::shared_ptr<LtpTransmissionRequestUserData>& userDataPtrToTake,
        uint64_t lengthOfRedPart, uint64_t memoryBlockId);
public:
    /** Issue a cancellation request.
     *
     * If we are issuing a cancellation request as a sender AND the session does NOT exist in the active transmission sessions, returns immediately.
     * Else, calls LtpEngine::EraseTxSession() to remove the session from the active transmission sessions.
     * Queues a cancellation segment with a cancel code of USER_CANCELLED for transmission, then calls LtpEngine::TrySaturateSendPacketPipeline()
     * to dequeue the cancellation segment.
     *
     * If we are issuing a cancellation request as a receiver AND the session does NOT exist in the active reception sessions, returns immediately.
     * Else, if the session is safe to delete, calls ltpEngine::EraseRxSession() to remove the session from the active reception sessions, if the
     * session is NOT safe to delete, appends it to the receivers with pending operations needing deleted queue.
     * Queues a cancellation segment with a cancel code of USER_CANCELLED for transmission, then calls LtpEngine::TrySaturateSendPacketPipeline()
     * to dequeue the cancellation segment.
     * @param sessionId The session ID.
     * @return True if a cancellation segment was queued successfully, or False otherwise.
     */
    LTP_LIB_EXPORT bool CancellationRequest(const Ltp::session_id_t & sessionId);
    
    /** Initiate a request to issue a cancellation request (thread-safe).
     *
     * Initiates an asynchronous request to LtpEngine::CancellationRequest().
     * @param sessionId The session ID.
     */
    LTP_LIB_EXPORT void CancellationRequest_ThreadSafe(const Ltp::session_id_t & sessionId);
    
    /** Set the session start callback.
     *
     * @param callback The callback to set.
     */
    LTP_LIB_EXPORT void SetSessionStartCallback(const SessionStartCallback_t & callback);
    
    /** Set the red data part reception callback.
     *
     * @param callback The callback to set.
     */
    LTP_LIB_EXPORT void SetRedPartReceptionCallback(const RedPartReceptionCallback_t & callback);
    
    /** Set the green data segment reception callback.
     *
     * @param callback The callback to set.
     */
    LTP_LIB_EXPORT void SetGreenPartSegmentArrivalCallback(const GreenPartSegmentArrivalCallback_t & callback);
    
    /** Set the reception session cancellation callback.
     *
     * @param callback The callback to set.
     */
    LTP_LIB_EXPORT void SetReceptionSessionCancelledCallback(const ReceptionSessionCancelledCallback_t & callback);
    
    /** Set the transmission session completion callback.
     *
     * @param callback The callback to set.
     */
    LTP_LIB_EXPORT void SetTransmissionSessionCompletedCallback(const TransmissionSessionCompletedCallback_t & callback);
    
    /** Set the initial data transmission completion callback.
     *
     * @param callback The callback to set.
     */
    LTP_LIB_EXPORT void SetInitialTransmissionCompletedCallback(const InitialTransmissionCompletedCallback_t & callback);
    
    /** Set the transmission session cancellation callback.
     *
     * @param callback The callback to set.
     */
    LTP_LIB_EXPORT void SetTransmissionSessionCancelledCallback(const TransmissionSessionCancelledCallback_t & callback);

    /** Set the failed byte buffer session data disk write completion callback.
     *
     * @param callback The callback to set.
     */
    LTP_LIB_EXPORT void SetOnFailedBundleVecSendCallback(const OnFailedBundleVecSendCallback_t& callback);
    
    /** Set the failed ZMQ session data disk write completion callback.
     *
     * @param callback The callback to set.
     */
    LTP_LIB_EXPORT void SetOnFailedBundleZmqSendCallback(const OnFailedBundleZmqSendCallback_t& callback);
    
    /** Set the successful session disk write completion callback.
     *
     * @param callback The callback to set.
     */
    LTP_LIB_EXPORT void SetOnSuccessfulBundleSendCallback(const OnSuccessfulBundleSendCallback_t& callback);
    
    /** Set the outduct link status event callback.
     *
     * @param callback The callback to set.
     */
    LTP_LIB_EXPORT void SetOnOutductLinkStatusChangedCallback(const OnOutductLinkStatusChangedCallback_t& callback);
    
    /** Set the outduct UUID.
     *
     * @param userAssignedUuid The outduct UUID to set.
     */
    LTP_LIB_EXPORT void SetUserAssignedUuid(uint64_t userAssignedUuid);
    
    /** Handle packet chunk reception.
     *
     * Calls Ltp::HandleReceivedChars() feeding the chunk to the Rx state machine for processing.
     * If processing was NOT successful, resets the Rx state machine.
     * Else, if the processing was successful BUT this is the last chunk of the packet AND we are NOT at the beginning state, resets the Rx state machine,
     * the processing at this point is marked unsuccessful.
     * If the processing operation is still in progress (due to a deferred disk write), nothing more to do, just wait until the next eventual
     * invocation of this function which will occur in the chain of events after successful disk write completion.
     * Else, if the processing operation has completed synchronously, calls LtpEngine::PacketInFullyProcessedCallback() with the success status
     * of the processing.
     * @param isLastChunkOfPacket Whether this is the last chunk of the packet.
     * @param data The data begin pointer.
     * @param size The data size in bytes.
     * @param sessionOriginatorEngineIdDecodedCallbackPtr The session originator engine ID decoded callback.
     * @return True if the processing was successful, or False otherwise.
     */
    LTP_LIB_EXPORT bool PacketIn(const bool isLastChunkOfPacket, const uint8_t * data, const std::size_t size,
        Ltp::SessionOriginatorEngineIdDecodedCallback_t * sessionOriginatorEngineIdDecodedCallbackPtr = NULL);
   
    /** Handle packet chunk reception (for unit testing).
     *
     * For each chunk, calls LtpEngine::PacketIn(isLastChunkOfPacket, constBufferVec.data(), constBufferVec.size()) with the last buffer
     * marked as isLastChunkOfPacket.
     * @param constBufferVec The data chunk buffers.
     * @return True if the processing for ALL chunks was successful, or False otherwise.
     */
    LTP_LIB_EXPORT bool PacketIn(const std::vector<boost::asio::const_buffer> & constBufferVec); //for testing

    /** Initiate a request to handle packet chunk reception (thread-safe).
     *
     * Initiates an asynchronous request to LtpEngine::PacketIn(true, data, size, sessionOriginatorEngineIdDecodedCallbackPtr).
     * @param data The data begin pointer.
     * @param size The data size in bytes.
     * @param sessionOriginatorEngineIdDecodedCallbackPtr The session originator engine ID decoded callback.
     */
    LTP_LIB_EXPORT void PacketIn_ThreadSafe(const uint8_t * data, const std::size_t size, Ltp::SessionOriginatorEngineIdDecodedCallback_t * sessionOriginatorEngineIdDecodedCallbackPtr = NULL);
    
    /** Initiate a request to handle packet chunk reception (thread-safe) (for unit testing).
     *
     * Initiates an asynchronous request to LtpEngine::PacketIn(constBufferVec).
     * @param constBufferVec The data chunk buffers.
     */
    LTP_LIB_EXPORT void PacketIn_ThreadSafe(const std::vector<boost::asio::const_buffer> & constBufferVec); //for testing

    /** Initiate a request to emit an outduct link down event (thread-safe).
     *
     * Initiates an asynchronous request to LtpEngine::DoExternalLinkDownEvent().
     */
    LTP_LIB_EXPORT void PostExternalLinkDownEvent_ThreadSafe();
    
    /** Load next packet to send.
     *
     * Data priority (in descending order, 1 is highest priority).
     * 1. Senders needing deleted queue:
     *   For each queued sender:
     *   A. If a call to LtpSessionSender::NextTimeCriticalDataToSend() loads a new packet in:
     *     The resulting critical data are loaded as the next packet to send and the sender is NOT popped from the queue, then returns
     *     with a value of True (kept so consecutive invocations exhaust the critical data it has left to send).
     *   B. If the sender does not have critical data left to send AND this is a failed session:
     *     a. If the failed byte buffer session data disk write completion callback is set:
     *       If the client service data to send are safe to move, calls LtpEngine::m_onFailedBundleVecSendCallback() passing them by reference.
     *       Else, calls LtpEngine::m_onFailedBundleVecSendCallback() passing them by copy.
     *     b. If the failed ZMQ session data disk write completion callback is set:
     *       If the client service data to send are safe to move, calls LtpEngine::m_onFailedBundleZmqSendCallback() passing them by reference.
     *       Else, calls LtpEngine::m_onFailedBundleZmqSendCallback() passing them by copy.
     *   C. If the sender does not have critical data left to send AND this is NOT a failed session:
     *     If the successful session data disk write completion callback is set AND has NOT already been invoked, calls LtpEngine::m_onSuccessfulBundleSendCallback().
     *   D. If the sender does not have critical data left to send, regardless if this is a failed session or not:
     *     If the session exists in the active transmission sessions, calls LtpEngine::EraseTxSession() to remove the session from the active transmission sessions.
     *     Finally, the sender is popped from the senders needing deleted queue.
     * 2. Pending successful bundle send callback context data queue:
     *   While we are within the maximum number of queued disk operation completion callbacks:
     *   A. If the successful session data disk write completion callback is set:
     *     For each queued operation callback, calls LtpEngine::m_onSuccessfulBundleSendCallback() to invoke it, then pops it from the queue.
     * 3. Receivers with pending operations needing deleted queue:
     *   For each queued receiver:
     *   A. If the session exists in the active reception sessions AND is safe to delete:
     *     The session is transferred to the receivers needing deleted queue.
     *   B. If the session exists in the active reception sessions AND is NOT safe to delete:
     *     Breaks out of this branch and stops flushing the queue, meaning if not empty, the front of the queue will always be a receiver not safe to delete.
     *   C. Finally:
     *     The receiver is popped from the receivers with pending operations needing deleted queue.
     * 4. Receivers needing deleted queue:
     *   A. If the session exists in the active reception sessions AND is safe to delete:
     *     Calls LtpEngine::EraseRxSession() to remove the session from the active reception sessions queue.
     *   B. If the session exists in the active reception sessions AND is NOT safe to delete:
     *     The session is transferred to the receivers with pending operations needing deleted queue.
     *   C. Finally:
     *     The receiver is popped from the receivers needing deleted queue.
     * 5. Cancellation segment context data queue:
     *   If the queue is NOT empty, the first queued cancellation segment is loaded as the next packet to send, a cancellation segment
     *   retransmission timer is started, and the segment is popped from the queue, finally returns with a value of True.
     * 6. Closed sessions data to send queue:
     *   If the queue is NOT empty, the first queued segment is loaded as the next packet to send, the segment is popped from the queue,
     *   finally returns with a value of True (no need for a retransmission timer since we are responding to a closed session).
     * 7. Senders needing critical data sent queue:
     *   A. If a call to LtpSessionSender::NextTimeCriticalDataToSend() loads a new packet in:
     *     The resulting critical data are loaded as the next packet to send and the sender is NOT popped from the queue, then returns
     *     with a value of True (kept so consecutive invocations exhaust the critical data it has left to send).
     *   B. If the sender does NOT have critical data left to send OR the session does NOT exist in the active transmission sessions:
     *     The sender is popped from the queue.
     * 8. Receivers needing data sent queue:
     *   A. If a call to LtpSessionReceiver::NextDataToSend() loads a new packet in:
     *     The resulting data are loaded as the next packet to send and the receiver is NOT popped from the queue, then returns
     *     with a value of True (kept so consecutive invocations exhaust the data it has left to send).
     *   B. If the receiver does NOT have data left to send OR the session does NOT exist in the active transmission sessions:
     *     The receiver is popped from the queue.
     * 9. Senders needing first-pass data sent queue:
     *   A. If a call to LtpSessionSender::NextFirstPassDataToSend() loads a new packet in:
     *     The resulting first-pass data are loaded as the next packet to send and the sender is NOT popped from the queue, then returns
     *     with a value of True (kept so consecutive invocations exhaust the first-pass data it has left to send).
     *   B. If the sender does NOT have first-pass data left to send OR the session does NOT exist in the active transmission sessions:
     *     The sender is popped from the queue.
     * 10. [System]: If execution has reached this point.
     *   Returns with a value of False indicating there is nothing to send and leaves the send operation context data unmodified.
     * @param udpSendPacketInfo The send operation context data to modify.
     * @return True if there is a packet to send and it could be loaded successfully (and thus the send operation context data are modified), or False otherwise.
     * @post If returns True, the argument to udpSendPacketInfo is modified accordingly (see above for details).
     */
    LTP_LIB_EXPORT bool GetNextPacketToSend(UdpSendPacketInfo& udpSendPacketInfo);

    /** Get the number of active reception sessions.
     *
     * @return The number of active reception sessions.
     */
    LTP_LIB_EXPORT std::size_t NumActiveReceivers() const;
    
    /** Get the number of active transmission sessions.
     *
     * @return The number of active transmission sessions.
     */
    LTP_LIB_EXPORT std::size_t NumActiveSenders() const;
    
    /** Get the maximum number of sessions in pipeline.
     *
     * @return M_MAX_SESSIONS_IN_PIPELINE.
     */
    LTP_LIB_EXPORT uint64_t GetMaxNumberOfSessionsInPipeline() const;
    
    /** Initiate a request to set the maximum bit rate (thread-safe).
     *
     * Initiates an asynchronous request to LtpEngine::SetRate().
     * @param maxSendRateBitsPerSecOrZeroToDisable The maximum bit rate to set.
     */
    LTP_LIB_EXPORT void SetRate_ThreadSafe(const uint64_t maxSendRateBitsPerSecOrZeroToDisable);

    /** Initiate a request to set the sender ping (thread-safe).
     *
     * Initiates an asynchronous request to LtpEngine::SetPing().
     * @param senderPingSecondsOrZeroToDisable The number of seconds between ltp session sender pings during times of zero data segment activity.
     */
    LTP_LIB_EXPORT void SetPing_ThreadSafe(const uint64_t senderPingSecondsOrZeroToDisable);

    /** Calls SetPing_ThreadSafe with param senderPingSecondsOrZeroToDisable set to the original config file value.
     */
    LTP_LIB_EXPORT void SetPingToDefaultConfig_ThreadSafe();
    
    /** Initiate a request to set the RTT time reference across all senders/receivers (thread-safe).
     *
     * Initiates an asynchronous request to LtpEngine::SetDelays().
     * @param oneWayLightTime
     * @param oneWayMarginTime
     * @param updateRunningTimers
     */
    LTP_LIB_EXPORT void SetDelays_ThreadSafe(const boost::posix_time::time_duration& oneWayLightTime, const boost::posix_time::time_duration& oneWayMarginTime, bool updateRunningTimers);
    
    /** Initiate a request to set the out-of-order transmission/reception compensation delays (thread-safe).
     *
     * Initiates an asynchronous request to LtpEngine::SetDeferDelays().
     * @param delaySendingOfReportSegmentsTimeMsOrZeroToDisable The delay for receivers.
     * @param delaySendingOfDataSegmentsTimeMsOrZeroToDisable The delay for senders.
     */
    LTP_LIB_EXPORT void SetDeferDelays_ThreadSafe(const uint64_t delaySendingOfReportSegmentsTimeMsOrZeroToDisable, const uint64_t delaySendingOfDataSegmentsTimeMsOrZeroToDisable);
    
    /** Initiate a request to set the MTU constraint shared across all receivers (thread-safe).
     *
     * Initiates an asynchronous request to LtpEngine::SetMtuReportSegment().
     * @param mtuReportSegment The MTU constraint to set.
     */
    LTP_LIB_EXPORT void SetMtuReportSegment_ThreadSafe(uint64_t mtuReportSegment);
    
    /** Initiate a request to set the MTU constraint shared across all senders (thread-safe).
     *
     * Initiates an asynchronous request to LtpEngine::SetMtuDataSegment().
     * @param mtuDataSegment The MTU constraint to set.
     */
    LTP_LIB_EXPORT void SetMtuDataSegment_ThreadSafe(uint64_t mtuDataSegment);
protected:
    /** Set the maximum bit rate.
     *
     * Calls TokenRateLimiter::SetRate().
     * (see docs for LtpEngineConfig::maxSendRateBitsPerSecOrZeroToDisable).
     * @param maxSendRateBitsPerSecOrZeroToDisable The maximum bit rate to set.
     */
    LTP_LIB_EXPORT void SetRate(const uint64_t maxSendRateBitsPerSecOrZeroToDisable);

    /** Set or disable the sender ping.
     *
     * @param senderPingSecondsOrZeroToDisable The number of seconds between ltp session sender pings during times of zero data segment activity.
     */
    LTP_LIB_EXPORT void SetPing(const uint64_t senderPingSecondsOrZeroToDisable);

    /** Calls SetPing with param senderPingSecondsOrZeroToDisable set to the original config file value.
     */
    LTP_LIB_EXPORT void SetPingToDefaultConfig();

    /** Set the RTT time reference across all senders/receivers.
     *
     * Recalculates all time references affected by a change in RTT.
     * If should update the running timers, calls LtpTimerManager::AdjustRunningTimers() on all the timer managers.
     * @param oneWayLightTime The one way light time.
     * @param oneWayMarginTime The one way margin time.
     * @param updateRunningTimers Whether the change should update the currently running timers.
     */
    LTP_LIB_EXPORT void SetDelays(const boost::posix_time::time_duration& oneWayLightTime, const boost::posix_time::time_duration& oneWayMarginTime, bool updateRunningTimers);
    
    /** Set the out-of-order transmission/reception compensation delays.
     *
     * (see docs for LtpEngineConfig::delaySendingOfReportSegmentsTimeMsOrZeroToDisable and LtpEngineConfig::delaySendingOfDataSegmentsTimeMsOrZeroToDisable).
     * @param delaySendingOfReportSegmentsTimeMsOrZeroToDisable The delay for receivers.
     * @param delaySendingOfDataSegmentsTimeMsOrZeroToDisable The delay for senders.
     * @post If the receiver delay feature is disabled, delaySendingOfReportSegmentsTimeMsOrZeroToDisable is set to (boost::posix_time::special_values::not_a_date_time).
     * @post If the sender delay feature is disabled, delaySendingOfDataSegmentsTimeMsOrZeroToDisable is set to (boost::posix_time::special_values::not_a_date_time).
     */
    LTP_LIB_EXPORT void SetDeferDelays(const uint64_t delaySendingOfReportSegmentsTimeMsOrZeroToDisable, const uint64_t delaySendingOfDataSegmentsTimeMsOrZeroToDisable);
    
    /** Set the MTU constraint shared across all receivers.
     *
     * Calculates the maximum number of report claims per report segment.
     * @param mtuReportSegment The MTU constraint to set.
     */
    LTP_LIB_EXPORT void SetMtuReportSegment(uint64_t mtuReportSegment);
    
    /** Set the MTU constraint shared across all senders.
     *
     * @param mtuDataSegment The MTU constraint to set.
     */
    LTP_LIB_EXPORT void SetMtuDataSegment(uint64_t mtuDataSegment);

    /** Handle packet processing completion.
     *
     * Virtual function, default implementation is a no-op.
     * @param success Whether the processing was successful.
     */
    LTP_LIB_EXPORT virtual void PacketInFullyProcessedCallback(bool success);
    
    /** Perform a SendPacket operation.
     *
     * Virtual function, default implementation is a no-op.
     * @param constBufferVec The data buffers to send.
     * @param underlyingDataToDeleteOnSentCallback The underlying data buffers shared pointer.
     * @param underlyingCsDataToDeleteOnSentCallback The underlying client service data to send shared pointer, should be nullptr.
     */
    LTP_LIB_EXPORT virtual void SendPacket(const std::vector<boost::asio::const_buffer>& constBufferVec,
        std::shared_ptr<std::vector<std::vector<uint8_t> > > && underlyingDataToDeleteOnSentCallback,
        std::shared_ptr<LtpClientServiceDataToSend>&& underlyingCsDataToDeleteOnSentCallback);
    
    /** Perform a SendPackets operation.
     *
     * Virtual function, default implementation is a no-op.
     * @param constBufferVecs The vector of data buffers to send.
     * @param underlyingDataToDeleteOnSentCallback The vector of underlying data buffers shared pointers.
     * @param underlyingCsDataToDeleteOnSentCallback The vector of underlying client service data to send shared pointers.
     */
    LTP_LIB_EXPORT virtual void SendPackets(std::shared_ptr<std::vector<UdpSendPacketInfo> >&& udpSendPacketInfoVecSharedPtr, const std::size_t numPacketsToSend);
    
    /** Initiate a request to handle a SendPackets operation completion (thread-safe).
     *
     * Initiates an asynchronous request to LtpEngine::OnSendPacketsSystemCallCompleted_NotThreadSafe().
     */
    LTP_LIB_EXPORT void OnSendPacketsSystemCallCompleted_ThreadSafe();
private:
    /** Initiate a request to saturate the send packet pipeline (thread-safe).
     *
     * Initiates an asynchronous request to LtpEngine::TrySaturateSendPacketPipeline().
     */
    LTP_LIB_NO_EXPORT void SignalReadyForSend_ThreadSafe();
    
    /** Try saturate the SendPacket pipeline.
     *
     * Loops while ltpEngine::TrySendPacketIfAvailable().
     */
    LTP_LIB_NO_EXPORT void TrySaturateSendPacketPipeline();
    
    /** Try perform a SendPacket operation.
     *
     * If the maximum number of pending SendPacket operations has been exceeded OR we are NOT using a dedicated I/O thread (typically in
     * unit tests), returns immediately.
     * Else, the processing goes as follows:
     * 1. If rate limiting is enabled and we CANNOT take tokens to process at the moment:
     *   There is nothing to send for now, calls LtpEngine::TryRestartTokenRefreshTimer() to restart the token refresh timer, which will then call
     *   this function when tokens are available for processing.
     * 2. If there are tokens to process AND batch processing is NOT enabled:
     *   A. If a call to LtpEngine::GetNextPacketToSend() does NOT load new data to send (there are no data available to send):
     *     No further processing is required.
     *   B. If there are data to send AND rate limiting is enabled:
     *     Takes N tokens from the rate limiter and calls LtpEngine::TryRestartTokenRefreshTimer() to restart the token refresh timer.
     *   C. Finally:
     *     If the data to send need to be read from disk, calls MemoryInFiles::ReadMemoryAsync() to initiate a deferred disk single-read with
     *     LtpEngine::OnDeferredReadCompleted() as the completion handler.
     *     Else, if the data are already in-memory, calls LtpEngine::SendPacket() on the data loaded for sending.
     * 3. If there are tokens to process AND batch processing is enabled:
     *   For each packet that could be loaded by successive calls to LtpEngine::GetNextPacketToSend() up until the configured maximum number
     *   of packets per system call, performs the same processing as (branch 2) on a multi-packet scale, meaning if data need to be read from
     *   disk, calls MemoryInFiles::ReadMemoryAsync() with LtpEngine::OnDeferredMultiReadCompleted() as the completion handler or if all data
     *   are already in-memory, calls LtpEngine::SendPackets() instead.
     * 4. If we are using the disk for intermediate storage AND there are memory blocks queued for deletion:
     *   For each memory block queued for deletion, calls MemoryInFiles::DeleteMemoryBlock() and pops it from the queue.
     * @return The number of JUST NOW SendPacket operations queued successfully.
     * @post Advances the number of pending SendPackets operations accordingly (see above for details).
     */
    LTP_LIB_NO_EXPORT bool TrySendPacketIfAvailable();
    
    /** Handle deferred disk single-read operation.
     *
     * If the single-read operation was successful, calls SendPacket(constBufferVec, underlyingDataToDeleteOnSentCallback, nullClientServiceData).
     * @param success Whether the single-read operation was successful.
     * @param constBufferVec The data buffers to send.
     * @param underlyingDataToDeleteOnSentCallback The underlying data buffers shared pointer.
     */
    LTP_LIB_NO_EXPORT void OnDeferredReadCompleted(bool success, const std::vector<boost::asio::const_buffer>& constBufferVec,
        std::shared_ptr<std::vector<std::vector<uint8_t> > >& underlyingDataToDeleteOnSentCallback);
    
    /** Handle deferred disk multi-read operation.
     *
     * If the multi-read operation was successful, calls LtpEngine::SendPackets(constBufferVecs, underlyingDataToDeleteOnSentCallbackVec, underlyingCsDataToDeleteOnSentCallbackVec).
     * @param success Whether the multi-read operation was successful.
     * @param constBufferVecs The vector of data buffers to send.
     * @param underlyingDataToDeleteOnSentCallbackVec The vector of underlying data buffers shared pointers.
     * @param underlyingCsDataToDeleteOnSentCallbackVec The vector of underlying client service data to send shared pointers.
     */
    LTP_LIB_NO_EXPORT void OnDeferredMultiReadCompleted(bool success, std::shared_ptr<std::vector<UdpSendPacketInfo> >& udpSendPacketInfoVecSharedPtr, const std::size_t numPacketsToSend);

    /** Handle cancellation segment reception.
     *
     * If the segment was issued by the sender AND the target session exists in the active reception sessions, if the cancellation callback is set
     * AND has NOT already been invoked for this session, calls LtpEngine::m_receptionSessionCancelledCallback() with a cancel code of reasonCode
     * to invoke the cancellation callback.
     * If the session is safe to delete calls LtpEngine::EraseRxSession() to delete the session immediately, else appends the session to the receivers
     * with pending operations needing deleted queue.
     *
     * If the segment was issued by the receiver AND the target session exists in the active transmission sessions, if the cancellation callback is set
     * AND has NOT already been invoked for this session, calls LtpEngine::m_transmissionSessionCancelledCallback() with a cancel code of reasonCode
     * to invoke the cancellation callback.
     * Calls LtpEngine::EraseTxSession() to delete the session.
     *
     * Appends a cancellation acknowledgment segment to the closed sessions data to send queue.
     * Calls LtpEngine::TrySaturateSendPacketPipeline() to resume processing.
     * @param sessionId The session ID.
     * @param reasonCode The reason code.
     * @param isFromSender Whether the cancellation segment was issued by the sender (if False, issued by receiver).
     * @param headerExtensions The LTP header extensions.
     * @param trailerExtensions The LTP trailer extensions.
     */
    LTP_LIB_NO_EXPORT void CancelSegmentReceivedCallback(const Ltp::session_id_t & sessionId, CANCEL_SEGMENT_REASON_CODES reasonCode, bool isFromSender,
        Ltp::ltp_extensions_t & headerExtensions, Ltp::ltp_extensions_t & trailerExtensions);
    
    /** Handle cancellation acknowledgment segment reception.
     *
     * If the segment is directed to the sender and the session originator ID is NOT our engine ID, returns immediately.
     * If the segment is directed to the receiver and the session originator ID is our engine ID (same-engine transfer), returns immediately.
     * Deletes the cancellation segment retransmission timer for the associated cancellation segment.
     * If the segment is directed to the sender AND this is a ping segment, sets the next ping expiry, then if the
     * outduct link status event callback is set, calls LtpEngine::m_onOutductLinkStatusChangedCallback(false, uuid) to emit an outduct link up event.
     * Else, if the segment is directed to the receiver AND the associated cancellation segment has a cancel code of UNREACHABLE, removes the session
     * from the sessions with wrong client service ID queue.
     * @param sessionId The session ID.
     * @param isToSender Whether the cancellation acknowledgment segment is directed to the sender (if False, directed to receiver).
     * @param headerExtensions The LTP header extensions.
     * @param trailerExtensions The LTP trailer extensions.
     * @post If this is an expired ping segment, M_NEXT_PING_START_EXPIRY is set to (now() + M_SENDER_PING_TIME).
     */
    LTP_LIB_NO_EXPORT void CancelAcknowledgementSegmentReceivedCallback(const Ltp::session_id_t & sessionId, bool isToSender,
        Ltp::ltp_extensions_t & headerExtensions, Ltp::ltp_extensions_t & trailerExtensions);
    
    /** Handle report acknowledgment segment reception.
     *
     * If the session originator ID is our engine ID (same-engine transfer), returns immediately.
     * If the target session exists in the active reception sessions, calls LtpSessionReceiver::ReportAcknowledgementSegmentReceivedCallback().
     * Calls LtpEngine::TrySaturateSendPacketPipeline() to resume processing.
     * @param sessionId The session ID.
     * @param reportSerialNumberBeingAcknowledged The associated report serial number.
     * @param headerExtensions The LTP header extensions.
     * @param trailerExtensions The LTP trailer extensions.
     */
    LTP_LIB_NO_EXPORT void ReportAcknowledgementSegmentReceivedCallback(const Ltp::session_id_t & sessionId, uint64_t reportSerialNumberBeingAcknowledged,
        Ltp::ltp_extensions_t & headerExtensions, Ltp::ltp_extensions_t & trailerExtensions);
    
    /** Handle report segment reception.
     *
     * If the session originator ID is our engine ID (same-engine transfer), returns immediately.
     * If the target session exists in the active transmission sessions, calls LtpSessionSender::ReportSegmentReceivedCallback().
     * Else, appends a report acknowledgment segment to the closed sessions data to send queue.
     * Calls LtpEngine::TrySaturateSendPacketPipeline() to resume processing.
     * @param sessionId The session ID.
     * @param reportSegment The report segment.
     * @param headerExtensions The LTP header extensions.
     * @param trailerExtensions The LTP trailer extensions.
     */
    LTP_LIB_NO_EXPORT void ReportSegmentReceivedCallback(const Ltp::session_id_t & sessionId, const Ltp::report_segment_t & reportSegment,
        Ltp::ltp_extensions_t & headerExtensions, Ltp::ltp_extensions_t & trailerExtensions);

    /** Handle data segment reception.
     *
     * If the session originator ID is our engine ID (same-engine transfer), returns immediately.
     * If the data segment destination client service ID is NOT our local client service ID, appends the session to the sessions with wrong client
     * service ID queue, then (only the first time) a cancellation segment is queued with a cancel code of UNREACHABLE, if the cancellation
     * callback is set, calls LtpEngine::m_receptionSessionCancelledCallback() with the same cancel code to invoke the cancellation callback,
     * finally calls LtpEngine::TrySaturateSendPacketPipeline() to dequeue the cancellation segment.
     * If a session with the segment session ID does NOT already exist, a new reception session is added to the active reception sessions
     * UNLESS the session recreation preventer feature is enabled and the session ID is currently in quarantine, if a new session is created AND
     * the session start callback is set, calls LtpEngine::m_sessionStartCallback() to invoke the session start callback.
     * Calls LtpSessionReceiver::DataSegmentReceivedCallback() eventually returning its return value if execution has reached this point.
     * Calls LtpEngine::TrySaturateSendPacketPipeline() to resume processing.
     * @param segmentTypeFlags The data segment type.
     * @param sessionId The session ID.
     * @param clientServiceDataVec The client service data.
     * @param dataSegmentMetadata The data segment metadata.
     * @param headerExtensions The LTP header extensions.
     * @param trailerExtensions The LTP trailer extensions.
     * @return True if the operation is still in progress on function exit (currently only on asynchronous disk writes), or False otherwise.
     * @post If returns False, indicates that the UDP circular index buffer can reduce its size.
     */
    LTP_LIB_NO_EXPORT bool DataSegmentReceivedCallback(uint8_t segmentTypeFlags, const Ltp::session_id_t & sessionId,
        Ltp::client_service_raw_data_t& clientServiceRawData, const Ltp::data_segment_metadata_t & dataSegmentMetadata,
        Ltp::ltp_extensions_t & headerExtensions, Ltp::ltp_extensions_t & trailerExtensions);

    /** Handle cancellation segment retransmission timer expiry.
     *
     * If the transmission retry count is within the retransmission limit per serial number, the cancellation segment is queued back for retransmission,
     * the retry count is incremented, then calls LtpEngine::TrySaturateSendPacketPipeline() to dequeue the cancellation segment.
     * Else, if this is (an expired) ping segment (see docs for LtpEngine::OnHousekeeping_TimerExpired), sets the next ping expiry, then if the
     * outduct link status event callback is set, calls LtpEngine::m_onOutductLinkStatusChangedCallback(true, uuid) to emit an outduct link down event.
     * @param cancelSegmentTimerSerialNumber The session ID.
     * @param userData The cancellation segment context data.
     * @post If this is an expired ping segment, M_NEXT_PING_START_EXPIRY is set to (now() + M_SENDER_PING_TIME).
     */
    LTP_LIB_NO_EXPORT void CancelSegmentTimerExpiredCallback(Ltp::session_id_t cancelSegmentTimerSerialNumber, std::vector<uint8_t> & userData);
    
    /** Handle sender should be queued for deletion event.
     *
     * If the session was cancelled, a cancellation segment is queued with a cancel code of reasonCode and if the cancellation callback is set AND
     * has NOT already been invoked for this session, calls LtpEngine::m_transmissionSessionCancelledCallback() with the same cancel code to invoke
     * the cancellation callback.
     * Else, if the session was closed, calls LtpEngine::m_transmissionSessionCompletedCallback() as the cancellation callback.
     *
     * Regardless if the session was cancelled or closed, queues the sender for deletion in the senders needing deleted queue,
     * then calls LtpEngine::SignalReadyForSend_ThreadSafe() on behalf of the sender.
     * @param sessionId The session ID.
     * @param wasCancelled Whether the session was cancelled (if False, session was closed).
     * @param reasonCode The cancel code.
     * @param userDataPtr The session attached client service data.
     */
    LTP_LIB_NO_EXPORT void NotifyEngineThatThisSenderNeedsDeletedCallback(const Ltp::session_id_t & sessionId, bool wasCancelled, CANCEL_SEGMENT_REASON_CODES reasonCode, std::shared_ptr<LtpTransmissionRequestUserData> & userDataPtr);
    
    /** Handle sender has data to send event.
     *
     * Appends the sender to the senders needing critical data sent queue, then calls LtpEngine::SignalReadyForSend_ThreadSafe() on behalf of the sender.
     * @param sessionNumber The session number.
     */
    LTP_LIB_NO_EXPORT void NotifyEngineThatThisSenderHasProducibleData(const uint64_t sessionNumber);
    
    /** Handle receiver should be queued for deletion event.
     *
     * If the session was cancelled, a cancellation segment is queued with a cancel code of reasonCode and if the cancellation callback is set
     * AND has NOT already been invoked for this session, calls LtpEngine::m_receptionSessionCancelledCallback() with the same cancel code
     * to invoke the cancellation callback.
     *
     * Regardless if the session was cancelled or closed, queues the receiver for deletion in the appropriate deletion queue,
     * then calls LtpEngine::SignalReadyForSend_ThreadSafe() on behalf of the receiver.
     * @param sessionId The session ID.
     * @param wasCancelled Whether the session was cancelled (if False, session was closed).
     * @param reasonCode The cancel code.
     */
    LTP_LIB_NO_EXPORT void NotifyEngineThatThisReceiverNeedsDeletedCallback(const Ltp::session_id_t & sessionId, bool wasCancelled, CANCEL_SEGMENT_REASON_CODES reasonCode);
    
    /** Handle receiver has data to send event.
     *
     * Appends the receiver to the receivers needing data sent queue, then calls LtpEngine::SignalReadyForSend_ThreadSafe() on behalf of the receiver.
     * @param sessionId The session ID.
     */
    LTP_LIB_NO_EXPORT void NotifyEngineThatThisReceiversTimersHasProducibleData(const Ltp::session_id_t & sessionId);
    
    /** Handle engine deferred disk operation completion.
     *
     * Calls LtpEngine::PacketInFullyProcessedCallback(true).
     */
    LTP_LIB_NO_EXPORT void NotifyEngineThatThisReceiverCompletedDeferredOperation();
    
    /** Handle initial data transmission (first pass) completion.
     *
     * If the initial data transmission callback is set, calls LtpEngine::m_initialTransmissionCompletedCallbackForUser() to invoke the initial
     * data transmission callback.
     * @param sessionId The session ID.
     * @param userDataPtr The session attached client service data.
     */
    LTP_LIB_NO_EXPORT void InitialTransmissionCompletedCallback(const Ltp::session_id_t & sessionId, std::shared_ptr<LtpTransmissionRequestUserData> & userDataPtr);

    /** Try restart the token refresh timer.
     *
     * If the token refresh timer is already running, returns immediately.
     * Else, starts the token refresh timer asynchronously with LtpEngine::OnTokenRefresh_TimerExpired() as a completion handler.
     */
    LTP_LIB_NO_EXPORT void TryRestartTokenRefreshTimer();
    
    /** Try restart the token refresh timer from the given time point.
     *
     * If the token refresh timer is already running, returns immediately.
     * Else, starts the token refresh timer asynchronously with LtpEngine::OnTokenRefresh_TimerExpired() as a completion handler.
     * @param nowPtime The time point to try restart from.
     */
    LTP_LIB_NO_EXPORT void TryRestartTokenRefreshTimer(const boost::posix_time::ptime & nowPtime);
    
    /** Handle token refresh timer expiry.
     *
     * Calls TokenRateLimiter::AddTime(now() - m_lastTimeTokensWereRefreshed) to tick the token rate limiter.
     * If the expiry occurred due to the timer being manually cancelled, returns immediately.
     * Else, if more tokens can be added, calls LtpEngine::TryRestartTokenRefreshTimer(now()) to asynchronously restart the token refresh timer and
     * let the next invocation add more tokens to the token rate limiter.
     * Calls LtpEngine::TrySaturateSendPacketPipeline() to resume processing.
     * @param e The error code.
     */
    LTP_LIB_NO_EXPORT void OnTokenRefresh_TimerExpired(const boost::system::error_code& e);

    /** Handle housekeeping timer expiry.
     *
     * If the expiry occurred due to the timer being manually cancelled, returns immediately.
     * Else, for each reception session, if enough time has passed that the session should now be considered stagnant AND has no active pending timers,
     * the receiver is queued for deletion in the appropriate deletion queue, then a cancellation segment is queued with a cancel code of USER_CANCELLED
     * and if the cancellation callback is set AND has NOT already been invoked for this session, calls LtpEngine::m_receptionSessionCancelledCallback() with
     * the same cancel code to invoke the cancellation callback.
     * Calls LtpEngine::TrySaturateSendPacketPipeline() to dequeue any queued cancellation segments.
     *
     * If pinging is enabled for this engine, if we are in a no data segment activity window AND we are overdue a ping, a ping segment is queued,
     * then calls LtpEngine::TrySaturateSendPacketPipeline() to dequeue the request.
     * (A ping segment is implemented as a cancellation segment of a known non-existent session number (typically supplied
     * by LtpRandomNumberGenerator::GetPingSession64() or the 32-bit variant), for which the receiver shall respond with a cancellation acknowledgment
     * in order to determine if the link is active, a link down event callback will be called if no acknowledgment is received
     * within a time window of (RTT * maxRetriesPerSerialNumber)).
     *
     * Starts the housekeeping timer asynchronously with itself as a completion handler to achieve to achieve a housekeeping interval.
     * @param e The error code.
     * @post If a ping segment is queued, M_NEXT_PING_START_EXPIRY is set to (boost::posix_time::special_values::pos_infin).
     */
    LTP_LIB_NO_EXPORT void OnHousekeeping_TimerExpired(const boost::system::error_code& e);

    /** Emit an outduct link down event.
     *
     * If the outduct link status event callback is set, calls LtpEngine::m_onOutductLinkStatusChangedCallback(true, uuid) to emit
     * an outduct link down event.
     */
    LTP_LIB_NO_EXPORT void DoExternalLinkDownEvent();
    
    //these four functions eliminate need for each session to have its own expensive boost::function/boost::bind using a reinterpret_cast of classPtr
    /** Handle report retransmission timer expiry.
     *
     * Calls LtpSessionReceiver::LtpReportSegmentTimerExpiredCallback().
     * @param classPtr Type-erased receiver pointer.
     * @param reportSerialNumberPlusSessionNumber The report serial number.
     * @param userData The report retransmission timer context data.
     */
    LTP_LIB_NO_EXPORT void LtpSessionReceiverReportSegmentTimerExpiredCallback(void* classPtr, const Ltp::session_id_t& reportSerialNumberPlusSessionNumber, std::vector<uint8_t>& userData);
    
    //these four functions eliminate need for each session to have its own expensive boost::function/boost::bind using a reinterpret_cast of classPtr
    /** Handle pending checkpoint delayed report transmission timer expiry.
     *
     * Calls LtpSessionReceiver::LtpDelaySendReportSegmentTimerExpiredCallback().
     * @param classPtr Type-erased receiver pointer.
     * @param checkpointSerialNumberPlusSessionNumber The associated checkpoint serial number.
     * @param userData The pending checkpoint delayed report transmission timer context data.
     */
    LTP_LIB_NO_EXPORT void LtpSessionReceiverDelaySendReportSegmentTimerExpiredCallback(void* classPtr, const Ltp::session_id_t& checkpointSerialNumberPlusSessionNumber, std::vector<uint8_t>& userData);
    
    //these four functions eliminate need for each session to have its own expensive boost::function/boost::bind using a reinterpret_cast of classPtr
    /** Handle checkpoint retransmission timer expiry.
     *
     * Calls LtpSessionSender::LtpCheckpointTimerExpiredCallback().
     * @param classPtr Type-erased sender pointer.
     * @param checkpointSerialNumberPlusSessionNumber The checkpoint session ID.
     * @param userData The checkpoint retransmission timer context data.
     */
    LTP_LIB_NO_EXPORT void LtpSessionSenderCheckpointTimerExpiredCallback(void* classPtr, const Ltp::session_id_t& checkpointSerialNumberPlusSessionNumber, std::vector<uint8_t>& userData);
    
    //these four functions eliminate need for each session to have its own expensive boost::function/boost::bind using a reinterpret_cast of classPtr
    /** Handle data segment retransmission timer expiry.
     *
     * Calls LtpSessionSender::LtpDelaySendDataSegmentsTimerExpiredCallback().
     * @param classPtr Type-erased sender pointer.
     * @param sessionNumber The session number.
     * @param userData The data segment retransmission timer context data.
     */
    LTP_LIB_NO_EXPORT void LtpSessionSenderDelaySendDataSegmentsTimerExpiredCallback(void* classPtr, const uint64_t& sessionNumber, std::vector<uint8_t>& userData);
private:
    /// Rx state machine
    Ltp m_ltpRxStateMachine;
    /// Random number generator
    LtpRandomNumberGenerator m_rng;
    /// Our engine ID
    const uint64_t M_THIS_ENGINE_ID;
    /// Number of pending SendPackets operations
    std::atomic<unsigned int> m_numQueuedSendSystemCallsAtomic;
protected:
    /// Maximum number of UDP packets to send per system call, if (M_MAX_UDP_PACKETS_TO_SEND_PER_SYSTEM_CALL > 1) enables batch sending
    const uint64_t M_MAX_UDP_PACKETS_TO_SEND_PER_SYSTEM_CALL;
private:
    /// RTT duration, ((oneWayLightTime * 2) + (oneWayMarginTime * 2)) where margin time is the estimated processing time overhead by the LTP engine
    boost::posix_time::time_duration m_transmissionToAckReceivedTime;
    /// Delayed report transmission duration, the duration to wait before transmitting reports after reception of data segments filling pending gaps,
    /// if this feature is disabled, the value is set to (boost::posix_time::special_values::not_a_date_time)
    /// (see docs for LtpEngineConfig::delaySendingOfReportSegmentsTimeMsOrZeroToDisable)
    boost::posix_time::time_duration m_delaySendingOfReportSegmentsTime;
    /// Delayed segment retransmission duration, the duration to wait before retransmitting data segments after reception of a report,
    /// if this feature is disabled the value is set to (boost::posix_time::special_values::not_a_date_time)
    /// (see docs for LtpEngineConfig::delaySendingOfDataSegmentsTimeMsOrZeroToDisable).
    boost::posix_time::time_duration m_delaySendingOfDataSegmentsTime;
    /// Housekeeping timer interval
    const boost::posix_time::time_duration M_HOUSEKEEPING_INTERVAL;
    /// Now time (updated periodically from housekeeping) so timestamp need not make system calls to get the time
    boost::posix_time::ptime m_nowTimeRef;
    /// Stagnated reception session duration, when (last received segment timestamp <= (now() - m_stagnantRxSessionTime) AND no active pending timers)
    /// indicates the reception session has stagnated and should be queued for deletion.
    boost::posix_time::time_duration m_stagnantRxSessionTime;
    /// Whether the engine is generating 32-bit random numbers (see docs for LtpEngineConfig::force32BitRandomNumbers for standard compliance and bandwidth details)
    const bool M_FORCE_32_BIT_RANDOM_NUMBERS;
    /// Default config file value for the number of seconds between ltp session sender pings during times of zero data segment activity,
    /// used for restoring the M_SENDER_PING_SECONDS_OR_ZERO_TO_DISABLE when re-enabling ping from a zero/disabled state
    const uint64_t M_DEFAULT_SENDER_PING_SECONDS_OR_ZERO_TO_DISABLE;
    /// Number of seconds between ltp session sender pings during times of zero data segment activity, if 0 this feature is disabled
    uint64_t m_senderPingSecondsOrZeroToDisable;
    /// Ping interval duration, will be zero-length if pinging is disabled
    boost::posix_time::time_duration m_senderPingTimeDuration;
    /// Next ping time point, if pings are enabled and (now() >= m_nextPingStartExpiry) indicates the next ping should be sent
    boost::posix_time::ptime m_nextPingStartExpiry;
    /// Whether the previous transmission request should count as a ping (toggle), when True the next queued ping should be skipped and the toggle disabled
    bool m_transmissionRequestServedAsPing;
    /// Maximum number of concurrent active sessions
    const uint64_t M_MAX_SIMULTANEOUS_SESSIONS;
    /// Maximum number of sessions in pipeline, less than (for disk) or equal to (for memory) M_MAX_SIMULTANEOUS_SESSIONS
    const uint64_t M_MAX_SESSIONS_IN_PIPELINE;
    /// Maximum number of queued disk operation completion callbacks, set to (M_MAX_SIMULTANEOUS_SESSIONS - M_MAX_SESSIONS_IN_PIPELINE)
    const uint64_t M_DISK_BUNDLE_ACK_CALLBACK_LIMIT;
    /// Session recreation preventer maximum number of session numbers to remember, if 0 this feature is disabled
    /// (see docs for LtpEngineConfig::rxDataSegmentSessionNumberRecreationPreventerHistorySizeOrZeroToDisable).
    const uint64_t M_MAX_RX_DATA_SEGMENT_HISTORY_OR_ZERO_DISABLE;

    //session receiver functions to be passed in AS REFERENCES (note declared before m_mapSessionIdToSessionReceiver so destroyed after map)
    /// This receiver should be queued for deletion notice function
    NotifyEngineThatThisReceiverNeedsDeletedCallback_t m_notifyEngineThatThisReceiverNeedsDeletedCallback;
    /// This receiver has data to send notice function
    NotifyEngineThatThisReceiversTimersHasProducibleDataFunction_t m_notifyEngineThatThisReceiversTimersHasProducibleDataFunction;
    /// This receiver has completed a deferred disk operation notice function
    NotifyEngineThatThisReceiverCompletedDeferredOperationFunction_t m_notifyEngineThatThisReceiverCompletedDeferredOperationFunction;

    //session sender functions to be passed in AS REFERENCES (note declared before m_mapSessionNumberToSessionSender so destroyed after map)
    /// This sender should be queued for deletion notice function
    NotifyEngineThatThisSenderNeedsDeletedCallback_t m_notifyEngineThatThisSenderNeedsDeletedCallback;
    /// This sender has data to send notice function
    NotifyEngineThatThisSenderHasProducibleDataFunction_t m_notifyEngineThatThisSenderHasProducibleDataFunction;
    /// This sender has completed initial data transmission (first pass) notice function, which then calls LtpEngine::m_initialTransmissionCompletedCallbackForUser
    InitialTransmissionCompletedCallback_t m_initialTransmissionCompletedCallbackCalledBySender;

    /// Type of map holding transmission sessions, mapped by session number
    typedef std::unordered_map<uint64_t, LtpSessionSender,
        std::hash<uint64_t>,
        std::equal_to<uint64_t>,
        FreeListAllocatorDynamic<std::pair<const uint64_t, LtpSessionSender> > > map_session_number_to_session_sender_t;
    /// Type of map holding reception sessions, mapped by session ID, hashed by session ID
    typedef std::unordered_map<Ltp::session_id_t, LtpSessionReceiver,
        Ltp::hash_session_id_t,
        std::equal_to<Ltp::session_id_t>,
        FreeListAllocatorDynamic<std::pair<const Ltp::session_id_t, LtpSessionReceiver> > > map_session_id_to_session_receiver_t;
    /// Active transmission sessions, mapped by session number
    map_session_number_to_session_sender_t m_mapSessionNumberToSessionSender;
    /// Active reception sessions, mapped by session ID, hashed by session ID
    map_session_id_to_session_receiver_t m_mapSessionIdToSessionReceiver;
    /** Give Tx Session Data back to user (e.g. for storage / retry later) if
     * m_onFailedBundleVecSendCallback and/or m_onFailedBundleZmqSendCallback are set.
     * @param txSessionIt The transmission session iterator.
     */
    LTP_LIB_NO_EXPORT void TryReturnTxSessionDataToUser(map_session_number_to_session_sender_t::iterator& txSessionIt);
    /** Remove given transmission session.
     *
     * Removes transmission session from the active transmission sessions, then if using the disk for intermediate storage
     * queues its memory block for deletion.
     * @param txSessionIt The transmission session iterator.
     */
    LTP_LIB_NO_EXPORT void EraseTxSession(map_session_number_to_session_sender_t::iterator& txSessionIt);
    /** Remove given reception session.
     *
     * Removes reception session from the active reception sessions.
     * @param rxSessionIt The reception session iterator.
     */
    LTP_LIB_NO_EXPORT void EraseRxSession(map_session_id_to_session_receiver_t::iterator& rxSessionIt);

    //reserve data so that operator new doesn't need called for resize of std::vector<boost::asio::const_buffer>
    // non-batch sender reserved
    std::vector<UdpSendPacketInfo> m_reservedUdpSendPacketInfo;
    std::vector<UdpSendPacketInfo>::iterator m_reservedUdpSendPacketInfoIterator;
    // batch sender reserved
    std::vector<std::shared_ptr<std::vector<UdpSendPacketInfo> > > m_reservedUdpSendPacketInfoVecsForBatchSender;
    std::vector<std::shared_ptr<std::vector<UdpSendPacketInfo> > >::iterator m_reservedUdpSendPacketInfoVecsForBatchSenderIterator;
    std::vector<MemoryInFiles::deferred_read_t> m_reservedDeferredReadsVec; //only used immediately and passed as const ref
    
    /// Sessions with wrong client service ID, all sessions cancelled with a cancel code of UNREACHABLE, on CAx should remove the associated session
    std::set<Ltp::session_id_t> m_ltpSessionsWithWrongClientServiceId;
    /// Closed sessions data to send queue, (session originator engine ID TO data to send), typically sending acknowledgment segments
    std::queue<std::pair<uint64_t, std::vector<uint8_t> > > m_queueClosedSessionDataToSend; //sessionOriginatorEngineId, data
    /// Cancellation segment context data queue, feeds timers managed by m_timeManagerOfCancelSegments
    std::queue<cancel_segment_timer_info_t> m_queueCancelSegmentTimerInfo;
    /// Senders needing deleted queue
    std::queue<uint64_t> m_queueSendersNeedingDeleted;
    /// Senders needing critical data sent queue
    std::queue<uint64_t> m_queueSendersNeedingTimeCriticalDataSent;
    /// Senders needing first-pass data sent queue
    std::queue<uint64_t> m_queueSendersNeedingFirstPassDataSent;
    /// Receivers needing deleted queue
    std::queue<Ltp::session_id_t> m_queueReceiversNeedingDeleted;
    /// Receivers with pending operations needing deleted queue
    std::queue<Ltp::session_id_t> m_queueReceiversNeedingDeletedButUnsafeToDelete;
    /// Receivers needing data sent queue
    std::queue<Ltp::session_id_t> m_queueReceiversNeedingDataSent;

    /// Session start callback
    SessionStartCallback_t m_sessionStartCallback;
    /// Red data part reception callback
    RedPartReceptionCallback_t m_redPartReceptionCallback;
    /// Green data segment reception callback
    GreenPartSegmentArrivalCallback_t m_greenPartSegmentArrivalCallback;
    /// Reception session cancellation callback
    ReceptionSessionCancelledCallback_t m_receptionSessionCancelledCallback;
    /// Transmission session completion callback
    TransmissionSessionCompletedCallback_t m_transmissionSessionCompletedCallback;
    /// Initial data transmission (first pass) completion callback
    InitialTransmissionCompletedCallback_t m_initialTransmissionCompletedCallbackForUser;
    /// Transmission session cancellation callback
    TransmissionSessionCancelledCallback_t m_transmissionSessionCancelledCallback;

    /// Failed byte buffer session data disk write completion callback, when this is a Byte Buffer session (see client service data states in LtpClientServiceDataToSend),
    /// when (m_onFailedBundleZmqSendCallback != NULL) indicates the user wants to retrieve the client service data to handle failure.
    OnFailedBundleVecSendCallback_t m_onFailedBundleVecSendCallback;
    /// Failed ZMQ session data disk write completion callback, when this is a ZMQ session (see client service data states in LtpClientServiceDataToSend),
    /// when (m_onFailedBundleZmqSendCallback != NULL) indicates the user wants to retrieve the client service data to handle failure.
    OnFailedBundleZmqSendCallback_t m_onFailedBundleZmqSendCallback;
    /// Successful session data disk write completion callback, invoked when the session was fully written to disk and NOT when the red part reception is completed
    /// (see comment in LtpEngine::OnTransmissionRequestDataWrittenToDisk for more details).
    OnSuccessfulBundleSendCallback_t m_onSuccessfulBundleSendCallback;
    /// Outduct link status event callback
    OnOutductLinkStatusChangedCallback_t m_onOutductLinkStatusChangedCallback;
    /// Outduct UUID
    uint64_t m_userAssignedUuid;

    /// Maximum number of retries/resends of a single LTP packet with a serial number before the session is terminated
    uint32_t m_maxRetriesPerSerialNumber;
protected:
    /// I/O execution context
    boost::asio::io_service m_ioServiceLtpEngine; //for timers and post calls only
private:
    /// Explicit work controller for m_ioServiceLtpEngine, allows for graceful shutdown of running tasks
    std::unique_ptr<boost::asio::io_service::work> m_workLtpEnginePtr;

    /// Report retransmission timer, managed by m_timeManagerOfReportSerialNumbers
    boost::asio::deadline_timer m_deadlineTimerForTimeManagerOfReportSerialNumbers;
    // within a session would normally be LtpTimerManager<uint64_t, std::hash<uint64_t> > m_timeManagerOfReportSerialNumbers;
    // but now sharing a single LtpTimerManager among all sessions, so use a
    // LtpTimerManager<Ltp::session_id_t, Ltp::hash_session_id_t> (which has hash map hashing function support)
    // such that: 
    //  sessionOriginatorEngineId = REPORT serial number
    //  sessionNumber = the session number
    //  since this is a receiver, the real sessionOriginatorEngineId is constant among all receiving sessions and is not needed
    /// Report retransmission timer manager, timer mapped by session ID, hashed by session ID
    LtpTimerManager<Ltp::session_id_t, Ltp::hash_session_id_t> m_timeManagerOfReportSerialNumbers;
    /// Report retransmission timer expiry callback
    LtpTimerManager<Ltp::session_id_t, Ltp::hash_session_id_t>::LtpTimerExpiredCallback_t m_rsnTimerExpiredCallback;

    /// Pending checkpoint delayed report transmission timer, managed by m_timeManagerOfSendingDelayedReceptionReports
    boost::asio::deadline_timer m_deadlineTimerForTimeManagerOfSendingDelayedReceptionReports;
    //  sessionOriginatorEngineId = CHECKPOINT serial number to which RS pertains
    //  sessionNumber = the session number
    //  since this is a receiver, the real sessionOriginatorEngineId is constant among all receiving sessions and is not needed
    /// Pending checkpoint delayed report transmission timer manager, timer mapped by session ID, hashed by session ID
    LtpTimerManager<Ltp::session_id_t, Ltp::hash_session_id_t> m_timeManagerOfSendingDelayedReceptionReports;
    /// Pending checkpoint delayed report transmission timer expiry callback
    LtpTimerManager<Ltp::session_id_t, Ltp::hash_session_id_t>::LtpTimerExpiredCallback_t m_delayedReceptionReportTimerExpiredCallback;

    /// Checkpoint retransmission timer, managed by m_timeManagerOfCheckpointSerialNumbers
    boost::asio::deadline_timer m_deadlineTimerForTimeManagerOfCheckpointSerialNumbers;
    // within a session would normally be LtpTimerManager<uint64_t, std::hash<uint64_t> > m_timeManagerOfCheckpointSerialNumbers;
    // but now sharing a single LtpTimerManager among all sessions, so use a
    // LtpTimerManager<Ltp::session_id_t, Ltp::hash_session_id_t> (which has hash map hashing function support)
    // such that: 
    //  sessionOriginatorEngineId = CHECKPOINT serial number
    //  sessionNumber = the session number
    //  since this is a sender, the real sessionOriginatorEngineId is constant among all sending sessions and is not needed
    /// Checkpoint retransmission timer manager, timer mapped by session ID, hashed by session ID
    LtpTimerManager<Ltp::session_id_t, Ltp::hash_session_id_t> m_timeManagerOfCheckpointSerialNumbers;
    /// Checkpoint retransmission timer expiry callback
    LtpTimerManager<Ltp::session_id_t, Ltp::hash_session_id_t>::LtpTimerExpiredCallback_t m_csnTimerExpiredCallback;

    /// Data segment retransmission timer, managed by m_timeManagerOfSendingDelayedDataSegments
    boost::asio::deadline_timer m_deadlineTimerForTimeManagerOfSendingDelayedDataSegments;
    // within a session would normally be a single deadline timer;
    // but now sharing a single LtpTimerManager among all sessions, so use a
    // LtpTimerManager<uint64_t, std::hash<uint64_t> >
    // such that: 
    //  uint64_t = the session number
    //  since this is a sender, the real sessionOriginatorEngineId is constant among all sending sessions and is not needed
    /// Data segment retransmission timer manager, timer mapped by session number, hashed by session number
    LtpTimerManager<uint64_t, std::hash<uint64_t> > m_timeManagerOfSendingDelayedDataSegments;
    /// Data segment retransmission timer expiry callback
    LtpTimerManager<uint64_t, std::hash<uint64_t> >::LtpTimerExpiredCallback_t m_delayedDataSegmentsTimerExpiredCallback;

    /// Cancellation segment retransmission timer expiry callback
    LtpTimerManager<Ltp::session_id_t, Ltp::hash_session_id_t>::LtpTimerExpiredCallback_t m_cancelSegmentTimerExpiredCallback;
    /// Cancellation segment retransmission timer, managed by m_timeManagerOfCancelSegments
    boost::asio::deadline_timer m_deadlineTimerForTimeManagerOfCancelSegments;
    /// Cancellation segment retransmission timer manager
    LtpTimerManager<Ltp::session_id_t, Ltp::hash_session_id_t> m_timeManagerOfCancelSegments;
    
    /// Housekeeping timer, runs on an interval for executing periodic checks on the current state of the engine
    boost::asio::deadline_timer m_housekeepingTimer;
    /// Token rate limiter
    TokenRateLimiter m_tokenRateLimiter;
    /// Token refresh timer
    boost::asio::deadline_timer m_tokenRefreshTimer;
    /// Rate limiting UDP send rate in bits per second, if set to 0 the engine will send UDP packets as fast as the operating system will allow
    uint64_t m_maxSendRateBitsPerSecOrZeroToDisable;
    /// Whether the token refresh timer is currently active
    bool m_tokenRefreshTimerIsRunning;
    /// Time point, used by the token refresh timer to calculate delta time
    boost::posix_time::ptime m_lastTimeTokensWereRefreshed;
    /// The window of time for averaging the UDP send rate over
    boost::posix_time::time_duration m_rateLimitPrecisionInterval;
    /// The interval to refresh tokens for the rate limiter
    boost::posix_time::time_duration m_tokenRefreshInterval;
    /// Thread that invokes m_ioServiceLtpEngine.run() (if using dedicated I/O thread)
    std::unique_ptr<boost::thread> m_ioServiceLtpEngineThreadPtr;

    //session re-creation prevention
    /// Session recreation preventers, mapped by session originator engine ID
    std::map<uint64_t, LtpSessionRecreationPreventer> m_mapSessionOriginatorEngineIdToLtpSessionRecreationPreventer;

    //memory in files
    /// Disk memory manager
    std::unique_ptr<MemoryInFiles> m_memoryInFilesPtr;
    /// Memory block IDs pending deletion queue, manages lifetime of transmission session memory blocks
    std::queue<uint64_t> m_memoryBlockIdsPendingDeletionQueue;
    /// Pending successful bundle send callback context data queue, feeds invocations of m_onSuccessfulBundleSendCallback
    std::queue<std::vector<uint8_t> > m_userDataPendingSuccessfulBundleSendCallbackQueue;


    //reference structs common to all sessions
    /// Session sender common data
    LtpSessionSender::LtpSessionSenderRecycler m_ltpSessionSenderRecycler;
    LtpSessionSender::LtpSessionSenderCommonData m_ltpSessionSenderCommonData;
    /// Session receiver common data
    LtpSessionReceiver::LtpSessionReceiverRecycler m_ltpSessionReceiverRecycler;
    LtpSessionReceiver::LtpSessionReceiverCommonData m_ltpSessionReceiverCommonData;

public:
    //stats

    //LtpEngine
    /// ...
    std::atomic<uint64_t> m_countAsyncSendsLimitedByRate;
    /// Number of packets pending further processing
    std::atomic<uint64_t> m_countPacketsWithOngoingOperations;
    /// Total number of packets fully processed
    std::atomic<uint64_t> m_countPacketsThatCompletedOngoingOperations;
    /// Total number of times a transmission request was written to disk prior to beginning transmission
    std::atomic<uint64_t> m_numEventsTransmissionRequestDiskWritesTooSlow;
    /// Total red data bytes successfully sent (needed here since TransmissionSessionCompletedCallback_t doesn't provide a byte count)
    std::atomic<uint64_t> m_totalRedDataBytesSuccessfullySent;
    /// Total red data bytes failed to send
    std::atomic<uint64_t> m_totalRedDataBytesFailedToSend;
    /// Total cancel segments that were initiated
    std::atomic<uint64_t> m_totalCancelSegmentsStarted;
    /// Total cancel segments retry operations because a timer expired
    std::atomic<uint64_t> m_totalCancelSegmentSendRetries;
    /// Total cancel segments that failed to send because retry limit exceeded (also serves as bool for printing one notice to logger)
    std::atomic<uint64_t> m_totalCancelSegmentsFailedToSend;
    /// Total cancel segments that were acknowledged by the remote
    std::atomic<uint64_t> m_totalCancelSegmentsAcknowledged;
    /// Total pings (which are cancel segments) that were initiated
    std::atomic<uint64_t> m_totalPingsStarted;
    /// Total pings (which are cancel segments) retry operations because a timer expired
    std::atomic<uint64_t> m_totalPingRetries;
    /// Total pings (which are cancel segments) that failed to send because retry limit exceeded
    std::atomic<uint64_t> m_totalPingsFailedToSend;
    /// Total pings (which are cancel segments) that were acknowledged by the remote
    std::atomic<uint64_t> m_totalPingsAcknowledged;
    /// Total Tx sessions which had their data returned to the user 
    std::atomic<uint64_t> m_numTxSessionsReturnedToStorage;
    /// Total Tx sessions cancelled by the receiver
    std::atomic<uint64_t> m_numTxSessionsCancelledByReceiver;
    /// Total Rx sessions cancelled by the sender
    std::atomic<uint64_t> m_numRxSessionsCancelledBySender;
    /// Total Stagnant Rx sessions deleted by the housekeeping
    std::atomic<uint64_t> m_numStagnantRxSessionsDeleted;


    //session sender stats (references to variables within m_ltpSessionSenderCommonData)
    /// Total number of checkpoint retransmission timer expiry callback invocations
    std::atomic<uint64_t>& m_numCheckpointTimerExpiredCallbacksRef;
    /// Total number of discretionary checkpoints reported received
    std::atomic<uint64_t>& m_numDiscretionaryCheckpointsNotResentRef;
    /// Total number of reports deleted after claiming reception of their entire scope
    std::atomic<uint64_t>& m_numDeletedFullyClaimedPendingReportsRef;

    //session receiver stats
    /// Total number of report segment timer expiry callback invocations
    std::atomic<uint64_t>& m_numReportSegmentTimerExpiredCallbacksRef;
    /// Total number of report segments unable to be issued
    std::atomic<uint64_t>& m_numReportSegmentsUnableToBeIssuedRef;
    /// Total number of reports too large needing-fragmented (when report claims > m_maxReceptionClaims)
    std::atomic<uint64_t>& m_numReportSegmentsTooLargeAndNeedingSplitRef;
    /// Total number of report segments produced from too large needing-fragmented reports
    std::atomic<uint64_t>& m_numReportSegmentsCreatedViaSplitRef;
    /// Total number of gaps filled by out-of-order data segments
    std::atomic<uint64_t>& m_numGapsFilledByOutOfOrderDataSegmentsRef;
    /// Total number of whole primary report segments sent (only reports when no gaps)
    std::atomic<uint64_t>& m_numDelayedFullyClaimedPrimaryReportSegmentsSentRef;
    /// Total number of whole secondary report segments sent (only reports when no gaps)
    std::atomic<uint64_t>& m_numDelayedFullyClaimedSecondaryReportSegmentsSentRef;
    /// Total number of out-of-order partial primary report segments
    std::atomic<uint64_t>& m_numDelayedPartiallyClaimedPrimaryReportSegmentsSentRef;
    /// Total number of out-of-order partial secondary report segments
    std::atomic<uint64_t>& m_numDelayedPartiallyClaimedSecondaryReportSegmentsSentRef;
};

#endif // LTP_ENGINE_H

