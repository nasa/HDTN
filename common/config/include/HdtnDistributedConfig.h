/**
 * @file HdtnDistributedConfig.h
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
    std::string m_zmqRouterAddress;

    //push-pull between ingress and egress
    uint16_t m_zmqBoundIngressToConnectingEgressPortPath;
    uint16_t m_zmqConnectingEgressToBoundIngressPortPath;

    //push sock from egress to router
    uint16_t m_zmqBoundEgressToConnectingRouterPortPath;

    //push sock from egress to ingress for TCPCL bundles received by egress
    uint16_t m_zmqConnectingEgressBundlesOnlyToBoundIngressPortPath;

    //push-pull between ingress and storage 
    uint16_t m_zmqBoundIngressToConnectingStoragePortPath;
    uint16_t m_zmqConnectingStorageToBoundIngressPortPath;

    //push-pull between storage and egress 
    uint16_t m_zmqConnectingStorageToBoundEgressPortPath;
    uint16_t m_zmqBoundEgressToConnectingStoragePortPath;

    //push sock from storage to router
    uint64_t m_zmqConnectingStorageToBoundRouterPortPath;

    //pub-sub from router to all modules (defined in HdtnConfig as the TCP socket is used by hdtn-one-process)
    //uint16_t m_zmqBoundRouterPubSubPortPath;
    
    //push sock from router to egress
    uint16_t m_zmqConnectingRouterToBoundEgressPortPath;

    //telemetry sockets
    uint16_t m_zmqConnectingTelemToFromBoundIngressPortPath;
    uint16_t m_zmqConnectingTelemToFromBoundEgressPortPath;
    uint16_t m_zmqConnectingTelemToFromBoundStoragePortPath;
    uint16_t m_zmqConnectingTelemToFromBoundRouterPortPath;
};

#endif // HDTN_DISTRIBUTED_CONFIG_H

