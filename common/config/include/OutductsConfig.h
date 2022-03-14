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
#include "config_lib_export.h"

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
    uint64_t ltpMaxSendRateBitsPerSecOrZeroToDisable;

    //specific to udp
    uint64_t udpRateBps;

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
    std::string certificationAuthorityPemFileForVerification;

    CONFIG_LIB_EXPORT outduct_element_config_t();
    CONFIG_LIB_EXPORT ~outduct_element_config_t();

    CONFIG_LIB_EXPORT bool operator==(const outduct_element_config_t & o) const;


    //a copy constructor: X(const X&)
    CONFIG_LIB_EXPORT outduct_element_config_t(const outduct_element_config_t& o);

    //a move constructor: X(X&&)
    CONFIG_LIB_EXPORT outduct_element_config_t(outduct_element_config_t&& o);

    //a copy assignment: operator=(const X&)
    CONFIG_LIB_EXPORT outduct_element_config_t& operator=(const outduct_element_config_t& o);

    //a move assignment: operator=(X&&)
    CONFIG_LIB_EXPORT outduct_element_config_t& operator=(outduct_element_config_t&& o);
};

typedef std::vector<outduct_element_config_t> outduct_element_config_vector_t;



class OutductsConfig;
typedef boost::shared_ptr<OutductsConfig> OutductsConfig_ptr;

class OutductsConfig : public JsonSerializable {


public:
    CONFIG_LIB_EXPORT OutductsConfig();
    CONFIG_LIB_EXPORT ~OutductsConfig();

    //a copy constructor: X(const X&)
    CONFIG_LIB_EXPORT OutductsConfig(const OutductsConfig& o);

    //a move constructor: X(X&&)
    CONFIG_LIB_EXPORT OutductsConfig(OutductsConfig&& o);

    //a copy assignment: operator=(const X&)
    CONFIG_LIB_EXPORT OutductsConfig& operator=(const OutductsConfig& o);

    //a move assignment: operator=(X&&)
    CONFIG_LIB_EXPORT OutductsConfig& operator=(OutductsConfig&& o);

    CONFIG_LIB_EXPORT bool operator==(const OutductsConfig & other) const;

    CONFIG_LIB_EXPORT static OutductsConfig_ptr CreateFromPtree(const boost::property_tree::ptree & pt);
    CONFIG_LIB_EXPORT static OutductsConfig_ptr CreateFromJson(const std::string & jsonString);
    CONFIG_LIB_EXPORT static OutductsConfig_ptr CreateFromJsonFile(const std::string & jsonFileName);
    CONFIG_LIB_EXPORT virtual boost::property_tree::ptree GetNewPropertyTree() const;
    CONFIG_LIB_EXPORT virtual bool SetValuesFromPropertyTree(const boost::property_tree::ptree & pt);

public:

    std::string m_outductConfigName;
    outduct_element_config_vector_t m_outductElementConfigVector;
};

#endif // OUTDUCTS_CONFIG_H

