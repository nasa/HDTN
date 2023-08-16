/**
 * @file InductsConfig.h
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
 * The InductsConfig class contains all the config parameters for
 * instantiating zero or more HDTN inducts, and it
 * provides JSON serialization and deserialization capability.
 */

#ifndef INDUCTS_CONFIG_H
#define INDUCTS_CONFIG_H 1

#include <string>
#include <memory>
#include <boost/integer.hpp>
#include <map>
#include <vector>
#include <utility>
#include <tuple>
#include "JsonSerializable.h"
#include <boost/filesystem.hpp>
#include "config_lib_export.h"

struct induct_element_config_t {
    //common to all inducts
    std::string name;
    std::string convergenceLayer;
    uint16_t boundPort;
    uint32_t numRxCircularBufferElements;
    uint32_t numRxCircularBufferBytesPerElement;

    //specific to ltp
    uint64_t thisLtpEngineId;
    uint64_t remoteLtpEngineId;
    uint32_t ltpReportSegmentMtu;
    uint64_t oneWayLightTimeMs;
    uint64_t oneWayMarginTimeMs;
    uint64_t clientServiceId;
    uint64_t preallocatedRedDataBytes;
    uint32_t ltpMaxRetriesPerSerialNumber;
    uint32_t ltpRandomNumberSizeBits;
    std::string ltpRemoteUdpHostname;
    uint16_t ltpRemoteUdpPort;
    uint64_t ltpRxDataSegmentSessionNumberRecreationPreventerHistorySize;
    uint64_t ltpMaxExpectedSimultaneousSessions;
    uint64_t ltpMaxUdpPacketsToSendPerSystemCall;
    uint64_t delaySendingOfReportSegmentsTimeMsOrZeroToDisable;
    bool keepActiveSessionDataOnDisk;
    uint64_t activeSessionDataOnDiskNewFileDurationMs;
    boost::filesystem::path activeSessionDataOnDiskDirectory;

    //specific to slip over uart
    std::string comPort;
    uint32_t baudRate;
    uint64_t uartRemoteNodeId;

    //specific to stcp and tcpcl
    uint32_t keepAliveIntervalSeconds;

    //specific to tcpcl version 3 (servers)
    uint64_t tcpclV3MyMaxTxSegmentSizeBytes;

    //specific to tcpcl version 4 (servers)
    uint64_t tcpclV4MyMaxRxSegmentSizeBytes;
    bool tlsIsRequired;
    boost::filesystem::path certificatePemFile;
    boost::filesystem::path privateKeyPemFile;
    boost::filesystem::path diffieHellmanParametersPemFile;

    CONFIG_LIB_EXPORT induct_element_config_t();
    CONFIG_LIB_EXPORT ~induct_element_config_t();

    CONFIG_LIB_EXPORT bool operator==(const induct_element_config_t & o) const;


    //a copy constructor: X(const X&)
    CONFIG_LIB_EXPORT induct_element_config_t(const induct_element_config_t& o);

    //a move constructor: X(X&&)
    CONFIG_LIB_EXPORT induct_element_config_t(induct_element_config_t&& o) noexcept;

    //a copy assignment: operator=(const X&)
    CONFIG_LIB_EXPORT induct_element_config_t& operator=(const induct_element_config_t& o);

    //a move assignment: operator=(X&&)
    CONFIG_LIB_EXPORT induct_element_config_t& operator=(induct_element_config_t&& o) noexcept;
};

typedef std::vector<induct_element_config_t> induct_element_config_vector_t;



class InductsConfig;
typedef std::shared_ptr<InductsConfig> InductsConfig_ptr;

class InductsConfig : public JsonSerializable {


public:
    CONFIG_LIB_EXPORT InductsConfig();
    CONFIG_LIB_EXPORT ~InductsConfig();

    //a copy constructor: X(const X&)
    CONFIG_LIB_EXPORT InductsConfig(const InductsConfig& o);

    //a move constructor: X(X&&)
    CONFIG_LIB_EXPORT InductsConfig(InductsConfig&& o) noexcept;

    //a copy assignment: operator=(const X&)
    CONFIG_LIB_EXPORT InductsConfig& operator=(const InductsConfig& o);

    //a move assignment: operator=(X&&)
    CONFIG_LIB_EXPORT InductsConfig& operator=(InductsConfig&& o) noexcept;

    CONFIG_LIB_EXPORT bool operator==(const InductsConfig & other) const;

    CONFIG_LIB_EXPORT static InductsConfig_ptr CreateFromPtree(const boost::property_tree::ptree & pt);
    CONFIG_LIB_EXPORT static InductsConfig_ptr CreateFromJson(const std::string& jsonString, bool verifyNoUnusedJsonKeys = true);
    CONFIG_LIB_EXPORT static InductsConfig_ptr CreateFromJsonFilePath(const boost::filesystem::path& jsonFilePath, bool verifyNoUnusedJsonKeys = true);
    CONFIG_LIB_EXPORT virtual boost::property_tree::ptree GetNewPropertyTree() const override;
    CONFIG_LIB_EXPORT virtual bool SetValuesFromPropertyTree(const boost::property_tree::ptree & pt) override;

public:

    std::string m_inductConfigName;
    induct_element_config_vector_t m_inductElementConfigVector;
};

#endif // INDUCTS_CONFIG_H

