#include "TcpclV4Outduct.h"
#include <iostream>
#include <boost/make_unique.hpp>
#include <boost/make_shared.hpp>
#include <boost/lexical_cast.hpp>

TcpclV4Outduct::TcpclV4Outduct(const outduct_element_config_t & outductConfig, const uint64_t myNodeId, const uint64_t outductUuid,
    const OutductOpportunisticProcessReceivedBundleCallback_t & outductOpportunisticProcessReceivedBundleCallback) :
    Outduct(outductConfig, outductUuid),
    m_tcpclV4BundleSource(outductConfig.keepAliveIntervalSeconds, myNodeId, outductConfig.nextHopEndpointId,
        outductConfig.bundlePipelineLimit + 5, outductConfig.tcpclAutoFragmentSizeBytes, outductOpportunisticProcessReceivedBundleCallback)
{}
TcpclV4Outduct::~TcpclV4Outduct() {}

std::size_t TcpclV4Outduct::GetTotalDataSegmentsUnacked() {
    return m_tcpclV4BundleSource.Virtual_GetTotalBundlesUnacked();
}
bool TcpclV4Outduct::Forward(const uint8_t* bundleData, const std::size_t size) {
    return m_tcpclV4BundleSource.BaseClass_Forward(bundleData, size);
}
bool TcpclV4Outduct::Forward(zmq::message_t & movableDataZmq) {
    return m_tcpclV4BundleSource.BaseClass_Forward(movableDataZmq);
}
bool TcpclV4Outduct::Forward(std::vector<uint8_t> & movableDataVec) {
    return m_tcpclV4BundleSource.BaseClass_Forward(movableDataVec);
}

void TcpclV4Outduct::SetOnSuccessfulAckCallback(const OnSuccessfulOutductAckCallback_t & callback) {
    m_tcpclV4BundleSource.SetOnSuccessfulAckCallback(callback);
}

void TcpclV4Outduct::Connect() {
    m_tcpclV4BundleSource.Connect(m_outductConfig.remoteHostname, boost::lexical_cast<std::string>(m_outductConfig.remotePort));
}
bool TcpclV4Outduct::ReadyToForward() {
    return m_tcpclV4BundleSource.ReadyToForward();
}
void TcpclV4Outduct::Stop() {
    m_tcpclV4BundleSource.Stop();
}
void TcpclV4Outduct::GetOutductFinalStats(OutductFinalStats & finalStats) {
    finalStats.m_convergenceLayer = m_outductConfig.convergenceLayer;
    finalStats.m_totalDataSegmentsOrPacketsAcked = m_tcpclV4BundleSource.Virtual_GetTotalBundlesAcked();
    finalStats.m_totalDataSegmentsOrPacketsSent = m_tcpclV4BundleSource.Virtual_GetTotalBundlesSent();
}
