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
    CONFIG_LIB_EXPORT HdtnConfig(HdtnConfig&& o) noexcept;

    //a copy assignment: operator=(const X&)
    CONFIG_LIB_EXPORT HdtnConfig& operator=(const HdtnConfig& o);

    //a move assignment: operator=(X&&)
    CONFIG_LIB_EXPORT HdtnConfig& operator=(HdtnConfig&& o) noexcept;

    CONFIG_LIB_EXPORT bool operator==(const HdtnConfig & other) const;

    CONFIG_LIB_EXPORT static HdtnConfig_ptr CreateFromPtree(const boost::property_tree::ptree & pt);
    CONFIG_LIB_EXPORT static HdtnConfig_ptr CreateFromJson(const std::string & jsonString, bool verifyNoUnusedJsonKeys = true);
    CONFIG_LIB_EXPORT static HdtnConfig_ptr CreateFromJsonFilePath(const boost::filesystem::path& jsonFilePath, bool verifyNoUnusedJsonKeys = true);
    CONFIG_LIB_EXPORT virtual boost::property_tree::ptree GetNewPropertyTree() const override;
    CONFIG_LIB_EXPORT virtual bool SetValuesFromPropertyTree(const boost::property_tree::ptree & pt) override;

public:

    std::string m_hdtnConfigName;
    bool m_userInterfaceOn;
    std::string m_mySchemeName;
    uint64_t m_myNodeId;
    uint64_t m_myBpEchoServiceId;
    std::string m_myCustodialSsp;
    uint64_t m_myCustodialServiceId;
    uint64_t m_mySchedulerServiceId;
    bool m_isAcsAware;
    uint64_t m_acsMaxFillsPerAcsPacket;
    uint64_t m_acsSendPeriodMilliseconds;
    uint64_t m_retransmitBundleAfterNoCustodySignalMilliseconds;
    uint64_t m_maxBundleSizeBytes;
    uint64_t m_maxIngressBundleWaitOnEgressMilliseconds;
    bool m_bufferRxToStorageOnLinkUpSaturation;
    uint64_t m_maxLtpReceiveUdpPacketSizeBytes;

    //pub-sub from scheduler to all modules (defined in HdtnConfig as the TCP socket is used by hdtn-one-process)
    uint16_t m_zmqBoundSchedulerPubSubPortPath;
    uint16_t m_zmqBoundTelemApiPortPath;

    InductsConfig m_inductsConfig;
    OutductsConfig m_outductsConfig;
    StorageConfig m_storageConfig;
};

#endif // HDTN_CONFIG_H

