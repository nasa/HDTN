#include "LtpSessionSender.h"
#include <iostream>
#include <inttypes.h>
#include <boost/bind.hpp>
#include <boost/make_shared.hpp>
#include <boost/next_prior.hpp>



LtpSessionSender::LtpSessionSender(uint64_t randomInitialSenderCheckpointSerialNumber,
    std::vector<uint8_t> && dataToSend, uint64_t lengthOfRedPart, const uint64_t MTU, const Ltp::session_id_t & sessionId, const uint64_t clientServiceId,
    const boost::posix_time::time_duration & oneWayLightTime, const boost::posix_time::time_duration & oneWayMarginTime, boost::asio::io_service & ioServiceRef, 
    const NotifyEngineThatThisSenderNeedsDeletedCallback_t & notifyEngineThatThisSenderNeedsDeletedCallback,
    const NotifyEngineThatThisSendersTimersProducedDataFunction_t & notifyEngineThatThisSendersTimersProducedDataFunction,
    const InitialTransmissionCompletedCallback_t & initialTransmissionCompletedCallback, 
    const uint64_t checkpointEveryNthDataPacket) :
    m_timeManagerOfCheckpointSerialNumbers(ioServiceRef, oneWayLightTime, oneWayMarginTime, boost::bind(&LtpSessionSender::LtpCheckpointTimerExpiredCallback, this, boost::placeholders::_1, boost::placeholders::_2)),
    m_receptionClaimIndex(0),
    m_nextCheckpointSerialNumber(randomInitialSenderCheckpointSerialNumber),
    m_dataToSend(std::move(dataToSend)),
    M_LENGTH_OF_RED_PART(lengthOfRedPart),
    m_dataIndexFirstPass(0),
    M_MTU(MTU),
    M_SESSION_ID(sessionId),
    M_CLIENT_SERVICE_ID(clientServiceId),
    M_CHECKPOINT_EVERY_NTH_DATA_PACKET(checkpointEveryNthDataPacket),
    m_checkpointEveryNthDataPacketCounter(checkpointEveryNthDataPacket),
    m_ioServiceRef(ioServiceRef),
    m_notifyEngineThatThisSenderNeedsDeletedCallback(notifyEngineThatThisSenderNeedsDeletedCallback),
    m_notifyEngineThatThisSendersTimersProducedDataFunction(notifyEngineThatThisSendersTimersProducedDataFunction),
    m_initialTransmissionCompletedCallback(initialTransmissionCompletedCallback),
    m_numTimerExpiredCallbacks(0)
{

}

void LtpSessionSender::LtpCheckpointTimerExpiredCallback(uint64_t checkpointSerialNumber, std::vector<uint8_t> & userData) {
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
    //std::cout << "LtpCheckpointTimerExpiredCallback timer expired!!!\n";
    ++m_numTimerExpiredCallbacks;
    if (userData.size() != sizeof(resend_fragment_t)) {
        std::cerr << "error in LtpSessionSender::LtpCheckpointTimerExpiredCallback: userData.size() != sizeof(resend_fragment_t)\n";
        return;
    }
    resend_fragment_t resendFragment;
    memcpy(&resendFragment, userData.data(), sizeof(resendFragment));

    if (resendFragment.retryCount <= 5) {
        //resend 
        ++resendFragment.retryCount;
        m_resendFragmentsList.push_back(resendFragment);
        m_notifyEngineThatThisSendersTimersProducedDataFunction();
    }
    else {
        m_notifyEngineThatThisSenderNeedsDeletedCallback(M_SESSION_ID, true, CANCEL_SEGMENT_REASON_CODES::RLEXC);
    }
}

bool LtpSessionSender::NextDataToSend(std::vector<boost::asio::const_buffer> & constBufferVec, boost::shared_ptr<std::vector<std::vector<uint8_t> > > & underlyingDataToDeleteOnSentCallback) {
    if (!m_nonDataToSend.empty()) { //includes report ack segments
        //highest priority
        underlyingDataToDeleteOnSentCallback = boost::make_shared<std::vector<std::vector<uint8_t> > >(1);
        (*underlyingDataToDeleteOnSentCallback)[0] = std::move(m_nonDataToSend.front());
        m_nonDataToSend.pop_front();
        constBufferVec.resize(1);
        constBufferVec[0] = boost::asio::buffer((*underlyingDataToDeleteOnSentCallback)[0]);
        return true;
    }

    if (!m_resendFragmentsList.empty()) {
        //std::cout << "resend fragment\n";
        LtpSessionSender::resend_fragment_t & resendFragment = m_resendFragmentsList.front();
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
            m_timeManagerOfCheckpointSerialNumbers.StartTimer(resendFragment.checkpointSerialNumber, std::vector<uint8_t>(resendFragmentPtr, resendFragmentPtr + sizeof(resendFragment)));
        }
        else {
            meta.checkpointSerialNumber = NULL;
            meta.reportSerialNumber = NULL;
        }
        underlyingDataToDeleteOnSentCallback = boost::make_shared<std::vector<std::vector<uint8_t> > >(2); //2 in case of trailer extensions
        Ltp::GenerateLtpHeaderPlusDataSegmentMetadata((*underlyingDataToDeleteOnSentCallback)[0], resendFragment.flags,
            M_SESSION_ID, meta, NULL, 0);
        constBufferVec.resize(3); //3 in case of trailer
        constBufferVec[0] = boost::asio::buffer((*underlyingDataToDeleteOnSentCallback)[0]);
        //std::cout << "rf o: " << resendFragment.offset << " l: " << resendFragment.length << " flags: " << (int)resendFragment.flags << std::endl;
        //std::cout << (int)(*(m_dataToSend.data() + resendFragment.offset)) << std::endl;
        constBufferVec[1] = boost::asio::buffer(m_dataToSend.data() + resendFragment.offset, resendFragment.length);
        m_resendFragmentsList.pop_front();
        return true;
    }

    if (m_dataIndexFirstPass < m_dataToSend.size()) {
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
                    const bool isEndOfBlock = (M_LENGTH_OF_RED_PART == m_dataToSend.size());
                    if (isEndOfBlock) {
                        flags = LTP_DATA_SEGMENT_TYPE_FLAGS::REDDATA_CHECKPOINT_ENDOFREDPART_ENDOFBLOCK;
                    }
                }
                std::cout << "send sync csn " << cp << std::endl;
                LtpSessionSender::resend_fragment_t resendFragment(m_dataIndexFirstPass, bytesToSendRed, cp, rsn, flags);
                const uint8_t * const resendFragmentPtr = (uint8_t*)&resendFragment;
                m_timeManagerOfCheckpointSerialNumbers.StartTimer(cp, std::vector<uint8_t>(resendFragmentPtr, resendFragmentPtr + sizeof(resendFragment)));
            }

            Ltp::data_segment_metadata_t meta;
            meta.clientServiceId = M_CLIENT_SERVICE_ID;
            meta.offset = m_dataIndexFirstPass;
            meta.length = bytesToSendRed;
            meta.checkpointSerialNumber = checkpointSerialNumber;
            meta.reportSerialNumber = reportSerialNumber;
            underlyingDataToDeleteOnSentCallback = boost::make_shared<std::vector<std::vector<uint8_t> > >(2); //2 in case of trailer extensions
            Ltp::GenerateLtpHeaderPlusDataSegmentMetadata((*underlyingDataToDeleteOnSentCallback)[0], flags,
                M_SESSION_ID, meta, NULL, 0);
            constBufferVec.resize(3); //3 in case of trailer
            constBufferVec[0] = boost::asio::buffer((*underlyingDataToDeleteOnSentCallback)[0]);
            constBufferVec[1] = boost::asio::buffer(m_dataToSend.data() + m_dataIndexFirstPass, bytesToSendRed);
            m_dataIndexFirstPass += bytesToSendRed;
        }
        else { //first pass of green data send
            uint64_t bytesToSendGreen = std::min(m_dataToSend.size() - m_dataIndexFirstPass, M_MTU);
            const bool isEndOfBlock = ((bytesToSendGreen + m_dataIndexFirstPass) == m_dataToSend.size());
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
            underlyingDataToDeleteOnSentCallback = boost::make_shared<std::vector<std::vector<uint8_t> > >(2); //2 in case of trailer extensions
            Ltp::GenerateLtpHeaderPlusDataSegmentMetadata((*underlyingDataToDeleteOnSentCallback)[0], flags,
                M_SESSION_ID, meta, NULL, 0);
            constBufferVec.resize(3); //3 in case of trailer
            constBufferVec[0] = boost::asio::buffer((*underlyingDataToDeleteOnSentCallback)[0]);
            constBufferVec[1] = boost::asio::buffer(m_dataToSend.data() + m_dataIndexFirstPass, bytesToSendGreen);
            m_dataIndexFirstPass += bytesToSendGreen;
        }
        if (m_dataIndexFirstPass == m_dataToSend.size()) {
            m_initialTransmissionCompletedCallback(M_SESSION_ID);
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
    m_nonDataToSend.emplace_back();
    Ltp::GenerateReportAcknowledgementSegmentLtpPacket(m_nonDataToSend.back(),
        M_SESSION_ID, reportSegment.reportSerialNumber, NULL, NULL);

    //If the RS segment is redundant -- i.e., either the indicated session is unknown
    //(for example, the RS segment is received after the session has been
    //completed or canceled) or the RS segment's report serial number
    //matches that of an RS segment that has already been received and
    //processed -- then no further action is taken.
    if (m_reportSegmentSerialNumbersReceivedSet.insert(reportSegment.reportSerialNumber).second == false) { //serial number was not inserted (already exists)
        return; //no work to do.. ignore this redundant report segment
    }

    //If the report's checkpoint serial number is not zero, then the
    //countdown timer associated with the indicated checkpoint segment is deleted.
    if (reportSegment.checkpointSerialNumber) {
        std::cout << "delete rs's csn " << reportSegment.checkpointSerialNumber << std::endl;
        m_timeManagerOfCheckpointSerialNumbers.DeleteTimer(reportSegment.checkpointSerialNumber);
    }


    LtpFragmentMap::AddReportSegmentToFragmentSet(m_dataFragmentsAckedByReceiver, reportSegment);
    //std::cout << "rs: " << reportSegment << std::endl;
    //std::cout << "acked segments: "; LtpFragmentMap::PrintFragmentSet(m_dataFragmentsAckedByReceiver); std::cout << std::endl;
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
    if ((m_dataIndexFirstPass == m_dataToSend.size()) && (m_dataFragmentsAckedByReceiver.size() == 1)) {
        std::set<LtpFragmentMap::data_fragment_t>::const_iterator it = m_dataFragmentsAckedByReceiver.cbegin();
        //std::cout << "it->beginIndex " << it->beginIndex << " it->endIndex " << it->endIndex << std::endl;
        if ((it->beginIndex == 0) && (it->endIndex >= (M_LENGTH_OF_RED_PART - 1))) { //>= in case some green data was acked
            if (m_notifyEngineThatThisSenderNeedsDeletedCallback) {
                m_notifyEngineThatThisSenderNeedsDeletedCallback(M_SESSION_ID, false, CANCEL_SEGMENT_REASON_CODES::RESERVED);
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
    std::set<LtpFragmentMap::data_fragment_t> fragmentsNeedingResent;
    LtpFragmentMap::AddReportSegmentToFragmentSetNeedingResent(fragmentsNeedingResent, reportSegment);
    //std::cout << "need resent: "; LtpFragmentMap::PrintFragmentSet(fragmentsNeedingResent); std::cout << std::endl;
    //std::cout << "resend\n";
    for (std::set<LtpFragmentMap::data_fragment_t>::const_iterator it = fragmentsNeedingResent.cbegin(); it != fragmentsNeedingResent.cend(); ++it) {
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
                    const bool isEndOfBlock = (M_LENGTH_OF_RED_PART == m_dataToSend.size());
                    if (isEndOfBlock) {
                        flags = LTP_DATA_SEGMENT_TYPE_FLAGS::REDDATA_CHECKPOINT_ENDOFREDPART_ENDOFBLOCK;
                    }
                }
            }
            
            m_resendFragmentsList.emplace_back(dataIndex, bytesToSendRed, checkpointSerialNumber, reportSegment.reportSerialNumber, flags);
            dataIndex += bytesToSendRed;
        }
    }
}
