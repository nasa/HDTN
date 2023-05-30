/**
 * @file BPSecConfig.h
 * @author  Nadia Kortas <nadia.kortas@nasa.gov>
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
 * The BPSecConfig class contains all the config parameters to run BPSec
 *
 */

#ifndef BPSEC_CONFIG_H
#define BPSEC_CONFIG_H 1

#include <string>
#include <memory>
#include <boost/filesystem.hpp>
#include <boost/integer.hpp>
#include <set>
#include <vector>
#include <utility>
#include <tuple>
#include "JsonSerializable.h"
#include "BPSecConfig.h"
#include "config_lib_export.h"
#include <map>
#include "Logger.h"


struct security_operation_events_config_t {
    std::string eventId;
    std::set<std::string> actions;

    CONFIG_LIB_EXPORT security_operation_events_config_t();
    CONFIG_LIB_EXPORT ~security_operation_events_config_t();

    CONFIG_LIB_EXPORT bool operator==(const security_operation_events_config_t & other) const;

    //a copy constructor: X(const X&)
    CONFIG_LIB_EXPORT security_operation_events_config_t(const security_operation_events_config_t& o);

    //a move constructor: X(X&&)
    CONFIG_LIB_EXPORT security_operation_events_config_t(security_operation_events_config_t&& o) noexcept;

    //a copy assignment: operator=(const X&)
    CONFIG_LIB_EXPORT security_operation_events_config_t& operator=(const security_operation_events_config_t& o);

    //a move assignment: operator=(X&&)
    CONFIG_LIB_EXPORT security_operation_events_config_t& operator=(security_operation_events_config_t&& o) noexcept;
};

typedef std::vector<security_operation_events_config_t> security_operation_events_config_vector_t;

struct security_context_params_config_t {
    uint64_t id;
    std::string paramName;
    uint64_t value;

    CONFIG_LIB_EXPORT security_context_params_config_t();
    CONFIG_LIB_EXPORT ~security_context_params_config_t();

    CONFIG_LIB_EXPORT bool operator==(const security_context_params_config_t & other) const;

    //a copy constructor: X(const X&)
    CONFIG_LIB_EXPORT security_context_params_config_t(const security_context_params_config_t& o);

    //a move constructor: X(X&&)
    CONFIG_LIB_EXPORT security_context_params_config_t(security_context_params_config_t&& o) noexcept;

    //a copy assignment: operator=(const X&)
    CONFIG_LIB_EXPORT security_context_params_config_t& operator=(const security_context_params_config_t& o);

    //a move assignment: operator=(X&&)
    CONFIG_LIB_EXPORT security_context_params_config_t& operator=(security_context_params_config_t&& o) noexcept;
};

typedef std::vector<security_context_params_config_t> security_context_params_vector_t;

struct policy_rules_config_t {
    std::string desc;
    uint64_t securityPolicyRuleId;
    std::string securityRole;
    std::string securitySource;
    std::string bundleSource;
    std::string finalDest;
    std::string securityTargetBlockType;
    std::string securityService;
    std::string securityContextId;
    std::string securityFailureEventSetReference;
    security_context_params_vector_t securityContextParams;

    CONFIG_LIB_EXPORT policy_rules_config_t();
    CONFIG_LIB_EXPORT ~policy_rules_config_t();

    CONFIG_LIB_EXPORT bool operator==(const policy_rules_config_t & other) const;
    //a copy constructor: X(const X&)
    CONFIG_LIB_EXPORT policy_rules_config_t(const policy_rules_config_t& o);

    //a move constructor: X(X&&)
    CONFIG_LIB_EXPORT policy_rules_config_t(policy_rules_config_t&& o) noexcept;

    //a copy assignment: operator=(const X&)
    CONFIG_LIB_EXPORT policy_rules_config_t& operator=(const policy_rules_config_t& o);

    //a move assignment: operator=(X&&)
    CONFIG_LIB_EXPORT policy_rules_config_t& operator=(policy_rules_config_t&& o) noexcept;
};

typedef std::vector<policy_rules_config_t> policy_rules_config_vector_t;

struct security_failure_eventSets_config_t {
    //common to all outducts
    std::string name;
    std::string desc;
    security_operation_events_config_vector_t securityOperationEvents;

    //boost::filesystem::path certificationAuthorityPemFileForVerification;

    CONFIG_LIB_EXPORT security_failure_eventSets_config_t();
    CONFIG_LIB_EXPORT ~security_failure_eventSets_config_t();

    CONFIG_LIB_EXPORT bool operator==(const security_failure_eventSets_config_t & o) const;


    //a copy constructor: X(const X&)
    CONFIG_LIB_EXPORT security_failure_eventSets_config_t(const security_failure_eventSets_config_t& o);

    //a move constructor: X(X&&)
    CONFIG_LIB_EXPORT security_failure_eventSets_config_t(security_failure_eventSets_config_t&& o) noexcept;

    //a copy assignment: operator=(const X&)
    CONFIG_LIB_EXPORT security_failure_eventSets_config_t& operator=(const security_failure_eventSets_config_t& o);

    //a move assignment: operator=(X&&)
    CONFIG_LIB_EXPORT security_failure_eventSets_config_t& operator=(security_failure_eventSets_config_t&& o) noexcept;
};

typedef std::vector<security_failure_eventSets_config_t> security_failure_eventSets_config_vector_t;

class BPSecConfig;
typedef std::shared_ptr<BPSecConfig> BPSecConfig_ptr;

class BPSecConfig : public JsonSerializable {

public:
    CONFIG_LIB_EXPORT BPSecConfig();
    CONFIG_LIB_EXPORT ~BPSecConfig();

    //a copy constructor: X(const X&)
    CONFIG_LIB_EXPORT BPSecConfig(const BPSecConfig& o);

    //a move constructor: X(X&&)
    CONFIG_LIB_EXPORT BPSecConfig(BPSecConfig&& o) noexcept;

    //a copy assignment: operator=(const X&)
    CONFIG_LIB_EXPORT BPSecConfig& operator=(const BPSecConfig& o);

    //a move assignment: operator=(X&&)
    CONFIG_LIB_EXPORT BPSecConfig& operator=(BPSecConfig&& o) noexcept;

    CONFIG_LIB_EXPORT bool operator==(const BPSecConfig & other) const;

    CONFIG_LIB_EXPORT static BPSecConfig_ptr CreateFromPtree(const boost::property_tree::ptree & pt);
    CONFIG_LIB_EXPORT static BPSecConfig_ptr CreateFromJson(const std::string & jsonString, bool verifyNoUnusedJsonKeys = true);
    CONFIG_LIB_EXPORT static BPSecConfig_ptr CreateFromJsonFilePath(const boost::filesystem::path& jsonFilePath, bool verifyNoUnusedJsonKeys = true);
    CONFIG_LIB_EXPORT virtual boost::property_tree::ptree GetNewPropertyTree() const override;
    CONFIG_LIB_EXPORT virtual bool SetValuesFromPropertyTree(const boost::property_tree::ptree & pt) override;

public:
    std::string m_bpsecConfigName;
    policy_rules_config_vector_t m_policyRulesConfigVector;
    security_failure_eventSets_config_vector_t m_securityFailureEventSetsConfigVector;
    security_context_params_vector_t m_securityContextParamsVector;
    security_operation_events_config_vector_t m_securityOperationEventsConfigVector;
    
};

#endif // BPSEC_CONFIG_H

