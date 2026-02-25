/**
 * @file LtpInduct.cpp
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

#include "LtpInduct.h"
#include <iostream>
#include <boost/make_unique.hpp>
#include <memory>


LtpInduct::LtpInduct(const InductProcessBundleCallback_t & inductProcessBundleCallback, const induct_element_config_t & inductConfig, const uint64_t maxBundleSizeBytes) :
    Induct(inductProcessBundleCallback, inductConfig)
{
    
    m_ltpRxCfg.thisEngineId = inductConfig.thisLtpEngineId;
    m_ltpRxCfg.remoteEngineId = inductConfig.remoteLtpEngineId; //expectedSessionOriginatorEngineId to be received
    m_ltpRxCfg.clientServiceId = inductConfig.clientServiceId; //not currently checked by induct
    m_ltpRxCfg.isInduct = true;
    m_ltpRxCfg.mtuClientServiceData = 1360; //unused for inducts
    m_ltpRxCfg.mtuReportSegment = inductConfig.ltpReportSegmentMtu;
    m_ltpRxCfg.oneWayLightTime = boost::posix_time::milliseconds(inductConfig.oneWayLightTimeMs);
    m_ltpRxCfg.oneWayMarginTime = boost::posix_time::milliseconds(inductConfig.oneWayMarginTimeMs);
    m_ltpRxCfg.remoteHostname = inductConfig.ltpRemoteUdpHostname;
    m_ltpRxCfg.remotePort = inductConfig.ltpRemoteUdpPort;
    m_ltpRxCfg.myBoundUdpPort = inductConfig.boundPort;
    m_ltpRxCfg.encapLocalSocketOrPipePath = inductConfig.ltpEncapLocalSocketOrPipePath;
    m_ltpRxCfg.numUdpRxCircularBufferVectors = inductConfig.numRxCircularBufferElements;
    m_ltpRxCfg.estimatedBytesToReceivePerSession = inductConfig.preallocatedRedDataBytes;
    m_ltpRxCfg.maxRedRxBytesPerSession = maxBundleSizeBytes;
    m_ltpRxCfg.checkpointEveryNthDataPacketSender = 0; //unused for inducts
    m_ltpRxCfg.maxRetriesPerSerialNumber = inductConfig.ltpMaxRetriesPerSerialNumber;
    m_ltpRxCfg.force32BitRandomNumbers = (inductConfig.ltpRandomNumberSizeBits == 32);
    m_ltpRxCfg.maxSendRateBitsPerSecOrZeroToDisable = 0; //always disable rate for report segments, etc
    m_ltpRxCfg.maxSimultaneousSessions = inductConfig.ltpMaxExpectedSimultaneousSessions;
    m_ltpRxCfg.rxDataSegmentSessionNumberRecreationPreventerHistorySizeOrZeroToDisable = inductConfig.ltpRxDataSegmentSessionNumberRecreationPreventerHistorySize;
    m_ltpRxCfg.maxUdpPacketsToSendPerSystemCall = inductConfig.ltpMaxUdpPacketsToSendPerSystemCall;
    m_ltpRxCfg.senderPingSecondsOrZeroToDisable = 0; //unused for inducts
    m_ltpRxCfg.delaySendingOfReportSegmentsTimeMsOrZeroToDisable = inductConfig.delaySendingOfReportSegmentsTimeMsOrZeroToDisable;
    m_ltpRxCfg.delaySendingOfDataSegmentsTimeMsOrZeroToDisable = 0; //unused for inducts (must be set to 0)
    m_ltpRxCfg.activeSessionDataOnDiskNewFileDurationMsOrZeroToDisable = (inductConfig.keepActiveSessionDataOnDisk) ? //for both inducts and outducts
        inductConfig.activeSessionDataOnDiskNewFileDurationMs : 0;
    m_ltpRxCfg.activeSessionDataOnDiskDirectory = inductConfig.activeSessionDataOnDiskDirectory; //for both inducts and outducts
    m_ltpRxCfg.rateLimitPrecisionMicroSec = 0; //unused for inducts

}

bool LtpInduct::Init() {
    return SetLtpBundleSinkPtr(); //virtual function call
}

LtpInduct::~LtpInduct() {}

void LtpInduct::PopulateInductTelemetry(InductTelemetry_t& inductTelem) {
    inductTelem.m_convergenceLayer = m_inductConfig.convergenceLayer;
    inductTelem.m_listInductConnections.clear();
    if (m_ltpBundleSinkPtr) {
        std::unique_ptr<LtpInductConnectionTelemetry_t> t = boost::make_unique<LtpInductConnectionTelemetry_t>();
        m_ltpBundleSinkPtr->GetTelemetry(*t);
        inductTelem.m_listInductConnections.emplace_back(std::move(t));
    }
    else {
        std::unique_ptr<LtpInductConnectionTelemetry_t> c = boost::make_unique<LtpInductConnectionTelemetry_t>();
        c->m_connectionName = "null";
        c->m_inputName = std::string("*:") + boost::lexical_cast<std::string>(m_ltpRxCfg.myBoundUdpPort);
        inductTelem.m_listInductConnections.emplace_back(std::move(c));
    }
}
