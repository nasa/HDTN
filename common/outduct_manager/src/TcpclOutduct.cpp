#include "TcpclOutduct.h"
#include <iostream>
#include <boost/make_unique.hpp>
#include <memory>
#include <boost/lexical_cast.hpp>

TcpclOutduct::TcpclOutduct(const outduct_element_config_t & outductConfig, const uint64_t myNodeId, const uint64_t outductUuid,
    const OutductOpportunisticProcessReceivedBundleCallback_t & outductOpportunisticProcessReceivedBundleCallback) :
    Outduct(outductConfig, outductUuid),
    m_tcpclBundleSource(outductConfig.keepAliveIntervalSeconds, myNodeId, outductConfig.nextHopEndpointId,
        outductConfig.bundlePipelineLimit + 5, outductConfig.tcpclV3MyMaxTxSegmentSizeBytes, outductOpportunisticProcessReceivedBundleCallback)
{}
TcpclOutduct::~TcpclOutduct() {}

std::size_t TcpclOutduct::GetTotalDataSegmentsUnacked() {
    return m_tcpclBundleSource.Virtual_GetTotalBundlesUnacked();
}
bool TcpclOutduct::Forward(const uint8_t* bundleData, const std::size_t size) {
    return m_tcpclBundleSource.BaseClass_Forward(bundleData, size);
}
bool TcpclOutduct::Forward(zmq::message_t & movableDataZmq) {
    return m_tcpclBundleSource.BaseClass_Forward(movableDataZmq);
}
bool TcpclOutduct::Forward(std::vector<uint8_t> & movableDataVec) {
    return m_tcpclBundleSource.BaseClass_Forward(movableDataVec);
}

void TcpclOutduct::SetOnSuccessfulAckCallback(const OnSuccessfulOutductAckCallback_t & callback) {
    m_tcpclBundleSource.SetOnSuccessfulAckCallback(callback);
}
void TcpclOutduct::SetOnFailedBundleVecSendCallback(const OnFailedBundleVecSendCallback_t& callback) {
    m_tcpclBundleSource.BaseClass_SetOnFailedBundleVecSendCallback(callback);
}
void TcpclOutduct::SetOnFailedBundleZmqSendCallback(const OnFailedBundleZmqSendCallback_t& callback) {
    m_tcpclBundleSource.BaseClass_SetOnFailedBundleZmqSendCallback(callback);
}
void TcpclOutduct::SetOnOutductLinkStatusChangedCallback(const OnOutductLinkStatusChangedCallback_t& callback) {
    m_tcpclBundleSource.BaseClass_SetOnOutductLinkStatusChangedCallback(callback);
}
void TcpclOutduct::SetUserAssignedUuid(uint64_t userAssignedUuid) {
    m_tcpclBundleSource.BaseClass_SetUserAssignedUuid(userAssignedUuid);
}

void TcpclOutduct::Connect() {
    m_tcpclBundleSource.Connect(m_outductConfig.remoteHostname, boost::lexical_cast<std::string>(m_outductConfig.remotePort));
}
bool TcpclOutduct::ReadyToForward() {
    return m_tcpclBundleSource.ReadyToForward();
}
void TcpclOutduct::Stop() {
    m_tcpclBundleSource.Stop();
}
void TcpclOutduct::GetOutductFinalStats(OutductFinalStats & finalStats) {
    finalStats.m_convergenceLayer = m_outductConfig.convergenceLayer;
    finalStats.m_totalDataSegmentsOrPacketsAcked = m_tcpclBundleSource.Virtual_GetTotalBundlesAcked();
    finalStats.m_totalDataSegmentsOrPacketsSent = m_tcpclBundleSource.Virtual_GetTotalBundlesSent();
}
