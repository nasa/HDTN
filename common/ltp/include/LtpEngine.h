/**
 * @file LtpEngine.h
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
 * This LtpEngine class manages all the active LTP sending or receiving sessions.
 */

#ifndef LTP_ENGINE_H
#define LTP_ENGINE_H 1

#include <boost/random/random_device.hpp>
#include <boost/thread.hpp>
#include "LtpFragmentSet.h"
#include "Ltp.h"
#include "LtpRandomNumberGenerator.h"
#include "LtpSessionReceiver.h"
#include "LtpSessionSender.h"
#include "LtpNoticesToClientService.h"
#include "LtpClientServiceDataToSend.h"
#include "LtpSessionRecreationPreventer.h"
#include "TokenRateLimiter.h"
#include "BundleCallbackFunctionDefines.h"
#include <unordered_map>
#include <queue>
#include <memory>
#include <boost/core/noncopyable.hpp>

class CLASS_VISIBILITY_LTP_LIB LtpEngine : private boost::noncopyable {
private:
    LtpEngine() = delete;
public:
    struct transmission_request_t {
        uint64_t destinationClientServiceId;
        uint64_t destinationLtpEngineId;
        LtpClientServiceDataToSend clientServiceDataToSend;
        uint64_t lengthOfRedPart;
        std::shared_ptr<LtpTransmissionRequestUserData> userDataPtr;
    };
    struct cancel_segment_timer_info_t {
        cancel_segment_timer_info_t() = default;
        cancel_segment_timer_info_t(const uint8_t * data); //for queue emplace of user data

        Ltp::session_id_t sessionId;
        CANCEL_SEGMENT_REASON_CODES reasonCode;
        bool isFromSender;
        uint8_t retryCount;
    };
    
    LTP_LIB_EXPORT LtpEngine(const uint64_t thisEngineId, const uint8_t engineIndexForEncodingIntoRandomSessionNumber, const uint64_t mtuClientServiceData, uint64_t mtuReportSegment,
        const boost::posix_time::time_duration & oneWayLightTime, const boost::posix_time::time_duration & oneWayMarginTime,
        const uint64_t ESTIMATED_BYTES_TO_RECEIVE_PER_SESSION, const uint64_t maxRedRxBytesPerSession, bool startIoServiceThread,
        uint32_t checkpointEveryNthDataPacketSender, uint32_t maxRetriesPerSerialNumber, const bool force32BitRandomNumbers, const uint64_t maxSendRateBitsPerSecOrZeroToDisable,
        const uint64_t maxSimultaneousSessions, const uint64_t rxDataSegmentSessionNumberRecreationPreventerHistorySizeOrZeroToDisable,
        const uint64_t maxUdpPacketsToSendPerSystemCall, const uint64_t senderPingSecondsOrZeroToDisable, const uint64_t delaySendingOfReportSegmentsTimeMsOrZeroToDisable,
        const uint64_t delaySendingOfDataSegmentsTimeMsOrZeroToDisable);

    LTP_LIB_EXPORT virtual ~LtpEngine();

    LTP_LIB_EXPORT virtual void Reset();
    LTP_LIB_EXPORT void SetCheckpointEveryNthDataPacketForSenders(uint64_t checkpointEveryNthDataPacketSender);
    LTP_LIB_EXPORT void SetMtuReportSegment(uint64_t mtuReportSegment);

    LTP_LIB_EXPORT void TransmissionRequest(std::shared_ptr<transmission_request_t> & transmissionRequest);
    LTP_LIB_EXPORT void TransmissionRequest_ThreadSafe(std::shared_ptr<transmission_request_t> && transmissionRequest);
    LTP_LIB_EXPORT void TransmissionRequest(uint64_t destinationClientServiceId, uint64_t destinationLtpEngineId,
        LtpClientServiceDataToSend && clientServiceDataToSend, std::shared_ptr<LtpTransmissionRequestUserData> && userDataPtrToTake, uint64_t lengthOfRedPart);
    LTP_LIB_EXPORT void TransmissionRequest(uint64_t destinationClientServiceId, uint64_t destinationLtpEngineId,
        const uint8_t * clientServiceDataToCopyAndSend, uint64_t length, std::shared_ptr<LtpTransmissionRequestUserData> && userDataPtrToTake, uint64_t lengthOfRedPart);
    LTP_LIB_EXPORT void TransmissionRequest(uint64_t destinationClientServiceId, uint64_t destinationLtpEngineId,
        const uint8_t * clientServiceDataToCopyAndSend, uint64_t length, uint64_t lengthOfRedPart);

    LTP_LIB_EXPORT bool CancellationRequest(const Ltp::session_id_t & sessionId);
    LTP_LIB_EXPORT void CancellationRequest_ThreadSafe(const Ltp::session_id_t & sessionId);
    
    LTP_LIB_EXPORT void SetSessionStartCallback(const SessionStartCallback_t & callback);
    LTP_LIB_EXPORT void SetRedPartReceptionCallback(const RedPartReceptionCallback_t & callback);
    LTP_LIB_EXPORT void SetGreenPartSegmentArrivalCallback(const GreenPartSegmentArrivalCallback_t & callback);
    LTP_LIB_EXPORT void SetReceptionSessionCancelledCallback(const ReceptionSessionCancelledCallback_t & callback);
    LTP_LIB_EXPORT void SetTransmissionSessionCompletedCallback(const TransmissionSessionCompletedCallback_t & callback);
    LTP_LIB_EXPORT void SetInitialTransmissionCompletedCallback(const InitialTransmissionCompletedCallback_t & callback);
    LTP_LIB_EXPORT void SetTransmissionSessionCancelledCallback(const TransmissionSessionCancelledCallback_t & callback);

    LTP_LIB_EXPORT void SetOnFailedBundleVecSendCallback(const OnFailedBundleVecSendCallback_t& callback);
    LTP_LIB_EXPORT void SetOnFailedBundleZmqSendCallback(const OnFailedBundleZmqSendCallback_t& callback);
    LTP_LIB_EXPORT void SetOnSuccessfulBundleSendCallback(const OnSuccessfulBundleSendCallback_t& callback);
    LTP_LIB_EXPORT void SetOnOutductLinkStatusChangedCallback(const OnOutductLinkStatusChangedCallback_t& callback);
    LTP_LIB_EXPORT void SetUserAssignedUuid(uint64_t userAssignedUuid);

    LTP_LIB_EXPORT bool PacketIn(const uint8_t * data, const std::size_t size, Ltp::SessionOriginatorEngineIdDecodedCallback_t * sessionOriginatorEngineIdDecodedCallbackPtr = NULL);
    LTP_LIB_EXPORT bool PacketIn(const std::vector<boost::asio::const_buffer> & constBufferVec); //for testing

    LTP_LIB_EXPORT void PacketIn_ThreadSafe(const uint8_t * data, const std::size_t size, Ltp::SessionOriginatorEngineIdDecodedCallback_t * sessionOriginatorEngineIdDecodedCallbackPtr = NULL);
    LTP_LIB_EXPORT void PacketIn_ThreadSafe(const std::vector<boost::asio::const_buffer> & constBufferVec); //for testing

    LTP_LIB_EXPORT void PostExternalLinkDownEvent_ThreadSafe();

    LTP_LIB_EXPORT bool GetNextPacketToSend(std::vector<boost::asio::const_buffer> & constBufferVec,
        std::shared_ptr<std::vector<std::vector<uint8_t> > > & underlyingDataToDeleteOnSentCallback,
        std::shared_ptr<LtpClientServiceDataToSend>& underlyingCsDataToDeleteOnSentCallback,
        uint64_t & sessionOriginatorEngineId);

    LTP_LIB_EXPORT std::size_t NumActiveReceivers() const;
    LTP_LIB_EXPORT std::size_t NumActiveSenders() const;

    LTP_LIB_EXPORT void UpdateRate(const uint64_t maxSendRateBitsPerSecOrZeroToDisable);
    LTP_LIB_EXPORT void UpdateRate_ThreadSafe(const uint64_t maxSendRateBitsPerSecOrZeroToDisable);
    
    LTP_LIB_EXPORT void SetDelays_ThreadSafe(const boost::posix_time::time_duration& oneWayLightTime, const boost::posix_time::time_duration& oneWayMarginTime, bool updateRunningTimers);
    LTP_LIB_EXPORT void SetDeferDelays_ThreadSafe(const uint64_t delaySendingOfReportSegmentsTimeMsOrZeroToDisable, const uint64_t delaySendingOfDataSegmentsTimeMsOrZeroToDisable);
protected:
    LTP_LIB_EXPORT void SetDelays(const boost::posix_time::time_duration& oneWayLightTime, const boost::posix_time::time_duration& oneWayMarginTime, bool updateRunningTimers);
    LTP_LIB_EXPORT void SetDeferDelays(const uint64_t delaySendingOfReportSegmentsTimeMsOrZeroToDisable, const uint64_t delaySendingOfDataSegmentsTimeMsOrZeroToDisable);
    LTP_LIB_EXPORT virtual void PacketInFullyProcessedCallback(bool success);
    LTP_LIB_EXPORT virtual void SendPacket(std::vector<boost::asio::const_buffer>& constBufferVec,
        std::shared_ptr<std::vector<std::vector<uint8_t> > >& underlyingDataToDeleteOnSentCallback,
        std::shared_ptr<LtpClientServiceDataToSend>& underlyingCsDataToDeleteOnSentCallback);
    LTP_LIB_EXPORT void SignalReadyForSend_ThreadSafe();
    LTP_LIB_EXPORT virtual void SendPackets(std::vector<std::vector<boost::asio::const_buffer> >& constBufferVecs,
        std::vector<std::shared_ptr<std::vector<std::vector<uint8_t> > > >& underlyingDataToDeleteOnSentCallback,
        std::vector<std::shared_ptr<LtpClientServiceDataToSend> >& underlyingCsDataToDeleteOnSentCallback);
private:
    LTP_LIB_NO_EXPORT void TrySendPacketIfAvailable();

    LTP_LIB_NO_EXPORT void CancelSegmentReceivedCallback(const Ltp::session_id_t & sessionId, CANCEL_SEGMENT_REASON_CODES reasonCode, bool isFromSender,
        Ltp::ltp_extensions_t & headerExtensions, Ltp::ltp_extensions_t & trailerExtensions);
    LTP_LIB_NO_EXPORT void CancelAcknowledgementSegmentReceivedCallback(const Ltp::session_id_t & sessionId, bool isToSender,
        Ltp::ltp_extensions_t & headerExtensions, Ltp::ltp_extensions_t & trailerExtensions);
    LTP_LIB_NO_EXPORT void ReportAcknowledgementSegmentReceivedCallback(const Ltp::session_id_t & sessionId, uint64_t reportSerialNumberBeingAcknowledged,
        Ltp::ltp_extensions_t & headerExtensions, Ltp::ltp_extensions_t & trailerExtensions);
    LTP_LIB_NO_EXPORT void ReportSegmentReceivedCallback(const Ltp::session_id_t & sessionId, const Ltp::report_segment_t & reportSegment,
        Ltp::ltp_extensions_t & headerExtensions, Ltp::ltp_extensions_t & trailerExtensions);
    LTP_LIB_NO_EXPORT void DataSegmentReceivedCallback(uint8_t segmentTypeFlags, const Ltp::session_id_t & sessionId,
        std::vector<uint8_t> & clientServiceDataVec, const Ltp::data_segment_metadata_t & dataSegmentMetadata,
        Ltp::ltp_extensions_t & headerExtensions, Ltp::ltp_extensions_t & trailerExtensions);

    LTP_LIB_NO_EXPORT void CancelSegmentTimerExpiredCallback(Ltp::session_id_t cancelSegmentTimerSerialNumber, std::vector<uint8_t> & userData);
    LTP_LIB_NO_EXPORT void NotifyEngineThatThisSenderNeedsDeletedCallback(const Ltp::session_id_t & sessionId, bool wasCancelled, CANCEL_SEGMENT_REASON_CODES reasonCode, std::shared_ptr<LtpTransmissionRequestUserData> & userDataPtr);
    LTP_LIB_NO_EXPORT void NotifyEngineThatThisSenderHasProducibleData(const uint64_t sessionNumber);
    LTP_LIB_NO_EXPORT void NotifyEngineThatThisReceiverNeedsDeletedCallback(const Ltp::session_id_t & sessionId, bool wasCancelled, CANCEL_SEGMENT_REASON_CODES reasonCode);
    LTP_LIB_NO_EXPORT void NotifyEngineThatThisReceiversTimersHasProducibleData(const Ltp::session_id_t & sessionId);
    LTP_LIB_NO_EXPORT void InitialTransmissionCompletedCallback(const Ltp::session_id_t & sessionId, std::shared_ptr<LtpTransmissionRequestUserData> & userDataPtr);

    LTP_LIB_NO_EXPORT void TryRestartTokenRefreshTimer();
    LTP_LIB_NO_EXPORT void TryRestartTokenRefreshTimer(const boost::posix_time::ptime & nowPtime);
    LTP_LIB_NO_EXPORT void OnTokenRefresh_TimerExpired(const boost::system::error_code& e);

    LTP_LIB_NO_EXPORT void OnHousekeeping_TimerExpired(const boost::system::error_code& e);

    LTP_LIB_NO_EXPORT void DoExternalLinkDownEvent();
private:
    Ltp m_ltpRxStateMachine;
    LtpRandomNumberGenerator m_rng;
    const uint64_t M_ESTIMATED_BYTES_TO_RECEIVE_PER_SESSION;
    const uint64_t M_MAX_RED_RX_BYTES_PER_SESSION;
    const uint64_t M_THIS_ENGINE_ID;
    const uint64_t M_MTU_CLIENT_SERVICE_DATA;
protected:
    const uint64_t M_MAX_UDP_PACKETS_TO_SEND_PER_SYSTEM_CALL;
private:
    const boost::posix_time::time_duration M_ONE_WAY_LIGHT_TIME;
    const boost::posix_time::time_duration M_ONE_WAY_MARGIN_TIME;
    boost::posix_time::time_duration m_transmissionToAckReceivedTime;
    boost::posix_time::time_duration m_delaySendingOfReportSegmentsTime;
    boost::posix_time::time_duration m_delaySendingOfDataSegmentsTime;
    const boost::posix_time::time_duration M_HOUSEKEEPING_INTERVAL;
    boost::posix_time::time_duration m_stagnantRxSessionTime;
    const bool M_FORCE_32_BIT_RANDOM_NUMBERS;
    const uint64_t M_SENDER_PING_SECONDS_OR_ZERO_TO_DISABLE;
    const boost::posix_time::time_duration M_SENDER_PING_TIME;
    boost::posix_time::ptime M_NEXT_PING_START_EXPIRY;
    bool m_transmissionRequestServedAsPing;
    const uint64_t M_MAX_SIMULTANEOUS_SESSIONS;
    const uint64_t M_MAX_RX_DATA_SEGMENT_HISTORY_OR_ZERO_DISABLE;//rxDataSegmentSessionNumberRecreationPreventerHistorySizeOrZeroToDisable
    boost::random_device m_randomDevice;

    typedef std::unordered_map<uint64_t, std::unique_ptr<LtpSessionSender> > map_session_number_to_session_sender_t;
    typedef std::unordered_map<Ltp::session_id_t, std::unique_ptr<LtpSessionReceiver>, Ltp::hash_session_id_t > map_session_id_to_session_receiver_t;
    map_session_number_to_session_sender_t m_mapSessionNumberToSessionSender;
    map_session_id_to_session_receiver_t m_mapSessionIdToSessionReceiver;

    std::queue<std::pair<uint64_t, std::vector<uint8_t> > > m_queueClosedSessionDataToSend; //sessionOriginatorEngineId, data
    std::queue<cancel_segment_timer_info_t> m_queueCancelSegmentTimerInfo;
    std::queue<uint64_t> m_queueSendersNeedingDeleted;
    std::queue<uint64_t> m_queueSendersNeedingTimeCriticalDataSent;
    std::queue<uint64_t> m_queueSendersNeedingFirstPassDataSent;
    std::queue<Ltp::session_id_t> m_queueReceiversNeedingDeleted;
    std::queue<Ltp::session_id_t> m_queueReceiversNeedingDataSent;

    SessionStartCallback_t m_sessionStartCallback;
    RedPartReceptionCallback_t m_redPartReceptionCallback;
    GreenPartSegmentArrivalCallback_t m_greenPartSegmentArrivalCallback;
    ReceptionSessionCancelledCallback_t m_receptionSessionCancelledCallback;
    TransmissionSessionCompletedCallback_t m_transmissionSessionCompletedCallback;
    InitialTransmissionCompletedCallback_t m_initialTransmissionCompletedCallback;
    TransmissionSessionCancelledCallback_t m_transmissionSessionCancelledCallback;

    OnFailedBundleVecSendCallback_t m_onFailedBundleVecSendCallback;
    OnFailedBundleZmqSendCallback_t m_onFailedBundleZmqSendCallback;
    OnSuccessfulBundleSendCallback_t m_onSuccessfulBundleSendCallback;
    OnOutductLinkStatusChangedCallback_t m_onOutductLinkStatusChangedCallback;
    uint64_t m_userAssignedUuid;

    uint64_t m_checkpointEveryNthDataPacketSender;
    uint64_t m_maxReceptionClaims;
    uint32_t m_maxRetriesPerSerialNumber;
protected:
    boost::asio::io_service m_ioServiceLtpEngine; //for timers and post calls only
private:
    std::unique_ptr<boost::asio::io_service::work> m_workLtpEnginePtr;

    boost::asio::deadline_timer m_deadlineTimerForTimeManagerOfReportSerialNumbers;
    // within a session would normally be LtpTimerManager<uint64_t, std::hash<uint64_t> > m_timeManagerOfReportSerialNumbers;
    // but now sharing a single LtpTimerManager among all sessions, so use a
    // LtpTimerManager<Ltp::session_id_t, Ltp::hash_session_id_t> (which has hash map hashing function support)
    // such that: 
    //  sessionOriginatorEngineId = REPORT serial number
    //  sessionNumber = the session number
    //  since this is a receiver, the real sessionOriginatorEngineId is constant among all receiving sessions and is not needed
    LtpTimerManager<Ltp::session_id_t, Ltp::hash_session_id_t> m_timeManagerOfReportSerialNumbers;

    boost::asio::deadline_timer m_deadlineTimerForTimeManagerOfSendingDelayedReceptionReports;
    //  sessionOriginatorEngineId = CHECKPOINT serial number to which RS pertains
    //  sessionNumber = the session number
    //  since this is a receiver, the real sessionOriginatorEngineId is constant among all receiving sessions and is not needed
    LtpTimerManager<Ltp::session_id_t, Ltp::hash_session_id_t> m_timeManagerOfSendingDelayedReceptionReports;

    boost::asio::deadline_timer m_deadlineTimerForTimeManagerOfCheckpointSerialNumbers;
    // within a session would normally be LtpTimerManager<uint64_t, std::hash<uint64_t> > m_timeManagerOfCheckpointSerialNumbers;
    // but now sharing a single LtpTimerManager among all sessions, so use a
    // LtpTimerManager<Ltp::session_id_t, Ltp::hash_session_id_t> (which has hash map hashing function support)
    // such that: 
    //  sessionOriginatorEngineId = CHECKPOINT serial number
    //  sessionNumber = the session number
    //  since this is a sender, the real sessionOriginatorEngineId is constant among all sending sessions and is not needed
    LtpTimerManager<Ltp::session_id_t, Ltp::hash_session_id_t> m_timeManagerOfCheckpointSerialNumbers;

    boost::asio::deadline_timer m_deadlineTimerForTimeManagerOfSendingDelayedDataSegments;
    // within a session would normally be a single deadline timer;
    // but now sharing a single LtpTimerManager among all sessions, so use a
    // LtpTimerManager<uint64_t, std::hash<uint64_t> >
    // such that: 
    //  uint64_t = the session number
    //  since this is a sender, the real sessionOriginatorEngineId is constant among all sending sessions and is not needed
    LtpTimerManager<uint64_t, std::hash<uint64_t> > m_timeManagerOfSendingDelayedDataSegments;

    LtpTimerManager<Ltp::session_id_t, Ltp::hash_session_id_t>::LtpTimerExpiredCallback_t m_cancelSegmentTimerExpiredCallback;
    boost::asio::deadline_timer m_deadlineTimerForTimeManagerOfCancelSegments;
    LtpTimerManager<Ltp::session_id_t, Ltp::hash_session_id_t> m_timeManagerOfCancelSegments;

    boost::asio::deadline_timer m_housekeepingTimer;
    TokenRateLimiter m_tokenRateLimiter;
    boost::asio::deadline_timer m_tokenRefreshTimer;
    uint64_t m_maxSendRateBitsPerSecOrZeroToDisable;
    bool m_tokenRefreshTimerIsRunning;
    boost::posix_time::ptime m_lastTimeTokensWereRefreshed;
    std::unique_ptr<boost::thread> m_ioServiceLtpEngineThreadPtr;

    //session re-creation prevention
    std::map<uint64_t, std::unique_ptr<LtpSessionRecreationPreventer> > m_mapSessionOriginatorEngineIdToLtpSessionRecreationPreventer;

public:
    //stats

    uint64_t m_countAsyncSendsLimitedByRate;

    //session sender stats
    uint64_t m_numCheckpointTimerExpiredCallbacks;
    uint64_t m_numDiscretionaryCheckpointsNotResent;
    uint64_t m_numDeletedFullyClaimedPendingReports;

    //session receiver stats
    uint64_t m_numReportSegmentTimerExpiredCallbacks;
    uint64_t m_numReportSegmentsUnableToBeIssued;
    uint64_t m_numReportSegmentsTooLargeAndNeedingSplit;
    uint64_t m_numReportSegmentsCreatedViaSplit;
    uint64_t m_numGapsFilledByOutOfOrderDataSegments;
    uint64_t m_numDelayedFullyClaimedPrimaryReportSegmentsSent;
    uint64_t m_numDelayedFullyClaimedSecondaryReportSegmentsSent;
    uint64_t m_numDelayedPartiallyClaimedPrimaryReportSegmentsSent;
    uint64_t m_numDelayedPartiallyClaimedSecondaryReportSegmentsSent;
};

#endif // LTP_ENGINE_H

