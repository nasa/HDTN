#include "LtpSessionSender.h"
#include <iostream>
#include <inttypes.h>
#include <boost/bind.hpp>
#include <boost/make_shared.hpp>

LtpSessionSender::LtpSessionSender(uint64_t randomInitialSenderCheckpointSerialNumber,
    std::vector<uint8_t> && dataToSend, uint64_t lengthOfRedPart, const uint64_t MTU, const Ltp::session_id_t & sessionId, const uint64_t clientServiceId) :
    m_receptionClaimIndex(0),
    m_nextCheckpointSerialNumber(randomInitialSenderCheckpointSerialNumber),
    m_dataToSend(std::move(dataToSend)),
    M_LENGTH_OF_RED_PART(lengthOfRedPart),
    m_dataIndexFirstPass(0),
    M_MTU(MTU),
    M_SESSION_ID(sessionId),
    M_CLIENT_SERVICE_ID(clientServiceId)
{

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
        meta.checkpointSerialNumber = &resendFragment.checkpointSerialNumber;
        meta.reportSerialNumber = &resendFragment.reportSerialNumber;
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

            LTP_DATA_SEGMENT_TYPE_FLAGS flags = LTP_DATA_SEGMENT_TYPE_FLAGS::REDDATA;
            uint64_t cp;
            uint64_t * checkpointSerialNumber = NULL;
            uint64_t rsn = 0; //0 in this state because not a response to an RS
            uint64_t * reportSerialNumber = NULL;
            if (isEndOfRedPart) {
                flags = LTP_DATA_SEGMENT_TYPE_FLAGS::REDDATA_CHECKPOINT_ENDOFREDPART;
                cp = m_nextCheckpointSerialNumber++;
                checkpointSerialNumber = &cp;
                reportSerialNumber = &rsn;
                const bool isEndOfBlock = (M_LENGTH_OF_RED_PART == m_dataToSend.size());
                if (isEndOfBlock) {
                    flags = LTP_DATA_SEGMENT_TYPE_FLAGS::REDDATA_CHECKPOINT_ENDOFREDPART_ENDOFBLOCK;
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
    LtpFragmentMap::AddReportSegmentToFragmentSet(m_dataFragmentsAckedByReceiver, reportSegment);
    m_nonDataToSend.emplace_back();
    Ltp::GenerateReportAcknowledgementSegmentLtpPacket(m_nonDataToSend.back(),
        M_SESSION_ID, reportSegment.reportSerialNumber, NULL, NULL);

    std::set<LtpFragmentMap::data_fragment_t> fragmentsNeedingResent;
    LtpFragmentMap::AddReportSegmentToFragmentSetNeedingResent(fragmentsNeedingResent, reportSegment);
    for (std::set<LtpFragmentMap::data_fragment_t>::const_iterator it = fragmentsNeedingResent.cbegin(); it != fragmentsNeedingResent.cend(); ++it) {
        for (uint64_t dataIndex = it->beginIndex; dataIndex <= it->endIndex; ) {
            uint64_t bytesToSendRed = std::min((it->endIndex - dataIndex) + 1, M_MTU);
            if ((bytesToSendRed + dataIndex) > M_LENGTH_OF_RED_PART) {
                std::cerr << "critical error gt length red part\n";
            }
            const bool isEndOfRedPart = ((bytesToSendRed + dataIndex) == M_LENGTH_OF_RED_PART);
            
            LTP_DATA_SEGMENT_TYPE_FLAGS flags = LTP_DATA_SEGMENT_TYPE_FLAGS::REDDATA_CHECKPOINT;
            if (isEndOfRedPart) {
                flags = LTP_DATA_SEGMENT_TYPE_FLAGS::REDDATA_CHECKPOINT_ENDOFREDPART;
                const bool isEndOfBlock = (M_LENGTH_OF_RED_PART == m_dataToSend.size());
                if (isEndOfBlock) {
                    flags = LTP_DATA_SEGMENT_TYPE_FLAGS::REDDATA_CHECKPOINT_ENDOFREDPART_ENDOFBLOCK;
                }
            }
            
            m_resendFragmentsList.emplace_back(dataIndex, bytesToSendRed, reportSegment.checkpointSerialNumber, reportSegment.reportSerialNumber, flags);
            dataIndex += bytesToSendRed;
        }
    }
}
