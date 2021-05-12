#include "LtpSessionReceiver.h"
#include <iostream>
#include <inttypes.h>
#include <boost/bind.hpp>
#include <boost/make_shared.hpp>

LtpSessionReceiver::LtpSessionReceiver(uint64_t randomNextReportSegmentReportSerialNumber, const uint64_t MTU,
    const Ltp::session_id_t & sessionId, const uint64_t clientServiceId,
    const boost::posix_time::time_duration & oneWayLightTime, const boost::posix_time::time_duration & oneWayMarginTime, boost::asio::io_service & ioServiceRef) :
    m_timeManagerOfReportSerialNumbers(ioServiceRef, oneWayLightTime, oneWayMarginTime, boost::bind(&LtpSessionReceiver::LtpReportSegmentTimerExpiredCallback, this, boost::placeholders::_1)),
    m_nextReportSegmentReportSerialNumber(randomNextReportSegmentReportSerialNumber),
    M_MTU(MTU),
    M_SESSION_ID(sessionId),
    M_CLIENT_SERVICE_ID(clientServiceId),
    m_lengthOfRedPart(UINT64_MAX),
    m_ioServiceRef(ioServiceRef)
{

}

void LtpSessionReceiver::LtpReportSegmentTimerExpiredCallback(uint64_t reportSerialNumber) {

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
    std::set<uint64_t>::iterator reportSegmentIt = m_reportSegmentReportSerialNumbersUnackedSet.find(reportSerialNumberBeingAcknowledged);
    if (reportSegmentIt != m_reportSegmentReportSerialNumbersUnackedSet.end()) { //found
        m_reportSegmentReportSerialNumbersUnackedSet.erase(reportSegmentIt);
    }
    else {
        std::cerr << "error in LtpSessionReceiver::ReportAcknowledgementSegmentReceivedCallback: cannot find report segment\n";
    }
    
}


void LtpSessionReceiver::DataSegmentReceivedCallback(uint8_t segmentTypeFlags,
    std::vector<uint8_t> & clientServiceDataVec, const Ltp::data_segment_metadata_t & dataSegmentMetadata,
    Ltp::ltp_extensions_t & headerExtensions, Ltp::ltp_extensions_t & trailerExtensions, const RedPartReceptionCallback_t & redPartReceptionCallback)
{
    const uint64_t offsetPlusLength = dataSegmentMetadata.offset + dataSegmentMetadata.length;
    if (m_dataReceived.size() < offsetPlusLength) {
        m_dataReceived.resize(offsetPlusLength);
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
            LtpFragmentMap::data_fragment_t(dataSegmentMetadata.offset, offsetPlusLength - 1));
        //LtpFragmentMap::PrintFragmentSet(m_receivedDataFragmentsSet);
        //std::cout << "offset: " << dataSegmentMetadata.offset << " l: " << dataSegmentMetadata.length << " d: " << (int)clientServiceDataVec[0] << std::endl;
        if (isEndOfRedPart) {
            m_lengthOfRedPart = offsetPlusLength;
        }
        if (isRedCheckpoint) {
            if ((dataSegmentMetadata.checkpointSerialNumber == NULL) || (dataSegmentMetadata.reportSerialNumber == NULL)) {
                std::cerr << "error in LtpSessionReceiver::DataSegmentReceivedCallback: checkpoint but NULL values\n";
                return;
            }
            //std::cout << "csn rxed: " << *dataSegmentMetadata.checkpointSerialNumber << std::endl;
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
            //std::cout << "UB: " << upperBound << std::endl;

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
                    //std::cout << "1LB: " << lowerBound << std::endl;
                }
                else {
                    std::cerr << "error in LtpSessionReceiver::DataSegmentReceivedCallback: cannot find report segment\n";
                }
            }
            else {
                //- If the checkpoint was not issued in response to a report
                //segment, this report is a "primary" reception report.
                if (m_mapPrimaryReportSegmentsSent.empty()) {
                    //The lower bound of the first primary reception report issued for any session MUST be zero.
                    lowerBound = 0;
                    //std::cout << "2LB: " << lowerBound << std::endl;
                }
                else {
                    //The lower bound of each subsequent
                    //primary reception report issued for the same session SHOULD be
                    //the upper bound of the prior primary reception report issued for
                    //the session, to minimize unnecessary retransmission.
                    lowerBound = m_mapPrimaryReportSegmentsSent.crbegin()->second.upperBound;
                    //std::cout << "3LB: " << lowerBound << std::endl;
                }
            }

            Ltp::report_segment_t reportSegment;
            if (!LtpFragmentMap::PopulateReportSegment(m_receivedDataFragmentsSet, reportSegment, lowerBound, upperBound)) {
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

            if (!checkpointIsResponseToReportSegment) {
                m_mapPrimaryReportSegmentsSent[rsn] = reportSegment;
            }
            m_mapAllReportSegmentsSent[rsn] = std::move(reportSegment);
            if (m_reportSegmentReportSerialNumbersUnackedSet.insert(rsn).second == false) { //serial number was not inserted (already exists)
                std::cerr << "unexpected error: m_reportSegmentReportSerialNumbersUnackedSet.insert(rsn) not inserted\n";
            }
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
