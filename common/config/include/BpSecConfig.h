/**
 * @file BpSecConfig.h
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
 * The BpSecConfig class contains all the config parameters to run BpSec
 *
 */

#ifndef BPSEC_CONFIG_H
#define BPSEC_CONFIG_H 1

#include <string>
#include <memory>
#include <boost/filesystem.hpp>
#include <set>
#include <array>
#include <vector>
#include <utility>
#include <tuple>
#include "JsonSerializable.h"
#include "BpSecConfig.h"
#include "EnumAsFlagsMacro.h"
#include <map>
#include "Logger.h"
#include "config_lib_export.h"


enum class BPSEC_SECURITY_FAILURE_EVENT : uint8_t {
    UNDEFINED = 0,
    SECURITY_OPERATION_MISCONFIGURED_AT_VERIFIER,
    SECURITY_OPERATION_MISSING_AT_VERIFIER,
    SECURITY_OPERATION_CORRUPTED_AT_VERIFIER,
    SECURITY_OPERATION_MISCONFIGURED_AT_ACCEPTOR,
    SECURITY_OPERATION_MISSING_AT_ACCEPTOR,
    SECURITY_OPERATION_CORRUPTED_AT_ACCEPTOR,
    RESERVED_MAX_EVENTS
};

enum class BPSEC_SECURITY_FAILURE_PROCESSING_ACTION_MASKS : uint16_t {
    NO_ACTIONS_SET =                           0,
    REMOVE_SECURITY_OPERATION =                1 << 0,
    REMOVE_SECURITY_OPERATION_TARGET_BLOCK =   1 << 1,
    REMOVE_ALL_SECURITY_TARGET_OPERATIONS =    1 << 2,
    FAIL_BUNDLE_FORWARDING =                   1 << 3,
    REQUEST_BUNDLE_STORAGE =                   1 << 4,
    REPORT_REASON_CODE =                       1 << 5,
    OVERRIDE_SECURITY_TARGET_BLOCK_BPCF =      1 << 6,
    OVERRIDE_SECURITY_BLOCK_BPCF =             1 << 7,
    RESERVED_NUM_MASKS =                       8
};
MAKE_ENUM_SUPPORT_FLAG_OPERATORS(BPSEC_SECURITY_FAILURE_PROCESSING_ACTION_MASKS);
MAKE_ENUM_SUPPORT_OSTREAM_OPERATOR(BPSEC_SECURITY_FAILURE_PROCESSING_ACTION_MASKS);


struct security_operation_event_plus_actions_pair_t : public JsonSerializable {
    BPSEC_SECURITY_FAILURE_EVENT m_event;
    BPSEC_SECURITY_FAILURE_PROCESSING_ACTION_MASKS m_actionMasks;

    CONFIG_LIB_EXPORT security_operation_event_plus_actions_pair_t();

    CONFIG_LIB_EXPORT bool operator==(const security_operation_event_plus_actions_pair_t& o) const;

    CONFIG_LIB_EXPORT virtual boost::property_tree::ptree GetNewPropertyTree() const override;
    CONFIG_LIB_EXPORT virtual bool SetValuesFromPropertyTree(const boost::property_tree::ptree& pt) override;

    CONFIG_LIB_EXPORT static unsigned int ActionMaskToBitPosition(const BPSEC_SECURITY_FAILURE_PROCESSING_ACTION_MASKS actionMask);
};

typedef std::vector<security_operation_event_plus_actions_pair_t> security_operation_event_plus_actions_pairs_vec_t;
typedef std::array<const security_operation_event_plus_actions_pair_t*,
    static_cast<uint8_t>(BPSEC_SECURITY_FAILURE_EVENT::RESERVED_MAX_EVENTS)> event_type_to_event_set_ptr_lut_t;

struct security_failure_event_sets_t : public JsonSerializable {
    //common to all outducts
    std::string m_name;
    std::string m_description;
    security_operation_event_plus_actions_pairs_vec_t m_securityOperationEventsVec;
    event_type_to_event_set_ptr_lut_t m_eventTypeToEventSetPtrLut;

    //boost::filesystem::path certificationAuthorityPemFileForVerification;

    CONFIG_LIB_EXPORT security_failure_event_sets_t();
    CONFIG_LIB_EXPORT ~security_failure_event_sets_t();

    CONFIG_LIB_EXPORT bool operator==(const security_failure_event_sets_t& o) const;
    CONFIG_LIB_EXPORT bool operator<(const security_failure_event_sets_t& o) const;


    //a copy constructor: X(const X&)
    CONFIG_LIB_EXPORT security_failure_event_sets_t(const security_failure_event_sets_t& o);

    //a move constructor: X(X&&)
    CONFIG_LIB_EXPORT security_failure_event_sets_t(security_failure_event_sets_t&& o) noexcept;

    //a copy assignment: operator=(const X&)
    CONFIG_LIB_EXPORT security_failure_event_sets_t& operator=(const security_failure_event_sets_t& o);

    //a move assignment: operator=(X&&)
    CONFIG_LIB_EXPORT security_failure_event_sets_t& operator=(security_failure_event_sets_t&& o) noexcept;

    CONFIG_LIB_EXPORT virtual boost::property_tree::ptree GetNewPropertyTree() const override;
    CONFIG_LIB_EXPORT virtual bool SetValuesFromPropertyTree(const boost::property_tree::ptree& pt) override;
};

typedef std::set<security_failure_event_sets_t> security_failure_event_sets_set_t;



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

struct security_context_param_t : public JsonSerializable {
    BPSEC_SECURITY_CONTEXT_PARAM_NAME m_paramName;
    uint64_t m_valueUint;
    boost::filesystem::path m_valuePath;

    CONFIG_LIB_EXPORT security_context_param_t();
    CONFIG_LIB_EXPORT security_context_param_t(BPSEC_SECURITY_CONTEXT_PARAM_NAME paramName, uint64_t valueUint);
    CONFIG_LIB_EXPORT security_context_param_t(BPSEC_SECURITY_CONTEXT_PARAM_NAME paramName, const boost::filesystem::path& valuePath);
    CONFIG_LIB_EXPORT ~security_context_param_t();

    CONFIG_LIB_EXPORT bool operator==(const security_context_param_t & other) const;

    //a copy constructor: X(const X&)
    CONFIG_LIB_EXPORT security_context_param_t(const security_context_param_t& o);

    //a move constructor: X(X&&)
    CONFIG_LIB_EXPORT security_context_param_t(security_context_param_t&& o) noexcept;

    //a copy assignment: operator=(const X&)
    CONFIG_LIB_EXPORT security_context_param_t& operator=(const security_context_param_t& o);

    //a move assignment: operator=(X&&)
    CONFIG_LIB_EXPORT security_context_param_t& operator=(security_context_param_t&& o) noexcept;

    CONFIG_LIB_EXPORT virtual boost::property_tree::ptree GetNewPropertyTree() const override;
    CONFIG_LIB_EXPORT virtual bool SetValuesFromPropertyTree(const boost::property_tree::ptree& pt) override;
};

typedef std::vector<security_context_param_t> security_context_params_vector_t;

struct policy_rules_t : public JsonSerializable {
    std::string m_description;
    uint64_t m_securityPolicyRuleId;
    std::string m_securityRole;
    std::string m_securitySource;
    std::set<std::string> m_bundleSource;
    std::set<std::string> m_bundleFinalDestination;
    std::set<uint64_t> m_securityTargetBlockTypes;
    std::string m_securityService;
    std::string m_securityContext;
    std::string m_securityFailureEventSetReferenceName;
    const security_failure_event_sets_t* m_securityFailureEventSetReferencePtr;
    security_context_params_vector_t m_securityContextParamsVec;

    CONFIG_LIB_EXPORT policy_rules_t();
    CONFIG_LIB_EXPORT ~policy_rules_t();

    CONFIG_LIB_EXPORT bool operator==(const policy_rules_t & other) const;
    //a copy constructor: X(const X&)
    CONFIG_LIB_EXPORT policy_rules_t(const policy_rules_t& o);

    //a move constructor: X(X&&)
    CONFIG_LIB_EXPORT policy_rules_t(policy_rules_t&& o) noexcept;

    //a copy assignment: operator=(const X&)
    CONFIG_LIB_EXPORT policy_rules_t& operator=(const policy_rules_t& o);

    //a move assignment: operator=(X&&)
    CONFIG_LIB_EXPORT policy_rules_t& operator=(policy_rules_t&& o) noexcept;

    CONFIG_LIB_EXPORT virtual boost::property_tree::ptree GetNewPropertyTree() const override;
    CONFIG_LIB_EXPORT virtual bool SetValuesFromPropertyTree(const boost::property_tree::ptree& pt) override;
};

typedef std::vector<policy_rules_t> policy_rules_vector_t;



class BpSecConfig;
typedef std::shared_ptr<BpSecConfig> BpSecConfig_ptr;

class BpSecConfig : public JsonSerializable {

public:
    CONFIG_LIB_EXPORT BpSecConfig();
    CONFIG_LIB_EXPORT ~BpSecConfig();

    //a copy constructor: X(const X&)
    CONFIG_LIB_EXPORT BpSecConfig(const BpSecConfig& o);

    //a move constructor: X(X&&)
    CONFIG_LIB_EXPORT BpSecConfig(BpSecConfig&& o) noexcept;

    //a copy assignment: operator=(const X&)
    CONFIG_LIB_EXPORT BpSecConfig& operator=(const BpSecConfig& o);

    //a move assignment: operator=(X&&)
    CONFIG_LIB_EXPORT BpSecConfig& operator=(BpSecConfig&& o) noexcept;

    CONFIG_LIB_EXPORT bool operator==(const BpSecConfig & other) const;

    CONFIG_LIB_EXPORT static BpSecConfig_ptr CreateFromPtree(const boost::property_tree::ptree & pt);
    CONFIG_LIB_EXPORT static BpSecConfig_ptr CreateFromJson(const std::string & jsonString, bool verifyNoUnusedJsonKeys = true);
    CONFIG_LIB_EXPORT static BpSecConfig_ptr CreateFromJsonFilePath(const boost::filesystem::path& jsonFilePath, bool verifyNoUnusedJsonKeys = true);
    CONFIG_LIB_EXPORT virtual boost::property_tree::ptree GetNewPropertyTree() const override;
    CONFIG_LIB_EXPORT virtual bool SetValuesFromPropertyTree(const boost::property_tree::ptree & pt) override;

public:
    std::string m_bpsecConfigName;
    policy_rules_vector_t m_policyRulesVector;
    security_failure_event_sets_set_t m_securityFailureEventSetsSet;
    BPSEC_SECURITY_FAILURE_PROCESSING_ACTION_MASKS m_actionMaskSopMissingAtAcceptor;
};

#endif // BPSEC_CONFIG_H

