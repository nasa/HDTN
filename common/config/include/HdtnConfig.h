/**
 * @file HdtnConfig.h
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
 * The HdtnConfig class contains all the config parameters to run HDTN and
 * provides JSON serialization and deserialization capability.
 */

#ifndef HDTN_CONFIG_H
#define HDTN_CONFIG_H 1

#include <string>
#include <memory>
#include <boost/integer.hpp>
#include <set>
#include <vector>
#include <utility>
#include <tuple>
#include "JsonSerializable.h"
#include "InductsConfig.h"
#include "OutductsConfig.h"
#include "StorageConfig.h"
#include "config_lib_export.h"

class HdtnConfig;
typedef std::shared_ptr<HdtnConfig> HdtnConfig_ptr;

class HdtnConfig : public JsonSerializable {


public:
    CONFIG_LIB_EXPORT HdtnConfig();
    CONFIG_LIB_EXPORT ~HdtnConfig();

    //a copy constructor: X(const X&)
    CONFIG_LIB_EXPORT HdtnConfig(const HdtnConfig& o);

    //a move constructor: X(X&&)
    CONFIG_LIB_EXPORT HdtnConfig(HdtnConfig&& o);

    //a copy assignment: operator=(const X&)
    CONFIG_LIB_EXPORT HdtnConfig& operator=(const HdtnConfig& o);

    //a move assignment: operator=(X&&)
    CONFIG_LIB_EXPORT HdtnConfig& operator=(HdtnConfig&& o);

    CONFIG_LIB_EXPORT bool operator==(const HdtnConfig & other) const;

    CONFIG_LIB_EXPORT static HdtnConfig_ptr CreateFromPtree(const boost::property_tree::ptree & pt);
    CONFIG_LIB_EXPORT static HdtnConfig_ptr CreateFromJson(const std::string & jsonString);
    CONFIG_LIB_EXPORT static HdtnConfig_ptr CreateFromJsonFile(const std::string & jsonFileName);
    CONFIG_LIB_EXPORT virtual boost::property_tree::ptree GetNewPropertyTree() const;
    CONFIG_LIB_EXPORT virtual bool SetValuesFromPropertyTree(const boost::property_tree::ptree & pt);

public:

    std::string m_hdtnConfigName;
    bool m_userInterfaceOn;
    std::string m_mySchemeName;
    uint64_t m_myNodeId;
    uint64_t m_myBpEchoServiceId;
    std::string m_myCustodialSsp;
    uint64_t m_myCustodialServiceId;
    bool m_isAcsAware;
    uint64_t m_acsMaxFillsPerAcsPacket;
    uint64_t m_acsSendPeriodMilliseconds;
    uint64_t m_retransmitBundleAfterNoCustodySignalMilliseconds;
    uint64_t m_maxBundleSizeBytes;
    uint64_t m_maxIngressBundleWaitOnEgressMilliseconds;
    uint64_t m_maxLtpReceiveUdpPacketSizeBytes;

    std::string m_zmqIngressAddress;
    std::string m_zmqEgressAddress;
    std::string m_zmqStorageAddress;
    std::string m_zmqSchedulerAddress;
    std::string m_zmqRouterAddress;
    uint16_t m_zmqBoundIngressToConnectingEgressPortPath; //#define HDTN_BOUND_INGRESS_TO_CONNECTING_EGRESS_PATH "tcp://127.0.0.1:10100"
    uint16_t m_zmqConnectingEgressToBoundIngressPortPath; //#define HDTN_CONNECTING_EGRESS_TO_BOUND_INGRESS_PATH "tcp://127.0.0.1:10160"
    uint16_t m_zmqConnectingEgressToBoundSchedulerPortPath; //#define HDTN_CONNECTING_EGRESS_TO_BOUND_SCHEDULER_PATH "tcp://127.0.0.1:10162"

    uint16_t m_zmqConnectingEgressBundlesOnlyToBoundIngressPortPath; //"tcp://127.0.0.1:10161"
    //push-pull from ingress to storage 
    //#define HDTN_STORAGE_PATH "tcp://0.0.0.0:10110"
    uint16_t m_zmqBoundIngressToConnectingStoragePortPath; //#define HDTN_BOUND_INGRESS_TO_CONNECTING_STORAGE_PATH "tcp://127.0.0.1:10110"
    uint16_t m_zmqConnectingStorageToBoundIngressPortPath; //#define HDTN_CONNECTING_STORAGE_TO_BOUND_INGRESS_PATH "tcp://127.0.0.1:10150"
//push-pull from storage to release 
//#define HDTN_RELEASE_PATH "tcp://0.0.0.0:10120"
    uint16_t m_zmqConnectingStorageToBoundEgressPortPath; //#define HDTN_CONNECTING_STORAGE_TO_BOUND_EGRESS_PATH "tcp://127.0.0.1:10120"
    uint16_t m_zmqBoundEgressToConnectingStoragePortPath; //#define HDTN_BOUND_EGRESS_TO_CONNECTING_STORAGE_PATH "tcp://127.0.0.1:10130"
//pub-sub from scheduler to modules
//#define HDTN_SCHEDULER_PATH "tcp://0.0.0.0:10200"
    uint16_t m_zmqBoundSchedulerPubSubPortPath; //#define HDTN_BOUND_SCHEDULER_PUBSUB_PATH "tcp://127.0.0.1:10200"
    uint16_t m_zmqBoundRouterPubSubPortPath; //#define HDTN_BOUND_ROUTER_PUBSUB_PATH "tcp://127.0.0.1:10210"
    uint64_t m_zmqMaxMessageSizeBytes;

    InductsConfig m_inductsConfig;
    OutductsConfig m_outductsConfig;
    StorageConfig m_storageConfig;
};

#endif // HDTN_CONFIG_H

