/**
 * @file UdpOutduct.cpp
 * @author  Brian Tomko <brian.j.tomko@nasa.gov>
 *
 * @copyright Copyright (c) 2026 United States Government as represented by
 * the National Aeronautics and Space Administration.
 * No copyright is claimed in the United States under Title 17, U.S.Code.
 * All Other Rights Reserved.
 *
 * @section LICENSE
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "UdpOutduct.h"
#include <iostream>
#include <boost/make_unique.hpp>
#include <memory>
#include <boost/lexical_cast.hpp>

UdpOutduct::UdpOutduct(const outduct_element_config_t & outductConfig, const uint64_t outductUuid) :
    Outduct(outductConfig, outductUuid, false),
    m_udpBundleSource(outductConfig.maxNumberOfBundlesInPipeline + 5, outductConfig.rateLimitPrecisionMicroSec)
{}
UdpOutduct::~UdpOutduct() {}

std::size_t UdpOutduct::GetTotalBundlesUnacked() const noexcept {
    return m_udpBundleSource.GetTotalUdpPacketsUnacked();
}
bool UdpOutduct::Forward(const uint8_t* bundleData, const std::size_t size, std::vector<uint8_t>&& userData) {
    return m_udpBundleSource.Forward(bundleData, size, std::move(userData));
}
bool UdpOutduct::Forward(zmq::message_t & movableDataZmq, std::vector<uint8_t>&& userData) {
    return m_udpBundleSource.Forward(movableDataZmq, std::move(userData));
}
bool UdpOutduct::Forward(padded_vector_uint8_t& movableDataVec, std::vector<uint8_t>&& userData) {
    return m_udpBundleSource.Forward(movableDataVec, std::move(userData));
}

void UdpOutduct::SetOnFailedBundleVecSendCallback(const OnFailedBundleVecSendCallback_t& callback) {
    m_udpBundleSource.SetOnFailedBundleVecSendCallback(callback);
}
void UdpOutduct::SetOnFailedBundleZmqSendCallback(const OnFailedBundleZmqSendCallback_t& callback) {
    m_udpBundleSource.SetOnFailedBundleZmqSendCallback(callback);
}
void UdpOutduct::SetOnSuccessfulBundleSendCallback(const OnSuccessfulBundleSendCallback_t& callback) {
    m_udpBundleSource.SetOnSuccessfulBundleSendCallback(callback);
}
void UdpOutduct::SetOnOutductLinkStatusChangedCallback(const OnOutductLinkStatusChangedCallback_t& callback) {
    m_udpBundleSource.SetOnOutductLinkStatusChangedCallback(callback);
}
void UdpOutduct::SetUserAssignedUuid(uint64_t userAssignedUuid) {
    m_udpBundleSource.SetUserAssignedUuid(userAssignedUuid);
}
void UdpOutduct::SetRate(uint64_t maxSendRateBitsPerSecOrZeroToDisable) {
    m_udpBundleSource.UpdateRate(maxSendRateBitsPerSecOrZeroToDisable);
}

void UdpOutduct::Connect() {
    m_udpBundleSource.Connect(m_outductConfig.remoteHostname, boost::lexical_cast<std::string>(m_outductConfig.remotePort));
}
bool UdpOutduct::ReadyToForward() {
    return m_udpBundleSource.ReadyToForward();
}
void UdpOutduct::Stop() {
    m_udpBundleSource.Stop();
}
void UdpOutduct::GetOutductFinalStats(OutductFinalStats & finalStats) {
    finalStats.m_convergenceLayer = m_outductConfig.convergenceLayer;
    finalStats.m_totalBundlesAcked = m_udpBundleSource.GetTotalUdpPacketsAcked();
    finalStats.m_totalBundlesSent = m_udpBundleSource.GetTotalUdpPacketsSent();
}
void UdpOutduct::PopulateOutductTelemetry(std::unique_ptr<OutductTelemetry_t>& outductTelem) {
    std::unique_ptr<UdpOutductTelemetry_t> t = boost::make_unique<UdpOutductTelemetry_t>();
    m_udpBundleSource.GetTelemetry(*t);
    outductTelem = std::move(t);
    outductTelem->m_linkIsUpPerTimeSchedule = m_linkIsUpPerTimeSchedule;
}
