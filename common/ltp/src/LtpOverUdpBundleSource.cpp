/**
 * @file LtpOverUdpBundleSource.cpp
 * @author  Brian Tomko <brian.j.tomko@nasa.gov>
 *
 * @copyright Copyright (c) 2026 United States Government as represented by
 * the National Aeronautics and Space Administration.
 * No copyright is claimed in the United States under Title 17, U.S.Code.
 * All Other Rights Reserved.
 *
 * @section LICENSE
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
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
