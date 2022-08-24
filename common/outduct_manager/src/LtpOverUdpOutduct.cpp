#include "LtpOverUdpOutduct.h"
#include <iostream>
#include <boost/make_unique.hpp>
#include <memory>
#include <boost/lexical_cast.hpp>

LtpOverUdpOutduct::LtpOverUdpOutduct(const outduct_element_config_t & outductConfig, const uint64_t outductUuid) :
    Outduct(outductConfig, outductUuid),
    m_ltpBundleSource(outductConfig.clientServiceId, outductConfig.remoteLtpEngineId, outductConfig.thisLtpEngineId, outductConfig.ltpDataSegmentMtu,
        boost::posix_time::milliseconds(outductConfig.oneWayLightTimeMs), boost::posix_time::milliseconds(outductConfig.oneWayMarginTimeMs),
        outductConfig.ltpSenderBoundPort, outductConfig.numRxCircularBufferElements,
        outductConfig.ltpCheckpointEveryNthDataSegment, outductConfig.ltpMaxRetriesPerSerialNumber, (outductConfig.ltpRandomNumberSizeBits == 32),
        m_outductConfig.remoteHostname, m_outductConfig.remotePort, m_outductConfig.ltpMaxSendRateBitsPerSecOrZeroToDisable, m_outductConfig.bundlePipelineLimit)
{}
LtpOverUdpOutduct::~LtpOverUdpOutduct() {}

std::size_t LtpOverUdpOutduct::GetTotalDataSegmentsUnacked() {
    return m_ltpBundleSource.GetTotalDataSegmentsUnacked();
}
bool LtpOverUdpOutduct::Forward(const uint8_t* bundleData, const std::size_t size) {
    return m_ltpBundleSource.Forward(bundleData, size);
}
bool LtpOverUdpOutduct::Forward(zmq::message_t & movableDataZmq) {
    return m_ltpBundleSource.Forward(movableDataZmq);
}
bool LtpOverUdpOutduct::Forward(std::vector<uint8_t> & movableDataVec) {
    return m_ltpBundleSource.Forward(movableDataVec);
}

void LtpOverUdpOutduct::SetOnSuccessfulAckCallback(const OnSuccessfulOutductAckCallback_t & callback) {
    m_ltpBundleSource.SetOnSuccessfulAckCallback(callback);
}

void LtpOverUdpOutduct::Connect() {

}
bool LtpOverUdpOutduct::ReadyToForward() {
    return true;
}
void LtpOverUdpOutduct::Stop() {
    m_ltpBundleSource.Stop();
}
void LtpOverUdpOutduct::GetOutductFinalStats(OutductFinalStats & finalStats) {
    finalStats.m_convergenceLayer = m_outductConfig.convergenceLayer;
    finalStats.m_totalDataSegmentsOrPacketsAcked = m_ltpBundleSource.GetTotalDataSegmentsAcked();
    finalStats.m_totalDataSegmentsOrPacketsSent = m_ltpBundleSource.GetTotalDataSegmentsSent();
}
uint64_t LtpOverUdpOutduct::GetOutductTelemetry(uint8_t* data, uint64_t bufferSize) {
    m_ltpBundleSource.SyncTelemetry();
    return m_ltpBundleSource.m_ltpOutductTelemetry.SerializeToLittleEndian(data, bufferSize);
}
