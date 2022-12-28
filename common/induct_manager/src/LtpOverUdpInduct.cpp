/**
 * @file LtpOverUdpInduct.cpp
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

#include "LtpOverUdpInduct.h"
#include <iostream>
#include <boost/make_unique.hpp>
#include <memory>
#include "LtpEngineConfig.h"

LtpOverUdpInduct::LtpOverUdpInduct(const InductProcessBundleCallback_t & inductProcessBundleCallback, const induct_element_config_t & inductConfig, const uint64_t maxBundleSizeBytes) :
    Induct(inductProcessBundleCallback, inductConfig)
{
    LtpEngineConfig ltpRxCfg;
    ltpRxCfg.thisEngineId = inductConfig.thisLtpEngineId;
    ltpRxCfg.remoteEngineId = inductConfig.remoteLtpEngineId; //expectedSessionOriginatorEngineId to be received
    ltpRxCfg.clientServiceId = inductConfig.clientServiceId; //not currently checked by induct
    ltpRxCfg.isInduct = true;
    ltpRxCfg.mtuClientServiceData = 1360; //unused for inducts
    ltpRxCfg.mtuReportSegment = inductConfig.ltpReportSegmentMtu;
    ltpRxCfg.oneWayLightTime = boost::posix_time::milliseconds(inductConfig.oneWayLightTimeMs);
    ltpRxCfg.oneWayMarginTime = boost::posix_time::milliseconds(inductConfig.oneWayMarginTimeMs);
    ltpRxCfg.remoteHostname = inductConfig.ltpRemoteUdpHostname;
    ltpRxCfg.remotePort = inductConfig.ltpRemoteUdpPort;
    ltpRxCfg.myBoundUdpPort = inductConfig.boundPort;
    ltpRxCfg.numUdpRxCircularBufferVectors = inductConfig.numRxCircularBufferElements;
    ltpRxCfg.estimatedBytesToReceivePerSession = inductConfig.preallocatedRedDataBytes;
    ltpRxCfg.maxRedRxBytesPerSession = maxBundleSizeBytes;
    ltpRxCfg.checkpointEveryNthDataPacketSender = 0; //unused for inducts
    ltpRxCfg.maxRetriesPerSerialNumber = inductConfig.ltpMaxRetriesPerSerialNumber;
    ltpRxCfg.force32BitRandomNumbers = (inductConfig.ltpRandomNumberSizeBits == 32);
    ltpRxCfg.maxSendRateBitsPerSecOrZeroToDisable = 0;
    ltpRxCfg.maxSimultaneousSessions = inductConfig.ltpMaxExpectedSimultaneousSessions;
    ltpRxCfg.rxDataSegmentSessionNumberRecreationPreventerHistorySizeOrZeroToDisable = inductConfig.ltpRxDataSegmentSessionNumberRecreationPreventerHistorySize;
    ltpRxCfg.maxUdpPacketsToSendPerSystemCall = inductConfig.ltpMaxUdpPacketsToSendPerSystemCall;
    ltpRxCfg.senderPingSecondsOrZeroToDisable = 0; //unused for inducts
    ltpRxCfg.delaySendingOfReportSegmentsTimeMsOrZeroToDisable = inductConfig.delaySendingOfReportSegmentsTimeMsOrZeroToDisable;
    ltpRxCfg.delaySendingOfDataSegmentsTimeMsOrZeroToDisable = 0; //unused for inducts (must be set to 0)
    ltpRxCfg.activeSessionDataOnDiskNewFileDurationMsOrZeroToDisable = (inductConfig.keepActiveSessionDataOnDisk) ? //for both inducts and outducts
        inductConfig.activeSessionDataOnDiskNewFileDurationMs : 0;
    ltpRxCfg.activeSessionDataOnDiskDirectory = inductConfig.activeSessionDataOnDiskDirectory; //for both inducts and outducts

    m_ltpBundleSinkPtr = boost::make_unique<LtpBundleSink>(m_inductProcessBundleCallback, ltpRxCfg); 

}
LtpOverUdpInduct::~LtpOverUdpInduct() {
    m_ltpBundleSinkPtr.reset();
}
