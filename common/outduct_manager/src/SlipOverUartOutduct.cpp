/**
 * @file SlipOverUartOutduct.cpp
 * @author  Brian Tomko <brian.j.tomko@nasa.gov>
 *
 * @copyright Copyright (c) 2021 United States Government as represented by
 * the National Aeronautics and Space Administration.
 * No copyright is claimed in the United States under Title 17, U.S.Code.
 * All Other Rights Reserved.
 *
 * @section LICENSE
 * Released under the NASA Open Source Agreement (NOSA)
 * See LICENSE.md in the source root directory for more information.
 */

#include "SlipOverUartOutduct.h"
#include <iostream>
#include <boost/make_unique.hpp>
#include <memory>
#include <boost/lexical_cast.hpp>
#include "Uri.h"

SlipOverUartOutduct::SlipOverUartOutduct(const outduct_element_config_t & outductConfig, const uint64_t outductUuid,
    const OutductOpportunisticProcessReceivedBundleCallback_t & outductOpportunisticProcessReceivedBundleCallback) :
    Outduct(outductConfig, outductUuid),
    m_uartInterface(outductConfig.comPort, //used as com port name
        outductConfig.baudRate,//baud
        5, //numRxCircularBufferVectors
        1000000, //maxRxBundleSizeBytes
        outductConfig.maxNumberOfBundlesInPipeline, //maxTxBundlesInFlight
        outductOpportunisticProcessReceivedBundleCallback)
{}
SlipOverUartOutduct::~SlipOverUartOutduct() {}

std::size_t SlipOverUartOutduct::GetTotalBundlesUnacked() const noexcept {
    return m_uartInterface.GetTotalBundlesUnacked();
}
bool SlipOverUartOutduct::Forward(const uint8_t* bundleData, const std::size_t size, std::vector<uint8_t>&& userData) {
    return m_uartInterface.Forward(bundleData, size, std::move(userData));
}
bool SlipOverUartOutduct::Forward(zmq::message_t & movableDataZmq, std::vector<uint8_t>&& userData) {
    return m_uartInterface.Forward(movableDataZmq, std::move(userData));
}
bool SlipOverUartOutduct::Forward(padded_vector_uint8_t& movableDataVec, std::vector<uint8_t>&& userData) {
    return m_uartInterface.Forward(movableDataVec, std::move(userData));
}

void SlipOverUartOutduct::SetOnFailedBundleVecSendCallback(const OnFailedBundleVecSendCallback_t& callback) {
    m_uartInterface.SetOnFailedBundleVecSendCallback(callback);
}
void SlipOverUartOutduct::SetOnFailedBundleZmqSendCallback(const OnFailedBundleZmqSendCallback_t& callback) {
    m_uartInterface.SetOnFailedBundleZmqSendCallback(callback);
}
void SlipOverUartOutduct::SetOnSuccessfulBundleSendCallback(const OnSuccessfulBundleSendCallback_t& callback) {
    m_uartInterface.SetOnSuccessfulBundleSendCallback(callback);
}
void SlipOverUartOutduct::SetOnOutductLinkStatusChangedCallback(const OnOutductLinkStatusChangedCallback_t& callback) {
    m_uartInterface.SetOnOutductLinkStatusChangedCallback(callback);
}
void SlipOverUartOutduct::SetUserAssignedUuid(uint64_t userAssignedUuid) {
    m_uartInterface.SetUserAssignedUuid(userAssignedUuid);
}

void SlipOverUartOutduct::Connect() {
    //no-op
}
bool SlipOverUartOutduct::ReadyToForward() {
    return m_uartInterface.ReadyToForward();
}
void SlipOverUartOutduct::Stop() {
    m_uartInterface.Stop();
}
void SlipOverUartOutduct::GetOutductFinalStats(OutductFinalStats & finalStats) {
    finalStats.m_convergenceLayer = m_outductConfig.convergenceLayer;
    finalStats.m_totalBundlesAcked = m_uartInterface.GetTotalBundlesAcked();
    finalStats.m_totalBundlesSent = m_uartInterface.GetTotalBundlesSent();
}
void SlipOverUartOutduct::PopulateOutductTelemetry(std::unique_ptr<OutductTelemetry_t>& outductTelem) {
    m_uartInterface.SyncTelemetry();
    outductTelem = boost::make_unique<SlipOverUartOutductTelemetry_t>(m_uartInterface.m_outductTelemetry);
    outductTelem->m_linkIsUpPerTimeSchedule = m_linkIsUpPerTimeSchedule;
}
