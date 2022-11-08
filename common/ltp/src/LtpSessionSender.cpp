/**
 * @file LtpSessionSender.cpp
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

#include "LtpSessionSender.h"
#include "Logger.h"
#include <inttypes.h>
#include <boost/bind/bind.hpp>
#include <boost/next_prior.hpp>

static constexpr hdtn::Logger::SubProcess subprocess = hdtn::Logger::SubProcess::none;

LtpSessionSender::LtpSessionSender(uint64_t randomInitialSenderCheckpointSerialNumber,
    LtpClientServiceDataToSend && dataToSend, std::shared_ptr<LtpTransmissionRequestUserData> && userDataPtrToTake,
    uint64_t lengthOfRedPart, const uint64_t MTU, const Ltp::session_id_t & sessionId, const uint64_t clientServiceId,
    const boost::posix_time::time_duration & oneWayLightTime, const boost::posix_time::time_duration & oneWayMarginTime,
    LtpTimerManager<Ltp::session_id_t, Ltp::hash_session_id_t>& timeManagerOfCheckpointSerialNumbersRef,
    LtpTimerManager<uint64_t, std::hash<uint64_t> >& timeManagerOfSendingDelayedDataSegmentsRef,
    const NotifyEngineThatThisSenderNeedsDeletedCallback_t & notifyEngineThatThisSenderNeedsDeletedCallback,
    const NotifyEngineThatThisSenderHasProducibleDataFunction_t & notifyEngineThatThisSenderHasProducibleDataFunction,
    const InitialTransmissionCompletedCallback_t & initialTransmissionCompletedCallback, 
    const uint64_t checkpointEveryNthDataPacket, const uint32_t maxRetriesPerSerialNumber) :
    m_timeManagerOfCheckpointSerialNumbersRef(timeManagerOfCheckpointSerialNumbersRef),
    m_timeManagerOfSendingDelayedDataSegmentsRef(timeManagerOfSendingDelayedDataSegmentsRef),
    m_receptionClaimIndex(0),
    m_nextCheckpointSerialNumber(randomInitialSenderCheckpointSerialNumber),
    m_dataToSendSharedPtr(std::make_shared<LtpClientServiceDataToSend>(std::move(dataToSend))),
    m_userDataPtr(std::move(userDataPtrToTake)),
    M_LENGTH_OF_RED_PART(lengthOfRedPart),
    m_dataIndexFirstPass(0),
    m_didNotifyForDeletion(false),
    m_allRedDataReceivedByRemote(false),
    M_MTU(MTU),
    M_SESSION_ID(sessionId),
    M_CLIENT_SERVICE_ID(clientServiceId),
    M_CHECKPOINT_EVERY_NTH_DATA_PACKET(checkpointEveryNthDataPacket),
    m_checkpointEveryNthDataPacketCounter(checkpointEveryNthDataPacket),
    M_MAX_RETRIES_PER_SERIAL_NUMBER(maxRetriesPerSerialNumber),
    //m_numActiveTimers(0),
    m_notifyEngineThatThisSenderNeedsDeletedCallback(notifyEngineThatThisSenderNeedsDeletedCallback),
    m_notifyEngineThatThisSenderHasProducibleDataFunction(notifyEngineThatThisSenderHasProducibleDataFunction),
    m_initialTransmissionCompletedCallback(initialTransmissionCompletedCallback),
    m_numCheckpointTimerExpiredCallbacks(0),
    m_numDiscretionaryCheckpointsNotResent(0),
    m_numDeletedFullyClaimedPendingReports(0),
    m_isFailedSession(false),
    m_calledCancelledOrCompletedCallback(false)
{
    m_timerExpiredCallback = boost::bind(&LtpSessionSender::LtpCheckpointTimerExpiredCallback, this, boost::placeholders::_1, boost::placeholders::_2);
    m_delayedDataSegmentsTimerExpiredCallback = boost::bind(&LtpSessionSender::LtpDelaySendDataSegmentsTimerExpiredCallback, this, boost::placeholders::_1, boost::placeholders::_2);
    m_notifyEngineThatThisSenderHasProducibleDataFunction(M_SESSION_ID.sessionNumber); //to trigger first pass of red data
}

LtpSessionSender::~LtpSessionSender() {
    //clean up this sending session's active timers within the shared LtpTimerManager
    for (std::list<uint64_t>::const_iterator it = m_checkpointSerialNumberActiveTimersList.cbegin(); it != m_checkpointSerialNumberActiveTimersList.cend(); ++it) {
        const uint64_t csn = *it;

        // within a session would normally be LtpTimerManager<uint64_t, std::hash<uint64_t> > m_timeManagerOfCheckpointSerialNumbers;
        // but now sharing a single LtpTimerManager among all sessions, so use a
        // LtpTimerManager<Ltp::session_id_t, Ltp::hash_session_id_t> (which has hash map hashing function support)
        // such that: 
        //  sessionOriginatorEngineId = CHECKPOINT serial number
        //  sessionNumber = the session number
        //  since this is a sender, the real sessionOriginatorEngineId is constant among all sending sessions and is not needed
        const Ltp::session_id_t checkpointSerialNumberPlusSessionNumber(csn, M_SESSION_ID.sessionNumber);

        if (!m_timeManagerOfCheckpointSerialNumbersRef.DeleteTimer(checkpointSerialNumberPlusSessionNumber)) {
            LOG_ERROR(subprocess) << "LtpSessionSender::~LtpSessionSender: did not delete timer";
        }
    }
    //clean up this sending session's single active timer within the shared LtpTimerManager
    if (m_mapRsBoundsToRsnPendingGeneration.size()) {
        if (!m_timeManagerOfSendingDelayedDataSegmentsRef.DeleteTimer(M_SESSION_ID.sessionNumber)) {
            LOG_ERROR(subprocess) << "LtpSessionSender::~LtpSessionSender: did not delete timer in m_timeManagerOfSendingDelayedDataSegmentsRef";
        }
    }
}

void LtpSessionSender::LtpCheckpointTimerExpiredCallback(const Ltp::session_id_t& checkpointSerialNumberPlusSessionNumber, std::vector<uint8_t> & userData) {
    // within a session would normally be LtpTimerManager<uint64_t, std::hash<uint64_t> > m_timeManagerOfCheckpointSerialNumbers;
    // but now sharing a single LtpTimerManager among all sessions, so use a
    // LtpTimerManager<Ltp::session_id_t, Ltp::hash_session_id_t> (which has hash map hashing function support)
    // such that: 
    //  sessionOriginatorEngineId = CHECKPOINT serial number
    //  sessionNumber = the session number
    //  since this is a sender, the real sessionOriginatorEngineId is constant among all sending sessions and is not needed
    const uint64_t checkpointSerialNumber = checkpointSerialNumberPlusSessionNumber.sessionOriginatorEngineId;

    if (userData.size() != sizeof(csntimer_userdata_t)) {
        LOG_ERROR(subprocess) << "LtpSessionReceiver::LtpReportSegmentTimerExpiredCallback: userData.size() != sizeof(rsntimer_userdata_t)";
        return;
    }
    csntimer_userdata_t* userDataPtr = reinterpret_cast<csntimer_userdata_t*>(userData.data());

    //keep track of this sending session's active timers within the shared LtpTimerManager
    m_checkpointSerialNumberActiveTimersList.erase(userDataPtr->itCheckpointSerialNumberActiveTimersList);

    //6.7.  Retransmit Checkpoint
    //This procedure is triggered by the expiration of a countdown timer
    //associated with a CP segment.
    //
    //Response: if the number of times this CP segment has been queued for
    //transmission exceeds the checkpoint retransmission limit established
    //for the local LTP engine by network management, then the session of
    //which the segment is one token is canceled : the "Cancel Session"
    //procedure(Section 6.19) is invoked, a CS with reason - code RLEXC is
    //appended to the(conceptual) application data queue, and a
    //transmission - session cancellation notice(Section 7.5) is sent back
    //to the client service that requested the transmission.
    //
    //Otherwise, a new copy of the CP segment is appended to the
    //(conceptual) application data queue for the destination LTP engine.

    
    ++m_numCheckpointTimerExpiredCallbacks;

    resend_fragment_t & resendFragment = userDataPtr->resendFragment;

    if (resendFragment.retryCount <= M_MAX_RETRIES_PER_SERIAL_NUMBER) {
        const bool isDiscretionaryCheckpoint = (resendFragment.flags == LTP_DATA_SEGMENT_TYPE_FLAGS::REDDATA_CHECKPOINT);
        if (isDiscretionaryCheckpoint && LtpFragmentSet::ContainsFragmentEntirely(m_dataFragmentsAckedByReceiver,
            LtpFragmentSet::data_fragment_t(resendFragment.offset, (resendFragment.offset + resendFragment.length) - 1)))
        {
            ++m_numDiscretionaryCheckpointsNotResent;
        }
        else {
            //resend 
            ++(resendFragment.retryCount);
            m_resendFragmentsQueue.push(resendFragment);
            m_notifyEngineThatThisSenderHasProducibleDataFunction(M_SESSION_ID.sessionNumber);
        }
    }
    else {
        if (!m_didNotifyForDeletion) {
            m_isFailedSession = true;
            m_didNotifyForDeletion = true;
            m_notifyEngineThatThisSenderNeedsDeletedCallback(M_SESSION_ID, true, CANCEL_SEGMENT_REASON_CODES::RLEXC, m_userDataPtr);
        }
    }
}

void LtpSessionSender::LtpDelaySendDataSegmentsTimerExpiredCallback(const uint64_t& sessionNumber, std::vector<uint8_t>& userData) {
    // Github issue 24: Defer data retransmission with out-of-order report segments (see detailed description below)
    //...When the retransmission timer expires (i.e. there are still gaps to send) then send data segments to cover the remaining gaps for the session.
    std::list<std::pair<uint64_t, std::set<LtpFragmentSet::data_fragment_t> > > listFragmentSetNeedingResentForEachReport;
    LtpFragmentSet::ReduceReportSegments(m_mapRsBoundsToRsnPendingGeneration, m_dataFragmentsAckedByReceiver, listFragmentSetNeedingResentForEachReport);
    for (std::list<std::pair<uint64_t, std::set<LtpFragmentSet::data_fragment_t> > >::const_iterator it = listFragmentSetNeedingResentForEachReport.cbegin();
        it != listFragmentSetNeedingResentForEachReport.cend(); ++it)
    {
        ResendDataFromReport(it->second, it->first);
    }
    m_mapRsBoundsToRsnPendingGeneration.clear(); //also flag that signifies timer stopped
    if (!m_didNotifyForDeletion) {
        m_notifyEngineThatThisSenderHasProducibleDataFunction(M_SESSION_ID.sessionNumber);
    }
}

bool LtpSessionSender::NextDataToSend(std::vector<boost::asio::const_buffer>& constBufferVec,
    std::shared_ptr<std::vector<std::vector<uint8_t> > >& underlyingDataToDeleteOnSentCallback,
    std::shared_ptr<LtpClientServiceDataToSend>& underlyingCsDataToDeleteOnSentCallback)
{
    if (!m_nonDataToSend.empty()) { //includes report ack segments
        //highest priority
        underlyingDataToDeleteOnSentCallback = std::make_shared<std::vector<std::vector<uint8_t> > >(1);
        (*underlyingDataToDeleteOnSentCallback)[0] = std::move(m_nonDataToSend.front());
        m_nonDataToSend.pop();
        constBufferVec.resize(1);
        constBufferVec[0] = boost::asio::buffer((*underlyingDataToDeleteOnSentCallback)[0]);
        return true;
    }

    while (!m_resendFragmentsQueue.empty()) {
        if (m_allRedDataReceivedByRemote) {
            //Continuation of Github issue 23:
            //If the sender detects that all Red data has been acknowledged by the remote,
            //the sender shall remove all Red data segments (checkpoint or non-checkpoint) from the
            //outgoing transmission queue.
            m_resendFragmentsQueue.pop();
            continue;
        }
        LtpSessionSender::resend_fragment_t & resendFragment = m_resendFragmentsQueue.front();
        Ltp::data_segment_metadata_t meta;
        meta.clientServiceId = M_CLIENT_SERVICE_ID;
        meta.offset = resendFragment.offset;
        meta.length = resendFragment.length;
        const bool isCheckpoint = (resendFragment.flags != LTP_DATA_SEGMENT_TYPE_FLAGS::REDDATA);
        if (isCheckpoint) {
            meta.checkpointSerialNumber = &resendFragment.checkpointSerialNumber;
            meta.reportSerialNumber = &resendFragment.reportSerialNumber;

            //6.2.  Start Checkpoint Timer
            //This procedure is triggered by the arrival of a link state cue
            //indicating the de - queuing(for transmission) of a CP segment.
            //
            //Response: the expected arrival time of the RS segment that will be
            //produced on reception of this CP segment is computed, and a countdown
            //timer is started for this arrival time.However, if it is known that
            //the remote LTP engine has ceased transmission(Section 6.5), then
            //this timer is immediately suspended, because the computed expected
            //arrival time may require an adjustment that cannot yet be computed.
            

            // within a session would normally be LtpTimerManager<uint64_t, std::hash<uint64_t> > m_timeManagerOfCheckpointSerialNumbers;
            // but now sharing a single LtpTimerManager among all sessions, so use a
            // LtpTimerManager<Ltp::session_id_t, Ltp::hash_session_id_t> (which has hash map hashing function support)
            // such that: 
            //  sessionOriginatorEngineId = CHECKPOINT serial number
            //  sessionNumber = the session number
            //  since this is a sender, the real sessionOriginatorEngineId is constant among all sending sessions and is not needed
            const Ltp::session_id_t checkpointSerialNumberPlusSessionNumber(resendFragment.checkpointSerialNumber, M_SESSION_ID.sessionNumber);

            std::vector<uint8_t> userData(sizeof(csntimer_userdata_t));
            csntimer_userdata_t* userDataPtr = reinterpret_cast<csntimer_userdata_t*>(userData.data());
            userDataPtr->resendFragment = resendFragment;
            m_checkpointSerialNumberActiveTimersList.emplace_front(resendFragment.checkpointSerialNumber); //keep track of this sending session's active timers within the shared LtpTimerManager
            userDataPtr->itCheckpointSerialNumberActiveTimersList = m_checkpointSerialNumberActiveTimersList.begin();
            if (!m_timeManagerOfCheckpointSerialNumbersRef.StartTimer(checkpointSerialNumberPlusSessionNumber, &m_timerExpiredCallback, std::move(userData))) {
                m_checkpointSerialNumberActiveTimersList.erase(m_checkpointSerialNumberActiveTimersList.begin());
                LOG_ERROR(subprocess) << "LtpSessionSender::NextDataToSend: did not start timer";
            }
        }
        else { //non-checkpoint
            meta.checkpointSerialNumber = NULL;
            meta.reportSerialNumber = NULL;
        }
        underlyingDataToDeleteOnSentCallback = std::make_shared<std::vector<std::vector<uint8_t> > >(1); //2 would be needed in case of trailer extensions (but not used here)
        Ltp::GenerateLtpHeaderPlusDataSegmentMetadata((*underlyingDataToDeleteOnSentCallback)[0], resendFragment.flags,
            M_SESSION_ID, meta, NULL, 0);
        constBufferVec.resize(2); //3 would be needed in case of trailer extensions (but not used here)
        constBufferVec[0] = boost::asio::buffer((*underlyingDataToDeleteOnSentCallback)[0]);
        constBufferVec[1] = boost::asio::buffer(m_dataToSendSharedPtr->data() + resendFragment.offset, resendFragment.length);
        //Increase the reference count of the LtpClientServiceDataToSend shared_ptr
        //so that the LtpClientServiceDataToSend won't get deleted before the UDP send operation completes.
        //This event would be caused by the LtpSessionSender getting deleted before the UDP send operation completes,
        //which would almost always happen with green data and could happen with red data.
        underlyingCsDataToDeleteOnSentCallback = m_dataToSendSharedPtr;
        m_resendFragmentsQueue.pop();
        return true;
    }

    if (m_dataIndexFirstPass < m_dataToSendSharedPtr->size()) {
        if (m_dataIndexFirstPass < M_LENGTH_OF_RED_PART) { //first pass of red data send
            uint64_t bytesToSendRed = std::min(M_LENGTH_OF_RED_PART - m_dataIndexFirstPass, M_MTU);
            const bool isEndOfRedPart = ((bytesToSendRed + m_dataIndexFirstPass) == M_LENGTH_OF_RED_PART);
            bool isPeriodicCheckpoint = false;
            if (M_CHECKPOINT_EVERY_NTH_DATA_PACKET && (--m_checkpointEveryNthDataPacketCounter == 0)) {
                m_checkpointEveryNthDataPacketCounter = M_CHECKPOINT_EVERY_NTH_DATA_PACKET;
                isPeriodicCheckpoint = true;
            }
            const bool isCheckpoint = isPeriodicCheckpoint || isEndOfRedPart;

            LTP_DATA_SEGMENT_TYPE_FLAGS flags = LTP_DATA_SEGMENT_TYPE_FLAGS::REDDATA;
            uint64_t cp;
            uint64_t * checkpointSerialNumber = NULL;
            uint64_t rsn = 0; //0 in this state because not a response to an RS
            uint64_t * reportSerialNumber = NULL;
            if (isCheckpoint) {
                flags = LTP_DATA_SEGMENT_TYPE_FLAGS::REDDATA_CHECKPOINT;
                cp = m_nextCheckpointSerialNumber++;
                checkpointSerialNumber = &cp;
                reportSerialNumber = &rsn;
                if (isEndOfRedPart) {
                    flags = LTP_DATA_SEGMENT_TYPE_FLAGS::REDDATA_CHECKPOINT_ENDOFREDPART;
                    const bool isEndOfBlock = (M_LENGTH_OF_RED_PART == m_dataToSendSharedPtr->size());
                    if (isEndOfBlock) {
                        flags = LTP_DATA_SEGMENT_TYPE_FLAGS::REDDATA_CHECKPOINT_ENDOFREDPART_ENDOFBLOCK;
                    }
                }
                std::vector<uint8_t> userData(sizeof(csntimer_userdata_t));
                csntimer_userdata_t* userDataPtr = reinterpret_cast<csntimer_userdata_t*>(userData.data());
                LtpSessionSender::resend_fragment_t & resendFragment = userDataPtr->resendFragment;
                new (&resendFragment) LtpSessionSender::resend_fragment_t(m_dataIndexFirstPass, bytesToSendRed, cp, rsn, flags); //placement new

                // within a session would normally be LtpTimerManager<uint64_t, std::hash<uint64_t> > m_timeManagerOfCheckpointSerialNumbers;
                // but now sharing a single LtpTimerManager among all sessions, so use a
                // LtpTimerManager<Ltp::session_id_t, Ltp::hash_session_id_t> (which has hash map hashing function support)
                // such that: 
                //  sessionOriginatorEngineId = CHECKPOINT serial number
                //  sessionNumber = the session number
                //  since this is a sender, the real sessionOriginatorEngineId is constant among all sending sessions and is not needed
                const Ltp::session_id_t checkpointSerialNumberPlusSessionNumber(cp, M_SESSION_ID.sessionNumber);

                m_checkpointSerialNumberActiveTimersList.emplace_front(cp); //keep track of this sending session's active timers within the shared LtpTimerManager
                userDataPtr->itCheckpointSerialNumberActiveTimersList = m_checkpointSerialNumberActiveTimersList.begin();
                if (!m_timeManagerOfCheckpointSerialNumbersRef.StartTimer(checkpointSerialNumberPlusSessionNumber, &m_timerExpiredCallback, std::move(userData))) {
                    m_checkpointSerialNumberActiveTimersList.erase(m_checkpointSerialNumberActiveTimersList.begin());
                    LOG_ERROR(subprocess) << "LtpSessionSender::NextDataToSend: did not start timer";
                }
            }

            Ltp::data_segment_metadata_t meta;
            meta.clientServiceId = M_CLIENT_SERVICE_ID;
            meta.offset = m_dataIndexFirstPass;
            meta.length = bytesToSendRed;
            meta.checkpointSerialNumber = checkpointSerialNumber;
            meta.reportSerialNumber = reportSerialNumber;
            underlyingDataToDeleteOnSentCallback = std::make_shared<std::vector<std::vector<uint8_t> > >(1); //2 would be needed in case of trailer extensions (but not used here)
            Ltp::GenerateLtpHeaderPlusDataSegmentMetadata((*underlyingDataToDeleteOnSentCallback)[0], flags,
                M_SESSION_ID, meta, NULL, 0);
            constBufferVec.resize(2); //3 would be needed in case of trailer extensions (but not used here)
            constBufferVec[0] = boost::asio::buffer((*underlyingDataToDeleteOnSentCallback)[0]);
            constBufferVec[1] = boost::asio::buffer(m_dataToSendSharedPtr->data() + m_dataIndexFirstPass, bytesToSendRed);
            m_dataIndexFirstPass += bytesToSendRed;
        }
        else { //first pass of green data send
            uint64_t bytesToSendGreen = std::min(m_dataToSendSharedPtr->size() - m_dataIndexFirstPass, M_MTU);
            const bool isEndOfBlock = ((bytesToSendGreen + m_dataIndexFirstPass) == m_dataToSendSharedPtr->size());
            LTP_DATA_SEGMENT_TYPE_FLAGS flags = LTP_DATA_SEGMENT_TYPE_FLAGS::GREENDATA;
            if (isEndOfBlock) {
                flags = LTP_DATA_SEGMENT_TYPE_FLAGS::GREENDATA_ENDOFBLOCK;
            }
            Ltp::data_segment_metadata_t meta;
            meta.clientServiceId = M_CLIENT_SERVICE_ID;
            meta.offset = m_dataIndexFirstPass;
            meta.length = bytesToSendGreen;
            meta.checkpointSerialNumber = NULL;
            meta.reportSerialNumber = NULL;
            underlyingDataToDeleteOnSentCallback = std::make_shared<std::vector<std::vector<uint8_t> > >(1); //2 would be needed in case of trailer extensions (but not used here)
            Ltp::GenerateLtpHeaderPlusDataSegmentMetadata((*underlyingDataToDeleteOnSentCallback)[0], flags,
                M_SESSION_ID, meta, NULL, 0);
            constBufferVec.resize(2); //3 would be needed in case of trailer extensions (but not used here)
            constBufferVec[0] = boost::asio::buffer((*underlyingDataToDeleteOnSentCallback)[0]);
            constBufferVec[1] = boost::asio::buffer(m_dataToSendSharedPtr->data() + m_dataIndexFirstPass, bytesToSendGreen);
            m_dataIndexFirstPass += bytesToSendGreen;
        }
        if (m_dataIndexFirstPass == m_dataToSendSharedPtr->size()) { //only ever enters here once
            //Increase the reference count of the LtpClientServiceDataToSend shared_ptr
            //so that the LtpClientServiceDataToSend won't get deleted before the UDP send operation completes.
            //This event would be caused by the LtpSessionSender getting deleted before the UDP send operation completes,
            //which would almost always happen with green data and could happen with red data.
            underlyingCsDataToDeleteOnSentCallback = m_dataToSendSharedPtr;

            m_initialTransmissionCompletedCallback(M_SESSION_ID, m_userDataPtr);
            if (M_LENGTH_OF_RED_PART == 0) { //fully green case complete (notify engine for deletion)
                if (!m_didNotifyForDeletion) {
                    m_didNotifyForDeletion = true;
                    m_notifyEngineThatThisSenderNeedsDeletedCallback(M_SESSION_ID, false, CANCEL_SEGMENT_REASON_CODES::RESERVED, m_userDataPtr);
                }
            }
            else if (m_dataFragmentsAckedByReceiver.size() == 1) { //in case red data already acked before green data send completes
                std::set<LtpFragmentSet::data_fragment_t>::const_iterator it = m_dataFragmentsAckedByReceiver.cbegin();
                if ((it->beginIndex == 0) && (it->endIndex >= (M_LENGTH_OF_RED_PART - 1))) { //>= in case some green data was acked
                    if (!m_didNotifyForDeletion) {
                        m_didNotifyForDeletion = true;
                        m_notifyEngineThatThisSenderNeedsDeletedCallback(M_SESSION_ID, false, CANCEL_SEGMENT_REASON_CODES::RESERVED, m_userDataPtr);
                    }
                }
            }
        }
        
        return true;
    }
    return false;
}


void LtpSessionSender::ReportSegmentReceivedCallback(const Ltp::report_segment_t & reportSegment,
    Ltp::ltp_extensions_t & headerExtensions, Ltp::ltp_extensions_t & trailerExtensions)
{
    //6.13.  Retransmit Data

    //Response: first, an RA segment with the same report serial number as
    //the RS segment is issued and is, in concept, appended to the queue of
    //internal operations traffic bound for the receiver.
    m_nonDataToSend.emplace(); //m_notifyEngineThatThisSenderHasProducibleDataFunction at the end of this function
    Ltp::GenerateReportAcknowledgementSegmentLtpPacket(m_nonDataToSend.back(),
        M_SESSION_ID, reportSegment.reportSerialNumber, NULL, NULL);

    //If the RS segment is redundant -- i.e., either the indicated session is unknown
    //(for example, the RS segment is received after the session has been
    //completed or canceled) or the RS segment's report serial number
    //matches that of an RS segment that has already been received and
    //processed -- then no further action is taken.
    if (m_reportSegmentSerialNumbersReceivedSet.insert(reportSegment.reportSerialNumber).second) { //serial number was inserted (it's new)
        //If the report's checkpoint serial number is not zero, then the
        //countdown timer associated with the indicated checkpoint segment is deleted.
        if (reportSegment.checkpointSerialNumber) {

            // within a session would normally be LtpTimerManager<uint64_t, std::hash<uint64_t> > m_timeManagerOfCheckpointSerialNumbers;
            // but now sharing a single LtpTimerManager among all sessions, so use a
            // LtpTimerManager<Ltp::session_id_t, Ltp::hash_session_id_t> (which has hash map hashing function support)
            // such that: 
            //  sessionOriginatorEngineId = CHECKPOINT serial number
            //  sessionNumber = the session number
            //  since this is a sender, the real sessionOriginatorEngineId is constant among all sending sessions and is not needed
            const Ltp::session_id_t checkpointSerialNumberPlusSessionNumber(reportSegment.checkpointSerialNumber, M_SESSION_ID.sessionNumber);

            std::vector<uint8_t> userDataReturned;
            if (m_timeManagerOfCheckpointSerialNumbersRef.DeleteTimer(checkpointSerialNumberPlusSessionNumber, userDataReturned)) { //if delete of a timer was successful
                if (userDataReturned.size() != sizeof(csntimer_userdata_t)) {
                    LOG_ERROR(subprocess) << "LtpSessionSender::ReportSegmentReceivedCallback: userDataReturned.size() != sizeof(csntimer_userdata_t)";
                }
                else {
                    const csntimer_userdata_t* userDataPtr = reinterpret_cast<csntimer_userdata_t*>(userDataReturned.data());
                    //keep track of this sending session's active timers within the shared LtpTimerManager
                    m_checkpointSerialNumberActiveTimersList.erase(userDataPtr->itCheckpointSerialNumberActiveTimersList);
                }
            }
        }


        if (LtpFragmentSet::AddReportSegmentToFragmentSet(m_dataFragmentsAckedByReceiver, reportSegment)) { //this RS indicates new acks by receiver

            

            //6.12.  Signify Transmission Completion
            //
            //This procedure is triggered at the earliest time at which(a) all
            //data in the block are known to have been transmitted *and* (b)the
            //entire red - part of the block-- if of non - zero length -- is known to
            //have been successfully received.Condition(a) is signaled by
            //arrival of a link state cue indicating the de - queuing(for
            //transmission) of the EOB segment for the block.Condition(b) is
            //signaled by reception of an RS segment whose reception claims, taken
            //together with the reception claims of all other RS segments
            //previously received in the course of this session, indicate complete
            //reception of the red - part of the block.
            //
            //Response: a transmission - session completion notice(Section 7.4) is
            //sent to the local client service associated with the session, and the
            //session is closed : the "Close Session" procedure(Section 6.20) is
            //invoked.
            if (m_allRedDataReceivedByRemote == false) { //the m_allRedDataReceivedByRemote flag is used to prevent resending of non-checkpoint data (Continuation of Github issue 23)
                if (m_dataFragmentsAckedByReceiver.size() == 1) {
                    std::set<LtpFragmentSet::data_fragment_t>::const_iterator it = m_dataFragmentsAckedByReceiver.cbegin();
                    if ((it->beginIndex == 0) && (it->endIndex >= (M_LENGTH_OF_RED_PART - 1))) { //>= in case some green data was acked
                        m_allRedDataReceivedByRemote = true;
                    }
                }
            }
            if ((m_dataIndexFirstPass == m_dataToSendSharedPtr->size()) && m_allRedDataReceivedByRemote) { //if red and green fully sent and all red data acked
                if (!m_didNotifyForDeletion) {
                    m_didNotifyForDeletion = true;
                    m_notifyEngineThatThisSenderNeedsDeletedCallback(M_SESSION_ID, false, CANCEL_SEGMENT_REASON_CODES::RESERVED, m_userDataPtr);
                }
            }


            //If the segment's reception claims indicate incomplete data reception
            //within the scope of the report segment :
            //If the number of transmission problems for this session has not
            //exceeded any limit, new data segments encapsulating all block
            //data whose non - reception is implied by the reception claims are
            //appended to the transmission queue bound for the receiver.The
            //last-- and only the last -- data segment must be marked as a CP
            //segment carrying a new CP serial number(obtained by
            //incrementing the last CP serial number used) and the report
            //serial number of the received RS segment.
            std::set<LtpFragmentSet::data_fragment_t> fragmentsNeedingResent;
#if 0
            LtpFragmentSet::AddReportSegmentToFragmentSetNeedingResent(fragmentsNeedingResent, reportSegment);
#else
            // improvements from Github issue 24
            const FragmentSet::data_fragment_t bounds(reportSegment.lowerBound, reportSegment.upperBound - 1);
            const LtpFragmentSet::data_fragment_unique_overlapping_t& boundsUnique = *(reinterpret_cast<const LtpFragmentSet::data_fragment_unique_overlapping_t*>(&bounds));
            FragmentSet::GetBoundsMinusFragments(bounds, m_dataFragmentsAckedByReceiver, fragmentsNeedingResent);
#endif
            // Send the data segments immediately if the out-of-order deferral feature is disabled (i.e. (time_duration == not_a_date_time))
            // which is needed for TestLtpEngine.
            if (m_timeManagerOfSendingDelayedDataSegmentsRef.GetTimeDurationRef() == boost::posix_time::special_values::not_a_date_time) { //disabled
                ResendDataFromReport(fragmentsNeedingResent, reportSegment.reportSerialNumber); //will do nothing if fragmentsNeedingResent.empty() (i.e. this rs Has No Gaps In its Claims)
            }
            else {
                // Github issue 24: Defer data retransmission with out-of-order report segments
                //
                // When the network is causing out-of-order segment reception it is possible that one or more
                // synchronous reception reports are received either out-of-order or within a short time window,
                // possibly followed by an asynchronous reception report (see #23) indicating that the full red
                // part was received. To avoid unnecessary data retransmission the sending engine should defer
                // sending gap-filling data segments until some small window of time after the last reception
                // report for that session.
                //
                // Upon receiving a reception report the sending engine should (in addition to sending a report ack segment):
                //
                // 1.) Add (as a set union) the report's claims to the total claims seen for the session.
                // 2.) If a retransmission timer is not running:
                //      a.) If there are gaps in this report's claims (between its lower and upper bounds)
                //          (Note: gaps exist because the report itself has gaps AND no prior received report filled those gaps),
                //          then add this report to a pending report list and start a retransmission timer for the session
                // 3.) Otherwise, (i.e. If a retransmission timer is running):
                //      a.) add this report to the pending report list
                //      b.) if there are no gaps between the smallest lower bound of the pending report list
                //          and the largest upper bound of the pending report list then:
                //             stop the timer and clear the list (there is no need to retransmit
                //             data segments from any of the reports in the list)
                //
                // When the retransmission timer expires (i.e. there are still gaps to send) then send data segments to cover the remaining gaps for the session.
                // This involves iterating through the pending report list starting from the report with the lowest lowerBound
                // and transmitting the report serial number as the checkpoint for its last segment.
                // Subsequent reports in the iteration of the list should be ignored if everything
                // between their upper and lower bounds are fully contained in the set union
                // of ((total claims seen for the session) UNION (the just sent data segments of the prior reports in this list)).
                //
                // The result of this procedure is that the sending engine will not send either duplicate data segments to
                // cover gaps in earlier-sent reports which are claimed in later-sent reports. In the case of no loss but
                // highly out-of-order this will result in no unnecessary data retransmission to occur.

                if (m_mapRsBoundsToRsnPendingGeneration.empty()) { //timer is not running
                    if (!fragmentsNeedingResent.empty()) { //the only rs has gaps in the claims
                        //start the timer
                        m_largestEndIndexPendingGeneration = boundsUnique.endIndex;
                        m_mapRsBoundsToRsnPendingGeneration.emplace(boundsUnique, reportSegment.reportSerialNumber);
                        if (!m_timeManagerOfSendingDelayedDataSegmentsRef.StartTimer(M_SESSION_ID.sessionNumber, &m_delayedDataSegmentsTimerExpiredCallback)) {
                            LOG_ERROR(subprocess) << "LtpSessionSender::ReportSegmentReceivedCallback: unable to start m_timeManagerOfSendingDelayedDataSegmentsRef timer";
                        }
                    }
                    //else no work to do (no data segments to send)
                }
                else { //timer is running
                    m_largestEndIndexPendingGeneration = std::max(m_largestEndIndexPendingGeneration, boundsUnique.endIndex);
                    m_mapRsBoundsToRsnPendingGeneration.emplace(boundsUnique, reportSegment.reportSerialNumber);
                    const uint64_t largestBeginIndexPendingGeneration = m_mapRsBoundsToRsnPendingGeneration.begin()->first.beginIndex; //based on operator <
                    const bool pendingReportsHaveNoGapsInClaims = LtpFragmentSet::ContainsFragmentEntirely(m_dataFragmentsAckedByReceiver,
                        LtpFragmentSet::data_fragment_t(largestBeginIndexPendingGeneration, m_largestEndIndexPendingGeneration));
                    if (pendingReportsHaveNoGapsInClaims) {
                        m_numDeletedFullyClaimedPendingReports += m_mapRsBoundsToRsnPendingGeneration.size();
                        //since there is a retransmission timer running stop it (there is no need to retransmit in this case)
                        if (!m_timeManagerOfSendingDelayedDataSegmentsRef.DeleteTimer(M_SESSION_ID.sessionNumber)) {
                            LOG_ERROR(subprocess) << "LtpSessionSender::ReportSegmentReceivedCallback: did not delete timer in m_timeManagerOfSendingDelayedDataSegmentsRef";
                        }
                        m_mapRsBoundsToRsnPendingGeneration.clear(); //also used as flag to signify timer no longer running
                    }
                    //else since there are gaps in the claims, add to the retransmission timer for the session (already done above through m_mapRsBoundsToRsnPendingGeneration.emplace)
                    
                }
            }
            
        }
    }
    if (!m_didNotifyForDeletion) {
        m_notifyEngineThatThisSenderHasProducibleDataFunction(M_SESSION_ID.sessionNumber);
    }
}

void LtpSessionSender::ResendDataFromReport(const std::set<LtpFragmentSet::data_fragment_t>& fragmentsNeedingResent, const uint64_t reportSerialNumber) {
    for (std::set<LtpFragmentSet::data_fragment_t>::const_iterator it = fragmentsNeedingResent.cbegin(); it != fragmentsNeedingResent.cend(); ++it) {
        const bool isLastFragmentNeedingResent = (boost::next(it) == fragmentsNeedingResent.cend());
        for (uint64_t dataIndex = it->beginIndex; dataIndex <= it->endIndex; ) {
            uint64_t bytesToSendRed = std::min((it->endIndex - dataIndex) + 1, M_MTU);
            if ((bytesToSendRed + dataIndex) > M_LENGTH_OF_RED_PART) {
                LOG_FATAL(subprocess) << "gt length red part";
            }
            const bool isLastPacketNeedingResent = ((isLastFragmentNeedingResent) && ((dataIndex + bytesToSendRed) == (it->endIndex + 1)));
            const bool isEndOfRedPart = ((bytesToSendRed + dataIndex) == M_LENGTH_OF_RED_PART);
            if (isEndOfRedPart && !isLastPacketNeedingResent) {
                LOG_FATAL(subprocess) << "end of red part but not last packet being resent";
            }

            uint64_t checkpointSerialNumber = 0; //dont care
            LTP_DATA_SEGMENT_TYPE_FLAGS flags = LTP_DATA_SEGMENT_TYPE_FLAGS::REDDATA;
            if (isLastPacketNeedingResent) {
                flags = LTP_DATA_SEGMENT_TYPE_FLAGS::REDDATA_CHECKPOINT;
                checkpointSerialNumber = m_nextCheckpointSerialNumber++; //now we care since this is now a checkpoint
                if (isEndOfRedPart) {
                    flags = LTP_DATA_SEGMENT_TYPE_FLAGS::REDDATA_CHECKPOINT_ENDOFREDPART;
                    const bool isEndOfBlock = (M_LENGTH_OF_RED_PART == m_dataToSendSharedPtr->size());
                    if (isEndOfBlock) {
                        flags = LTP_DATA_SEGMENT_TYPE_FLAGS::REDDATA_CHECKPOINT_ENDOFREDPART_ENDOFBLOCK;
                    }
                }
            }

            m_resendFragmentsQueue.emplace(dataIndex, bytesToSendRed, checkpointSerialNumber, reportSerialNumber, flags);
            dataIndex += bytesToSendRed;
        }
    }
}
