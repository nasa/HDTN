/**
 * @file SlipOverUartInduct.cpp
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

#include "SlipOverUartInduct.h"
#include "Logger.h"
#include <boost/make_unique.hpp>
#include <boost/lexical_cast.hpp>
#include "Uri.h"
#include "ThreadNamer.h"

static constexpr hdtn::Logger::SubProcess subprocess = hdtn::Logger::SubProcess::none;

static const unsigned int maxTxBundlesInFlight = 5;
SlipOverUartInduct::SlipOverUartInduct(const InductProcessBundleCallback_t& inductProcessBundleCallback, const induct_element_config_t& inductConfig,
    const uint64_t maxBundleSizeBytes, const OnNewOpportunisticLinkCallback_t& onNewOpportunisticLinkCallback,
    const OnDeletedOpportunisticLinkCallback_t& onDeletedOpportunisticLinkCallback) :
    Induct(inductProcessBundleCallback, inductConfig),
    m_uartInterface(inductConfig.comPort,
        inductConfig.baudRate,//baud
        inductConfig.numRxCircularBufferElements, //numRxCircularBufferVectors
        maxBundleSizeBytes, //maxRxBundleSizeBytes
        maxTxBundlesInFlight, //maxTxBundlesInFlight
        inductProcessBundleCallback)
{
    m_uartInterface.m_inductTelemetry.m_connectionName = 
        Uri::GetIpnUriStringAnyServiceNumber(inductConfig.remoteNodeId)
            + " " + m_uartInterface.m_inductTelemetry.m_connectionName;
    m_onNewOpportunisticLinkCallback = onNewOpportunisticLinkCallback;
    m_onDeletedOpportunisticLinkCallback = onDeletedOpportunisticLinkCallback;

    m_uartInterface.SetOnFailedBundleVecSendCallback(boost::bind(&SlipOverUartInduct::OnFailedBundleVecSendCallback, this,
        boost::placeholders::_1, boost::placeholders::_2, boost::placeholders::_3, boost::placeholders::_4));
    m_uartInterface.SetOnFailedBundleZmqSendCallback(boost::bind(&SlipOverUartInduct::OnFailedBundleZmqSendCallback, this,
        boost::placeholders::_1, boost::placeholders::_2, boost::placeholders::_3, boost::placeholders::_4));
    m_uartInterface.SetOnSuccessfulBundleSendCallback(boost::bind(&SlipOverUartInduct::OnSuccessfulBundleSendCallback, this,
        boost::placeholders::_1, boost::placeholders::_2));

    m_mapNodeIdToOpportunisticBundleQueueMutex.lock();
    m_mapNodeIdToOpportunisticBundleQueue.erase(inductConfig.remoteNodeId);
    OpportunisticBundleQueue& opportunisticBundleQueue = m_mapNodeIdToOpportunisticBundleQueue[inductConfig.remoteNodeId];
    opportunisticBundleQueue.m_maxTxBundlesInPipeline = maxTxBundlesInFlight;
    opportunisticBundleQueue.m_remoteNodeId = inductConfig.remoteNodeId;
    m_opportunisticBundleQueuePtr = &opportunisticBundleQueue;
    m_mapNodeIdToOpportunisticBundleQueueMutex.unlock();

    if (m_onNewOpportunisticLinkCallback) {
        m_onNewOpportunisticLinkCallback(inductConfig.remoteNodeId, this, NULL);
    }
}
SlipOverUartInduct::~SlipOverUartInduct() {
    if (m_onDeletedOpportunisticLinkCallback) {
        try {
            m_onDeletedOpportunisticLinkCallback(m_inductConfig.remoteNodeId, this, NULL);
        }
        catch (const boost::bad_function_call&) {
            LOG_ERROR(subprocess) << "~SlipOverUartInduct cannot call m_onDeletedOpportunisticLinkCallback";
        }
    }
}



void SlipOverUartInduct::NotifyBundleReadyToSend_FromIoServiceThread(const uint64_t remoteNodeId) {
    if (!m_uartInterface.ReadyToForward()) {
        LOG_ERROR(subprocess) << "opportunistic link unavailable";
        return;
    }
    if (m_inductConfig.remoteNodeId != remoteNodeId) {
        LOG_ERROR(subprocess) << "SlipOverUartInduct remote node mismatch: expected "
            << m_inductConfig.remoteNodeId << " but got " << remoteNodeId;
        return;
    }
    std::pair<std::unique_ptr<zmq::message_t>, padded_vector_uint8_t> bundleDataPair;
    const std::size_t totalBundlesUnacked = m_uartInterface.GetTotalBundlesUnacked();
    if ((totalBundlesUnacked < maxTxBundlesInFlight) && BundleSinkTryGetData_FromIoServiceThread(*m_opportunisticBundleQueuePtr, bundleDataPair)) {
        if (bundleDataPair.first) {
            m_uartInterface.Forward(*bundleDataPair.first, std::vector<uint8_t>());
        }
        else if (bundleDataPair.second.size()) {
            m_uartInterface.Forward(bundleDataPair.second, std::vector<uint8_t>());
        }
        else {
            LOG_ERROR(subprocess) << "SlipOverUartInduct::NotifyBundleReadyToSend_FromIoServiceThread: empty data";
        }
    }
}

void SlipOverUartInduct::Virtual_PostNotifyBundleReadyToSend_FromIoServiceThread(const uint64_t remoteNodeId) {
    boost::asio::post(m_uartInterface.GetIoServiceRef(), boost::bind(&SlipOverUartInduct::NotifyBundleReadyToSend_FromIoServiceThread, this, remoteNodeId));
}

//todo failures cause opportunistic bundles to be lost
void SlipOverUartInduct::OnFailedBundleVecSendCallback(padded_vector_uint8_t& movableBundle,
    std::vector<uint8_t>& userData, uint64_t outductUuid, bool successCallbackCalled)
{
    (void)movableBundle;
    (void)userData;
    (void)outductUuid;
    (void)successCallbackCalled;
    BundleSinkNotifyOpportunisticDataAcked_FromIoServiceThread(*m_opportunisticBundleQueuePtr);
}
void SlipOverUartInduct::OnFailedBundleZmqSendCallback(zmq::message_t& movableBundle,
    std::vector<uint8_t>& userData, uint64_t outductUuid, bool successCallbackCalled)
{
    (void)movableBundle;
    (void)userData;
    (void)outductUuid;
    (void)successCallbackCalled;
    BundleSinkNotifyOpportunisticDataAcked_FromIoServiceThread(*m_opportunisticBundleQueuePtr);
}
void SlipOverUartInduct::OnSuccessfulBundleSendCallback(std::vector<uint8_t>& userData, uint64_t outductUuid) {
    (void)userData;
    (void)outductUuid;
    BundleSinkNotifyOpportunisticDataAcked_FromIoServiceThread(*m_opportunisticBundleQueuePtr);
}

void SlipOverUartInduct::PopulateInductTelemetry(InductTelemetry_t& inductTelem) {
    m_uartInterface.SyncTelemetry();
    inductTelem.m_convergenceLayer = "slip_over_uart";
    inductTelem.m_listInductConnections.clear();
    inductTelem.m_listInductConnections.emplace_back(boost::make_unique<SlipOverUartInductConnectionTelemetry_t>(m_uartInterface.m_inductTelemetry));
}
