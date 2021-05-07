#include "LtpSessionReceiver.h"
#include <iostream>
#include <inttypes.h>
#include <boost/bind.hpp>
#include <boost/make_shared.hpp>

LtpSessionReceiver::LtpSessionReceiver(uint64_t randomNextReportSegmentReportSerialNumber, const uint64_t MTU,
    const Ltp::session_id_t & sessionId, const uint64_t clientServiceId) :
    m_nextReportSegmentReportSerialNumber(randomNextReportSegmentReportSerialNumber),
    M_MTU(MTU),
    M_SESSION_ID(sessionId),
    M_CLIENT_SERVICE_ID(clientServiceId),
    m_lengthOfRedPart(UINT64_MAX)
{

}

bool LtpSessionReceiver::NextDataToSend(std::vector<boost::asio::const_buffer> & constBufferVec, boost::shared_ptr<std::vector<std::vector<uint8_t> > > & underlyingDataToDeleteOnSentCallback) {
    if (!m_nonDataToSend.empty()) { //includes report segments
        //highest priority
        underlyingDataToDeleteOnSentCallback = boost::make_shared<std::vector<std::vector<uint8_t> > >(1);
        (*underlyingDataToDeleteOnSentCallback)[0] = std::move(m_nonDataToSend.front());
        m_nonDataToSend.pop_front();
        constBufferVec.resize(1);
        constBufferVec[0] = boost::asio::buffer((*underlyingDataToDeleteOnSentCallback)[0]);
        return true;
    }
    return false;
}

void LtpSessionReceiver::CancelSegmentReceivedCallback(CANCEL_SEGMENT_REASON_CODES reasonCode, 
    Ltp::ltp_extensions_t & headerExtensions, Ltp::ltp_extensions_t & trailerExtensions)
{

}

void LtpSessionReceiver::CancelAcknowledgementSegmentReceivedCallback(Ltp::ltp_extensions_t & headerExtensions, Ltp::ltp_extensions_t & trailerExtensions)
{

}

void LtpSessionReceiver::ReportAcknowledgementSegmentReceivedCallback(uint64_t reportSerialNumberBeingAcknowledged,
    Ltp::ltp_extensions_t & headerExtensions, Ltp::ltp_extensions_t & trailerExtensions)
{
    std::map<uint64_t, Ltp::report_segment_t>::iterator reportSegmentIt = m_mapReportSegmentsSent.find(reportSerialNumberBeingAcknowledged);
    if (reportSegmentIt != m_mapReportSegmentsSent.end()) { //found
        m_mapReportSegmentsSent.erase(reportSegmentIt);
    }
    else {
        std::cerr << "error in LtpSessionReceiver::ReportAcknowledgementSegmentReceivedCallback: cannot find report segment\n";
    }
    
}


void LtpSessionReceiver::DataSegmentReceivedCallback(uint8_t segmentTypeFlags,
    std::vector<uint8_t> & clientServiceDataVec, const Ltp::data_segment_metadata_t & dataSegmentMetadata,
    Ltp::ltp_extensions_t & headerExtensions, Ltp::ltp_extensions_t & trailerExtensions, const RedPartReceptionCallback_t & redPartReceptionCallback)
{
    const uint64_t minVectorSize = dataSegmentMetadata.offset + dataSegmentMetadata.length;
    if (m_dataReceived.size() < minVectorSize) {
        m_dataReceived.resize(minVectorSize);
    }
    if (dataSegmentMetadata.length != clientServiceDataVec.size()) {
        std::cerr << "error dataSegmentMetadata.length != clientServiceDataVec.size()\n";
    }
    memcpy(m_dataReceived.data() + dataSegmentMetadata.offset, clientServiceDataVec.data(), dataSegmentMetadata.length);

    bool isRedData = (segmentTypeFlags <= 3);
    bool isEndOfBlock = ((segmentTypeFlags & 3) == 3);
    if (isRedData) {
        bool isRedCheckpoint = (segmentTypeFlags != 0);
        bool isEndOfRedPart = (segmentTypeFlags & 2);
        LtpFragmentMap::InsertFragment(m_receivedDataFragmentsSet, 
            LtpFragmentMap::data_fragment_t(dataSegmentMetadata.offset, (dataSegmentMetadata.offset + dataSegmentMetadata.length) - 1));
        //LtpFragmentMap::PrintFragmentSet(m_receivedDataFragmentsSet);
        //std::cout << "offset: " << dataSegmentMetadata.offset << " l: " << dataSegmentMetadata.length << " d: " << (int)clientServiceDataVec[0] << std::endl;
        if (isEndOfRedPart) {
            m_lengthOfRedPart = dataSegmentMetadata.offset + dataSegmentMetadata.length;
        }
        if (isRedCheckpoint) {
            Ltp::report_segment_t reportSegment;
            if (!LtpFragmentMap::PopulateReportSegment(m_receivedDataFragmentsSet, reportSegment)) {
                std::cerr << "error in LtpSessionReceiver::DataSegmentReceivedCallback: cannot populate report segment\n";
            }

            //The value of the checkpoint serial number MUST be zero if the
            //report segment is NOT a response to reception of a checkpoint,
            //i.e., the reception report is asynchronous; otherwise, it MUST be
            //the checkpoint serial number of the checkpoint that caused the RS
            //to be issued.
            reportSegment.checkpointSerialNumber = *dataSegmentMetadata.checkpointSerialNumber;

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
            m_nonDataToSend.emplace_back();
            Ltp::GenerateReportSegmentLtpPacket(m_nonDataToSend.back(),
                M_SESSION_ID, reportSegment, NULL, NULL);

            m_mapReportSegmentsSent[rsn] = std::move(reportSegment);
        }
        if ((m_lengthOfRedPart != UINT64_MAX) && (m_receivedDataFragmentsSet.size() == 1)) {
            std::set<LtpFragmentMap::data_fragment_t>::const_iterator it = m_receivedDataFragmentsSet.cbegin();
            if ((it->beginIndex == 0) && (it->endIndex == (m_lengthOfRedPart - 1))) {
                if (redPartReceptionCallback) {
                    redPartReceptionCallback(M_SESSION_ID,
                        m_dataReceived, m_lengthOfRedPart, dataSegmentMetadata.clientServiceId, isEndOfBlock);
                }
            }
        }
    }
    
}
