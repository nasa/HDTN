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


enum class BPSEC_SECURITY_CONTEXT_PARAM_NAME : uint8_t {
    UNDEFINED = 0,
    AES_VARIANT,
    SHA_VARIANT,
    IV_SIZE_BYTES,
    SCOPE_FLAGS,
    SECURITY_BLOCK_CRC,
    KEY_ENCRYPTION_KEY_FILE,
    KEY_FILE,
    RESERVED_MAX_PARAM_NAMES
};
enum class BPSEC_SECURITY_CONTEXT_PARAM_TYPE : uint8_t {
    UNDEFINED = 0,
    U64,
    PATH
};

struct security_context_params_config_t : public JsonSerializable {
    BPSEC_SECURITY_CONTEXT_PARAM_NAME m_paramName;
    uint64_t m_valueUint;
    boost::filesystem::path m_valuePath;

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

    CONFIG_LIB_EXPORT virtual boost::property_tree::ptree GetNewPropertyTree() const override;
    CONFIG_LIB_EXPORT virtual bool SetValuesFromPropertyTree(const boost::property_tree::ptree& pt) override;
};

typedef std::vector<security_context_params_config_t> security_context_params_vector_t;

struct policy_rules_config_t : public JsonSerializable {
    std::string m_description;
    uint64_t m_securityPolicyRuleId;
    std::string m_securityRole;
    std::string m_securitySource;
    std::set<std::string> m_bundleSource;
    std::set<std::string> m_bundleFinalDestination;
    std::set<uint64_t> m_securityTargetBlockTypes;
    std::string m_securityService;
    std::string m_securityContext;
    std::string m_securityFailureEventSetReference;
    security_context_params_vector_t m_securityContextParamsVec;

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

    CONFIG_LIB_EXPORT virtual boost::property_tree::ptree GetNewPropertyTree() const override;
    CONFIG_LIB_EXPORT virtual bool SetValuesFromPropertyTree(const boost::property_tree::ptree& pt) override;
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
    security_operation_events_config_vector_t m_securityOperationEventsConfigVector;
    
};

#endif // BPSEC_CONFIG_H

