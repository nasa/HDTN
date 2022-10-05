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
#include <iostream>
#include <inttypes.h>
#include <boost/bind/bind.hpp>
#include <boost/next_prior.hpp>



LtpSessionSender::LtpSessionSender(uint64_t randomInitialSenderCheckpointSerialNumber,
    LtpClientServiceDataToSend && dataToSend, std::shared_ptr<LtpTransmissionRequestUserData> && userDataPtrToTake,
    uint64_t lengthOfRedPart, const uint64_t MTU, const Ltp::session_id_t & sessionId, const uint64_t clientServiceId,
    const boost::posix_time::time_duration & oneWayLightTime, const boost::posix_time::time_duration & oneWayMarginTime,
    LtpTimerManager<Ltp::session_id_t, Ltp::hash_session_id_t>& timeManagerOfCheckpointSerialNumbersRef,
    const NotifyEngineThatThisSenderNeedsDeletedCallback_t & notifyEngineThatThisSenderNeedsDeletedCallback,
    const NotifyEngineThatThisSenderHasProducibleDataFunction_t & notifyEngineThatThisSenderHasProducibleDataFunction,
    const InitialTransmissionCompletedCallback_t & initialTransmissionCompletedCallback, 
    const uint64_t checkpointEveryNthDataPacket, const uint32_t maxRetriesPerSerialNumber) :
    m_timeManagerOfCheckpointSerialNumbersRef(timeManagerOfCheckpointSerialNumbersRef),
    m_receptionClaimIndex(0),
    m_nextCheckpointSerialNumber(randomInitialSenderCheckpointSerialNumber),
    m_dataToSendSharedPtr(std::make_shared<LtpClientServiceDataToSend>(std::move(dataToSend))),
    m_userDataPtr(std::move(userDataPtrToTake)),
    M_LENGTH_OF_RED_PART(lengthOfRedPart),
    m_dataIndexFirstPass(0),
    m_didNotifyForDeletion(false),
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
    m_isFailedSession(false)
{
    m_timerExpiredCallback = boost::bind(&LtpSessionSender::LtpCheckpointTimerExpiredCallback, this, boost::placeholders::_1, boost::placeholders::_2);
    m_notifyEngineThatThisSenderHasProducibleDataFunction(M_SESSION_ID.sessionNumber); //to trigger first pass of red data
}

LtpSessionSender::~LtpSessionSender() {
    //clean up this sending session's active timers within the shared LtpTimerManager
    for (std::set<uint64_t>::const_iterator it = m_checkpointSerialNumberActiveTimersSet.cbegin(); it != m_checkpointSerialNumberActiveTimersSet.cend(); ++it) {
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
            std::cout << "error in LtpSessionSender::~LtpSessionSender: did not delete timer\n";
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
    //std::cout << "LtpCheckpointTimerExpiredCallback timer expired!!! checkpointSerialNumber = " << checkpointSerialNumber << std::endl;

    //keep track of this sending session's active timers within the shared LtpTimerManager
    if (m_checkpointSerialNumberActiveTimersSet.erase(checkpointSerialNumber) == 0) {
        std::cout << "error in LtpSessionSender::LtpCheckpointTimerExpiredCallback: did not erase m_checkpointSerialNumberActiveTimersSet\n";
    }

    ++m_numCheckpointTimerExpiredCallbacks;
    if (userData.size() != sizeof(resend_fragment_t)) {
        std::cerr << "error in LtpSessionSender::LtpCheckpointTimerExpiredCallback: userData.size() != sizeof(resend_fragment_t)\n";
        return;
    }
    resend_fragment_t * const resendFragmentPtr = (resend_fragment_t*)userData.data(); //userData will be 64-bit aligned

    if (resendFragmentPtr->retryCount <= M_MAX_RETRIES_PER_SERIAL_NUMBER) {
        const bool isDiscretionaryCheckpoint = (resendFragmentPtr->flags == LTP_DATA_SEGMENT_TYPE_FLAGS::REDDATA_CHECKPOINT);
        if (isDiscretionaryCheckpoint && LtpFragmentSet::ContainsFragmentEntirely(m_dataFragmentsAckedByReceiver, LtpFragmentSet::data_fragment_t(resendFragmentPtr->offset, (resendFragmentPtr->offset + resendFragmentPtr->length) - 1))) {
            //std::cout << "  Discretionary checkpoint not being resent because its data was already received successfully by the receiver." << std::endl;
            ++m_numDiscretionaryCheckpointsNotResent;
        }
        else {
            //resend 
            ++(resendFragmentPtr->retryCount);
            m_resendFragmentsQueue.push(*resendFragmentPtr);
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

bool LtpSessionSender::NextDataToSend(std::vector<boost::asio::const_buffer>& constBufferVec,
    std::shared_ptr<std::vector<std::vector<uint8_t> > >& underlyingDataToDeleteOnSentCallback,
    std::shared_ptr<LtpClientServiceDataToSend>& underlyingCsDataToDeleteOnSentCallback)
{
    if (!m_nonDataToSend.empty()) { //includes report ack segments
        //std::cout << "sender dequeue\n";
        //highest priority
        underlyingDataToDeleteOnSentCallback = std::make_shared<std::vector<std::vector<uint8_t> > >(1);
        (*underlyingDataToDeleteOnSentCallback)[0] = std::move(m_nonDataToSend.front());
        m_nonDataToSend.pop();
        constBufferVec.resize(1);
        constBufferVec[0] = boost::asio::buffer((*underlyingDataToDeleteOnSentCallback)[0]);
        return true;
    }

    if (!m_resendFragmentsQueue.empty()) {
        //std::cout << "resend fragment\n";
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
            const uint8_t * const resendFragmentPtr = (uint8_t*)&resendFragment;
            //std::cout << "resend csn " << resendFragment.checkpointSerialNumber << std::endl;

            // within a session would normally be LtpTimerManager<uint64_t, std::hash<uint64_t> > m_timeManagerOfCheckpointSerialNumbers;
            // but now sharing a single LtpTimerManager among all sessions, so use a
            // LtpTimerManager<Ltp::session_id_t, Ltp::hash_session_id_t> (which has hash map hashing function support)
            // such that: 
            //  sessionOriginatorEngineId = CHECKPOINT serial number
            //  sessionNumber = the session number
            //  since this is a sender, the real sessionOriginatorEngineId is constant among all sending sessions and is not needed
            const Ltp::session_id_t checkpointSerialNumberPlusSessionNumber(resendFragment.checkpointSerialNumber, M_SESSION_ID.sessionNumber);

            if (m_timeManagerOfCheckpointSerialNumbersRef.StartTimer(checkpointSerialNumberPlusSessionNumber, &m_timerExpiredCallback, std::vector<uint8_t>(resendFragmentPtr, resendFragmentPtr + sizeof(resendFragment)))) {
                //keep track of this sending session's active timers within the shared LtpTimerManager
                if (!m_checkpointSerialNumberActiveTimersSet.insert(resendFragment.checkpointSerialNumber).second) {
                    std::cout << "error in LtpSessionSender::NextDataToSend: did not insert m_checkpointSerialNumberActiveTimersSet\n";
                }
            }
        }
        else {
            meta.checkpointSerialNumber = NULL;
            meta.reportSerialNumber = NULL;
        }
        underlyingDataToDeleteOnSentCallback = std::make_shared<std::vector<std::vector<uint8_t> > >(1); //2 would be needed in case of trailer extensions (but not used here)
        Ltp::GenerateLtpHeaderPlusDataSegmentMetadata((*underlyingDataToDeleteOnSentCallback)[0], resendFragment.flags,
            M_SESSION_ID, meta, NULL, 0);
        constBufferVec.resize(2); //3 would be needed in case of trailer extensions (but not used here)
        constBufferVec[0] = boost::asio::buffer((*underlyingDataToDeleteOnSentCallback)[0]);
        //std::cout << "rf o: " << resendFragment.offset << " l: " << resendFragment.length << " flags: " << (int)resendFragment.flags << std::endl;
        //std::cout << (int)(*(m_dataToSend.data() + resendFragment.offset)) << std::endl;
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
                //std::cout << "send sync csn " << cp << std::endl;
                LtpSessionSender::resend_fragment_t resendFragment(m_dataIndexFirstPass, bytesToSendRed, cp, rsn, flags);
                const uint8_t * const resendFragmentPtr = (uint8_t*)&resendFragment;

                // within a session would normally be LtpTimerManager<uint64_t, std::hash<uint64_t> > m_timeManagerOfCheckpointSerialNumbers;
                // but now sharing a single LtpTimerManager among all sessions, so use a
                // LtpTimerManager<Ltp::session_id_t, Ltp::hash_session_id_t> (which has hash map hashing function support)
                // such that: 
                //  sessionOriginatorEngineId = CHECKPOINT serial number
                //  sessionNumber = the session number
                //  since this is a sender, the real sessionOriginatorEngineId is constant among all sending sessions and is not needed
                const Ltp::session_id_t checkpointSerialNumberPlusSessionNumber(cp, M_SESSION_ID.sessionNumber);

                if (m_timeManagerOfCheckpointSerialNumbersRef.StartTimer(checkpointSerialNumberPlusSessionNumber, &m_timerExpiredCallback, std::vector<uint8_t>(resendFragmentPtr, resendFragmentPtr + sizeof(resendFragment)))) {
                    //keep track of this sending session's active timers within the shared LtpTimerManager
                    if (!m_checkpointSerialNumberActiveTimersSet.insert(cp).second) {
                        std::cout << "error in LtpSessionSender::NextDataToSend: did not insert cp into m_checkpointSerialNumberActiveTimersSet\n";
                    }
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
            //std::cout << "green m_dataToSend[" << m_dataIndexFirstPass << "]=" << (int)(m_dataToSend.data()[m_dataIndexFirstPass]) << "\n";
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
                //std::cout << "it->beginIndex " << it->beginIndex << " it->endIndex " << it->endIndex << std::endl;
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
    //std::cout << "sender queue rsn " << reportSegment.reportSerialNumber << "\n";
    Ltp::GenerateReportAcknowledgementSegmentLtpPacket(m_nonDataToSend.back(),
        M_SESSION_ID, reportSegment.reportSerialNumber, NULL, NULL);

    //If the RS segment is redundant -- i.e., either the indicated session is unknown
    //(for example, the RS segment is received after the session has been
    //completed or canceled) or the RS segment's report serial number
    //matches that of an RS segment that has already been received and
    //processed -- then no further action is taken.
    if (m_reportSegmentSerialNumbersReceivedSet.insert(reportSegment.reportSerialNumber).second == false) { //serial number was not inserted (already exists)
        //std::cout << "serial number was not inserted (already exists)\n";
        m_notifyEngineThatThisSenderHasProducibleDataFunction(M_SESSION_ID.sessionNumber); //from the m_nonDataToSend
        return; //no work to do.. ignore this redundant report segment
    }

    //If the report's checkpoint serial number is not zero, then the
    //countdown timer associated with the indicated checkpoint segment is deleted.
    if (reportSegment.checkpointSerialNumber) {
        //std::cout << "delete rs's csn " << reportSegment.checkpointSerialNumber << std::endl;

        // within a session would normally be LtpTimerManager<uint64_t, std::hash<uint64_t> > m_timeManagerOfCheckpointSerialNumbers;
        // but now sharing a single LtpTimerManager among all sessions, so use a
        // LtpTimerManager<Ltp::session_id_t, Ltp::hash_session_id_t> (which has hash map hashing function support)
        // such that: 
        //  sessionOriginatorEngineId = CHECKPOINT serial number
        //  sessionNumber = the session number
        //  since this is a sender, the real sessionOriginatorEngineId is constant among all sending sessions and is not needed
        const Ltp::session_id_t checkpointSerialNumberPlusSessionNumber(reportSegment.checkpointSerialNumber, M_SESSION_ID.sessionNumber);

        if (m_timeManagerOfCheckpointSerialNumbersRef.DeleteTimer(checkpointSerialNumberPlusSessionNumber)) {
            //keep track of this sending session's active timers within the shared LtpTimerManager
            if (m_checkpointSerialNumberActiveTimersSet.erase(reportSegment.checkpointSerialNumber) == 0) {
                std::cout << "error in LtpSessionSender::ReportSegmentReceivedCallback: did not erase m_checkpointSerialNumberActiveTimersSet\n";
            }
        }
    }


    LtpFragmentSet::AddReportSegmentToFragmentSet(m_dataFragmentsAckedByReceiver, reportSegment);
    //std::cout << "rs: " << reportSegment << std::endl;
    //std::cout << "acked segments: "; LtpFragmentSet::PrintFragmentSet(m_dataFragmentsAckedByReceiver); std::cout << std::endl;
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
    //std::cout << "M_LENGTH_OF_RED_PART " << M_LENGTH_OF_RED_PART << " m_dataFragmentsAckedByReceiver.size() " << m_dataFragmentsAckedByReceiver.size() << std::endl;
    //std::cout << "m_dataIndexFirstPass " << m_dataIndexFirstPass << " m_dataToSend.size() " << m_dataToSend.size() << std::endl;
    if ((m_dataIndexFirstPass == m_dataToSendSharedPtr->size()) && (m_dataFragmentsAckedByReceiver.size() == 1)) {
        std::set<LtpFragmentSet::data_fragment_t>::const_iterator it = m_dataFragmentsAckedByReceiver.cbegin();
        //std::cout << "it->beginIndex " << it->beginIndex << " it->endIndex " << it->endIndex << std::endl;
        if ((it->beginIndex == 0) && (it->endIndex >= (M_LENGTH_OF_RED_PART - 1))) { //>= in case some green data was acked
            if (!m_didNotifyForDeletion) {
                m_didNotifyForDeletion = true;
                m_notifyEngineThatThisSenderNeedsDeletedCallback(M_SESSION_ID, false, CANCEL_SEGMENT_REASON_CODES::RESERVED, m_userDataPtr);
            }
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
    LtpFragmentSet::AddReportSegmentToFragmentSetNeedingResent(fragmentsNeedingResent, reportSegment);
    //std::cout << "need resent: "; LtpFragmentSet::PrintFragmentSet(fragmentsNeedingResent); std::cout << std::endl;
    //std::cout << "resend\n";
    for (std::set<LtpFragmentSet::data_fragment_t>::const_iterator it = fragmentsNeedingResent.cbegin(); it != fragmentsNeedingResent.cend(); ++it) {
        //std::cout << "h1\n";
        const bool isLastFragmentNeedingResent = (boost::next(it) == fragmentsNeedingResent.cend());
        for (uint64_t dataIndex = it->beginIndex; dataIndex <= it->endIndex; ) {
            //std::cout << "h2 " << "isLastFragmentNeedingResent " << isLastFragmentNeedingResent << "\n";
            uint64_t bytesToSendRed = std::min((it->endIndex - dataIndex) + 1, M_MTU);
            if ((bytesToSendRed + dataIndex) > M_LENGTH_OF_RED_PART) {
                std::cerr << "critical error gt length red part\n";
            }
            //std::cout << "(dataIndex + bytesToSendRed) " << (dataIndex + bytesToSendRed) <<  " it->endIndex " << it->endIndex << "\n";
            const bool isLastPacketNeedingResent = ((isLastFragmentNeedingResent) && ((dataIndex + bytesToSendRed) == (it->endIndex + 1)));
            const bool isEndOfRedPart = ((bytesToSendRed + dataIndex) == M_LENGTH_OF_RED_PART);
            if (isEndOfRedPart && !isLastPacketNeedingResent) {
                std::cerr << "critical error: end of red part but not last packet being resent\n";
            }
            
            uint64_t checkpointSerialNumber = 0; //dont care
            LTP_DATA_SEGMENT_TYPE_FLAGS flags = LTP_DATA_SEGMENT_TYPE_FLAGS::REDDATA;
            if (isLastPacketNeedingResent) {
                //std::cout << "h3\n";
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
            
            m_resendFragmentsQueue.emplace(dataIndex, bytesToSendRed, checkpointSerialNumber, reportSegment.reportSerialNumber, flags);
            dataIndex += bytesToSendRed;
        }
    }

    if (!m_didNotifyForDeletion) {
        m_notifyEngineThatThisSenderHasProducibleDataFunction(M_SESSION_ID.sessionNumber);
    }
}
