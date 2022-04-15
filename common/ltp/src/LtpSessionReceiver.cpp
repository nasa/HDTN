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
#include <iostream>
#include <inttypes.h>
#include <boost/bind/bind.hpp>
#include <boost/make_shared.hpp>

LtpSessionReceiver::LtpSessionReceiver(uint64_t randomNextReportSegmentReportSerialNumber, const uint64_t MAX_RECEPTION_CLAIMS,
    const uint64_t ESTIMATED_BYTES_TO_RECEIVE, const uint64_t maxRedRxBytes,
    const Ltp::session_id_t & sessionId, const uint64_t clientServiceId,
    const boost::posix_time::time_duration & oneWayLightTime, const boost::posix_time::time_duration & oneWayMarginTime, boost::asio::io_service & ioServiceRef,
    const NotifyEngineThatThisReceiverNeedsDeletedCallback_t & notifyEngineThatThisReceiverNeedsDeletedCallback,
    const NotifyEngineThatThisReceiversTimersProducedDataFunction_t & notifyEngineThatThisReceiversTimersProducedDataFunction,
    const uint32_t maxRetriesPerSerialNumber) :
    m_timeManagerOfReportSerialNumbers(ioServiceRef, oneWayLightTime, oneWayMarginTime, boost::bind(&LtpSessionReceiver::LtpReportSegmentTimerExpiredCallback, this, boost::placeholders::_1, boost::placeholders::_2)),
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
    m_ioServiceRef(ioServiceRef),
    m_notifyEngineThatThisReceiverNeedsDeletedCallback(notifyEngineThatThisReceiverNeedsDeletedCallback),
    m_notifyEngineThatThisReceiversTimersProducedDataFunction(notifyEngineThatThisReceiversTimersProducedDataFunction),
    m_numReportSegmentTimerExpiredCallbacks(0),
    m_numReportSegmentsUnableToBeIssued(0),
    m_numReportSegmentsTooLargeAndNeedingSplit(0),
    m_numReportSegmentsCreatedViaSplit(0)
{
    m_dataReceivedRed.reserve(ESTIMATED_BYTES_TO_RECEIVE);
}

LtpSessionReceiver::~LtpSessionReceiver() {}

void LtpSessionReceiver::LtpReportSegmentTimerExpiredCallback(uint64_t reportSerialNumber, std::vector<uint8_t> & userData) {
    //std::cout << "LtpReportSegmentTimerExpiredCallback reportSerialNumber " << reportSerialNumber << std::endl;
    
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
    if (userData.size() != sizeof(uint8_t)) {
        std::cerr << "error in LtpSessionReceiver::LtpReportSegmentTimerExpiredCallback: userData.size() != sizeof(uint8_t)\n";
        return;
    }
    const uint8_t retryCount = userData[0];

    if (retryCount <= M_MAX_RETRIES_PER_SERIAL_NUMBER) {
        //resend 
        m_reportSerialNumbersToSendList.push_back(std::pair<uint64_t, uint8_t>(reportSerialNumber, retryCount + 1)); //initial retryCount of 1
        m_notifyEngineThatThisReceiversTimersProducedDataFunction();
    }
    else {
        if (!m_didNotifyForDeletion) {
            m_didNotifyForDeletion = true;
            m_notifyEngineThatThisReceiverNeedsDeletedCallback(M_SESSION_ID, true, CANCEL_SEGMENT_REASON_CODES::RLEXC);
        }
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
            if (!m_didNotifyForDeletion) {
                m_didNotifyForDeletion = true;
                m_notifyEngineThatThisReceiverNeedsDeletedCallback(M_SESSION_ID, false, CANCEL_SEGMENT_REASON_CODES::RESERVED); //close session (not cancelled)
                //std::cout << "rx notified\n";
            }
        }
    }
    
}


void LtpSessionReceiver::DataSegmentReceivedCallback(uint8_t segmentTypeFlags,
    std::vector<uint8_t> & clientServiceDataVec, const Ltp::data_segment_metadata_t & dataSegmentMetadata,
    Ltp::ltp_extensions_t & headerExtensions, Ltp::ltp_extensions_t & trailerExtensions, const RedPartReceptionCallback_t & redPartReceptionCallback,
    const GreenPartSegmentArrivalCallback_t & greenPartSegmentArrivalCallback)
{
    const uint64_t offsetPlusLength = dataSegmentMetadata.offset + dataSegmentMetadata.length;
    
    if (dataSegmentMetadata.length != clientServiceDataVec.size()) {
        std::cerr << "error dataSegmentMetadata.length != clientServiceDataVec.size()\n";
    }
    

    

    bool isRedData = (segmentTypeFlags <= 3);
    bool isEndOfBlock = ((segmentTypeFlags & 3) == 3);
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
            //std::cout << "miscolored red\n";
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
            std::cout << "error in LtpSessionReceiver::DataSegmentReceivedCallback: current red data length ("
                << m_currentRedLength << " bytes) exceeds maximum of " << M_MAX_RED_RX_BYTES << " bytes\n";
            if (!m_didNotifyForDeletion) {
                m_didNotifyForDeletion = true;
                m_notifyEngineThatThisReceiverNeedsDeletedCallback(M_SESSION_ID, true, CANCEL_SEGMENT_REASON_CODES::SYSTEM_CANCELLED); //close session (cancelled)
            }
            return;
        }
        if (m_dataReceivedRed.size() < offsetPlusLength) {
            m_dataReceivedRed.resize(offsetPlusLength);
            //std::cout << m_dataReceived.size() << " " << m_dataReceived.capacity() << std::endl;
        }
        memcpy(m_dataReceivedRed.data() + dataSegmentMetadata.offset, clientServiceDataVec.data(), dataSegmentMetadata.length);

        bool isRedCheckpoint = (segmentTypeFlags != 0);
        bool isEndOfRedPart = (segmentTypeFlags & 2);
        LtpFragmentSet::InsertFragment(m_receivedDataFragmentsSet, 
            LtpFragmentSet::data_fragment_t(dataSegmentMetadata.offset, offsetPlusLength - 1));
        //LtpFragmentSet::PrintFragmentSet(m_receivedDataFragmentsSet);
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
                //std::cout << "Notice: this receiver not issuing RS to checkpoint sn " << *dataSegmentMetadata.checkpointSerialNumber 
                //    << " containing report sn " << *dataSegmentMetadata.reportSerialNumber 
                //    << " due to lowerBound(" << lowerBound << ") >= upperBound(" << upperBound
                //    << ") probably due to out-of-order arrival of discretionary checkpoints." << std::endl;
            }
            else {
                std::vector<Ltp::report_segment_t> reportSegmentsVec(1);
                if (!LtpFragmentSet::PopulateReportSegment(m_receivedDataFragmentsSet, reportSegmentsVec[0], lowerBound, upperBound)) {
                    std::cerr << "error in LtpSessionReceiver::DataSegmentReceivedCallback: cannot populate report segment\n";
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
                    //std::cout << "splitting 1 report segment with " << reportSegmentsVec[0].receptionClaims.size() << " reception claims into "
                    //    << reportSegmentsSplitVec.size() << " report segments with no more than " << M_MAX_RECEPTION_CLAIMS << " reception claims per report segment" << std::endl;
                    ++m_numReportSegmentsTooLargeAndNeedingSplit;
                    m_numReportSegmentsCreatedViaSplit += reportSegmentsSplitVec.size();
                    reportSegmentsVec = std::move(reportSegmentsSplitVec);
                }

                for (std::vector<Ltp::report_segment_t>::iterator it = reportSegmentsVec.begin(); it != reportSegmentsVec.end(); ++it) {
                    Ltp::report_segment_t & reportSegment = *it;

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
                    //LtpFragmentSet::PrintFragmentSet(m_receivedDataFragmentsSet);

                    if (!checkpointIsResponseToReportSegment) {
                        m_mapPrimaryReportSegmentsSent[rsn] = reportSegment;
                    }
                    m_mapAllReportSegmentsSent[rsn] = std::move(reportSegment);
                    //std::cout << "queue send rsn " << rsn << "\n";
                    m_reportSerialNumbersToSendList.push_back(std::pair<uint64_t, uint8_t>(rsn, 1)); //initial retryCount of 1
                }
            }
        }
        //std::cout << "m_lengthOfRedPart " << m_lengthOfRedPart << " m_receivedDataFragmentsSet.size() " << m_receivedDataFragmentsSet.size() << std::endl;
        if ((!m_didRedPartReceptionCallback) && (m_lengthOfRedPart != UINT64_MAX) && (m_receivedDataFragmentsSet.size() == 1)) {
            std::set<LtpFragmentSet::data_fragment_t>::const_iterator it = m_receivedDataFragmentsSet.cbegin();
            //std::cout << "it->beginIndex " << it->beginIndex << " it->endIndex " << it->endIndex << std::endl;
            if ((it->beginIndex == 0) && (it->endIndex == (m_lengthOfRedPart - 1))) {
                if (redPartReceptionCallback) {
                    m_didRedPartReceptionCallback = true;
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
            //std::cout << "miscolored green\n";
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
                    //std::cout << "rx notified\n";
                }
            }
        }
    }
    
}
