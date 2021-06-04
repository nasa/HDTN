#ifndef INDUCTS_CONFIG_H
#define INDUCTS_CONFIG_H 1

#include <string>
#include <boost/shared_ptr.hpp>
#include <boost/integer.hpp>
#include <map>
#include <vector>
#include <utility>
#include <tuple>
#include "JsonSerializable.h"

struct induct_element_config_t {
    //common to all inducts
    std::string name;
    std::string convergenceLayer;
    std::string endpointIdStr;
    uint16_t boundPort;
    uint32_t numRxCircularBufferElements;
    uint32_t numRxCircularBufferBytesPerElement;

    //specific to ltp
    uint64_t thisLtpEngineId;
    uint32_t ltpReportSegmentMtu;
    uint64_t oneWayLightTimeMs;
    uint64_t oneWayMarginTimeMs;
    uint64_t clientServiceId;
    uint64_t preallocatedRedDataBytes;

    //specific to stcp and tcpcl
    uint32_t keepAliveIntervalSeconds;

    induct_element_config_t();
    ~induct_element_config_t();

    bool operator==(const induct_element_config_t & o) const;


    //a copy constructor: X(const X&)
    induct_element_config_t(const induct_element_config_t& o);

    //a move constructor: X(X&&)
    induct_element_config_t(induct_element_config_t&& o);

    //a copy assignment: operator=(const X&)
    induct_element_config_t& operator=(const induct_element_config_t& o);

    //a move assignment: operator=(X&&)
    induct_element_config_t& operator=(induct_element_config_t&& o);
};

typedef std::vector<induct_element_config_t> induct_element_config_vector_t;



class InductsConfig;
typedef boost::shared_ptr<InductsConfig> InductsConfig_ptr;

class InductsConfig : public JsonSerializable {


public:
    InductsConfig();
    ~InductsConfig();

    //a copy constructor: X(const X&)
    InductsConfig(const InductsConfig& o);

    //a move constructor: X(X&&)
    InductsConfig(InductsConfig&& o);

    //a copy assignment: operator=(const X&)
    InductsConfig& operator=(const InductsConfig& o);

    //a move assignment: operator=(X&&)
    InductsConfig& operator=(InductsConfig&& o);

    bool operator==(const InductsConfig & other) const;

    static InductsConfig_ptr CreateFromPtree(const boost::property_tree::ptree & pt);
    static InductsConfig_ptr CreateFromJson(const std::string & jsonString);
    static InductsConfig_ptr CreateFromJsonFile(const std::string & jsonFileName);
    virtual boost::property_tree::ptree GetNewPropertyTree() const;
    virtual bool SetValuesFromPropertyTree(const boost::property_tree::ptree & pt);

public:

    std::string m_inductConfigName;
    induct_element_config_vector_t m_inductElementConfigVector;
};

#endif // INDUCTS_CONFIG_H

