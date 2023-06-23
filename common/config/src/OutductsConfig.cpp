/**
 * @file OutductsConfig.cpp
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

#include "OutductsConfig.h"
#include "Logger.h"
#include <memory>
#include <boost/foreach.hpp>
#include <boost/lexical_cast.hpp>
#include "Uri.h"
#ifndef _WIN32
#include <sys/socket.h> //for ltpMaxUdpPacketsToSendPerSystemCall checks
#endif

static constexpr hdtn::Logger::SubProcess subprocess = hdtn::Logger::SubProcess::none;

static const std::vector<std::string> VALID_CONVERGENCE_LAYER_NAMES = { "ltp_over_udp", "ltp_over_ipc", "udp", "stcp", "tcpcl_v3", "tcpcl_v4" };

outduct_element_config_t::outduct_element_config_t() :
    name(""),
    convergenceLayer(""),
    nextHopNodeId(0),
    remoteHostname(""),
    remotePort(0),
    maxNumberOfBundlesInPipeline(0),
    maxSumOfBundleBytesInPipeline(0),
    
    thisLtpEngineId(0),
    remoteLtpEngineId(0),
    ltpDataSegmentMtu(0),
    oneWayLightTimeMs(0),
    oneWayMarginTimeMs(0),
    clientServiceId(0),
    numRxCircularBufferElements(0),
    ltpMaxRetriesPerSerialNumber(0),
    ltpCheckpointEveryNthDataSegment(0),
    ltpRandomNumberSizeBits(0),
    ltpSenderBoundPort(0),
    ltpMaxUdpPacketsToSendPerSystemCall(0),
    ltpSenderPingSecondsOrZeroToDisable(0),
    delaySendingOfDataSegmentsTimeMsOrZeroToDisable(20),
    keepActiveSessionDataOnDisk(false),
    activeSessionDataOnDiskNewFileDurationMs(2000),
    activeSessionDataOnDiskDirectory("./"),

    keepAliveIntervalSeconds(0),
    tcpclV3MyMaxTxSegmentSizeBytes(0),
    tcpclAllowOpportunisticReceiveBundles(true),

    tcpclV4MyMaxRxSegmentSizeBytes(0),
    tryUseTls(false),
    tlsIsRequired(false),
    useTlsVersion1_3(false),
    doX509CertificateVerification(false),
    verifySubjectAltNameInX509Certificate(false),
    certificationAuthorityPemFileForVerification("") {}

outduct_element_config_t::~outduct_element_config_t() {}


//a copy constructor: X(const X&)
outduct_element_config_t::outduct_element_config_t(const outduct_element_config_t& o) :
    name(o.name),
    convergenceLayer(o.convergenceLayer),
    nextHopNodeId(o.nextHopNodeId),
    remoteHostname(o.remoteHostname),
    remotePort(o.remotePort),
    maxNumberOfBundlesInPipeline(o.maxNumberOfBundlesInPipeline),
    maxSumOfBundleBytesInPipeline(o.maxSumOfBundleBytesInPipeline),
    
    thisLtpEngineId(o.thisLtpEngineId),
    remoteLtpEngineId(o.remoteLtpEngineId),
    ltpDataSegmentMtu(o.ltpDataSegmentMtu),
    oneWayLightTimeMs(o.oneWayLightTimeMs),
    oneWayMarginTimeMs(o.oneWayMarginTimeMs),
    clientServiceId(o.clientServiceId),
    numRxCircularBufferElements(o.numRxCircularBufferElements),
    ltpMaxRetriesPerSerialNumber(o.ltpMaxRetriesPerSerialNumber),
    ltpCheckpointEveryNthDataSegment(o.ltpCheckpointEveryNthDataSegment),
    ltpRandomNumberSizeBits(o.ltpRandomNumberSizeBits),
    ltpSenderBoundPort(o.ltpSenderBoundPort),
    ltpMaxUdpPacketsToSendPerSystemCall(o.ltpMaxUdpPacketsToSendPerSystemCall),
    ltpSenderPingSecondsOrZeroToDisable(o.ltpSenderPingSecondsOrZeroToDisable),
    delaySendingOfDataSegmentsTimeMsOrZeroToDisable(o.delaySendingOfDataSegmentsTimeMsOrZeroToDisable),
    keepActiveSessionDataOnDisk(o.keepActiveSessionDataOnDisk),
    activeSessionDataOnDiskNewFileDurationMs(o.activeSessionDataOnDiskNewFileDurationMs),
    activeSessionDataOnDiskDirectory(o.activeSessionDataOnDiskDirectory),

    keepAliveIntervalSeconds(o.keepAliveIntervalSeconds),
    tcpclV3MyMaxTxSegmentSizeBytes(o.tcpclV3MyMaxTxSegmentSizeBytes),
    tcpclAllowOpportunisticReceiveBundles(o.tcpclAllowOpportunisticReceiveBundles),

    tcpclV4MyMaxRxSegmentSizeBytes(o.tcpclV4MyMaxRxSegmentSizeBytes),
    tryUseTls(o.tryUseTls),
    tlsIsRequired(o.tlsIsRequired),
    useTlsVersion1_3(o.useTlsVersion1_3),
    doX509CertificateVerification(o.doX509CertificateVerification),
    verifySubjectAltNameInX509Certificate(o.verifySubjectAltNameInX509Certificate),
    certificationAuthorityPemFileForVerification(o.certificationAuthorityPemFileForVerification) { }

//a move constructor: X(X&&)
outduct_element_config_t::outduct_element_config_t(outduct_element_config_t&& o) noexcept :
    name(std::move(o.name)),
    convergenceLayer(std::move(o.convergenceLayer)),
    nextHopNodeId(o.nextHopNodeId),
    remoteHostname(std::move(o.remoteHostname)),
    remotePort(o.remotePort),
    maxNumberOfBundlesInPipeline(o.maxNumberOfBundlesInPipeline),
    maxSumOfBundleBytesInPipeline(o.maxSumOfBundleBytesInPipeline),
    
    thisLtpEngineId(o.thisLtpEngineId),
    remoteLtpEngineId(o.remoteLtpEngineId),
    ltpDataSegmentMtu(o.ltpDataSegmentMtu),
    oneWayLightTimeMs(o.oneWayLightTimeMs),
    oneWayMarginTimeMs(o.oneWayMarginTimeMs),
    clientServiceId(o.clientServiceId),
    numRxCircularBufferElements(o.numRxCircularBufferElements),
    ltpMaxRetriesPerSerialNumber(o.ltpMaxRetriesPerSerialNumber),
    ltpCheckpointEveryNthDataSegment(o.ltpCheckpointEveryNthDataSegment),
    ltpRandomNumberSizeBits(o.ltpRandomNumberSizeBits),
    ltpSenderBoundPort(o.ltpSenderBoundPort),
    ltpMaxUdpPacketsToSendPerSystemCall(o.ltpMaxUdpPacketsToSendPerSystemCall),
    ltpSenderPingSecondsOrZeroToDisable(o.ltpSenderPingSecondsOrZeroToDisable),
    delaySendingOfDataSegmentsTimeMsOrZeroToDisable(o.delaySendingOfDataSegmentsTimeMsOrZeroToDisable),
    keepActiveSessionDataOnDisk(o.keepActiveSessionDataOnDisk),
    activeSessionDataOnDiskNewFileDurationMs(o.activeSessionDataOnDiskNewFileDurationMs),
    activeSessionDataOnDiskDirectory(std::move(o.activeSessionDataOnDiskDirectory)),

    keepAliveIntervalSeconds(o.keepAliveIntervalSeconds),
    tcpclV3MyMaxTxSegmentSizeBytes(o.tcpclV3MyMaxTxSegmentSizeBytes),
    tcpclAllowOpportunisticReceiveBundles(o.tcpclAllowOpportunisticReceiveBundles),

    tcpclV4MyMaxRxSegmentSizeBytes(o.tcpclV4MyMaxRxSegmentSizeBytes),
    tryUseTls(o.tryUseTls),
    tlsIsRequired(o.tlsIsRequired),
    useTlsVersion1_3(o.useTlsVersion1_3),
    doX509CertificateVerification(o.doX509CertificateVerification),
    verifySubjectAltNameInX509Certificate(o.verifySubjectAltNameInX509Certificate),
    certificationAuthorityPemFileForVerification(std::move(o.certificationAuthorityPemFileForVerification)) { }

//a copy assignment: operator=(const X&)
outduct_element_config_t& outduct_element_config_t::operator=(const outduct_element_config_t& o) {
    name = o.name;
    convergenceLayer = o.convergenceLayer;
    nextHopNodeId = o.nextHopNodeId;
    remoteHostname = o.remoteHostname;
    remotePort = o.remotePort;
    maxNumberOfBundlesInPipeline = o.maxNumberOfBundlesInPipeline;
    maxSumOfBundleBytesInPipeline = o.maxSumOfBundleBytesInPipeline;
    
    thisLtpEngineId = o.thisLtpEngineId;
    remoteLtpEngineId = o.remoteLtpEngineId;
    ltpDataSegmentMtu = o.ltpDataSegmentMtu;
    oneWayLightTimeMs = o.oneWayLightTimeMs;
    oneWayMarginTimeMs = o.oneWayMarginTimeMs;
    clientServiceId = o.clientServiceId;
    numRxCircularBufferElements = o.numRxCircularBufferElements;
    ltpMaxRetriesPerSerialNumber = o.ltpMaxRetriesPerSerialNumber;
    ltpCheckpointEveryNthDataSegment = o.ltpCheckpointEveryNthDataSegment;
    ltpRandomNumberSizeBits = o.ltpRandomNumberSizeBits;
    ltpSenderBoundPort = o.ltpSenderBoundPort;
    ltpMaxUdpPacketsToSendPerSystemCall = o.ltpMaxUdpPacketsToSendPerSystemCall;
    ltpSenderPingSecondsOrZeroToDisable = o.ltpSenderPingSecondsOrZeroToDisable;
    delaySendingOfDataSegmentsTimeMsOrZeroToDisable = o.delaySendingOfDataSegmentsTimeMsOrZeroToDisable;
    keepActiveSessionDataOnDisk = o.keepActiveSessionDataOnDisk;
    activeSessionDataOnDiskNewFileDurationMs = o.activeSessionDataOnDiskNewFileDurationMs;
    activeSessionDataOnDiskDirectory = o.activeSessionDataOnDiskDirectory;

    keepAliveIntervalSeconds = o.keepAliveIntervalSeconds;

    tcpclV3MyMaxTxSegmentSizeBytes = o.tcpclV3MyMaxTxSegmentSizeBytes;
    tcpclAllowOpportunisticReceiveBundles = o.tcpclAllowOpportunisticReceiveBundles;

    tcpclV4MyMaxRxSegmentSizeBytes = o.tcpclV4MyMaxRxSegmentSizeBytes;
    tryUseTls = o.tryUseTls;
    tlsIsRequired = o.tlsIsRequired;
    useTlsVersion1_3 = o.useTlsVersion1_3;
    doX509CertificateVerification = o.doX509CertificateVerification;
    verifySubjectAltNameInX509Certificate = o.verifySubjectAltNameInX509Certificate;
    certificationAuthorityPemFileForVerification = o.certificationAuthorityPemFileForVerification;
    return *this;
}

//a move assignment: operator=(X&&)
outduct_element_config_t& outduct_element_config_t::operator=(outduct_element_config_t&& o) noexcept {
    name = std::move(o.name);
    convergenceLayer = std::move(o.convergenceLayer);
    nextHopNodeId = o.nextHopNodeId;
    remoteHostname = std::move(o.remoteHostname);
    remotePort = o.remotePort;
    maxNumberOfBundlesInPipeline = o.maxNumberOfBundlesInPipeline;
    maxSumOfBundleBytesInPipeline = o.maxSumOfBundleBytesInPipeline;

    thisLtpEngineId = o.thisLtpEngineId;
    remoteLtpEngineId = o.remoteLtpEngineId;
    ltpDataSegmentMtu = o.ltpDataSegmentMtu;
    oneWayLightTimeMs = o.oneWayLightTimeMs;
    oneWayMarginTimeMs = o.oneWayMarginTimeMs;
    clientServiceId = o.clientServiceId;
    numRxCircularBufferElements = o.numRxCircularBufferElements;
    ltpMaxRetriesPerSerialNumber = o.ltpMaxRetriesPerSerialNumber;
    ltpCheckpointEveryNthDataSegment = o.ltpCheckpointEveryNthDataSegment;
    ltpRandomNumberSizeBits = o.ltpRandomNumberSizeBits;
    ltpSenderBoundPort = o.ltpSenderBoundPort;
    ltpMaxUdpPacketsToSendPerSystemCall = o.ltpMaxUdpPacketsToSendPerSystemCall;
    ltpSenderPingSecondsOrZeroToDisable = o.ltpSenderPingSecondsOrZeroToDisable;
    delaySendingOfDataSegmentsTimeMsOrZeroToDisable = o.delaySendingOfDataSegmentsTimeMsOrZeroToDisable;
    keepActiveSessionDataOnDisk = o.keepActiveSessionDataOnDisk;
    activeSessionDataOnDiskNewFileDurationMs = o.activeSessionDataOnDiskNewFileDurationMs;
    activeSessionDataOnDiskDirectory = std::move(o.activeSessionDataOnDiskDirectory);

    keepAliveIntervalSeconds = o.keepAliveIntervalSeconds;

    tcpclV3MyMaxTxSegmentSizeBytes = o.tcpclV3MyMaxTxSegmentSizeBytes;
    tcpclAllowOpportunisticReceiveBundles = o.tcpclAllowOpportunisticReceiveBundles;

    tcpclV4MyMaxRxSegmentSizeBytes = o.tcpclV4MyMaxRxSegmentSizeBytes;
    tryUseTls = o.tryUseTls;
    tlsIsRequired = o.tlsIsRequired;
    useTlsVersion1_3 = o.useTlsVersion1_3;
    doX509CertificateVerification = o.doX509CertificateVerification;
    verifySubjectAltNameInX509Certificate = o.verifySubjectAltNameInX509Certificate;
    certificationAuthorityPemFileForVerification = std::move(o.certificationAuthorityPemFileForVerification);
    return *this;
}

bool outduct_element_config_t::operator==(const outduct_element_config_t & o) const {
    return (name == o.name) &&
        (convergenceLayer == o.convergenceLayer) &&
        (nextHopNodeId == o.nextHopNodeId) &&
        (remoteHostname == o.remoteHostname) &&
        (remotePort == o.remotePort) &&
        (maxNumberOfBundlesInPipeline == o.maxNumberOfBundlesInPipeline) &&
        (maxSumOfBundleBytesInPipeline == o.maxSumOfBundleBytesInPipeline) &&
        
        (thisLtpEngineId == o.thisLtpEngineId) &&
        (remoteLtpEngineId == o.remoteLtpEngineId) &&
        (ltpDataSegmentMtu == o.ltpDataSegmentMtu) &&
        (oneWayLightTimeMs == o.oneWayLightTimeMs) &&
        (oneWayMarginTimeMs == o.oneWayMarginTimeMs) &&
        (clientServiceId == o.clientServiceId) &&
        (numRxCircularBufferElements == o.numRxCircularBufferElements) &&
        (ltpMaxRetriesPerSerialNumber == o.ltpMaxRetriesPerSerialNumber) &&
        (ltpCheckpointEveryNthDataSegment == o.ltpCheckpointEveryNthDataSegment) &&
        (ltpRandomNumberSizeBits == o.ltpRandomNumberSizeBits) &&
        (ltpSenderBoundPort == o.ltpSenderBoundPort) &&
        (ltpMaxUdpPacketsToSendPerSystemCall == o.ltpMaxUdpPacketsToSendPerSystemCall) &&
        (ltpSenderPingSecondsOrZeroToDisable == o.ltpSenderPingSecondsOrZeroToDisable) &&
        (delaySendingOfDataSegmentsTimeMsOrZeroToDisable == o.delaySendingOfDataSegmentsTimeMsOrZeroToDisable) &&
        (keepActiveSessionDataOnDisk == o.keepActiveSessionDataOnDisk) &&
        (activeSessionDataOnDiskNewFileDurationMs == o.activeSessionDataOnDiskNewFileDurationMs) &&
        (activeSessionDataOnDiskDirectory == o.activeSessionDataOnDiskDirectory) &&

        (keepAliveIntervalSeconds == o.keepAliveIntervalSeconds) &&
        
        (tcpclV3MyMaxTxSegmentSizeBytes == o.tcpclV3MyMaxTxSegmentSizeBytes) &&
        (tcpclAllowOpportunisticReceiveBundles == o.tcpclAllowOpportunisticReceiveBundles) &&
        
        (tcpclV4MyMaxRxSegmentSizeBytes == o.tcpclV4MyMaxRxSegmentSizeBytes) &&
        (tryUseTls == o.tryUseTls) &&
        (tlsIsRequired == o.tlsIsRequired) &&
        (useTlsVersion1_3 == o.useTlsVersion1_3) &&
        (doX509CertificateVerification == o.doX509CertificateVerification) &&
        (verifySubjectAltNameInX509Certificate == o.verifySubjectAltNameInX509Certificate) &&
        (certificationAuthorityPemFileForVerification == o.certificationAuthorityPemFileForVerification);
}

OutductsConfig::OutductsConfig() {
}

OutductsConfig::~OutductsConfig() {
}

//a copy constructor: X(const X&)
OutductsConfig::OutductsConfig(const OutductsConfig& o) :
    m_outductConfigName(o.m_outductConfigName), m_outductElementConfigVector(o.m_outductElementConfigVector) { }

//a move constructor: X(X&&)
OutductsConfig::OutductsConfig(OutductsConfig&& o) noexcept :
    m_outductConfigName(std::move(o.m_outductConfigName)), m_outductElementConfigVector(std::move(o.m_outductElementConfigVector)) { }

//a copy assignment: operator=(const X&)
OutductsConfig& OutductsConfig::operator=(const OutductsConfig& o) {
    m_outductConfigName = o.m_outductConfigName;
    m_outductElementConfigVector = o.m_outductElementConfigVector;
    return *this;
}

//a move assignment: operator=(X&&)
OutductsConfig& OutductsConfig::operator=(OutductsConfig&& o) noexcept {
    m_outductConfigName = std::move(o.m_outductConfigName);
    m_outductElementConfigVector = std::move(o.m_outductElementConfigVector);
    return *this;
}

bool OutductsConfig::operator==(const OutductsConfig & o) const {
    return (m_outductConfigName == o.m_outductConfigName) && (m_outductElementConfigVector == o.m_outductElementConfigVector);
}

bool OutductsConfig::SetValuesFromPropertyTree(const boost::property_tree::ptree & pt) {
    m_outductConfigName = pt.get<std::string>("outductConfigName", ""); //non-throw version
    if (m_outductConfigName == "") {
        LOG_ERROR(subprocess) << "outductConfigName must be defined and not empty string";
        return false;
    }

    //for non-throw versions of get_child which return a reference to the second parameter
    static const boost::property_tree::ptree EMPTY_PTREE;

    const boost::property_tree::ptree & outductElementConfigVectorPt = pt.get_child("outductVector", EMPTY_PTREE); //non-throw version
    m_outductElementConfigVector.resize(outductElementConfigVectorPt.size());
    unsigned int vectorIndex = 0;
    BOOST_FOREACH(const boost::property_tree::ptree::value_type & outductElementConfigPt, outductElementConfigVectorPt) {
        outduct_element_config_t & outductElementConfig = m_outductElementConfigVector[vectorIndex++];
        try {
            outductElementConfig.name = outductElementConfigPt.second.get<std::string>("name");
            outductElementConfig.convergenceLayer = outductElementConfigPt.second.get<std::string>("convergenceLayer");
            outductElementConfig.nextHopNodeId = outductElementConfigPt.second.get<uint64_t>("nextHopNodeId");
            {
                bool found = false;
                for (std::vector<std::string>::const_iterator it = VALID_CONVERGENCE_LAYER_NAMES.cbegin(); it != VALID_CONVERGENCE_LAYER_NAMES.cend(); ++it) {
                    if (outductElementConfig.convergenceLayer == *it) {
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    LOG_ERROR(subprocess) << "error parsing JSON outductVector[" << (vectorIndex - 1) << "]: " << "invalid convergence layer " << outductElementConfig.convergenceLayer;
                    return false;
                }
            }
            outductElementConfig.remoteHostname = outductElementConfigPt.second.get<std::string>("remoteHostname");
            if (outductElementConfig.remoteHostname == "") {
                LOG_ERROR(subprocess) << "error parsing JSON outductVector[" << (vectorIndex - 1) << "]: " << "invalid outduct remoteHostname, must not be empty";
                return false;
            }
            outductElementConfig.remotePort = outductElementConfigPt.second.get<uint16_t>("remotePort");
            if (outductElementConfig.remotePort == 0) {
                LOG_ERROR(subprocess) << "error parsing JSON outductVector[" << (vectorIndex - 1) << "]: " << "invalid remotePort, must be non-zero";
                return false;
            }
            outductElementConfig.maxNumberOfBundlesInPipeline = outductElementConfigPt.second.get<uint32_t>("maxNumberOfBundlesInPipeline");
            outductElementConfig.maxSumOfBundleBytesInPipeline = outductElementConfigPt.second.get<uint64_t>("maxSumOfBundleBytesInPipeline");

            if ((outductElementConfig.convergenceLayer == "ltp_over_udp") || (outductElementConfig.convergenceLayer == "ltp_over_ipc")) {
                outductElementConfig.thisLtpEngineId = outductElementConfigPt.second.get<uint64_t>("thisLtpEngineId");
                outductElementConfig.remoteLtpEngineId = outductElementConfigPt.second.get<uint64_t>("remoteLtpEngineId");
                outductElementConfig.ltpDataSegmentMtu = outductElementConfigPt.second.get<uint32_t>("ltpDataSegmentMtu");
                outductElementConfig.oneWayLightTimeMs = outductElementConfigPt.second.get<uint64_t>("oneWayLightTimeMs");
                outductElementConfig.oneWayMarginTimeMs = outductElementConfigPt.second.get<uint64_t>("oneWayMarginTimeMs");
                outductElementConfig.clientServiceId = outductElementConfigPt.second.get<uint64_t>("clientServiceId");
                outductElementConfig.numRxCircularBufferElements = outductElementConfigPt.second.get<uint32_t>("numRxCircularBufferElements");
                outductElementConfig.ltpMaxRetriesPerSerialNumber = outductElementConfigPt.second.get<uint32_t>("ltpMaxRetriesPerSerialNumber");
                outductElementConfig.ltpCheckpointEveryNthDataSegment = outductElementConfigPt.second.get<uint32_t>("ltpCheckpointEveryNthDataSegment");
                outductElementConfig.ltpRandomNumberSizeBits = outductElementConfigPt.second.get<uint32_t>("ltpRandomNumberSizeBits");
                if ((outductElementConfig.ltpRandomNumberSizeBits != 32) && (outductElementConfig.ltpRandomNumberSizeBits != 64)) { //not inserted
                    LOG_ERROR(subprocess) << "error parsing JSON outductVector[" << (vectorIndex - 1) << "]: " << "ltpRandomNumberSizeBits ("
                        << outductElementConfig.ltpRandomNumberSizeBits << ") must be either 32 or 64";
                    return false;
                }
                outductElementConfig.ltpSenderBoundPort = outductElementConfigPt.second.get<uint16_t>("ltpSenderBoundPort");
                outductElementConfig.ltpMaxUdpPacketsToSendPerSystemCall = outductElementConfigPt.second.get<uint64_t>("ltpMaxUdpPacketsToSendPerSystemCall");
                if (outductElementConfig.ltpMaxUdpPacketsToSendPerSystemCall == 0) {
                    LOG_ERROR(subprocess) << "error parsing JSON outductVector[" << (vectorIndex - 1) << "]: ltpMaxUdpPacketsToSendPerSystemCall ("
                        << outductElementConfig.ltpMaxUdpPacketsToSendPerSystemCall << ") must be non-zero.";
                    return false;
                }
#ifdef UIO_MAXIOV
                //sendmmsg() is Linux-specific. NOTES The value specified in vlen is capped to UIO_MAXIOV (1024).
                if (outductElementConfig.ltpMaxUdpPacketsToSendPerSystemCall > UIO_MAXIOV) {
                    LOG_ERROR(subprocess) << "error parsing JSON outductVector[" << (vectorIndex - 1) << "]: ltpMaxUdpPacketsToSendPerSystemCall ("
                        << outductElementConfig.ltpMaxUdpPacketsToSendPerSystemCall << ") must be <= UIO_MAXIOV (" << UIO_MAXIOV << ").";
                    return false;
                }
#endif //UIO_MAXIOV
                outductElementConfig.ltpSenderPingSecondsOrZeroToDisable = outductElementConfigPt.second.get<uint64_t>("ltpSenderPingSecondsOrZeroToDisable");
                outductElementConfig.delaySendingOfDataSegmentsTimeMsOrZeroToDisable = outductElementConfigPt.second.get<uint64_t>("delaySendingOfDataSegmentsTimeMsOrZeroToDisable");
                outductElementConfig.keepActiveSessionDataOnDisk = outductElementConfigPt.second.get<bool>("keepActiveSessionDataOnDisk");
                outductElementConfig.activeSessionDataOnDiskNewFileDurationMs = outductElementConfigPt.second.get<uint64_t>("activeSessionDataOnDiskNewFileDurationMs");
                outductElementConfig.activeSessionDataOnDiskDirectory = outductElementConfigPt.second.get<boost::filesystem::path>("activeSessionDataOnDiskDirectory");
            }
            else {
                static const std::vector<std::string> LTP_ONLY_VALUES = { "thisLtpEngineId" , "remoteLtpEngineId", "ltpDataSegmentMtu", "oneWayLightTimeMs", "oneWayMarginTimeMs",
                    "clientServiceId", "numRxCircularBufferElements", "ltpMaxRetriesPerSerialNumber", "ltpCheckpointEveryNthDataSegment", "ltpRandomNumberSizeBits", "ltpSenderBoundPort"
                };
                for (std::size_t i = 0; i < LTP_ONLY_VALUES.size(); ++i) {
                    if (outductElementConfigPt.second.count(LTP_ONLY_VALUES[i]) != 0) {
                        LOG_ERROR(subprocess) << "error parsing JSON outductVector[" << (vectorIndex - 1) << "]: outduct convergence layer  " << outductElementConfig.convergenceLayer
                            << " has an ltp outduct only configuration parameter of \"" << LTP_ONLY_VALUES[i] << "\".. please remove";
                        return false;
                    }
                }
            }

            if ((outductElementConfig.convergenceLayer == "stcp") || (outductElementConfig.convergenceLayer == "tcpcl_v3") || (outductElementConfig.convergenceLayer == "tcpcl_v4")) {
                outductElementConfig.keepAliveIntervalSeconds = outductElementConfigPt.second.get<uint32_t>("keepAliveIntervalSeconds");
            }
            else if (outductElementConfigPt.second.count("keepAliveIntervalSeconds") != 0) {
                LOG_ERROR(subprocess) << "error parsing JSON outductVector[" << (vectorIndex - 1) << "]: outduct convergence layer  " << outductElementConfig.convergenceLayer
                    << " has an stcp or tcpcl outduct only configuration parameter of \"keepAliveIntervalSeconds\".. please remove";
                return false;
            }

            if (outductElementConfig.convergenceLayer == "tcpcl_v3") {
                outductElementConfig.tcpclV3MyMaxTxSegmentSizeBytes = outductElementConfigPt.second.get<uint64_t>("tcpclV3MyMaxTxSegmentSizeBytes");
            }
            else if (outductElementConfigPt.second.count("tcpclV3MyMaxTxSegmentSizeBytes") != 0) {
                LOG_ERROR(subprocess) << "error parsing JSON outductVector[" << (vectorIndex - 1) << "]: outduct convergence layer  " << outductElementConfig.convergenceLayer
                    << " has a tcpcl_v3 outduct only configuration parameter of \"tcpclV3MyMaxTxSegmentSizeBytes\".. please remove";
                return false;
            }

            if ((outductElementConfig.convergenceLayer == "tcpcl_v3") || (outductElementConfig.convergenceLayer == "tcpcl_v4")) {
                outductElementConfig.tcpclAllowOpportunisticReceiveBundles = outductElementConfigPt.second.get<bool>("tcpclAllowOpportunisticReceiveBundles");
            }
            else if (outductElementConfigPt.second.count("tcpclAllowOpportunisticReceiveBundles") != 0) {
                LOG_ERROR(subprocess) << "error parsing JSON outductVector[" << (vectorIndex - 1) << "]: outduct convergence layer  " << outductElementConfig.convergenceLayer
                    << " has a tcpcl (all versions) outduct only configuration parameter of \"tcpclAllowOpportunisticReceiveBundles\".. please remove";
                return false;
            }

            if (outductElementConfig.convergenceLayer == "tcpcl_v4") {
                outductElementConfig.tcpclV4MyMaxRxSegmentSizeBytes = outductElementConfigPt.second.get<uint64_t>("tcpclV4MyMaxRxSegmentSizeBytes");
                outductElementConfig.tryUseTls = outductElementConfigPt.second.get<bool>("tryUseTls");
                outductElementConfig.tlsIsRequired = outductElementConfigPt.second.get<bool>("tlsIsRequired");
                outductElementConfig.useTlsVersion1_3 = outductElementConfigPt.second.get<bool>("useTlsVersion1_3");
                outductElementConfig.doX509CertificateVerification = outductElementConfigPt.second.get<bool>("doX509CertificateVerification");
                outductElementConfig.verifySubjectAltNameInX509Certificate = outductElementConfigPt.second.get<bool>("verifySubjectAltNameInX509Certificate");
                try {
                    outductElementConfig.certificationAuthorityPemFileForVerification = outductElementConfigPt.second.get<boost::filesystem::path>("certificationAuthorityPemFileForVerification");
                }
                catch (const boost::property_tree::ptree_bad_data&) {
                    outductElementConfig.certificationAuthorityPemFileForVerification.clear();
                }
            }
            else {
                static const std::vector<std::string> VALID_TCPCL_V4_OUTDUCT_PARAMETERS = { 
                    "tcpclV4MyMaxRxSegmentSizeBytes", "tryUseTls", "tlsIsRequired", "useTlsVersion1_3",
                    "doX509CertificateVerification", "verifySubjectAltNameInX509Certificate", "certificationAuthorityPemFileForVerification" };
                
                for (std::vector<std::string>::const_iterator it = VALID_TCPCL_V4_OUTDUCT_PARAMETERS.cbegin(); it != VALID_TCPCL_V4_OUTDUCT_PARAMETERS.cend(); ++it) {
                    if (outductElementConfigPt.second.count(*it) != 0) {
                        LOG_ERROR(subprocess) << "error parsing JSON outductVector[" << (vectorIndex - 1) << "]: outduct convergence layer  " << outductElementConfig.convergenceLayer
                            << " has a tcpcl_v4 outduct only configuration parameter of \"" << (*it) << "\".. please remove";
                        return false;
                    }
                }
            }
        }
        catch (const boost::property_tree::ptree_error & e) {
            LOG_ERROR(subprocess) << "error parsing JSON outductVector[" << (vectorIndex - 1) << "]: " << e.what();
            return false;
        }
    }

    return true;
}

OutductsConfig_ptr OutductsConfig::CreateFromJson(const std::string& jsonString, bool verifyNoUnusedJsonKeys) {
    boost::property_tree::ptree pt;
    OutductsConfig_ptr config; //NULL
    if (GetPropertyTreeFromJsonString(jsonString, pt)) { //prints message if failed
        config = CreateFromPtree(pt);
        //verify that there are no unused variables within the original json
        if (config && verifyNoUnusedJsonKeys) {
            std::string returnedErrorMessage;
            if (JsonSerializable::HasUnusedJsonVariablesInString(*config, jsonString, returnedErrorMessage)) {
                LOG_ERROR(subprocess) << returnedErrorMessage;
                config.reset(); //NULL
            }
        }
    }
    return config;
}

OutductsConfig_ptr OutductsConfig::CreateFromJsonFilePath(const boost::filesystem::path& jsonFilePath, bool verifyNoUnusedJsonKeys) {
    boost::property_tree::ptree pt;
    OutductsConfig_ptr config; //NULL
    if (GetPropertyTreeFromJsonFilePath(jsonFilePath, pt)) { //prints message if failed
        config = CreateFromPtree(pt);
        //verify that there are no unused variables within the original json
        if (config && verifyNoUnusedJsonKeys) {
            std::string returnedErrorMessage;
            if (JsonSerializable::HasUnusedJsonVariablesInFilePath(*config, jsonFilePath, returnedErrorMessage)) {
                LOG_ERROR(subprocess) << returnedErrorMessage;
                config.reset(); //NULL
            }
        }
    }
    return config;
}

OutductsConfig_ptr OutductsConfig::CreateFromPtree(const boost::property_tree::ptree & pt) {

    OutductsConfig_ptr ptrOutductsConfig = std::make_shared<OutductsConfig>();
    if (!ptrOutductsConfig->SetValuesFromPropertyTree(pt)) {
        ptrOutductsConfig = OutductsConfig_ptr(); //failed, so delete and set it NULL
    }
    return ptrOutductsConfig;
}

boost::property_tree::ptree OutductsConfig::GetNewPropertyTree() const {
    boost::property_tree::ptree pt;
    pt.put("outductConfigName", m_outductConfigName);
    boost::property_tree::ptree & outductElementConfigVectorPt = pt.put_child("outductVector", m_outductElementConfigVector.empty() ? boost::property_tree::ptree("[]") : boost::property_tree::ptree());
    for (outduct_element_config_vector_t::const_iterator outductElementConfigVectorIt = m_outductElementConfigVector.cbegin(); outductElementConfigVectorIt != m_outductElementConfigVector.cend(); ++outductElementConfigVectorIt) {
        const outduct_element_config_t & outductElementConfig = *outductElementConfigVectorIt;
        boost::property_tree::ptree & outductElementConfigPt = (outductElementConfigVectorPt.push_back(std::make_pair("", boost::property_tree::ptree())))->second; //using "" as key creates json array
        outductElementConfigPt.put("name", outductElementConfig.name);
        outductElementConfigPt.put("convergenceLayer", outductElementConfig.convergenceLayer);
        outductElementConfigPt.put("nextHopNodeId", outductElementConfig.nextHopNodeId);
        outductElementConfigPt.put("remoteHostname", outductElementConfig.remoteHostname);
        outductElementConfigPt.put("remotePort", outductElementConfig.remotePort);
        outductElementConfigPt.put("maxNumberOfBundlesInPipeline", outductElementConfig.maxNumberOfBundlesInPipeline);
        outductElementConfigPt.put("maxSumOfBundleBytesInPipeline", outductElementConfig.maxSumOfBundleBytesInPipeline);
        
        if ((outductElementConfig.convergenceLayer == "ltp_over_udp") || (outductElementConfig.convergenceLayer == "ltp_over_ipc")) {
            outductElementConfigPt.put("thisLtpEngineId", outductElementConfig.thisLtpEngineId);
            outductElementConfigPt.put("remoteLtpEngineId", outductElementConfig.remoteLtpEngineId);
            outductElementConfigPt.put("ltpDataSegmentMtu", outductElementConfig.ltpDataSegmentMtu);
            outductElementConfigPt.put("oneWayLightTimeMs", outductElementConfig.oneWayLightTimeMs);
            outductElementConfigPt.put("oneWayMarginTimeMs", outductElementConfig.oneWayMarginTimeMs);
            outductElementConfigPt.put("clientServiceId", outductElementConfig.clientServiceId);
            outductElementConfigPt.put("numRxCircularBufferElements", outductElementConfig.numRxCircularBufferElements);
            outductElementConfigPt.put("ltpMaxRetriesPerSerialNumber", outductElementConfig.ltpMaxRetriesPerSerialNumber);
            outductElementConfigPt.put("ltpCheckpointEveryNthDataSegment", outductElementConfig.ltpCheckpointEveryNthDataSegment);
            outductElementConfigPt.put("ltpRandomNumberSizeBits", outductElementConfig.ltpRandomNumberSizeBits);
            outductElementConfigPt.put("ltpSenderBoundPort", outductElementConfig.ltpSenderBoundPort);
            outductElementConfigPt.put("ltpMaxUdpPacketsToSendPerSystemCall", outductElementConfig.ltpMaxUdpPacketsToSendPerSystemCall);
            outductElementConfigPt.put("ltpSenderPingSecondsOrZeroToDisable", outductElementConfig.ltpSenderPingSecondsOrZeroToDisable);
            outductElementConfigPt.put("delaySendingOfDataSegmentsTimeMsOrZeroToDisable", outductElementConfig.delaySendingOfDataSegmentsTimeMsOrZeroToDisable);
            outductElementConfigPt.put("keepActiveSessionDataOnDisk", outductElementConfig.keepActiveSessionDataOnDisk);
            outductElementConfigPt.put("activeSessionDataOnDiskNewFileDurationMs", outductElementConfig.activeSessionDataOnDiskNewFileDurationMs);
            outductElementConfigPt.put("activeSessionDataOnDiskDirectory", outductElementConfig.activeSessionDataOnDiskDirectory.string()); //.string() prevents nested quotes in json file
        }
        if ((outductElementConfig.convergenceLayer == "stcp") || (outductElementConfig.convergenceLayer == "tcpcl_v3") || (outductElementConfig.convergenceLayer == "tcpcl_v4")) {
            outductElementConfigPt.put("keepAliveIntervalSeconds", outductElementConfig.keepAliveIntervalSeconds);
        }
        if (outductElementConfig.convergenceLayer == "tcpcl_v3") {
            outductElementConfigPt.put("tcpclV3MyMaxTxSegmentSizeBytes", outductElementConfig.tcpclV3MyMaxTxSegmentSizeBytes);
        }
        if ((outductElementConfig.convergenceLayer == "tcpcl_v3") || (outductElementConfig.convergenceLayer == "tcpcl_v4")) {
            outductElementConfigPt.put("tcpclAllowOpportunisticReceiveBundles", outductElementConfig.tcpclAllowOpportunisticReceiveBundles);
        }
        if (outductElementConfig.convergenceLayer == "tcpcl_v4") {
            outductElementConfigPt.put("tcpclV4MyMaxRxSegmentSizeBytes", outductElementConfig.tcpclV4MyMaxRxSegmentSizeBytes);
            outductElementConfigPt.put("tryUseTls", outductElementConfig.tryUseTls);
            outductElementConfigPt.put("tlsIsRequired", outductElementConfig.tlsIsRequired);
            outductElementConfigPt.put("useTlsVersion1_3", outductElementConfig.useTlsVersion1_3);
            outductElementConfigPt.put("doX509CertificateVerification", outductElementConfig.doX509CertificateVerification);
            outductElementConfigPt.put("verifySubjectAltNameInX509Certificate", outductElementConfig.verifySubjectAltNameInX509Certificate);
            outductElementConfigPt.put("certificationAuthorityPemFileForVerification", outductElementConfig.certificationAuthorityPemFileForVerification.string()); //.string() prevents nested quotes in json file
        }
    }

    return pt;
}
