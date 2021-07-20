#ifndef HDTN_CONFIG_H
#define HDTN_CONFIG_H 1

#include <string>
#include <boost/shared_ptr.hpp>
#include <boost/integer.hpp>
#include <set>
#include <vector>
#include <utility>
#include <tuple>
#include "JsonSerializable.h"
#include "InductsConfig.h"
#include "OutductsConfig.h"
#include "StorageConfig.h"

class HdtnConfig;
typedef boost::shared_ptr<HdtnConfig> HdtnConfig_ptr;

class HdtnConfig : public JsonSerializable {


public:
    HdtnConfig();
    ~HdtnConfig();

    //a copy constructor: X(const X&)
    HdtnConfig(const HdtnConfig& o);

    //a move constructor: X(X&&)
    HdtnConfig(HdtnConfig&& o);

    //a copy assignment: operator=(const X&)
    HdtnConfig& operator=(const HdtnConfig& o);

    //a move assignment: operator=(X&&)
    HdtnConfig& operator=(HdtnConfig&& o);

    bool operator==(const HdtnConfig & other) const;

    static HdtnConfig_ptr CreateFromPtree(const boost::property_tree::ptree & pt);
    static HdtnConfig_ptr CreateFromJson(const std::string & jsonString);
    static HdtnConfig_ptr CreateFromJsonFile(const std::string & jsonFileName);
    virtual boost::property_tree::ptree GetNewPropertyTree() const;
    virtual bool SetValuesFromPropertyTree(const boost::property_tree::ptree & pt);

public:

    std::string m_hdtnConfigName;

    std::string m_zmqIngressAddress;
    std::string m_zmqEgressAddress;
    std::string m_zmqStorageAddress;
    std::string m_zmqRegistrationServerAddress;
    std::string m_zmqSchedulerAddress;

    uint16_t m_zmqBoundIngressToConnectingEgressPortPath; //#define HDTN_BOUND_INGRESS_TO_CONNECTING_EGRESS_PATH "tcp://127.0.0.1:10100"
    uint16_t m_zmqConnectingEgressToBoundIngressPortPath; //#define HDTN_CONNECTING_EGRESS_TO_BOUND_INGRESS_PATH "tcp://127.0.0.1:10160"
    //push-pull from ingress to storage 
    //#define HDTN_STORAGE_PATH "tcp://0.0.0.0:10110"
    uint16_t m_zmqBoundIngressToConnectingStoragePortPath; //#define HDTN_BOUND_INGRESS_TO_CONNECTING_STORAGE_PATH "tcp://127.0.0.1:10110"
    uint16_t m_zmqConnectingStorageToBoundIngressPortPath; //#define HDTN_CONNECTING_STORAGE_TO_BOUND_INGRESS_PATH "tcp://127.0.0.1:10150"
//push-pull from storage to release 
//#define HDTN_RELEASE_PATH "tcp://0.0.0.0:10120"
    uint16_t m_zmqConnectingStorageToBoundEgressPortPath; //#define HDTN_CONNECTING_STORAGE_TO_BOUND_EGRESS_PATH "tcp://127.0.0.1:10120"
    uint16_t m_zmqBoundEgressToConnectingStoragePortPath; //#define HDTN_BOUND_EGRESS_TO_CONNECTING_STORAGE_PATH "tcp://127.0.0.1:10130"
//req-reply to reg server
    uint16_t m_zmqRegistrationServerPortPath; //#define HDTN_REG_SERVER_PATH "tcp://127.0.0.1:10140"
//pub-sub from scheduler to modules
//#define HDTN_SCHEDULER_PATH "tcp://0.0.0.0:10200"
    uint16_t m_zmqBoundSchedulerPubSubPortPath; //#define HDTN_BOUND_SCHEDULER_PUBSUB_PATH "tcp://127.0.0.1:10200"

    uint64_t m_zmqMaxMessagesPerPath;
    uint64_t m_zmqMaxMessageSizeBytes;

    InductsConfig m_inductsConfig;
    OutductsConfig m_outductsConfig;
    StorageConfig m_storageConfig;
};

#endif // HDTN_CONFIG_H

