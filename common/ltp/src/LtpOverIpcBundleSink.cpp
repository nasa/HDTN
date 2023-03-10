/**
 * @file LtpOverIpcBundleSink.cpp
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

void LtpOverIpcBundleSink::SyncTransportLayerSpecificTelem() {
    if (m_ltpIpcEnginePtr) {
        m_telemetry.m_countUdpPacketsSent = m_ltpIpcEnginePtr->m_countAsyncSendCallbackCalls + m_ltpIpcEnginePtr->m_countBatchUdpPacketsSent;
        m_telemetry.m_countRxUdpCircularBufferOverruns = m_ltpIpcEnginePtr->m_countCircularBufferOverruns;
    }
}
