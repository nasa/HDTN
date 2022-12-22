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
#include <queue>
#include <set>
#include <map>
#include <boost/asio.hpp>
#include "LtpNoticesToClientService.h"
#include "LtpClientServiceDataToSend.h"
#include <boost/core/noncopyable.hpp>

typedef boost::function<void(const Ltp::session_id_t & sessionId, bool wasCancelled, CANCEL_SEGMENT_REASON_CODES reasonCode)> NotifyEngineThatThisReceiverNeedsDeletedCallback_t;

typedef boost::function<void(const Ltp::session_id_t & sessionId)> NotifyEngineThatThisReceiversTimersHasProducibleDataFunction_t;

class LtpSessionReceiver : private boost::noncopyable {
private:
    LtpSessionReceiver() = delete;

    LTP_LIB_NO_EXPORT void LtpDelaySendReportSegmentTimerExpiredCallback(const Ltp::session_id_t& checkpointSerialNumberPlusSessionNumber, std::vector<uint8_t>& userData);
    LTP_LIB_NO_EXPORT void LtpReportSegmentTimerExpiredCallback(const Ltp::session_id_t & reportSerialNumberPlusSessionNumber, std::vector<uint8_t> & userData);
    LTP_LIB_NO_EXPORT void HandleGenerateAndSendReportSegment(const uint64_t checkpointSerialNumber,
        const uint64_t lowerBound, const uint64_t upperBound, const bool checkpointIsResponseToReportSegment);
    LTP_LIB_NO_EXPORT void OnDataSegmentWrittenToDisk(std::shared_ptr<std::vector<uint8_t> >& clientServiceDataReceivedSharedPtr, bool isEndOfBlock);
    LTP_LIB_NO_EXPORT void OnRedDataRecoveredFromDisk(bool success, bool isEndOfBlock);
public:

    //The LtpEngine shall have one copy of this struct and pass it's reference to each LtpSessionReceiver
    struct LtpSessionReceiverCommonData : private boost::noncopyable {
        LtpSessionReceiverCommonData() = delete;
        LTP_LIB_EXPORT LtpSessionReceiverCommonData(
            const uint64_t clientServiceId,
            uint64_t maxReceptionClaims,
            uint64_t estimatedBytesToReceive,
            uint64_t maxRedRxBytes,
            uint32_t& maxRetriesPerSerialNumberRef,
            LtpTimerManager<Ltp::session_id_t, Ltp::hash_session_id_t>& timeManagerOfReportSerialNumbersRef,
            LtpTimerManager<Ltp::session_id_t, Ltp::hash_session_id_t>& timeManagerOfSendingDelayedReceptionReportsRef,
            const NotifyEngineThatThisReceiverNeedsDeletedCallback_t& notifyEngineThatThisReceiverNeedsDeletedCallbackRef,
            const NotifyEngineThatThisReceiversTimersHasProducibleDataFunction_t& notifyEngineThatThisReceiversTimersHasProducibleDataFunctionRef,
            const RedPartReceptionCallback_t& redPartReceptionCallbackRef,
            const GreenPartSegmentArrivalCallback_t& greenPartSegmentArrivalCallbackRef,
            std::unique_ptr<MemoryInFiles>& memoryInFilesPtrRef);

        const uint64_t m_clientServiceId;
        uint64_t m_maxReceptionClaims;
        uint64_t m_estimatedBytesToReceive;
        uint64_t m_maxRedRxBytes;
        uint32_t& m_maxRetriesPerSerialNumberRef;

        LtpTimerManager<Ltp::session_id_t, Ltp::hash_session_id_t>& m_timeManagerOfReportSerialNumbersRef;
        LtpTimerManager<Ltp::session_id_t, Ltp::hash_session_id_t>& m_timeManagerOfSendingDelayedReceptionReportsRef;
        const NotifyEngineThatThisReceiverNeedsDeletedCallback_t& m_notifyEngineThatThisReceiverNeedsDeletedCallbackRef;
        const NotifyEngineThatThisReceiversTimersHasProducibleDataFunction_t& m_notifyEngineThatThisReceiversTimersHasProducibleDataFunctionRef;
        const RedPartReceptionCallback_t& m_redPartReceptionCallbackRef;
        const GreenPartSegmentArrivalCallback_t& m_greenPartSegmentArrivalCallbackRef;
        std::unique_ptr<MemoryInFiles>& m_memoryInFilesPtrRef;

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
    
    
    LTP_LIB_EXPORT LtpSessionReceiver(uint64_t randomNextReportSegmentReportSerialNumber,
        const Ltp::session_id_t & sessionId,
        LtpSessionReceiverCommonData& ltpSessionReceiverCommonDataRef);

    LTP_LIB_EXPORT ~LtpSessionReceiver();
    LTP_LIB_EXPORT bool NextDataToSend(UdpSendPacketInfo& udpSendPacketInfo);
    
    LTP_LIB_EXPORT std::size_t GetNumActiveTimers() const; //stagnant rx session detection in ltp engine with periodic housekeeping timer
    LTP_LIB_EXPORT bool IsSafeToDelete() const noexcept;
    LTP_LIB_EXPORT void ReportAcknowledgementSegmentReceivedCallback(uint64_t reportSerialNumberBeingAcknowledged,
        Ltp::ltp_extensions_t & headerExtensions, Ltp::ltp_extensions_t & trailerExtensions);
    LTP_LIB_EXPORT void DataSegmentReceivedCallback(uint8_t segmentTypeFlags,
        std::vector<uint8_t> & clientServiceDataVec, const Ltp::data_segment_metadata_t & dataSegmentMetadata,
        Ltp::ltp_extensions_t & headerExtensions, Ltp::ltp_extensions_t & trailerExtensions);
private:
    std::set<LtpFragmentSet::data_fragment_t> m_receivedDataFragmentsSet;
    typedef std::map<uint64_t, Ltp::report_segment_t> report_segments_sent_map_t;
    report_segments_sent_map_t m_mapAllReportSegmentsSent;
    report_segments_sent_map_t::const_iterator m_itLastPrimaryReportSegmentSent; //std::map<uint64_t, Ltp::report_segment_t> m_mapPrimaryReportSegmentsSent;
    
    //std::set<LtpFragmentSet::data_fragment_t> m_receivedDataFragmentsThatSenderKnowsAboutSet;
    std::set<uint64_t> m_checkpointSerialNumbersReceivedSet;
    typedef std::pair<report_segments_sent_map_t::const_iterator, uint32_t> it_retrycount_pair_t; //pair<iterator from m_mapAllReportSegmentsSent, retryCount>
    std::queue<it_retrycount_pair_t> m_reportsToSendQueue;
    
    LtpTimerManager<Ltp::session_id_t, Ltp::hash_session_id_t>::LtpTimerExpiredCallback_t m_timerExpiredCallback;
    std::list<uint64_t> m_reportSerialNumberActiveTimersList;
    
    struct rsntimer_userdata_t {
        report_segments_sent_map_t::const_iterator itMapAllReportSegmentsSent;
        std::list<uint64_t>::iterator itReportSerialNumberActiveTimersList;
        uint32_t retryCount;
    };

    LtpTimerManager<Ltp::session_id_t, Ltp::hash_session_id_t>::LtpTimerExpiredCallback_t m_delayedReceptionReportTimerExpiredCallback;
    //(rsLowerBound, rsUpperBound) to (checkpointSerialNumberToWhichRsPertains, checkpointIsResponseToReportSegment) map
    typedef std::pair<uint64_t, bool> csn_issecondary_pair_t;
    typedef std::map<FragmentSet::data_fragment_no_overlap_allow_abut_t, csn_issecondary_pair_t> rs_pending_map_t;
    rs_pending_map_t m_mapReportSegmentsPendingGeneration;
    
    uint64_t m_nextReportSegmentReportSerialNumber;
    padded_vector_uint8_t m_dataReceivedRed;
    uint64_t m_dataReceivedRedSize; //cannot just use variable in m_dataReceivedRed.size() because may be using disk instead
    uint64_t m_memoryBlockIdReservedSize; //since its resize rounds up
    const Ltp::session_id_t M_SESSION_ID;
    uint64_t m_lengthOfRedPart;
    uint64_t m_lowestGreenOffsetReceived;
    uint64_t m_currentRedLength;
    LtpSessionReceiverCommonData& m_ltpSessionReceiverCommonDataRef;
    uint64_t m_memoryBlockId;
public:
    boost::posix_time::ptime m_lastSegmentReceivedTimestamp; //stagnant rx session detection in ltp engine with periodic housekeeping timer
private:
    uint32_t m_numActiveAsyncDiskOperations;
    bool m_didRedPartReceptionCallback;
    bool m_didNotifyForDeletion;
    bool m_receivedEobFromGreenOrRed;
public:
    bool m_calledCancelledCallback;
};

#endif // LTP_SESSION_RECEIVER_H

