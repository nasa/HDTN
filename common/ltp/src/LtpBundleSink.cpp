/**
 * @file LtpBundleSink.cpp
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

#include <boost/bind/bind.hpp>
#include <memory>
#include "LtpBundleSink.h"
#include "Logger.h"
#include <boost/make_unique.hpp>
#include <boost/lexical_cast.hpp>

static constexpr hdtn::Logger::SubProcess subprocess = hdtn::Logger::SubProcess::none;

LtpBundleSink::LtpBundleSink(const LtpWholeBundleReadyCallback_t& ltpWholeBundleReadyCallback, const LtpEngineConfig& ltpRxCfg) :

    m_ltpWholeBundleReadyCallback(ltpWholeBundleReadyCallback),
    M_EXPECTED_SESSION_ORIGINATOR_ENGINE_ID(ltpRxCfg.remoteEngineId),
    m_ltpUdpEngineManagerPtr(LtpUdpEngineManager::GetOrCreateInstance(ltpRxCfg.myBoundUdpPort, true))
   
{
    m_ltpUdpEnginePtr = m_ltpUdpEngineManagerPtr->GetLtpUdpEnginePtrByRemoteEngineId(ltpRxCfg.remoteEngineId, true); //sessionOriginatorEngineId is the remote engine id in the case of an induct
    if (m_ltpUdpEnginePtr == NULL) {
        static constexpr uint64_t maxSendRateBitsPerSecOrZeroToDisable = 0; //always disable rate for report segments, etc
        m_ltpUdpEngineManagerPtr->AddLtpUdpEngine(ltpRxCfg); //delaySendingOfDataSegmentsTimeMsOrZeroToDisable must be 0
        m_ltpUdpEnginePtr = m_ltpUdpEngineManagerPtr->GetLtpUdpEnginePtrByRemoteEngineId(ltpRxCfg.remoteEngineId, true); //sessionOriginatorEngineId is the remote engine id in the case of an induct
    }
    
    m_ltpUdpEnginePtr->SetRedPartReceptionCallback(boost::bind(&LtpBundleSink::RedPartReceptionCallback, this, boost::placeholders::_1, boost::placeholders::_2, boost::placeholders::_3,
        boost::placeholders::_4, boost::placeholders::_5));
    m_ltpUdpEnginePtr->SetReceptionSessionCancelledCallback(boost::bind(&LtpBundleSink::ReceptionSessionCancelledCallback, this, boost::placeholders::_1, boost::placeholders::_2));

    
    LOG_INFO(subprocess) << "this ltp bundle sink for engine ID " << ltpRxCfg.thisEngineId << " will receive on port "
        << ltpRxCfg.myBoundUdpPort << " and send report segments to " << ltpRxCfg.remoteHostname << ":" << ltpRxCfg.remotePort;
}

void LtpBundleSink::RemoveCallback() {
    m_removeEngineMutex.lock();
    m_removeEngineInProgress = false;
    m_removeEngineMutex.unlock();
    m_removeEngineCv.notify_one();
}

LtpBundleSink::~LtpBundleSink() {
    LOG_INFO(subprocess) << "waiting to remove ltp bundle sink for M_EXPECTED_SESSION_ORIGINATOR_ENGINE_ID " << M_EXPECTED_SESSION_ORIGINATOR_ENGINE_ID;
    boost::mutex::scoped_lock cvLock(m_removeEngineMutex);
    m_removeEngineInProgress = true;
    m_ltpUdpEngineManagerPtr->RemoveLtpUdpEngineByRemoteEngineId_ThreadSafe(M_EXPECTED_SESSION_ORIGINATOR_ENGINE_ID, true,
        boost::bind(&LtpBundleSink::RemoveCallback, this)); //sessionOriginatorEngineId is the remote engine id in the case of an induct
    while (m_removeEngineInProgress) { //lock mutex (above) before checking condition
        //Returns: false if the call is returning because the time specified by abs_time was reached, true otherwise.
        if (!m_removeEngineCv.timed_wait(cvLock, boost::posix_time::seconds(3))) {
            LOG_ERROR(subprocess) << "timed out waiting (for 3 seconds) to remove ltp bundle sink for M_EXPECTED_SESSION_ORIGINATOR_ENGINE_ID "
                << M_EXPECTED_SESSION_ORIGINATOR_ENGINE_ID;
            break;
        }
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
    LOG_INFO(subprocess) << "remote has cancelled session " << sessionId << " with reason code " << (int)reasonCode;
}



bool LtpBundleSink::ReadyToBeDeleted() {
    return true;
}
