/***************************************************************************
 * NASA Glenn Research Center, Cleveland, OH
 * Released under the NASA Open Source Agreement (NOSA)
 * May  2021
 ****************************************************************************
 */

#include "HdtnConfig.h"
#include <boost/make_shared.hpp>
#include <boost/foreach.hpp>
#include <iostream>
#include <boost/lexical_cast.hpp>

HdtnConfig::HdtnConfig() :
    m_hdtnConfigName("unnamed hdtn config"),
    m_mySchemeName("unused_scheme_name"),
    m_myNodeId(0),
    m_myCustodialSsp("unused_custodial_ssp"),
    m_myCustodialServiceId(0),
    m_zmqIngressAddress("localhost"),
    m_zmqEgressAddress("localhost"),
    m_zmqStorageAddress("localhost"),
    m_zmqRegistrationServerAddress("localhost"),
    m_zmqSchedulerAddress("localhost"),
    m_zmqBoundIngressToConnectingEgressPortPath(10100),
    m_zmqConnectingEgressToBoundIngressPortPath(10160),
    m_zmqBoundIngressToConnectingStoragePortPath(10110),
    m_zmqConnectingStorageToBoundIngressPortPath(10150),
    m_zmqConnectingStorageToBoundEgressPortPath(10120),
    m_zmqBoundEgressToConnectingStoragePortPath(10130),
    m_zmqRegistrationServerPortPath(10140),
    m_zmqBoundSchedulerPubSubPortPath(10200),
    m_zmqMaxMessagesPerPath(5),
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
    m_mySchemeName(o.m_mySchemeName),
    m_myNodeId(o.m_myNodeId),
    m_myCustodialSsp(o.m_myCustodialSsp),
    m_myCustodialServiceId(o.m_myCustodialServiceId),
    m_zmqIngressAddress(o.m_zmqIngressAddress),
    m_zmqEgressAddress(o.m_zmqEgressAddress),
    m_zmqStorageAddress(o.m_zmqStorageAddress),
    m_zmqRegistrationServerAddress(o.m_zmqRegistrationServerAddress),
    m_zmqSchedulerAddress(o.m_zmqSchedulerAddress),
    m_zmqBoundIngressToConnectingEgressPortPath(o.m_zmqBoundIngressToConnectingEgressPortPath),
    m_zmqConnectingEgressToBoundIngressPortPath(o.m_zmqConnectingEgressToBoundIngressPortPath),
    m_zmqBoundIngressToConnectingStoragePortPath(o.m_zmqBoundIngressToConnectingStoragePortPath),
    m_zmqConnectingStorageToBoundIngressPortPath(o.m_zmqConnectingStorageToBoundIngressPortPath),
    m_zmqConnectingStorageToBoundEgressPortPath(o.m_zmqConnectingStorageToBoundEgressPortPath),
    m_zmqBoundEgressToConnectingStoragePortPath(o.m_zmqBoundEgressToConnectingStoragePortPath),
    m_zmqRegistrationServerPortPath(o.m_zmqRegistrationServerPortPath),
    m_zmqBoundSchedulerPubSubPortPath(o.m_zmqBoundSchedulerPubSubPortPath),
    m_zmqMaxMessagesPerPath(o.m_zmqMaxMessagesPerPath),
    m_zmqMaxMessageSizeBytes(o.m_zmqMaxMessageSizeBytes),
    m_inductsConfig(o.m_inductsConfig),
    m_outductsConfig(o.m_outductsConfig),
    m_storageConfig(o.m_storageConfig)
{ }

//a move constructor: X(X&&)
HdtnConfig::HdtnConfig(HdtnConfig&& o) :
    m_hdtnConfigName(std::move(o.m_hdtnConfigName)),
    m_mySchemeName(std::move(o.m_mySchemeName)),
    m_myNodeId(o.m_myNodeId),
    m_myCustodialSsp(std::move(o.m_myCustodialSsp)),
    m_myCustodialServiceId(o.m_myCustodialServiceId),
    m_zmqIngressAddress(std::move(o.m_zmqIngressAddress)),
    m_zmqEgressAddress(std::move(o.m_zmqEgressAddress)),
    m_zmqStorageAddress(std::move(o.m_zmqStorageAddress)),
    m_zmqRegistrationServerAddress(std::move(o.m_zmqRegistrationServerAddress)),
    m_zmqSchedulerAddress(std::move(o.m_zmqSchedulerAddress)),
    m_zmqBoundIngressToConnectingEgressPortPath(o.m_zmqBoundIngressToConnectingEgressPortPath),
    m_zmqConnectingEgressToBoundIngressPortPath(o.m_zmqConnectingEgressToBoundIngressPortPath),
    m_zmqBoundIngressToConnectingStoragePortPath(o.m_zmqBoundIngressToConnectingStoragePortPath),
    m_zmqConnectingStorageToBoundIngressPortPath(o.m_zmqConnectingStorageToBoundIngressPortPath),
    m_zmqConnectingStorageToBoundEgressPortPath(o.m_zmqConnectingStorageToBoundEgressPortPath),
    m_zmqBoundEgressToConnectingStoragePortPath(o.m_zmqBoundEgressToConnectingStoragePortPath),
    m_zmqRegistrationServerPortPath(o.m_zmqRegistrationServerPortPath),
    m_zmqBoundSchedulerPubSubPortPath(o.m_zmqBoundSchedulerPubSubPortPath),
    m_zmqMaxMessagesPerPath(o.m_zmqMaxMessagesPerPath),
    m_zmqMaxMessageSizeBytes(o.m_zmqMaxMessageSizeBytes),
    m_inductsConfig(std::move(o.m_inductsConfig)),
    m_outductsConfig(std::move(o.m_outductsConfig)),
    m_storageConfig(std::move(o.m_storageConfig))
{ }

//a copy assignment: operator=(const X&)
HdtnConfig& HdtnConfig::operator=(const HdtnConfig& o) {
    m_hdtnConfigName = o.m_hdtnConfigName;
    m_mySchemeName = o.m_mySchemeName;
    m_myNodeId = o.m_myNodeId;
    m_myCustodialSsp = o.m_myCustodialSsp;
    m_myCustodialServiceId = o.m_myCustodialServiceId;
    m_zmqIngressAddress = o.m_zmqIngressAddress;
    m_zmqEgressAddress = o.m_zmqEgressAddress;
    m_zmqStorageAddress = o.m_zmqStorageAddress;
    m_zmqRegistrationServerAddress = o.m_zmqRegistrationServerAddress;
    m_zmqSchedulerAddress = o.m_zmqSchedulerAddress;
    m_zmqBoundIngressToConnectingEgressPortPath = o.m_zmqBoundIngressToConnectingEgressPortPath;
    m_zmqConnectingEgressToBoundIngressPortPath = o.m_zmqConnectingEgressToBoundIngressPortPath;
    m_zmqBoundIngressToConnectingStoragePortPath = o.m_zmqBoundIngressToConnectingStoragePortPath;
    m_zmqConnectingStorageToBoundIngressPortPath = o.m_zmqConnectingStorageToBoundIngressPortPath;
    m_zmqConnectingStorageToBoundEgressPortPath = o.m_zmqConnectingStorageToBoundEgressPortPath;
    m_zmqBoundEgressToConnectingStoragePortPath = o.m_zmqBoundEgressToConnectingStoragePortPath;
    m_zmqRegistrationServerPortPath = o.m_zmqRegistrationServerPortPath;
    m_zmqBoundSchedulerPubSubPortPath = o.m_zmqBoundSchedulerPubSubPortPath;
    m_zmqMaxMessagesPerPath = o.m_zmqMaxMessagesPerPath;
    m_zmqMaxMessageSizeBytes = o.m_zmqMaxMessageSizeBytes;
    m_inductsConfig = o.m_inductsConfig;
    m_outductsConfig = o.m_outductsConfig;
    m_storageConfig = o.m_storageConfig;
    return *this;
}

//a move assignment: operator=(X&&)
HdtnConfig& HdtnConfig::operator=(HdtnConfig&& o) {
    m_hdtnConfigName = std::move(o.m_hdtnConfigName);
    m_mySchemeName = std::move(o.m_mySchemeName);
    m_myNodeId = o.m_myNodeId;
    m_myCustodialSsp = std::move(o.m_myCustodialSsp);
    m_myCustodialServiceId = o.m_myCustodialServiceId;
    m_zmqIngressAddress = std::move(o.m_zmqIngressAddress);
    m_zmqEgressAddress = std::move(o.m_zmqEgressAddress);
    m_zmqStorageAddress = std::move(o.m_zmqStorageAddress);
    m_zmqRegistrationServerAddress = std::move(o.m_zmqRegistrationServerAddress);
    m_zmqSchedulerAddress = std::move(o.m_zmqSchedulerAddress);
    m_zmqBoundIngressToConnectingEgressPortPath = o.m_zmqBoundIngressToConnectingEgressPortPath;
    m_zmqConnectingEgressToBoundIngressPortPath = o.m_zmqConnectingEgressToBoundIngressPortPath;
    m_zmqBoundIngressToConnectingStoragePortPath = o.m_zmqBoundIngressToConnectingStoragePortPath;
    m_zmqConnectingStorageToBoundIngressPortPath = o.m_zmqConnectingStorageToBoundIngressPortPath;
    m_zmqConnectingStorageToBoundEgressPortPath = o.m_zmqConnectingStorageToBoundEgressPortPath;
    m_zmqBoundEgressToConnectingStoragePortPath = o.m_zmqBoundEgressToConnectingStoragePortPath;
    m_zmqRegistrationServerPortPath = o.m_zmqRegistrationServerPortPath;
    m_zmqBoundSchedulerPubSubPortPath = o.m_zmqBoundSchedulerPubSubPortPath;
    m_zmqMaxMessagesPerPath = o.m_zmqMaxMessagesPerPath;
    m_zmqMaxMessageSizeBytes = o.m_zmqMaxMessageSizeBytes;
    m_inductsConfig = std::move(o.m_inductsConfig);
    m_outductsConfig = std::move(o.m_outductsConfig);
    m_storageConfig = std::move(o.m_storageConfig);
    return *this;
}

bool HdtnConfig::operator==(const HdtnConfig & o) const {
    return (m_hdtnConfigName == o.m_hdtnConfigName) &&
        (m_mySchemeName == o.m_mySchemeName) &&
        (m_myNodeId == o.m_myNodeId) &&
        (m_myCustodialSsp == o.m_myCustodialSsp) &&
        (m_myCustodialServiceId == o.m_myCustodialServiceId) &&
        (m_zmqIngressAddress == o.m_zmqIngressAddress) &&
        (m_zmqEgressAddress == o.m_zmqEgressAddress) &&
        (m_zmqStorageAddress == o.m_zmqStorageAddress) &&
        (m_zmqRegistrationServerAddress == o.m_zmqRegistrationServerAddress) &&
        (m_zmqSchedulerAddress == o.m_zmqSchedulerAddress) &&
        (m_zmqBoundIngressToConnectingEgressPortPath == o.m_zmqBoundIngressToConnectingEgressPortPath) &&
        (m_zmqConnectingEgressToBoundIngressPortPath == o.m_zmqConnectingEgressToBoundIngressPortPath) &&
        (m_zmqBoundIngressToConnectingStoragePortPath == o.m_zmqBoundIngressToConnectingStoragePortPath) &&
        (m_zmqConnectingStorageToBoundIngressPortPath == o.m_zmqConnectingStorageToBoundIngressPortPath) &&
        (m_zmqConnectingStorageToBoundEgressPortPath == o.m_zmqConnectingStorageToBoundEgressPortPath) &&
        (m_zmqBoundEgressToConnectingStoragePortPath == o.m_zmqBoundEgressToConnectingStoragePortPath) &&
        (m_zmqRegistrationServerPortPath == o.m_zmqRegistrationServerPortPath) &&
        (m_zmqBoundSchedulerPubSubPortPath == o.m_zmqBoundSchedulerPubSubPortPath) &&
        (m_zmqMaxMessagesPerPath == o.m_zmqMaxMessagesPerPath) &&
        (m_zmqMaxMessageSizeBytes == o.m_zmqMaxMessageSizeBytes) &&
        (m_inductsConfig == o.m_inductsConfig) &&
        (m_outductsConfig == o.m_outductsConfig) &&
        (m_storageConfig == o.m_storageConfig);
}

bool HdtnConfig::SetValuesFromPropertyTree(const boost::property_tree::ptree & pt) {
    m_hdtnConfigName = pt.get<std::string>("hdtnConfigName", ""); //non-throw version
    if (m_hdtnConfigName == "") {
        std::cerr << "error: hdtnConfigName must be defined and not empty string\n";
        return false;
    }
    m_mySchemeName = pt.get<std::string>("mySchemeName", "unused_scheme_name"); //non-throw version
    m_myNodeId = pt.get<uint64_t>("myNodeId", 0); //non-throw version
    if (m_myNodeId == 0) {
        std::cerr << "error: myNodeId must be defined and not zero\n";
        return false;
    }
    m_myCustodialSsp = pt.get<std::string>("myCustodialSsp", "unused_custodial_ssp"); //non-throw version
    m_myCustodialServiceId = pt.get<uint64_t>("myCustodialServiceId", 0); //non-throw version

    m_zmqIngressAddress = pt.get<std::string>("zmqIngressAddress", "localhost"); //non-throw version
    m_zmqEgressAddress = pt.get<std::string>("zmqEgressAddress", "localhost"); //non-throw version
    m_zmqStorageAddress = pt.get<std::string>("zmqStorageAddress", "localhost"); //non-throw version
    m_zmqRegistrationServerAddress = pt.get<std::string>("zmqRegistrationServerAddress", "localhost"); //non-throw version
    m_zmqSchedulerAddress = pt.get<std::string>("zmqSchedulerAddress", "localhost"); //non-throw version

    m_zmqBoundIngressToConnectingEgressPortPath = pt.get<uint16_t>("zmqBoundIngressToConnectingEgressPortPath", 10100); //non-throw version
    m_zmqConnectingEgressToBoundIngressPortPath = pt.get<uint16_t>("zmqConnectingEgressToBoundIngressPortPath", 10160); //non-throw version
    m_zmqBoundIngressToConnectingStoragePortPath = pt.get<uint16_t>("zmqBoundIngressToConnectingStoragePortPath", 10110); //non-throw version
    m_zmqConnectingStorageToBoundIngressPortPath = pt.get<uint16_t>("zmqConnectingStorageToBoundIngressPortPath", 10150); //non-throw version
    m_zmqConnectingStorageToBoundEgressPortPath = pt.get<uint16_t>("zmqConnectingStorageToBoundEgressPortPath", 10120); //non-throw version
    m_zmqBoundEgressToConnectingStoragePortPath = pt.get<uint16_t>("zmqBoundEgressToConnectingStoragePortPath", 10130); //non-throw version
    m_zmqRegistrationServerPortPath = pt.get<uint16_t>("zmqRegistrationServerPortPath", 10140); //non-throw version
    m_zmqBoundSchedulerPubSubPortPath = pt.get<uint16_t>("zmqBoundSchedulerPubSubPortPath", 10200); //non-throw version

    m_zmqMaxMessagesPerPath = pt.get<uint64_t>("zmqMaxMessagesPerPath", 5); //non-throw version
    m_zmqMaxMessageSizeBytes = pt.get<uint64_t>("zmqMaxMessageSizeBytes", 5); //non-throw version


    const boost::property_tree::ptree & inductsConfigPt = pt.get_child("inductsConfig", boost::property_tree::ptree()); //non-throw version
    if (!m_inductsConfig.SetValuesFromPropertyTree(inductsConfigPt)) {
        std::cerr << "error: inductsConfig must be defined inside an hdtnConfig\n";
        return false;
    }
    const boost::property_tree::ptree & outductsConfigPt = pt.get_child("outductsConfig", boost::property_tree::ptree()); //non-throw version
    if (!m_outductsConfig.SetValuesFromPropertyTree(outductsConfigPt)) {
        std::cerr << "error: outductsConfig must be defined inside an hdtnConfig\n";
        return false;
    }
    const boost::property_tree::ptree & storageConfigPt = pt.get_child("storageConfig", boost::property_tree::ptree()); //non-throw version
    if (!m_storageConfig.SetValuesFromPropertyTree(storageConfigPt)) {
        std::cerr << "error: storageConfig must be defined inside an hdtnConfig\n";
        return false;
    }
   

    return true;
}

HdtnConfig_ptr HdtnConfig::CreateFromJson(const std::string & jsonString) {
    try {
        return HdtnConfig::CreateFromPtree(JsonSerializable::GetPropertyTreeFromJsonString(jsonString));
    }
    catch (boost::property_tree::json_parser_error & e) {
        const std::string message = "In HdtnConfig::CreateFromJson. Error: " + std::string(e.what());
        std::cerr << message << std::endl;
    }

    return HdtnConfig_ptr(); //NULL
}

HdtnConfig_ptr HdtnConfig::CreateFromJsonFile(const std::string & jsonFileName) {
    try {
        return HdtnConfig::CreateFromPtree(JsonSerializable::GetPropertyTreeFromJsonFile(jsonFileName));
    }
    catch (boost::property_tree::json_parser_error & e) {
        const std::string message = "In HdtnConfig::CreateFromJsonFile. Error: " + std::string(e.what());
        std::cerr << message << std::endl;
    }

    return HdtnConfig_ptr(); //NULL
}

HdtnConfig_ptr HdtnConfig::CreateFromPtree(const boost::property_tree::ptree & pt) {

    HdtnConfig_ptr ptrHdtnConfig = boost::make_shared<HdtnConfig>();
    if (!ptrHdtnConfig->SetValuesFromPropertyTree(pt)) {
        ptrHdtnConfig = HdtnConfig_ptr(); //failed, so delete and set it NULL
    }
    return ptrHdtnConfig;
}

boost::property_tree::ptree HdtnConfig::GetNewPropertyTree() const {
    boost::property_tree::ptree pt;
    pt.put("hdtnConfigName", m_hdtnConfigName);
    pt.put("mySchemeName", m_mySchemeName);
    pt.put("myNodeId", m_myNodeId);
    pt.put("myCustodialSsp", m_myCustodialSsp);
    pt.put("myCustodialServiceId", m_myCustodialServiceId); //non-throw version
    pt.put("zmqIngressAddress", m_zmqIngressAddress);
    pt.put("zmqEgressAddress", m_zmqEgressAddress);
    pt.put("zmqStorageAddress", m_zmqStorageAddress);
    pt.put("zmqRegistrationServerAddress", m_zmqRegistrationServerAddress);
    pt.put("zmqSchedulerAddress", m_zmqSchedulerAddress);
    pt.put("zmqBoundIngressToConnectingEgressPortPath", m_zmqBoundIngressToConnectingEgressPortPath);
    pt.put("zmqConnectingEgressToBoundIngressPortPath", m_zmqConnectingEgressToBoundIngressPortPath);
    pt.put("zmqBoundIngressToConnectingStoragePortPath", m_zmqBoundIngressToConnectingStoragePortPath);
    pt.put("zmqConnectingStorageToBoundIngressPortPath", m_zmqConnectingStorageToBoundIngressPortPath);
    pt.put("zmqConnectingStorageToBoundEgressPortPath", m_zmqConnectingStorageToBoundEgressPortPath);
    pt.put("zmqBoundEgressToConnectingStoragePortPath", m_zmqBoundEgressToConnectingStoragePortPath);
    pt.put("zmqRegistrationServerPortPath", m_zmqRegistrationServerPortPath);
    pt.put("zmqBoundSchedulerPubSubPortPath", m_zmqBoundSchedulerPubSubPortPath);
    pt.put("zmqMaxMessagesPerPath", m_zmqMaxMessagesPerPath);
    pt.put("zmqMaxMessageSizeBytes", m_zmqMaxMessageSizeBytes);

    pt.put_child("inductsConfig", m_inductsConfig.GetNewPropertyTree());
    pt.put_child("outductsConfig", m_outductsConfig.GetNewPropertyTree());
    pt.put_child("storageConfig", m_storageConfig.GetNewPropertyTree());
    
    return pt;
}
