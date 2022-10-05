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
#include <queue>
#include <set>
#include <map>
#include <boost/asio.hpp>
#include "LtpNoticesToClientService.h"

typedef boost::function<void(const Ltp::session_id_t & sessionId, bool wasCancelled, CANCEL_SEGMENT_REASON_CODES reasonCode)> NotifyEngineThatThisReceiverNeedsDeletedCallback_t;

typedef boost::function<void(const Ltp::session_id_t & sessionId)> NotifyEngineThatThisReceiversTimersHasProducibleDataFunction_t;

class LtpSessionReceiver {
private:
    LtpSessionReceiver();

    LTP_LIB_NO_EXPORT void LtpReportSegmentTimerExpiredCallback(const Ltp::session_id_t & reportSerialNumberPlusSessionNumber, std::vector<uint8_t> & userData);
public:
    
    
    LTP_LIB_EXPORT LtpSessionReceiver(uint64_t randomNextReportSegmentReportSerialNumber, const uint64_t MAX_RECEPTION_CLAIMS,
        const uint64_t ESTIMATED_BYTES_TO_RECEIVE, const uint64_t maxRedRxBytes,
        const Ltp::session_id_t & sessionId, const uint64_t clientServiceId,
        const boost::posix_time::time_duration & oneWayLightTime, const boost::posix_time::time_duration & oneWayMarginTime,
        LtpTimerManager<Ltp::session_id_t, Ltp::hash_session_id_t> & timeManagerOfReportSerialNumbersRef,
        const NotifyEngineThatThisReceiverNeedsDeletedCallback_t & notifyEngineThatThisReceiverNeedsDeletedCallback,
        const NotifyEngineThatThisReceiversTimersHasProducibleDataFunction_t & notifyEngineThatThisSendersTimersHasProducibleDataFunction,
        const uint32_t maxRetriesPerSerialNumber = 5);

    LTP_LIB_EXPORT ~LtpSessionReceiver();
    LTP_LIB_EXPORT bool NextDataToSend(std::vector<boost::asio::const_buffer> & constBufferVec, std::shared_ptr<std::vector<std::vector<uint8_t> > > & underlyingDataToDeleteOnSentCallback);
    
    
    LTP_LIB_EXPORT void ReportAcknowledgementSegmentReceivedCallback(uint64_t reportSerialNumberBeingAcknowledged,
        Ltp::ltp_extensions_t & headerExtensions, Ltp::ltp_extensions_t & trailerExtensions);
    LTP_LIB_EXPORT void DataSegmentReceivedCallback(uint8_t segmentTypeFlags,
        std::vector<uint8_t> & clientServiceDataVec, const Ltp::data_segment_metadata_t & dataSegmentMetadata,
        Ltp::ltp_extensions_t & headerExtensions, Ltp::ltp_extensions_t & trailerExtensions, const RedPartReceptionCallback_t & redPartReceptionCallback,
        const GreenPartSegmentArrivalCallback_t & greenPartSegmentArrivalCallback);
private:
    std::set<LtpFragmentSet::data_fragment_t> m_receivedDataFragmentsSet;
    std::map<uint64_t, Ltp::report_segment_t> m_mapAllReportSegmentsSent;
    std::map<uint64_t, Ltp::report_segment_t> m_mapPrimaryReportSegmentsSent;
    //std::set<LtpFragmentSet::data_fragment_t> m_receivedDataFragmentsThatSenderKnowsAboutSet;
    std::set<uint64_t> m_checkpointSerialNumbersReceivedSet;
    std::queue<std::pair<uint64_t, uint32_t> > m_reportSerialNumbersToSendQueue; //pair<reportSerialNumber, retryCount>
    
    LtpTimerManager<Ltp::session_id_t, Ltp::hash_session_id_t>::LtpTimerExpiredCallback_t m_timerExpiredCallback;
    LtpTimerManager<Ltp::session_id_t, Ltp::hash_session_id_t> & m_timeManagerOfReportSerialNumbersRef;
    std::set<uint64_t> m_reportSerialNumberActiveTimersSet;
    
    uint64_t m_nextReportSegmentReportSerialNumber;
    padded_vector_uint8_t m_dataReceivedRed;
    const uint64_t M_MAX_RECEPTION_CLAIMS;
    const uint64_t M_ESTIMATED_BYTES_TO_RECEIVE;
    const uint64_t M_MAX_RED_RX_BYTES;
    const Ltp::session_id_t M_SESSION_ID;
    const uint64_t M_CLIENT_SERVICE_ID;
    const uint32_t M_MAX_RETRIES_PER_SERIAL_NUMBER;
    uint64_t m_lengthOfRedPart;
    uint64_t m_lowestGreenOffsetReceived;
    uint64_t m_currentRedLength;
    bool m_didRedPartReceptionCallback;
    bool m_didNotifyForDeletion;
    bool m_receivedEobFromGreenOrRed;
    const NotifyEngineThatThisReceiverNeedsDeletedCallback_t m_notifyEngineThatThisReceiverNeedsDeletedCallback;
    const NotifyEngineThatThisReceiversTimersHasProducibleDataFunction_t m_notifyEngineThatThisSendersTimersHasProducibleDataFunction;

public:
    //stagnant rx session detection in ltp engine with periodic housekeeping timer
    boost::posix_time::ptime m_lastDataSegmentReceivedTimestamp;

    //stats
    uint64_t m_numReportSegmentTimerExpiredCallbacks;
    uint64_t m_numReportSegmentsUnableToBeIssued;
    uint64_t m_numReportSegmentsTooLargeAndNeedingSplit;
    uint64_t m_numReportSegmentsCreatedViaSplit;
};

#endif // LTP_SESSION_RECEIVER_H

