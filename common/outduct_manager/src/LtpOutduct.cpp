/**
 * @file LtpOutduct.cpp
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

#include "LtpOutduct.h"
#include <iostream>
#include <boost/make_unique.hpp>
#include <memory>
#include <boost/lexical_cast.hpp>

static constexpr bool hasPing(const outduct_element_config_t& cfg) {
    return cfg.ltpSenderPingSecondsOrZeroToDisable != 0;
}

LtpOutduct::LtpOutduct(const outduct_element_config_t& outductConfig, const uint64_t outductUuid) :
    Outduct(outductConfig, outductUuid, hasPing(outductConfig)),
    m_ltpBundleSourcePtr(NULL)
{

    
    m_ltpTxCfg.thisEngineId = outductConfig.thisLtpEngineId;
    m_ltpTxCfg.remoteEngineId = outductConfig.remoteLtpEngineId;
    m_ltpTxCfg.clientServiceId = outductConfig.clientServiceId;
    m_ltpTxCfg.isInduct = false;
    m_ltpTxCfg.mtuClientServiceData = outductConfig.ltpDataSegmentMtu;
    m_ltpTxCfg.mtuReportSegment = 1360; //unused for outducts
    m_ltpTxCfg.oneWayLightTime = boost::posix_time::milliseconds(outductConfig.oneWayLightTimeMs);
    m_ltpTxCfg.oneWayMarginTime = boost::posix_time::milliseconds(outductConfig.oneWayMarginTimeMs);
    m_ltpTxCfg.remoteHostname = m_outductConfig.remoteHostname;
    m_ltpTxCfg.remotePort = m_outductConfig.remotePort;
    m_ltpTxCfg.myBoundUdpPort = outductConfig.ltpSenderBoundPort;
    m_ltpTxCfg.numUdpRxCircularBufferVectors = outductConfig.numRxCircularBufferElements;
    m_ltpTxCfg.estimatedBytesToReceivePerSession = 0; //unused for outducts
    m_ltpTxCfg.maxRedRxBytesPerSession = 0; //unused for outducts
    m_ltpTxCfg.checkpointEveryNthDataPacketSender = outductConfig.ltpCheckpointEveryNthDataSegment;
    m_ltpTxCfg.maxRetriesPerSerialNumber = outductConfig.ltpMaxRetriesPerSerialNumber;
    m_ltpTxCfg.force32BitRandomNumbers = (outductConfig.ltpRandomNumberSizeBits == 32);
    m_ltpTxCfg.maxSendRateBitsPerSecOrZeroToDisable = 0; // Set by contact plan (or commandline arg for apps)
    m_ltpTxCfg.maxSimultaneousSessions = m_outductConfig.maxNumberOfBundlesInPipeline;
    m_ltpTxCfg.rxDataSegmentSessionNumberRecreationPreventerHistorySizeOrZeroToDisable = 0; //unused for outducts
    m_ltpTxCfg.maxUdpPacketsToSendPerSystemCall = m_outductConfig.ltpMaxUdpPacketsToSendPerSystemCall;
    m_ltpTxCfg.senderPingSecondsOrZeroToDisable = m_outductConfig.ltpSenderPingSecondsOrZeroToDisable;
    m_ltpTxCfg.delaySendingOfReportSegmentsTimeMsOrZeroToDisable = 0; //unused for outducts
    m_ltpTxCfg.delaySendingOfDataSegmentsTimeMsOrZeroToDisable = m_outductConfig.delaySendingOfDataSegmentsTimeMsOrZeroToDisable;
    m_ltpTxCfg.activeSessionDataOnDiskNewFileDurationMsOrZeroToDisable = (m_outductConfig.keepActiveSessionDataOnDisk) ? //for both inducts and outducts
        m_outductConfig.activeSessionDataOnDiskNewFileDurationMs : 0;
    m_ltpTxCfg.activeSessionDataOnDiskDirectory = m_outductConfig.activeSessionDataOnDiskDirectory; //for both inducts and outducts

}
LtpOutduct::~LtpOutduct() {}

bool LtpOutduct::Init() {
    return SetLtpBundleSourcePtr(); //virtual function call
}

std::size_t LtpOutduct::GetTotalDataSegmentsUnacked() {
    return m_ltpBundleSourcePtr->GetTotalDataSegmentsUnacked();
}
bool LtpOutduct::Forward(const uint8_t* bundleData, const std::size_t size, std::vector<uint8_t>&& userData) {
    return m_ltpBundleSourcePtr->Forward(bundleData, size, std::move(userData));
}
bool LtpOutduct::Forward(zmq::message_t & movableDataZmq, std::vector<uint8_t>&& userData) {
    return m_ltpBundleSourcePtr->Forward(movableDataZmq, std::move(userData));
}
bool LtpOutduct::Forward(padded_vector_uint8_t& movableDataVec, std::vector<uint8_t>&& userData) {
    return m_ltpBundleSourcePtr->Forward(movableDataVec, std::move(userData));
}

void LtpOutduct::SetOnFailedBundleVecSendCallback(const OnFailedBundleVecSendCallback_t& callback) {
    m_ltpBundleSourcePtr->SetOnFailedBundleVecSendCallback(callback);
}
void LtpOutduct::SetOnFailedBundleZmqSendCallback(const OnFailedBundleZmqSendCallback_t& callback) {
    m_ltpBundleSourcePtr->SetOnFailedBundleZmqSendCallback(callback);
}
void LtpOutduct::SetOnSuccessfulBundleSendCallback(const OnSuccessfulBundleSendCallback_t& callback) {
    m_ltpBundleSourcePtr->SetOnSuccessfulBundleSendCallback(callback);
}
void LtpOutduct::SetOnOutductLinkStatusChangedCallback(const OnOutductLinkStatusChangedCallback_t& callback) {
    m_ltpBundleSourcePtr->SetOnOutductLinkStatusChangedCallback(callback);
}
void LtpOutduct::SetUserAssignedUuid(uint64_t userAssignedUuid) {
    m_ltpBundleSourcePtr->SetUserAssignedUuid(userAssignedUuid);
}
void LtpOutduct::SetRate(uint64_t maxSendRateBitsPerSecOrZeroToDisable) {
    m_ltpBundleSourcePtr->SetRate(maxSendRateBitsPerSecOrZeroToDisable);
}
uint64_t LtpOutduct::GetOutductMaxNumberOfBundlesInPipeline() const {
    return m_ltpBundleSourcePtr->GetOutductMaxNumberOfBundlesInPipeline();
}

void LtpOutduct::Connect() {

}
bool LtpOutduct::ReadyToForward() {
    return true;
}
void LtpOutduct::Stop() {
    m_ltpBundleSourcePtr->Stop();
}
void LtpOutduct::GetOutductFinalStats(OutductFinalStats & finalStats) {
    finalStats.m_convergenceLayer = m_outductConfig.convergenceLayer;
    finalStats.m_totalDataSegmentsOrPacketsAcked = m_ltpBundleSourcePtr->GetTotalDataSegmentsAcked();
    finalStats.m_totalDataSegmentsOrPacketsSent = m_ltpBundleSourcePtr->GetTotalDataSegmentsSent();
}
void LtpOutduct::PopulateOutductTelemetry(std::unique_ptr<OutductTelemetry_t>& outductTelem) {
    m_ltpBundleSourcePtr->SyncTelemetry();
    outductTelem = boost::make_unique<LtpOutductTelemetry_t>(m_ltpBundleSourcePtr->m_ltpOutductTelemetry);
    outductTelem->m_linkIsUpPerTimeSchedule = m_linkIsUpPerTimeSchedule;
    outductTelem->m_linkIsUpPhysically = m_linkIsUpPhysically;
}
