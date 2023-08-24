/**
 * @file LtpInduct.cpp
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
    m_ltpRxCfg.maxSendRateBitsPerSecOrZeroToDisable = 0;
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
