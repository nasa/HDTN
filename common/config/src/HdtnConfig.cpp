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
#include <sstream>
#include <set>

static constexpr hdtn::Logger::SubProcess subprocess = hdtn::Logger::SubProcess::none;

HdtnConfig::HdtnConfig() :
    m_hdtnConfigName("unnamed hdtn config"),
    m_userInterfaceOn(true),
    m_mySchemeName("unused_scheme_name"),
    m_myNodeId(0),
    m_myBpEchoServiceId(2047), //dtnme default
    m_myCustodialSsp("unused_custodial_ssp"),
    m_myCustodialServiceId(0),
    m_mySchedulerServiceId(100),
    m_isAcsAware(true),
    m_acsMaxFillsPerAcsPacket(100),
    m_acsSendPeriodMilliseconds(1000),
    m_retransmitBundleAfterNoCustodySignalMilliseconds(10000),
    m_maxBundleSizeBytes(10000000), //10MB
    m_maxIngressBundleWaitOnEgressMilliseconds(2000),
    m_bufferRxToStorageOnLinkUpSaturation(false),
    m_maxLtpReceiveUdpPacketSizeBytes(65536),
    m_zmqBoundSchedulerPubSubPortPath(10200),
    m_zmqBoundTelemApiPortPath(10305),
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
    m_mySchedulerServiceId(o.m_mySchedulerServiceId),
    m_isAcsAware(o.m_isAcsAware),
    m_acsMaxFillsPerAcsPacket(o.m_acsMaxFillsPerAcsPacket),
    m_acsSendPeriodMilliseconds(o.m_acsSendPeriodMilliseconds),
    m_retransmitBundleAfterNoCustodySignalMilliseconds(o.m_retransmitBundleAfterNoCustodySignalMilliseconds),
    m_maxBundleSizeBytes(o.m_maxBundleSizeBytes),
    m_maxIngressBundleWaitOnEgressMilliseconds(o.m_maxIngressBundleWaitOnEgressMilliseconds),
    m_bufferRxToStorageOnLinkUpSaturation(o.m_bufferRxToStorageOnLinkUpSaturation),
    m_maxLtpReceiveUdpPacketSizeBytes(o.m_maxLtpReceiveUdpPacketSizeBytes),
    m_zmqBoundSchedulerPubSubPortPath(o.m_zmqBoundSchedulerPubSubPortPath),
    m_zmqBoundTelemApiPortPath(o.m_zmqBoundTelemApiPortPath),
    m_inductsConfig(o.m_inductsConfig),
    m_outductsConfig(o.m_outductsConfig),
    m_storageConfig(o.m_storageConfig)
{ }

//a move constructor: X(X&&)
HdtnConfig::HdtnConfig(HdtnConfig&& o) noexcept :
    m_hdtnConfigName(std::move(o.m_hdtnConfigName)),
    m_userInterfaceOn(o.m_userInterfaceOn),
    m_mySchemeName(std::move(o.m_mySchemeName)),
    m_myNodeId(o.m_myNodeId),
    m_myBpEchoServiceId(o.m_myBpEchoServiceId),
    m_myCustodialSsp(std::move(o.m_myCustodialSsp)),
    m_myCustodialServiceId(o.m_myCustodialServiceId),
    m_mySchedulerServiceId(o.m_mySchedulerServiceId),
    m_isAcsAware(o.m_isAcsAware),
    m_acsMaxFillsPerAcsPacket(o.m_acsMaxFillsPerAcsPacket),
    m_acsSendPeriodMilliseconds(o.m_acsSendPeriodMilliseconds),
    m_retransmitBundleAfterNoCustodySignalMilliseconds(o.m_retransmitBundleAfterNoCustodySignalMilliseconds),
    m_maxBundleSizeBytes(o.m_maxBundleSizeBytes),
    m_maxIngressBundleWaitOnEgressMilliseconds(o.m_maxIngressBundleWaitOnEgressMilliseconds),
    m_bufferRxToStorageOnLinkUpSaturation(o.m_bufferRxToStorageOnLinkUpSaturation),
    m_maxLtpReceiveUdpPacketSizeBytes(o.m_maxLtpReceiveUdpPacketSizeBytes),
    m_zmqBoundSchedulerPubSubPortPath(o.m_zmqBoundSchedulerPubSubPortPath),
    m_zmqBoundTelemApiPortPath(o.m_zmqBoundTelemApiPortPath),
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
    m_mySchedulerServiceId = o.m_mySchedulerServiceId;
    m_isAcsAware = o.m_isAcsAware;
    m_acsMaxFillsPerAcsPacket = o.m_acsMaxFillsPerAcsPacket;
    m_acsSendPeriodMilliseconds = o.m_acsSendPeriodMilliseconds;
    m_retransmitBundleAfterNoCustodySignalMilliseconds = o.m_retransmitBundleAfterNoCustodySignalMilliseconds;
    m_maxBundleSizeBytes = o.m_maxBundleSizeBytes;
    m_maxIngressBundleWaitOnEgressMilliseconds = o.m_maxIngressBundleWaitOnEgressMilliseconds;
    m_bufferRxToStorageOnLinkUpSaturation = o.m_bufferRxToStorageOnLinkUpSaturation;
    m_maxLtpReceiveUdpPacketSizeBytes = o.m_maxLtpReceiveUdpPacketSizeBytes;
    m_zmqBoundSchedulerPubSubPortPath = o.m_zmqBoundSchedulerPubSubPortPath;
    m_zmqBoundTelemApiPortPath = o.m_zmqBoundTelemApiPortPath;
    m_inductsConfig = o.m_inductsConfig;
    m_outductsConfig = o.m_outductsConfig;
    m_storageConfig = o.m_storageConfig;
    return *this;
}

//a move assignment: operator=(X&&)
HdtnConfig& HdtnConfig::operator=(HdtnConfig&& o) noexcept {
    m_hdtnConfigName = std::move(o.m_hdtnConfigName);
    m_userInterfaceOn = o.m_userInterfaceOn;
    m_mySchemeName = std::move(o.m_mySchemeName);
    m_myNodeId = o.m_myNodeId;
    m_myBpEchoServiceId = o.m_myBpEchoServiceId;
    m_myCustodialSsp = std::move(o.m_myCustodialSsp);
    m_myCustodialServiceId = o.m_myCustodialServiceId;
    m_mySchedulerServiceId = o.m_mySchedulerServiceId;
    m_isAcsAware = o.m_isAcsAware;
    m_acsMaxFillsPerAcsPacket = o.m_acsMaxFillsPerAcsPacket;
    m_acsSendPeriodMilliseconds = o.m_acsSendPeriodMilliseconds;
    m_retransmitBundleAfterNoCustodySignalMilliseconds = o.m_retransmitBundleAfterNoCustodySignalMilliseconds;
    m_maxBundleSizeBytes = o.m_maxBundleSizeBytes;
    m_maxIngressBundleWaitOnEgressMilliseconds = o.m_maxIngressBundleWaitOnEgressMilliseconds;
    m_bufferRxToStorageOnLinkUpSaturation = o.m_bufferRxToStorageOnLinkUpSaturation;
    m_maxLtpReceiveUdpPacketSizeBytes = o.m_maxLtpReceiveUdpPacketSizeBytes;
    m_zmqBoundSchedulerPubSubPortPath = o.m_zmqBoundSchedulerPubSubPortPath;
    m_zmqBoundTelemApiPortPath = o.m_zmqBoundTelemApiPortPath;
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
        (m_mySchedulerServiceId == o.m_mySchedulerServiceId) &&
        (m_isAcsAware == o.m_isAcsAware) &&
        (m_acsMaxFillsPerAcsPacket == o.m_acsMaxFillsPerAcsPacket) &&
        (m_acsSendPeriodMilliseconds == o.m_acsSendPeriodMilliseconds) &&
        (m_retransmitBundleAfterNoCustodySignalMilliseconds == o.m_retransmitBundleAfterNoCustodySignalMilliseconds) &&
        (m_maxBundleSizeBytes == o.m_maxBundleSizeBytes) &&
        (m_maxIngressBundleWaitOnEgressMilliseconds == o.m_maxIngressBundleWaitOnEgressMilliseconds) &&
        (m_bufferRxToStorageOnLinkUpSaturation == o.m_bufferRxToStorageOnLinkUpSaturation) &&
        (m_maxLtpReceiveUdpPacketSizeBytes == o.m_maxLtpReceiveUdpPacketSizeBytes) &&
        (m_zmqBoundSchedulerPubSubPortPath == o.m_zmqBoundSchedulerPubSubPortPath) &&
        (m_zmqBoundTelemApiPortPath == o.m_zmqBoundTelemApiPortPath) &&
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
        m_mySchedulerServiceId = pt.get<uint64_t>("mySchedulerServiceId");
        m_isAcsAware = pt.get<bool>("isAcsAware");
        m_acsMaxFillsPerAcsPacket = pt.get<uint64_t>("acsMaxFillsPerAcsPacket");
        m_acsSendPeriodMilliseconds = pt.get<uint64_t>("acsSendPeriodMilliseconds");
        m_retransmitBundleAfterNoCustodySignalMilliseconds = pt.get<uint64_t>("retransmitBundleAfterNoCustodySignalMilliseconds");
        m_maxBundleSizeBytes = pt.get<uint64_t>("maxBundleSizeBytes");
        m_maxIngressBundleWaitOnEgressMilliseconds = pt.get<uint64_t>("maxIngressBundleWaitOnEgressMilliseconds");
        m_bufferRxToStorageOnLinkUpSaturation = pt.get<bool>("bufferRxToStorageOnLinkUpSaturation");
        m_maxLtpReceiveUdpPacketSizeBytes = pt.get<uint64_t>("maxLtpReceiveUdpPacketSizeBytes");

        m_zmqBoundSchedulerPubSubPortPath = pt.get<uint16_t>("zmqBoundSchedulerPubSubPortPath");
        m_zmqBoundTelemApiPortPath = pt.get<uint16_t>("zmqBoundTelemApiPortPath");
    }
    catch (const boost::property_tree::ptree_error & e) {
        LOG_ERROR(subprocess) << "parsing JSON HDTN config: " << e.what();
        return false;
    }

    //for non-throw versions of get_child which return a reference to the second parameter
    static const boost::property_tree::ptree EMPTY_PTREE;

    const boost::property_tree::ptree & inductsConfigPt = pt.get_child("inductsConfig", EMPTY_PTREE); //non-throw version
    if (!m_inductsConfig.SetValuesFromPropertyTree(inductsConfigPt)) {
        LOG_ERROR(subprocess) << "inductsConfig must be defined inside an hdtnConfig";
        return false;
    }
    const boost::property_tree::ptree & outductsConfigPt = pt.get_child("outductsConfig", EMPTY_PTREE); //non-throw version
    if (!m_outductsConfig.SetValuesFromPropertyTree(outductsConfigPt)) {
        LOG_ERROR(subprocess) << "outductsConfig must be defined inside an hdtnConfig";
        return false;
    }
    const boost::property_tree::ptree & storageConfigPt = pt.get_child("storageConfig", EMPTY_PTREE); //non-throw version
    if (!m_storageConfig.SetValuesFromPropertyTree(storageConfigPt)) {
        LOG_ERROR(subprocess) << "storageConfig must be defined inside an hdtnConfig";
        return false;
    }
   

    return true;
}



HdtnConfig_ptr HdtnConfig::CreateFromJson(const std::string & jsonString, bool verifyNoUnusedJsonKeys) {
    boost::property_tree::ptree pt;
    HdtnConfig_ptr config; //NULL
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

HdtnConfig_ptr HdtnConfig::CreateFromJsonFilePath(const boost::filesystem::path& jsonFilePath, bool verifyNoUnusedJsonKeys) {
    boost::property_tree::ptree pt;
    HdtnConfig_ptr config; //NULL
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
    pt.put("mySchedulerServiceId", m_mySchedulerServiceId);
    pt.put("isAcsAware", m_isAcsAware);
    pt.put("acsMaxFillsPerAcsPacket", m_acsMaxFillsPerAcsPacket);
    pt.put("acsSendPeriodMilliseconds", m_acsSendPeriodMilliseconds);
    pt.put("retransmitBundleAfterNoCustodySignalMilliseconds", m_retransmitBundleAfterNoCustodySignalMilliseconds);
    pt.put("maxBundleSizeBytes", m_maxBundleSizeBytes);
    pt.put("maxIngressBundleWaitOnEgressMilliseconds", m_maxIngressBundleWaitOnEgressMilliseconds);
    pt.put("bufferRxToStorageOnLinkUpSaturation", m_bufferRxToStorageOnLinkUpSaturation);
    pt.put("maxLtpReceiveUdpPacketSizeBytes", m_maxLtpReceiveUdpPacketSizeBytes);

    pt.put("zmqBoundSchedulerPubSubPortPath", m_zmqBoundSchedulerPubSubPortPath);
    pt.put("zmqBoundTelemApiPortPath", m_zmqBoundTelemApiPortPath);

    pt.put_child("inductsConfig", m_inductsConfig.GetNewPropertyTree());
    pt.put_child("outductsConfig", m_outductsConfig.GetNewPropertyTree());
    pt.put_child("storageConfig", m_storageConfig.GetNewPropertyTree());
    
    return pt;
}
