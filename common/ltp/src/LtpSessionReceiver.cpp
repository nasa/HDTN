/**
 * @file LtpSessionReceiver.cpp
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

#include "LtpSessionReceiver.h"
#include "Logger.h"
#include <inttypes.h>
#include <boost/bind/bind.hpp>

static constexpr hdtn::Logger::SubProcess subprocess = hdtn::Logger::SubProcess::none;

LtpSessionReceiver::LtpSessionReceiver(uint64_t randomNextReportSegmentReportSerialNumber, const uint64_t MAX_RECEPTION_CLAIMS,
    const uint64_t ESTIMATED_BYTES_TO_RECEIVE, const uint64_t maxRedRxBytes,
    const Ltp::session_id_t & sessionId, const uint64_t clientServiceId,
    const boost::posix_time::time_duration & oneWayLightTime, const boost::posix_time::time_duration & oneWayMarginTime,
    LtpTimerManager<Ltp::session_id_t, Ltp::hash_session_id_t> & timeManagerOfReportSerialNumbersRef,
    LtpTimerManager<Ltp::session_id_t, Ltp::hash_session_id_t> & timeManagerOfSendingDelayedReceptionReportsRef,
    const NotifyEngineThatThisReceiverNeedsDeletedCallback_t & notifyEngineThatThisReceiverNeedsDeletedCallback,
    const NotifyEngineThatThisReceiversTimersHasProducibleDataFunction_t & notifyEngineThatThisSendersTimersHasProducibleDataFunction,
    const uint32_t maxRetriesPerSerialNumber) :
    
    m_itLastPrimaryReportSegmentSent(m_mapAllReportSegmentsSent.end()),
    m_timeManagerOfReportSerialNumbersRef(timeManagerOfReportSerialNumbersRef),
    m_timeManagerOfSendingDelayedReceptionReportsRef(timeManagerOfSendingDelayedReceptionReportsRef),
    m_nextReportSegmentReportSerialNumber(randomNextReportSegmentReportSerialNumber),
    M_MAX_RECEPTION_CLAIMS(MAX_RECEPTION_CLAIMS),
    M_ESTIMATED_BYTES_TO_RECEIVE(ESTIMATED_BYTES_TO_RECEIVE),
    M_MAX_RED_RX_BYTES(maxRedRxBytes),
    M_SESSION_ID(sessionId),
    M_CLIENT_SERVICE_ID(clientServiceId),
    M_MAX_RETRIES_PER_SERIAL_NUMBER(maxRetriesPerSerialNumber),
    m_lengthOfRedPart(UINT64_MAX),
    m_lowestGreenOffsetReceived(UINT64_MAX),
    m_currentRedLength(0),
    m_didRedPartReceptionCallback(false),
    m_didNotifyForDeletion(false),
    m_receivedEobFromGreenOrRed(false),
    m_notifyEngineThatThisReceiverNeedsDeletedCallback(notifyEngineThatThisReceiverNeedsDeletedCallback),
    m_notifyEngineThatThisSendersTimersHasProducibleDataFunction(notifyEngineThatThisSendersTimersHasProducibleDataFunction),
    //m_lastDataSegmentReceivedTimestamp(boost::posix_time::special_values::) //initialization not required because LtpEngine calls DataSegmentReceivedCallback right after emplace
    m_calledCancelledCallback(false),
    m_numReportSegmentTimerExpiredCallbacks(0),
    m_numReportSegmentsUnableToBeIssued(0),
    m_numReportSegmentsTooLargeAndNeedingSplit(0),
    m_numReportSegmentsCreatedViaSplit(0),
    m_numGapsFilledByOutOfOrderDataSegments(0),
    m_numDelayedFullyClaimedPrimaryReportSegmentsSent(0),
    m_numDelayedFullyClaimedSecondaryReportSegmentsSent(0),
    m_numDelayedPartiallyClaimedPrimaryReportSegmentsSent(0),
    m_numDelayedPartiallyClaimedSecondaryReportSegmentsSent(0)
{
    m_timerExpiredCallback = boost::bind(&LtpSessionReceiver::LtpReportSegmentTimerExpiredCallback, this, boost::placeholders::_1, boost::placeholders::_2);
    m_delayedReceptionReportTimerExpiredCallback = boost::bind(&LtpSessionReceiver::LtpDelaySendReportSegmentTimerExpiredCallback, this, boost::placeholders::_1, boost::placeholders::_2);
    m_dataReceivedRed.reserve(ESTIMATED_BYTES_TO_RECEIVE);
}

LtpSessionReceiver::~LtpSessionReceiver() {
    //clean up this receiving session's active timers within the shared LtpTimerManager
    for (std::list<uint64_t>::const_iterator it = m_reportSerialNumberActiveTimersList.cbegin(); it != m_reportSerialNumberActiveTimersList.cend(); ++it) {
        const uint64_t rsn = *it;

        // within a session would normally be LtpTimerManager<uint64_t, std::hash<uint64_t> > m_timeManagerOfReportSerialNumbers;
        // but now sharing a single LtpTimerManager among all sessions, so use a
        // LtpTimerManager<Ltp::session_id_t, Ltp::hash_session_id_t> (which has hash map hashing function support)
        // such that: 
        //  sessionOriginatorEngineId = report serial number
        //  sessionNumber = the session number
        //  since this is a receiver, the real sessionOriginatorEngineId is constant among all receiving sessions and is not needed
        const Ltp::session_id_t reportSerialNumberPlusSessionNumber(rsn, M_SESSION_ID.sessionNumber);

        if (!m_timeManagerOfReportSerialNumbersRef.DeleteTimer(reportSerialNumberPlusSessionNumber)) {
            LOG_ERROR(subprocess) << "LtpSessionReceiver::~LtpSessionReceiver: did not delete timer";
        }
    }
    for (rs_pending_map_t::const_iterator it = m_mapReportSegmentsPendingGeneration.cbegin(); it != m_mapReportSegmentsPendingGeneration.cend(); ++it) {
        const csn_issecondary_pair_t& p = it->second;
        const uint64_t csn = p.first;

        //  sessionOriginatorEngineId = CHECKPOINT serial number to which RS pertains
        //  sessionNumber = the session number
        //  since this is a receiver, the real sessionOriginatorEngineId is constant among all receiving sessions and is not needed
        const Ltp::session_id_t checkpointSerialNumberPlusSessionNumber(csn, M_SESSION_ID.sessionNumber);

        if (!m_timeManagerOfSendingDelayedReceptionReportsRef.DeleteTimer(checkpointSerialNumberPlusSessionNumber)) {
            LOG_ERROR(subprocess) << "LtpSessionReceiver::~LtpSessionReceiver: did not delete timer in m_timeManagerOfSendingDelayedReceptionReportsRef";
        }
    }
}

std::size_t LtpSessionReceiver::GetNumActiveTimers() const {
    return m_reportSerialNumberActiveTimersList.size() + m_mapReportSegmentsPendingGeneration.size();
}

void LtpSessionReceiver::LtpDelaySendReportSegmentTimerExpiredCallback(const Ltp::session_id_t& checkpointSerialNumberPlusSessionNumber, std::vector<uint8_t>& userData) {
    //  sessionOriginatorEngineId = CHECKPOINT serial number to which RS pertains
    //  sessionNumber = the session number
    //  since this is a receiver, the real sessionOriginatorEngineId is constant among all receiving sessions and is not needed
    const uint64_t checkpointSerialNumber = checkpointSerialNumberPlusSessionNumber.sessionOriginatorEngineId;
    rs_pending_map_t::iterator* itRsPendingPtr = (rs_pending_map_t::iterator*) userData.data();
    rs_pending_map_t::iterator& it = *itRsPendingPtr;

    const uint64_t thisRsLowerBound = it->first.beginIndex;
    const uint64_t thisRsUpperBound = it->first.endIndex + 1;
    const csn_issecondary_pair_t& p = it->second;
    const uint64_t thisRsCheckpointSerialNumber = p.first;
    const bool thisCheckpointIsResponseToReportSegment = p.second;
    m_numDelayedPartiallyClaimedPrimaryReportSegmentsSent += (!thisCheckpointIsResponseToReportSegment);
    m_numDelayedPartiallyClaimedSecondaryReportSegmentsSent += thisCheckpointIsResponseToReportSegment;
    HandleGenerateAndSendReportSegment(thisRsCheckpointSerialNumber, thisRsLowerBound, thisRsUpperBound, thisCheckpointIsResponseToReportSegment);
    m_mapReportSegmentsPendingGeneration.erase(it);
}

void LtpSessionReceiver::LtpReportSegmentTimerExpiredCallback(const Ltp::session_id_t & reportSerialNumberPlusSessionNumber, std::vector<uint8_t> & userData) {
    // within a session would normally be LtpTimerManager<uint64_t, std::hash<uint64_t> > m_timeManagerOfReportSerialNumbers;
    // but now sharing a single LtpTimerManager among all sessions, so use a
    // LtpTimerManager<Ltp::session_id_t, Ltp::hash_session_id_t> (which has hash map hashing function support)
    // such that: 
    //  sessionOriginatorEngineId = REPORT serial number
    //  sessionNumber = the session number
    //  since this is a receiver, the real sessionOriginatorEngineId is constant among all receiving sessions and is not needed
    const uint64_t reportSerialNumber = reportSerialNumberPlusSessionNumber.sessionOriginatorEngineId;

    if (userData.size() != sizeof(rsntimer_userdata_t)) {
        LOG_ERROR(subprocess) << "LtpSessionReceiver::LtpReportSegmentTimerExpiredCallback: userData.size() != sizeof(rsntimer_userdata_t)";
        return;
    }
    const rsntimer_userdata_t* userDataPtr = reinterpret_cast<rsntimer_userdata_t*>(userData.data());

    //keep track of this receiving session's active timers within the shared LtpTimerManager
    m_reportSerialNumberActiveTimersList.erase(userDataPtr->itReportSerialNumberActiveTimersList);

    
    //6.8.  Retransmit RS
    //
    //This procedure is triggered by either (a) the expiration of a
    //countdown timer associated with an RS segment or (b) the reception of
    //a CP segment for which one or more RS segments were previously issued
    //-- a redundantly retransmitted checkpoint.
    //
    //Response: if the number of times any affected RS segment has been
    //queued for transmission exceeds the report retransmission limit
    //established for the local LTP engine by network management, then the
    //session of which the segment is one token is canceled: the "Cancel
    //Session" procedure (Section 6.19) is invoked, a CR segment with
    //reason-code RLEXC is queued for transmission to the LTP engine that
    //originated the session, and a reception-session cancellation notice
    //(Section 7.6) is sent to the client service identified in each of the
    //data segments received in this session.
    //
    //Otherwise, a new copy of each affected RS segment is queued for
    //transmission to the LTP engine that originated the session.
    ++m_numReportSegmentTimerExpiredCallbacks;
    

    if (userDataPtr->retryCount <= M_MAX_RETRIES_PER_SERIAL_NUMBER) {
        //resend
        m_reportsToSendQueue.emplace(userDataPtr->itMapAllReportSegmentsSent, userDataPtr->retryCount + 1); //initial retryCount of 1
        m_notifyEngineThatThisSendersTimersHasProducibleDataFunction(M_SESSION_ID);
    }
    else {
        if (!m_didNotifyForDeletion) {
            m_didNotifyForDeletion = true;
            m_notifyEngineThatThisReceiverNeedsDeletedCallback(M_SESSION_ID, true, CANCEL_SEGMENT_REASON_CODES::RLEXC);
        }
    }
}

bool LtpSessionReceiver::NextDataToSend(std::vector<boost::asio::const_buffer> & constBufferVec, std::shared_ptr<std::vector<std::vector<uint8_t> > > & underlyingDataToDeleteOnSentCallback) {

    if (!m_reportsToSendQueue.empty()) {
        const it_retrycount_pair_t & p = m_reportsToSendQueue.front();
        const report_segments_sent_map_t::const_iterator & reportSegmentIt = p.first;
        const uint64_t rsn = reportSegmentIt->first;
        const uint32_t retryCount = p.second;
        //std::map<uint64_t, Ltp::report_segment_t>::iterator reportSegmentIt = m_mapAllReportSegmentsSent.find(rsn);
        //if (reportSegmentIt != m_mapAllReportSegmentsSent.end()) { //found
        underlyingDataToDeleteOnSentCallback = std::make_shared<std::vector<std::vector<uint8_t> > >(1); //2 would be needed in case of trailer extensions (but not used here)
        Ltp::GenerateReportSegmentLtpPacket((*underlyingDataToDeleteOnSentCallback)[0],
            M_SESSION_ID, reportSegmentIt->second, NULL, NULL);
        constBufferVec.resize(1);
        constBufferVec[0] = boost::asio::buffer((*underlyingDataToDeleteOnSentCallback)[0]);
        m_reportsToSendQueue.pop();
            
        // within a session would normally be LtpTimerManager<uint64_t, std::hash<uint64_t> > m_timeManagerOfReportSerialNumbers;
        // but now sharing a single LtpTimerManager among all sessions, so use a
        // LtpTimerManager<Ltp::session_id_t, Ltp::hash_session_id_t> (which has hash map hashing function support)
        // such that: 
        //  sessionOriginatorEngineId = report serial number
        //  sessionNumber = the session number
        //  since this is a receiver, the real sessionOriginatorEngineId is constant among all receiving sessions and is not needed
        const Ltp::session_id_t reportSerialNumberPlusSessionNumber(rsn, M_SESSION_ID.sessionNumber);

        std::vector<uint8_t> userData(sizeof(rsntimer_userdata_t));
        rsntimer_userdata_t* userDataPtr = reinterpret_cast<rsntimer_userdata_t*>(userData.data());
        userDataPtr->itMapAllReportSegmentsSent = reportSegmentIt;
        m_reportSerialNumberActiveTimersList.emplace_front(rsn); //keep track of this receiving session's active timers within the shared LtpTimerManager
        userDataPtr->itReportSerialNumberActiveTimersList = m_reportSerialNumberActiveTimersList.begin();
        userDataPtr->retryCount = retryCount;
        if (!m_timeManagerOfReportSerialNumbersRef.StartTimer(reportSerialNumberPlusSessionNumber, &m_timerExpiredCallback, std::move(userData))) {
            m_reportSerialNumberActiveTimersList.erase(m_reportSerialNumberActiveTimersList.begin());
            LOG_ERROR(subprocess) << "LtpSessionReceiver::NextDataToSend: did not start timer";
        }
        return true;
    }

    return false;
}


void LtpSessionReceiver::ReportAcknowledgementSegmentReceivedCallback(uint64_t reportSerialNumberBeingAcknowledged,
    Ltp::ltp_extensions_t & headerExtensions, Ltp::ltp_extensions_t & trailerExtensions)
{
    m_lastSegmentReceivedTimestamp = boost::posix_time::microsec_clock::universal_time();

    //6.14.  Stop RS Timer
    //
    //This procedure is triggered by the reception of an RA.
    //
    //Response: the countdown timer associated with the original RS segment
    //(identified by the report serial number of the RA segment) is
    //deleted.If no other countdown timers associated with RS segments
    //exist for this session, then the session is closed : the "Close
    //Session" procedure (Section 6.20) is invoked.

    // within a session would normally be LtpTimerManager<uint64_t, std::hash<uint64_t> > m_timeManagerOfReportSerialNumbers;
    // but now sharing a single LtpTimerManager among all sessions, so use a
    // LtpTimerManager<Ltp::session_id_t, Ltp::hash_session_id_t> (which has hash map hashing function support)
    // such that: 
    //  sessionOriginatorEngineId = report serial number
    //  sessionNumber = the session number
    //  since this is a receiver, the real sessionOriginatorEngineId is constant among all receiving sessions and is not needed
    const Ltp::session_id_t reportSerialNumberPlusSessionNumber(reportSerialNumberBeingAcknowledged, M_SESSION_ID.sessionNumber);

    std::vector<uint8_t> userDataReturned;
    if (m_timeManagerOfReportSerialNumbersRef.DeleteTimer(reportSerialNumberPlusSessionNumber, userDataReturned)) { //if delete of a timer was successful
        if (userDataReturned.size() != sizeof(rsntimer_userdata_t)) {
            LOG_ERROR(subprocess) << "LtpSessionReceiver::ReportAcknowledgementSegmentReceivedCallback: userDataReturned.size() != sizeof(rsntimer_userdata_t)";
        }
        else {
            const rsntimer_userdata_t* userDataPtr = reinterpret_cast<rsntimer_userdata_t*>(userDataReturned.data());
            //keep track of this receiving session's active timers within the shared LtpTimerManager
            m_reportSerialNumberActiveTimersList.erase(userDataPtr->itReportSerialNumberActiveTimersList);
        }
    }
    if (m_reportsToSendQueue.empty() && m_reportSerialNumberActiveTimersList.empty()) { // cannot do within a shared timer: m_timeManagerOfReportSerialNumbers.Empty()) {
        //TODO.. NOT SURE WHAT TO DO WHEN GREEN EOB LOST
        if (m_receivedEobFromGreenOrRed && m_didRedPartReceptionCallback) {
            if (!m_didNotifyForDeletion) {
                m_didNotifyForDeletion = true;
                m_notifyEngineThatThisReceiverNeedsDeletedCallback(M_SESSION_ID, false, CANCEL_SEGMENT_REASON_CODES::RESERVED); //close session (not cancelled)
            }
        }
    }
    
}


void LtpSessionReceiver::DataSegmentReceivedCallback(uint8_t segmentTypeFlags,
    std::vector<uint8_t> & clientServiceDataVec, const Ltp::data_segment_metadata_t & dataSegmentMetadata,
    Ltp::ltp_extensions_t & headerExtensions, Ltp::ltp_extensions_t & trailerExtensions, const RedPartReceptionCallback_t & redPartReceptionCallback,
    const GreenPartSegmentArrivalCallback_t & greenPartSegmentArrivalCallback)
{
    m_lastSegmentReceivedTimestamp = boost::posix_time::microsec_clock::universal_time();

    const uint64_t offsetPlusLength = dataSegmentMetadata.offset + dataSegmentMetadata.length;
    
    if (dataSegmentMetadata.length != clientServiceDataVec.size()) {
        LOG_ERROR(subprocess) << "dataSegmentMetadata.length != clientServiceDataVec.size()";
    }
    

    

    const bool isRedData = (segmentTypeFlags <= 3);
    const bool isEndOfBlock = ((segmentTypeFlags & 3) == 3);
    if (isEndOfBlock) {
        m_receivedEobFromGreenOrRed = true;
    }
    if (isRedData) {
        m_currentRedLength = std::max(offsetPlusLength, m_currentRedLength);
        //6.21.  Handle Miscolored Segment
        //This procedure is triggered by the arrival of either(a) a red - part
        //data segment whose block offset begins at an offset higher than the
        //block offset of any green - part data segment previously received for
        //the same session
        //
        //The arrival of a segment
        //matching either of the above checks is a violation of the protocol
        //requirement of having all red - part data as the block prefix and all
        //green - part data as the block suffix.
        //
        //Response: the received data segment is simply discarded.
        //
        //The Cancel Session procedure(Section 6.19) is invoked and a CR
        //segment with reason - code MISCOLORED SHOULD be enqueued for
        //transmission to the data sender.
        if (m_currentRedLength > m_lowestGreenOffsetReceived) {
            if (!m_didNotifyForDeletion) {
                m_didNotifyForDeletion = true;
                m_notifyEngineThatThisReceiverNeedsDeletedCallback(M_SESSION_ID, true, CANCEL_SEGMENT_REASON_CODES::MISCOLORED); //close session (cancelled)
            }
            return;
        }

        if (m_didRedPartReceptionCallback) {
            return;
        }
        if (m_currentRedLength > M_MAX_RED_RX_BYTES) {
            LOG_ERROR(subprocess) << "LtpSessionReceiver::DataSegmentReceivedCallback: current red data length ("
                << m_currentRedLength << " bytes) exceeds maximum of " << M_MAX_RED_RX_BYTES << " bytes";
            if (!m_didNotifyForDeletion) {
                m_didNotifyForDeletion = true;
                m_notifyEngineThatThisReceiverNeedsDeletedCallback(M_SESSION_ID, true, CANCEL_SEGMENT_REASON_CODES::SYSTEM_CANCELLED); //close session (cancelled)
            }
            return;
        }
        const bool neededResize = (m_dataReceivedRed.size() < offsetPlusLength);
        if (neededResize) {
            m_dataReceivedRed.resize(offsetPlusLength);
        }
        const bool dataReceivedWasNew = LtpFragmentSet::InsertFragment(m_receivedDataFragmentsSet, 
            LtpFragmentSet::data_fragment_t(dataSegmentMetadata.offset, offsetPlusLength - 1));
        if (dataReceivedWasNew) {
            memcpy(m_dataReceivedRed.data() + dataSegmentMetadata.offset, clientServiceDataVec.data(), dataSegmentMetadata.length);
        }
        bool rsWasJustNowSentWithFullRedBounds = false;
        const bool gapsWereFilled = (dataReceivedWasNew && (!neededResize));
        if (gapsWereFilled) {
            // Github issue #22 Defer synchronous reception report with out-of-order data segments (see below for full description)
            // The delay time should reset upon any data segments which fill gaps.
            
            // If the report segment bounds are fully claimed (i.e. no gaps) then the report can be sent immediately.
            rs_pending_map_t::iterator it = //search by lower bound
                m_mapReportSegmentsPendingGeneration.find(LtpFragmentSet::data_fragment_no_overlap_allow_abut_t(dataSegmentMetadata.offset, dataSegmentMetadata.offset));
            if (it != m_mapReportSegmentsPendingGeneration.end()) { //found by lower bound
                ++m_numGapsFilledByOutOfOrderDataSegments;
                if (LtpFragmentSet::ContainsFragmentEntirely(m_receivedDataFragmentsSet, LtpFragmentSet::data_fragment_t(it->first.beginIndex, it->first.endIndex))) { //fully claimed (no gaps)
                    
                    const uint64_t thisRsLowerBound = it->first.beginIndex;
                    const uint64_t thisRsUpperBound = it->first.endIndex + 1;
                    const csn_issecondary_pair_t& p = it->second;
                    const uint64_t thisRsCheckpointSerialNumber = p.first;
                    const bool thisCheckpointIsResponseToReportSegment = p.second;
                    m_numDelayedFullyClaimedPrimaryReportSegmentsSent += (!thisCheckpointIsResponseToReportSegment);
                    m_numDelayedFullyClaimedSecondaryReportSegmentsSent += thisCheckpointIsResponseToReportSegment;
                    HandleGenerateAndSendReportSegment(thisRsCheckpointSerialNumber, thisRsLowerBound, thisRsUpperBound, thisCheckpointIsResponseToReportSegment);
                    if ((thisRsLowerBound == 0) && (thisRsUpperBound == m_lengthOfRedPart)) {
                        rsWasJustNowSentWithFullRedBounds = true;
                    }
                    //  sessionOriginatorEngineId = CHECKPOINT serial number to which RS pertains
                    //  sessionNumber = the session number
                    //  since this is a receiver, the real sessionOriginatorEngineId is constant among all receiving sessions and is not needed
                    const Ltp::session_id_t checkpointSerialNumberPlusSessionNumber(thisRsCheckpointSerialNumber, M_SESSION_ID.sessionNumber);

                    if (!m_timeManagerOfSendingDelayedReceptionReportsRef.DeleteTimer(checkpointSerialNumberPlusSessionNumber)) {
                        LOG_ERROR(subprocess) << "LtpSessionReceiver::DataSegmentReceivedCallback: did not delete timer in m_timeManagerOfSendingDelayedReceptionReportsRef";
                    }
                    m_mapReportSegmentsPendingGeneration.erase(it);
                }
            }
        }

        const bool isRedCheckpoint = (segmentTypeFlags != 0);
        const bool isEndOfRedPart = (segmentTypeFlags & 2);
        
        //LtpFragmentSet::PrintFragmentSet(m_receivedDataFragmentsSet);
        if (isEndOfRedPart) {
            m_lengthOfRedPart = offsetPlusLength;
        }
        if (isRedCheckpoint) {
            if ((dataSegmentMetadata.checkpointSerialNumber == NULL) || (dataSegmentMetadata.reportSerialNumber == NULL)) {
                LOG_ERROR(subprocess) << "LtpSessionReceiver::DataSegmentReceivedCallback: checkpoint but NULL values";
                return;
            }
            //6.11.  Send Reception Report
            //This procedure is triggered by either (a) the original reception of a
            //CP segment(the checkpoint serial number identifying this CP is new)
            if (m_checkpointSerialNumbersReceivedSet.insert(*dataSegmentMetadata.checkpointSerialNumber).second == false) { //serial number was not inserted (already exists)
                return; //no work to do.. ignore this redundant checkpoint
            }

            //data segment Report serial number (SDNV)
            //If the checkpoint was queued for transmission in response to the
            //reception of an RS(Section 6.13), then its value MUST be the
            //report serial number value of the RS that caused the data segment
            //to be queued for transmission.
            //Otherwise, the value of report serial number MUST be zero.
            const bool checkpointIsResponseToReportSegment = ((*dataSegmentMetadata.reportSerialNumber) != 0);

            //a reception report is issued as follows.
            //If production of the reception report was triggered by reception of a checkpoint :
            // - The upper bound of the report SHOULD be the upper bound (the sum of the offset and length) of the checkpoint data segment, to minimize unnecessary retransmission.
            const uint64_t upperBound = offsetPlusLength;

            uint64_t lowerBound = 0;
            if (checkpointIsResponseToReportSegment) {
                // -If the checkpoint was itself issued in response to a report
                //segment, then this report is a "secondary" reception report.In
                //that case, the lower bound of the report SHOULD be the lower
                //bound of the report segment to which the triggering checkpoint
                //was itself a response, to minimize unnecessary retransmission.
                std::map<uint64_t, Ltp::report_segment_t>::iterator reportSegmentIt = m_mapAllReportSegmentsSent.find(*dataSegmentMetadata.reportSerialNumber);
                if (reportSegmentIt != m_mapAllReportSegmentsSent.end()) { //found
                    lowerBound = reportSegmentIt->second.lowerBound;
                }
                else {
                    LOG_ERROR(subprocess) << "LtpSessionReceiver::DataSegmentReceivedCallback: cannot find report segment";
                }
            }
            else {
                //- If the checkpoint was not issued in response to a report
                //segment, this report is a "primary" reception report.
                if (m_itLastPrimaryReportSegmentSent == m_mapAllReportSegmentsSent.end()) { //if (m_mapPrimaryReportSegmentsSent.empty()) {
                    //The lower bound of the first primary reception report issued for any session MUST be zero.
                    lowerBound = 0;
                }
                else {
                    //The lower bound of each subsequent
                    //primary reception report issued for the same session SHOULD be
                    //the upper bound of the prior primary reception report issued for
                    //the session, to minimize unnecessary retransmission.
                    lowerBound = m_itLastPrimaryReportSegmentSent->second.upperBound; //m_mapPrimaryReportSegmentsSent.crbegin()->second.upperBound;
                }
                for (rs_pending_map_t::const_reverse_iterator it = m_mapReportSegmentsPendingGeneration.crbegin(); it != m_mapReportSegmentsPendingGeneration.crend(); ++it) {
                    const csn_issecondary_pair_t& p = it->second;
                    if (!p.second) { //is prior primary and awaiting send
                        const uint64_t thisPrimaryUpperBound = it->first.endIndex + 1;
                        if (lowerBound < thisPrimaryUpperBound) {
                            lowerBound = thisPrimaryUpperBound;
                        }
                        break;
                    }
                }
            }

            //Note: If a discretionary
            //checkpoint is lost but subsequent segments are received, then by
            //the time the retransmission of the lost checkpoint is received
            //the receiver would have segments at block offsets beyond the
            //upper bound of the checkpoint.
            //In all cases, if the applicable lower bound of the scope of a report
            //is determined to be greater than or equal to the applicable upper
            //bound(for example, due to out - of - order arrival of discretionary
            //checkpoints) then the reception report MUST NOT be issued.
            if (lowerBound >= upperBound) {
                ++m_numReportSegmentsUnableToBeIssued;
            }
            else {
                // Github issue #22 Defer synchronous reception report with out-of-order data segments 
                //
                // When red part data is segmented and delivered to the receiving engine out-of-order,
                // the checkpoint(s) and EORP can be received before the earlier-in-block data segments.
                // If a synchronous report is sent immediately upon receiving the checkpoint there will be
                // data segments in-flight and about to be delivered that will be seen as reception gaps in the report.
                //
                // Instead of sending the synchronous report immediately upon receiving a checkpoint segment
                // the receiving engine should have some more complex logic:
                //
                // If the report segment bounds are fully claimed (i.e. no gaps) then the report can be sent immediately.
                // (Also send the report immediately if the out-of-order deferral feature is disabled (i.e. (time_duration == not_a_date_time))
                // which is needed for TestLtpEngine.)
                if ((m_timeManagerOfSendingDelayedReceptionReportsRef.GetTimeDurationRef() == boost::posix_time::special_values::not_a_date_time) ||
                    LtpFragmentSet::ContainsFragmentEntirely(m_receivedDataFragmentsSet, LtpFragmentSet::data_fragment_t(lowerBound, upperBound - 1)))
                {
                    HandleGenerateAndSendReportSegment(*dataSegmentMetadata.checkpointSerialNumber, lowerBound, upperBound, checkpointIsResponseToReportSegment);
                    // no need to set rsWasJustNowSentWithFullRedBounds because this section is for checkpoints only
                    
                }
                else {
                    // Otherwise, the engine should wait a very small window of time for gaps to be filled.
                    // The delay time should reset upon any data segments which fill gaps.
                    // In a situation with no loss but lots of out-of-order delivery this will have exactly the same number of reports,
                    // they will just be sent when the full checkpointed bounds of data have been received.
                    // In a situation with loss this will send reports with the fewest size of claim gaps.

                    //this should work regardless of primary or secondary reception reports
                    const std::size_t initialSetSize = m_mapReportSegmentsPendingGeneration.size();
                    rs_pending_map_t::iterator itRsPending =
                        m_mapReportSegmentsPendingGeneration.emplace_hint(m_mapReportSegmentsPendingGeneration.end(), //hint may be wrong for secondary reception reports
                        FragmentSet::data_fragment_no_overlap_allow_abut_t(lowerBound, upperBound - 1),
                        csn_issecondary_pair_t(*dataSegmentMetadata.checkpointSerialNumber, checkpointIsResponseToReportSegment));
                    if (initialSetSize == m_mapReportSegmentsPendingGeneration.size()) { //failedInsertion
                        LOG_ERROR(subprocess) << "LtpSessionReceiver::DataSegmentReceivedCallback: unable to insert " 
                            << ((checkpointIsResponseToReportSegment) ? "secondary" : "primary") << " reception into m_mapReportSegmentsPendingGeneration";
                    }
                    else {
                        //  sessionOriginatorEngineId = CHECKPOINT serial number to which RS pertains
                        //  sessionNumber = the session number
                        //  since this is a receiver, the real sessionOriginatorEngineId is constant among all receiving sessions and is not needed
                        const Ltp::session_id_t checkpointSerialNumberPlusSessionNumber(*dataSegmentMetadata.checkpointSerialNumber, M_SESSION_ID.sessionNumber);
                        std::vector<uint8_t> userData(sizeof(itRsPending));
                        rs_pending_map_t::iterator* itRsPendingPtr = (rs_pending_map_t::iterator*) userData.data();
                        *itRsPendingPtr = itRsPending;
                        if (!m_timeManagerOfSendingDelayedReceptionReportsRef.StartTimer(checkpointSerialNumberPlusSessionNumber, &m_delayedReceptionReportTimerExpiredCallback, std::move(userData))) {
                            LOG_ERROR(subprocess) << "unexpected error in LtpSessionReceiver::DataSegmentReceivedCallback: unable to start m_timeManagerOfSendingDelayedReceptionReportsRef timer for "
                                << ((checkpointIsResponseToReportSegment) ? "secondary" : "primary") << " reception report";
                        }
                    }
                }
            }
        }
        if ((!m_didRedPartReceptionCallback) && (m_lengthOfRedPart != UINT64_MAX) && (m_receivedDataFragmentsSet.size() == 1)) {
            std::set<LtpFragmentSet::data_fragment_t>::const_iterator it = m_receivedDataFragmentsSet.cbegin();
            if ((it->beginIndex == 0) && (it->endIndex == (m_lengthOfRedPart - 1))) { //all data fully received by this segment

                if (!isRedCheckpoint) { //Only when the red part data was completed by a non-checkpoint segment is the async. reception report needed.
                    // Github issue 23: Guarantee reception report when full red part data is received
                    //
                    // In the case where data segments arrive out-of-order and with enough delay
                    // that even a deferred reception report (Github issue 22) has some gap in it,
                    // there may be a point where the full set of data is received (with no retransmission, 
                    // only out-of-order in the first flight).
                    // When this happens the receiving engine must guarantee that an "asynchronous reception report"
                    // (one not in response to a checkpoint) is sent so that the sender knows to stop any
                    // data retransmission that hasn't yet gone out.
                    // This is a report guarantee in the sense that if the last received red segment that completed the
                    // red part data was a checkpoint there's no need for an async. reception report.
                    // Only when the red part data was completed by a non-checkpoint segment is the async. reception report needed.
                    //
                    // Do not send the async reception report if this non-checkpoint segment filled all the gaps of a pending/delayed reception
                    // report (thus immediately sending it out) AND that reception report had lowerBound == 0 and upperBound == lengthOfRedPart
                    if (!rsWasJustNowSentWithFullRedBounds) {
                        //Send Async reception report (i.e. checkpoint serial number == 0)
                        // and set lower bound and upper bound to the full range of red data.
                        HandleGenerateAndSendReportSegment(0, 0, m_lengthOfRedPart, false);
                    }
                }

                m_didRedPartReceptionCallback = true;
                if (redPartReceptionCallback) {
                    redPartReceptionCallback(M_SESSION_ID,
                        m_dataReceivedRed, m_lengthOfRedPart, dataSegmentMetadata.clientServiceId, isEndOfBlock);
                }
            }
        }
    }
    else { //green
        m_lowestGreenOffsetReceived = std::min(dataSegmentMetadata.offset, m_lowestGreenOffsetReceived);

        //6.21.  Handle Miscolored Segment
        //This procedure is triggered by the arrival of either (b) a green-part data segment whose block offset
        //is lower than the block offset of any red - part data segment previously received for the same session.
        //
        //The arrival of a segment
        //matching either of the above checks is a violation of the protocol
        //requirement of having all red - part data as the block prefix and all
        //green - part data as the block suffix.
        //
        //Response: the received data segment is simply discarded.
        //
        //The Cancel Session procedure(Section 6.19) is invoked and a CR
        //segment with reason - code MISCOLORED SHOULD be enqueued for
        //transmission to the data sender.
        if (m_currentRedLength > m_lowestGreenOffsetReceived) {
            if (!m_didNotifyForDeletion) {
                m_didNotifyForDeletion = true;
                m_notifyEngineThatThisReceiverNeedsDeletedCallback(M_SESSION_ID, true, CANCEL_SEGMENT_REASON_CODES::MISCOLORED); //close session (cancelled)
            }
            return;
        }

        if (greenPartSegmentArrivalCallback) {
            greenPartSegmentArrivalCallback(M_SESSION_ID, clientServiceDataVec, offsetPlusLength, dataSegmentMetadata.clientServiceId, isEndOfBlock);
        }
        
        if (isEndOfBlock) { //a green EOB
            //Note that if there were no red data segments received in the session
            //yet, including the case where the session was indeed fully green or
            //the pathological case where the entire red - part of the block gets
            //lost but at least the green data segment marked EOB is received(the
            //LTP receiver has no indication of whether the session had a red - part
            //transmission), the LTP receiver assumes the "RP rcvd. fully"
            //condition to be true and moves to the CLOSED state from the
            //WAIT_RP_REC state.
            const bool noRedSegmentsReceived = ((m_lengthOfRedPart == UINT64_MAX) && m_receivedDataFragmentsSet.empty()); //green EOB and no red segments received

            if (noRedSegmentsReceived || m_didRedPartReceptionCallback) { //if no red received or red fully complete, this green EOB shall close the session
                if (!m_didNotifyForDeletion) {
                    m_didNotifyForDeletion = true;
                    m_notifyEngineThatThisReceiverNeedsDeletedCallback(M_SESSION_ID, false, CANCEL_SEGMENT_REASON_CODES::RESERVED); //close session (not cancelled)
                }
            }
        }
    }
    
}

void LtpSessionReceiver::HandleGenerateAndSendReportSegment(const uint64_t checkpointSerialNumber,
    const uint64_t lowerBound, const uint64_t upperBound, const bool checkpointIsResponseToReportSegment)
{

    std::vector<Ltp::report_segment_t> reportSegmentsVec(1);
    if (!LtpFragmentSet::PopulateReportSegment(m_receivedDataFragmentsSet, reportSegmentsVec[0], lowerBound, upperBound)) {
        LOG_ERROR(subprocess) << "LtpSessionReceiver::DataSegmentReceivedCallback: cannot populate report segment";
    }

    if (reportSegmentsVec[0].receptionClaims.size() > M_MAX_RECEPTION_CLAIMS) {
        //3.2.  Retransmission
        //
        //... The maximum size of a report segment, like
        //all LTP segments, is constrained by the data - link MTU; if many non -
        //contiguous segments were lost in a large block transmission and/or
        //the data - link MTU was relatively small, multiple report segments need
        //to be generated.  In this case, LTP generates as many report segments
        //as are necessary and splits the scope of red - part data covered across
        //multiple report segments so that each of them may stand on their own.
        //For example, if three report segments are to be generated as part of
        //a reception report covering red - part data in range[0:1,000,000],
        //they could look like this: RS 19, scope[0:300,000], RS 20, scope
        //[300,000:950,000], and RS 21, scope[950,000:1,000,000].  In all
        //cases, a timer is started upon transmission of each report segment of
        //the reception report.
        std::vector<Ltp::report_segment_t> reportSegmentsSplitVec;
        LtpFragmentSet::SplitReportSegment(reportSegmentsVec[0], reportSegmentsSplitVec, M_MAX_RECEPTION_CLAIMS);
        ++m_numReportSegmentsTooLargeAndNeedingSplit;
        m_numReportSegmentsCreatedViaSplit += reportSegmentsSplitVec.size();
        reportSegmentsVec = std::move(reportSegmentsSplitVec);
    }

    for (std::vector<Ltp::report_segment_t>::iterator it = reportSegmentsVec.begin(); it != reportSegmentsVec.end(); ++it) {
        Ltp::report_segment_t& reportSegment = *it;

        //The value of the checkpoint serial number MUST be zero if the
        //report segment is NOT a response to reception of a checkpoint,
        //i.e., the reception report is asynchronous; otherwise, it MUST be
        //the checkpoint serial number of the checkpoint that caused the RS
        //to be issued.
        reportSegment.checkpointSerialNumber = checkpointSerialNumber;

        //The report serial number uniquely identifies the report among all
        //reports issued by the receiver in a session.The first report
        //issued by the receiver MUST have this serial number chosen
        //randomly for security reasons, and it is RECOMMENDED that the
        //receiver use the guidelines in[ESC05] for this.Any subsequent
        //RS issued by the receiver MUST have the serial number value found
        //by incrementing the last report serial number by 1.  When an RS is
        //retransmitted however, its serial number MUST be the same as when
        //it was originally transmitted.The report serial number MUST NOT
        //be zero.
        const uint64_t rsn = m_nextReportSegmentReportSerialNumber++;
        reportSegment.reportSerialNumber = rsn;
        //LtpFragmentSet::PrintFragmentSet(m_receivedDataFragmentsSet);

        //The emplace_hint function optimizes its insertion time if position points to the element that will follow the inserted element (or to the end, if it would be the last).
        report_segments_sent_map_t::const_iterator itRsSent = m_mapAllReportSegmentsSent.emplace_hint(m_mapAllReportSegmentsSent.end(), rsn, std::move(reportSegment)); //m_mapAllReportSegmentsSent[rsn] = std::move(reportSegment);
        if (!checkpointIsResponseToReportSegment) {
            m_itLastPrimaryReportSegmentSent = itRsSent;
            //m_mapPrimaryReportSegmentsSent.emplace_hint(m_mapPrimaryReportSegmentsSent.end(), rsn, reportSegment); //m_mapPrimaryReportSegmentsSent[rsn] = reportSegment;
        }
        m_reportsToSendQueue.emplace(itRsSent, 1); //initial retryCount of 1
        m_notifyEngineThatThisSendersTimersHasProducibleDataFunction(M_SESSION_ID);
    }
}
