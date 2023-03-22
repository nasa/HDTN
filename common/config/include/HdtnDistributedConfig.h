/**
 * @file HdtnDistributedConfig.h
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
 *
 * @section DESCRIPTION
 *
 * The HdtnDistributedConfig class contains all the additional config parameters to run
 * HDTN in distributed mode. HdtnConfig is still required as it contains the core config.
 * HdtnDistributedConfig provides JSON serialization and deserialization capability.
 */

#ifndef HDTN_DISTRIBUTED_CONFIG_H
#define HDTN_DISTRIBUTED_CONFIG_H 1

#include <string>
#include <memory>
#include <boost/integer.hpp>
#include <set>
#include <vector>
#include <utility>
#include <tuple>
#include "JsonSerializable.h"
#include "config_lib_export.h"

class HdtnDistributedConfig;
typedef std::shared_ptr<HdtnDistributedConfig> HdtnDistributedConfig_ptr;

class HdtnDistributedConfig : public JsonSerializable {


public:
    CONFIG_LIB_EXPORT HdtnDistributedConfig();
    CONFIG_LIB_EXPORT ~HdtnDistributedConfig();

    //a copy constructor: X(const X&)
    CONFIG_LIB_EXPORT HdtnDistributedConfig(const HdtnDistributedConfig& o);

    //a move constructor: X(X&&)
    CONFIG_LIB_EXPORT HdtnDistributedConfig(HdtnDistributedConfig&& o) noexcept;

    //a copy assignment: operator=(const X&)
    CONFIG_LIB_EXPORT HdtnDistributedConfig& operator=(const HdtnDistributedConfig& o);

    //a move assignment: operator=(X&&)
    CONFIG_LIB_EXPORT HdtnDistributedConfig& operator=(HdtnDistributedConfig&& o) noexcept;

    CONFIG_LIB_EXPORT bool operator==(const HdtnDistributedConfig& other) const;

    CONFIG_LIB_EXPORT static HdtnDistributedConfig_ptr CreateFromPtree(const boost::property_tree::ptree & pt);
    CONFIG_LIB_EXPORT static HdtnDistributedConfig_ptr CreateFromJson(const std::string & jsonString, bool verifyNoUnusedJsonKeys = true);
    CONFIG_LIB_EXPORT static HdtnDistributedConfig_ptr CreateFromJsonFilePath(const boost::filesystem::path& jsonFilePath, bool verifyNoUnusedJsonKeys = true);
    CONFIG_LIB_EXPORT virtual boost::property_tree::ptree GetNewPropertyTree() const override;
    CONFIG_LIB_EXPORT virtual bool SetValuesFromPropertyTree(const boost::property_tree::ptree & pt) override;

public:

    

    std::string m_zmqIngressAddress;
    std::string m_zmqEgressAddress;
    std::string m_zmqStorageAddress;
    std::string m_zmqSchedulerAddress;
    std::string m_zmqRouterAddress;

    //push-pull between ingress and egress
    uint16_t m_zmqBoundIngressToConnectingEgressPortPath;
    uint16_t m_zmqConnectingEgressToBoundIngressPortPath;

    //push sock from egress to scheduler
    uint16_t m_zmqBoundEgressToConnectingSchedulerPortPath;

    //push sock from egress to ingress for TCPCL bundles received by egress
    uint16_t m_zmqConnectingEgressBundlesOnlyToBoundIngressPortPath;

    //push-pull between ingress and storage 
    uint16_t m_zmqBoundIngressToConnectingStoragePortPath;
    uint16_t m_zmqConnectingStorageToBoundIngressPortPath;

    //push-pull between storage and egress 
    uint16_t m_zmqConnectingStorageToBoundEgressPortPath;
    uint16_t m_zmqBoundEgressToConnectingStoragePortPath;

    //pub-sub from scheduler to all modules (defined in HdtnConfig as the TCP socket is used by hdtn-one-process)
    //uint16_t m_zmqBoundSchedulerPubSubPortPath;
    
    //push sock from router to egress
    uint16_t m_zmqConnectingRouterToBoundEgressPortPath;

    //telemetry sockets
    uint16_t m_zmqConnectingTelemToFromBoundIngressPortPath;
    uint16_t m_zmqConnectingTelemToFromBoundEgressPortPath;
    uint16_t m_zmqConnectingTelemToFromBoundStoragePortPath;
    uint16_t m_zmqConnectingTelemToFromBoundSchedulerPortPath;
};

#endif // HDTN_DISTRIBUTED_CONFIG_H

