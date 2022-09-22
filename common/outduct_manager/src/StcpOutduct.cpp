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
bool StcpOutduct::Forward(const uint8_t* bundleData, const std::size_t size, std::vector<uint8_t>&& userData) {
    return m_stcpBundleSource.Forward(bundleData, size, std::move(userData));
}
bool StcpOutduct::Forward(zmq::message_t & movableDataZmq, std::vector<uint8_t>&& userData) {
    return m_stcpBundleSource.Forward(movableDataZmq, std::move(userData));
}
bool StcpOutduct::Forward(std::vector<uint8_t> & movableDataVec, std::vector<uint8_t>&& userData) {
    return m_stcpBundleSource.Forward(movableDataVec, std::move(userData));
}

void StcpOutduct::SetOnSuccessfulAckCallback(const OnSuccessfulOutductAckCallback_t & callback) {
    m_stcpBundleSource.SetOnSuccessfulAckCallback(callback);
}
void StcpOutduct::SetOnFailedBundleVecSendCallback(const OnFailedBundleVecSendCallback_t& callback) {
    m_stcpBundleSource.SetOnFailedBundleVecSendCallback(callback);
}
void StcpOutduct::SetOnFailedBundleZmqSendCallback(const OnFailedBundleZmqSendCallback_t& callback) {
    m_stcpBundleSource.SetOnFailedBundleZmqSendCallback(callback);
}
void StcpOutduct::SetOnSuccessfulBundleSendCallback(const OnSuccessfulBundleSendCallback_t& callback) {
    m_stcpBundleSource.SetOnSuccessfulBundleSendCallback(callback);
}
void StcpOutduct::SetOnOutductLinkStatusChangedCallback(const OnOutductLinkStatusChangedCallback_t& callback) {
    m_stcpBundleSource.SetOnOutductLinkStatusChangedCallback(callback);
}
void StcpOutduct::SetUserAssignedUuid(uint64_t userAssignedUuid) {
    m_stcpBundleSource.SetUserAssignedUuid(userAssignedUuid);
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
