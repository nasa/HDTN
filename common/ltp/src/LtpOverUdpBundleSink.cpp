/**
 * @file LtpOverUdpBundleSink.cpp
 * @author  Brian Tomko <brian.j.tomko@nasa.gov>
 *
 * @copyright Copyright (c) 2021 United States Government as represented by
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
#include "LtpOverUdpBundleSink.h"
#include "Logger.h"
#include <boost/make_unique.hpp>
#include <boost/lexical_cast.hpp>

static constexpr hdtn::Logger::SubProcess subprocess = hdtn::Logger::SubProcess::none;

LtpOverUdpBundleSink::LtpOverUdpBundleSink(const LtpWholeBundleReadyCallback_t& ltpWholeBundleReadyCallback, const LtpEngineConfig& ltpRxCfg) :
    LtpBundleSink(ltpWholeBundleReadyCallback, ltpRxCfg),
    m_ltpUdpEnginePtr(NULL)   
{
}

bool LtpOverUdpBundleSink::SetLtpEnginePtr() {
    m_ltpUdpEngineManagerPtr = LtpUdpEngineManager::GetOrCreateInstance(m_ltpRxCfg.myBoundUdpPort, true);
    m_ltpUdpEnginePtr = m_ltpUdpEngineManagerPtr->GetLtpUdpEnginePtrByRemoteEngineId(m_ltpRxCfg.remoteEngineId, true); //sessionOriginatorEngineId is the remote engine id in the case of an induct
    if (m_ltpUdpEnginePtr == NULL) {
        m_ltpUdpEngineManagerPtr->AddLtpUdpEngine(m_ltpRxCfg); //delaySendingOfDataSegmentsTimeMsOrZeroToDisable must be 0
        m_ltpUdpEnginePtr = m_ltpUdpEngineManagerPtr->GetLtpUdpEnginePtrByRemoteEngineId(m_ltpRxCfg.remoteEngineId, true); //sessionOriginatorEngineId is the remote engine id in the case of an induct
    }
    m_ltpEnginePtr = m_ltpUdpEnginePtr;
    LOG_INFO(subprocess) << "this ltp bundle sink for engine ID " << m_ltpRxCfg.thisEngineId << " will receive on port "
        << m_ltpRxCfg.myBoundUdpPort << " and send report segments to " << m_ltpRxCfg.remoteHostname << ":" << m_ltpRxCfg.remotePort;
    return true;
}

void LtpOverUdpBundleSink::RemoveCallback() {
    m_removeEngineMutex.lock();
    m_removeEngineInProgress = false;
    m_removeEngineMutex.unlock();
    m_removeEngineCv.notify_one();
}

LtpOverUdpBundleSink::~LtpOverUdpBundleSink() {
    LOG_INFO(subprocess) << "waiting to remove ltp bundle sink for M_EXPECTED_SESSION_ORIGINATOR_ENGINE_ID " << M_EXPECTED_SESSION_ORIGINATOR_ENGINE_ID;
    boost::mutex::scoped_lock cvLock(m_removeEngineMutex);
    m_removeEngineInProgress = true;
    m_ltpUdpEngineManagerPtr->RemoveLtpUdpEngineByRemoteEngineId_ThreadSafe(M_EXPECTED_SESSION_ORIGINATOR_ENGINE_ID, true,
        boost::bind(&LtpOverUdpBundleSink::RemoveCallback, this)); //sessionOriginatorEngineId is the remote engine id in the case of an induct
    while (m_removeEngineInProgress) { //lock mutex (above) before checking condition
        //Returns: false if the call is returning because the time specified by abs_time was reached, true otherwise.
        if (!m_removeEngineCv.timed_wait(cvLock, boost::posix_time::seconds(3))) {
            LOG_ERROR(subprocess) << "timed out waiting (for 3 seconds) to remove ltp bundle sink for M_EXPECTED_SESSION_ORIGINATOR_ENGINE_ID "
                << M_EXPECTED_SESSION_ORIGINATOR_ENGINE_ID;
            break;
        }
    }
}


bool LtpOverUdpBundleSink::ReadyToBeDeleted() {
    return true;
}

void LtpOverUdpBundleSink::GetTransportLayerSpecificTelem(LtpInductConnectionTelemetry_t& telem) const {
    if (m_ltpUdpEnginePtr) {
        telem.m_countUdpPacketsSent = m_ltpUdpEnginePtr->m_countAsyncSendCallbackCalls.load(std::memory_order_acquire)
            + m_ltpUdpEnginePtr->m_countBatchUdpPacketsSent.load(std::memory_order_acquire);
        telem.m_countRxUdpCircularBufferOverruns = m_ltpUdpEnginePtr->m_countCircularBufferOverruns.load(std::memory_order_acquire);
    }
}
