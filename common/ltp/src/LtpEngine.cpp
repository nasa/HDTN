#include "LtpEngine.h"
#include <iostream>
#include <inttypes.h>
#include <boost/bind.hpp>

LtpEngine::LtpEngine(const uint64_t thisEngineId, const uint64_t mtuClientServiceData) : M_THIS_ENGINE_ID(thisEngineId), M_MTU_CLIENT_SERVICE_DATA(mtuClientServiceData) {

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

    m_sendersIterator = m_mapSessionNumberToSessionSender.begin();
    m_receiversIterator = m_mapSessionIdToSessionReceiver.begin();
}

void LtpEngine::Reset() {
    m_mapSessionNumberToSessionSender.clear();
    m_mapSessionIdToSessionReceiver.clear();
    m_ltpRxStateMachine.InitRx();
    m_sendersIterator = m_mapSessionNumberToSessionSender.begin();
    m_receiversIterator = m_mapSessionIdToSessionReceiver.begin();
}

bool LtpEngine::PacketIn(const uint8_t * data, const std::size_t size) {
    std::string errorMessage;
    if (!m_ltpRxStateMachine.HandleReceivedChars(data, size, errorMessage)) {
        std::cerr << "error in LtpEngine::PacketIn: " << errorMessage << std::endl;
        return false;
    }
    return true;
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

bool LtpEngine::NextPacketToSendRoundRobin(std::vector<boost::asio::const_buffer> & constBufferVec, boost::shared_ptr<std::vector<std::vector<uint8_t> > > & underlyingDataToDeleteOnSentCallback) {

    bool foundDataToSend = false;
    while ((m_sendersIterator != m_mapSessionNumberToSessionSender.end()) && !foundDataToSend) {
        foundDataToSend = m_sendersIterator->second->NextDataToSend(constBufferVec, underlyingDataToDeleteOnSentCallback);
        ++m_sendersIterator;
    }
    while ((m_receiversIterator != m_mapSessionIdToSessionReceiver.end()) && !foundDataToSend) {
        foundDataToSend = m_receiversIterator->second->NextDataToSend(constBufferVec, underlyingDataToDeleteOnSentCallback);
        ++m_receiversIterator;
    }
    if ((m_sendersIterator == m_mapSessionNumberToSessionSender.end()) && (m_receiversIterator == m_mapSessionIdToSessionReceiver.end())) {
        m_sendersIterator = m_mapSessionNumberToSessionSender.begin();
        m_receiversIterator = m_mapSessionIdToSessionReceiver.begin();
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
void LtpEngine::TransmissionRequest(uint64_t destinationClientServiceId, uint64_t destinationLtpEngineId, std::vector<uint8_t> && clientServiceDataToSend, uint64_t lengthOfRedPart) {

    uint64_t randomSessionNumberGeneratedBySender;
    uint64_t randomInitialSenderCheckpointSerialNumber; //incremented by 1 for new
    {
        boost::mutex::scoped_lock lock(m_randomDeviceMutex);
        randomSessionNumberGeneratedBySender = m_rng.GetRandom(m_randomDevice);
        randomInitialSenderCheckpointSerialNumber = m_rng.GetRandom(m_randomDevice);
    }
    Ltp::session_id_t senderSessionId(M_THIS_ENGINE_ID, randomSessionNumberGeneratedBySender);
    m_mapSessionNumberToSessionSender[randomSessionNumberGeneratedBySender] = std::make_unique<LtpSessionSender>(
        randomInitialSenderCheckpointSerialNumber, std::move(clientServiceDataToSend), lengthOfRedPart, M_MTU_CLIENT_SERVICE_DATA, senderSessionId, destinationClientServiceId);
    //revalidate iterator
    m_sendersIterator = m_mapSessionNumberToSessionSender.begin();
}
void LtpEngine::TransmissionRequest(uint64_t destinationClientServiceId, uint64_t destinationLtpEngineId, const uint8_t * clientServiceDataToCopyAndSend, uint64_t length, uint64_t lengthOfRedPart) {
    TransmissionRequest(destinationClientServiceId, destinationLtpEngineId, std::vector<uint8_t>(clientServiceDataToCopyAndSend, clientServiceDataToCopyAndSend + length), lengthOfRedPart);
}

void LtpEngine::CancelSegmentReceivedCallback(const Ltp::session_id_t & sessionId, CANCEL_SEGMENT_REASON_CODES reasonCode, bool isFromSender,
    Ltp::ltp_extensions_t & headerExtensions, Ltp::ltp_extensions_t & trailerExtensions)
{
    if (isFromSender) {
        std::map<Ltp::session_id_t, std::unique_ptr<LtpSessionReceiver> >::iterator rxSessionIt = m_mapSessionIdToSessionReceiver.find(sessionId);
        if (rxSessionIt != m_mapSessionIdToSessionReceiver.end()) { //found
            rxSessionIt->second->CancelAcknowledgementSegmentReceivedCallback(headerExtensions, trailerExtensions);;
        }        
    }
    else {
        std::map<uint64_t, std::unique_ptr<LtpSessionSender> >::iterator txSessionIt = m_mapSessionNumberToSessionSender.find(sessionId.sessionNumber);
        if (txSessionIt != m_mapSessionNumberToSessionSender.end()) { //found
            txSessionIt->second->CancelAcknowledgementSegmentReceivedCallback(headerExtensions, trailerExtensions);
        }
    }
}

void LtpEngine::CancelAcknowledgementSegmentReceivedCallback(const Ltp::session_id_t & sessionId, bool isToSender,
    Ltp::ltp_extensions_t & headerExtensions, Ltp::ltp_extensions_t & trailerExtensions)
{
    if (sessionId.sessionOriginatorEngineId != M_THIS_ENGINE_ID) {
        std::cerr << "error in CA received: sessionId.sessionOriginatorEngineId != M_THIS_ENGINE_ID\n";
        return;
    }
    if (isToSender) {
        std::map<uint64_t, std::unique_ptr<LtpSessionSender> >::iterator txSessionIt = m_mapSessionNumberToSessionSender.find(sessionId.sessionNumber);
        if (txSessionIt != m_mapSessionNumberToSessionSender.end()) { //found
            txSessionIt->second->CancelAcknowledgementSegmentReceivedCallback(headerExtensions, trailerExtensions);
        }
    }
    else {
        std::map<Ltp::session_id_t, std::unique_ptr<LtpSessionReceiver> >::iterator rxSessionIt = m_mapSessionIdToSessionReceiver.find(sessionId);
        if (rxSessionIt != m_mapSessionIdToSessionReceiver.end()) { //found
            rxSessionIt->second->CancelAcknowledgementSegmentReceivedCallback(headerExtensions, trailerExtensions);;
        }
    }
}

void LtpEngine::ReportAcknowledgementSegmentReceivedCallback(const Ltp::session_id_t & sessionId, uint64_t reportSerialNumberBeingAcknowledged,
    Ltp::ltp_extensions_t & headerExtensions, Ltp::ltp_extensions_t & trailerExtensions)
{
    //if (sessionId.sessionOriginatorEngineId != M_THIS_ENGINE_ID) {
    //    std::cerr << "error in RA received: sessionId.sessionOriginatorEngineId != M_THIS_ENGINE_ID\n";
    //    return;
    //}
    std::map<Ltp::session_id_t, std::unique_ptr<LtpSessionReceiver> >::iterator rxSessionIt = m_mapSessionIdToSessionReceiver.find(sessionId);
    if (rxSessionIt != m_mapSessionIdToSessionReceiver.end()) { //found
        rxSessionIt->second->ReportAcknowledgementSegmentReceivedCallback(reportSerialNumberBeingAcknowledged, headerExtensions, trailerExtensions);
    }
}

void LtpEngine::ReportSegmentReceivedCallback(const Ltp::session_id_t & sessionId, const Ltp::report_segment_t & reportSegment,
    Ltp::ltp_extensions_t & headerExtensions, Ltp::ltp_extensions_t & trailerExtensions)
{
    if (sessionId.sessionOriginatorEngineId != M_THIS_ENGINE_ID) {
        std::cerr << "error in RS received: sessionId.sessionOriginatorEngineId != M_THIS_ENGINE_ID\n";
        return;
    }
    std::map<uint64_t, std::unique_ptr<LtpSessionSender> >::iterator txSessionIt = m_mapSessionNumberToSessionSender.find(sessionId.sessionNumber);
    if (txSessionIt != m_mapSessionNumberToSessionSender.end()) { //found
        txSessionIt->second->ReportSegmentReceivedCallback(reportSegment, headerExtensions, trailerExtensions);
    }
}

//SESSION START:
//At the receiver, this notice indicates the beginning of a
//new reception session, and is delivered upon arrival of the first
//data segment carrying a new session ID.

void LtpEngine::DataSegmentReceivedCallback(uint8_t segmentTypeFlags, const Ltp::session_id_t & sessionId,
    std::vector<uint8_t> & clientServiceDataVec, const Ltp::data_segment_metadata_t & dataSegmentMetadata,
    Ltp::ltp_extensions_t & headerExtensions, Ltp::ltp_extensions_t & trailerExtensions)
{
    //if (sessionId.sessionOriginatorEngineId != M_THIS_ENGINE_ID) {
    //    std::cerr << "error in DS received: sessionId.sessionOriginatorEngineId(" << sessionId.sessionOriginatorEngineId << ") != M_THIS_ENGINE_ID(" << M_THIS_ENGINE_ID << ")\n";
    //    return;
    //}

    std::map<Ltp::session_id_t, std::unique_ptr<LtpSessionReceiver> >::iterator rxSessionIt = m_mapSessionIdToSessionReceiver.find(sessionId);
    if (rxSessionIt == m_mapSessionIdToSessionReceiver.end()) { //not found.. new session started
        uint64_t randomNextReportSegmentReportSerialNumber; //incremented by 1 for new
        {
            boost::mutex::scoped_lock lock(m_randomDeviceMutex);
            randomNextReportSegmentReportSerialNumber = m_rng.GetRandom(m_randomDevice);
        }
        std::unique_ptr<LtpSessionReceiver> session = std::make_unique<LtpSessionReceiver>(randomNextReportSegmentReportSerialNumber, M_MTU_CLIENT_SERVICE_DATA,
            sessionId, dataSegmentMetadata.clientServiceId);

        std::pair<std::map<Ltp::session_id_t, std::unique_ptr<LtpSessionReceiver> >::iterator, bool> res =
            m_mapSessionIdToSessionReceiver.insert(std::pair< Ltp::session_id_t, std::unique_ptr<LtpSessionReceiver> >(sessionId, std::move(session)));
        if (res.second == false) { //fragment key was not inserted
            std::cerr << "error new rx session cannot be inserted??\n";
            return;
        }
        rxSessionIt = res.first;
        //revalidate iterator
        m_receiversIterator = m_mapSessionIdToSessionReceiver.begin();
    }
    rxSessionIt->second->DataSegmentReceivedCallback(segmentTypeFlags, clientServiceDataVec, dataSegmentMetadata, headerExtensions, trailerExtensions, m_redPartReceptionCallback);
}

void LtpEngine::SetRedPartReceptionCallback(const RedPartReceptionCallback_t & callback) {
    m_redPartReceptionCallback = callback;
}