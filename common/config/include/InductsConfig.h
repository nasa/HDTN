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

    //specific to stcp and tcpcl
    uint32_t keepAliveIntervalSeconds;

    //specific to tcpcl version 3 (servers)
    uint64_t tcpclV3MyMaxTxSegmentSizeBytes;

    //specific to tcpcl version 4 (servers)
    uint64_t tcpclV4MyMaxRxSegmentSizeBytes;
    bool tlsIsRequired;
    std::string certificatePemFile;
    std::string privateKeyPemFile;
    std::string diffieHellmanParametersPemFile;

    CONFIG_LIB_EXPORT induct_element_config_t();
    CONFIG_LIB_EXPORT ~induct_element_config_t();

    CONFIG_LIB_EXPORT bool operator==(const induct_element_config_t & o) const;


    //a copy constructor: X(const X&)
    CONFIG_LIB_EXPORT induct_element_config_t(const induct_element_config_t& o);

    //a move constructor: X(X&&)
    CONFIG_LIB_EXPORT induct_element_config_t(induct_element_config_t&& o);

    //a copy assignment: operator=(const X&)
    CONFIG_LIB_EXPORT induct_element_config_t& operator=(const induct_element_config_t& o);

    //a move assignment: operator=(X&&)
    CONFIG_LIB_EXPORT induct_element_config_t& operator=(induct_element_config_t&& o);
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
    CONFIG_LIB_EXPORT InductsConfig(InductsConfig&& o);

    //a copy assignment: operator=(const X&)
    CONFIG_LIB_EXPORT InductsConfig& operator=(const InductsConfig& o);

    //a move assignment: operator=(X&&)
    CONFIG_LIB_EXPORT InductsConfig& operator=(InductsConfig&& o);

    CONFIG_LIB_EXPORT bool operator==(const InductsConfig & other) const;

    CONFIG_LIB_EXPORT static InductsConfig_ptr CreateFromPtree(const boost::property_tree::ptree & pt);
    CONFIG_LIB_EXPORT static InductsConfig_ptr CreateFromJson(const std::string & jsonString);
    CONFIG_LIB_EXPORT static InductsConfig_ptr CreateFromJsonFile(const std::string & jsonFileName);
    CONFIG_LIB_EXPORT virtual boost::property_tree::ptree GetNewPropertyTree() const;
    CONFIG_LIB_EXPORT virtual bool SetValuesFromPropertyTree(const boost::property_tree::ptree & pt);

public:

    std::string m_inductConfigName;
    induct_element_config_vector_t m_inductElementConfigVector;
};

#endif // INDUCTS_CONFIG_H

