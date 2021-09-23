#include "LtpEngine.h"
#include <iostream>
#include <inttypes.h>
#include <boost/bind.hpp>
#include <boost/make_unique.hpp>

LtpEngine::LtpEngine(const uint64_t thisEngineId, const uint64_t mtuClientServiceData, uint64_t mtuReportSegment,
    const boost::posix_time::time_duration & oneWayLightTime, const boost::posix_time::time_duration & oneWayMarginTime,
    const uint64_t ESTIMATED_BYTES_TO_RECEIVE_PER_SESSION, bool startIoServiceThread,
    uint32_t checkpointEveryNthDataPacketSender, uint32_t maxRetriesPerSerialNumber, const bool force32BitRandomNumbers) :
    M_ESTIMATED_BYTES_TO_RECEIVE_PER_SESSION(ESTIMATED_BYTES_TO_RECEIVE_PER_SESSION),
    M_THIS_ENGINE_ID(thisEngineId),
    M_MTU_CLIENT_SERVICE_DATA(mtuClientServiceData),
    M_ONE_WAY_LIGHT_TIME(oneWayLightTime),
    M_ONE_WAY_MARGIN_TIME(oneWayMarginTime),
    M_TRANSMISSION_TO_ACK_RECEIVED_TIME((oneWayLightTime * 2) + (oneWayMarginTime * 2)),
    M_FORCE_32_BIT_RANDOM_NUMBERS(force32BitRandomNumbers),
    m_checkpointEveryNthDataPacketSender(checkpointEveryNthDataPacketSender),
    m_maxRetriesPerSerialNumber(maxRetriesPerSerialNumber),
    m_workLtpEnginePtr(boost::make_unique< boost::asio::io_service::work>(m_ioServiceLtpEngine)),
    m_timeManagerOfCancelSegments(m_ioServiceLtpEngine, oneWayLightTime, oneWayMarginTime, boost::bind(&LtpEngine::CancelSegmentTimerExpiredCallback, this, boost::placeholders::_1, boost::placeholders::_2))
{
    m_ltpRxStateMachine.SetCancelSegmentContentsReadCallback(boost::bind(&LtpEngine::CancelSegmentReceivedCallback, this,
        boost::placeholders::_1, boost::placeholders::_2, boost::placeholders::_3,
        boost::placeholders::_4, boost::placeholders::_5));
    m_ltpRxStateMachine.SetCancelAcknowledgementSegmentContentsReadCallback(boost::bind(&LtpEngine::CancelAcknowledgementSegmentReceivedCallback, this,
        boost::placeholders::_1, boost::placeholders::_2, boost::placeholders::_3,
        boost::placeholders::_4));
    m_ltpRxStateMachine.SetReportAcknowledgementSegmentContentsReadCallback(boost::bind(&LtpEngine::ReportAcknowledgementSegmentReceivedCallback, this,
        boost::placeholders::_1, boost::placeholders::_2, boost::placeholders::_3,
        boost::placeholders::_4));
    m_ltpRxStateMachine.SetReportSegmentContentsReadCallback(boost::bind(&LtpEngine::ReportSegmentReceivedCallback, this,
        boost::placeholders::_1, boost::placeholders::_2, boost::placeholders::_3,
        boost::placeholders::_4));
    m_ltpRxStateMachine.SetDataSegmentContentsReadCallback(boost::bind(&LtpEngine::DataSegmentReceivedCallback, this,
        boost::placeholders::_1, boost::placeholders::_2, boost::placeholders::_3,
        boost::placeholders::_4, boost::placeholders::_5, boost::placeholders::_6));

    SetMtuReportSegment(mtuReportSegment);
    Reset();

    if (startIoServiceThread) {
        m_ioServiceLtpEngineThreadPtr = boost::make_unique<boost::thread>(boost::bind(&boost::asio::io_service::run, &m_ioServiceLtpEngine));
    }
}

LtpEngine::~LtpEngine() {
    std::cout << "LtpEngine Stats:" << std::endl; //print before reset
    std::cout << "m_numCheckpointTimerExpiredCallbacks: " << m_numCheckpointTimerExpiredCallbacks << std::endl;
    std::cout << "m_numDiscretionaryCheckpointsNotResent: " << m_numDiscretionaryCheckpointsNotResent << std::endl;
    std::cout << "m_numReportSegmentTimerExpiredCallbacks: " << m_numReportSegmentTimerExpiredCallbacks << std::endl;
    std::cout << "m_numReportSegmentsUnableToBeIssued: " << m_numReportSegmentsUnableToBeIssued << std::endl;
    std::cout << "m_numReportSegmentsTooLargeAndNeedingSplit: " << m_numReportSegmentsTooLargeAndNeedingSplit << std::endl;
    std::cout << "m_numReportSegmentsCreatedViaSplit: " << m_numReportSegmentsCreatedViaSplit << std::endl;

    if (m_ioServiceLtpEngineThreadPtr) {
        boost::asio::post(m_ioServiceLtpEngine, boost::bind(&LtpEngine::Reset, this));
        m_workLtpEnginePtr.reset(); //erase the work object (destructor is thread safe) so that io_service thread will exit when it runs out of work 
        m_ioServiceLtpEngineThreadPtr->join();
        m_ioServiceLtpEngineThreadPtr.reset(); //delete it
    }
}

void LtpEngine::Reset() {
    m_mapSessionNumberToSessionSender.clear();
    m_mapSessionIdToSessionReceiver.clear();
    m_ltpRxStateMachine.InitRx();
    m_sendersIterator = m_mapSessionNumberToSessionSender.begin();
    m_receiversIterator = m_mapSessionIdToSessionReceiver.begin();
    m_closedSessionDataToSend.clear();
    m_timeManagerOfCancelSegments.Reset();
    m_listCancelSegmentTimerInfo.clear();
    m_listSendersNeedingDeleted.clear();
    m_listReceiversNeedingDeleted.clear();

    m_numCheckpointTimerExpiredCallbacks = 0;
    m_numDiscretionaryCheckpointsNotResent = 0;
    m_numReportSegmentTimerExpiredCallbacks = 0;
    m_numReportSegmentsUnableToBeIssued = 0;
    m_numReportSegmentsTooLargeAndNeedingSplit = 0;
    m_numReportSegmentsCreatedViaSplit = 0;
}

void LtpEngine::SetCheckpointEveryNthDataPacketForSenders(uint64_t checkpointEveryNthDataPacketSender) {
    m_checkpointEveryNthDataPacketSender = checkpointEveryNthDataPacketSender;
}

void LtpEngine::SetMtuReportSegment(uint64_t mtuReportSegment) {
    //(5 * 10) + (receptionClaims.size() * (2 * 10)); //5 sdnvs * 10 bytes sdnv max + reception claims * 2sdnvs per claim
    //70 bytes worst case minimum for 1 claim
    if (mtuReportSegment < 70) {
        std::cerr << "error in LtpEngine::SetMtuReportSegment: mtuReportSegment must be at least 70 bytes!!!!.. setting to 70 bytes" << std::endl;
        m_maxReceptionClaims = 1;
    }
    m_maxReceptionClaims = (mtuReportSegment - 50) / 20;
    std::cout << "max reception claims = " << m_maxReceptionClaims << std::endl;
}

bool LtpEngine::PacketIn(const uint8_t * data, const std::size_t size, Ltp::SessionOriginatorEngineIdDecodedCallback_t * sessionOriginatorEngineIdDecodedCallbackPtr) {
    std::string errorMessage;
    const bool success = m_ltpRxStateMachine.HandleReceivedChars(data, size, errorMessage, sessionOriginatorEngineIdDecodedCallbackPtr);
    if (!success) {
        std::cerr << "error in LtpEngine::PacketIn: " << errorMessage << std::endl;
        m_ltpRxStateMachine.InitRx();
    }
    PacketInFullyProcessedCallback(success);
    return success;
}

bool LtpEngine::PacketIn(const std::vector<boost::asio::const_buffer> & constBufferVec) { //for testing
    for (std::size_t i = 0; i < constBufferVec.size(); ++i) {
        const boost::asio::const_buffer & cb = constBufferVec[i];
        if (!PacketIn((const uint8_t *) cb.data(), cb.size())) {
            return false;
        }
    }
    return true;
}

void LtpEngine::PacketIn_ThreadSafe(const uint8_t * data, const std::size_t size, Ltp::SessionOriginatorEngineIdDecodedCallback_t * sessionOriginatorEngineIdDecodedCallbackPtr) {
    boost::asio::post(m_ioServiceLtpEngine, boost::bind(&LtpEngine::PacketIn, this, data, size, sessionOriginatorEngineIdDecodedCallbackPtr));
}
void LtpEngine::PacketIn_ThreadSafe(const std::vector<boost::asio::const_buffer> & constBufferVec) { //for testing
    boost::asio::post(m_ioServiceLtpEngine, boost::bind(&LtpEngine::PacketIn, this, constBufferVec));
}

void LtpEngine::SignalReadyForSend_ThreadSafe() { //called by HandleUdpSend
    boost::asio::post(m_ioServiceLtpEngine, boost::bind(&LtpEngine::TrySendPacketIfAvailable, this));
}

void LtpEngine::TrySendPacketIfAvailable() {
    if (m_ioServiceLtpEngineThreadPtr) { //if not running inside a unit test
        std::vector<boost::asio::const_buffer> constBufferVec;
        boost::shared_ptr<std::vector<std::vector<uint8_t> > >  underlyingDataToDeleteOnSentCallback;
        uint64_t sessionOriginatorEngineId;
        if (NextPacketToSendRoundRobin(constBufferVec, underlyingDataToDeleteOnSentCallback, sessionOriginatorEngineId)) {
            SendPacket(constBufferVec, underlyingDataToDeleteOnSentCallback, sessionOriginatorEngineId); //virtual call to child implementation
        }
    }
}

void LtpEngine::PacketInFullyProcessedCallback(bool success) {}

void LtpEngine::SendPacket(std::vector<boost::asio::const_buffer> & constBufferVec, boost::shared_ptr<std::vector<std::vector<uint8_t> > > & underlyingDataToDeleteOnSentCallback, const uint64_t sessionOriginatorEngineId) {}

bool LtpEngine::NextPacketToSendRoundRobin(std::vector<boost::asio::const_buffer> & constBufferVec, boost::shared_ptr<std::vector<std::vector<uint8_t> > > & underlyingDataToDeleteOnSentCallback, uint64_t & sessionOriginatorEngineId) {
    while (!m_listSendersNeedingDeleted.empty()) {
        std::map<uint64_t, std::unique_ptr<LtpSessionSender> >::iterator txSessionIt = m_mapSessionNumberToSessionSender.find(m_listSendersNeedingDeleted.front());
        if (txSessionIt != m_mapSessionNumberToSessionSender.end()) { //found
            if (txSessionIt->second->NextDataToSend(constBufferVec, underlyingDataToDeleteOnSentCallback)) { //if the session to be deleted still has data to send, send it before deletion
                sessionOriginatorEngineId = M_THIS_ENGINE_ID;
                return true;
            }
            else {
                m_numCheckpointTimerExpiredCallbacks += txSessionIt->second->m_numCheckpointTimerExpiredCallbacks;
                m_numDiscretionaryCheckpointsNotResent += txSessionIt->second->m_numDiscretionaryCheckpointsNotResent;
                m_mapSessionNumberToSessionSender.erase(txSessionIt);
                //revalidate iterator
                m_sendersIterator = m_mapSessionNumberToSessionSender.begin();
                ////std::cout << "deleted session sender " << m_listSendersNeedingDeleted.front() << std::endl;
            }
        }
        else {
            std::cout << "error in LtpEngine::NextPacketToSendRoundRobin: could not find session sender " << m_listSendersNeedingDeleted.front() << " to delete" << std::endl;
        }
        m_listSendersNeedingDeleted.pop_front();
    }

    while (!m_listReceiversNeedingDeleted.empty()) {
        std::map<Ltp::session_id_t, std::unique_ptr<LtpSessionReceiver> >::iterator rxSessionIt = m_mapSessionIdToSessionReceiver.find(m_listReceiversNeedingDeleted.front());
        if (rxSessionIt != m_mapSessionIdToSessionReceiver.end()) { //found rx Session
            if (rxSessionIt->second->NextDataToSend(constBufferVec, underlyingDataToDeleteOnSentCallback)) { //if the session to be deleted still has data to send, send it before deletion
                sessionOriginatorEngineId = rxSessionIt->first.sessionOriginatorEngineId;
                return true;
            }
            else {
                //erase session
                m_numReportSegmentTimerExpiredCallbacks += rxSessionIt->second->m_numReportSegmentTimerExpiredCallbacks;
                m_numReportSegmentsUnableToBeIssued += rxSessionIt->second->m_numReportSegmentsUnableToBeIssued;
                m_numReportSegmentsTooLargeAndNeedingSplit += rxSessionIt->second->m_numReportSegmentsTooLargeAndNeedingSplit;
                m_numReportSegmentsCreatedViaSplit += rxSessionIt->second->m_numReportSegmentsCreatedViaSplit;
                m_mapSessionIdToSessionReceiver.erase(rxSessionIt);
                //revalidate iterator
                m_receiversIterator = m_mapSessionIdToSessionReceiver.begin();
                ////std::cout << "deleted session receiver sessionNumber " << m_listReceiversNeedingDeleted.front().sessionNumber << std::endl;
            }
        }
        else {
            std::cout << "error in LtpEngine::NextPacketToSendRoundRobin: could not find session receiver " << m_listReceiversNeedingDeleted.front() << " to delete" << std::endl;
        }
        m_listReceiversNeedingDeleted.pop_front();
    }

    if (!m_listCancelSegmentTimerInfo.empty()) {
        //6.15.  Start Cancel Timer
        //This procedure is triggered by arrival of a link state cue indicating
        //the de - queuing(for transmission) of a Cx segment.
        //Response: the expected arrival time of the CAx segment that will be
        //produced on reception of this Cx segment is computed and a countdown
        //timer for this arrival time is started.
        cancel_segment_timer_info_t & info = m_listCancelSegmentTimerInfo.front();
        

        //send Cancel Segment
        underlyingDataToDeleteOnSentCallback = boost::make_shared<std::vector<std::vector<uint8_t> > >(1);
        Ltp::GenerateCancelSegmentLtpPacket((*underlyingDataToDeleteOnSentCallback)[0],
            info.sessionId, info.reasonCode, info.isFromSender, NULL, NULL);
        constBufferVec.resize(1);
        constBufferVec[0] = boost::asio::buffer((*underlyingDataToDeleteOnSentCallback)[0]);
        sessionOriginatorEngineId = info.sessionId.sessionOriginatorEngineId;

        const uint8_t * const infoPtr = (uint8_t*)&info;
        m_timeManagerOfCancelSegments.StartTimer(info.sessionId, std::vector<uint8_t>(infoPtr, infoPtr + sizeof(info)));
        //std::cout << "start cancel timer\n";
        m_listCancelSegmentTimerInfo.pop_front();
        return true;
    }

    if (!m_closedSessionDataToSend.empty()) { //includes report ack segments and cancel ack segments from closed sessions (which do not require timers)
        //highest priority
        underlyingDataToDeleteOnSentCallback = boost::make_shared<std::vector<std::vector<uint8_t> > >(1);
        (*underlyingDataToDeleteOnSentCallback)[0] = std::move(m_closedSessionDataToSend.front().second);
        sessionOriginatorEngineId = m_closedSessionDataToSend.front().first;
        m_closedSessionDataToSend.pop_front();
        constBufferVec.resize(1);
        constBufferVec[0] = boost::asio::buffer((*underlyingDataToDeleteOnSentCallback)[0]);
        return true;
    }

    bool foundDataToSend = false;
    for (unsigned int i = 0; i < 2; ++i) {
        while ((m_sendersIterator != m_mapSessionNumberToSessionSender.end()) && !foundDataToSend) {
            foundDataToSend = m_sendersIterator->second->NextDataToSend(constBufferVec, underlyingDataToDeleteOnSentCallback);
            sessionOriginatorEngineId = M_THIS_ENGINE_ID;
            ++m_sendersIterator;
        }
        while ((m_receiversIterator != m_mapSessionIdToSessionReceiver.end()) && !foundDataToSend) {
            foundDataToSend = m_receiversIterator->second->NextDataToSend(constBufferVec, underlyingDataToDeleteOnSentCallback);
            sessionOriginatorEngineId = m_receiversIterator->first.sessionOriginatorEngineId;
            ++m_receiversIterator;
        }
        if ((m_sendersIterator == m_mapSessionNumberToSessionSender.end()) && (m_receiversIterator == m_mapSessionIdToSessionReceiver.end())) {
            m_sendersIterator = m_mapSessionNumberToSessionSender.begin();
            m_receiversIterator = m_mapSessionIdToSessionReceiver.begin();
            if (!foundDataToSend) {
                continue; //continue once
            }
        }
    }
    return foundDataToSend;
}

/*
    Transmission Request

   In order to request transmission of a block of client service data,
   the client service MUST provide the following parameters to LTP:

      Destination client service ID.

      Destination LTP engine ID.

      Client service data to send, as an array of bytes.

      Length of the data to be sent.

      Length of the red-part of the data.  This value MUST be in the
      range from zero to the total length of data to be sent.

   On reception of a valid transmission request from a client service,
   LTP proceeds as follows.
   */
void LtpEngine::TransmissionRequest(uint64_t destinationClientServiceId, uint64_t destinationLtpEngineId, LtpClientServiceDataToSend && clientServiceDataToSend, uint64_t lengthOfRedPart) { //only called directly by unit test (not thread safe)

    uint64_t randomSessionNumberGeneratedBySender;
    uint64_t randomInitialSenderCheckpointSerialNumber; //incremented by 1 for new
    if(M_FORCE_32_BIT_RANDOM_NUMBERS) {
        randomSessionNumberGeneratedBySender = m_rng.GetRandomSession32(m_randomDevice);
        randomInitialSenderCheckpointSerialNumber = m_rng.GetRandomSerialNumber32(m_randomDevice);
    }
    else {
        randomSessionNumberGeneratedBySender = m_rng.GetRandomSession64(m_randomDevice);
        randomInitialSenderCheckpointSerialNumber = m_rng.GetRandomSerialNumber64(m_randomDevice);
    }
    Ltp::session_id_t senderSessionId(M_THIS_ENGINE_ID, randomSessionNumberGeneratedBySender);
    m_mapSessionNumberToSessionSender[randomSessionNumberGeneratedBySender] = boost::make_unique<LtpSessionSender>(
        randomInitialSenderCheckpointSerialNumber, std::move(clientServiceDataToSend), lengthOfRedPart, M_MTU_CLIENT_SERVICE_DATA, senderSessionId, destinationClientServiceId,
        M_ONE_WAY_LIGHT_TIME, M_ONE_WAY_MARGIN_TIME, m_ioServiceLtpEngine,
        boost::bind(&LtpEngine::NotifyEngineThatThisSenderNeedsDeletedCallback, this, boost::placeholders::_1, boost::placeholders::_2, boost::placeholders::_3),
        boost::bind(&LtpEngine::TrySendPacketIfAvailable, this),
        boost::bind(&LtpEngine::InitialTransmissionCompletedCallback, this, boost::placeholders::_1), m_checkpointEveryNthDataPacketSender, m_maxRetriesPerSerialNumber);
    //revalidate iterator
    m_sendersIterator = m_mapSessionNumberToSessionSender.begin();

    if (m_sessionStartCallback) {
        //At the sender, the session start notice informs the client service of the initiation of the transmission session.
        m_sessionStartCallback(senderSessionId);
    }
}
void LtpEngine::TransmissionRequest(uint64_t destinationClientServiceId, uint64_t destinationLtpEngineId, const uint8_t * clientServiceDataToCopyAndSend, uint64_t length, uint64_t lengthOfRedPart) {  //only called directly by unit test (not thread safe)
    TransmissionRequest(destinationClientServiceId, destinationLtpEngineId, std::vector<uint8_t>(clientServiceDataToCopyAndSend, clientServiceDataToCopyAndSend + length), lengthOfRedPart);
}
void LtpEngine::TransmissionRequest(boost::shared_ptr<transmission_request_t> & transmissionRequest) {
    TransmissionRequest(transmissionRequest->destinationClientServiceId, transmissionRequest->destinationLtpEngineId, std::move(transmissionRequest->clientServiceDataToSend), transmissionRequest->lengthOfRedPart);
    TrySendPacketIfAvailable();
}
void LtpEngine::TransmissionRequest_ThreadSafe(boost::shared_ptr<transmission_request_t> && transmissionRequest) {
    //The arguments to bind are copied or moved, and are never passed by reference unless wrapped in std::ref or std::cref.
    boost::asio::post(m_ioServiceLtpEngine, boost::bind(&LtpEngine::TransmissionRequest, this, std::move(transmissionRequest)));
}

void LtpEngine::CancellationRequest_ThreadSafe(const Ltp::session_id_t & sessionId) {
    //The arguments to bind are copied or moved, and are never passed by reference unless wrapped in std::ref or std::cref.
    boost::asio::post(m_ioServiceLtpEngine, boost::bind(&LtpEngine::CancellationRequest, this, sessionId));
}

//4.2.  Cancellation Request
//In order to request cancellation of a session, either as the sender
//or as the receiver of the associated data block, the client service
//must provide the session ID identifying the session to be canceled.
bool LtpEngine::CancellationRequest(const Ltp::session_id_t & sessionId) { //only called directly by unit test (not thread safe)

    //On reception of a valid cancellation request from a client service,
    //LTP proceeds as follows.

    //First, the internal "Cancel Session" procedure(Section 6.19) is
    //invoked.
    //6.19.  Cancel Session
    //Response: all segments of the affected session that are currently
    //queued for transmission can be deleted from the outbound traffic
    //queues.All countdown timers currently associated with the session
    //are deleted.Note : If the local LTP engine is the sender, then all
    //remaining data retransmission buffer space allocated to the session
    //can be released.
    
    
    
    if (M_THIS_ENGINE_ID == sessionId.sessionOriginatorEngineId) {
        //if the session is being canceled by the sender (i.e., the
        //session originator part of the session ID supplied in the
        //cancellation request is the local LTP engine ID):

        std::map<uint64_t, std::unique_ptr<LtpSessionSender> >::iterator txSessionIt = m_mapSessionNumberToSessionSender.find(sessionId.sessionNumber);
        if (txSessionIt != m_mapSessionNumberToSessionSender.end()) { //found
            

            //-If none of the data segments previously queued for transmission
            //as part of this session have yet been de - queued and transmitted
            //-- i.e., if the destination engine cannot possibly be aware of
            //this session -- then the session is simply closed; the "Close
            //Session" procedure (Section 6.20) is invoked.

            //- Otherwise, a CS(cancel by block sender) segment with the
            //reason - code USR_CNCLD MUST be queued for transmission to the
            //destination LTP engine specified in the transmission request
            //that started this session.
            //erase session
            m_numCheckpointTimerExpiredCallbacks += txSessionIt->second->m_numCheckpointTimerExpiredCallbacks;
            m_numDiscretionaryCheckpointsNotResent += txSessionIt->second->m_numDiscretionaryCheckpointsNotResent;
            m_mapSessionNumberToSessionSender.erase(txSessionIt);
            std::cout << "LtpEngine::CancellationRequest deleted session sender session number " << sessionId.sessionNumber << std::endl;
            //revalidate iterator
            m_sendersIterator = m_mapSessionNumberToSessionSender.begin();

            //send Cancel Segment to receiver (NextPacketToSendRoundRobin() will create the packet and start the timer)
            m_listCancelSegmentTimerInfo.emplace_back();
            cancel_segment_timer_info_t & info = m_listCancelSegmentTimerInfo.back();
            info.sessionId = sessionId;
            info.retryCount = 1;
            info.isFromSender = true;
            info.reasonCode = CANCEL_SEGMENT_REASON_CODES::USER_CANCELLED;

            TrySendPacketIfAvailable();
            return true;
        }
        return false;
    }
    else { //not sender, try receiver
        std::map<Ltp::session_id_t, std::unique_ptr<LtpSessionReceiver> >::iterator rxSessionIt = m_mapSessionIdToSessionReceiver.find(sessionId);
        if (rxSessionIt != m_mapSessionIdToSessionReceiver.end()) { //found rx Session

            //if the session is being canceled by the receiver:
            //-If there is no transmission queue - set bound for the sender
            //(possibly because the local LTP engine is running on a receive -
            //only device), then the session is simply closed; the "Close
            //Session" procedure (Section 6.20) is invoked.
            //- Otherwise, a CR(cancel by block receiver) segment with reason -
            //code USR_CNCLD MUST be queued for transmission to the block
            //sender.

            //erase session
            m_numReportSegmentTimerExpiredCallbacks += rxSessionIt->second->m_numReportSegmentTimerExpiredCallbacks;
            m_numReportSegmentsUnableToBeIssued += rxSessionIt->second->m_numReportSegmentsUnableToBeIssued;
            m_numReportSegmentsTooLargeAndNeedingSplit += rxSessionIt->second->m_numReportSegmentsTooLargeAndNeedingSplit;
            m_numReportSegmentsCreatedViaSplit += rxSessionIt->second->m_numReportSegmentsCreatedViaSplit;
            m_mapSessionIdToSessionReceiver.erase(rxSessionIt);
            std::cout << "LtpEngine::CancellationRequest deleted session receiver session number " << sessionId.sessionNumber << std::endl;
            //revalidate iterator
            m_receiversIterator = m_mapSessionIdToSessionReceiver.begin();

            //send Cancel Segment to sender (NextPacketToSendRoundRobin() will create the packet and start the timer)
            m_listCancelSegmentTimerInfo.emplace_back();
            cancel_segment_timer_info_t & info = m_listCancelSegmentTimerInfo.back();
            info.sessionId = sessionId;
            info.retryCount = 1;
            info.isFromSender = false;
            info.reasonCode = CANCEL_SEGMENT_REASON_CODES::USER_CANCELLED;
            


            TrySendPacketIfAvailable();
            return true;
        }
        return false;
    }
    
}

void LtpEngine::CancelSegmentReceivedCallback(const Ltp::session_id_t & sessionId, CANCEL_SEGMENT_REASON_CODES reasonCode, bool isFromSender,
    Ltp::ltp_extensions_t & headerExtensions, Ltp::ltp_extensions_t & trailerExtensions)
{
    if (isFromSender) { //to receiver
        std::map<Ltp::session_id_t, std::unique_ptr<LtpSessionReceiver> >::iterator rxSessionIt = m_mapSessionIdToSessionReceiver.find(sessionId);
        if (rxSessionIt != m_mapSessionIdToSessionReceiver.end()) { //found
            if (m_receptionSessionCancelledCallback) {
                m_receptionSessionCancelledCallback(sessionId, reasonCode); //No subsequent delivery notices will be issued for this session.
            }
            //erase session
            m_numReportSegmentTimerExpiredCallbacks += rxSessionIt->second->m_numReportSegmentTimerExpiredCallbacks;
            m_numReportSegmentsUnableToBeIssued += rxSessionIt->second->m_numReportSegmentsUnableToBeIssued;
            m_numReportSegmentsTooLargeAndNeedingSplit += rxSessionIt->second->m_numReportSegmentsTooLargeAndNeedingSplit;
            m_numReportSegmentsCreatedViaSplit += rxSessionIt->second->m_numReportSegmentsCreatedViaSplit;
            m_mapSessionIdToSessionReceiver.erase(rxSessionIt);
            std::cout << "LtpEngine::CancelSegmentReceivedCallback deleted session receiver session number " << sessionId.sessionNumber << std::endl;
            //revalidate iterator
            m_receiversIterator = m_mapSessionIdToSessionReceiver.begin();
            //Send CAx after outer if-else statement
            
        }
        else { //not found
            //The LTP receiver might also receive a retransmitted CS segment at the
            //CLOSED state(either if the CAS segment previously transmitted was
            //lost or if the CS timer expired prematurely at the LTP sender).In
            //such a case, the CAS is scheduled for retransmission.

            //Send CAx after outer if-else statement
        }
    }
    else { //to sender
        std::map<uint64_t, std::unique_ptr<LtpSessionSender> >::iterator txSessionIt = m_mapSessionNumberToSessionSender.find(sessionId.sessionNumber);
        if (txSessionIt != m_mapSessionNumberToSessionSender.end()) { //found
            if (m_transmissionSessionCancelledCallback) {
                m_transmissionSessionCancelledCallback(sessionId, reasonCode);
            }
            //erase session
            m_numCheckpointTimerExpiredCallbacks += txSessionIt->second->m_numCheckpointTimerExpiredCallbacks;
            m_numDiscretionaryCheckpointsNotResent += txSessionIt->second->m_numDiscretionaryCheckpointsNotResent;
            m_mapSessionNumberToSessionSender.erase(txSessionIt);
            std::cout << "LtpEngine::CancelSegmentReceivedCallback deleted session sender session number " << sessionId.sessionNumber << std::endl;
            //revalidate iterator
            m_sendersIterator = m_mapSessionNumberToSessionSender.begin();
            //Send CAx after outer if-else statement
        }
        else { //not found
            //Note that while at the CLOSED state:
            //If the session was canceled
            //by the receiver by issuing a CR segment, the receiver may retransmit
            //the CR segment(either prematurely or because the acknowledging CAR
            //segment got lost).In this case, the LTP sender retransmits the
            //acknowledging CAR segment and stays in the CLOSED state.
            //send CAx to receiver

            //Send CAx after outer if-else statement
        }
    }
    //send CAx to receiver or sender (whether or not session exists) (here because all data in session is erased)
    m_closedSessionDataToSend.emplace_back();
    Ltp::GenerateCancelAcknowledgementSegmentLtpPacket(m_closedSessionDataToSend.back().second,
        sessionId, isFromSender, NULL, NULL);
    m_closedSessionDataToSend.back().first = sessionId.sessionOriginatorEngineId;
    TrySendPacketIfAvailable();
}

void LtpEngine::CancelAcknowledgementSegmentReceivedCallback(const Ltp::session_id_t & sessionId, bool isToSender,
    Ltp::ltp_extensions_t & headerExtensions, Ltp::ltp_extensions_t & trailerExtensions)
{
    if (isToSender) {
        if (sessionId.sessionOriginatorEngineId != M_THIS_ENGINE_ID) {
            std::cerr << "error in CA received to sender: sessionId.sessionOriginatorEngineId (" << sessionId.sessionOriginatorEngineId << ") != M_THIS_ENGINE_ID (" << M_THIS_ENGINE_ID << ")\n";
            return;
        }
    }
    else {//to receiver
        if (sessionId.sessionOriginatorEngineId == M_THIS_ENGINE_ID) {
            std::cerr << "error in CA received to receiver: sessionId.sessionOriginatorEngineId (" << sessionId.sessionOriginatorEngineId << ") == M_THIS_ENGINE_ID (" << M_THIS_ENGINE_ID << ")\n";
            return;
        }
    }

    //6.18.  Stop Cancel Timer
    //This procedure is triggered by the reception of a CAx segment.
    //
    //Response: the timer associated with the Cx segment is deleted, and
    //the session of which the segment is one token is closed, i.e., the
    //"Close Session" procedure(Section 6.20) is invoked.
    //std::cout << "CancelAcknowledgementSegmentReceivedCallback1\n";
    if (!m_timeManagerOfCancelSegments.DeleteTimer(sessionId)) {
        std::cout << "LtpEngine::CancelAcknowledgementSegmentReceivedCallback didn't delete timer\n";
    }
    //std::cout << "CancelAcknowledgementSegmentReceivedCallback2\n";
}

void LtpEngine::CancelSegmentTimerExpiredCallback(Ltp::session_id_t cancelSegmentTimerSerialNumber, std::vector<uint8_t> & userData) {
    std::cout << "CancelSegmentTimerExpiredCallback!\n";
    cancel_segment_timer_info_t info;
    if (userData.size() != sizeof(info)) {
        std::cerr << "error in LtpEngine::CancelSegmentTimerExpiredCallback: userData.size() != sizeof(info)\n";
        return;
    }
    memcpy(&info, userData.data(), sizeof(info));

    if (info.retryCount <= m_maxRetriesPerSerialNumber) {
        //resend Cancel Segment to receiver (NextPacketToSendRoundRobin() will create the packet and start the timer)
        ++info.retryCount;
        m_listCancelSegmentTimerInfo.push_back(info);

        TrySendPacketIfAvailable();
    }
    
}

void LtpEngine::NotifyEngineThatThisSenderNeedsDeletedCallback(const Ltp::session_id_t & sessionId, bool wasCancelled, CANCEL_SEGMENT_REASON_CODES reasonCode) {
    if (wasCancelled) {
        //send Cancel Segment to receiver (NextPacketToSendRoundRobin() will create the packet and start the timer)
        m_listCancelSegmentTimerInfo.emplace_back();
        cancel_segment_timer_info_t & info = m_listCancelSegmentTimerInfo.back();
        info.sessionId = sessionId;
        info.retryCount = 1;
        info.isFromSender = true;
        info.reasonCode = reasonCode;

        if (m_transmissionSessionCancelledCallback) {
            m_transmissionSessionCancelledCallback(sessionId, reasonCode);
        }
    }
    else {
        if (m_transmissionSessionCompletedCallback) {
            m_transmissionSessionCompletedCallback(sessionId);
        }
    }

    m_listSendersNeedingDeleted.push_back(sessionId.sessionNumber);
    SignalReadyForSend_ThreadSafe(); //posts the TrySendPacketIfAvailable(); so this won't be deleteted during execution
}

void LtpEngine::NotifyEngineThatThisReceiverNeedsDeletedCallback(const Ltp::session_id_t & sessionId, bool wasCancelled, CANCEL_SEGMENT_REASON_CODES reasonCode) {
    if (wasCancelled) {
        //send Cancel Segment to sender (NextPacketToSendRoundRobin() will create the packet and start the timer)
        m_listCancelSegmentTimerInfo.emplace_back();
        cancel_segment_timer_info_t & info = m_listCancelSegmentTimerInfo.back();
        info.sessionId = sessionId;
        info.retryCount = 1;
        info.isFromSender = false;
        info.reasonCode = reasonCode;

        if (m_receptionSessionCancelledCallback) {
            m_receptionSessionCancelledCallback(sessionId, reasonCode);
        }
    }
    else { //not cancelled

    }
    
    m_listReceiversNeedingDeleted.push_back(sessionId);
    SignalReadyForSend_ThreadSafe(); //posts the TrySendPacketIfAvailable(); so this won't be deleteted during execution
}

void LtpEngine::InitialTransmissionCompletedCallback(const Ltp::session_id_t & sessionId) {
    if (m_initialTransmissionCompletedCallback) {
        m_initialTransmissionCompletedCallback(sessionId);
    }
}

void LtpEngine::ReportAcknowledgementSegmentReceivedCallback(const Ltp::session_id_t & sessionId, uint64_t reportSerialNumberBeingAcknowledged,
    Ltp::ltp_extensions_t & headerExtensions, Ltp::ltp_extensions_t & trailerExtensions)
{
    if (sessionId.sessionOriginatorEngineId == M_THIS_ENGINE_ID) {
        std::cerr << "error in RA received: sessionId.sessionOriginatorEngineId == M_THIS_ENGINE_ID\n";
        return;
    }
    std::map<Ltp::session_id_t, std::unique_ptr<LtpSessionReceiver> >::iterator rxSessionIt = m_mapSessionIdToSessionReceiver.find(sessionId);
    if (rxSessionIt != m_mapSessionIdToSessionReceiver.end()) { //found
        rxSessionIt->second->ReportAcknowledgementSegmentReceivedCallback(reportSerialNumberBeingAcknowledged, headerExtensions, trailerExtensions);
    }
    TrySendPacketIfAvailable();
}

void LtpEngine::ReportSegmentReceivedCallback(const Ltp::session_id_t & sessionId, const Ltp::report_segment_t & reportSegment,
    Ltp::ltp_extensions_t & headerExtensions, Ltp::ltp_extensions_t & trailerExtensions)
{
    if (sessionId.sessionOriginatorEngineId != M_THIS_ENGINE_ID) {
        std::cerr << "error in RS received: sessionId.sessionOriginatorEngineId(" << sessionId.sessionOriginatorEngineId << ")  != M_THIS_ENGINE_ID(" << M_THIS_ENGINE_ID << ")" << std::endl;
        return;
    }
    std::map<uint64_t, std::unique_ptr<LtpSessionSender> >::iterator txSessionIt = m_mapSessionNumberToSessionSender.find(sessionId.sessionNumber);
    if (txSessionIt != m_mapSessionNumberToSessionSender.end()) { //found
        txSessionIt->second->ReportSegmentReceivedCallback(reportSegment, headerExtensions, trailerExtensions);
    }
    else { //not found
        //Note that while at the CLOSED state, the LTP sender might receive an
        //RS segment(if the last transmitted RA segment before session close
        //got lost or if the LTP receiver retransmitted the RS segment
        //prematurely), in which case it retransmits an acknowledging RA
        //segment and stays in the CLOSED state.
        m_closedSessionDataToSend.emplace_back();
        Ltp::GenerateReportAcknowledgementSegmentLtpPacket(m_closedSessionDataToSend.back().second,
            sessionId, reportSegment.reportSerialNumber, NULL, NULL);
        m_closedSessionDataToSend.back().first = sessionId.sessionOriginatorEngineId;
    }
    TrySendPacketIfAvailable();
}

//SESSION START:
//At the receiver, this notice indicates the beginning of a
//new reception session, and is delivered upon arrival of the first
//data segment carrying a new session ID.

void LtpEngine::DataSegmentReceivedCallback(uint8_t segmentTypeFlags, const Ltp::session_id_t & sessionId,
    std::vector<uint8_t> & clientServiceDataVec, const Ltp::data_segment_metadata_t & dataSegmentMetadata,
    Ltp::ltp_extensions_t & headerExtensions, Ltp::ltp_extensions_t & trailerExtensions)
{
    if (sessionId.sessionOriginatorEngineId == M_THIS_ENGINE_ID) {
        std::cerr << "error in DS received: sessionId.sessionOriginatorEngineId(" << sessionId.sessionOriginatorEngineId << ") == M_THIS_ENGINE_ID(" << M_THIS_ENGINE_ID << ")\n";
        return;
    }

    std::map<Ltp::session_id_t, std::unique_ptr<LtpSessionReceiver> >::iterator rxSessionIt = m_mapSessionIdToSessionReceiver.find(sessionId);
    if (rxSessionIt == m_mapSessionIdToSessionReceiver.end()) { //not found.. new session started
        //first check if the session has been closed prevously before recreating
        std::map<uint64_t, std::unique_ptr<LtpSessionRecreationPreventer> >::iterator it = m_mapSessionOriginatorEngineIdToLtpSessionRecreationPreventer.find(sessionId.sessionOriginatorEngineId);
        if (it == m_mapSessionOriginatorEngineIdToLtpSessionRecreationPreventer.end()) {
            std::cout << "create new LtpSessionRecreationPreventer for sessionOriginatorEngineId " << sessionId.sessionOriginatorEngineId << std::endl;
            it = m_mapSessionOriginatorEngineIdToLtpSessionRecreationPreventer.insert(
                std::pair<uint64_t, std::unique_ptr<LtpSessionRecreationPreventer> >(sessionId.sessionOriginatorEngineId, boost::make_unique<LtpSessionRecreationPreventer>(1000))).first;
        }
        if (!it->second->AddSession(sessionId.sessionNumber)) {
            std::cout << "preventing old session from being recreated for " << sessionId << std::endl;
            return;
        }
        //if(m_ltpSessionRecreationPreventer.AddSession(sessionId.sessionNumber))
        const uint64_t randomNextReportSegmentReportSerialNumber = (M_FORCE_32_BIT_RANDOM_NUMBERS) ? m_rng.GetRandomSerialNumber32(m_randomDevice) : m_rng.GetRandomSerialNumber64(m_randomDevice); //incremented by 1 for new
        std::unique_ptr<LtpSessionReceiver> session = boost::make_unique<LtpSessionReceiver>(randomNextReportSegmentReportSerialNumber, m_maxReceptionClaims, M_ESTIMATED_BYTES_TO_RECEIVE_PER_SESSION,
            sessionId, dataSegmentMetadata.clientServiceId, M_ONE_WAY_LIGHT_TIME, M_ONE_WAY_MARGIN_TIME, m_ioServiceLtpEngine,
            boost::bind(&LtpEngine::NotifyEngineThatThisReceiverNeedsDeletedCallback, this, boost::placeholders::_1, boost::placeholders::_2, boost::placeholders::_3),
            boost::bind(&LtpEngine::TrySendPacketIfAvailable, this), m_maxRetriesPerSerialNumber);

        std::pair<std::map<Ltp::session_id_t, std::unique_ptr<LtpSessionReceiver> >::iterator, bool> res =
            m_mapSessionIdToSessionReceiver.insert(std::pair< Ltp::session_id_t, std::unique_ptr<LtpSessionReceiver> >(sessionId, std::move(session)));
        if (res.second == false) { //fragment key was not inserted
            std::cerr << "error new rx session cannot be inserted??\n";
            return;
        }
        rxSessionIt = res.first;
        //revalidate iterator
        m_receiversIterator = m_mapSessionIdToSessionReceiver.begin();

        if (m_sessionStartCallback) {
            //At the receiver, this notice indicates the beginning of a new reception session, and is delivered upon arrival of the first data segment carrying a new session ID.
            m_sessionStartCallback(sessionId);
        }
    }
    rxSessionIt->second->DataSegmentReceivedCallback(segmentTypeFlags, clientServiceDataVec, dataSegmentMetadata, headerExtensions, trailerExtensions, m_redPartReceptionCallback, m_greenPartSegmentArrivalCallback);
    TrySendPacketIfAvailable();
}

void LtpEngine::SetSessionStartCallback(const SessionStartCallback_t & callback) {
    m_sessionStartCallback = callback;
}
void LtpEngine::SetRedPartReceptionCallback(const RedPartReceptionCallback_t & callback) {
    m_redPartReceptionCallback = callback;
}
void LtpEngine::SetGreenPartSegmentArrivalCallback(const GreenPartSegmentArrivalCallback_t & callback) {
    m_greenPartSegmentArrivalCallback = callback;
}
void LtpEngine::SetReceptionSessionCancelledCallback(const ReceptionSessionCancelledCallback_t & callback) {
    m_receptionSessionCancelledCallback = callback;
}
void LtpEngine::SetTransmissionSessionCompletedCallback(const TransmissionSessionCompletedCallback_t & callback) {
    m_transmissionSessionCompletedCallback = callback;
}
void LtpEngine::SetInitialTransmissionCompletedCallback(const InitialTransmissionCompletedCallback_t & callback) {
    m_initialTransmissionCompletedCallback = callback;
}
void LtpEngine::SetTransmissionSessionCancelledCallback(const TransmissionSessionCancelledCallback_t & callback) {
    m_transmissionSessionCancelledCallback = callback;
}

std::size_t LtpEngine::NumActiveReceivers() const {
    return m_mapSessionIdToSessionReceiver.size();
}
std::size_t LtpEngine::NumActiveSenders() const {
    return m_mapSessionNumberToSessionSender.size();
}
