#include <boost/bind/bind.hpp>
#include <boost/make_shared.hpp>
#include <iostream>
#include "LtpBundleSink.h"
#include <boost/make_unique.hpp>
#include <boost/lexical_cast.hpp>

LtpBundleSink::LtpBundleSink(const LtpWholeBundleReadyCallback_t & ltpWholeBundleReadyCallback,
    const uint64_t thisEngineId, const uint64_t expectedSessionOriginatorEngineId, uint64_t mtuReportSegment,
    const boost::posix_time::time_duration & oneWayLightTime, const boost::posix_time::time_duration & oneWayMarginTime,
    const uint16_t myBoundUdpPort, const unsigned int numUdpRxCircularBufferVectors,
    const uint64_t ESTIMATED_BYTES_TO_RECEIVE_PER_SESSION,
    uint32_t ltpMaxRetriesPerSerialNumber, const bool force32BitRandomNumbers,
    const std::string & remoteUdpHostname, const uint16_t remoteUdpPort, const uint64_t maxBundleSizeBytes) :

    m_ltpWholeBundleReadyCallback(ltpWholeBundleReadyCallback),
    M_THIS_ENGINE_ID(thisEngineId),
    M_EXPECTED_SESSION_ORIGINATOR_ENGINE_ID(expectedSessionOriginatorEngineId),
    m_ltpUdpEngineManagerPtr(LtpUdpEngineManager::GetOrCreateInstance(myBoundUdpPort))
   
{
    m_ltpUdpEnginePtr = m_ltpUdpEngineManagerPtr->GetLtpUdpEnginePtr(expectedSessionOriginatorEngineId, true);
    if (m_ltpUdpEnginePtr == NULL) {
        m_ltpUdpEngineManagerPtr->AddLtpUdpEngine(thisEngineId, expectedSessionOriginatorEngineId, true, 1, mtuReportSegment, oneWayLightTime, oneWayMarginTime,
            remoteUdpHostname, remoteUdpPort, numUdpRxCircularBufferVectors, ESTIMATED_BYTES_TO_RECEIVE_PER_SESSION, maxBundleSizeBytes, 0, ltpMaxRetriesPerSerialNumber, force32BitRandomNumbers);
        m_ltpUdpEnginePtr = m_ltpUdpEngineManagerPtr->GetLtpUdpEnginePtr(expectedSessionOriginatorEngineId, true);
    }
    
    m_ltpUdpEnginePtr->SetRedPartReceptionCallback(boost::bind(&LtpBundleSink::RedPartReceptionCallback, this, boost::placeholders::_1, boost::placeholders::_2, boost::placeholders::_3,
        boost::placeholders::_4, boost::placeholders::_5));
    m_ltpUdpEnginePtr->SetReceptionSessionCancelledCallback(boost::bind(&LtpBundleSink::ReceptionSessionCancelledCallback, this, boost::placeholders::_1, boost::placeholders::_2));

    
    std::cout << "this ltp bundle sink for engine ID " << thisEngineId << " will receive on port "
        << myBoundUdpPort << " and send report segments to " << remoteUdpHostname << ":" << remoteUdpPort << std::endl;
}

void LtpBundleSink::RemoveCallback() {
    m_removeCallbackCalled = true;
}

LtpBundleSink::~LtpBundleSink() {
    m_removeCallbackCalled = false;
    m_ltpUdpEngineManagerPtr->RemoveLtpUdpEngine_ThreadSafe(M_EXPECTED_SESSION_ORIGINATOR_ENGINE_ID, true, boost::bind(&LtpBundleSink::RemoveCallback, this));
    boost::this_thread::sleep(boost::posix_time::milliseconds(100));
    for (unsigned int attempt = 0; attempt < 20; ++attempt) {
        if (m_removeCallbackCalled) {
            break;
        }
        std::cout << "waiting to remove ltp bundle sink for M_EXPECTED_SESSION_ORIGINATOR_ENGINE_ID " << M_EXPECTED_SESSION_ORIGINATOR_ENGINE_ID << std::endl;
        boost::this_thread::sleep(boost::posix_time::milliseconds(100));
    }
}



void LtpBundleSink::RedPartReceptionCallback(const Ltp::session_id_t & sessionId, padded_vector_uint8_t & movableClientServiceDataVec,
    uint64_t lengthOfRedPart, uint64_t clientServiceId, bool isEndOfBlock)
{
    m_ltpWholeBundleReadyCallback(movableClientServiceDataVec);

    //This function is holding up the LtpEngine thread.  Once this red part reception callback exits, the last LTP checkpoint report segment (ack)
    //can be sent to the sending ltp engine to close the session
}


void LtpBundleSink::ReceptionSessionCancelledCallback(const Ltp::session_id_t & sessionId, CANCEL_SEGMENT_REASON_CODES reasonCode)
{
    std::cout << "remote has cancelled session " << sessionId << " with reason code " << (int)reasonCode << std::endl;
}



bool LtpBundleSink::ReadyToBeDeleted() {
    return true;
}
