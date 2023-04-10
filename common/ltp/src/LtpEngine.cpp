/**
 * @file LtpEngine.cpp
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

#include "LtpEngine.h"
#include "Logger.h"
#include <inttypes.h>
#include <boost/bind/bind.hpp>
#include <boost/make_unique.hpp>
#include "ThreadNamer.h"

static constexpr hdtn::Logger::SubProcess subprocess = hdtn::Logger::SubProcess::none;

/// Token limit of rateBytesPerSecond / (1000ms/100ms) = rateBytesPerSecond / 10
static const boost::posix_time::time_duration static_tokenMaxLimitDurationWindow(boost::posix_time::milliseconds(100));
/// Token refresh timer duration
static const boost::posix_time::time_duration static_tokenRefreshTimeDurationWindow(boost::posix_time::milliseconds(20));

/// Maximum number of pending SendPackets operations.
/// assumes (time to complete 5 batch udp send operations or 5 single packet udp send operations) < (margin time).
static constexpr unsigned int MAX_QUEUED_SEND_SYSTEM_CALLS = 5;
static constexpr unsigned int MIN_QUEUED_SEND_SYSTEM_CALLS = 1;

/// Disk pipeline limit.
/// should be even number to split between ingress and storage cut-through pipeline.
static constexpr uint64_t diskPipelineLimit = 6;

LtpEngine::cancel_segment_timer_info_t::cancel_segment_timer_info_t(const uint8_t* data) {
    memcpy(this, data, sizeof(cancel_segment_timer_info_t));
}

LtpEngine::LtpEngine(const LtpEngineConfig& ltpRxOrTxCfg, const uint8_t engineIndexForEncodingIntoRandomSessionNumber, bool startIoServiceThread) :
    M_THIS_ENGINE_ID(ltpRxOrTxCfg.thisEngineId),
    m_numQueuedSendSystemCallsAtomic(0),
    M_MAX_UDP_PACKETS_TO_SEND_PER_SYSTEM_CALL(ltpRxOrTxCfg.maxUdpPacketsToSendPerSystemCall),
    m_transmissionToAckReceivedTime((ltpRxOrTxCfg.oneWayLightTime * 2) + (ltpRxOrTxCfg.oneWayMarginTime * 2)),
    m_delaySendingOfReportSegmentsTime((ltpRxOrTxCfg.delaySendingOfReportSegmentsTimeMsOrZeroToDisable) ?
        boost::posix_time::milliseconds(ltpRxOrTxCfg.delaySendingOfReportSegmentsTimeMsOrZeroToDisable) :
        boost::posix_time::time_duration(boost::posix_time::special_values::not_a_date_time)),
    m_delaySendingOfDataSegmentsTime((ltpRxOrTxCfg.delaySendingOfDataSegmentsTimeMsOrZeroToDisable) ?
        boost::posix_time::milliseconds(ltpRxOrTxCfg.delaySendingOfDataSegmentsTimeMsOrZeroToDisable) :
        boost::posix_time::time_duration(boost::posix_time::special_values::not_a_date_time)),
    M_HOUSEKEEPING_INTERVAL(boost::posix_time::milliseconds(1000)),
    m_nowTimeRef(boost::posix_time::microsec_clock::universal_time()),
    m_stagnantRxSessionTime((m_transmissionToAckReceivedTime* static_cast<int>(ltpRxOrTxCfg.maxRetriesPerSerialNumber + 1)) + (M_HOUSEKEEPING_INTERVAL * 2)),
    M_FORCE_32_BIT_RANDOM_NUMBERS(ltpRxOrTxCfg.force32BitRandomNumbers),
    M_SENDER_PING_SECONDS_OR_ZERO_TO_DISABLE(ltpRxOrTxCfg.senderPingSecondsOrZeroToDisable),
    M_SENDER_PING_TIME(boost::posix_time::seconds(ltpRxOrTxCfg.senderPingSecondsOrZeroToDisable)),
    M_NEXT_PING_START_EXPIRY(boost::posix_time::microsec_clock::universal_time() + M_SENDER_PING_TIME),
    m_transmissionRequestServedAsPing(false),
    M_MAX_SIMULTANEOUS_SESSIONS(ltpRxOrTxCfg.maxSimultaneousSessions),
    M_MAX_SESSIONS_IN_PIPELINE((ltpRxOrTxCfg.activeSessionDataOnDiskNewFileDurationMsOrZeroToDisable != 0) ?
        std::min(M_MAX_SIMULTANEOUS_SESSIONS, diskPipelineLimit) : M_MAX_SIMULTANEOUS_SESSIONS),
    M_DISK_BUNDLE_ACK_CALLBACK_LIMIT(M_MAX_SIMULTANEOUS_SESSIONS - M_MAX_SESSIONS_IN_PIPELINE),
    M_MAX_RX_DATA_SEGMENT_HISTORY_OR_ZERO_DISABLE(ltpRxOrTxCfg.rxDataSegmentSessionNumberRecreationPreventerHistorySizeOrZeroToDisable),
    m_userAssignedUuid(UINT64_MAX), //manually set later
    m_maxRetriesPerSerialNumber(ltpRxOrTxCfg.maxRetriesPerSerialNumber),
    m_workLtpEnginePtr(boost::make_unique<boost::asio::io_service::work>(m_ioServiceLtpEngine)),
    m_deadlineTimerForTimeManagerOfReportSerialNumbers(m_ioServiceLtpEngine),
    m_timeManagerOfReportSerialNumbers(m_deadlineTimerForTimeManagerOfReportSerialNumbers, m_transmissionToAckReceivedTime, (M_MAX_SIMULTANEOUS_SESSIONS * 2) + 1),
    m_deadlineTimerForTimeManagerOfSendingDelayedReceptionReports(m_ioServiceLtpEngine),
    m_timeManagerOfSendingDelayedReceptionReports(m_deadlineTimerForTimeManagerOfSendingDelayedReceptionReports, m_delaySendingOfReportSegmentsTime, (M_MAX_SIMULTANEOUS_SESSIONS * 2) + 1), //TODO
    m_deadlineTimerForTimeManagerOfCheckpointSerialNumbers(m_ioServiceLtpEngine),
    m_timeManagerOfCheckpointSerialNumbers(m_deadlineTimerForTimeManagerOfCheckpointSerialNumbers, m_transmissionToAckReceivedTime, (M_MAX_SIMULTANEOUS_SESSIONS * 2) + 1),
    m_deadlineTimerForTimeManagerOfSendingDelayedDataSegments(m_ioServiceLtpEngine),
    m_timeManagerOfSendingDelayedDataSegments(m_deadlineTimerForTimeManagerOfSendingDelayedDataSegments, m_delaySendingOfDataSegmentsTime, M_MAX_SIMULTANEOUS_SESSIONS + 1),
    m_deadlineTimerForTimeManagerOfCancelSegments(m_ioServiceLtpEngine),
    m_timeManagerOfCancelSegments(m_deadlineTimerForTimeManagerOfCancelSegments, m_transmissionToAckReceivedTime, M_MAX_SIMULTANEOUS_SESSIONS + 1),
    m_housekeepingTimer(m_ioServiceLtpEngine),
    m_tokenRefreshTimer(m_ioServiceLtpEngine),
    m_maxSendRateBitsPerSecOrZeroToDisable(ltpRxOrTxCfg.maxSendRateBitsPerSecOrZeroToDisable),
    m_tokenRefreshTimerIsRunning(false),
    m_lastTimeTokensWereRefreshed(boost::posix_time::special_values::neg_infin),
    m_ltpSessionSenderRecycler(M_MAX_SIMULTANEOUS_SESSIONS + 1),
    m_ltpSessionSenderCommonData(
        ltpRxOrTxCfg.mtuClientServiceData,
        ltpRxOrTxCfg.checkpointEveryNthDataPacketSender,
        m_maxRetriesPerSerialNumber, //reference
        m_timeManagerOfCheckpointSerialNumbers,
        m_csnTimerExpiredCallback,
        m_timeManagerOfSendingDelayedDataSegments,
        m_delayedDataSegmentsTimerExpiredCallback,
        m_notifyEngineThatThisSenderNeedsDeletedCallback,
        m_notifyEngineThatThisSenderHasProducibleDataFunction,
        m_initialTransmissionCompletedCallbackCalledBySender,
        m_ltpSessionSenderRecycler), //reference
    m_ltpSessionReceiverRecycler(M_MAX_SIMULTANEOUS_SESSIONS + 1),
    m_ltpSessionReceiverCommonData(
        ltpRxOrTxCfg.clientServiceId,
        0, //maxReceptionClaims will be immediately set by SetMtuReportSegment below
        ltpRxOrTxCfg.estimatedBytesToReceivePerSession,
        ltpRxOrTxCfg.maxRedRxBytesPerSession,
        m_maxRetriesPerSerialNumber, //reference
        m_timeManagerOfReportSerialNumbers,
        m_rsnTimerExpiredCallback,
        m_timeManagerOfSendingDelayedReceptionReports,
        m_delayedReceptionReportTimerExpiredCallback,
        m_notifyEngineThatThisReceiverNeedsDeletedCallback,
        m_notifyEngineThatThisReceiversTimersHasProducibleDataFunction,
        m_notifyEngineThatThisReceiverCompletedDeferredOperationFunction,
        m_redPartReceptionCallback,
        m_greenPartSegmentArrivalCallback,
        m_memoryInFilesPtr, //reference
        m_ltpSessionReceiverRecycler, //reference
        m_nowTimeRef), //reference
    m_numCheckpointTimerExpiredCallbacksRef(m_ltpSessionSenderCommonData.m_numCheckpointTimerExpiredCallbacks),
    m_numDiscretionaryCheckpointsNotResentRef(m_ltpSessionSenderCommonData.m_numDiscretionaryCheckpointsNotResent),
    m_numDeletedFullyClaimedPendingReportsRef(m_ltpSessionSenderCommonData.m_numDeletedFullyClaimedPendingReports),
    m_numReportSegmentTimerExpiredCallbacksRef(m_ltpSessionReceiverCommonData.m_numReportSegmentTimerExpiredCallbacks),
    m_numReportSegmentsUnableToBeIssuedRef(m_ltpSessionReceiverCommonData.m_numReportSegmentsUnableToBeIssued),
    m_numReportSegmentsTooLargeAndNeedingSplitRef(m_ltpSessionReceiverCommonData.m_numReportSegmentsTooLargeAndNeedingSplit),
    m_numReportSegmentsCreatedViaSplitRef(m_ltpSessionReceiverCommonData.m_numReportSegmentsCreatedViaSplit),
    m_numGapsFilledByOutOfOrderDataSegmentsRef(m_ltpSessionReceiverCommonData.m_numGapsFilledByOutOfOrderDataSegments),
    m_numDelayedFullyClaimedPrimaryReportSegmentsSentRef(m_ltpSessionReceiverCommonData.m_numDelayedFullyClaimedPrimaryReportSegmentsSent),
    m_numDelayedFullyClaimedSecondaryReportSegmentsSentRef(m_ltpSessionReceiverCommonData.m_numDelayedFullyClaimedSecondaryReportSegmentsSent),
    m_numDelayedPartiallyClaimedPrimaryReportSegmentsSentRef(m_ltpSessionReceiverCommonData.m_numDelayedPartiallyClaimedPrimaryReportSegmentsSent),
    m_numDelayedPartiallyClaimedSecondaryReportSegmentsSentRef(m_ltpSessionReceiverCommonData.m_numDelayedPartiallyClaimedSecondaryReportSegmentsSent)
{
    m_cancelSegmentTimerExpiredCallback = boost::bind(&LtpEngine::CancelSegmentTimerExpiredCallback,
        this, boost::placeholders::_2, boost::placeholders::_3); //boost::placeholders::_1 is unused for classPtr, 

    //session receiver functions to be passed in AS REFERENCES
    m_notifyEngineThatThisReceiverNeedsDeletedCallback = boost::bind(&LtpEngine::NotifyEngineThatThisReceiverNeedsDeletedCallback,
        this, boost::placeholders::_1, boost::placeholders::_2, boost::placeholders::_3);
    m_notifyEngineThatThisReceiversTimersHasProducibleDataFunction = boost::bind(&LtpEngine::NotifyEngineThatThisReceiversTimersHasProducibleData,
        this, boost::placeholders::_1);
    m_notifyEngineThatThisReceiverCompletedDeferredOperationFunction = boost::bind(&LtpEngine::NotifyEngineThatThisReceiverCompletedDeferredOperation,
        this);
    m_rsnTimerExpiredCallback = boost::bind(&LtpEngine::LtpSessionReceiverReportSegmentTimerExpiredCallback, this,
        boost::placeholders::_1, boost::placeholders::_2, boost::placeholders::_3);
    m_delayedReceptionReportTimerExpiredCallback = boost::bind(&LtpEngine::LtpSessionReceiverDelaySendReportSegmentTimerExpiredCallback, this,
        boost::placeholders::_1, boost::placeholders::_2, boost::placeholders::_3);

    //session sender functions to be passed in AS REFERENCES
    m_notifyEngineThatThisSenderNeedsDeletedCallback = boost::bind(&LtpEngine::NotifyEngineThatThisSenderNeedsDeletedCallback,
        this, boost::placeholders::_1, boost::placeholders::_2, boost::placeholders::_3, boost::placeholders::_4);
    m_notifyEngineThatThisSenderHasProducibleDataFunction = boost::bind(&LtpEngine::NotifyEngineThatThisSenderHasProducibleData,
        this, boost::placeholders::_1);
    m_initialTransmissionCompletedCallbackCalledBySender = boost::bind(&LtpEngine::InitialTransmissionCompletedCallback,
        this, boost::placeholders::_1, boost::placeholders::_2);
    m_csnTimerExpiredCallback = boost::bind(&LtpEngine::LtpSessionSenderCheckpointTimerExpiredCallback, this,
        boost::placeholders::_1, boost::placeholders::_2, boost::placeholders::_3);
    m_delayedDataSegmentsTimerExpiredCallback = boost::bind(&LtpEngine::LtpSessionSenderDelaySendDataSegmentsTimerExpiredCallback, this,
        boost::placeholders::_1, boost::placeholders::_2, boost::placeholders::_3);

    m_ltpRxStateMachine.SetCancelSegmentContentsReadCallback(boost::bind(&LtpEngine::CancelSegmentReceivedCallback, this,
        boost::placeholders::_1, boost::placeholders::_2, boost::placeholders::_3,
        boost::placeholders::_4, boost::placeholders::_5));
    m_ltpRxStateMachine.SetCancelAcknowledgementSegmentContentsReadCallback(boost::bind(&LtpEngine::CancelAcknowledgementSegmentReceivedCallback, this,
        boost::placeholders::_1, boost::placeholders::_2, boost::placeholders::_3,
        boost::placeholders::_4));
    m_ltpRxStateMachine.SetReportAcknowledgementSegmentContentsReadCallback(boost::bind(&LtpEngine::ReportAcknowledgementSegmentReceivedCallback, this,
        boost::placeholders::_1, boost::placeholders::_2, boost::placeholders::_3,
        boost::placeholders::_4));
    m_ltpRxStateMachine.SetReportSegmentContentsReadCallback(boost::bind(&LtpEngine::ReportSegmentReceivedCallback, this,
        boost::placeholders::_1, boost::placeholders::_2, boost::placeholders::_3,
        boost::placeholders::_4));
    m_ltpRxStateMachine.SetDataSegmentContentsReadCallback(boost::bind(&LtpEngine::DataSegmentReceivedCallback, this,
        boost::placeholders::_1, boost::placeholders::_2, boost::placeholders::_3,
        boost::placeholders::_4, boost::placeholders::_5, boost::placeholders::_6));

    //reserve data so that operator new doesn't need called for resize of std::vector<boost::asio::const_buffer>
    if (M_MAX_UDP_PACKETS_TO_SEND_PER_SYSTEM_CALL <= 1) { //single udp send packet function call
        m_reservedUdpSendPacketInfo.resize(MAX_QUEUED_SEND_SYSTEM_CALLS * 2);
        m_reservedUdpSendPacketInfoIterator = m_reservedUdpSendPacketInfo.begin();
        for (std::size_t i = 0; i < m_reservedUdpSendPacketInfo.size(); ++i) {
            UdpSendPacketInfo& info = m_reservedUdpSendPacketInfo[i];
            info.constBufferVec.reserve(4);
            info.underlyingDataToDeleteOnSentCallback = std::make_shared<std::vector<std::vector<uint8_t> > >(4); //2 is generally worse case
            for (std::size_t j = 0; j < info.underlyingDataToDeleteOnSentCallback->size(); ++j) {
                std::vector<uint8_t>& subVec = info.underlyingDataToDeleteOnSentCallback->at(j);
                subVec.reserve(100); //generally just LTP headers, 100 more than generous, can grow with resizes
            }
        }
    }
    else { //reserve for batch send
        m_reservedDeferredReadsVec.reserve(M_MAX_UDP_PACKETS_TO_SEND_PER_SYSTEM_CALL);
        m_reservedUdpSendPacketInfoVecsForBatchSender.resize(MAX_QUEUED_SEND_SYSTEM_CALLS * 2);
        m_reservedUdpSendPacketInfoVecsForBatchSenderIterator = m_reservedUdpSendPacketInfoVecsForBatchSender.begin();
        for (std::size_t batchI = 0; batchI < m_reservedUdpSendPacketInfoVecsForBatchSender.size(); ++batchI) {
            std::shared_ptr<std::vector<UdpSendPacketInfo> >& thisReservedUdpSendPacketInfoVecPtr = m_reservedUdpSendPacketInfoVecsForBatchSender[batchI];
            thisReservedUdpSendPacketInfoVecPtr = std::make_shared<std::vector<UdpSendPacketInfo> >(M_MAX_UDP_PACKETS_TO_SEND_PER_SYSTEM_CALL * 2);
            std::vector<UdpSendPacketInfo>& thisReservedUdpSendPacketInfoVec = *thisReservedUdpSendPacketInfoVecPtr;
            for (std::size_t i = 0; i < thisReservedUdpSendPacketInfoVec.size(); ++i) {
                UdpSendPacketInfo& info = thisReservedUdpSendPacketInfoVec[i];
                info.constBufferVec.reserve(4);
                info.underlyingDataToDeleteOnSentCallback = std::make_shared<std::vector<std::vector<uint8_t> > >(4); //2 is generally worse case
                for (std::size_t j = 0; j < info.underlyingDataToDeleteOnSentCallback->size(); ++j) {
                    std::vector<uint8_t>& subVec = info.underlyingDataToDeleteOnSentCallback->at(j);
                    subVec.reserve(100); //generally just LTP headers, 100 more than generous, can grow with resizes
                }
            }
        }
    }

    m_rng.SetEngineIndex(engineIndexForEncodingIntoRandomSessionNumber);
    SetMtuReportSegment(ltpRxOrTxCfg.mtuReportSegment);

    UpdateRate(m_maxSendRateBitsPerSecOrZeroToDisable);
    if (m_maxSendRateBitsPerSecOrZeroToDisable) {
        const uint64_t tokenLimit = m_tokenRateLimiter.GetRemainingTokens();
        LOG_INFO(subprocess) << "LtpEngine: rate bitsPerSec = " << m_maxSendRateBitsPerSecOrZeroToDisable << "  token limit = " << tokenLimit;
    }

    Reset();

    //start housekeeping timer (ping for tx engines and session leak detection for rx engines)
    m_housekeepingTimer.expires_at(boost::posix_time::microsec_clock::universal_time() + M_HOUSEKEEPING_INTERVAL);
    m_housekeepingTimer.async_wait(boost::bind(&LtpEngine::OnHousekeeping_TimerExpired, this, boost::asio::placeholders::error));

    //start memory in files for keeping session data on disk instead of RAM
    if (ltpRxOrTxCfg.activeSessionDataOnDiskNewFileDurationMsOrZeroToDisable && startIoServiceThread) { //startIoServiceThread needed to ensure skip using memoryInFiles when under TestLtpEngine
        m_memoryInFilesPtr = boost::make_unique<MemoryInFiles>(m_ioServiceLtpEngine,
            ltpRxOrTxCfg.activeSessionDataOnDiskDirectory,
            ltpRxOrTxCfg.activeSessionDataOnDiskNewFileDurationMsOrZeroToDisable,
            M_MAX_SIMULTANEOUS_SESSIONS * 2);
    }
    //sizeof(LtpSessionSender); //272 => 224 using two ForwardListQueue's instead of two std::queue's
    //sizeof(LtpSessionReceiver); //248 => 216 using one ForwardListQueue instead of one std::queue and removing m_dataReceivedRedSize
    if (startIoServiceThread) {
        m_ioServiceLtpEngineThreadPtr = boost::make_unique<boost::thread>(boost::bind(&boost::asio::io_service::run, &m_ioServiceLtpEngine));
        const std::string threadName = "ioServiceLtpEngine engId=" + boost::lexical_cast<std::string>(M_THIS_ENGINE_ID);
        //ThreadNamer::SetThreadName(*m_ioServiceLtpEngineThreadPtr, threadName); //won't work on linux
        ThreadNamer::SetIoServiceThreadName(m_ioServiceLtpEngine, threadName);
    }
}

LtpEngine::~LtpEngine() {
    LOG_INFO(subprocess) << "LtpEngine Stats:"; //print before reset
    LOG_INFO(subprocess) << "numCheckpointTimerExpiredCallbacks: " << m_numCheckpointTimerExpiredCallbacksRef;
    LOG_INFO(subprocess) << "numDiscretionaryCheckpointsNotResent: " << m_numDiscretionaryCheckpointsNotResentRef;
    LOG_INFO(subprocess) << "numDeletedFullyClaimedPendingReports: " << m_numDeletedFullyClaimedPendingReportsRef;
    LOG_INFO(subprocess) << "numReportSegmentTimerExpiredCallbacks: " << m_numReportSegmentTimerExpiredCallbacksRef;
    LOG_INFO(subprocess) << "numReportSegmentsUnableToBeIssued: " << m_numReportSegmentsUnableToBeIssuedRef;
    LOG_INFO(subprocess) << "numReportSegmentsTooLargeAndNeedingSplit: " << m_numReportSegmentsTooLargeAndNeedingSplitRef;
    LOG_INFO(subprocess) << "numReportSegmentsCreatedViaSplit: " << m_numReportSegmentsCreatedViaSplitRef;
    LOG_INFO(subprocess) << "numGapsFilledByOutOfOrderDataSegments: " << m_numGapsFilledByOutOfOrderDataSegmentsRef;
    LOG_INFO(subprocess) << "numDelayedFullyClaimedPrimaryReportSegmentsSent: " << m_numDelayedFullyClaimedPrimaryReportSegmentsSentRef;
    LOG_INFO(subprocess) << "numDelayedFullyClaimedSecondaryReportSegmentsSent: " << m_numDelayedFullyClaimedSecondaryReportSegmentsSentRef;
    LOG_INFO(subprocess) << "numDelayedPartiallyClaimedPrimaryReportSegmentsSent: " << m_numDelayedPartiallyClaimedPrimaryReportSegmentsSentRef;
    LOG_INFO(subprocess) << "numDelayedPartiallyClaimedSecondaryReportSegmentsSent: " << m_numDelayedPartiallyClaimedSecondaryReportSegmentsSentRef;
    LOG_INFO(subprocess) << "countAsyncSendsLimitedByRate " << m_countAsyncSendsLimitedByRate;
    LOG_INFO(subprocess) << "countPacketsWithOngoingOperations=" << m_countPacketsWithOngoingOperations
        << " countPacketsThatCompletedOngoingOperations=" << m_countPacketsThatCompletedOngoingOperations
        << " numEventsTxRequestDiskWritesTooSlow=" << m_numEventsTransmissionRequestDiskWritesTooSlow;

    if (m_ioServiceLtpEngineThreadPtr) {
        try {
            m_housekeepingTimer.cancel(); //keep here instead of Reset() so unit test can call Reset()
        }
        catch (boost::system::system_error& e) {
            LOG_ERROR(subprocess) << "unable to cancel housekeeping timer: " << e.what();
        }
        boost::asio::post(m_ioServiceLtpEngine, boost::bind(&LtpEngine::Reset, this));
        m_workLtpEnginePtr.reset(); //erase the work object (destructor is thread safe) so that io_service thread will exit when it runs out of work
        try {
            m_ioServiceLtpEngineThreadPtr->join();
            m_ioServiceLtpEngineThreadPtr.reset(); //delete it
        }
        catch (boost::thread_resource_error& e) {
            LOG_ERROR(subprocess) << "unable to stop ioServiceLtpEngineThread: " << e.what();
        }
    }
}

void LtpEngine::Reset() {
    m_mapSessionNumberToSessionSender.clear();
    m_mapSessionIdToSessionReceiver.clear();

    //By default, unordered_map containers have a max_load_factor of 1.0.
    m_mapSessionNumberToSessionSender.reserve(M_MAX_SIMULTANEOUS_SESSIONS << 1);
    m_mapSessionIdToSessionReceiver.reserve(M_MAX_SIMULTANEOUS_SESSIONS << 1);

    //set max number of recyclable allocated max elements for the map
    // - once M_MAX_SIMULTANEOUS_SESSIONS has been reached, operater new ops will cease
    // - if M_MAX_SIMULTANEOUS_SESSIONS is never exceeded, operator delete will never occur
    // + 2 => to add slight buffer
    m_mapSessionNumberToSessionSender.get_allocator().SetMaxListSizeFromGetAllocatorCopy(M_MAX_SIMULTANEOUS_SESSIONS + 2);
    m_mapSessionIdToSessionReceiver.get_allocator().SetMaxListSizeFromGetAllocatorCopy(M_MAX_SIMULTANEOUS_SESSIONS + 2);

    m_ltpRxStateMachine.InitRx();
    m_queueClosedSessionDataToSend = std::queue<std::pair<uint64_t, std::vector<uint8_t> > >();

    //the following are single threaded resets, but safe because Reset() is called by boost::asio::post(m_ioServiceLtpEngine...
    m_timeManagerOfCancelSegments.Reset();
    m_timeManagerOfCheckpointSerialNumbers.Reset();
    m_timeManagerOfReportSerialNumbers.Reset();
    m_timeManagerOfSendingDelayedReceptionReports.Reset();

    m_queueCancelSegmentTimerInfo = std::queue<cancel_segment_timer_info_t>();
    m_queueSendersNeedingDeleted = std::queue<uint64_t>();
    m_queueSendersNeedingTimeCriticalDataSent = std::queue<uint64_t>();
    m_queueSendersNeedingFirstPassDataSent = std::queue<uint64_t>();
    m_queueReceiversNeedingDeleted = std::queue<Ltp::session_id_t>();
    m_queueReceiversNeedingDeletedButUnsafeToDelete = std::queue<Ltp::session_id_t>();
    m_queueReceiversNeedingDataSent = std::queue<Ltp::session_id_t>();

    m_countAsyncSendsLimitedByRate = 0;
    m_countPacketsWithOngoingOperations = 0;
    m_countPacketsThatCompletedOngoingOperations = 0;
    m_numEventsTransmissionRequestDiskWritesTooSlow = 0;
    m_totalRedDataBytesSuccessfullySent = 0;
    m_totalRedDataBytesFailedToSend = 0;
    m_totalCancelSegmentsStarted = 0;
    m_totalCancelSegmentSendRetries = 0;
    m_totalCancelSegmentsFailedToSend = 0;
    m_totalCancelSegmentsAcknowledged = 0;
    m_totalPingsStarted = 0;
    m_totalPingRetries = 0;
    m_totalPingsFailedToSend = 0;
    m_totalPingsAcknowledged = 0;
    m_numTxSessionsReturnedToStorage = 0;
    m_numTxSessionsCancelledByReceiver = 0;
    m_numRxSessionsCancelledBySender = 0;
    m_numStagnantRxSessionsDeleted = 0;
    m_senderLinkIsUpPhysically = true; //assume the link on startup is up physically until ping determines it down (for gui)

    m_numCheckpointTimerExpiredCallbacksRef = 0;
    m_numDiscretionaryCheckpointsNotResentRef = 0;
    m_numDeletedFullyClaimedPendingReportsRef = 0;

    m_numReportSegmentTimerExpiredCallbacksRef = 0;
    m_numReportSegmentsUnableToBeIssuedRef = 0;
    m_numReportSegmentsTooLargeAndNeedingSplitRef = 0;
    m_numReportSegmentsCreatedViaSplitRef = 0;
    m_numGapsFilledByOutOfOrderDataSegmentsRef = 0;
    m_numDelayedFullyClaimedPrimaryReportSegmentsSentRef = 0;
    m_numDelayedFullyClaimedSecondaryReportSegmentsSentRef = 0;
    m_numDelayedPartiallyClaimedPrimaryReportSegmentsSentRef = 0;
    m_numDelayedPartiallyClaimedSecondaryReportSegmentsSentRef = 0;
}

void LtpEngine::SetCheckpointEveryNthDataPacketForSenders(uint64_t checkpointEveryNthDataPacketSender) {
    m_ltpSessionSenderCommonData.m_checkpointEveryNthDataPacket = checkpointEveryNthDataPacketSender;
}
uint8_t LtpEngine::GetEngineIndex() {
    return m_rng.GetEngineIndex();
}

void LtpEngine::SetMtuReportSegment_ThreadSafe(uint64_t mtuReportSegment) {
    boost::asio::post(m_ioServiceLtpEngine, boost::bind(&LtpEngine::SetMtuReportSegment, this, mtuReportSegment));
}
void LtpEngine::SetMtuReportSegment(uint64_t mtuReportSegment) {
    //(5 * 10) + (receptionClaims.size() * (2 * 10)); //5 sdnvs * 10 bytes sdnv max + reception claims * 2sdnvs per claim
    //70 bytes worst case minimum for 1 claim
    if (mtuReportSegment < 70) {
        LOG_ERROR(subprocess) << "LtpEngine::SetMtuReportSegment: mtuReportSegment must be at least 70 bytes!!!!.. setting to 70 bytes";
        m_ltpSessionReceiverCommonData.m_maxReceptionClaims = 1;
    }
    m_ltpSessionReceiverCommonData.m_maxReceptionClaims = (mtuReportSegment - 50) / 20;
    LOG_INFO(subprocess) << "max reception claims = " << m_ltpSessionReceiverCommonData.m_maxReceptionClaims;
}

void LtpEngine::SetMtuDataSegment_ThreadSafe(uint64_t mtuDataSegment) {
    boost::asio::post(m_ioServiceLtpEngine, boost::bind(&LtpEngine::SetMtuDataSegment, this, mtuDataSegment));
}
void LtpEngine::SetMtuDataSegment(uint64_t mtuDataSegment) {
    m_ltpSessionSenderCommonData.m_mtuClientServiceData = mtuDataSegment;
}

bool LtpEngine::PacketIn(const bool isLastChunkOfPacket, const uint8_t * data, const std::size_t size,
    Ltp::SessionOriginatorEngineIdDecodedCallback_t * sessionOriginatorEngineIdDecodedCallbackPtr)
{
    std::string errorMessage;
    bool operationIsOngoing;
    bool success = m_ltpRxStateMachine.HandleReceivedChars(data, size, operationIsOngoing, errorMessage, sessionOriginatorEngineIdDecodedCallbackPtr);
    if (!success) {
        LOG_ERROR(subprocess) << "LtpEngine::PacketIn: " << errorMessage;
        m_ltpRxStateMachine.InitRx();
    }
    else if (isLastChunkOfPacket && (!m_ltpRxStateMachine.IsAtBeginningState())) {
        LOG_ERROR(subprocess) << "LtpEngine::PacketIn: not at beginning state (only partial udp packet?).";
        m_ltpRxStateMachine.InitRx();
        success = false;
    }

    if (operationIsOngoing) { //operationIsOngoing always valid because HandleReceivedChars sets it to false immediately
        //LOG_DEBUG(subprocess) << "LtpEngine::PacketIn: operation is ongoing.";
        //Receiver is writing this UDP packet data segment to disk (asynchronously).
        //Once the disk write eventually completes, the
        //Receiver will then call LtpEngine::NotifyEngineThatThisReceiverCompletedDeferredOperation()
        //which immediately calls PacketInFullyProcessedCallback(true)
        //which immediately calls m_circularIndexBuffer.CommitRead();
        //See comment below in LtpEngine::NotifyEngineThatThisReceiverCompletedDeferredOperation().
        ++m_countPacketsWithOngoingOperations;
    }
    else {
        PacketInFullyProcessedCallback(success); //immediately calls m_circularIndexBuffer.CommitRead();
    }
    return success;
}

bool LtpEngine::PacketIn(const std::vector<boost::asio::const_buffer> & constBufferVec) { //for testing
    for (std::size_t i = 0; i < constBufferVec.size(); ++i) {
        const boost::asio::const_buffer & cb = constBufferVec[i];
        const bool isLastChunkOfPacket = (i == (constBufferVec.size() - 1));
        if (!PacketIn(isLastChunkOfPacket, (const uint8_t *) cb.data(), cb.size())) {
            return false;
        }
    }
    return true;
}

void LtpEngine::PacketIn_ThreadSafe(const uint8_t * data, const std::size_t size,
    Ltp::SessionOriginatorEngineIdDecodedCallback_t * sessionOriginatorEngineIdDecodedCallbackPtr)
{
    boost::asio::post(m_ioServiceLtpEngine, boost::bind(&LtpEngine::PacketIn, this, true, data, size, sessionOriginatorEngineIdDecodedCallbackPtr));
}
void LtpEngine::PacketIn_ThreadSafe(const std::vector<boost::asio::const_buffer> & constBufferVec) { //for testing
    boost::asio::post(m_ioServiceLtpEngine, boost::bind(&LtpEngine::PacketIn, this, constBufferVec));
}

//called by HandleUdpSend (from the LtpUdpEngine thread)
void LtpEngine::OnSendPacketsSystemCallCompleted_ThreadSafe() {
    //This function is short. The LtpUdpEngine thread simply notifies the LtpEngine thread to do the work of finding data to send.
    //Meanwhile, the LtpUdpEngine thread can return quickly and go back to doing UDP operations.
    //Try to reduce the number of calls to post() as this is an expensive operation
    // (it should not need to be called for every udp send operation, hence the atomic variable)
    if(--m_numQueuedSendSystemCallsAtomic <= MIN_QUEUED_SEND_SYSTEM_CALLS) {
        boost::asio::post(m_ioServiceLtpEngine, boost::bind(&LtpEngine::TrySaturateSendPacketPipeline, this));
    }
}

//called by callbacks of session senders and session receivers to prevent them from deleting themselves during their own code execution
//because TrySaturateSendPacketPipeline() calls TrySendPacketIfAvailable() which can delete session senders and session receivers
void LtpEngine::SignalReadyForSend_ThreadSafe() {
    boost::asio::post(m_ioServiceLtpEngine, boost::bind(&LtpEngine::TrySaturateSendPacketPipeline, this));
}

void LtpEngine::TrySaturateSendPacketPipeline() {
    while (TrySendPacketIfAvailable()) {}
}

static std::size_t GetBytesToSend(const std::vector<boost::asio::const_buffer>& cbVec) {
    std::size_t bytesToSend = 0;
    for (std::size_t i = 0; i < cbVec.size(); ++i) {
        bytesToSend += cbVec[i].size();
    }
    return bytesToSend;
}

bool LtpEngine::TrySendPacketIfAvailable() {
    bool sendOperationQueuedSuccessfully = false;
    if (m_ioServiceLtpEngineThreadPtr && (m_numQueuedSendSystemCallsAtomic < MAX_QUEUED_SEND_SYSTEM_CALLS)) { //m_ioServiceLtpEngineThreadPtr => if not running inside a unit test
        //RATE STUFF (the TrySendPacketIfAvailable and OnTokenRefresh_TimerExpired run in the same thread)
        //if rate limiting enabled AND no tokens available for next send, TrySendPacketIfAvailable() will be called at the next m_tokenRefreshTimer expiration
        if (m_maxSendRateBitsPerSecOrZeroToDisable && (!m_tokenRateLimiter.CanTakeTokens())) { 
            TryRestartTokenRefreshTimer(); //make sure this is running so that tokens can be replenished, and don't send anything for now
        }
        else if (M_MAX_UDP_PACKETS_TO_SEND_PER_SYSTEM_CALL <= 1) { //single udp send packet function call
            UdpSendPacketInfo& udpSendPacketInfo = *m_reservedUdpSendPacketInfoIterator;
            if (GetNextPacketToSend(udpSendPacketInfo)) { //sets memoryBlockId = 0; //MUST BE RESET TO 0 since this is a recycled struct
                sendOperationQueuedSuccessfully = true;
                ++m_reservedUdpSendPacketInfoIterator;
                if (m_reservedUdpSendPacketInfoIterator == m_reservedUdpSendPacketInfo.end()) {
                    m_reservedUdpSendPacketInfoIterator = m_reservedUdpSendPacketInfo.begin();
                }
                if (m_maxSendRateBitsPerSecOrZeroToDisable) { //if rate limiting enabled
                    const std::size_t bytesToSend = GetBytesToSend(udpSendPacketInfo.constBufferVec);
                    m_tokenRateLimiter.TakeTokens(bytesToSend);
                    TryRestartTokenRefreshTimer(); //tokens were taken, so make sure this is running so that tokens can be replenished
                }
                if (udpSendPacketInfo.deferredRead.memoryBlockId) {
                    if (udpSendPacketInfo.underlyingCsDataToDeleteOnSentCallback) {
                        LOG_ERROR(subprocess) << "after GetNextPacketToSend with read from disk, got non-null underlyingCsDataToDeleteOnSentCallback";
                    }
                    else if (!m_memoryInFilesPtr->ReadMemoryAsync(udpSendPacketInfo.deferredRead,
                        boost::bind(&LtpEngine::OnDeferredReadCompleted,
                            this, boost::placeholders::_1,
                            boost::cref(udpSendPacketInfo.constBufferVec), //don't move to preserve preallocation
                            udpSendPacketInfo.underlyingDataToDeleteOnSentCallback))) //copy the shared_ptr (want to preserve preallocation)
                    {
                        LOG_ERROR(subprocess) << "cannot start async read from disk for memoryBlockId="
                            << udpSendPacketInfo.deferredRead.memoryBlockId
                            << " length=" << udpSendPacketInfo.deferredRead.length
                            << " offset=" << udpSendPacketInfo.deferredRead.offset
                            << " ptr=" << udpSendPacketInfo.deferredRead.readToThisLocationPtr;
                    }
                }
                else {
                    //copy the shared_ptr since it will get moved from SendPacket(), but want to preserve preallocation
                    std::shared_ptr<std::vector<std::vector<uint8_t> > > underlyingDataToDeleteOnSentCallbackCopy(udpSendPacketInfo.underlyingDataToDeleteOnSentCallback);

                    SendPacket(udpSendPacketInfo.constBufferVec, //const ref, preallocation preserved
                        std::move(underlyingDataToDeleteOnSentCallbackCopy), //copy gets moved, preallocation preserved
                        std::move(udpSendPacketInfo.underlyingCsDataToDeleteOnSentCallback) //gets moved, SessionSender may have attached a shared_ptr copy of its data
                    ); // , sessionOriginatorEngineId); //virtual call to child implementation
                }
            }
        }
        else { //batch udp send packets using a single function call
            std::shared_ptr<std::vector<UdpSendPacketInfo> >& udpSendPacketInfoVecPtr = *m_reservedUdpSendPacketInfoVecsForBatchSenderIterator;
            std::vector<UdpSendPacketInfo>& udpSendPacketInfoVec = *udpSendPacketInfoVecPtr;

            std::size_t numPacketsToSend = 0;
            std::size_t bytesToSend = 0;
            for (std::size_t packetI = 0; (packetI < M_MAX_UDP_PACKETS_TO_SEND_PER_SYSTEM_CALL); ++packetI) {
                UdpSendPacketInfo& udpSendPacketInfo = udpSendPacketInfoVec[packetI];
                if (!GetNextPacketToSend(udpSendPacketInfo)) { //sets memoryBlockId = 0; //MUST BE RESET TO 0 since this is a recycled struct
                    break;
                }
                if (packetI == 0) { //a system call is guaranteed
                    sendOperationQueuedSuccessfully = true;
                    ++m_reservedUdpSendPacketInfoVecsForBatchSenderIterator;
                    if (m_reservedUdpSendPacketInfoVecsForBatchSenderIterator == m_reservedUdpSendPacketInfoVecsForBatchSender.end()) {
                        m_reservedUdpSendPacketInfoVecsForBatchSenderIterator = m_reservedUdpSendPacketInfoVecsForBatchSender.begin();
                    }
                    if (m_memoryInFilesPtr) {
                        m_reservedDeferredReadsVec.resize(0);
                    }
                }
                if (m_maxSendRateBitsPerSecOrZeroToDisable) { //if rate limiting enabled
                    bytesToSend += GetBytesToSend(udpSendPacketInfo.constBufferVec);
                }
                ++numPacketsToSend;
                if (udpSendPacketInfo.deferredRead.memoryBlockId) {
                    m_reservedDeferredReadsVec.emplace_back(udpSendPacketInfo.deferredRead);
                }
                //udpSendPacketInfo.Reset(); //call before subsequent GetNextPacketToSend() (not needed because destructed and recreated on each loop iteration)
            }
            if (numPacketsToSend) {
                if (m_maxSendRateBitsPerSecOrZeroToDisable) { //if rate limiting enabled
                    m_tokenRateLimiter.TakeTokens(bytesToSend);
                    TryRestartTokenRefreshTimer(); //tokens were taken, so make sure this is running so that tokens can be replenished
                }

                //copy the shared_ptr since it will get moved from SendPackets(), but want to preserve preallocation
                std::shared_ptr<std::vector<UdpSendPacketInfo> > udpSendPacketInfoVecPtrCopy(udpSendPacketInfoVecPtr);

                if (m_reservedDeferredReadsVec.size()) { //if non-zero, will always have non-zero constBufferVecs.size()
                    if (!m_memoryInFilesPtr->ReadMemoryAsync(m_reservedDeferredReadsVec,
                        boost::bind(&LtpEngine::OnDeferredMultiReadCompleted,
                            this, boost::placeholders::_1, std::move(udpSendPacketInfoVecPtrCopy), numPacketsToSend)))
                    {
                        LOG_ERROR(subprocess) << "cannot start multi async read from disk of size " << m_reservedDeferredReadsVec.size();
                    }
                }
                else { //if (constBufferVecs.size()) { //data to batch send
                    SendPackets(std::move(udpSendPacketInfoVecPtrCopy), numPacketsToSend); //virtual call to child implementation
                }
            }
        }
        
        if (m_memoryInFilesPtr) {
            //GetNextPacketToSend will immediately delete session in fully green case.
            //Memory block id is queued for deletion on session deletion to prevent memory block deletion before a call to ReadMemoryAsync.
            //Now that ReadMemoryAsync has been safely called, check if there are any queued memory block deletion requests.
            while (m_memoryBlockIdsPendingDeletionQueue.size()) {
                const uint64_t memoryBlockId = m_memoryBlockIdsPendingDeletionQueue.front();
                if (!m_memoryInFilesPtr->DeleteMemoryBlock(memoryBlockId)) {
                    LOG_INFO(subprocess) << "LTP write to disk support enabled for session sender but cannot erase memoryBlockId=" << memoryBlockId;
                }
                else {
                    //LOG_DEBUG(subprocess) << "erased memoryBlockId=" << memoryBlockId;
                }
                m_memoryBlockIdsPendingDeletionQueue.pop();
            }
        }
        
    }
    //m_numQueuedSendSystemCallsAtomic += sendOperationQueuedSuccessfully;
    if (sendOperationQueuedSuccessfully) {
        ++m_numQueuedSendSystemCallsAtomic; //atomic
    }
    return sendOperationQueuedSuccessfully;
}

void LtpEngine::PacketInFullyProcessedCallback(bool success) {}

void LtpEngine::OnDeferredReadCompleted(bool success, const std::vector<boost::asio::const_buffer>& constBufferVec,
    std::shared_ptr<std::vector<std::vector<uint8_t> > >& underlyingDataToDeleteOnSentCallback)
{
    if (success) {
        std::shared_ptr<LtpClientServiceDataToSend> nullClientServiceData;
        SendPacket(constBufferVec, std::move(underlyingDataToDeleteOnSentCallback), std::move(nullClientServiceData)); //virtual call to child implementation
    }
    else {
        LOG_ERROR(subprocess) << "Failure in LtpEngine::OnDeferredReadCompleted";
    }
}
void LtpEngine::OnDeferredMultiReadCompleted(bool success, std::shared_ptr<std::vector<UdpSendPacketInfo> >& udpSendPacketInfoVecSharedPtr, const std::size_t numPacketsToSend)
{
    if (success) {
        SendPackets(std::move(udpSendPacketInfoVecSharedPtr), numPacketsToSend); //virtual call to child implementation
    }
    else {
        LOG_ERROR(subprocess) << "Failure in LtpEngine::OnDeferredReadCompleted";
    }
}

void LtpEngine::SendPacket(const std::vector<boost::asio::const_buffer>& constBufferVec,
    std::shared_ptr<std::vector<std::vector<uint8_t> > >&& underlyingDataToDeleteOnSentCallback,
    std::shared_ptr<LtpClientServiceDataToSend>&& underlyingCsDataToDeleteOnSentCallback) {}

void LtpEngine::SendPackets(std::shared_ptr<std::vector<UdpSendPacketInfo> >&& udpSendPacketInfoVecSharedPtr, const std::size_t numPacketsToSend) {}

bool LtpEngine::GetNextPacketToSend(UdpSendPacketInfo& udpSendPacketInfo) {
    udpSendPacketInfo.deferredRead.memoryBlockId = 0; //MUST BE RESET TO 0 since this is a recycled struct
    while (!m_queueSendersNeedingDeleted.empty()) {
        map_session_number_to_session_sender_t::iterator txSessionIt = m_mapSessionNumberToSessionSender.find(m_queueSendersNeedingDeleted.front());
        if (txSessionIt != m_mapSessionNumberToSessionSender.end()) { //found
            if (txSessionIt->second.NextTimeCriticalDataToSend(udpSendPacketInfo)) { //if the session to be deleted still has data to send, send it before deletion
                udpSendPacketInfo.sessionOriginatorEngineId = M_THIS_ENGINE_ID;
                return true;
            }
            else {
                std::shared_ptr<LtpClientServiceDataToSend>& csdRef = txSessionIt->second.m_dataToSendSharedPtr;
                const bool successCallbackAlreadyCalled = (m_memoryInFilesPtr) ? true : false;
                if (txSessionIt->second.m_isFailedSession) { //give the bundle back to the user
                    m_senderLinkIsUpPhysically = false;
                    TryReturnTxSessionDataToUser(txSessionIt);
                }
                else { //successful send
                    m_senderLinkIsUpPhysically = true;
                    if ((!successCallbackAlreadyCalled) && m_onSuccessfulBundleSendCallback) {
                        m_onSuccessfulBundleSendCallback(csdRef->m_userData, m_userAssignedUuid);
                    }
                }
                EraseTxSession(txSessionIt);
            }
        }
        else {
            LOG_ERROR(subprocess) << "LtpEngine::GetNextPacketToSend: could not find session sender " << m_queueSendersNeedingDeleted.front() << " to delete";
        }
        m_queueSendersNeedingDeleted.pop();
    }

    //this while loop for session senders writing to disk (run after m_queueSendersNeedingDeleted (above) may have freed up a session)
    while ((!m_userDataPendingSuccessfulBundleSendCallbackQueue.empty()) && (m_mapSessionNumberToSessionSender.size() < M_DISK_BUNDLE_ACK_CALLBACK_LIMIT)) {
        if (m_onSuccessfulBundleSendCallback) {
            m_onSuccessfulBundleSendCallback(m_userDataPendingSuccessfulBundleSendCallbackQueue.front(), m_userAssignedUuid);
        }
        m_userDataPendingSuccessfulBundleSendCallbackQueue.pop();
        ++m_numEventsTransmissionRequestDiskWritesTooSlow;
    }

    while (!m_queueReceiversNeedingDeletedButUnsafeToDelete.empty()) {
        map_session_id_to_session_receiver_t::iterator rxSessionIt = m_mapSessionIdToSessionReceiver.find(m_queueReceiversNeedingDeletedButUnsafeToDelete.front());
        if (rxSessionIt != m_mapSessionIdToSessionReceiver.end()) { //found rx Session
            if (rxSessionIt->second.IsSafeToDelete()) {
                m_queueReceiversNeedingDeleted.emplace(m_queueReceiversNeedingDeletedButUnsafeToDelete.front()); //transfer to other queue
            }
            else {
                break; //halt queue operations until first queued safe to delete (and won't pop)
            }
        }
        else {
            LOG_ERROR(subprocess) << "LtpEngine::GetNextPacketToSend: could not find session receiver " << m_queueReceiversNeedingDeletedButUnsafeToDelete.front() << " to try to delete";
        }
        m_queueReceiversNeedingDeletedButUnsafeToDelete.pop();
    }

    while (!m_queueReceiversNeedingDeleted.empty()) {
        map_session_id_to_session_receiver_t::iterator rxSessionIt = m_mapSessionIdToSessionReceiver.find(m_queueReceiversNeedingDeleted.front());
        if (rxSessionIt != m_mapSessionIdToSessionReceiver.end()) { //found rx Session
            if (rxSessionIt->second.NextDataToSend(udpSendPacketInfo)) { //if the session to be deleted still has data to send, send it before deletion
                udpSendPacketInfo.sessionOriginatorEngineId = rxSessionIt->first.sessionOriginatorEngineId;
                return true;
            }
            else if (rxSessionIt->second.IsSafeToDelete()) {
                EraseRxSession(rxSessionIt);
            }
            else {
                m_queueReceiversNeedingDeletedButUnsafeToDelete.emplace(m_queueReceiversNeedingDeleted.front()); //postpone deletion until safe
            }
        }
        else {
            LOG_ERROR(subprocess) << "LtpEngine::GetNextPacketToSend: could not find session receiver " << m_queueReceiversNeedingDeleted.front() << " to delete";
        }
        m_queueReceiversNeedingDeleted.pop();
    }

    if (!m_queueCancelSegmentTimerInfo.empty()) {
        //6.15.  Start Cancel Timer
        //This procedure is triggered by arrival of a link state cue indicating
        //the de - queuing(for transmission) of a Cx segment.
        //Response: the expected arrival time of the CAx segment that will be
        //produced on reception of this Cx segment is computed and a countdown
        //timer for this arrival time is started.
        cancel_segment_timer_info_t & info = m_queueCancelSegmentTimerInfo.front();
        

        //send Cancel Segment
        udpSendPacketInfo.underlyingDataToDeleteOnSentCallback = std::make_shared<std::vector<std::vector<uint8_t> > >(1);
        Ltp::GenerateCancelSegmentLtpPacket((*udpSendPacketInfo.underlyingDataToDeleteOnSentCallback)[0],
            info.sessionId, info.reasonCode, info.isFromSender, NULL, NULL);
        udpSendPacketInfo.constBufferVec.resize(1);
        udpSendPacketInfo.constBufferVec[0] = boost::asio::buffer((*udpSendPacketInfo.underlyingDataToDeleteOnSentCallback)[0]);
        udpSendPacketInfo.sessionOriginatorEngineId = info.sessionId.sessionOriginatorEngineId;

        const uint8_t * const infoPtr = (uint8_t*)&info;
        std::vector<uint8_t> userData;
        m_timeManagerOfCancelSegments.m_userDataRecycler.GetRecycledOrCreateNewUserData(userData);
        userData.assign(infoPtr, infoPtr + sizeof(info));
        m_timeManagerOfCancelSegments.StartTimer(NULL, info.sessionId, &m_cancelSegmentTimerExpiredCallback, std::move(userData));
        m_queueCancelSegmentTimerInfo.pop();
        return true;
    }

    if (!m_queueClosedSessionDataToSend.empty()) { //includes report ack segments and cancel ack segments from closed sessions (which do not require timers)
        //highest priority
        udpSendPacketInfo.underlyingDataToDeleteOnSentCallback = std::make_shared<std::vector<std::vector<uint8_t> > >(1);
        (*udpSendPacketInfo.underlyingDataToDeleteOnSentCallback)[0] = std::move(m_queueClosedSessionDataToSend.front().second);
        udpSendPacketInfo.sessionOriginatorEngineId = m_queueClosedSessionDataToSend.front().first;
        m_queueClosedSessionDataToSend.pop();
        udpSendPacketInfo.constBufferVec.resize(1);
        udpSendPacketInfo.constBufferVec[0] = boost::asio::buffer((*udpSendPacketInfo.underlyingDataToDeleteOnSentCallback)[0]);
        return true;
    }

    while (!m_queueSendersNeedingTimeCriticalDataSent.empty()) { //note that m_queueSendersNeedingDeleted from above may empty the data by the time it reaches this point
        map_session_number_to_session_sender_t::iterator txSessionIt = m_mapSessionNumberToSessionSender.find(m_queueSendersNeedingTimeCriticalDataSent.front());
        if (txSessionIt != m_mapSessionNumberToSessionSender.end()) { //found
            if (txSessionIt->second.NextTimeCriticalDataToSend(udpSendPacketInfo)) {
                udpSendPacketInfo.sessionOriginatorEngineId = M_THIS_ENGINE_ID;
                return true;
            }
            else {
                //remove from queue (only when this sender is completely done sending data)
                m_queueSendersNeedingTimeCriticalDataSent.pop();
            }
        }
        else {
            //remove from queue (if the session number no longer exists do to deletion)
            m_queueSendersNeedingTimeCriticalDataSent.pop();
        }
    }

    while (!m_queueReceiversNeedingDataSent.empty()) {
        map_session_id_to_session_receiver_t::iterator rxSessionIt = m_mapSessionIdToSessionReceiver.find(m_queueReceiversNeedingDataSent.front());
        if (rxSessionIt != m_mapSessionIdToSessionReceiver.end()) { //found rx Session
            if (rxSessionIt->second.NextDataToSend(udpSendPacketInfo)) { //if the session to be deleted still has data to send, send it before deletion
                udpSendPacketInfo.sessionOriginatorEngineId = rxSessionIt->first.sessionOriginatorEngineId;
                return true;
            }
            else {
                //remove from queue (only when this receiver is completely done sending data)
                m_queueReceiversNeedingDataSent.pop();
            }
        }
        else {
            //remove from queue (if the session number no longer exists do to deletion)
            m_queueReceiversNeedingDataSent.pop();
        }
    }

    //first pass data is lowest priority compared to time-critical data above,
    //so put it last to prevent long first passes from starving and expiring the time-critical data
    while (!m_queueSendersNeedingFirstPassDataSent.empty()) {
        map_session_number_to_session_sender_t::iterator txSessionIt = m_mapSessionNumberToSessionSender.find(m_queueSendersNeedingFirstPassDataSent.front());
        if (txSessionIt != m_mapSessionNumberToSessionSender.end()) { //found
            if (txSessionIt->second.NextFirstPassDataToSend(udpSendPacketInfo)) {
                udpSendPacketInfo.sessionOriginatorEngineId = M_THIS_ENGINE_ID;
                return true;
            }
            else {
                //remove from queue (only when this sender is completely done sending data)
                m_queueSendersNeedingFirstPassDataSent.pop();
            }
        }
        else {
            //remove from queue (if the session number no longer exists do to deletion)
            m_queueSendersNeedingFirstPassDataSent.pop();
        }
    }

    return false; //did not find any data to send this round
}

/*
    Transmission Request

   In order to request transmission of a block of client service data,
   the client service MUST provide the following parameters to LTP:

      Destination client service ID.

      Destination LTP engine ID.

      Client service data to send, as an array of bytes.

      Length of the data to be sent.

      Length of the red-part of the data.  This value MUST be in the
      range from zero to the total length of data to be sent.

   On reception of a valid transmission request from a client service,
   LTP proceeds as follows.
   */
void LtpEngine::TransmissionRequest(uint64_t destinationClientServiceId, uint64_t destinationLtpEngineId,
    LtpClientServiceDataToSend&& clientServiceDataToSend, std::shared_ptr<LtpTransmissionRequestUserData>&& userDataPtrToTake, uint64_t lengthOfRedPart)
{ //only called directly by unit test (not thread safe)
    if (m_memoryInFilesPtr) {
        
        const uint64_t memoryBlockId = m_memoryInFilesPtr->AllocateNewWriteMemoryBlock(clientServiceDataToSend.size());
        if (memoryBlockId == 0) {
            LOG_ERROR(subprocess) << "cannot allocate new memoryBlockId in LtpEngine::TransmissionRequest";
            return;
        }
        std::shared_ptr<LtpClientServiceDataToSend> clientServiceDataToSendSharedPtr = std::make_shared<LtpClientServiceDataToSend>(std::move(clientServiceDataToSend));
        
        MemoryInFiles::deferred_write_t deferredWrite;
        deferredWrite.memoryBlockId = memoryBlockId;
        deferredWrite.offset = 0;
        deferredWrite.writeFromThisLocationPtr = clientServiceDataToSendSharedPtr->data();
        deferredWrite.length = clientServiceDataToSendSharedPtr->size();

        if (!m_memoryInFilesPtr->WriteMemoryAsync(deferredWrite,
            boost::bind(&LtpEngine::OnTransmissionRequestDataWrittenToDisk, this,
                destinationClientServiceId, destinationLtpEngineId,
                std::move(clientServiceDataToSendSharedPtr), std::move(userDataPtrToTake),
                lengthOfRedPart, memoryBlockId)))
        {
            LOG_ERROR(subprocess) << "cannot start async write to disk for memoryBlockId="
                << deferredWrite.memoryBlockId
                << " length=" << deferredWrite.length
                << " offset=" << deferredWrite.offset
                << " ptr=" << deferredWrite.writeFromThisLocationPtr;
        }
    }
    else {
        DoTransmissionRequest(destinationClientServiceId, destinationLtpEngineId,
            std::move(clientServiceDataToSend), std::move(userDataPtrToTake),
            lengthOfRedPart, 0);
    }
}
void LtpEngine::OnTransmissionRequestDataWrittenToDisk(uint64_t destinationClientServiceId, uint64_t destinationLtpEngineId,
    std::shared_ptr<LtpClientServiceDataToSend>& clientServiceDataToSendPtrToTake, std::shared_ptr<LtpTransmissionRequestUserData>& userDataPtrToTake,
    uint64_t lengthOfRedPart, uint64_t memoryBlockId)
{
    clientServiceDataToSendPtrToTake->clear(false); //free memory, set data() to null, but leave size() unchanged
    std::vector<uint8_t> userDataForSuccessfulBundleSendCallback(std::move(clientServiceDataToSendPtrToTake->m_userData));
    DoTransmissionRequest(destinationClientServiceId, destinationLtpEngineId,
        std::move(*clientServiceDataToSendPtrToTake), std::move(userDataPtrToTake),
        lengthOfRedPart, memoryBlockId);
    //if ltp sender session is storing to disk, an OnSuccessfulBundleSendCallback_t shall be called
    // NOT when confirmation that red part was received by the receiver to close the session,
    // BUT RATHER when the session was fully written to disk and the SessionStart callback was called.
    // The OnSuccessfulBundleSendCallback_t (or OnFailed callbacks) are needed for Egress to send acks to storage or ingress in order
    // to free up ZMQ bundle pipeline.
    // If successCallbackCalled is true, then the OnFailed callbacks shall not ack ingress or storage to free up ZMQ pipeline,
    // but instead treat the returned movableBundle as a new bundle.
    if (m_mapSessionNumberToSessionSender.size() < M_DISK_BUNDLE_ACK_CALLBACK_LIMIT) {
        if (m_onSuccessfulBundleSendCallback) {
            m_onSuccessfulBundleSendCallback(userDataForSuccessfulBundleSendCallback, m_userAssignedUuid);
        }
    }
    else {
        m_userDataPendingSuccessfulBundleSendCallbackQueue.emplace(std::move(userDataForSuccessfulBundleSendCallback));
    }
}
void LtpEngine::DoTransmissionRequest(uint64_t destinationClientServiceId, uint64_t destinationLtpEngineId,
    LtpClientServiceDataToSend && clientServiceDataToSend, std::shared_ptr<LtpTransmissionRequestUserData> && userDataPtrToTake,
    uint64_t lengthOfRedPart, uint64_t memoryBlockId)
{ //only called directly by unit test (not thread safe)
    m_transmissionRequestServedAsPing = true;
    uint64_t randomSessionNumberGeneratedBySender;
    uint64_t randomInitialSenderCheckpointSerialNumber; //incremented by 1 for new
    if(M_FORCE_32_BIT_RANDOM_NUMBERS) {
        randomSessionNumberGeneratedBySender = m_rng.GetRandomSession32();
        randomInitialSenderCheckpointSerialNumber = m_rng.GetRandomSerialNumber32();
    }
    else {
        randomSessionNumberGeneratedBySender = m_rng.GetRandomSession64();
        randomInitialSenderCheckpointSerialNumber = m_rng.GetRandomSerialNumber64();
    }
    Ltp::session_id_t senderSessionId(M_THIS_ENGINE_ID, randomSessionNumberGeneratedBySender);
    std::pair<map_session_number_to_session_sender_t::iterator, bool> res = m_mapSessionNumberToSessionSender.emplace(
        std::piecewise_construct,
        std::forward_as_tuple(randomSessionNumberGeneratedBySender),
        std::forward_as_tuple(
            randomInitialSenderCheckpointSerialNumber, std::move(clientServiceDataToSend), std::move(userDataPtrToTake),
            lengthOfRedPart, senderSessionId, destinationClientServiceId, memoryBlockId,
            m_ltpSessionSenderCommonData //reference stored, so must not be destroyed until after all sessions destroyed
        )
    );
    if (res.second == false) { //session was not inserted
        LOG_ERROR(subprocess) << "new tx session cannot be inserted??";
        return;
    }

    m_queueSendersNeedingFirstPassDataSent.emplace(randomSessionNumberGeneratedBySender);

    if (m_sessionStartCallback) {
        //At the sender, the session start notice informs the client service of the initiation of the transmission session.
        m_sessionStartCallback(senderSessionId);
    }
    TrySaturateSendPacketPipeline(); //because added to m_queueSendersNeedingFirstPassDataSent
}
void LtpEngine::TransmissionRequest(uint64_t destinationClientServiceId, uint64_t destinationLtpEngineId,
    const uint8_t * clientServiceDataToCopyAndSend, uint64_t length, std::shared_ptr<LtpTransmissionRequestUserData> && userDataPtrToTake, uint64_t lengthOfRedPart)
{  //only called directly by unit test (not thread safe)
    TransmissionRequest(destinationClientServiceId, destinationLtpEngineId,
        std::move(std::vector<uint8_t>(clientServiceDataToCopyAndSend, clientServiceDataToCopyAndSend + length)),
        std::move(userDataPtrToTake), lengthOfRedPart
    );
}
void LtpEngine::TransmissionRequest(uint64_t destinationClientServiceId, uint64_t destinationLtpEngineId,
    const uint8_t * clientServiceDataToCopyAndSend, uint64_t length, uint64_t lengthOfRedPart)
{  //only called directly by unit test (not thread safe)
    TransmissionRequest(destinationClientServiceId, destinationLtpEngineId,
        std::move(std::vector<uint8_t>(clientServiceDataToCopyAndSend, clientServiceDataToCopyAndSend + length)),
        std::move(std::shared_ptr<LtpTransmissionRequestUserData>()), lengthOfRedPart
    );
}
void LtpEngine::TransmissionRequest(std::shared_ptr<transmission_request_t> & transmissionRequest) {
    TransmissionRequest(transmissionRequest->destinationClientServiceId, transmissionRequest->destinationLtpEngineId,
        std::move(transmissionRequest->clientServiceDataToSend),
        std::move(transmissionRequest->userDataPtr),
        transmissionRequest->lengthOfRedPart
    );
}
void LtpEngine::TransmissionRequest_ThreadSafe(std::shared_ptr<transmission_request_t> && transmissionRequest) {
    //The arguments to bind are copied or moved, and are never passed by reference unless wrapped in std::ref or std::cref.
    boost::asio::post(m_ioServiceLtpEngine, boost::bind(&LtpEngine::TransmissionRequest, this, std::move(transmissionRequest)));
}

void LtpEngine::CancellationRequest_ThreadSafe(const Ltp::session_id_t & sessionId) {
    //The arguments to bind are copied or moved, and are never passed by reference unless wrapped in std::ref or std::cref.
    boost::asio::post(m_ioServiceLtpEngine, boost::bind(&LtpEngine::CancellationRequest, this, sessionId));
}

//4.2.  Cancellation Request
//In order to request cancellation of a session, either as the sender
//or as the receiver of the associated data block, the client service
//must provide the session ID identifying the session to be canceled.
bool LtpEngine::CancellationRequest(const Ltp::session_id_t & sessionId) { //only called directly by unit test (not thread safe)

    //On reception of a valid cancellation request from a client service,
    //LTP proceeds as follows.

    //First, the internal "Cancel Session" procedure(Section 6.19) is
    //invoked.
    //6.19.  Cancel Session
    //Response: all segments of the affected session that are currently
    //queued for transmission can be deleted from the outbound traffic
    //queues.All countdown timers currently associated with the session
    //are deleted.Note : If the local LTP engine is the sender, then all
    //remaining data retransmission buffer space allocated to the session
    //can be released.
    
    
    
    if (M_THIS_ENGINE_ID == sessionId.sessionOriginatorEngineId) {
        //if the session is being canceled by the sender (i.e., the
        //session originator part of the session ID supplied in the
        //cancellation request is the local LTP engine ID):

        map_session_number_to_session_sender_t::iterator txSessionIt = m_mapSessionNumberToSessionSender.find(sessionId.sessionNumber);
        if (txSessionIt != m_mapSessionNumberToSessionSender.end()) { //found
            

            //-If none of the data segments previously queued for transmission
            //as part of this session have yet been de - queued and transmitted
            //-- i.e., if the destination engine cannot possibly be aware of
            //this session -- then the session is simply closed; the "Close
            //Session" procedure (Section 6.20) is invoked.

            //- Otherwise, a CS(cancel by block sender) segment with the
            //reason - code USR_CNCLD MUST be queued for transmission to the
            //destination LTP engine specified in the transmission request
            //that started this session.
            TryReturnTxSessionDataToUser(txSessionIt); //TODO should this be done here because this would have been cancelled by this Tx user
            EraseTxSession(txSessionIt);
            LOG_INFO(subprocess) << "LtpEngine::CancellationRequest deleted session sender session number " << sessionId.sessionNumber;

            //send Cancel Segment to receiver (GetNextPacketToSend() will create the packet and start the timer)
            ++m_totalCancelSegmentsStarted;
            m_queueCancelSegmentTimerInfo.emplace();
            cancel_segment_timer_info_t & info = m_queueCancelSegmentTimerInfo.back();
            info.sessionId = sessionId;
            info.retryCount = 1;
            info.isFromSender = true;
            info.reasonCode = CANCEL_SEGMENT_REASON_CODES::USER_CANCELLED;

            TrySaturateSendPacketPipeline();
            return true;
        }
        return false;
    }
    else { //not sender, try receiver
        map_session_id_to_session_receiver_t::iterator rxSessionIt = m_mapSessionIdToSessionReceiver.find(sessionId);
        if (rxSessionIt != m_mapSessionIdToSessionReceiver.end()) { //found rx Session

            //if the session is being canceled by the receiver:
            //-If there is no transmission queue - set bound for the sender
            //(possibly because the local LTP engine is running on a receive -
            //only device), then the session is simply closed; the "Close
            //Session" procedure (Section 6.20) is invoked.
            //- Otherwise, a CR(cancel by block receiver) segment with reason -
            //code USR_CNCLD MUST be queued for transmission to the block
            //sender.

            if (rxSessionIt->second.IsSafeToDelete()) {
                EraseRxSession(rxSessionIt);
                LOG_INFO(subprocess) << "LtpEngine::CancellationRequest deleted session receiver session number " << sessionId.sessionNumber;
            }
            else {
                LOG_INFO(subprocess) << "LtpEngine::CancellationRequest will delete session receiver session number " << sessionId.sessionNumber << " when safe to do so.";
                m_queueReceiversNeedingDeletedButUnsafeToDelete.emplace(sessionId); //postpone deletion until safe
            }

            //send Cancel Segment to sender (GetNextPacketToSend() will create the packet and start the timer)
            ++m_totalCancelSegmentsStarted;
            m_queueCancelSegmentTimerInfo.emplace();
            cancel_segment_timer_info_t & info = m_queueCancelSegmentTimerInfo.back();
            info.sessionId = sessionId;
            info.retryCount = 1;
            info.isFromSender = false;
            info.reasonCode = CANCEL_SEGMENT_REASON_CODES::USER_CANCELLED;
            


            TrySaturateSendPacketPipeline();
            return true;
        }
        return false;
    }
    
}

void LtpEngine::CancelSegmentReceivedCallback(const Ltp::session_id_t & sessionId, CANCEL_SEGMENT_REASON_CODES reasonCode, bool isFromSender,
    Ltp::ltp_extensions_t & headerExtensions, Ltp::ltp_extensions_t & trailerExtensions)
{
    if (isFromSender) { //to receiver
        map_session_id_to_session_receiver_t::iterator rxSessionIt = m_mapSessionIdToSessionReceiver.find(sessionId);
        if (rxSessionIt != m_mapSessionIdToSessionReceiver.end()) { //found
            //Github Issue #31: Multiple TX canceled callbacks are called if multiple cancel segments received
            if (rxSessionIt->second.m_calledCancelledCallback == false) {
                rxSessionIt->second.m_calledCancelledCallback = true;
                if (m_receptionSessionCancelledCallback) {
                    m_receptionSessionCancelledCallback(sessionId, reasonCode); //No subsequent delivery notices will be issued for this session.
                }
            }
            const bool isSafeToDelete = rxSessionIt->second.IsSafeToDelete();
            if (isSafeToDelete) {
                EraseRxSession(rxSessionIt);
            }
            else {
                m_queueReceiversNeedingDeletedButUnsafeToDelete.emplace(sessionId); //postpone deletion until safe
            }
            if (m_numRxSessionsCancelledBySender == 0) {
                LOG_INFO(subprocess) << "First time remote cancelled an Rx session (reason code "
                    << static_cast<unsigned int>(reasonCode) << ", session #" << sessionId.sessionNumber
                    << ((isSafeToDelete) ? " deleted" : " queued for deletion")
                    << ").  Future cancellation messages will now be suppressed.";
            }
            ++m_numRxSessionsCancelledBySender;
            //Send CAx after outer if-else statement

        }
        else { //not found
            //The LTP receiver might also receive a retransmitted CS segment at the
            //CLOSED state(either if the CAS segment previously transmitted was
            //lost or if the CS timer expired prematurely at the LTP sender).In
            //such a case, the CAS is scheduled for retransmission.

            //Send CAx after outer if-else statement
        }
    }
    else { //to sender
        map_session_number_to_session_sender_t::iterator txSessionIt = m_mapSessionNumberToSessionSender.find(sessionId.sessionNumber);
        if (txSessionIt != m_mapSessionNumberToSessionSender.end()) { //found
            //Github Issue #31: Multiple TX canceled callbacks are called if multiple cancel segments received
            if (txSessionIt->second.m_calledCancelledOrCompletedCallback == false) {
                txSessionIt->second.m_calledCancelledOrCompletedCallback = true;
                if (m_transmissionSessionCancelledCallback) {
                    m_transmissionSessionCancelledCallback(sessionId, reasonCode, txSessionIt->second.m_userDataPtr);
                }
            }
            TryReturnTxSessionDataToUser(txSessionIt); //TODO check reason codes
            EraseTxSession(txSessionIt);
            if (m_numTxSessionsCancelledByReceiver == 0) {
                LOG_INFO(subprocess) << "First time remote cancelled a Tx session (reason code "
                    << static_cast<unsigned int>(reasonCode) << ", session #" << sessionId.sessionNumber
                    << ").  Future cancellation messages will now be suppressed.";
            }
            ++m_numTxSessionsCancelledByReceiver;
            //Send CAx after outer if-else statement
        }
        else { //not found
            //Note that while at the CLOSED state:
            //If the session was canceled
            //by the receiver by issuing a CR segment, the receiver may retransmit
            //the CR segment(either prematurely or because the acknowledging CAR
            //segment got lost).In this case, the LTP sender retransmits the
            //acknowledging CAR segment and stays in the CLOSED state.
            //send CAx to receiver

            //Send CAx after outer if-else statement
        }
    }
    //send CAx to receiver or sender (whether or not session exists) (here because all data in session is erased)
    m_queueClosedSessionDataToSend.emplace();
    Ltp::GenerateCancelAcknowledgementSegmentLtpPacket(m_queueClosedSessionDataToSend.back().second,
        sessionId, isFromSender, NULL, NULL);
    m_queueClosedSessionDataToSend.back().first = sessionId.sessionOriginatorEngineId;
    TrySaturateSendPacketPipeline();
}

void LtpEngine::CancelAcknowledgementSegmentReceivedCallback(const Ltp::session_id_t& sessionId, bool isToSender,
    Ltp::ltp_extensions_t& headerExtensions, Ltp::ltp_extensions_t& trailerExtensions)
{
    if (isToSender) {
        if (sessionId.sessionOriginatorEngineId != M_THIS_ENGINE_ID) {
            LOG_ERROR(subprocess) << "CA received to sender: sessionId.sessionOriginatorEngineId (" << sessionId.sessionOriginatorEngineId << ") != M_THIS_ENGINE_ID (" << M_THIS_ENGINE_ID << ")";
            return;
        }
    }
    else {//to receiver
        if (sessionId.sessionOriginatorEngineId == M_THIS_ENGINE_ID) {
            //Github issue 26: Allow same-engine transfers
            //There are currently three places in LtpEngine.cpp that guard against
            //sessionId.sessionOriginatorEngineId == M_THIS_ENGINE_ID and early-return from processing segments.
            //There is no prohibition against sending segments to your own engine,
            //and for our purposes some transfers are made to the local BPA.
            //
            //If there is no technical reason to prohibit these, it would be helpful to remove these guards.
            //I did a quick experiment of just removing the three checks and things seem to go through properly.
#ifndef LTP_ENGINE_ALLOW_SAME_ENGINE_TRANSFERS
            LOG_ERROR(subprocess) << "CA received to receiver: sessionId.sessionOriginatorEngineId (" << sessionId.sessionOriginatorEngineId << ") == M_THIS_ENGINE_ID (" << M_THIS_ENGINE_ID << ")";
            return;
#endif
        }
    }

    
    //6.18.  Stop Cancel Timer
    //This procedure is triggered by the reception of a CAx segment.
    //
    //Response: the timer associated with the Cx segment is deleted, and
    //the session of which the segment is one token is closed, i.e., the
    //"Close Session" procedure(Section 6.20) is invoked.
    std::vector<uint8_t> userDataReturned;
    if (!m_timeManagerOfCancelSegments.DeleteTimer(sessionId, userDataReturned)) {
        LOG_INFO(subprocess) << "LtpEngine::CancelAcknowledgementSegmentReceivedCallback didn't delete timer";
    }
    else { //a non-redundant cancel ack was received successfully
        const bool isPing = (isToSender && LtpRandomNumberGenerator::IsPingSession(sessionId.sessionNumber, M_FORCE_32_BIT_RANDOM_NUMBERS));
        m_totalCancelSegmentsAcknowledged += (!isPing);
        m_totalPingsAcknowledged += isPing;
        if (isPing) {
            //sender ping is successful
            M_NEXT_PING_START_EXPIRY = boost::posix_time::microsec_clock::universal_time() + M_SENDER_PING_TIME;
            if (!m_senderLinkIsUpPhysically) {
                m_senderLinkIsUpPhysically = true;
                if (m_onOutductLinkStatusChangedCallback) { //let user know of link up event
                    m_onOutductLinkStatusChangedCallback(false, m_userAssignedUuid);
                }
            }
        }
        else if (!isToSender) {
            if (userDataReturned.size() == sizeof(cancel_segment_timer_info_t)) {
                const cancel_segment_timer_info_t* userDataPtr = reinterpret_cast<cancel_segment_timer_info_t*>(userDataReturned.data());
                if (userDataPtr->reasonCode == CANCEL_SEGMENT_REASON_CODES::UNREACHABLE) {
                    if (m_ltpSessionsWithWrongClientServiceId.erase(userDataPtr->sessionId)) {
                        LOG_INFO(subprocess) << "Received CAx for unreachable (due to wrong client service id)";
                    }
                }
                //this overload of DeleteTimer does not auto-recycle user data and must be manually invoked
                m_timeManagerOfCancelSegments.m_userDataRecycler.ReturnUserData(std::move(userDataReturned));
            }
        }
    }
}

void LtpEngine::CancelSegmentTimerExpiredCallback(Ltp::session_id_t cancelSegmentTimerSerialNumber, std::vector<uint8_t> & userData) {
    
    if (userData.size() != sizeof(cancel_segment_timer_info_t)) {
        LOG_ERROR(subprocess) << "LtpEngine::CancelSegmentTimerExpiredCallback: userData.size() != sizeof(info)";
        return;
    }
    cancel_segment_timer_info_t info(userData.data());
    const bool isPing = (info.isFromSender && LtpRandomNumberGenerator::IsPingSession(info.sessionId.sessionNumber, M_FORCE_32_BIT_RANDOM_NUMBERS));

    if (info.retryCount <= m_maxRetriesPerSerialNumber) {
        m_totalCancelSegmentSendRetries += (!isPing);
        m_totalPingRetries += isPing;
        //resend Cancel Segment to receiver (GetNextPacketToSend() will create the packet and start the timer)
        ++info.retryCount;
        m_queueCancelSegmentTimerInfo.emplace(info);

        TrySaturateSendPacketPipeline();
    }
    else {
        if (isPing) {
            //sender ping failed
            ++m_totalPingsFailedToSend;
            M_NEXT_PING_START_EXPIRY = boost::posix_time::microsec_clock::universal_time() + M_SENDER_PING_TIME;
            if (m_senderLinkIsUpPhysically) {
                m_senderLinkIsUpPhysically = false;
                if (m_onOutductLinkStatusChangedCallback) { //let user know of link down event
                    m_onOutductLinkStatusChangedCallback(true, m_userAssignedUuid);
                }
            }
        }
        else {
            if (m_totalCancelSegmentsFailedToSend == 0) {
                LOG_INFO(subprocess) << "This is the first cancel segment that failed to send!  This message will now be suppressed.";
            }
            ++m_totalCancelSegmentsFailedToSend;
        }
    }
    //userData shall be recycled automatically after this callback completes
}

void LtpEngine::NotifyEngineThatThisSenderNeedsDeletedCallback(const Ltp::session_id_t & sessionId, bool wasCancelled, CANCEL_SEGMENT_REASON_CODES reasonCode, std::shared_ptr<LtpTransmissionRequestUserData> & userDataPtr) {
    map_session_number_to_session_sender_t::iterator txSessionIt = m_mapSessionNumberToSessionSender.find(sessionId.sessionNumber);
    if (wasCancelled) {
        //send Cancel Segment to receiver (GetNextPacketToSend() will create the packet and start the timer)
        ++m_totalCancelSegmentsStarted;
        m_queueCancelSegmentTimerInfo.emplace();
        cancel_segment_timer_info_t & info = m_queueCancelSegmentTimerInfo.back();
        info.sessionId = sessionId;
        info.retryCount = 1;
        info.isFromSender = true;
        info.reasonCode = reasonCode;

        //Github Issue #31: Multiple TX canceled callbacks are called if multiple cancel segments received
        if (txSessionIt != m_mapSessionNumberToSessionSender.end()) { //found
            if (txSessionIt->second.m_calledCancelledOrCompletedCallback == false) {
                txSessionIt->second.m_calledCancelledOrCompletedCallback = true;
                if (m_transmissionSessionCancelledCallback) {
                    m_transmissionSessionCancelledCallback(sessionId, reasonCode, userDataPtr);
                }
            }
        }
    }
    else {
        //Github Issue #31: Multiple TX canceled callbacks are called if multiple cancel segments received
        if (txSessionIt != m_mapSessionNumberToSessionSender.end()) { //found
            if (txSessionIt->second.m_calledCancelledOrCompletedCallback == false) {
                txSessionIt->second.m_calledCancelledOrCompletedCallback = true;
                if (m_transmissionSessionCompletedCallback) {
                    m_transmissionSessionCompletedCallback(sessionId, userDataPtr);
                }
            }
        }
    }
    m_queueSendersNeedingDeleted.emplace(sessionId.sessionNumber);
    SignalReadyForSend_ThreadSafe(); //posts the TrySaturateSendPacketPipeline(); so this won't be deleted during execution
}

void LtpEngine::NotifyEngineThatThisSenderHasProducibleData(const uint64_t sessionNumber) {
    m_queueSendersNeedingTimeCriticalDataSent.emplace(sessionNumber);
    SignalReadyForSend_ThreadSafe(); //posts the TrySaturateSendPacketPipeline(); so this won't be deleted during execution
}

void LtpEngine::NotifyEngineThatThisReceiverNeedsDeletedCallback(const Ltp::session_id_t & sessionId, bool wasCancelled, CANCEL_SEGMENT_REASON_CODES reasonCode) {
    bool isSafeToDelete = true;
    if (wasCancelled) {
        //send Cancel Segment to sender (GetNextPacketToSend() will create the packet and start the timer)
        ++m_totalCancelSegmentsStarted;
        m_queueCancelSegmentTimerInfo.emplace();
        cancel_segment_timer_info_t & info = m_queueCancelSegmentTimerInfo.back();
        info.sessionId = sessionId;
        info.retryCount = 1;
        info.isFromSender = false;
        info.reasonCode = reasonCode;

        map_session_id_to_session_receiver_t::iterator rxSessionIt = m_mapSessionIdToSessionReceiver.find(sessionId);
        if (rxSessionIt != m_mapSessionIdToSessionReceiver.end()) { //found
            isSafeToDelete = rxSessionIt->second.IsSafeToDelete();
            //Github Issue #31: Multiple TX canceled callbacks are called if multiple cancel segments received
            if (rxSessionIt->second.m_calledCancelledCallback == false) {
                rxSessionIt->second.m_calledCancelledCallback = true;
                if (m_receptionSessionCancelledCallback) {
                    m_receptionSessionCancelledCallback(sessionId, reasonCode);
                }
            }
        }
    }
    else { //not cancelled

    }
    
    if (isSafeToDelete) {
        m_queueReceiversNeedingDeleted.emplace(sessionId); //if found to be unsafe to delete, it will automatically get moved to the unsafe queue
    }
    else {
        m_queueReceiversNeedingDeletedButUnsafeToDelete.emplace(sessionId);
    }
    SignalReadyForSend_ThreadSafe(); //posts the TrySaturateSendPacketPipeline(); so this won't be deleted during execution
}

void LtpEngine::NotifyEngineThatThisReceiversTimersHasProducibleData(const Ltp::session_id_t & sessionId) {
    m_queueReceiversNeedingDataSent.emplace(sessionId);
    SignalReadyForSend_ThreadSafe(); //posts the TrySaturateSendPacketPipeline(); so this won't be deleted during execution
}
void LtpEngine::NotifyEngineThatThisReceiverCompletedDeferredOperation() {
    //In the event that the receiver has slow disk write operations, the io_service queue won't exhaust system memory
    //but rather will cause the UDP packets circular buffer to overflow which will simply drop UDP packets (which will be resent later).
    //Therefore, the circular buffer should be sized appropriately for disk write latency, and the disk operations should
    //be faster than the UDP receive rate.
    //LOG_DEBUG(subprocess) << "LtpEngine::NotifyEngineThatThisReceiverCompletedDeferredOperation: ongoing operation completed";
    ++m_countPacketsThatCompletedOngoingOperations;
    PacketInFullyProcessedCallback(true);
}

void LtpEngine::InitialTransmissionCompletedCallback(const Ltp::session_id_t & sessionId, std::shared_ptr<LtpTransmissionRequestUserData> & userDataPtr) {
    if (m_initialTransmissionCompletedCallbackForUser) {
        m_initialTransmissionCompletedCallbackForUser(sessionId, userDataPtr);
    }
}

void LtpEngine::ReportAcknowledgementSegmentReceivedCallback(const Ltp::session_id_t & sessionId, uint64_t reportSerialNumberBeingAcknowledged,
    Ltp::ltp_extensions_t & headerExtensions, Ltp::ltp_extensions_t & trailerExtensions)
{
    if (sessionId.sessionOriginatorEngineId == M_THIS_ENGINE_ID) {
        //Github issue 26: Allow same-engine transfers
        //There are currently three places in LtpEngine.cpp that guard against
        //sessionId.sessionOriginatorEngineId == M_THIS_ENGINE_ID and early-return from processing segments.
        //There is no prohibition against sending segments to your own engine,
        //and for our purposes some transfers are made to the local BPA.
        //
        //If there is no technical reason to prohibit these, it would be helpful to remove these guards.
        //I did a quick experiment of just removing the three checks and things seem to go through properly.
#ifndef LTP_ENGINE_ALLOW_SAME_ENGINE_TRANSFERS
        LOG_ERROR(subprocess) << "RA received: sessionId.sessionOriginatorEngineId == M_THIS_ENGINE_ID";
        return;
#endif
    }
    map_session_id_to_session_receiver_t::iterator rxSessionIt = m_mapSessionIdToSessionReceiver.find(sessionId);
    if (rxSessionIt != m_mapSessionIdToSessionReceiver.end()) { //found
        rxSessionIt->second.ReportAcknowledgementSegmentReceivedCallback(reportSerialNumberBeingAcknowledged, headerExtensions, trailerExtensions);
    }
    TrySaturateSendPacketPipeline();
}

void LtpEngine::ReportSegmentReceivedCallback(const Ltp::session_id_t & sessionId, const Ltp::report_segment_t & reportSegment,
    Ltp::ltp_extensions_t & headerExtensions, Ltp::ltp_extensions_t & trailerExtensions)
{
    if (sessionId.sessionOriginatorEngineId != M_THIS_ENGINE_ID) {
        LOG_ERROR(subprocess) << "RS received: sessionId.sessionOriginatorEngineId(" << sessionId.sessionOriginatorEngineId << ")  != M_THIS_ENGINE_ID(" << M_THIS_ENGINE_ID << ")";
        return;
    }
    map_session_number_to_session_sender_t::iterator txSessionIt = m_mapSessionNumberToSessionSender.find(sessionId.sessionNumber);
    if (txSessionIt != m_mapSessionNumberToSessionSender.end()) { //found
        txSessionIt->second.ReportSegmentReceivedCallback(reportSegment, headerExtensions, trailerExtensions);
    }
    else { //not found
        //Note that while at the CLOSED state, the LTP sender might receive an
        //RS segment(if the last transmitted RA segment before session close
        //got lost or if the LTP receiver retransmitted the RS segment
        //prematurely), in which case it retransmits an acknowledging RA
        //segment and stays in the CLOSED state.
        m_queueClosedSessionDataToSend.emplace();
        Ltp::GenerateReportAcknowledgementSegmentLtpPacket(m_queueClosedSessionDataToSend.back().second,
            sessionId, reportSegment.reportSerialNumber, NULL, NULL);
        m_queueClosedSessionDataToSend.back().first = sessionId.sessionOriginatorEngineId;
    }
    TrySaturateSendPacketPipeline();
}

//SESSION START:
//At the receiver, this notice indicates the beginning of a
//new reception session, and is delivered upon arrival of the first
//data segment carrying a new session ID.
bool LtpEngine::DataSegmentReceivedCallback(uint8_t segmentTypeFlags, const Ltp::session_id_t & sessionId,
    Ltp::client_service_raw_data_t& clientServiceRawData, const Ltp::data_segment_metadata_t & dataSegmentMetadata,
    Ltp::ltp_extensions_t & headerExtensions, Ltp::ltp_extensions_t & trailerExtensions)
{
    bool operationIsOngoing = false;
    if (sessionId.sessionOriginatorEngineId == M_THIS_ENGINE_ID) {
        //Github issue 26: Allow same-engine transfers
        //There are currently three places in LtpEngine.cpp that guard against
        //sessionId.sessionOriginatorEngineId == M_THIS_ENGINE_ID and early-return from processing segments.
        //There is no prohibition against sending segments to your own engine,
        //and for our purposes some transfers are made to the local BPA.
        //
        //If there is no technical reason to prohibit these, it would be helpful to remove these guards.
        //I did a quick experiment of just removing the three checks and things seem to go through properly.
#ifndef LTP_ENGINE_ALLOW_SAME_ENGINE_TRANSFERS
        LOG_ERROR(subprocess) << "DS received: sessionId.sessionOriginatorEngineId(" << sessionId.sessionOriginatorEngineId << ") == M_THIS_ENGINE_ID(" << M_THIS_ENGINE_ID << ")";
        return operationIsOngoing;
#endif
    }


    //The LTP receiver begins at the CLOSED state and enters the Data
    //Segment Reception (DS_REC) state upon receiving the first data
    //segment.  If the client service ID referenced in the data segment was
    //non-existent, a Cx segment with reason-code UNREACH SHOULD be sent to
    //the LTP sender via the Cancellation sequence beginning with the CX
    //marker (second part of the diagram).
    if (dataSegmentMetadata.clientServiceId != m_ltpSessionReceiverCommonData.m_clientServiceId) {
        //send Cancel Segment to sender (GetNextPacketToSend() will create the packet and start the timer)
        if (m_ltpSessionsWithWrongClientServiceId.emplace(sessionId).second) { //only send one per sessionId
            ++m_totalCancelSegmentsStarted;
            m_queueCancelSegmentTimerInfo.emplace();
            cancel_segment_timer_info_t& info = m_queueCancelSegmentTimerInfo.back();
            info.sessionId = sessionId;
            info.retryCount = 1;
            info.isFromSender = false;
            info.reasonCode = CANCEL_SEGMENT_REASON_CODES::UNREACHABLE;
            LOG_ERROR(subprocess) << "invalid clientServiceId(" << dataSegmentMetadata.clientServiceId
                << "), expected clientServiceId=" << m_ltpSessionReceiverCommonData.m_clientServiceId << " for " << sessionId
                << " ..sending UNREACHABLE Cx segment to LTP sender.";
            if (m_receptionSessionCancelledCallback) {
                m_receptionSessionCancelledCallback(sessionId, CANCEL_SEGMENT_REASON_CODES::UNREACHABLE);
            }
            TrySaturateSendPacketPipeline();
        }
        return operationIsOngoing;
    }

    map_session_id_to_session_receiver_t::iterator rxSessionIt = m_mapSessionIdToSessionReceiver.find(sessionId);
    if (rxSessionIt == m_mapSessionIdToSessionReceiver.end()) { //not found.. new session started
        //first check if the session has been closed previously before recreating
        if (M_MAX_RX_DATA_SEGMENT_HISTORY_OR_ZERO_DISABLE) {
            std::map<uint64_t, LtpSessionRecreationPreventer>::iterator it = m_mapSessionOriginatorEngineIdToLtpSessionRecreationPreventer.find(sessionId.sessionOriginatorEngineId);
            if (it == m_mapSessionOriginatorEngineIdToLtpSessionRecreationPreventer.end()) {
                LOG_INFO(subprocess) << "create new LtpSessionRecreationPreventer for sessionOriginatorEngineId " << sessionId.sessionOriginatorEngineId << " with history size " << M_MAX_RX_DATA_SEGMENT_HISTORY_OR_ZERO_DISABLE;
                it = m_mapSessionOriginatorEngineIdToLtpSessionRecreationPreventer.emplace(sessionId.sessionOriginatorEngineId, M_MAX_RX_DATA_SEGMENT_HISTORY_OR_ZERO_DISABLE).first;
            }
            if (!it->second.AddSession(sessionId.sessionNumber)) {
                LOG_INFO(subprocess) << "preventing old session from being recreated for " << sessionId;
                return operationIsOngoing;
            }
        }
        const uint64_t randomNextReportSegmentReportSerialNumber = (M_FORCE_32_BIT_RANDOM_NUMBERS) ? m_rng.GetRandomSerialNumber32() : m_rng.GetRandomSerialNumber64(); //incremented by 1 for new
        std::pair<map_session_id_to_session_receiver_t::iterator, bool> res = m_mapSessionIdToSessionReceiver.emplace(
            std::piecewise_construct,
            std::forward_as_tuple(sessionId),
            std::forward_as_tuple(randomNextReportSegmentReportSerialNumber,
                sessionId,
                m_ltpSessionReceiverCommonData //reference stored, so must not be destroyed until after all sessions destroyed
            )
        );

        if (res.second == false) { //session was not inserted
            LOG_ERROR(subprocess) << "new rx session cannot be inserted??";
            return operationIsOngoing;
        }
        rxSessionIt = res.first;

        if (m_sessionStartCallback) {
            //At the receiver, this notice indicates the beginning of a new reception session, and is delivered upon arrival of the first data segment carrying a new session ID.
            m_sessionStartCallback(sessionId);
        }
    }
    operationIsOngoing = rxSessionIt->second.DataSegmentReceivedCallback(segmentTypeFlags,
        clientServiceRawData, dataSegmentMetadata, headerExtensions, trailerExtensions);
    TrySaturateSendPacketPipeline();
    return operationIsOngoing;
}

void LtpEngine::SetSessionStartCallback(const SessionStartCallback_t & callback) {
    m_sessionStartCallback = callback;
}
void LtpEngine::SetRedPartReceptionCallback(const RedPartReceptionCallback_t & callback) {
    m_redPartReceptionCallback = callback;
}
void LtpEngine::SetGreenPartSegmentArrivalCallback(const GreenPartSegmentArrivalCallback_t & callback) {
    m_greenPartSegmentArrivalCallback = callback;
}
void LtpEngine::SetReceptionSessionCancelledCallback(const ReceptionSessionCancelledCallback_t & callback) {
    m_receptionSessionCancelledCallback = callback;
}
void LtpEngine::SetTransmissionSessionCompletedCallback(const TransmissionSessionCompletedCallback_t & callback) {
    m_transmissionSessionCompletedCallback = callback;
}
void LtpEngine::SetInitialTransmissionCompletedCallback(const InitialTransmissionCompletedCallback_t & callback) {
    m_initialTransmissionCompletedCallbackForUser = callback;
}
void LtpEngine::SetTransmissionSessionCancelledCallback(const TransmissionSessionCancelledCallback_t & callback) {
    m_transmissionSessionCancelledCallback = callback;
}

void LtpEngine::SetOnFailedBundleVecSendCallback(const OnFailedBundleVecSendCallback_t& callback) {
    m_onFailedBundleVecSendCallback = callback;
}
void LtpEngine::SetOnFailedBundleZmqSendCallback(const OnFailedBundleZmqSendCallback_t& callback) {
    m_onFailedBundleZmqSendCallback = callback;
}
void LtpEngine::SetOnSuccessfulBundleSendCallback(const OnSuccessfulBundleSendCallback_t& callback) {
    m_onSuccessfulBundleSendCallback = callback;
}
void LtpEngine::SetOnOutductLinkStatusChangedCallback(const OnOutductLinkStatusChangedCallback_t& callback) {
    m_onOutductLinkStatusChangedCallback = callback;
}
void LtpEngine::SetUserAssignedUuid(uint64_t userAssignedUuid) {
    m_userAssignedUuid = userAssignedUuid;
}

std::size_t LtpEngine::NumActiveReceivers() const {
    return m_mapSessionIdToSessionReceiver.size();
}
std::size_t LtpEngine::NumActiveSenders() const {
    return m_mapSessionNumberToSessionSender.size();
}
uint64_t LtpEngine::GetMaxNumberOfSessionsInPipeline() const {
    return M_MAX_SESSIONS_IN_PIPELINE;
}

void LtpEngine::UpdateRate(const uint64_t maxSendRateBitsPerSecOrZeroToDisable) {
    m_maxSendRateBitsPerSecOrZeroToDisable = maxSendRateBitsPerSecOrZeroToDisable;
    if (maxSendRateBitsPerSecOrZeroToDisable) {
        const uint64_t rateBytesPerSecond = m_maxSendRateBitsPerSecOrZeroToDisable >> 3;
        m_tokenRateLimiter.SetRate(
            rateBytesPerSecond,
            boost::posix_time::seconds(1),
            static_tokenMaxLimitDurationWindow //token limit of rateBytesPerSecond / (1000ms/100ms) = rateBytesPerSecond / 10
        );
    }
}

void LtpEngine::UpdateRate_ThreadSafe(const uint64_t maxSendRateBitsPerSecOrZeroToDisable) {
    boost::asio::post(m_ioServiceLtpEngine, boost::bind(&LtpEngine::UpdateRate, this, maxSendRateBitsPerSecOrZeroToDisable));
}


//restarts the token refresh timer if it is not running from now
void LtpEngine::TryRestartTokenRefreshTimer() {
    if (!m_tokenRefreshTimerIsRunning) {
        const boost::posix_time::ptime nowPtime = boost::posix_time::microsec_clock::universal_time();
        if (m_lastTimeTokensWereRefreshed.is_neg_infinity()) {
            m_lastTimeTokensWereRefreshed = nowPtime;
        }
        m_tokenRefreshTimer.expires_at(nowPtime + static_tokenRefreshTimeDurationWindow);
        m_tokenRefreshTimer.async_wait(boost::bind(&LtpEngine::OnTokenRefresh_TimerExpired, this, boost::asio::placeholders::error));
        m_tokenRefreshTimerIsRunning = true;
    }
}
//restarts the token refresh timer if it is not running from the given ptime
void LtpEngine::TryRestartTokenRefreshTimer(const boost::posix_time::ptime & nowPtime) {
    if (!m_tokenRefreshTimerIsRunning) {
        if (m_lastTimeTokensWereRefreshed.is_neg_infinity()) {
            m_lastTimeTokensWereRefreshed = nowPtime;
        }
        m_tokenRefreshTimer.expires_at(nowPtime + static_tokenRefreshTimeDurationWindow);
        m_tokenRefreshTimer.async_wait(boost::bind(&LtpEngine::OnTokenRefresh_TimerExpired, this, boost::asio::placeholders::error));
        m_tokenRefreshTimerIsRunning = true;
    }
}

void LtpEngine::OnTokenRefresh_TimerExpired(const boost::system::error_code& e) {
    const boost::posix_time::ptime nowPtime = boost::posix_time::microsec_clock::universal_time();
    const boost::posix_time::time_duration diff = nowPtime - m_lastTimeTokensWereRefreshed;
    m_tokenRateLimiter.AddTime(diff);
    m_lastTimeTokensWereRefreshed = nowPtime;
    m_tokenRefreshTimerIsRunning = false;
    if (e != boost::asio::error::operation_aborted) {
        // Timer was not cancelled, take necessary action.
        
        //If more tokens can be added, restart the timer so more tokens will be added at the next timer expiration.
        //Otherwise, if full, don't restart the timer and the next send packet operation will start it.
        if (!m_tokenRateLimiter.HasFullBucketOfTokens()) {
            TryRestartTokenRefreshTimer(nowPtime); //do this first before TrySaturateSendPacketPipeline so nowPtime can be used
        }

        TrySaturateSendPacketPipeline();
    }
}

void LtpEngine::OnHousekeeping_TimerExpired(const boost::system::error_code& e) {
    m_nowTimeRef = boost::posix_time::microsec_clock::universal_time(); //will be used by LtpSessionReceiver to update m_lastSegmentReceivedTimestamp
    const boost::posix_time::ptime stagnantRxSessionTimeThreshold = m_nowTimeRef - m_stagnantRxSessionTime;
    if (e != boost::asio::error::operation_aborted) {
        // Timer was not cancelled, take necessary action.

        // Check for stagnant rx sessions and delete them
        //
        // -----------------
        // The current LTP engine and session keeping logic does not provide a mechanism
        // for a session to timeout except for red data checkpoint (and EORP, EOB) reports.
        // Unfortunately, the LTP spec (both IETF RFC 5326 and CCSDS 734.1-B-1) are silent
        // about correct engine behavior in the case of a non-checkpoint timeout situation.
        //
        // Two simple ways to produce this problem:
        //
        // Send an LTP segment type 0 (red non-checkpoint) with a new Session ID and arbitrary data.
        // Send an LTP segment type 4 (green non-EOB) with a new Session ID and arbitrary data.
        // In both cases the LTP engine will establish new session state and wait
        // for all red data (with associated reports and their timeouts) or all green data.
        // But if these never arrive, for whatever reason, the LTP engine will persist this session
        // state indefinitely and also never give an indication to the application that this has occurred.
        //
        // It would be very helpful to the application to have some kind of "no further data received for the session"
        // cancellation from the receiver, with its associated API callbacks for RX session cancellation.
        // The LTP specs don't reserve a cancel reason for this, but the existing code 0 "Client service canceled session" could be used.
        // I could implement this as a client-service-level timeout for green segments not for red segments,
        // as the API doesn't indicate individual red segments to the client.
        // This timeout would be something like "maximum time between receiving any segments associated
        // with a session before cancelling" with a timer reset any time any segment is received for a given RX Session ID.
        //
        //Because this is a housekeeping timeout, it could be considerably larger than the normal round-trip-delay computed timeouts
        //so it wouldn't conflict with having a simple logic that resets any time a segment is received for an RX session.
        //The point is to know when to give up on an RX session and discard unneeded state.
        uint64_t countStagnantRxSessions = 0;
        for (map_session_id_to_session_receiver_t::iterator rxSessionIt = m_mapSessionIdToSessionReceiver.begin(); rxSessionIt != m_mapSessionIdToSessionReceiver.end(); ++rxSessionIt) {
            if ((rxSessionIt->second.m_lastSegmentReceivedTimestamp <= stagnantRxSessionTimeThreshold) && (rxSessionIt->second.GetNumActiveTimers() == 0)) {
                ++countStagnantRxSessions;

                //erase session
                const Ltp::session_id_t & sessionId = rxSessionIt->first;
                if (rxSessionIt->second.IsSafeToDelete()) {
                    //if found to be unsafe to delete, it will automatically get moved to the unsafe queue
                    m_queueReceiversNeedingDeleted.emplace(sessionId); //don't want to invalidate iterator in for loop
                }
                else {
                    m_queueReceiversNeedingDeletedButUnsafeToDelete.emplace(sessionId);
                }
                if (m_numStagnantRxSessionsDeleted == 0) {
                    LOG_INFO(subprocess) << "First time housekeeping deleted a stagnant Rx session. (session #" << sessionId.sessionNumber
                        << ").  Future messages like this will now be suppressed.";
                }
                ++m_numStagnantRxSessionsDeleted;

                //send Cancel Segment to sender (GetNextPacketToSend() will create the packet and start the timer)
                ++m_totalCancelSegmentsStarted;
                m_queueCancelSegmentTimerInfo.emplace();
                cancel_segment_timer_info_t& info = m_queueCancelSegmentTimerInfo.back();
                info.sessionId = sessionId;
                info.retryCount = 1;
                info.isFromSender = false;
                info.reasonCode = CANCEL_SEGMENT_REASON_CODES::USER_CANCELLED;

                //Github Issue #31: Multiple TX canceled callbacks are called if multiple cancel segments received
                if (rxSessionIt->second.m_calledCancelledCallback == false) {
                    rxSessionIt->second.m_calledCancelledCallback = true;
                    if (m_receptionSessionCancelledCallback) {
                        m_receptionSessionCancelledCallback(sessionId, CANCEL_SEGMENT_REASON_CODES::USER_CANCELLED);
                    }
                }
            }
        }
        //dequeue all Cancel Segments to sender
        TrySaturateSendPacketPipeline();

        
        // Start ltp session sender pings during times of zero data segment activity.
        // An LTP ping is defined as a sender sending a cancel segment of a known non-existent session number to a receiver,
        // in which the receiver shall respond with a cancel ack in order to determine if the link is active.
        // A link down callback will be called if a cancel ack is not received after (RTT * maxRetriesPerSerialNumber).
        // This parameter should be set to zero for a receiver as there is currently no use case for a receiver to detect link-up.
        if (M_SENDER_PING_SECONDS_OR_ZERO_TO_DISABLE && (m_nowTimeRef >= M_NEXT_PING_START_EXPIRY)) {
            if (m_transmissionRequestServedAsPing) { //skip this ping
                M_NEXT_PING_START_EXPIRY = m_nowTimeRef + M_SENDER_PING_TIME;
                m_transmissionRequestServedAsPing = false;
            }
            else {
                M_NEXT_PING_START_EXPIRY = boost::posix_time::special_values::pos_infin; //will be set after ping succeeds or fails
                //send Cancel Segment to receiver (GetNextPacketToSend() will create the packet and start the timer)
                ++m_totalPingsStarted;
                m_queueCancelSegmentTimerInfo.emplace();
                cancel_segment_timer_info_t& info = m_queueCancelSegmentTimerInfo.back();
                info.sessionId.sessionOriginatorEngineId = M_THIS_ENGINE_ID;
                info.sessionId.sessionNumber = (M_FORCE_32_BIT_RANDOM_NUMBERS) ? m_rng.GetPingSession32() : m_rng.GetPingSession64();
                info.retryCount = 1;
                info.isFromSender = true;
                info.reasonCode = CANCEL_SEGMENT_REASON_CODES::USER_CANCELLED;

                TrySaturateSendPacketPipeline();
            }
        }
        
        //restart housekeeping timer
        m_housekeepingTimer.expires_at(m_nowTimeRef + M_HOUSEKEEPING_INTERVAL);
        m_housekeepingTimer.async_wait(boost::bind(&LtpEngine::OnHousekeeping_TimerExpired, this, boost::asio::placeholders::error));
    }
}

void LtpEngine::DoExternalLinkDownEvent() {
    if (m_senderLinkIsUpPhysically) {
        m_senderLinkIsUpPhysically = false;
        if (m_onOutductLinkStatusChangedCallback) { //let user know of link down event
            m_onOutductLinkStatusChangedCallback(true, m_userAssignedUuid);
        }
    }
}
void LtpEngine::PostExternalLinkDownEvent_ThreadSafe() {
    boost::asio::post(m_ioServiceLtpEngine, boost::bind(&LtpEngine::DoExternalLinkDownEvent, this));
}

void LtpEngine::SetDelays_ThreadSafe(const boost::posix_time::time_duration& oneWayLightTime, const boost::posix_time::time_duration& oneWayMarginTime, bool updateRunningTimers) {
    boost::asio::post(m_ioServiceLtpEngine, boost::bind(&LtpEngine::SetDelays, this, oneWayLightTime, oneWayMarginTime, updateRunningTimers));
}
void LtpEngine::SetDelays(const boost::posix_time::time_duration& oneWayLightTime, const boost::posix_time::time_duration& oneWayMarginTime, bool updateRunningTimers) {
    const boost::posix_time::time_duration oldTransmissionToAckReceivedTime = m_transmissionToAckReceivedTime;
    m_transmissionToAckReceivedTime = ((oneWayLightTime * 2) + (oneWayMarginTime * 2)); //this variable is referenced by all timers, so new timers will use this new value
    const boost::posix_time::time_duration diffNewMinusOld = m_transmissionToAckReceivedTime - oldTransmissionToAckReceivedTime;
    m_stagnantRxSessionTime = (m_transmissionToAckReceivedTime * static_cast<int>(m_maxRetriesPerSerialNumber + 1)); //update this housekeeping variable which was calculated based on RTT
    if (updateRunningTimers) {
        m_timeManagerOfReportSerialNumbers.AdjustRunningTimers(diffNewMinusOld);
        m_timeManagerOfCheckpointSerialNumbers.AdjustRunningTimers(diffNewMinusOld);
        m_timeManagerOfCancelSegments.AdjustRunningTimers(diffNewMinusOld);
    }
}

void LtpEngine::SetDeferDelays_ThreadSafe(const uint64_t delaySendingOfReportSegmentsTimeMsOrZeroToDisable, const uint64_t delaySendingOfDataSegmentsTimeMsOrZeroToDisable) {
    boost::asio::post(m_ioServiceLtpEngine, boost::bind(&LtpEngine::SetDeferDelays, this,
        delaySendingOfReportSegmentsTimeMsOrZeroToDisable, delaySendingOfDataSegmentsTimeMsOrZeroToDisable));
}

void LtpEngine::SetDeferDelays(const uint64_t delaySendingOfReportSegmentsTimeMsOrZeroToDisable, const uint64_t delaySendingOfDataSegmentsTimeMsOrZeroToDisable) {
    m_delaySendingOfReportSegmentsTime = (delaySendingOfReportSegmentsTimeMsOrZeroToDisable) ?
        boost::posix_time::milliseconds(delaySendingOfReportSegmentsTimeMsOrZeroToDisable) :
        boost::posix_time::time_duration(boost::posix_time::special_values::not_a_date_time);
    m_delaySendingOfDataSegmentsTime = (delaySendingOfDataSegmentsTimeMsOrZeroToDisable) ?
        boost::posix_time::milliseconds(delaySendingOfDataSegmentsTimeMsOrZeroToDisable) :
        boost::posix_time::time_duration(boost::posix_time::special_values::not_a_date_time);
}

void LtpEngine::TryReturnTxSessionDataToUser(map_session_number_to_session_sender_t::iterator& txSessionIt) {
    std::shared_ptr<LtpClientServiceDataToSend>& csdRef = txSessionIt->second.m_dataToSendSharedPtr;
    const bool safeToMove = (csdRef.use_count() == 1); //not also involved in a send operation
    const bool successCallbackAlreadyCalled = (m_memoryInFilesPtr) ? true : false;
    if (m_onFailedBundleVecSendCallback) { //if the user wants the data back
        std::vector<uint8_t>& vecRef = csdRef->GetVecRef();
        if (vecRef.size()) { //this session sender is using vector<uint8_t> client service data
            if (m_numTxSessionsReturnedToStorage == 0) {
                LOG_INFO(subprocess) << "First time LTP sender "
                    << ((safeToMove) ? "moving" : "copying")
                    << " a send-failed vector bundle back to the user (back to storage).  Future messages like this will now be suppressed.";
            }
            ++m_numTxSessionsReturnedToStorage;
            if (safeToMove) {
                m_onFailedBundleVecSendCallback(vecRef, csdRef->m_userData, m_userAssignedUuid, successCallbackAlreadyCalled);
            }
            else {
                std::vector<uint8_t> vecCopy(vecRef);
                m_onFailedBundleVecSendCallback(vecCopy, csdRef->m_userData, m_userAssignedUuid, successCallbackAlreadyCalled);
            }
        }
    }
    if (m_onFailedBundleZmqSendCallback) { //if the user wants the data back
        zmq::message_t& zmqRef = csdRef->GetZmqRef();
        if (zmqRef.size()) { //this session sender is using zmq client service data
            if (m_numTxSessionsReturnedToStorage == 0) {
                LOG_INFO(subprocess) << "First time LTP sender "
                    << ((safeToMove) ? "moving" : "copying")
                    << " a send-failed zmq bundle back to the user (back to storage).  Future messages like this will now be suppressed.";
            }
            ++m_numTxSessionsReturnedToStorage;
            if (safeToMove) {
                m_onFailedBundleZmqSendCallback(zmqRef, csdRef->m_userData, m_userAssignedUuid, successCallbackAlreadyCalled);
            }
            else {
                zmq::message_t zmqCopy(zmqRef.data(), zmqRef.size());
                m_onFailedBundleZmqSendCallback(zmqCopy, csdRef->m_userData, m_userAssignedUuid, successCallbackAlreadyCalled);
            }
        }
    }
}

void LtpEngine::EraseTxSession(map_session_number_to_session_sender_t::iterator& txSessionIt) {
    const LtpSessionSender& txSession = txSessionIt->second;
    if (m_memoryInFilesPtr) {
        m_memoryBlockIdsPendingDeletionQueue.emplace(txSession.MEMORY_BLOCK_ID);
    }
    if (txSession.m_isFailedSession) {
        m_totalRedDataBytesFailedToSend += txSession.M_LENGTH_OF_RED_PART;
    }
    else {
        m_totalRedDataBytesSuccessfullySent += txSession.M_LENGTH_OF_RED_PART;
    }
    m_mapSessionNumberToSessionSender.erase(txSessionIt);
    
}

void LtpEngine::EraseRxSession(map_session_id_to_session_receiver_t::iterator& rxSessionIt) {
    //const LtpSessionReceiver & rxSession = rxSessionIt->second;
    m_mapSessionIdToSessionReceiver.erase(rxSessionIt);
}

void LtpEngine::LtpSessionReceiverReportSegmentTimerExpiredCallback(void* classPtr, const Ltp::session_id_t& reportSerialNumberPlusSessionNumber, std::vector<uint8_t>& userData) {
    (reinterpret_cast<LtpSessionReceiver*>(classPtr))->LtpReportSegmentTimerExpiredCallback(reportSerialNumberPlusSessionNumber, userData);
}
void LtpEngine::LtpSessionReceiverDelaySendReportSegmentTimerExpiredCallback(void* classPtr, const Ltp::session_id_t& checkpointSerialNumberPlusSessionNumber, std::vector<uint8_t>& userData) {
    (reinterpret_cast<LtpSessionReceiver*>(classPtr))->LtpDelaySendReportSegmentTimerExpiredCallback(checkpointSerialNumberPlusSessionNumber, userData);
}
void LtpEngine::LtpSessionSenderCheckpointTimerExpiredCallback(void* classPtr, const Ltp::session_id_t& checkpointSerialNumberPlusSessionNumber, std::vector<uint8_t>& userData) {
    (reinterpret_cast<LtpSessionSender*>(classPtr))->LtpCheckpointTimerExpiredCallback(checkpointSerialNumberPlusSessionNumber, userData);
}
void LtpEngine::LtpSessionSenderDelaySendDataSegmentsTimerExpiredCallback(void* classPtr, const uint64_t& sessionNumber, std::vector<uint8_t>& userData) {
    (reinterpret_cast<LtpSessionSender*>(classPtr))->LtpDelaySendDataSegmentsTimerExpiredCallback(sessionNumber, userData);
}
