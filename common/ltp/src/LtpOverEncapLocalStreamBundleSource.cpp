/**
 * @file LtpOverEncapLocalStreamBundleSource.cpp
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

#include <string>
#include "LtpOverEncapLocalStreamBundleSource.h"
#include "Logger.h"
#include <boost/lexical_cast.hpp>
#include <boost/make_unique.hpp>
#include <memory>

static constexpr hdtn::Logger::SubProcess subprocess = hdtn::Logger::SubProcess::none;

LtpOverEncapLocalStreamBundleSource::LtpOverEncapLocalStreamBundleSource(const LtpEngineConfig& ltpTxCfg) :
    LtpBundleSource(ltpTxCfg)
{
}

LtpOverEncapLocalStreamBundleSource::~LtpOverEncapLocalStreamBundleSource() {
    Stop(); //parent call
    LOG_INFO(subprocess) << "removing bundle source LtpEncapLocalStream";
    m_ltpEncapLocalStreamEnginePtr->Stop();
    m_ltpEncapLocalStreamEnginePtr.reset();
    LOG_INFO(subprocess) << "successfully removed bundle source LtpEncapLocalStream";
}

bool LtpOverEncapLocalStreamBundleSource::SetLtpEnginePtr() {
    const uint64_t maxUdpRxPacketSizeBytes = 65535; //initial reserved size (will resize if necessary)
    m_ltpEncapLocalStreamEnginePtr = boost::make_unique<LtpEncapLocalStreamEngine>(
        maxUdpRxPacketSizeBytes,
        m_ltpTxCfg);

    LOG_INFO(subprocess) << "connecting to Ltp encapLocalSocketOrPipePath: "
        << m_ltpTxCfg.encapLocalSocketOrPipePath;
    if (!m_ltpEncapLocalStreamEnginePtr->Connect(m_ltpTxCfg.encapLocalSocketOrPipePath, false)) { //false => not stream creator (client/connector)
        return false;
    }

    m_ltpEnginePtr = m_ltpEncapLocalStreamEnginePtr.get();
    LOG_INFO(subprocess) << "this ltp bundle source for engine ID "
        << m_ltpTxCfg.thisEngineId
        << " will send and receive from encap local stream named "
        << m_ltpTxCfg.encapLocalSocketOrPipePath;
    return true;
}

bool LtpOverEncapLocalStreamBundleSource::ReadyToForward() {
    return m_ltpEncapLocalStreamEnginePtr->ReadyToSend(); //in case the EncapLocalStream reader thread is stopped
}

void LtpOverEncapLocalStreamBundleSource::GetTransportLayerSpecificTelem(LtpOutductTelemetry_t& telem) const {
    if (m_ltpEncapLocalStreamEnginePtr) {
        telem.m_countUdpPacketsSent = m_ltpEncapLocalStreamEnginePtr->m_countAsyncSendCallbackCalls.load(std::memory_order_acquire)
            + m_ltpEncapLocalStreamEnginePtr->m_countBatchUdpPacketsSent.load(std::memory_order_acquire);
        telem.m_countRxUdpCircularBufferOverruns = m_ltpEncapLocalStreamEnginePtr->m_countCircularBufferOverruns.load(std::memory_order_acquire);
    }
}
