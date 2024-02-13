/**
 * @file LtpOverEncapLocalStreamBundleSink.cpp
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
