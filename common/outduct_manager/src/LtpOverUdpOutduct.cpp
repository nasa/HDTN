#include "LtpOverUdpOutduct.h"
#include <iostream>
#include <boost/make_unique.hpp>
#include <memory>
#include <boost/lexical_cast.hpp>
#include "LtpEngineConfig.h"

LtpOverUdpOutduct::LtpOverUdpOutduct(const outduct_element_config_t& outductConfig, const uint64_t outductUuid) :
    Outduct(outductConfig, outductUuid)
{

    LtpEngineConfig ltpTxCfg;
    ltpTxCfg.thisEngineId = outductConfig.thisLtpEngineId;
    ltpTxCfg.remoteEngineId = outductConfig.remoteLtpEngineId;
    ltpTxCfg.clientServiceId = outductConfig.clientServiceId;
    ltpTxCfg.isInduct = false;
    ltpTxCfg.mtuClientServiceData = outductConfig.ltpDataSegmentMtu;
    ltpTxCfg.mtuReportSegment = 1360; //unused for outducts
    ltpTxCfg.oneWayLightTime = boost::posix_time::milliseconds(outductConfig.oneWayLightTimeMs);
    ltpTxCfg.oneWayMarginTime = boost::posix_time::milliseconds(outductConfig.oneWayMarginTimeMs);
    ltpTxCfg.remoteHostname = m_outductConfig.remoteHostname;
    ltpTxCfg.remotePort = m_outductConfig.remotePort;
    ltpTxCfg.myBoundUdpPort = outductConfig.ltpSenderBoundPort;
    ltpTxCfg.numUdpRxCircularBufferVectors = outductConfig.numRxCircularBufferElements;
    ltpTxCfg.estimatedBytesToReceivePerSession = 0; //unused for outducts
    ltpTxCfg.maxRedRxBytesPerSession = 0; //unused for outducts
    ltpTxCfg.checkpointEveryNthDataPacketSender = outductConfig.ltpCheckpointEveryNthDataSegment;
    ltpTxCfg.maxRetriesPerSerialNumber = outductConfig.ltpMaxRetriesPerSerialNumber;
    ltpTxCfg.force32BitRandomNumbers = (outductConfig.ltpRandomNumberSizeBits == 32);
    ltpTxCfg.maxSendRateBitsPerSecOrZeroToDisable = m_outductConfig.ltpMaxSendRateBitsPerSecOrZeroToDisable;
    ltpTxCfg.maxSimultaneousSessions = m_outductConfig.maxNumberOfBundlesInPipeline;
    ltpTxCfg.rxDataSegmentSessionNumberRecreationPreventerHistorySizeOrZeroToDisable = 0; //unused for outducts
    ltpTxCfg.maxUdpPacketsToSendPerSystemCall = m_outductConfig.ltpMaxUdpPacketsToSendPerSystemCall;
    ltpTxCfg.senderPingSecondsOrZeroToDisable = m_outductConfig.ltpSenderPingSecondsOrZeroToDisable;
    ltpTxCfg.delaySendingOfReportSegmentsTimeMsOrZeroToDisable = 0; //unused for outducts
    ltpTxCfg.delaySendingOfDataSegmentsTimeMsOrZeroToDisable = 20; //todo delaySendingOfDataSegmentsTimeMsOrZeroToDisable
    ltpTxCfg.senderNewFileDurationMsToStoreSessionDataOrZeroToDisable = 0; //todo
    ltpTxCfg.senderWriteSessionDataToFilesPath = "./";
    //ltpTxCfg.senderNewFileDurationMsToStoreSessionDataOrZeroToDisable = 2000; //todo
    //ltpTxCfg.senderWriteSessionDataToFilesPath = "W:\\";

    m_ltpBundleSourcePtr = boost::make_unique<LtpBundleSource>(ltpTxCfg);
}
LtpOverUdpOutduct::~LtpOverUdpOutduct() {}

std::size_t LtpOverUdpOutduct::GetTotalDataSegmentsUnacked() {
    return m_ltpBundleSourcePtr->GetTotalDataSegmentsUnacked();
}
bool LtpOverUdpOutduct::Forward(const uint8_t* bundleData, const std::size_t size, std::vector<uint8_t>&& userData) {
    return m_ltpBundleSourcePtr->Forward(bundleData, size, std::move(userData));
}
bool LtpOverUdpOutduct::Forward(zmq::message_t & movableDataZmq, std::vector<uint8_t>&& userData) {
    return m_ltpBundleSourcePtr->Forward(movableDataZmq, std::move(userData));
}
bool LtpOverUdpOutduct::Forward(std::vector<uint8_t> & movableDataVec, std::vector<uint8_t>&& userData) {
    return m_ltpBundleSourcePtr->Forward(movableDataVec, std::move(userData));
}

void LtpOverUdpOutduct::SetOnFailedBundleVecSendCallback(const OnFailedBundleVecSendCallback_t& callback) {
    m_ltpBundleSourcePtr->SetOnFailedBundleVecSendCallback(callback);
}
void LtpOverUdpOutduct::SetOnFailedBundleZmqSendCallback(const OnFailedBundleZmqSendCallback_t& callback) {
    m_ltpBundleSourcePtr->SetOnFailedBundleZmqSendCallback(callback);
}
void LtpOverUdpOutduct::SetOnSuccessfulBundleSendCallback(const OnSuccessfulBundleSendCallback_t& callback) {
    m_ltpBundleSourcePtr->SetOnSuccessfulBundleSendCallback(callback);
}
void LtpOverUdpOutduct::SetOnOutductLinkStatusChangedCallback(const OnOutductLinkStatusChangedCallback_t& callback) {
    m_ltpBundleSourcePtr->SetOnOutductLinkStatusChangedCallback(callback);
}
void LtpOverUdpOutduct::SetUserAssignedUuid(uint64_t userAssignedUuid) {
    m_ltpBundleSourcePtr->SetUserAssignedUuid(userAssignedUuid);
}

void LtpOverUdpOutduct::Connect() {

}
bool LtpOverUdpOutduct::ReadyToForward() {
    return true;
}
void LtpOverUdpOutduct::Stop() {
    m_ltpBundleSourcePtr->Stop();
}
void LtpOverUdpOutduct::GetOutductFinalStats(OutductFinalStats & finalStats) {
    finalStats.m_convergenceLayer = m_outductConfig.convergenceLayer;
    finalStats.m_totalDataSegmentsOrPacketsAcked = m_ltpBundleSourcePtr->GetTotalDataSegmentsAcked();
    finalStats.m_totalDataSegmentsOrPacketsSent = m_ltpBundleSourcePtr->GetTotalDataSegmentsSent();
}
uint64_t LtpOverUdpOutduct::GetOutductTelemetry(uint8_t* data, uint64_t bufferSize) {
    m_ltpBundleSourcePtr->SyncTelemetry();
    return m_ltpBundleSourcePtr->m_ltpOutductTelemetry.SerializeToLittleEndian(data, bufferSize);
}
