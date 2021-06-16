#include "TcpclOutduct.h"
#include <iostream>
#include <boost/make_unique.hpp>
#include <boost/make_shared.hpp>
#include <boost/lexical_cast.hpp>

TcpclOutduct::TcpclOutduct(const outduct_element_config_t & outductConfig, const uint64_t outductUuid) :
    Outduct(outductConfig, outductUuid),
    m_tcpclBundleSource(outductConfig.keepAliveIntervalSeconds, outductConfig.endpointIdStr, outductConfig.bundlePipelineLimit + 5)
{}
TcpclOutduct::~TcpclOutduct() {}

std::size_t TcpclOutduct::GetTotalDataSegmentsUnacked() {
    return m_tcpclBundleSource.GetTotalDataSegmentsUnacked();
}
bool TcpclOutduct::Forward(const uint8_t* bundleData, const std::size_t size) {
    return m_tcpclBundleSource.Forward(bundleData, size);
}
bool TcpclOutduct::Forward(zmq::message_t & movableDataZmq) {
    return m_tcpclBundleSource.Forward(movableDataZmq);
}
bool TcpclOutduct::Forward(std::vector<uint8_t> & movableDataVec) {
    return m_tcpclBundleSource.Forward(movableDataVec);
}

void TcpclOutduct::SetOnSuccessfulAckCallback(const OnSuccessfulOutductAckCallback_t & callback) {
    m_tcpclBundleSource.SetOnSuccessfulAckCallback(callback);
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
    finalStats.m_totalDataSegmentsOrPacketsAcked = m_tcpclBundleSource.GetTotalDataSegmentsAcked();
    finalStats.m_totalDataSegmentsOrPacketsSent = m_tcpclBundleSource.GetTotalDataSegmentsSent();
}
