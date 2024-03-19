/**
 * @file LtpOverUdpBundleSource.cpp
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

#include <string>
#include "LtpOverUdpBundleSource.h"
#include "Logger.h"
#include <boost/lexical_cast.hpp>
#include <memory>

static constexpr hdtn::Logger::SubProcess subprocess = hdtn::Logger::SubProcess::none;

LtpOverUdpBundleSource::LtpOverUdpBundleSource(const LtpEngineConfig& ltpTxCfg) :
    LtpBundleSource(ltpTxCfg),
    m_ltpUdpEnginePtr(NULL)
{
   
}

LtpOverUdpBundleSource::~LtpOverUdpBundleSource() {
    Stop(); //parent call
    LOG_INFO(subprocess) << "waiting to remove ltp bundle source for engine ID " << M_THIS_ENGINE_ID;
    boost::mutex::scoped_lock cvLock(m_removeEngineMutex);
    m_removeEngineInProgress = true;
    m_ltpUdpEngineManagerPtr->RemoveLtpUdpEngineByRemoteEngineId_ThreadSafe(M_REMOTE_LTP_ENGINE_ID, false, boost::bind(&LtpOverUdpBundleSource::RemoveCallback, this));
    while (m_removeEngineInProgress) { //lock mutex (above) before checking condition
        //Returns: false if the call is returning because the time specified by abs_time was reached, true otherwise.
        if (!m_removeEngineCv.timed_wait(cvLock, boost::posix_time::seconds(3))) {
            LOG_ERROR(subprocess) << "timed out waiting (for 3 seconds) to remove ltp bundle source for engine ID " << M_THIS_ENGINE_ID;
            break;
        }
    }
    m_ltpEnginePtr = NULL;
    m_ltpUdpEnginePtr = NULL;
}

bool LtpOverUdpBundleSource::SetLtpEnginePtr() {
    m_ltpUdpEngineManagerPtr = LtpUdpEngineManager::GetOrCreateInstance(m_ltpTxCfg.myBoundUdpPort, true);
    m_ltpUdpEnginePtr = m_ltpUdpEngineManagerPtr->GetLtpUdpEnginePtrByRemoteEngineId(m_ltpTxCfg.remoteEngineId, false);
    if (m_ltpUdpEnginePtr == NULL) {
        if (!m_ltpUdpEngineManagerPtr->AddLtpUdpEngine(m_ltpTxCfg)) {
            LOG_ERROR(subprocess) << "LtpOverUdpBundleSource::SetLtpEnginePtr: cannot AddLtpUdpEngine";
            return false;
        }
        m_ltpUdpEnginePtr = m_ltpUdpEngineManagerPtr->GetLtpUdpEnginePtrByRemoteEngineId(m_ltpTxCfg.remoteEngineId, false);
        if (m_ltpUdpEnginePtr == NULL) {
            LOG_FATAL(subprocess) << "LtpOverUdpBundleSource::SetLtpEnginePtr: got a NULL ltpUdpEnginePtr";
            return false;
        }
    }
    m_ltpEnginePtr = m_ltpUdpEnginePtr;
    return true;
}

bool LtpOverUdpBundleSource::ReadyToForward() {
    if (!m_ltpUdpEngineManagerPtr->ReadyToForward()) { //in case there's a general error for the manager's udp receive, stop it here
        return false;
    }

    if (!m_ltpUdpEnginePtr->ReadyToSend()) { //in case there's a send error from the udp engine's socket send operation, stop it here
        return false;
    }
    return true;
}

void LtpOverUdpBundleSource::GetTransportLayerSpecificTelem(LtpOutductTelemetry_t& telem) const {
    if (m_ltpUdpEnginePtr) {
        telem.m_countUdpPacketsSent = m_ltpUdpEnginePtr->m_countAsyncSendCallbackCalls.load(std::memory_order_acquire)
            + m_ltpUdpEnginePtr->m_countBatchUdpPacketsSent.load(std::memory_order_acquire);
        telem.m_countRxUdpCircularBufferOverruns = m_ltpUdpEnginePtr->m_countCircularBufferOverruns.load(std::memory_order_acquire);
    }
}

void LtpOverUdpBundleSource::RemoveCallback() {
    m_removeEngineMutex.lock();
    m_removeEngineInProgress = false;
    m_removeEngineMutex.unlock();
    m_removeEngineCv.notify_one();
}
