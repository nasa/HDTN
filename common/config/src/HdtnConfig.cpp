/**
 * @file HdtnConfig.cpp
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

#include "HdtnConfig.h"
#include "Logger.h"
#include <memory>
#include <boost/foreach.hpp>
#include <boost/lexical_cast.hpp>

static constexpr hdtn::Logger::SubProcess subprocess = hdtn::Logger::SubProcess::none;

HdtnConfig::HdtnConfig() :
    m_hdtnConfigName("unnamed hdtn config"),
    m_userInterfaceOn(true),
    m_mySchemeName("unused_scheme_name"),
    m_myNodeId(0),
    m_myBpEchoServiceId(2047), //dtnme default
    m_myCustodialSsp("unused_custodial_ssp"),
    m_myCustodialServiceId(0),
    m_isAcsAware(true),
    m_acsMaxFillsPerAcsPacket(100),
    m_acsSendPeriodMilliseconds(1000),
    m_retransmitBundleAfterNoCustodySignalMilliseconds(10000),
    m_maxBundleSizeBytes(10000000), //10MB
    m_maxIngressBundleWaitOnEgressMilliseconds(2000),
    m_maxLtpReceiveUdpPacketSizeBytes(65536),
    m_zmqIngressAddress("localhost"),
    m_zmqEgressAddress("localhost"),
    m_zmqStorageAddress("localhost"),
    m_zmqSchedulerAddress("localhost"),
    m_zmqRouterAddress("localhost"),
    m_zmqBoundIngressToConnectingEgressPortPath(10100),
    m_zmqConnectingEgressToBoundIngressPortPath(10160),
    m_zmqConnectingEgressToBoundSchedulerPortPath(10162),
    m_zmqConnectingEgressBundlesOnlyToBoundIngressPortPath(10161),
    m_zmqBoundIngressToConnectingStoragePortPath(10110),
    m_zmqConnectingStorageToBoundIngressPortPath(10150),
    m_zmqConnectingStorageToBoundEgressPortPath(10120),
    m_zmqBoundEgressToConnectingStoragePortPath(10130),
    m_zmqBoundSchedulerPubSubPortPath(10200),
    m_zmqBoundRouterPubSubPortPath(10210),
    m_zmqMaxMessageSizeBytes(100000000),
    m_inductsConfig(),
    m_outductsConfig(),
    m_storageConfig() 
{}

HdtnConfig::~HdtnConfig() {
}

//a copy constructor: X(const X&)
HdtnConfig::HdtnConfig(const HdtnConfig& o) :
    m_hdtnConfigName(o.m_hdtnConfigName),
    m_userInterfaceOn(o.m_userInterfaceOn),
    m_mySchemeName(o.m_mySchemeName),
    m_myNodeId(o.m_myNodeId),
    m_myBpEchoServiceId(o.m_myBpEchoServiceId),
    m_myCustodialSsp(o.m_myCustodialSsp),
    m_myCustodialServiceId(o.m_myCustodialServiceId),
    m_isAcsAware(o.m_isAcsAware),
    m_acsMaxFillsPerAcsPacket(o.m_acsMaxFillsPerAcsPacket),
    m_acsSendPeriodMilliseconds(o.m_acsSendPeriodMilliseconds),
    m_retransmitBundleAfterNoCustodySignalMilliseconds(o.m_retransmitBundleAfterNoCustodySignalMilliseconds),
    m_maxBundleSizeBytes(o.m_maxBundleSizeBytes),
    m_maxIngressBundleWaitOnEgressMilliseconds(o.m_maxIngressBundleWaitOnEgressMilliseconds),
    m_maxLtpReceiveUdpPacketSizeBytes(o.m_maxLtpReceiveUdpPacketSizeBytes),
    m_zmqIngressAddress(o.m_zmqIngressAddress),
    m_zmqEgressAddress(o.m_zmqEgressAddress),
    m_zmqStorageAddress(o.m_zmqStorageAddress),
    m_zmqSchedulerAddress(o.m_zmqSchedulerAddress),
    m_zmqRouterAddress(o.m_zmqRouterAddress),
    m_zmqBoundIngressToConnectingEgressPortPath(o.m_zmqBoundIngressToConnectingEgressPortPath),
    m_zmqConnectingEgressToBoundIngressPortPath(o.m_zmqConnectingEgressToBoundIngressPortPath),
    m_zmqConnectingEgressToBoundSchedulerPortPath(o.m_zmqConnectingEgressToBoundSchedulerPortPath),
    m_zmqConnectingEgressBundlesOnlyToBoundIngressPortPath(o.m_zmqConnectingEgressBundlesOnlyToBoundIngressPortPath),
    m_zmqBoundIngressToConnectingStoragePortPath(o.m_zmqBoundIngressToConnectingStoragePortPath),
    m_zmqConnectingStorageToBoundIngressPortPath(o.m_zmqConnectingStorageToBoundIngressPortPath),
    m_zmqConnectingStorageToBoundEgressPortPath(o.m_zmqConnectingStorageToBoundEgressPortPath),
    m_zmqBoundEgressToConnectingStoragePortPath(o.m_zmqBoundEgressToConnectingStoragePortPath),
    m_zmqBoundSchedulerPubSubPortPath(o.m_zmqBoundSchedulerPubSubPortPath),
    m_zmqBoundRouterPubSubPortPath(o.m_zmqBoundRouterPubSubPortPath),
    m_zmqMaxMessageSizeBytes(o.m_zmqMaxMessageSizeBytes),
    m_inductsConfig(o.m_inductsConfig),
    m_outductsConfig(o.m_outductsConfig),
    m_storageConfig(o.m_storageConfig)
{ }

//a move constructor: X(X&&)
HdtnConfig::HdtnConfig(HdtnConfig&& o) :
    m_hdtnConfigName(std::move(o.m_hdtnConfigName)),
    m_userInterfaceOn(o.m_userInterfaceOn),
    m_mySchemeName(std::move(o.m_mySchemeName)),
    m_myNodeId(o.m_myNodeId),
    m_myBpEchoServiceId(o.m_myBpEchoServiceId),
    m_myCustodialSsp(std::move(o.m_myCustodialSsp)),
    m_myCustodialServiceId(o.m_myCustodialServiceId),
    m_isAcsAware(o.m_isAcsAware),
    m_acsMaxFillsPerAcsPacket(o.m_acsMaxFillsPerAcsPacket),
    m_acsSendPeriodMilliseconds(o.m_acsSendPeriodMilliseconds),
    m_retransmitBundleAfterNoCustodySignalMilliseconds(o.m_retransmitBundleAfterNoCustodySignalMilliseconds),
    m_maxBundleSizeBytes(o.m_maxBundleSizeBytes),
    m_maxIngressBundleWaitOnEgressMilliseconds(o.m_maxIngressBundleWaitOnEgressMilliseconds),
    m_maxLtpReceiveUdpPacketSizeBytes(o.m_maxLtpReceiveUdpPacketSizeBytes),
    m_zmqIngressAddress(std::move(o.m_zmqIngressAddress)),
    m_zmqEgressAddress(std::move(o.m_zmqEgressAddress)),
    m_zmqStorageAddress(std::move(o.m_zmqStorageAddress)),
    m_zmqSchedulerAddress(std::move(o.m_zmqSchedulerAddress)),
    m_zmqRouterAddress(std::move(o.m_zmqRouterAddress)),
    m_zmqBoundIngressToConnectingEgressPortPath(o.m_zmqBoundIngressToConnectingEgressPortPath),
    m_zmqConnectingEgressToBoundIngressPortPath(o.m_zmqConnectingEgressToBoundIngressPortPath),
    m_zmqConnectingEgressToBoundSchedulerPortPath(o.m_zmqConnectingEgressToBoundSchedulerPortPath),
    m_zmqConnectingEgressBundlesOnlyToBoundIngressPortPath(o.m_zmqConnectingEgressBundlesOnlyToBoundIngressPortPath),
    m_zmqBoundIngressToConnectingStoragePortPath(o.m_zmqBoundIngressToConnectingStoragePortPath),
    m_zmqConnectingStorageToBoundIngressPortPath(o.m_zmqConnectingStorageToBoundIngressPortPath),
    m_zmqConnectingStorageToBoundEgressPortPath(o.m_zmqConnectingStorageToBoundEgressPortPath),
    m_zmqBoundEgressToConnectingStoragePortPath(o.m_zmqBoundEgressToConnectingStoragePortPath),
    m_zmqBoundSchedulerPubSubPortPath(o.m_zmqBoundSchedulerPubSubPortPath),
    m_zmqBoundRouterPubSubPortPath(o.m_zmqBoundRouterPubSubPortPath),
    m_zmqMaxMessageSizeBytes(o.m_zmqMaxMessageSizeBytes),
    m_inductsConfig(std::move(o.m_inductsConfig)),
    m_outductsConfig(std::move(o.m_outductsConfig)),
    m_storageConfig(std::move(o.m_storageConfig))
{ }

//a copy assignment: operator=(const X&)
HdtnConfig& HdtnConfig::operator=(const HdtnConfig& o) {
    m_hdtnConfigName = o.m_hdtnConfigName;
    m_userInterfaceOn = o.m_userInterfaceOn;
    m_mySchemeName = o.m_mySchemeName;
    m_myNodeId = o.m_myNodeId;
    m_myBpEchoServiceId = o.m_myBpEchoServiceId;
    m_myCustodialSsp = o.m_myCustodialSsp;
    m_myCustodialServiceId = o.m_myCustodialServiceId;
    m_isAcsAware = o.m_isAcsAware;
    m_acsMaxFillsPerAcsPacket = o.m_acsMaxFillsPerAcsPacket;
    m_acsSendPeriodMilliseconds = o.m_acsSendPeriodMilliseconds;
    m_retransmitBundleAfterNoCustodySignalMilliseconds = o.m_retransmitBundleAfterNoCustodySignalMilliseconds;
    m_maxBundleSizeBytes = o.m_maxBundleSizeBytes;
    m_maxIngressBundleWaitOnEgressMilliseconds = o.m_maxIngressBundleWaitOnEgressMilliseconds;
    m_maxLtpReceiveUdpPacketSizeBytes = o.m_maxLtpReceiveUdpPacketSizeBytes;
    m_zmqIngressAddress = o.m_zmqIngressAddress;
    m_zmqEgressAddress = o.m_zmqEgressAddress;
    m_zmqStorageAddress = o.m_zmqStorageAddress;
    m_zmqSchedulerAddress = o.m_zmqSchedulerAddress;
    m_zmqRouterAddress = o.m_zmqRouterAddress;
    m_zmqBoundIngressToConnectingEgressPortPath = o.m_zmqBoundIngressToConnectingEgressPortPath;
    m_zmqConnectingEgressToBoundIngressPortPath = o.m_zmqConnectingEgressToBoundIngressPortPath;
    m_zmqConnectingEgressToBoundSchedulerPortPath = o.m_zmqConnectingEgressToBoundSchedulerPortPath;
    m_zmqConnectingEgressBundlesOnlyToBoundIngressPortPath = o.m_zmqConnectingEgressBundlesOnlyToBoundIngressPortPath;
    m_zmqBoundIngressToConnectingStoragePortPath = o.m_zmqBoundIngressToConnectingStoragePortPath;
    m_zmqConnectingStorageToBoundIngressPortPath = o.m_zmqConnectingStorageToBoundIngressPortPath;
    m_zmqConnectingStorageToBoundEgressPortPath = o.m_zmqConnectingStorageToBoundEgressPortPath;
    m_zmqBoundEgressToConnectingStoragePortPath = o.m_zmqBoundEgressToConnectingStoragePortPath;
    m_zmqBoundSchedulerPubSubPortPath = o.m_zmqBoundSchedulerPubSubPortPath;
    m_zmqBoundRouterPubSubPortPath = o.m_zmqBoundRouterPubSubPortPath;
    m_zmqMaxMessageSizeBytes = o.m_zmqMaxMessageSizeBytes;
    m_inductsConfig = o.m_inductsConfig;
    m_outductsConfig = o.m_outductsConfig;
    m_storageConfig = o.m_storageConfig;
    return *this;
}

//a move assignment: operator=(X&&)
HdtnConfig& HdtnConfig::operator=(HdtnConfig&& o) {
    m_hdtnConfigName = std::move(o.m_hdtnConfigName);
    m_userInterfaceOn = o.m_userInterfaceOn;
    m_mySchemeName = std::move(o.m_mySchemeName);
    m_myNodeId = o.m_myNodeId;
    m_myBpEchoServiceId = o.m_myBpEchoServiceId;
    m_myCustodialSsp = std::move(o.m_myCustodialSsp);
    m_myCustodialServiceId = o.m_myCustodialServiceId;
    m_isAcsAware = o.m_isAcsAware;
    m_acsMaxFillsPerAcsPacket = o.m_acsMaxFillsPerAcsPacket;
    m_acsSendPeriodMilliseconds = o.m_acsSendPeriodMilliseconds;
    m_retransmitBundleAfterNoCustodySignalMilliseconds = o.m_retransmitBundleAfterNoCustodySignalMilliseconds;
    m_maxBundleSizeBytes = o.m_maxBundleSizeBytes;
    m_maxIngressBundleWaitOnEgressMilliseconds = o.m_maxIngressBundleWaitOnEgressMilliseconds;
    m_maxLtpReceiveUdpPacketSizeBytes = o.m_maxLtpReceiveUdpPacketSizeBytes;
    m_zmqIngressAddress = std::move(o.m_zmqIngressAddress);
    m_zmqEgressAddress = std::move(o.m_zmqEgressAddress);
    m_zmqStorageAddress = std::move(o.m_zmqStorageAddress);
    m_zmqSchedulerAddress = std::move(o.m_zmqSchedulerAddress);
    m_zmqRouterAddress = std::move(o.m_zmqRouterAddress);
    m_zmqBoundIngressToConnectingEgressPortPath = o.m_zmqBoundIngressToConnectingEgressPortPath;
    m_zmqConnectingEgressToBoundIngressPortPath = o.m_zmqConnectingEgressToBoundIngressPortPath;
    m_zmqConnectingEgressToBoundSchedulerPortPath = o.m_zmqConnectingEgressToBoundSchedulerPortPath;
    m_zmqConnectingEgressBundlesOnlyToBoundIngressPortPath = o.m_zmqConnectingEgressBundlesOnlyToBoundIngressPortPath;
    m_zmqBoundIngressToConnectingStoragePortPath = o.m_zmqBoundIngressToConnectingStoragePortPath;
    m_zmqConnectingStorageToBoundIngressPortPath = o.m_zmqConnectingStorageToBoundIngressPortPath;
    m_zmqConnectingStorageToBoundEgressPortPath = o.m_zmqConnectingStorageToBoundEgressPortPath;
    m_zmqBoundEgressToConnectingStoragePortPath = o.m_zmqBoundEgressToConnectingStoragePortPath;
    m_zmqBoundSchedulerPubSubPortPath = o.m_zmqBoundSchedulerPubSubPortPath;
    m_zmqBoundRouterPubSubPortPath = o.m_zmqBoundRouterPubSubPortPath;
    m_zmqMaxMessageSizeBytes = o.m_zmqMaxMessageSizeBytes;
    m_inductsConfig = std::move(o.m_inductsConfig);
    m_outductsConfig = std::move(o.m_outductsConfig);
    m_storageConfig = std::move(o.m_storageConfig);
    return *this;
}

bool HdtnConfig::operator==(const HdtnConfig & o) const {
    return (m_hdtnConfigName == o.m_hdtnConfigName) &&
        (m_userInterfaceOn == o.m_userInterfaceOn) &&
        (m_mySchemeName == o.m_mySchemeName) &&
        (m_myNodeId == o.m_myNodeId) &&
        (m_myBpEchoServiceId == o.m_myBpEchoServiceId) &&
        (m_myCustodialSsp == o.m_myCustodialSsp) &&
        (m_myCustodialServiceId == o.m_myCustodialServiceId) &&
        (m_zmqIngressAddress == o.m_zmqIngressAddress) &&
        (m_zmqEgressAddress == o.m_zmqEgressAddress) &&
        (m_zmqStorageAddress == o.m_zmqStorageAddress) &&
        (m_isAcsAware == o.m_isAcsAware) &&
        (m_acsMaxFillsPerAcsPacket == o.m_acsMaxFillsPerAcsPacket) &&
        (m_acsSendPeriodMilliseconds == o.m_acsSendPeriodMilliseconds) &&
        (m_retransmitBundleAfterNoCustodySignalMilliseconds == o.m_retransmitBundleAfterNoCustodySignalMilliseconds) &&
        (m_maxBundleSizeBytes == o.m_maxBundleSizeBytes) &&
        (m_maxIngressBundleWaitOnEgressMilliseconds == o.m_maxIngressBundleWaitOnEgressMilliseconds) &&
        (m_maxLtpReceiveUdpPacketSizeBytes == o.m_maxLtpReceiveUdpPacketSizeBytes) &&
        (m_zmqSchedulerAddress == o.m_zmqSchedulerAddress) &&
        (m_zmqRouterAddress == o.m_zmqRouterAddress) &&
        (m_zmqBoundIngressToConnectingEgressPortPath == o.m_zmqBoundIngressToConnectingEgressPortPath) &&
        (m_zmqConnectingEgressToBoundIngressPortPath == o.m_zmqConnectingEgressToBoundIngressPortPath) &&
        (m_zmqConnectingEgressToBoundSchedulerPortPath == o.m_zmqConnectingEgressToBoundSchedulerPortPath) &&
        (m_zmqConnectingEgressBundlesOnlyToBoundIngressPortPath == o.m_zmqConnectingEgressBundlesOnlyToBoundIngressPortPath) &&
        (m_zmqBoundIngressToConnectingStoragePortPath == o.m_zmqBoundIngressToConnectingStoragePortPath) &&
        (m_zmqConnectingStorageToBoundIngressPortPath == o.m_zmqConnectingStorageToBoundIngressPortPath) &&
        (m_zmqConnectingStorageToBoundEgressPortPath == o.m_zmqConnectingStorageToBoundEgressPortPath) &&
        (m_zmqBoundEgressToConnectingStoragePortPath == o.m_zmqBoundEgressToConnectingStoragePortPath) &&
        (m_zmqBoundSchedulerPubSubPortPath == o.m_zmqBoundSchedulerPubSubPortPath) &&
        (m_zmqBoundRouterPubSubPortPath == o.m_zmqBoundRouterPubSubPortPath) &&
        (m_zmqMaxMessageSizeBytes == o.m_zmqMaxMessageSizeBytes) &&
        (m_inductsConfig == o.m_inductsConfig) &&
        (m_outductsConfig == o.m_outductsConfig) &&
        (m_storageConfig == o.m_storageConfig);
}

bool HdtnConfig::SetValuesFromPropertyTree(const boost::property_tree::ptree & pt) {
    try {
        m_hdtnConfigName = pt.get<std::string>("hdtnConfigName");
        if (m_hdtnConfigName == "") {
            LOG_ERROR(subprocess) << "parsing JSON HDTN config: hdtnConfigName must be defined and not empty string";
            return false;
        }
        m_userInterfaceOn = pt.get<bool>("userInterfaceOn");
        m_mySchemeName = pt.get<std::string>("mySchemeName");
        m_myNodeId = pt.get<uint64_t>("myNodeId");
        if (m_myNodeId == 0) {
            LOG_ERROR(subprocess) << "parsing JSON HDTN config: myNodeId must be defined and not zero";
            return false;
        }
        m_myBpEchoServiceId = pt.get<uint64_t>("myBpEchoServiceId");
        m_myCustodialSsp = pt.get<std::string>("myCustodialSsp");
        m_myCustodialServiceId = pt.get<uint64_t>("myCustodialServiceId");
        m_isAcsAware = pt.get<bool>("isAcsAware");
        m_acsMaxFillsPerAcsPacket = pt.get<uint64_t>("acsMaxFillsPerAcsPacket");
        m_acsSendPeriodMilliseconds = pt.get<uint64_t>("acsSendPeriodMilliseconds");
        m_retransmitBundleAfterNoCustodySignalMilliseconds = pt.get<uint64_t>("retransmitBundleAfterNoCustodySignalMilliseconds");
        m_maxBundleSizeBytes = pt.get<uint64_t>("maxBundleSizeBytes");
        m_maxIngressBundleWaitOnEgressMilliseconds = pt.get<uint64_t>("maxIngressBundleWaitOnEgressMilliseconds");
        m_maxLtpReceiveUdpPacketSizeBytes = pt.get<uint64_t>("maxLtpReceiveUdpPacketSizeBytes");

        m_zmqIngressAddress = pt.get<std::string>("zmqIngressAddress");
        m_zmqEgressAddress = pt.get<std::string>("zmqEgressAddress");
        m_zmqStorageAddress = pt.get<std::string>("zmqStorageAddress");
        m_zmqSchedulerAddress = pt.get<std::string>("zmqSchedulerAddress");
        m_zmqRouterAddress = pt.get<std::string>("zmqRouterAddress");
        m_zmqBoundIngressToConnectingEgressPortPath = pt.get<uint16_t>("zmqBoundIngressToConnectingEgressPortPath");
        m_zmqConnectingEgressToBoundIngressPortPath = pt.get<uint16_t>("zmqConnectingEgressToBoundIngressPortPath");
        m_zmqConnectingEgressToBoundSchedulerPortPath = pt.get<uint16_t>("zmqConnectingEgressToBoundSchedulerPortPath");
        m_zmqConnectingEgressBundlesOnlyToBoundIngressPortPath = pt.get<uint16_t>("zmqConnectingEgressBundlesOnlyToBoundIngressPortPath");
        m_zmqBoundIngressToConnectingStoragePortPath = pt.get<uint16_t>("zmqBoundIngressToConnectingStoragePortPath");
        m_zmqConnectingStorageToBoundIngressPortPath = pt.get<uint16_t>("zmqConnectingStorageToBoundIngressPortPath");
        m_zmqConnectingStorageToBoundEgressPortPath = pt.get<uint16_t>("zmqConnectingStorageToBoundEgressPortPath");
        m_zmqBoundEgressToConnectingStoragePortPath = pt.get<uint16_t>("zmqBoundEgressToConnectingStoragePortPath");
        m_zmqBoundSchedulerPubSubPortPath = pt.get<uint16_t>("zmqBoundSchedulerPubSubPortPath");
        m_zmqBoundRouterPubSubPortPath = pt.get<uint16_t>("zmqBoundRouterPubSubPortPath");
        m_zmqMaxMessageSizeBytes = pt.get<uint64_t>("zmqMaxMessageSizeBytes");
    }
    catch (const boost::property_tree::ptree_error & e) {
        LOG_ERROR(subprocess) << "parsing JSON HDTN config: " << e.what();
        return false;
    }


    const boost::property_tree::ptree & inductsConfigPt = pt.get_child("inductsConfig", boost::property_tree::ptree()); //non-throw version
    if (!m_inductsConfig.SetValuesFromPropertyTree(inductsConfigPt)) {
        LOG_ERROR(subprocess) << "inductsConfig must be defined inside an hdtnConfig";
        return false;
    }
    const boost::property_tree::ptree & outductsConfigPt = pt.get_child("outductsConfig", boost::property_tree::ptree()); //non-throw version
    if (!m_outductsConfig.SetValuesFromPropertyTree(outductsConfigPt)) {
        LOG_ERROR(subprocess) << "outductsConfig must be defined inside an hdtnConfig";
        return false;
    }
    const boost::property_tree::ptree & storageConfigPt = pt.get_child("storageConfig", boost::property_tree::ptree()); //non-throw version
    if (!m_storageConfig.SetValuesFromPropertyTree(storageConfigPt)) {
        LOG_ERROR(subprocess) << "storageConfig must be defined inside an hdtnConfig";
        return false;
    }
   

    return true;
}

HdtnConfig_ptr HdtnConfig::CreateFromJson(const std::string & jsonString) {
    try {
        return HdtnConfig::CreateFromPtree(JsonSerializable::GetPropertyTreeFromJsonString(jsonString));
    }
    catch (boost::property_tree::json_parser::json_parser_error & e) {
        const std::string message = "In HdtnConfig::CreateFromJson. Error: " + std::string(e.what());
        LOG_ERROR(subprocess) << message;
    }

    return HdtnConfig_ptr(); //NULL
}

HdtnConfig_ptr HdtnConfig::CreateFromJsonFile(const std::string & jsonFileName) {
    try {
        return HdtnConfig::CreateFromPtree(JsonSerializable::GetPropertyTreeFromJsonFile(jsonFileName));
    }
    catch (boost::property_tree::json_parser::json_parser_error & e) {
        const std::string message = "In HdtnConfig::CreateFromJsonFile. Error: " + std::string(e.what());
        LOG_ERROR(subprocess) << message;
    }

    return HdtnConfig_ptr(); //NULL
}

HdtnConfig_ptr HdtnConfig::CreateFromPtree(const boost::property_tree::ptree & pt) {

    HdtnConfig_ptr ptrHdtnConfig = std::make_shared<HdtnConfig>();
    if (!ptrHdtnConfig->SetValuesFromPropertyTree(pt)) {
        ptrHdtnConfig = HdtnConfig_ptr(); //failed, so delete and set it NULL
    }
    return ptrHdtnConfig;
}

boost::property_tree::ptree HdtnConfig::GetNewPropertyTree() const {
    boost::property_tree::ptree pt;
    pt.put("hdtnConfigName", m_hdtnConfigName);
    pt.put("userInterfaceOn", m_userInterfaceOn);
    pt.put("mySchemeName", m_mySchemeName);
    pt.put("myNodeId", m_myNodeId);
    pt.put("myBpEchoServiceId", m_myBpEchoServiceId);
    pt.put("myCustodialSsp", m_myCustodialSsp);
    pt.put("myCustodialServiceId", m_myCustodialServiceId);
    pt.put("isAcsAware", m_isAcsAware);
    pt.put("acsMaxFillsPerAcsPacket", m_acsMaxFillsPerAcsPacket);
    pt.put("acsSendPeriodMilliseconds", m_acsSendPeriodMilliseconds);
    pt.put("retransmitBundleAfterNoCustodySignalMilliseconds", m_retransmitBundleAfterNoCustodySignalMilliseconds);
    pt.put("maxBundleSizeBytes", m_maxBundleSizeBytes);
    pt.put("maxIngressBundleWaitOnEgressMilliseconds", m_maxIngressBundleWaitOnEgressMilliseconds);
    pt.put("maxLtpReceiveUdpPacketSizeBytes", m_maxLtpReceiveUdpPacketSizeBytes);

    pt.put("zmqIngressAddress", m_zmqIngressAddress);
    pt.put("zmqEgressAddress", m_zmqEgressAddress);
    pt.put("zmqStorageAddress", m_zmqStorageAddress);
    pt.put("zmqSchedulerAddress", m_zmqSchedulerAddress);
    pt.put("zmqRouterAddress", m_zmqRouterAddress);
    pt.put("zmqBoundIngressToConnectingEgressPortPath", m_zmqBoundIngressToConnectingEgressPortPath);
    pt.put("zmqConnectingEgressToBoundIngressPortPath", m_zmqConnectingEgressToBoundIngressPortPath);
    pt.put("zmqConnectingEgressToBoundSchedulerPortPath", m_zmqConnectingEgressToBoundSchedulerPortPath);
    pt.put("zmqConnectingEgressBundlesOnlyToBoundIngressPortPath", m_zmqConnectingEgressBundlesOnlyToBoundIngressPortPath);
    pt.put("zmqBoundIngressToConnectingStoragePortPath", m_zmqBoundIngressToConnectingStoragePortPath);
    pt.put("zmqConnectingStorageToBoundIngressPortPath", m_zmqConnectingStorageToBoundIngressPortPath);
    pt.put("zmqConnectingStorageToBoundEgressPortPath", m_zmqConnectingStorageToBoundEgressPortPath);
    pt.put("zmqBoundEgressToConnectingStoragePortPath", m_zmqBoundEgressToConnectingStoragePortPath);
    pt.put("zmqBoundSchedulerPubSubPortPath", m_zmqBoundSchedulerPubSubPortPath);
    pt.put("zmqBoundRouterPubSubPortPath", m_zmqBoundRouterPubSubPortPath);
    pt.put("zmqMaxMessageSizeBytes", m_zmqMaxMessageSizeBytes);

    pt.put_child("inductsConfig", m_inductsConfig.GetNewPropertyTree());
    pt.put_child("outductsConfig", m_outductsConfig.GetNewPropertyTree());
    pt.put_child("storageConfig", m_storageConfig.GetNewPropertyTree());
    
    return pt;
}
