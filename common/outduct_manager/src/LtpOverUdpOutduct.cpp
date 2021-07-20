#include "LtpOverUdpOutduct.h"
#include <iostream>
#include <boost/make_unique.hpp>
#include <boost/make_shared.hpp>
#include <boost/lexical_cast.hpp>

LtpOverUdpOutduct::LtpOverUdpOutduct(const outduct_element_config_t & outductConfig, const uint64_t outductUuid) :
    Outduct(outductConfig, outductUuid),
    m_ltpBundleSource(outductConfig.clientServiceId, outductConfig.remoteLtpEngineId, outductConfig.thisLtpEngineId, outductConfig.ltpDataSegmentMtu, 1,
        boost::posix_time::milliseconds(outductConfig.oneWayLightTimeMs), boost::posix_time::milliseconds(outductConfig.oneWayMarginTimeMs), 0, outductConfig.numRxCircularBufferElements,
        outductConfig.numRxCircularBufferBytesPerElement, 0, outductConfig.ltpCheckpointEveryNthDataSegment, outductConfig.ltpMaxRetriesPerSerialNumber)
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
    m_ltpBundleSource.Connect(m_outductConfig.remoteHostname, boost::lexical_cast<std::string>(m_outductConfig.remotePort));
}
bool LtpOverUdpOutduct::ReadyToForward() {
    return m_ltpBundleSource.ReadyToForward();
}
void LtpOverUdpOutduct::Stop() {
    m_ltpBundleSource.Stop();
}
void LtpOverUdpOutduct::GetOutductFinalStats(OutductFinalStats & finalStats) {
    finalStats.m_convergenceLayer = m_outductConfig.convergenceLayer;
    finalStats.m_totalDataSegmentsOrPacketsAcked = m_ltpBundleSource.GetTotalDataSegmentsAcked();
    finalStats.m_totalDataSegmentsOrPacketsSent = m_ltpBundleSource.GetTotalDataSegmentsSent();
}
