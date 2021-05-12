#include "LtpSessionSender.h"
#include <iostream>
#include <inttypes.h>
#include <boost/bind.hpp>
#include <boost/make_shared.hpp>
#include <boost/next_prior.hpp>

LtpSessionSender::LtpSessionSender(uint64_t randomInitialSenderCheckpointSerialNumber,
    std::vector<uint8_t> && dataToSend, uint64_t lengthOfRedPart, const uint64_t MTU, const Ltp::session_id_t & sessionId, const uint64_t clientServiceId,
    const boost::posix_time::time_duration & oneWayLightTime, const boost::posix_time::time_duration & oneWayMarginTime, boost::asio::io_service & ioServiceRef, const uint64_t checkpointEveryNthDataPacket) :
    m_timeManagerOfCheckpointSerialNumbers(ioServiceRef, oneWayLightTime, oneWayMarginTime, boost::bind(&LtpSessionSender::LtpCheckpointTimerExpiredCallback, this, boost::placeholders::_1)),
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
    m_ioServiceRef(ioServiceRef)
{

}

void LtpSessionSender::LtpCheckpointTimerExpiredCallback(uint64_t checkpointSerialNumber) {
    
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
        return true;
    }
    return false;
}

void LtpSessionSender::CancelSegmentReceivedCallback(CANCEL_SEGMENT_REASON_CODES reasonCode, Ltp::ltp_extensions_t & headerExtensions, Ltp::ltp_extensions_t & trailerExtensions)
{

}

void LtpSessionSender::CancelAcknowledgementSegmentReceivedCallback(Ltp::ltp_extensions_t & headerExtensions, Ltp::ltp_extensions_t & trailerExtensions)
{

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
        //TODO DELETE TIMER
    }


    LtpFragmentMap::AddReportSegmentToFragmentSet(m_dataFragmentsAckedByReceiver, reportSegment);
    
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
