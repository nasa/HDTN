/**
 * @file OutductsConfig.h
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
 * The OutductsConfig class contains all the config parameters for
 * instantiating zero or more HDTN outducts, and it
 * provides JSON serialization and deserialization capability.
 */

#ifndef OUTDUCTS_CONFIG_H
#define OUTDUCTS_CONFIG_H 1

#include <string>
#include <memory>
#include <boost/integer.hpp>
#include <boost/filesystem.hpp>
#include <set>
#include <vector>
#include <utility>
#include <tuple>
#include "JsonSerializable.h"
#include "config_lib_export.h"

struct outduct_element_config_t {
    //common to all outducts
    std::string name;
    std::string convergenceLayer;
    uint64_t nextHopNodeId;
    std::string remoteHostname;
    uint16_t remotePort;
    uint32_t maxNumberOfBundlesInPipeline;
    uint64_t maxSumOfBundleBytesInPipeline;
    

    //specific to ltp
    uint64_t thisLtpEngineId;
    uint64_t remoteLtpEngineId;
    uint32_t ltpDataSegmentMtu;
    uint64_t oneWayLightTimeMs;
    uint64_t oneWayMarginTimeMs;
    uint64_t clientServiceId;
    uint32_t numRxCircularBufferElements;
    uint32_t ltpMaxRetriesPerSerialNumber;
    uint32_t ltpCheckpointEveryNthDataSegment;
    uint32_t ltpRandomNumberSizeBits;
    uint16_t ltpSenderBoundPort;
    uint64_t ltpMaxUdpPacketsToSendPerSystemCall;
    uint64_t ltpSenderPingSecondsOrZeroToDisable;
    uint64_t delaySendingOfDataSegmentsTimeMsOrZeroToDisable;
    bool keepActiveSessionDataOnDisk;
    uint64_t activeSessionDataOnDiskNewFileDurationMs;
    boost::filesystem::path activeSessionDataOnDiskDirectory;

    //specific to udp and ltp
    uint64_t rateLimitPrecisionMicroSec;

    //specific to slip over uart
    std::string comPort;
    uint32_t baudRate;

    //specific to stcp and tcpcl
    uint32_t keepAliveIntervalSeconds;

    //specific to tcpcl version 3 (clients)
    uint64_t tcpclV3MyMaxTxSegmentSizeBytes;

    //specific to tcpcl all versions
    bool tcpclAllowOpportunisticReceiveBundles;

    //specific to tcpcl version 4 (clients)
    uint64_t tcpclV4MyMaxRxSegmentSizeBytes;
    bool tryUseTls;
    bool tlsIsRequired;
    bool useTlsVersion1_3;
    bool doX509CertificateVerification;
    bool verifySubjectAltNameInX509Certificate;
    boost::filesystem::path certificationAuthorityPemFileForVerification;

    CONFIG_LIB_EXPORT outduct_element_config_t();
    CONFIG_LIB_EXPORT ~outduct_element_config_t();

    CONFIG_LIB_EXPORT bool operator==(const outduct_element_config_t & o) const;


    //a copy constructor: X(const X&)
    CONFIG_LIB_EXPORT outduct_element_config_t(const outduct_element_config_t& o);

    //a move constructor: X(X&&)
    CONFIG_LIB_EXPORT outduct_element_config_t(outduct_element_config_t&& o) noexcept;

    //a copy assignment: operator=(const X&)
    CONFIG_LIB_EXPORT outduct_element_config_t& operator=(const outduct_element_config_t& o);

    //a move assignment: operator=(X&&)
    CONFIG_LIB_EXPORT outduct_element_config_t& operator=(outduct_element_config_t&& o) noexcept;
};

typedef std::vector<outduct_element_config_t> outduct_element_config_vector_t;



class OutductsConfig;
typedef std::shared_ptr<OutductsConfig> OutductsConfig_ptr;

class OutductsConfig : public JsonSerializable {


public:
    CONFIG_LIB_EXPORT OutductsConfig();
    CONFIG_LIB_EXPORT ~OutductsConfig();

    //a copy constructor: X(const X&)
    CONFIG_LIB_EXPORT OutductsConfig(const OutductsConfig& o);

    //a move constructor: X(X&&)
    CONFIG_LIB_EXPORT OutductsConfig(OutductsConfig&& o) noexcept;

    //a copy assignment: operator=(const X&)
    CONFIG_LIB_EXPORT OutductsConfig& operator=(const OutductsConfig& o);

    //a move assignment: operator=(X&&)
    CONFIG_LIB_EXPORT OutductsConfig& operator=(OutductsConfig&& o) noexcept;

    CONFIG_LIB_EXPORT bool operator==(const OutductsConfig & other) const;

    CONFIG_LIB_EXPORT static OutductsConfig_ptr CreateFromPtree(const boost::property_tree::ptree & pt);
    CONFIG_LIB_EXPORT static OutductsConfig_ptr CreateFromJson(const std::string& jsonString, bool verifyNoUnusedJsonKeys = true);
    CONFIG_LIB_EXPORT static OutductsConfig_ptr CreateFromJsonFilePath(const boost::filesystem::path& jsonFilePath, bool verifyNoUnusedJsonKeys = true);
    CONFIG_LIB_EXPORT virtual boost::property_tree::ptree GetNewPropertyTree() const override;
    CONFIG_LIB_EXPORT virtual bool SetValuesFromPropertyTree(const boost::property_tree::ptree & pt) override;

public:

    std::string m_outductConfigName;
    outduct_element_config_vector_t m_outductElementConfigVector;
};

#endif // OUTDUCTS_CONFIG_H

