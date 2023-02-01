/**
 * @file LtpOverIpcBundleSource.cpp
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
#include "LtpOverIpcBundleSource.h"
#include "Logger.h"
#include <boost/lexical_cast.hpp>
#include <boost/make_unique.hpp>
#include <memory>

static constexpr hdtn::Logger::SubProcess subprocess = hdtn::Logger::SubProcess::none;

LtpOverIpcBundleSource::LtpOverIpcBundleSource(const LtpEngineConfig& ltpTxCfg) :
    LtpBundleSource(ltpTxCfg)
{
}

LtpOverIpcBundleSource::~LtpOverIpcBundleSource() {
    Stop(); //parent call
    LOG_INFO(subprocess) << "removing bundle source IPC";
    m_ltpIpcEnginePtr->Stop();
    m_ltpIpcEnginePtr.reset();
    LOG_INFO(subprocess) << "successfully removed bundle source IPC";
}

bool LtpOverIpcBundleSource::SetLtpEnginePtr() {
    const std::string myTxSharedMemoryName = "ltp_outduct_id_"
        + boost::lexical_cast<std::string>(m_ltpTxCfg.thisEngineId)
        + "_to_induct_id_" + boost::lexical_cast<std::string>(m_ltpTxCfg.remoteEngineId);
    const uint64_t maxUdpRxPacketSizeBytes = m_ltpTxCfg.mtuClientServiceData + (1500 - 1360);
    m_ltpIpcEnginePtr = boost::make_unique<LtpIpcEngine>(
        myTxSharedMemoryName,
        maxUdpRxPacketSizeBytes,
        m_ltpTxCfg);

    const std::string remoteTxSharedMemoryName = "ltp_induct_id_"
        + boost::lexical_cast<std::string>(m_ltpTxCfg.remoteEngineId)
        + "_to_outduct_id_" + boost::lexical_cast<std::string>(m_ltpTxCfg.thisEngineId);

    LOG_INFO(subprocess) << "connecting to remoteTxSharedMemoryName: " << remoteTxSharedMemoryName;
    if (!m_ltpIpcEnginePtr->Connect(remoteTxSharedMemoryName)) {
        return false;
    }

    m_ltpEnginePtr = m_ltpIpcEnginePtr.get();
    LOG_INFO(subprocess) << "this ltp bundle source for engine ID " << m_ltpTxCfg.thisEngineId << " will receive from remote shared memory name "
        << remoteTxSharedMemoryName << " and send data segments to my shared memory name " << myTxSharedMemoryName;
    return true;
}

bool LtpOverIpcBundleSource::ReadyToForward() {
    return m_ltpIpcEnginePtr->ReadyToSend(); //in case the IPC reader thread is stopped
}

void LtpOverIpcBundleSource::SyncTransportLayerSpecificTelem() {
    if (m_ltpIpcEnginePtr) {
        m_ltpOutductTelemetry.countUdpPacketsSent = m_ltpIpcEnginePtr->m_countAsyncSendCallbackCalls + m_ltpIpcEnginePtr->m_countBatchUdpPacketsSent;
        m_ltpOutductTelemetry.countRxUdpCircularBufferOverruns = m_ltpIpcEnginePtr->m_countCircularBufferOverruns;
    }
}
