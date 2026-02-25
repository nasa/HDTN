/**
 * @file LtpOverEncapLocalStreamBundleSink.cpp
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
#include "LtpOverEncapLocalStreamBundleSink.h"
#include "Logger.h"
#include <boost/make_unique.hpp>
#include <boost/lexical_cast.hpp>

static constexpr hdtn::Logger::SubProcess subprocess = hdtn::Logger::SubProcess::none;

LtpOverEncapLocalStreamBundleSink::LtpOverEncapLocalStreamBundleSink(const LtpWholeBundleReadyCallback_t& ltpWholeBundleReadyCallback, const LtpEngineConfig& ltpRxCfg) :
    LtpBundleSink(ltpWholeBundleReadyCallback, ltpRxCfg)
{
}

bool LtpOverEncapLocalStreamBundleSink::SetLtpEnginePtr() {
    const uint64_t maxEncapRxPacketSizeBytes = 65535; //initial reserved size (will resize if necessary)
    m_ltpEncapLocalStreamEnginePtr = boost::make_unique<LtpEncapLocalStreamEngine>(
        maxEncapRxPacketSizeBytes,
        m_ltpRxCfg);
    
    if (!m_ltpEncapLocalStreamEnginePtr->Connect(m_ltpRxCfg.encapLocalSocketOrPipePath, true)) { //true => stream creator (server/binder)
        return false;
    }

    m_ltpEnginePtr = m_ltpEncapLocalStreamEnginePtr.get();
    LOG_INFO(subprocess) << "this ltp bundle sink for engine ID " << m_ltpRxCfg.thisEngineId
        << " will receive and send from encap local stream named "
        << m_ltpRxCfg.encapLocalSocketOrPipePath;
    return true;
}

LtpOverEncapLocalStreamBundleSink::~LtpOverEncapLocalStreamBundleSink() {
    LOG_INFO(subprocess) << "removing bundle sink Ltp EncapLocalStream";
    m_ltpEncapLocalStreamEnginePtr->Stop();
    m_ltpEncapLocalStreamEnginePtr.reset();
    LOG_INFO(subprocess) << "successfully removed bundle sink Ltp EncapLocalStream";
}


bool LtpOverEncapLocalStreamBundleSink::ReadyToBeDeleted() {
    return true;
}

void LtpOverEncapLocalStreamBundleSink::GetTransportLayerSpecificTelem(LtpInductConnectionTelemetry_t& telem) const {
    if (m_ltpEncapLocalStreamEnginePtr) {
        telem.m_countUdpPacketsSent = m_ltpEncapLocalStreamEnginePtr->m_countAsyncSendCallbackCalls.load(std::memory_order_acquire)
            + m_ltpEncapLocalStreamEnginePtr->m_countBatchUdpPacketsSent.load(std::memory_order_acquire);
        telem.m_countRxUdpCircularBufferOverruns = m_ltpEncapLocalStreamEnginePtr->m_countCircularBufferOverruns.load(std::memory_order_acquire);
    }
}
