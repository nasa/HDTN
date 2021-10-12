#ifndef OUTDUCTS_CONFIG_H
#define OUTDUCTS_CONFIG_H 1

#include <string>
#include <boost/shared_ptr.hpp>
#include <boost/integer.hpp>
#include <set>
#include <vector>
#include <utility>
#include <tuple>
#include "JsonSerializable.h"

struct outduct_element_config_t {
    //common to all outducts
    std::string name;
    std::string convergenceLayer;
    std::string nextHopEndpointId;
    std::string remoteHostname;
    uint16_t remotePort;
    uint32_t bundlePipelineLimit;
    std::set<std::string> finalDestinationEidUris;
    

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

    //specific to udp
    uint64_t udpRateBps;

    //specific to stcp and tcpcl
    uint32_t keepAliveIntervalSeconds;

    //specific to tcpcl
    uint64_t tcpclAutoFragmentSizeBytes;

    outduct_element_config_t();
    ~outduct_element_config_t();

    bool operator==(const outduct_element_config_t & o) const;


    //a copy constructor: X(const X&)
    outduct_element_config_t(const outduct_element_config_t& o);

    //a move constructor: X(X&&)
    outduct_element_config_t(outduct_element_config_t&& o);

    //a copy assignment: operator=(const X&)
    outduct_element_config_t& operator=(const outduct_element_config_t& o);

    //a move assignment: operator=(X&&)
    outduct_element_config_t& operator=(outduct_element_config_t&& o);
};

typedef std::vector<outduct_element_config_t> outduct_element_config_vector_t;



class OutductsConfig;
typedef boost::shared_ptr<OutductsConfig> OutductsConfig_ptr;

class OutductsConfig : public JsonSerializable {


public:
    OutductsConfig();
    ~OutductsConfig();

    //a copy constructor: X(const X&)
    OutductsConfig(const OutductsConfig& o);

    //a move constructor: X(X&&)
    OutductsConfig(OutductsConfig&& o);

    //a copy assignment: operator=(const X&)
    OutductsConfig& operator=(const OutductsConfig& o);

    //a move assignment: operator=(X&&)
    OutductsConfig& operator=(OutductsConfig&& o);

    bool operator==(const OutductsConfig & other) const;

    static OutductsConfig_ptr CreateFromPtree(const boost::property_tree::ptree & pt);
    static OutductsConfig_ptr CreateFromJson(const std::string & jsonString);
    static OutductsConfig_ptr CreateFromJsonFile(const std::string & jsonFileName);
    virtual boost::property_tree::ptree GetNewPropertyTree() const;
    virtual bool SetValuesFromPropertyTree(const boost::property_tree::ptree & pt);

public:

    std::string m_outductConfigName;
    outduct_element_config_vector_t m_outductElementConfigVector;
};

#endif // OUTDUCTS_CONFIG_H

