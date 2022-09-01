#include "StcpOutduct.h"
#include <iostream>
#include <boost/make_unique.hpp>
#include <memory>
#include <boost/lexical_cast.hpp>

StcpOutduct::StcpOutduct(const outduct_element_config_t & outductConfig, const uint64_t outductUuid) :
    Outduct(outductConfig, outductUuid),
    m_stcpBundleSource(outductConfig.keepAliveIntervalSeconds, outductConfig.bundlePipelineLimit + 5)
{}
StcpOutduct::~StcpOutduct() {}

std::size_t StcpOutduct::GetTotalDataSegmentsUnacked() {
    return m_stcpBundleSource.GetTotalDataSegmentsUnacked();
}
bool StcpOutduct::Forward(const uint8_t* bundleData, const std::size_t size) {
    return m_stcpBundleSource.Forward(bundleData, size);
}
bool StcpOutduct::Forward(zmq::message_t & movableDataZmq) {
    return m_stcpBundleSource.Forward(movableDataZmq);
}
bool StcpOutduct::Forward(std::vector<uint8_t> & movableDataVec) {
    return m_stcpBundleSource.Forward(movableDataVec);
}

void StcpOutduct::SetOnSuccessfulAckCallback(const OnSuccessfulOutductAckCallback_t & callback) {
    m_stcpBundleSource.SetOnSuccessfulAckCallback(callback);
}

void StcpOutduct::Connect() {
    m_stcpBundleSource.Connect(m_outductConfig.remoteHostname, boost::lexical_cast<std::string>(m_outductConfig.remotePort));
}
bool StcpOutduct::ReadyToForward() {
    return m_stcpBundleSource.ReadyToForward();
}
void StcpOutduct::Stop() {
    m_stcpBundleSource.Stop();
}
void StcpOutduct::GetOutductFinalStats(OutductFinalStats & finalStats) {
    finalStats.m_convergenceLayer = m_outductConfig.convergenceLayer;
    finalStats.m_totalDataSegmentsOrPacketsAcked = m_stcpBundleSource.GetTotalDataSegmentsAcked();
    finalStats.m_totalDataSegmentsOrPacketsSent = m_stcpBundleSource.GetTotalDataSegmentsSent();
}
uint64_t StcpOutduct::GetOutductTelemetry(uint8_t* data, uint64_t bufferSize) {
    return m_stcpBundleSource.m_stcpOutductTelemetry.SerializeToLittleEndian(data, bufferSize);
}
