#include "LtpSessionReceiver.h"
#include <iostream>
#include <inttypes.h>
#include <boost/bind.hpp>
#include <boost/make_shared.hpp>

LtpSessionReceiver::LtpSessionReceiver(uint64_t randomNextReportSegmentReportSerialNumber, const uint64_t MTU, const uint64_t ESTIMATED_BYTES_TO_RECEIVE,
    const Ltp::session_id_t & sessionId, const uint64_t clientServiceId,
    const boost::posix_time::time_duration & oneWayLightTime, const boost::posix_time::time_duration & oneWayMarginTime, boost::asio::io_service & ioServiceRef,
    const NotifyEngineThatThisReceiverNeedsDeletedCallback_t & notifyEngineThatThisReceiverNeedsDeletedCallback,
    const NotifyEngineThatThisReceiversTimersProducedDataFunction_t & notifyEngineThatThisReceiversTimersProducedDataFunction) :
    m_timeManagerOfReportSerialNumbers(ioServiceRef, oneWayLightTime, oneWayMarginTime, boost::bind(&LtpSessionReceiver::LtpReportSegmentTimerExpiredCallback, this, boost::placeholders::_1, boost::placeholders::_2)),
    m_nextReportSegmentReportSerialNumber(randomNextReportSegmentReportSerialNumber),
    M_MTU(MTU),
    M_ESTIMATED_BYTES_TO_RECEIVE(ESTIMATED_BYTES_TO_RECEIVE),
    M_SESSION_ID(sessionId),
    M_CLIENT_SERVICE_ID(clientServiceId),
    m_lengthOfRedPart(UINT64_MAX),
    m_didRedPartReceptionCallback(false),
    m_receivedEobFromGreenOrRed(false),
    m_ioServiceRef(ioServiceRef),
    m_notifyEngineThatThisReceiverNeedsDeletedCallback(notifyEngineThatThisReceiverNeedsDeletedCallback),
    m_notifyEngineThatThisReceiversTimersProducedDataFunction(notifyEngineThatThisReceiversTimersProducedDataFunction),
    m_numTimerExpiredCallbacks(0)
{
    m_dataReceived.reserve(ESTIMATED_BYTES_TO_RECEIVE);
}

void LtpSessionReceiver::LtpReportSegmentTimerExpiredCallback(uint64_t reportSerialNumber, std::vector<uint8_t> & userData) {
    std::cout << "LtpReportSegmentTimerExpiredCallback reportSerialNumber " << reportSerialNumber << std::endl;
    
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
    ++m_numTimerExpiredCallbacks;
    if (userData.size() != sizeof(uint8_t)) {
        std::cerr << "error in LtpSessionReceiver::LtpReportSegmentTimerExpiredCallback: userData.size() != sizeof(uint8_t)\n";
        return;
    }
    const uint8_t retryCount = userData[0];

    if (retryCount <= 5) {
        //resend 
        m_reportSerialNumbersToSendList.push_back(std::pair<uint64_t, uint8_t>(reportSerialNumber, retryCount + 1)); //initial retryCount of 1
        m_notifyEngineThatThisReceiversTimersProducedDataFunction();
    }
    else {
        m_notifyEngineThatThisReceiverNeedsDeletedCallback(M_SESSION_ID, true, CANCEL_SEGMENT_REASON_CODES::RLEXC);
    }
}

bool LtpSessionReceiver::NextDataToSend(std::vector<boost::asio::const_buffer> & constBufferVec, boost::shared_ptr<std::vector<std::vector<uint8_t> > > & underlyingDataToDeleteOnSentCallback) {

    if (!m_reportSerialNumbersToSendList.empty()) {
        
        const uint64_t rsn = m_reportSerialNumbersToSendList.front().first;
        //std::cout << "dequeue rsn for send " << rsn << std::endl;
        const uint8_t retryCount = m_reportSerialNumbersToSendList.front().second;
        std::map<uint64_t, Ltp::report_segment_t>::iterator reportSegmentIt = m_mapAllReportSegmentsSent.find(rsn);
        if (reportSegmentIt != m_mapAllReportSegmentsSent.end()) { //found
            //std::cout << "found!\n";
            underlyingDataToDeleteOnSentCallback = boost::make_shared<std::vector<std::vector<uint8_t> > >(1); //2 in case of trailer extensions
            Ltp::GenerateReportSegmentLtpPacket((*underlyingDataToDeleteOnSentCallback)[0],
                M_SESSION_ID, reportSegmentIt->second, NULL, NULL);
            constBufferVec.resize(1);
            constBufferVec[0] = boost::asio::buffer((*underlyingDataToDeleteOnSentCallback)[0]);
            m_reportSerialNumbersToSendList.pop_front();
            //std::cout << "start rsn " << rsn << std::endl;
            m_timeManagerOfReportSerialNumbers.StartTimer(rsn, std::vector<uint8_t>({ retryCount }));
            return true;
        }
        else {
            std::cerr << "error in LtpSessionReceiver::NextDataToSend: cannot find report segment\n";
            m_reportSerialNumbersToSendList.pop_front();
        }
    }

    return false;
}


void LtpSessionReceiver::ReportAcknowledgementSegmentReceivedCallback(uint64_t reportSerialNumberBeingAcknowledged,
    Ltp::ltp_extensions_t & headerExtensions, Ltp::ltp_extensions_t & trailerExtensions)
{
    

    //6.14.  Stop RS Timer
    //
    //This procedure is triggered by the reception of an RA.
    //
    //Response: the countdown timer associated with the original RS segment
    //(identified by the report serial number of the RA segment) is
    //deleted.If no other countdown timers associated with RS segments
    //exist for this session, then the session is closed : the "Close
    //Session" procedure (Section 6.20) is invoked.
    //std::cout << "acking rsn " << reportSerialNumberBeingAcknowledged << "\n";
    //std::cout << "m_receivedEobFromGreenOrRed: " << m_receivedEobFromGreenOrRed << std::endl;
    //std::cout << "m_reportSerialNumbersToSendList.empty(): " << m_reportSerialNumbersToSendList.empty() << std::endl;
    //std::cout << "m_timeManagerOfReportSerialNumbers.empty() b4: " << m_timeManagerOfReportSerialNumbers.Empty() << std::endl;
    m_timeManagerOfReportSerialNumbers.DeleteTimer(reportSerialNumberBeingAcknowledged);
    //std::cout << "m_timeManagerOfReportSerialNumbers.empty() after: " << m_timeManagerOfReportSerialNumbers.Empty() << std::endl;
    if (m_reportSerialNumbersToSendList.empty() && m_timeManagerOfReportSerialNumbers.Empty()) {
        //TODO.. NOT SURE WHAT TO DO WHEN GREEN EOB LOST
        if (m_receivedEobFromGreenOrRed && m_didRedPartReceptionCallback) {
            if (m_notifyEngineThatThisReceiverNeedsDeletedCallback) {
                m_notifyEngineThatThisReceiverNeedsDeletedCallback(M_SESSION_ID, false, CANCEL_SEGMENT_REASON_CODES::RESERVED); //close session (not cancelled)
                //std::cout << "rx notified\n";
            }
        }
    }
    
}


void LtpSessionReceiver::DataSegmentReceivedCallback(uint8_t segmentTypeFlags,
    std::vector<uint8_t> & clientServiceDataVec, const Ltp::data_segment_metadata_t & dataSegmentMetadata,
    Ltp::ltp_extensions_t & headerExtensions, Ltp::ltp_extensions_t & trailerExtensions, const RedPartReceptionCallback_t & redPartReceptionCallback)
{
    const uint64_t offsetPlusLength = dataSegmentMetadata.offset + dataSegmentMetadata.length;
    if (m_dataReceived.size() < offsetPlusLength) {
        m_dataReceived.resize(offsetPlusLength);
        //std::cout << m_dataReceived.size() << " " << m_dataReceived.capacity() << std::endl;
    }
    if (dataSegmentMetadata.length != clientServiceDataVec.size()) {
        std::cerr << "error dataSegmentMetadata.length != clientServiceDataVec.size()\n";
    }
    memcpy(m_dataReceived.data() + dataSegmentMetadata.offset, clientServiceDataVec.data(), dataSegmentMetadata.length);

    

    bool isRedData = (segmentTypeFlags <= 3);
    bool isEndOfBlock = ((segmentTypeFlags & 3) == 3);
    if (isEndOfBlock) {
        m_receivedEobFromGreenOrRed = true;
    }
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
                //std::cout << "find rsn: " << *dataSegmentMetadata.reportSerialNumber << "\n";
                std::map<uint64_t, Ltp::report_segment_t>::iterator reportSegmentIt = m_mapAllReportSegmentsSent.find(*dataSegmentMetadata.reportSerialNumber);
                if (reportSegmentIt != m_mapAllReportSegmentsSent.end()) { //found
                    lowerBound = reportSegmentIt->second.lowerBound;
                    //std::cout << "secondary LB: " << lowerBound << std::endl;
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
                    //std::cout << "primary first LB: " << lowerBound << std::endl;
                }
                else {
                    //The lower bound of each subsequent
                    //primary reception report issued for the same session SHOULD be
                    //the upper bound of the prior primary reception report issued for
                    //the session, to minimize unnecessary retransmission.
                    lowerBound = m_mapPrimaryReportSegmentsSent.crbegin()->second.upperBound;
                    //std::cout << "primary subsequent LB: " << lowerBound << std::endl;
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
            //std::cout << "reportSegment for lb: " << lowerBound << " and ub: " << upperBound << std::endl << reportSegment << std::endl;
            //LtpFragmentMap::PrintFragmentSet(m_receivedDataFragmentsSet);
            

            if (!checkpointIsResponseToReportSegment) {
                m_mapPrimaryReportSegmentsSent[rsn] = reportSegment;
            }
            m_mapAllReportSegmentsSent[rsn] = std::move(reportSegment);
            //std::cout << "queue send rsn " << rsn << "\n";
            m_reportSerialNumbersToSendList.push_back(std::pair<uint64_t, uint8_t>(rsn, 1)); //initial retryCount of 1
        }
        //std::cout << "m_lengthOfRedPart " << m_lengthOfRedPart << " m_receivedDataFragmentsSet.size() " << m_receivedDataFragmentsSet.size() << std::endl;
        if ((!m_didRedPartReceptionCallback) && (m_lengthOfRedPart != UINT64_MAX) && (m_receivedDataFragmentsSet.size() == 1)) {
            std::set<LtpFragmentMap::data_fragment_t>::const_iterator it = m_receivedDataFragmentsSet.cbegin();
            //std::cout << "it->beginIndex " << it->beginIndex << " it->endIndex " << it->endIndex << std::endl;
            if ((it->beginIndex == 0) && (it->endIndex == (m_lengthOfRedPart - 1))) {
                if (redPartReceptionCallback) {
                    m_didRedPartReceptionCallback = true;
                    redPartReceptionCallback(M_SESSION_ID,
                        m_dataReceived, m_lengthOfRedPart, dataSegmentMetadata.clientServiceId, isEndOfBlock);
                }
            }
        }
    }
    
}
