/**
 * @file LtpOverIpcBundleSink.cpp
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

#include <boost/bind/bind.hpp>
#include <memory>
#include "LtpOverIpcBundleSink.h"
#include "Logger.h"
#include <boost/make_unique.hpp>
#include <boost/lexical_cast.hpp>

static constexpr hdtn::Logger::SubProcess subprocess = hdtn::Logger::SubProcess::none;

LtpOverIpcBundleSink::LtpOverIpcBundleSink(const LtpWholeBundleReadyCallback_t& ltpWholeBundleReadyCallback, const LtpEngineConfig& ltpRxCfg) :
    LtpBundleSink(ltpWholeBundleReadyCallback, ltpRxCfg)
{
}

bool LtpOverIpcBundleSink::SetLtpEnginePtr() {
    const std::string myTxSharedMemoryName = "ltp_induct_id_" 
        + boost::lexical_cast<std::string>(m_ltpRxCfg.thisEngineId)
        + "_to_outduct_id_" + boost::lexical_cast<std::string>(m_ltpRxCfg.remoteEngineId);
    const uint64_t maxUdpRxPacketSizeBytes = m_ltpRxCfg.mtuReportSegment + (1500 - 1360);
    m_ltpIpcEnginePtr = boost::make_unique<LtpIpcEngine>(
        myTxSharedMemoryName,
        maxUdpRxPacketSizeBytes,
        m_ltpRxCfg);

    const std::string remoteTxSharedMemoryName = "ltp_outduct_id_"
        + boost::lexical_cast<std::string>(m_ltpRxCfg.remoteEngineId)
        + "_to_induct_id_" + boost::lexical_cast<std::string>(m_ltpRxCfg.thisEngineId);

    if (!m_ltpIpcEnginePtr->Connect(remoteTxSharedMemoryName)) {
        return false;
    }

    m_ltpEnginePtr = m_ltpIpcEnginePtr.get();
    LOG_INFO(subprocess) << "this ltp bundle sink for engine ID " << m_ltpRxCfg.thisEngineId << " will receive from remote shared memory name "
        << remoteTxSharedMemoryName << " and send report segments to my shared memory name " << myTxSharedMemoryName;
    return true;
}

LtpOverIpcBundleSink::~LtpOverIpcBundleSink() {
    LOG_INFO(subprocess) << "removing bundle sink IPC";
    m_ltpIpcEnginePtr->Stop();
    m_ltpIpcEnginePtr.reset();
    LOG_INFO(subprocess) << "successfully removed bundle sink IPC";
}


bool LtpOverIpcBundleSink::ReadyToBeDeleted() {
    return true;
}

void LtpOverIpcBundleSink::GetTransportLayerSpecificTelem(LtpInductConnectionTelemetry_t& telem) const {
    if (m_ltpIpcEnginePtr) {
        telem.m_countUdpPacketsSent = m_ltpIpcEnginePtr->m_countAsyncSendCallbackCalls.load(std::memory_order_acquire)
            + m_ltpIpcEnginePtr->m_countBatchUdpPacketsSent.load(std::memory_order_acquire);
        telem.m_countRxUdpCircularBufferOverruns = m_ltpIpcEnginePtr->m_countCircularBufferOverruns.load(std::memory_order_acquire);
    }
}
