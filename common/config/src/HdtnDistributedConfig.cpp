/**
 * @file HdtnDistributedConfig.cpp
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

#include "HdtnDistributedConfig.h"
#include "Logger.h"
#include <memory>
#include <boost/foreach.hpp>
#include <boost/lexical_cast.hpp>
#include <sstream>
#include <set>

static constexpr hdtn::Logger::SubProcess subprocess = hdtn::Logger::SubProcess::none;

HdtnDistributedConfig::HdtnDistributedConfig() :
    m_zmqIngressAddress("localhost"),
    m_zmqEgressAddress("localhost"),
    m_zmqStorageAddress("localhost"),
    m_zmqSchedulerAddress("localhost"),
    m_zmqRouterAddress("localhost"),
    m_zmqBoundIngressToConnectingEgressPortPath(10100),
    m_zmqConnectingEgressToBoundIngressPortPath(10160),
    m_zmqBoundEgressToConnectingSchedulerPortPath(10162),
    m_zmqConnectingEgressBundlesOnlyToBoundIngressPortPath(10161),
    m_zmqBoundIngressToConnectingStoragePortPath(10110),
    m_zmqConnectingStorageToBoundIngressPortPath(10150),
    m_zmqConnectingStorageToBoundEgressPortPath(10120),
    m_zmqBoundEgressToConnectingStoragePortPath(10130),
    m_zmqConnectingRouterToBoundEgressPortPath(10210),
    m_zmqConnectingTelemToFromBoundIngressPortPath(10301),
    m_zmqConnectingTelemToFromBoundEgressPortPath(10302),
    m_zmqConnectingTelemToFromBoundStoragePortPath(10303),
    m_zmqConnectingTelemToFromBoundSchedulerPortPath(10304)
{}

HdtnDistributedConfig::~HdtnDistributedConfig() {
}

//a copy constructor: X(const X&)
HdtnDistributedConfig::HdtnDistributedConfig(const HdtnDistributedConfig& o) :
    m_zmqIngressAddress(o.m_zmqIngressAddress),
    m_zmqEgressAddress(o.m_zmqEgressAddress),
    m_zmqStorageAddress(o.m_zmqStorageAddress),
    m_zmqSchedulerAddress(o.m_zmqSchedulerAddress),
    m_zmqRouterAddress(o.m_zmqRouterAddress),
    m_zmqBoundIngressToConnectingEgressPortPath(o.m_zmqBoundIngressToConnectingEgressPortPath),
    m_zmqConnectingEgressToBoundIngressPortPath(o.m_zmqConnectingEgressToBoundIngressPortPath),
    m_zmqBoundEgressToConnectingSchedulerPortPath(o.m_zmqBoundEgressToConnectingSchedulerPortPath),
    m_zmqConnectingEgressBundlesOnlyToBoundIngressPortPath(o.m_zmqConnectingEgressBundlesOnlyToBoundIngressPortPath),
    m_zmqBoundIngressToConnectingStoragePortPath(o.m_zmqBoundIngressToConnectingStoragePortPath),
    m_zmqConnectingStorageToBoundIngressPortPath(o.m_zmqConnectingStorageToBoundIngressPortPath),
    m_zmqConnectingStorageToBoundEgressPortPath(o.m_zmqConnectingStorageToBoundEgressPortPath),
    m_zmqBoundEgressToConnectingStoragePortPath(o.m_zmqBoundEgressToConnectingStoragePortPath),
    m_zmqConnectingRouterToBoundEgressPortPath(o.m_zmqConnectingRouterToBoundEgressPortPath),
    m_zmqConnectingTelemToFromBoundIngressPortPath(o.m_zmqConnectingTelemToFromBoundIngressPortPath),
    m_zmqConnectingTelemToFromBoundEgressPortPath(o.m_zmqConnectingTelemToFromBoundEgressPortPath),
    m_zmqConnectingTelemToFromBoundStoragePortPath(o.m_zmqConnectingTelemToFromBoundStoragePortPath),
    m_zmqConnectingTelemToFromBoundSchedulerPortPath(o.m_zmqConnectingTelemToFromBoundSchedulerPortPath)
{ }

//a move constructor: X(X&&)
HdtnDistributedConfig::HdtnDistributedConfig(HdtnDistributedConfig&& o) noexcept :
    m_zmqIngressAddress(std::move(o.m_zmqIngressAddress)),
    m_zmqEgressAddress(std::move(o.m_zmqEgressAddress)),
    m_zmqStorageAddress(std::move(o.m_zmqStorageAddress)),
    m_zmqSchedulerAddress(std::move(o.m_zmqSchedulerAddress)),
    m_zmqRouterAddress(std::move(o.m_zmqRouterAddress)),
    m_zmqBoundIngressToConnectingEgressPortPath(o.m_zmqBoundIngressToConnectingEgressPortPath),
    m_zmqConnectingEgressToBoundIngressPortPath(o.m_zmqConnectingEgressToBoundIngressPortPath),
    m_zmqBoundEgressToConnectingSchedulerPortPath(o.m_zmqBoundEgressToConnectingSchedulerPortPath),
    m_zmqConnectingEgressBundlesOnlyToBoundIngressPortPath(o.m_zmqConnectingEgressBundlesOnlyToBoundIngressPortPath),
    m_zmqBoundIngressToConnectingStoragePortPath(o.m_zmqBoundIngressToConnectingStoragePortPath),
    m_zmqConnectingStorageToBoundIngressPortPath(o.m_zmqConnectingStorageToBoundIngressPortPath),
    m_zmqConnectingStorageToBoundEgressPortPath(o.m_zmqConnectingStorageToBoundEgressPortPath),
    m_zmqBoundEgressToConnectingStoragePortPath(o.m_zmqBoundEgressToConnectingStoragePortPath),
    m_zmqConnectingRouterToBoundEgressPortPath(o.m_zmqConnectingRouterToBoundEgressPortPath),
    m_zmqConnectingTelemToFromBoundIngressPortPath(o.m_zmqConnectingTelemToFromBoundIngressPortPath),
    m_zmqConnectingTelemToFromBoundEgressPortPath(o.m_zmqConnectingTelemToFromBoundEgressPortPath),
    m_zmqConnectingTelemToFromBoundStoragePortPath(o.m_zmqConnectingTelemToFromBoundStoragePortPath),
    m_zmqConnectingTelemToFromBoundSchedulerPortPath(o.m_zmqConnectingTelemToFromBoundSchedulerPortPath)
{ }

//a copy assignment: operator=(const X&)
HdtnDistributedConfig& HdtnDistributedConfig::operator=(const HdtnDistributedConfig& o) {
    m_zmqIngressAddress = o.m_zmqIngressAddress;
    m_zmqEgressAddress = o.m_zmqEgressAddress;
    m_zmqStorageAddress = o.m_zmqStorageAddress;
    m_zmqSchedulerAddress = o.m_zmqSchedulerAddress;
    m_zmqRouterAddress = o.m_zmqRouterAddress;
    m_zmqBoundIngressToConnectingEgressPortPath = o.m_zmqBoundIngressToConnectingEgressPortPath;
    m_zmqConnectingEgressToBoundIngressPortPath = o.m_zmqConnectingEgressToBoundIngressPortPath;
    m_zmqBoundEgressToConnectingSchedulerPortPath = o.m_zmqBoundEgressToConnectingSchedulerPortPath;
    m_zmqConnectingEgressBundlesOnlyToBoundIngressPortPath = o.m_zmqConnectingEgressBundlesOnlyToBoundIngressPortPath;
    m_zmqBoundIngressToConnectingStoragePortPath = o.m_zmqBoundIngressToConnectingStoragePortPath;
    m_zmqConnectingStorageToBoundIngressPortPath = o.m_zmqConnectingStorageToBoundIngressPortPath;
    m_zmqConnectingStorageToBoundEgressPortPath = o.m_zmqConnectingStorageToBoundEgressPortPath;
    m_zmqBoundEgressToConnectingStoragePortPath = o.m_zmqBoundEgressToConnectingStoragePortPath;
    m_zmqConnectingRouterToBoundEgressPortPath = o.m_zmqConnectingRouterToBoundEgressPortPath;
    m_zmqConnectingTelemToFromBoundIngressPortPath = o.m_zmqConnectingTelemToFromBoundIngressPortPath;
    m_zmqConnectingTelemToFromBoundEgressPortPath = o.m_zmqConnectingTelemToFromBoundEgressPortPath;
    m_zmqConnectingTelemToFromBoundStoragePortPath = o.m_zmqConnectingTelemToFromBoundStoragePortPath;
    m_zmqConnectingTelemToFromBoundSchedulerPortPath = o.m_zmqConnectingTelemToFromBoundSchedulerPortPath;
    return *this;
}

//a move assignment: operator=(X&&)
HdtnDistributedConfig& HdtnDistributedConfig::operator=(HdtnDistributedConfig&& o) noexcept {
    m_zmqIngressAddress = std::move(o.m_zmqIngressAddress);
    m_zmqEgressAddress = std::move(o.m_zmqEgressAddress);
    m_zmqStorageAddress = std::move(o.m_zmqStorageAddress);
    m_zmqSchedulerAddress = std::move(o.m_zmqSchedulerAddress);
    m_zmqRouterAddress = std::move(o.m_zmqRouterAddress);
    m_zmqBoundIngressToConnectingEgressPortPath = o.m_zmqBoundIngressToConnectingEgressPortPath;
    m_zmqConnectingEgressToBoundIngressPortPath = o.m_zmqConnectingEgressToBoundIngressPortPath;
    m_zmqBoundEgressToConnectingSchedulerPortPath = o.m_zmqBoundEgressToConnectingSchedulerPortPath;
    m_zmqConnectingEgressBundlesOnlyToBoundIngressPortPath = o.m_zmqConnectingEgressBundlesOnlyToBoundIngressPortPath;
    m_zmqBoundIngressToConnectingStoragePortPath = o.m_zmqBoundIngressToConnectingStoragePortPath;
    m_zmqConnectingStorageToBoundIngressPortPath = o.m_zmqConnectingStorageToBoundIngressPortPath;
    m_zmqConnectingStorageToBoundEgressPortPath = o.m_zmqConnectingStorageToBoundEgressPortPath;
    m_zmqBoundEgressToConnectingStoragePortPath = o.m_zmqBoundEgressToConnectingStoragePortPath;
    m_zmqConnectingRouterToBoundEgressPortPath = o.m_zmqConnectingRouterToBoundEgressPortPath;
    m_zmqConnectingTelemToFromBoundIngressPortPath = o.m_zmqConnectingTelemToFromBoundIngressPortPath;
    m_zmqConnectingTelemToFromBoundEgressPortPath = o.m_zmqConnectingTelemToFromBoundEgressPortPath;
    m_zmqConnectingTelemToFromBoundStoragePortPath = o.m_zmqConnectingTelemToFromBoundStoragePortPath;
    m_zmqConnectingTelemToFromBoundSchedulerPortPath = o.m_zmqConnectingTelemToFromBoundSchedulerPortPath;
    return *this;
}

bool HdtnDistributedConfig::operator==(const HdtnDistributedConfig& o) const {
    return 
        (m_zmqIngressAddress == o.m_zmqIngressAddress) &&
        (m_zmqEgressAddress == o.m_zmqEgressAddress) &&
        (m_zmqStorageAddress == o.m_zmqStorageAddress) &&
        (m_zmqSchedulerAddress == o.m_zmqSchedulerAddress) &&
        (m_zmqRouterAddress == o.m_zmqRouterAddress) &&
        (m_zmqBoundIngressToConnectingEgressPortPath == o.m_zmqBoundIngressToConnectingEgressPortPath) &&
        (m_zmqConnectingEgressToBoundIngressPortPath == o.m_zmqConnectingEgressToBoundIngressPortPath) &&
        (m_zmqBoundEgressToConnectingSchedulerPortPath == o.m_zmqBoundEgressToConnectingSchedulerPortPath) &&
        (m_zmqConnectingEgressBundlesOnlyToBoundIngressPortPath == o.m_zmqConnectingEgressBundlesOnlyToBoundIngressPortPath) &&
        (m_zmqBoundIngressToConnectingStoragePortPath == o.m_zmqBoundIngressToConnectingStoragePortPath) &&
        (m_zmqConnectingStorageToBoundIngressPortPath == o.m_zmqConnectingStorageToBoundIngressPortPath) &&
        (m_zmqConnectingStorageToBoundEgressPortPath == o.m_zmqConnectingStorageToBoundEgressPortPath) &&
        (m_zmqBoundEgressToConnectingStoragePortPath == o.m_zmqBoundEgressToConnectingStoragePortPath) &&
        (m_zmqConnectingRouterToBoundEgressPortPath == o.m_zmqConnectingRouterToBoundEgressPortPath) &&
        (m_zmqConnectingTelemToFromBoundIngressPortPath == o.m_zmqConnectingTelemToFromBoundIngressPortPath) &&
        (m_zmqConnectingTelemToFromBoundEgressPortPath == o.m_zmqConnectingTelemToFromBoundEgressPortPath) &&
        (m_zmqConnectingTelemToFromBoundStoragePortPath == o.m_zmqConnectingTelemToFromBoundStoragePortPath) &&
        (m_zmqConnectingTelemToFromBoundSchedulerPortPath == o.m_zmqConnectingTelemToFromBoundSchedulerPortPath);
}

bool HdtnDistributedConfig::SetValuesFromPropertyTree(const boost::property_tree::ptree & pt) {
    try {
        m_zmqIngressAddress = pt.get<std::string>("zmqIngressAddress");
        m_zmqEgressAddress = pt.get<std::string>("zmqEgressAddress");
        m_zmqStorageAddress = pt.get<std::string>("zmqStorageAddress");
        m_zmqSchedulerAddress = pt.get<std::string>("zmqSchedulerAddress");
        m_zmqRouterAddress = pt.get<std::string>("zmqRouterAddress");
        m_zmqBoundIngressToConnectingEgressPortPath = pt.get<uint16_t>("zmqBoundIngressToConnectingEgressPortPath");
        m_zmqConnectingEgressToBoundIngressPortPath = pt.get<uint16_t>("zmqConnectingEgressToBoundIngressPortPath");
        m_zmqBoundEgressToConnectingSchedulerPortPath = pt.get<uint16_t>("zmqBoundEgressToConnectingSchedulerPortPath");
        m_zmqConnectingEgressBundlesOnlyToBoundIngressPortPath = pt.get<uint16_t>("zmqConnectingEgressBundlesOnlyToBoundIngressPortPath");
        m_zmqBoundIngressToConnectingStoragePortPath = pt.get<uint16_t>("zmqBoundIngressToConnectingStoragePortPath");
        m_zmqConnectingStorageToBoundIngressPortPath = pt.get<uint16_t>("zmqConnectingStorageToBoundIngressPortPath");
        m_zmqConnectingStorageToBoundEgressPortPath = pt.get<uint16_t>("zmqConnectingStorageToBoundEgressPortPath");
        m_zmqBoundEgressToConnectingStoragePortPath = pt.get<uint16_t>("zmqBoundEgressToConnectingStoragePortPath");
        m_zmqConnectingRouterToBoundEgressPortPath = pt.get<uint16_t>("zmqConnectingRouterToBoundEgressPortPath");
        m_zmqConnectingTelemToFromBoundIngressPortPath = pt.get<uint16_t>("zmqConnectingTelemToFromBoundIngressPortPath");
        m_zmqConnectingTelemToFromBoundEgressPortPath = pt.get<uint16_t>("zmqConnectingTelemToFromBoundEgressPortPath");
        m_zmqConnectingTelemToFromBoundStoragePortPath = pt.get<uint16_t>("zmqConnectingTelemToFromBoundStoragePortPath");
        m_zmqConnectingTelemToFromBoundSchedulerPortPath = pt.get<uint16_t>("zmqConnectingTelemToFromBoundSchedulerPortPath");
    }
    catch (const boost::property_tree::ptree_error & e) {
        LOG_ERROR(subprocess) << "parsing JSON HDTN config: " << e.what();
        return false;
    }

    return true;
}



HdtnDistributedConfig_ptr HdtnDistributedConfig::CreateFromJson(const std::string & jsonString, bool verifyNoUnusedJsonKeys) {
    boost::property_tree::ptree pt;
    HdtnDistributedConfig_ptr config; //NULL
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

HdtnDistributedConfig_ptr HdtnDistributedConfig::CreateFromJsonFilePath(const boost::filesystem::path& jsonFilePath, bool verifyNoUnusedJsonKeys) {
    boost::property_tree::ptree pt;
    HdtnDistributedConfig_ptr config; //NULL
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

HdtnDistributedConfig_ptr HdtnDistributedConfig::CreateFromPtree(const boost::property_tree::ptree & pt) {

    HdtnDistributedConfig_ptr ptrHdtnDistributedConfig = std::make_shared<HdtnDistributedConfig>();
    if (!ptrHdtnDistributedConfig->SetValuesFromPropertyTree(pt)) {
        ptrHdtnDistributedConfig = HdtnDistributedConfig_ptr(); //failed, so delete and set it NULL
    }
    return ptrHdtnDistributedConfig;
}



boost::property_tree::ptree HdtnDistributedConfig::GetNewPropertyTree() const {
    boost::property_tree::ptree pt;

    pt.put("zmqIngressAddress", m_zmqIngressAddress);
    pt.put("zmqEgressAddress", m_zmqEgressAddress);
    pt.put("zmqStorageAddress", m_zmqStorageAddress);
    pt.put("zmqSchedulerAddress", m_zmqSchedulerAddress);
    pt.put("zmqRouterAddress", m_zmqRouterAddress);
    pt.put("zmqBoundIngressToConnectingEgressPortPath", m_zmqBoundIngressToConnectingEgressPortPath);
    pt.put("zmqConnectingEgressToBoundIngressPortPath", m_zmqConnectingEgressToBoundIngressPortPath);
    pt.put("zmqBoundEgressToConnectingSchedulerPortPath", m_zmqBoundEgressToConnectingSchedulerPortPath);
    pt.put("zmqConnectingEgressBundlesOnlyToBoundIngressPortPath", m_zmqConnectingEgressBundlesOnlyToBoundIngressPortPath);
    pt.put("zmqBoundIngressToConnectingStoragePortPath", m_zmqBoundIngressToConnectingStoragePortPath);
    pt.put("zmqConnectingStorageToBoundIngressPortPath", m_zmqConnectingStorageToBoundIngressPortPath);
    pt.put("zmqConnectingStorageToBoundEgressPortPath", m_zmqConnectingStorageToBoundEgressPortPath);
    pt.put("zmqBoundEgressToConnectingStoragePortPath", m_zmqBoundEgressToConnectingStoragePortPath);
    pt.put("zmqConnectingRouterToBoundEgressPortPath", m_zmqConnectingRouterToBoundEgressPortPath);
    pt.put("zmqConnectingTelemToFromBoundIngressPortPath", m_zmqConnectingTelemToFromBoundIngressPortPath);
    pt.put("zmqConnectingTelemToFromBoundEgressPortPath", m_zmqConnectingTelemToFromBoundEgressPortPath);
    pt.put("zmqConnectingTelemToFromBoundStoragePortPath", m_zmqConnectingTelemToFromBoundStoragePortPath);
    pt.put("zmqConnectingTelemToFromBoundSchedulerPortPath", m_zmqConnectingTelemToFromBoundSchedulerPortPath);

    return pt;
}
