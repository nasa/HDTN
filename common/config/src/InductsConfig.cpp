/**
 * @file InductsConfig.cpp
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

#include "InductsConfig.h"
#include "Logger.h"
#include <memory>
#include <boost/foreach.hpp>
#include <iostream>
#ifndef _WIN32
#include <sys/socket.h> //for ltpMaxUdpPacketsToSendPerSystemCall checks
#endif

static constexpr hdtn::Logger::SubProcess subprocess = hdtn::Logger::SubProcess::none;

static const std::vector<std::string> VALID_CONVERGENCE_LAYER_NAMES = {
    "ltp_over_udp", "ltp_over_ipc", "udp", "stcp", "tcpcl_v3", "tcpcl_v4", "slip_over_uart"
};

induct_element_config_t::induct_element_config_t() :
    name(""),
    convergenceLayer(""),
    boundPort(0),
    numRxCircularBufferElements(0),
    numRxCircularBufferBytesPerElement(0),
    thisLtpEngineId(0),
    remoteLtpEngineId(0),
    ltpReportSegmentMtu(0),
    oneWayLightTimeMs(0),
    oneWayMarginTimeMs(0),
    clientServiceId(0),
    preallocatedRedDataBytes(0),
    ltpMaxRetriesPerSerialNumber(0),
    ltpRandomNumberSizeBits(0),
    ltpRemoteUdpHostname(""),
    ltpRemoteUdpPort(0),
    ltpRxDataSegmentSessionNumberRecreationPreventerHistorySize(0),
    ltpMaxExpectedSimultaneousSessions(0),
    ltpMaxUdpPacketsToSendPerSystemCall(0),
    delaySendingOfReportSegmentsTimeMsOrZeroToDisable(20),
    keepActiveSessionDataOnDisk(false),
    activeSessionDataOnDiskNewFileDurationMs(2000),
    activeSessionDataOnDiskDirectory("./"),

    comPort(""),
    baudRate(115200),
    uartRemoteNodeId(0),

    keepAliveIntervalSeconds(0),

    tcpclV3MyMaxTxSegmentSizeBytes(0),

    tcpclV4MyMaxRxSegmentSizeBytes(0),
    tlsIsRequired(false),
    certificatePemFile(""),
    privateKeyPemFile(""),
    diffieHellmanParametersPemFile("") {}

induct_element_config_t::~induct_element_config_t() {}


//a copy constructor: X(const X&)
induct_element_config_t::induct_element_config_t(const induct_element_config_t& o) :
    name(o.name),
    convergenceLayer(o.convergenceLayer),
    boundPort(o.boundPort),
    numRxCircularBufferElements(o.numRxCircularBufferElements),
    numRxCircularBufferBytesPerElement(o.numRxCircularBufferBytesPerElement),
    thisLtpEngineId(o.thisLtpEngineId),
    remoteLtpEngineId(o.remoteLtpEngineId),
    ltpReportSegmentMtu(o.ltpReportSegmentMtu),
    oneWayLightTimeMs(o.oneWayLightTimeMs),
    oneWayMarginTimeMs(o.oneWayMarginTimeMs),
    clientServiceId(o.clientServiceId),
    preallocatedRedDataBytes(o.preallocatedRedDataBytes),
    ltpMaxRetriesPerSerialNumber(o.ltpMaxRetriesPerSerialNumber),
    ltpRandomNumberSizeBits(o.ltpRandomNumberSizeBits),
    ltpRemoteUdpHostname(o.ltpRemoteUdpHostname),
    ltpRemoteUdpPort(o.ltpRemoteUdpPort),
    ltpRxDataSegmentSessionNumberRecreationPreventerHistorySize(o.ltpRxDataSegmentSessionNumberRecreationPreventerHistorySize),
    ltpMaxExpectedSimultaneousSessions(o.ltpMaxExpectedSimultaneousSessions),
    ltpMaxUdpPacketsToSendPerSystemCall(o.ltpMaxUdpPacketsToSendPerSystemCall),
    delaySendingOfReportSegmentsTimeMsOrZeroToDisable(o.delaySendingOfReportSegmentsTimeMsOrZeroToDisable),
    keepActiveSessionDataOnDisk(o.keepActiveSessionDataOnDisk),
    activeSessionDataOnDiskNewFileDurationMs(o.activeSessionDataOnDiskNewFileDurationMs),
    activeSessionDataOnDiskDirectory(o.activeSessionDataOnDiskDirectory),

    comPort(o.comPort),
    baudRate(o.baudRate),
    uartRemoteNodeId(o.uartRemoteNodeId),

    keepAliveIntervalSeconds(o.keepAliveIntervalSeconds),

    tcpclV3MyMaxTxSegmentSizeBytes(o.tcpclV3MyMaxTxSegmentSizeBytes),

    tcpclV4MyMaxRxSegmentSizeBytes(o.tcpclV4MyMaxRxSegmentSizeBytes),
    tlsIsRequired(o.tlsIsRequired),
    certificatePemFile(o.certificatePemFile),
    privateKeyPemFile(o.privateKeyPemFile),
    diffieHellmanParametersPemFile(o.diffieHellmanParametersPemFile) { }

//a move constructor: X(X&&)
induct_element_config_t::induct_element_config_t(induct_element_config_t&& o) noexcept :
    name(std::move(o.name)),
    convergenceLayer(std::move(o.convergenceLayer)),
    boundPort(o.boundPort),
    numRxCircularBufferElements(o.numRxCircularBufferElements),
    numRxCircularBufferBytesPerElement(o.numRxCircularBufferBytesPerElement),
    thisLtpEngineId(o.thisLtpEngineId),
    remoteLtpEngineId(o.remoteLtpEngineId),
    ltpReportSegmentMtu(o.ltpReportSegmentMtu),
    oneWayLightTimeMs(o.oneWayLightTimeMs),
    oneWayMarginTimeMs(o.oneWayMarginTimeMs),
    clientServiceId(o.clientServiceId),
    preallocatedRedDataBytes(o.preallocatedRedDataBytes),
    ltpMaxRetriesPerSerialNumber(o.ltpMaxRetriesPerSerialNumber),
    ltpRandomNumberSizeBits(o.ltpRandomNumberSizeBits),
    ltpRemoteUdpHostname(std::move(o.ltpRemoteUdpHostname)),
    ltpRemoteUdpPort(o.ltpRemoteUdpPort),
    ltpRxDataSegmentSessionNumberRecreationPreventerHistorySize(o.ltpRxDataSegmentSessionNumberRecreationPreventerHistorySize),
    ltpMaxExpectedSimultaneousSessions(o.ltpMaxExpectedSimultaneousSessions),
    ltpMaxUdpPacketsToSendPerSystemCall(o.ltpMaxUdpPacketsToSendPerSystemCall),
    delaySendingOfReportSegmentsTimeMsOrZeroToDisable(o.delaySendingOfReportSegmentsTimeMsOrZeroToDisable),
    keepActiveSessionDataOnDisk(o.keepActiveSessionDataOnDisk),
    activeSessionDataOnDiskNewFileDurationMs(o.activeSessionDataOnDiskNewFileDurationMs),
    activeSessionDataOnDiskDirectory(std::move(o.activeSessionDataOnDiskDirectory)),

    comPort(std::move(o.comPort)),
    baudRate(o.baudRate),
    uartRemoteNodeId(o.uartRemoteNodeId),

    keepAliveIntervalSeconds(o.keepAliveIntervalSeconds),

    tcpclV3MyMaxTxSegmentSizeBytes(o.tcpclV3MyMaxTxSegmentSizeBytes),

    tcpclV4MyMaxRxSegmentSizeBytes(o.tcpclV4MyMaxRxSegmentSizeBytes),
    tlsIsRequired(o.tlsIsRequired),
    certificatePemFile(std::move(o.certificatePemFile)),
    privateKeyPemFile(std::move(o.privateKeyPemFile)),
    diffieHellmanParametersPemFile(std::move(o.diffieHellmanParametersPemFile)) { }

//a copy assignment: operator=(const X&)
induct_element_config_t& induct_element_config_t::operator=(const induct_element_config_t& o) {
    name = o.name;
    convergenceLayer = o.convergenceLayer;
    boundPort = o.boundPort;
    numRxCircularBufferElements = o.numRxCircularBufferElements;
    numRxCircularBufferBytesPerElement = o.numRxCircularBufferBytesPerElement;
    thisLtpEngineId = o.thisLtpEngineId;
    remoteLtpEngineId = o.remoteLtpEngineId;
    ltpReportSegmentMtu = o.ltpReportSegmentMtu;
    oneWayLightTimeMs = o.oneWayLightTimeMs;
    oneWayMarginTimeMs = o.oneWayMarginTimeMs;
    clientServiceId = o.clientServiceId;
    preallocatedRedDataBytes = o.preallocatedRedDataBytes;
    ltpMaxRetriesPerSerialNumber = o.ltpMaxRetriesPerSerialNumber;
    ltpRandomNumberSizeBits = o.ltpRandomNumberSizeBits;
    ltpRemoteUdpHostname = o.ltpRemoteUdpHostname;
    ltpRemoteUdpPort = o.ltpRemoteUdpPort;
    ltpRxDataSegmentSessionNumberRecreationPreventerHistorySize = o.ltpRxDataSegmentSessionNumberRecreationPreventerHistorySize;
    ltpMaxExpectedSimultaneousSessions = o.ltpMaxExpectedSimultaneousSessions;
    ltpMaxUdpPacketsToSendPerSystemCall = o.ltpMaxUdpPacketsToSendPerSystemCall;
    delaySendingOfReportSegmentsTimeMsOrZeroToDisable = o.delaySendingOfReportSegmentsTimeMsOrZeroToDisable;
    keepActiveSessionDataOnDisk = o.keepActiveSessionDataOnDisk;
    activeSessionDataOnDiskNewFileDurationMs = o.activeSessionDataOnDiskNewFileDurationMs;
    activeSessionDataOnDiskDirectory = o.activeSessionDataOnDiskDirectory;

    comPort = o.comPort;
    baudRate = o.baudRate;
    uartRemoteNodeId = o.uartRemoteNodeId;

    keepAliveIntervalSeconds = o.keepAliveIntervalSeconds;

    tcpclV3MyMaxTxSegmentSizeBytes = o.tcpclV3MyMaxTxSegmentSizeBytes;

    tcpclV4MyMaxRxSegmentSizeBytes = o.tcpclV4MyMaxRxSegmentSizeBytes;
    tlsIsRequired = o.tlsIsRequired;
    certificatePemFile = o.certificatePemFile;
    privateKeyPemFile = o.privateKeyPemFile;
    diffieHellmanParametersPemFile = o.diffieHellmanParametersPemFile;
    return *this;
}

//a move assignment: operator=(X&&)
induct_element_config_t& induct_element_config_t::operator=(induct_element_config_t&& o) noexcept {
    name = std::move(o.name);
    convergenceLayer = std::move(o.convergenceLayer);
    boundPort = o.boundPort;
    numRxCircularBufferElements = o.numRxCircularBufferElements;
    numRxCircularBufferBytesPerElement = o.numRxCircularBufferBytesPerElement;
    thisLtpEngineId = o.thisLtpEngineId;
    remoteLtpEngineId = o.remoteLtpEngineId;
    ltpReportSegmentMtu = o.ltpReportSegmentMtu;
    oneWayLightTimeMs = o.oneWayLightTimeMs;
    oneWayMarginTimeMs = o.oneWayMarginTimeMs;
    clientServiceId = o.clientServiceId;
    preallocatedRedDataBytes = o.preallocatedRedDataBytes;
    ltpMaxRetriesPerSerialNumber = o.ltpMaxRetriesPerSerialNumber;
    ltpRandomNumberSizeBits = o.ltpRandomNumberSizeBits;
    ltpRemoteUdpHostname = std::move(o.ltpRemoteUdpHostname);
    ltpRemoteUdpPort = o.ltpRemoteUdpPort;
    ltpRxDataSegmentSessionNumberRecreationPreventerHistorySize = o.ltpRxDataSegmentSessionNumberRecreationPreventerHistorySize;
    ltpMaxExpectedSimultaneousSessions = o.ltpMaxExpectedSimultaneousSessions;
    ltpMaxUdpPacketsToSendPerSystemCall = o.ltpMaxUdpPacketsToSendPerSystemCall;
    delaySendingOfReportSegmentsTimeMsOrZeroToDisable = o.delaySendingOfReportSegmentsTimeMsOrZeroToDisable;
    keepActiveSessionDataOnDisk = o.keepActiveSessionDataOnDisk;
    activeSessionDataOnDiskNewFileDurationMs = o.activeSessionDataOnDiskNewFileDurationMs;
    activeSessionDataOnDiskDirectory = std::move(o.activeSessionDataOnDiskDirectory);

    comPort = std::move(o.comPort);
    baudRate = o.baudRate;
    uartRemoteNodeId = o.uartRemoteNodeId;

    keepAliveIntervalSeconds = o.keepAliveIntervalSeconds;

    tcpclV3MyMaxTxSegmentSizeBytes = o.tcpclV3MyMaxTxSegmentSizeBytes;

    tcpclV4MyMaxRxSegmentSizeBytes = o.tcpclV4MyMaxRxSegmentSizeBytes;
    tlsIsRequired = o.tlsIsRequired;
    certificatePemFile = std::move(o.certificatePemFile);
    privateKeyPemFile = std::move(o.privateKeyPemFile);
    diffieHellmanParametersPemFile = std::move(o.diffieHellmanParametersPemFile);
    return *this;
}

bool induct_element_config_t::operator==(const induct_element_config_t & o) const {
    return (name == o.name) &&
        (convergenceLayer == o.convergenceLayer) &&
        (boundPort == o.boundPort) &&
        (numRxCircularBufferElements == o.numRxCircularBufferElements) &&
        (numRxCircularBufferBytesPerElement == o.numRxCircularBufferBytesPerElement) &&
        (thisLtpEngineId == o.thisLtpEngineId) &&
        (remoteLtpEngineId == o.remoteLtpEngineId) &&
        (ltpReportSegmentMtu == o.ltpReportSegmentMtu) &&
        (oneWayLightTimeMs == o.oneWayLightTimeMs) &&
        (oneWayMarginTimeMs == o.oneWayMarginTimeMs) &&
        (clientServiceId == o.clientServiceId) &&
        (preallocatedRedDataBytes == o.preallocatedRedDataBytes) &&
        (ltpMaxRetriesPerSerialNumber == o.ltpMaxRetriesPerSerialNumber) &&
        (ltpRandomNumberSizeBits == o.ltpRandomNumberSizeBits) &&
        (ltpRemoteUdpHostname == o.ltpRemoteUdpHostname) &&
        (ltpRemoteUdpPort == o.ltpRemoteUdpPort) &&
        (ltpRxDataSegmentSessionNumberRecreationPreventerHistorySize == o.ltpRxDataSegmentSessionNumberRecreationPreventerHistorySize) &&
        (ltpMaxExpectedSimultaneousSessions == o.ltpMaxExpectedSimultaneousSessions) &&
        (ltpMaxUdpPacketsToSendPerSystemCall == o.ltpMaxUdpPacketsToSendPerSystemCall) &&
        (delaySendingOfReportSegmentsTimeMsOrZeroToDisable == o.delaySendingOfReportSegmentsTimeMsOrZeroToDisable) &&
        (keepActiveSessionDataOnDisk == o.keepActiveSessionDataOnDisk) &&
        (activeSessionDataOnDiskNewFileDurationMs == o.activeSessionDataOnDiskNewFileDurationMs) &&
        (activeSessionDataOnDiskDirectory == o.activeSessionDataOnDiskDirectory) &&

        (comPort == o.comPort) &&
        (baudRate == o.baudRate) &&
        (uartRemoteNodeId == o.uartRemoteNodeId) &&

        (keepAliveIntervalSeconds == o.keepAliveIntervalSeconds) &&
        
        (tcpclV3MyMaxTxSegmentSizeBytes == o.tcpclV3MyMaxTxSegmentSizeBytes) &&

        (tcpclV4MyMaxRxSegmentSizeBytes == o.tcpclV4MyMaxRxSegmentSizeBytes) &&
        (tlsIsRequired == o.tlsIsRequired) &&
        (certificatePemFile == o.certificatePemFile) &&
        (privateKeyPemFile == o.privateKeyPemFile) &&
        (diffieHellmanParametersPemFile == o.diffieHellmanParametersPemFile);
}

InductsConfig::InductsConfig() {
}

InductsConfig::~InductsConfig() {
}

//a copy constructor: X(const X&)
InductsConfig::InductsConfig(const InductsConfig& o) :
    m_inductConfigName(o.m_inductConfigName), m_inductElementConfigVector(o.m_inductElementConfigVector) { }

//a move constructor: X(X&&)
InductsConfig::InductsConfig(InductsConfig&& o) noexcept :
    m_inductConfigName(std::move(o.m_inductConfigName)), m_inductElementConfigVector(std::move(o.m_inductElementConfigVector)) { }

//a copy assignment: operator=(const X&)
InductsConfig& InductsConfig::operator=(const InductsConfig& o) {
    m_inductConfigName = o.m_inductConfigName;
    m_inductElementConfigVector = o.m_inductElementConfigVector;
    return *this;
}

//a move assignment: operator=(X&&)
InductsConfig& InductsConfig::operator=(InductsConfig&& o) noexcept {
    m_inductConfigName = std::move(o.m_inductConfigName);
    m_inductElementConfigVector = std::move(o.m_inductElementConfigVector);
    return *this;
}

bool InductsConfig::operator==(const InductsConfig & o) const {
    return (m_inductConfigName == o.m_inductConfigName) && (m_inductElementConfigVector == o.m_inductElementConfigVector);
}

bool InductsConfig::SetValuesFromPropertyTree(const boost::property_tree::ptree & pt) {
    m_inductConfigName = pt.get<std::string>("inductConfigName", ""); //non-throw version
    if (m_inductConfigName == "") {
        LOG_ERROR(subprocess) << "inductConfigName must be defined and not empty string";
        return false;
    }

    //for non-throw versions of get_child which return a reference to the second parameter
    static const boost::property_tree::ptree EMPTY_PTREE;

    const boost::property_tree::ptree & inductElementConfigVectorPt = pt.get_child("inductVector", EMPTY_PTREE); //non-throw version
    m_inductElementConfigVector.resize(inductElementConfigVectorPt.size());
    unsigned int vectorIndex = 0;
    BOOST_FOREACH(const boost::property_tree::ptree::value_type & inductElementConfigPt, inductElementConfigVectorPt) {
        induct_element_config_t & inductElementConfig = m_inductElementConfigVector[vectorIndex++];
        try {
            inductElementConfig.name = inductElementConfigPt.second.get<std::string>("name");
            inductElementConfig.convergenceLayer = inductElementConfigPt.second.get<std::string>("convergenceLayer");
            {
                bool found = false;
                for (std::vector<std::string>::const_iterator it = VALID_CONVERGENCE_LAYER_NAMES.cbegin(); it != VALID_CONVERGENCE_LAYER_NAMES.cend(); ++it) {
                    if (inductElementConfig.convergenceLayer == *it) {
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    LOG_ERROR(subprocess) << "error parsing JSON inductVector[" << (vectorIndex - 1) << "]: invalid convergence layer " << inductElementConfig.convergenceLayer;
                    return false;
                }
            }
            if (inductElementConfig.convergenceLayer != "slip_over_uart") { //uses com port only
                inductElementConfig.boundPort = inductElementConfigPt.second.get<uint16_t>("boundPort");
                if (inductElementConfig.boundPort == 0) {
                    LOG_ERROR(subprocess) << "error parsing JSON inductVector[" << (vectorIndex - 1) << "]: boundPort must be non-zero";
                    return false;
                }
            }
            inductElementConfig.numRxCircularBufferElements = inductElementConfigPt.second.get<uint32_t>("numRxCircularBufferElements");
            if ((inductElementConfig.convergenceLayer == "udp") || (inductElementConfig.convergenceLayer == "tcpcl_v3") || (inductElementConfig.convergenceLayer == "tcpcl_v4")) {
                inductElementConfig.numRxCircularBufferBytesPerElement = inductElementConfigPt.second.get<uint32_t>("numRxCircularBufferBytesPerElement");
            }
            else if (inductElementConfigPt.second.count("numRxCircularBufferBytesPerElement")) { //not used by stcp or ltp
                LOG_ERROR(subprocess) << "error parsing JSON inductVector[" << (vectorIndex - 1) << "]: convergence layer "
                    << inductElementConfig.convergenceLayer << " induct config does not use numRxCircularBufferBytesPerElement.. please remove";
                return false;
            }

            if ((inductElementConfig.convergenceLayer == "ltp_over_udp") || (inductElementConfig.convergenceLayer == "ltp_over_ipc")) {
                inductElementConfig.thisLtpEngineId = inductElementConfigPt.second.get<uint64_t>("thisLtpEngineId");
                inductElementConfig.remoteLtpEngineId = inductElementConfigPt.second.get<uint64_t>("remoteLtpEngineId");
                inductElementConfig.ltpReportSegmentMtu = inductElementConfigPt.second.get<uint32_t>("ltpReportSegmentMtu");
                inductElementConfig.oneWayLightTimeMs = inductElementConfigPt.second.get<uint64_t>("oneWayLightTimeMs");
                inductElementConfig.oneWayMarginTimeMs = inductElementConfigPt.second.get<uint64_t>("oneWayMarginTimeMs");
                inductElementConfig.clientServiceId = inductElementConfigPt.second.get<uint64_t>("clientServiceId");
                inductElementConfig.preallocatedRedDataBytes = inductElementConfigPt.second.get<uint64_t>("preallocatedRedDataBytes");
                inductElementConfig.ltpMaxRetriesPerSerialNumber = inductElementConfigPt.second.get<uint32_t>("ltpMaxRetriesPerSerialNumber");
                inductElementConfig.ltpRandomNumberSizeBits = inductElementConfigPt.second.get<uint32_t>("ltpRandomNumberSizeBits");
                if ((inductElementConfig.ltpRandomNumberSizeBits != 32) && (inductElementConfig.ltpRandomNumberSizeBits != 64)) { //not inserted
                    LOG_ERROR(subprocess) << "error parsing JSON inductVector[" << (vectorIndex - 1) << "]: ltpRandomNumberSizeBits ("
                        << inductElementConfig.ltpRandomNumberSizeBits << ") must be either 32 or 64";
                    return false;
                }
                inductElementConfig.ltpRemoteUdpHostname = inductElementConfigPt.second.get<std::string>("ltpRemoteUdpHostname");
                inductElementConfig.ltpRemoteUdpPort = inductElementConfigPt.second.get<uint16_t>("ltpRemoteUdpPort");
                inductElementConfig.ltpRxDataSegmentSessionNumberRecreationPreventerHistorySize = inductElementConfigPt.second.get<uint64_t>("ltpRxDataSegmentSessionNumberRecreationPreventerHistorySize");
                inductElementConfig.ltpMaxExpectedSimultaneousSessions = inductElementConfigPt.second.get<uint64_t>("ltpMaxExpectedSimultaneousSessions");
                inductElementConfig.ltpMaxUdpPacketsToSendPerSystemCall = inductElementConfigPt.second.get<uint64_t>("ltpMaxUdpPacketsToSendPerSystemCall");
                if (inductElementConfig.ltpMaxUdpPacketsToSendPerSystemCall == 0) {
                    LOG_ERROR(subprocess) << "error parsing JSON inductVector[" << (vectorIndex - 1) << "]: ltpMaxUdpPacketsToSendPerSystemCall ("
                        << inductElementConfig.ltpMaxUdpPacketsToSendPerSystemCall << ") must be non-zero.";
                    return false;
                }
#ifdef UIO_MAXIOV
                //sendmmsg() is Linux-specific. NOTES The value specified in vlen is capped to UIO_MAXIOV (1024).
                if (inductElementConfig.ltpMaxUdpPacketsToSendPerSystemCall > UIO_MAXIOV) {
                    LOG_ERROR(subprocess) << "error parsing JSON inductVector[" << (vectorIndex - 1) << "]: ltpMaxUdpPacketsToSendPerSystemCall ("
                        << inductElementConfig.ltpMaxUdpPacketsToSendPerSystemCall << ") must be <= UIO_MAXIOV (" << UIO_MAXIOV << ").";
                    return false;
                }
#endif //UIO_MAXIOV
                inductElementConfig.delaySendingOfReportSegmentsTimeMsOrZeroToDisable = inductElementConfigPt.second.get<uint64_t>("delaySendingOfReportSegmentsTimeMsOrZeroToDisable");
                inductElementConfig.keepActiveSessionDataOnDisk = inductElementConfigPt.second.get<bool>("keepActiveSessionDataOnDisk");
                inductElementConfig.activeSessionDataOnDiskNewFileDurationMs = inductElementConfigPt.second.get<uint64_t>("activeSessionDataOnDiskNewFileDurationMs");
                inductElementConfig.activeSessionDataOnDiskDirectory = inductElementConfigPt.second.get<boost::filesystem::path>("activeSessionDataOnDiskDirectory");
            }
            else {
                static const std::vector<std::string> LTP_ONLY_VALUES = { "thisLtpEngineId" , "remoteLtpEngineId", "ltpReportSegmentMtu", "oneWayLightTimeMs", "oneWayMarginTimeMs",
                    "clientServiceId", "preallocatedRedDataBytes", "ltpMaxRetriesPerSerialNumber", "ltpRandomNumberSizeBits", "ltpRemoteUdpHostname", "ltpRemoteUdpPort",
                    "ltpRxDataSegmentSessionNumberRecreationPreventerHistorySize"
                };
                for (std::size_t i = 0; i < LTP_ONLY_VALUES.size(); ++i) {
                    if (inductElementConfigPt.second.count(LTP_ONLY_VALUES[i]) != 0) {
                        LOG_ERROR(subprocess) << "error parsing JSON inductVector[" << (vectorIndex - 1) << "]: induct convergence layer  " << inductElementConfig.convergenceLayer
                            << " has an ltp induct only configuration parameter of \"" << LTP_ONLY_VALUES[i] << "\".. please remove";
                        return false;
                    }
                }
            }

            if (inductElementConfig.convergenceLayer == "slip_over_uart") {
                inductElementConfig.comPort = inductElementConfigPt.second.get<std::string>("comPort");
                inductElementConfig.baudRate = inductElementConfigPt.second.get<uint32_t>("baudRate");
                inductElementConfig.uartRemoteNodeId = inductElementConfigPt.second.get<uint64_t>("uartRemoteNodeId");
            }
            else {
                static const std::vector<std::string> UART_ONLY_VALUES = {
                    "comPort" , "baudRate", "uartRemoteNodeId"
                };
                for (std::size_t i = 0; i < UART_ONLY_VALUES.size(); ++i) {
                    if (inductElementConfigPt.second.count(UART_ONLY_VALUES[i]) != 0) {
                        LOG_ERROR(subprocess) << "error parsing JSON inductVector[" << (vectorIndex - 1)
                            << "]: induct convergence layer  " << inductElementConfig.convergenceLayer
                            << " has a slip_over_uart induct only configuration parameter of \""
                            << UART_ONLY_VALUES[i] << "\".. please remove";
                        return false;
                    }
                }
            }

            if ((inductElementConfig.convergenceLayer == "stcp") || (inductElementConfig.convergenceLayer == "tcpcl_v3") || (inductElementConfig.convergenceLayer == "tcpcl_v4")) {
                inductElementConfig.keepAliveIntervalSeconds = inductElementConfigPt.second.get<uint32_t>("keepAliveIntervalSeconds");
            }
            else if (inductElementConfigPt.second.count("keepAliveIntervalSeconds") != 0) {
                LOG_ERROR(subprocess) << "error parsing JSON inductVector[" << (vectorIndex - 1) << "]: induct convergence layer  " << inductElementConfig.convergenceLayer
                    << " has an stcp or tcpcl induct only configuration parameter of \"keepAliveIntervalSeconds\".. please remove";
                return false;
            }

            if (inductElementConfig.convergenceLayer == "tcpcl_v3") {
                inductElementConfig.tcpclV3MyMaxTxSegmentSizeBytes = inductElementConfigPt.second.get<uint64_t>("tcpclV3MyMaxTxSegmentSizeBytes");
            }
            else if (inductElementConfigPt.second.count("tcpclV3MyMaxTxSegmentSizeBytes") != 0) {
                LOG_ERROR(subprocess) << "error parsing JSON inductVector[" << (vectorIndex - 1) << "]: induct convergence layer  " << inductElementConfig.convergenceLayer
                    << " has a tcpcl_v3 induct only configuration parameter of \"tcpclV3MyMaxTxSegmentSizeBytes\".. please remove";
                return false;
            }

            if (inductElementConfig.convergenceLayer == "tcpcl_v4") {
                inductElementConfig.tcpclV4MyMaxRxSegmentSizeBytes = inductElementConfigPt.second.get<uint64_t>("tcpclV4MyMaxRxSegmentSizeBytes");
                inductElementConfig.tlsIsRequired = inductElementConfigPt.second.get<bool>("tlsIsRequired");
                try {
                    inductElementConfig.certificatePemFile = inductElementConfigPt.second.get<boost::filesystem::path>("certificatePemFile");
                }
                catch (const boost::property_tree::ptree_bad_data&) {
                    inductElementConfig.certificatePemFile.clear();
                }
                try {
                    inductElementConfig.privateKeyPemFile = inductElementConfigPt.second.get<boost::filesystem::path>("privateKeyPemFile");
                }
                catch (const boost::property_tree::ptree_bad_data&) {
                    inductElementConfig.privateKeyPemFile.clear();
                }
                try {
                    inductElementConfig.diffieHellmanParametersPemFile = inductElementConfigPt.second.get<boost::filesystem::path>("diffieHellmanParametersPemFile");
                }
                catch (const boost::property_tree::ptree_bad_data&) {
                    inductElementConfig.diffieHellmanParametersPemFile.clear();
                }
            }
            else {
                static const std::vector<std::string> VALID_TCPCL_V4_INDUCT_PARAMETERS = {
                    "tcpclV4MyMaxRxSegmentSizeBytes", "tlsIsRequired", "certificatePemFile",
                    "privateKeyPemFile", "diffieHellmanParametersPemFile" };

                for (std::vector<std::string>::const_iterator it = VALID_TCPCL_V4_INDUCT_PARAMETERS.cbegin(); it != VALID_TCPCL_V4_INDUCT_PARAMETERS.cend(); ++it) {
                    if (inductElementConfigPt.second.count(*it) != 0) {
                        LOG_ERROR(subprocess) << "error parsing JSON inductVector[" << (vectorIndex - 1) << "]: induct convergence layer  " << inductElementConfig.convergenceLayer
                            << " has a tcpcl_v4 induct only configuration parameter of \"" << (*it) << "\".. please remove";
                        return false;
                    }
                }
            }
        }
        catch (const boost::property_tree::ptree_error & e) {
            LOG_ERROR(subprocess) << "error parsing JSON inductVector[" << (vectorIndex - 1) << "]: " << e.what();
            return false;
        }
    }

    return true;
}

InductsConfig_ptr InductsConfig::CreateFromJson(const std::string& jsonString, bool verifyNoUnusedJsonKeys) {
    boost::property_tree::ptree pt;
    InductsConfig_ptr config; //NULL
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

InductsConfig_ptr InductsConfig::CreateFromJsonFilePath(const boost::filesystem::path& jsonFilePath, bool verifyNoUnusedJsonKeys) {
    boost::property_tree::ptree pt;
    InductsConfig_ptr config; //NULL
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

InductsConfig_ptr InductsConfig::CreateFromPtree(const boost::property_tree::ptree & pt) {

    InductsConfig_ptr ptrInductsConfig = std::make_shared<InductsConfig>();
    if (!ptrInductsConfig->SetValuesFromPropertyTree(pt)) {
        ptrInductsConfig = InductsConfig_ptr(); //failed, so delete and set it NULL
    }
    return ptrInductsConfig;
}

boost::property_tree::ptree InductsConfig::GetNewPropertyTree() const {
    boost::property_tree::ptree pt;
    pt.put("inductConfigName", m_inductConfigName);
    boost::property_tree::ptree & inductElementConfigVectorPt = pt.put_child("inductVector", m_inductElementConfigVector.empty() ? boost::property_tree::ptree("[]") : boost::property_tree::ptree());
    for (induct_element_config_vector_t::const_iterator inductElementConfigVectorIt = m_inductElementConfigVector.cbegin(); inductElementConfigVectorIt != m_inductElementConfigVector.cend(); ++inductElementConfigVectorIt) {
        const induct_element_config_t & inductElementConfig = *inductElementConfigVectorIt;
        boost::property_tree::ptree & inductElementConfigPt = (inductElementConfigVectorPt.push_back(std::make_pair("", boost::property_tree::ptree())))->second; //using "" as key creates json array
        inductElementConfigPt.put("name", inductElementConfig.name);
        inductElementConfigPt.put("convergenceLayer", inductElementConfig.convergenceLayer);
        if (inductElementConfig.convergenceLayer != "slip_over_uart") {
            inductElementConfigPt.put("boundPort", inductElementConfig.boundPort);
        }
        inductElementConfigPt.put("numRxCircularBufferElements", inductElementConfig.numRxCircularBufferElements);
        if ((inductElementConfig.convergenceLayer == "udp") || (inductElementConfig.convergenceLayer == "tcpcl_v3") || (inductElementConfig.convergenceLayer == "tcpcl_v4")) {
            inductElementConfigPt.put("numRxCircularBufferBytesPerElement", inductElementConfig.numRxCircularBufferBytesPerElement);
        }
        if ((inductElementConfig.convergenceLayer == "ltp_over_udp") || (inductElementConfig.convergenceLayer == "ltp_over_ipc")) {
            inductElementConfigPt.put("thisLtpEngineId", inductElementConfig.thisLtpEngineId);
            inductElementConfigPt.put("remoteLtpEngineId", inductElementConfig.remoteLtpEngineId);
            inductElementConfigPt.put("ltpReportSegmentMtu", inductElementConfig.ltpReportSegmentMtu);
            inductElementConfigPt.put("oneWayLightTimeMs", inductElementConfig.oneWayLightTimeMs);
            inductElementConfigPt.put("oneWayMarginTimeMs", inductElementConfig.oneWayMarginTimeMs);
            inductElementConfigPt.put("clientServiceId", inductElementConfig.clientServiceId);
            inductElementConfigPt.put("preallocatedRedDataBytes", inductElementConfig.preallocatedRedDataBytes);
            inductElementConfigPt.put("ltpMaxRetriesPerSerialNumber", inductElementConfig.ltpMaxRetriesPerSerialNumber);
            inductElementConfigPt.put("ltpRandomNumberSizeBits", inductElementConfig.ltpRandomNumberSizeBits);
            inductElementConfigPt.put("ltpRemoteUdpHostname", inductElementConfig.ltpRemoteUdpHostname);
            inductElementConfigPt.put("ltpRemoteUdpPort", inductElementConfig.ltpRemoteUdpPort);
            inductElementConfigPt.put("ltpRxDataSegmentSessionNumberRecreationPreventerHistorySize", inductElementConfig.ltpRxDataSegmentSessionNumberRecreationPreventerHistorySize);
            inductElementConfigPt.put("ltpMaxExpectedSimultaneousSessions", inductElementConfig.ltpMaxExpectedSimultaneousSessions);
            inductElementConfigPt.put("ltpMaxUdpPacketsToSendPerSystemCall", inductElementConfig.ltpMaxUdpPacketsToSendPerSystemCall);
            inductElementConfigPt.put("delaySendingOfReportSegmentsTimeMsOrZeroToDisable", inductElementConfig.delaySendingOfReportSegmentsTimeMsOrZeroToDisable);
            inductElementConfigPt.put("keepActiveSessionDataOnDisk", inductElementConfig.keepActiveSessionDataOnDisk);
            inductElementConfigPt.put("activeSessionDataOnDiskNewFileDurationMs", inductElementConfig.activeSessionDataOnDiskNewFileDurationMs);
            inductElementConfigPt.put("activeSessionDataOnDiskDirectory", inductElementConfig.activeSessionDataOnDiskDirectory.string()); //.string() prevents nested quotes in json file
        }
        if (inductElementConfig.convergenceLayer == "slip_over_uart") {
            inductElementConfigPt.put("comPort", inductElementConfig.comPort);
            inductElementConfigPt.put("baudRate", inductElementConfig.baudRate);
            inductElementConfigPt.put("uartRemoteNodeId", inductElementConfig.uartRemoteNodeId);
        }
        if ((inductElementConfig.convergenceLayer == "stcp") || (inductElementConfig.convergenceLayer == "tcpcl_v3") || (inductElementConfig.convergenceLayer == "tcpcl_v4")) {
            inductElementConfigPt.put("keepAliveIntervalSeconds", inductElementConfig.keepAliveIntervalSeconds);
        }
        if (inductElementConfig.convergenceLayer == "tcpcl_v3") {
            inductElementConfigPt.put("tcpclV3MyMaxTxSegmentSizeBytes", inductElementConfig.tcpclV3MyMaxTxSegmentSizeBytes);
        }
        if (inductElementConfig.convergenceLayer == "tcpcl_v4") {
            inductElementConfigPt.put("tcpclV4MyMaxRxSegmentSizeBytes", inductElementConfig.tcpclV4MyMaxRxSegmentSizeBytes);
            inductElementConfigPt.put("tlsIsRequired", inductElementConfig.tlsIsRequired);
            inductElementConfigPt.put("certificatePemFile", inductElementConfig.certificatePemFile.string()); //.string() prevents nested quotes in json file
            inductElementConfigPt.put("privateKeyPemFile", inductElementConfig.privateKeyPemFile.string()); //.string() prevents nested quotes in json file
            inductElementConfigPt.put("diffieHellmanParametersPemFile", inductElementConfig.diffieHellmanParametersPemFile.string()); //.string() prevents nested quotes in json file
        }
    }

    return pt;
}
