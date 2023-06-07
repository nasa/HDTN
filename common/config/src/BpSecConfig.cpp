/**
 * @file BpSecConfig.cpp
 * @author Nadia Kortas <nadia.kortas@nasa.gov>
 *
 * @copyright Copyright © 2021 United States Government as represented by
 * the National Aeronautics and Space Administration.
 * No copyright is claimed in the United States under Title 17, U.S.Code.
 * All Other Rights Reserved.
 *
 * @section LICENSE
 * Released under the NASA Open Source Agreement (NOSA)
 * See LICENSE.md in the source root directory for more information.
 */

#include "BpSecConfig.h"
#include "Logger.h"
#include <memory>
#include <boost/foreach.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string.hpp>
#include <sstream>
#include <set>
#include <map>
#include "Uri.h"

#include <boost/multiprecision/cpp_int.hpp>
#include <boost/multiprecision/detail/bitscan.hpp>

static constexpr hdtn::Logger::SubProcess subprocess = hdtn::Logger::SubProcess::none;

//for non-throw versions of get_child which return a reference to the second parameter
static const boost::property_tree::ptree EMPTY_PTREE;

security_context_param_t::security_context_param_t() :
    m_paramName(BPSEC_SECURITY_CONTEXT_PARAM_NAME::UNDEFINED),
    m_valueUint(0),
    m_valuePath() {}
security_context_param_t::~security_context_param_t() {}
security_context_param_t::security_context_param_t(const security_context_param_t& o) :
    m_paramName(o.m_paramName),
    m_valueUint(o.m_valueUint),
    m_valuePath(o.m_valuePath) {}
//a move constructor: X(X&&)
security_context_param_t::security_context_param_t(security_context_param_t&& o) noexcept :
    m_paramName(o.m_paramName),
    m_valueUint(o.m_valueUint),
    m_valuePath(std::move(o.m_valuePath)) {}
//a copy assignment: operator=(const X&)
security_context_param_t& security_context_param_t::operator=(const security_context_param_t& o) {
    m_paramName = o.m_paramName;
    m_valueUint = o.m_valueUint;
    m_valuePath = o.m_valuePath;
    return *this;
}
//a move assignment: operator=(X&&)
security_context_param_t& security_context_param_t::operator=(security_context_param_t&& o) noexcept {
    m_paramName = o.m_paramName;
    m_valueUint = o.m_valueUint;
    m_valuePath = std::move(o.m_valuePath);
    return *this;
}
bool security_context_param_t::operator==(const security_context_param_t& o) const {
    return (m_paramName == o.m_paramName) &&
        (m_valueUint == o.m_valueUint) &&
        (m_valuePath == o.m_valuePath);
}

static constexpr uint8_t MAX_PARAM_NAMES = static_cast<uint8_t>(BPSEC_SECURITY_CONTEXT_PARAM_NAME::RESERVED_MAX_PARAM_NAMES);
static const BPSEC_SECURITY_CONTEXT_PARAM_TYPE paramToTypeLut[MAX_PARAM_NAMES] = {
    BPSEC_SECURITY_CONTEXT_PARAM_TYPE::UNDEFINED, //UNDEFINED = 0,
    BPSEC_SECURITY_CONTEXT_PARAM_TYPE::U64, //AES_VARIANT,
    BPSEC_SECURITY_CONTEXT_PARAM_TYPE::U64, //SHA_VARIANT,
    BPSEC_SECURITY_CONTEXT_PARAM_TYPE::U64, //IV_SIZE_BYTES,
    BPSEC_SECURITY_CONTEXT_PARAM_TYPE::U64, //SCOPE_FLAGS,
    BPSEC_SECURITY_CONTEXT_PARAM_TYPE::U64, //SECURITY_BLOCK_CRC,
    BPSEC_SECURITY_CONTEXT_PARAM_TYPE::PATH, //KEY_ENCRYPTION_KEY_FILE,
    BPSEC_SECURITY_CONTEXT_PARAM_TYPE::PATH, //KEY_FILE,
};
static const std::string paramToStringNameLut[MAX_PARAM_NAMES] = {
    "", //UNDEFINED = 0,
    "aesVariant", //AES_VARIANT,
    "shaVariant", //SHA_VARIANT,
    "ivSizeBytes", //IV_SIZE_BYTES,
    "scopeFlags", //SCOPE_FLAGS,
    "securityBlockCrc", //SECURITY_BLOCK_CRC,
    "keyEncryptionKeyFile", //KEY_ENCRYPTION_KEY_FILE,
    "keyFile", //KEY_FILE,
};
static const std::map<std::string, BPSEC_SECURITY_CONTEXT_PARAM_NAME> paramNameStrToEnum = {
    {paramToStringNameLut[static_cast<uint8_t>(BPSEC_SECURITY_CONTEXT_PARAM_NAME::AES_VARIANT)], BPSEC_SECURITY_CONTEXT_PARAM_NAME::AES_VARIANT},
    {paramToStringNameLut[static_cast<uint8_t>(BPSEC_SECURITY_CONTEXT_PARAM_NAME::SHA_VARIANT)], BPSEC_SECURITY_CONTEXT_PARAM_NAME::SHA_VARIANT},
    {paramToStringNameLut[static_cast<uint8_t>(BPSEC_SECURITY_CONTEXT_PARAM_NAME::IV_SIZE_BYTES)], BPSEC_SECURITY_CONTEXT_PARAM_NAME::IV_SIZE_BYTES},
    {paramToStringNameLut[static_cast<uint8_t>(BPSEC_SECURITY_CONTEXT_PARAM_NAME::SCOPE_FLAGS)], BPSEC_SECURITY_CONTEXT_PARAM_NAME::SCOPE_FLAGS},
    {paramToStringNameLut[static_cast<uint8_t>(BPSEC_SECURITY_CONTEXT_PARAM_NAME::SECURITY_BLOCK_CRC)], BPSEC_SECURITY_CONTEXT_PARAM_NAME::SECURITY_BLOCK_CRC},
    {paramToStringNameLut[static_cast<uint8_t>(BPSEC_SECURITY_CONTEXT_PARAM_NAME::KEY_ENCRYPTION_KEY_FILE)], BPSEC_SECURITY_CONTEXT_PARAM_NAME::KEY_ENCRYPTION_KEY_FILE},
    {paramToStringNameLut[static_cast<uint8_t>(BPSEC_SECURITY_CONTEXT_PARAM_NAME::KEY_FILE)], BPSEC_SECURITY_CONTEXT_PARAM_NAME::KEY_FILE}
};
bool security_context_param_t::SetValuesFromPropertyTree(const boost::property_tree::ptree& pt) {
    
    try {
        const std::string paramNameAsStr = pt.get<std::string>("paramName");
        std::map<std::string, BPSEC_SECURITY_CONTEXT_PARAM_NAME>::const_iterator it = paramNameStrToEnum.find(paramNameAsStr);
        if (it == paramNameStrToEnum.cend()) {
            LOG_ERROR(subprocess) << "error parsing security context params: unknown param name " << paramNameAsStr;
            return false;
        }
        m_paramName = it->second;
        const BPSEC_SECURITY_CONTEXT_PARAM_TYPE type = paramToTypeLut[static_cast<uint8_t>(m_paramName)];
        if (type == BPSEC_SECURITY_CONTEXT_PARAM_TYPE::U64) {
            m_valueUint = pt.get<uint64_t>("value");
        }
        else if (type == BPSEC_SECURITY_CONTEXT_PARAM_TYPE::PATH) {
            m_valuePath = pt.get<boost::filesystem::path>("value");
        }
        else {
            LOG_ERROR(subprocess) << "error parsing security context params: unknown param type for param name " << paramNameAsStr;
            return false;
        }
    }
    catch (const boost::property_tree::ptree_error& e) {
        LOG_ERROR(subprocess) << "parsing JSON security_context_param_t: " << e.what();
        return false;
    }
    return true;
}

boost::property_tree::ptree security_context_param_t::GetNewPropertyTree() const {
    boost::property_tree::ptree pt;
    pt.put("paramName", paramToStringNameLut[static_cast<uint8_t>(m_paramName)]);
    const BPSEC_SECURITY_CONTEXT_PARAM_TYPE type = paramToTypeLut[static_cast<uint8_t>(m_paramName)];
    if (type == BPSEC_SECURITY_CONTEXT_PARAM_TYPE::U64) {
        pt.put("value", m_valueUint);
    }
    else if (type == BPSEC_SECURITY_CONTEXT_PARAM_TYPE::PATH) {
        pt.put("value", m_valuePath.string()); //.string() prevents nested quotes in json file
    }
    else {
        pt.put("value", "");
    }
    return pt;
}


static constexpr uint8_t MAX_SECURITY_FAILURE_EVENTS = static_cast<uint8_t>(BPSEC_SECURITY_FAILURE_EVENT::RESERVED_MAX_EVENTS);
static const std::string failureEventToStringNameLut[MAX_SECURITY_FAILURE_EVENTS] = {
    "", //UNDEFINED = 0,
    "sopMisconfiguredAtVerifier", //SECURITY_OPERATION_MISCONFIGURED_AT_VERIFIER,
    "sopMissingAtVerifier", //SECURITY_OPERATION_MISSING_AT_VERIFIER,
    "sopCorruptedAtVerifier", //SECURITY_OPERATION_CORRUPTED_AT_VERIFIER,
    "sopMisconfiguredAtAcceptor", //SECURITY_OPERATION_MISCONFIGURED_AT_ACCEPTOR,
    "sopMissingAtAcceptor", //SECURITY_OPERATION_MISSING_AT_ACCEPTOR,
    "sopCorruptedAtAcceptor" //SECURITY_OPERATION_CORRUPTED_AT_ACCEPTOR
};
static const std::map<std::string, BPSEC_SECURITY_FAILURE_EVENT> failureEventStrToEnum = {
    {
        failureEventToStringNameLut[static_cast<uint8_t>(BPSEC_SECURITY_FAILURE_EVENT::SECURITY_OPERATION_MISCONFIGURED_AT_VERIFIER)],
        BPSEC_SECURITY_FAILURE_EVENT::SECURITY_OPERATION_MISCONFIGURED_AT_VERIFIER
    },
    {
        failureEventToStringNameLut[static_cast<uint8_t>(BPSEC_SECURITY_FAILURE_EVENT::SECURITY_OPERATION_MISSING_AT_VERIFIER)],
        BPSEC_SECURITY_FAILURE_EVENT::SECURITY_OPERATION_MISSING_AT_VERIFIER
    },
    {
        failureEventToStringNameLut[static_cast<uint8_t>(BPSEC_SECURITY_FAILURE_EVENT::SECURITY_OPERATION_CORRUPTED_AT_VERIFIER)],
        BPSEC_SECURITY_FAILURE_EVENT::SECURITY_OPERATION_CORRUPTED_AT_VERIFIER
    },
    {
        failureEventToStringNameLut[static_cast<uint8_t>(BPSEC_SECURITY_FAILURE_EVENT::SECURITY_OPERATION_MISCONFIGURED_AT_ACCEPTOR)],
        BPSEC_SECURITY_FAILURE_EVENT::SECURITY_OPERATION_MISCONFIGURED_AT_ACCEPTOR
    },
    {
        failureEventToStringNameLut[static_cast<uint8_t>(BPSEC_SECURITY_FAILURE_EVENT::SECURITY_OPERATION_MISSING_AT_ACCEPTOR)],
        BPSEC_SECURITY_FAILURE_EVENT::SECURITY_OPERATION_MISSING_AT_ACCEPTOR
    },
    {
        failureEventToStringNameLut[static_cast<uint8_t>(BPSEC_SECURITY_FAILURE_EVENT::SECURITY_OPERATION_CORRUPTED_AT_ACCEPTOR)],
        BPSEC_SECURITY_FAILURE_EVENT::SECURITY_OPERATION_CORRUPTED_AT_ACCEPTOR
    }
};

const unsigned int security_operation_event_plus_actions_pair_t::ActionMaskToBitPosition(const BPSEC_SECURITY_FAILURE_PROCESSING_ACTION_MASKS actionMask) {
    return boost::multiprecision::detail::find_lsb<uint16_t>(static_cast<uint16_t>(actionMask));
}
static constexpr uint8_t MAX_ACTION_MASKS = static_cast<uint8_t>(BPSEC_SECURITY_FAILURE_PROCESSING_ACTION_MASKS::RESERVED_NUM_MASKS);
static const std::string failureActionBitPosToStringNameLut[MAX_ACTION_MASKS] = {
    "removeSecurityOperation", //REMOVE_SECURITY_OPERATION = 0,
    "removeSecurityOperationTargetBlock", //REMOVE_SECURITY_OPERATION_TARGET_BLOCK,
    "removeAllSecurityTargetOperations", //REMOVE_ALL_SECURITY_TARGET_OPERATIONS,
    "failBundleForwarding", //FAIL_BUNDLE_FORWARDING,
    "requestBundleStorage", //REQUEST_BUNDLE_STORAGE,
    "reportReasonCode", //REPORT_REASON_CODE,
    "overrideSecurityTargetBlockBpcf", //OVERRIDE_SECURITY_TARGET_BLOCK_BPCF
    "overrideSecurityBlockBpcf" //OVERRIDE_SECURITY_BLOCK_BPCF
};
static const std::map<std::string, BPSEC_SECURITY_FAILURE_PROCESSING_ACTION_MASKS> failureActionStrToMask = {
    {
        failureActionBitPosToStringNameLut[security_operation_event_plus_actions_pair_t::ActionMaskToBitPosition(
            BPSEC_SECURITY_FAILURE_PROCESSING_ACTION_MASKS::REMOVE_SECURITY_OPERATION)],
        BPSEC_SECURITY_FAILURE_PROCESSING_ACTION_MASKS::REMOVE_SECURITY_OPERATION
    },
    {
        failureActionBitPosToStringNameLut[security_operation_event_plus_actions_pair_t::ActionMaskToBitPosition(
            BPSEC_SECURITY_FAILURE_PROCESSING_ACTION_MASKS::REMOVE_SECURITY_OPERATION_TARGET_BLOCK)],
        BPSEC_SECURITY_FAILURE_PROCESSING_ACTION_MASKS::REMOVE_SECURITY_OPERATION_TARGET_BLOCK
    },
    {
        failureActionBitPosToStringNameLut[security_operation_event_plus_actions_pair_t::ActionMaskToBitPosition(
            BPSEC_SECURITY_FAILURE_PROCESSING_ACTION_MASKS::REMOVE_ALL_SECURITY_TARGET_OPERATIONS)],
        BPSEC_SECURITY_FAILURE_PROCESSING_ACTION_MASKS::REMOVE_ALL_SECURITY_TARGET_OPERATIONS
    },
    {
        failureActionBitPosToStringNameLut[security_operation_event_plus_actions_pair_t::ActionMaskToBitPosition(
            BPSEC_SECURITY_FAILURE_PROCESSING_ACTION_MASKS::FAIL_BUNDLE_FORWARDING)],
        BPSEC_SECURITY_FAILURE_PROCESSING_ACTION_MASKS::FAIL_BUNDLE_FORWARDING
    },
    {
        failureActionBitPosToStringNameLut[security_operation_event_plus_actions_pair_t::ActionMaskToBitPosition(
            BPSEC_SECURITY_FAILURE_PROCESSING_ACTION_MASKS::REQUEST_BUNDLE_STORAGE)],
        BPSEC_SECURITY_FAILURE_PROCESSING_ACTION_MASKS::REQUEST_BUNDLE_STORAGE
    },
    {
        failureActionBitPosToStringNameLut[security_operation_event_plus_actions_pair_t::ActionMaskToBitPosition(
            BPSEC_SECURITY_FAILURE_PROCESSING_ACTION_MASKS::REPORT_REASON_CODE)],
        BPSEC_SECURITY_FAILURE_PROCESSING_ACTION_MASKS::REPORT_REASON_CODE
    },
    {
        failureActionBitPosToStringNameLut[security_operation_event_plus_actions_pair_t::ActionMaskToBitPosition(
            BPSEC_SECURITY_FAILURE_PROCESSING_ACTION_MASKS::OVERRIDE_SECURITY_TARGET_BLOCK_BPCF)],
        BPSEC_SECURITY_FAILURE_PROCESSING_ACTION_MASKS::OVERRIDE_SECURITY_TARGET_BLOCK_BPCF
    },
    {
        failureActionBitPosToStringNameLut[security_operation_event_plus_actions_pair_t::ActionMaskToBitPosition(
            BPSEC_SECURITY_FAILURE_PROCESSING_ACTION_MASKS::OVERRIDE_SECURITY_BLOCK_BPCF)],
        BPSEC_SECURITY_FAILURE_PROCESSING_ACTION_MASKS::OVERRIDE_SECURITY_BLOCK_BPCF
    }
};

security_operation_event_plus_actions_pair_t::security_operation_event_plus_actions_pair_t() :
    m_event(BPSEC_SECURITY_FAILURE_EVENT::UNDEFINED),
    m_actionMasks(BPSEC_SECURITY_FAILURE_PROCESSING_ACTION_MASKS::NO_ACTIONS_SET) {}
bool security_operation_event_plus_actions_pair_t::operator==(const security_operation_event_plus_actions_pair_t& o) const {
    return (m_event == o.m_event) && (m_actionMasks == o.m_actionMasks);
}

bool security_operation_event_plus_actions_pair_t::SetValuesFromPropertyTree(const boost::property_tree::ptree& pt) {

    try {
        const std::string failureEventAsStr = pt.get<std::string>("eventId");
        std::map<std::string, BPSEC_SECURITY_FAILURE_EVENT>::const_iterator itEvent = failureEventStrToEnum.find(failureEventAsStr);
        if (itEvent == failureEventStrToEnum.cend()) {
            LOG_ERROR(subprocess) << "error parsing security operation event: unknown eventId " << failureEventAsStr;
            return false;
        }
        m_event = itEvent->second;

        const boost::property_tree::ptree& actionsPt = pt.get_child("actions", EMPTY_PTREE); //non-throw version
        m_actionMasks = BPSEC_SECURITY_FAILURE_PROCESSING_ACTION_MASKS::NO_ACTIONS_SET;
        BOOST_FOREACH(const boost::property_tree::ptree::value_type & actionValuePt, actionsPt) {
            const std::string actionAsStr = actionValuePt.second.get_value<std::string>();
            std::map<std::string, BPSEC_SECURITY_FAILURE_PROCESSING_ACTION_MASKS>::const_iterator itAction = failureActionStrToMask.find(actionAsStr);
            if (itAction == failureActionStrToMask.cend()) {
                LOG_ERROR(subprocess) << "error parsing security operation event: unknown action " << actionAsStr;
                return false;
            }
            const BPSEC_SECURITY_FAILURE_PROCESSING_ACTION_MASKS thisActionMask = itAction->second;
            if ((m_actionMasks & thisActionMask) != BPSEC_SECURITY_FAILURE_PROCESSING_ACTION_MASKS::NO_ACTIONS_SET) { //already set
                LOG_ERROR(subprocess) << "error parsing JSON policy rules: duplicate action " << actionAsStr;
                return false;
            }
            m_actionMasks |= thisActionMask;
        }

    }
    catch (const boost::property_tree::ptree_error& e) {
        LOG_ERROR(subprocess) << "parsing JSON security_context_param_t: " << e.what();
        return false;
    }
    return true;
}

boost::property_tree::ptree security_operation_event_plus_actions_pair_t::GetNewPropertyTree() const {
    boost::property_tree::ptree pt;
    pt.put("eventId", failureEventToStringNameLut[static_cast<uint8_t>(m_event)]);
    const bool hasActions = (m_actionMasks != BPSEC_SECURITY_FAILURE_PROCESSING_ACTION_MASKS::NO_ACTIONS_SET);
    boost::property_tree::ptree& actionsVecPt = pt.put_child("actions",
        hasActions ? boost::property_tree::ptree() : boost::property_tree::ptree("[]"));
    if (hasActions) {
        for (uint16_t i = 0; i < MAX_ACTION_MASKS; ++i) {
            const BPSEC_SECURITY_FAILURE_PROCESSING_ACTION_MASKS mask = static_cast<BPSEC_SECURITY_FAILURE_PROCESSING_ACTION_MASKS>(1u << i);
            if ((m_actionMasks & mask) != BPSEC_SECURITY_FAILURE_PROCESSING_ACTION_MASKS::NO_ACTIONS_SET) { //already set
                actionsVecPt.push_back(std::make_pair("", boost::property_tree::ptree(failureActionBitPosToStringNameLut[i]))); //using "" as key creates json array
            }
        }
    }
    return pt;
}


//////
policy_rules_t::policy_rules_t() :
    m_description(""),
    m_securityPolicyRuleId(0),
    m_securityRole(""),
    m_securitySource(""),
    m_bundleSource(),
    m_bundleFinalDestination(),
    m_securityTargetBlockTypes(),
    m_securityService(""),
    m_securityContext(""),
    m_securityFailureEventSetReferenceName(""),
    m_securityFailureEventSetReferencePtr(NULL),
    m_securityContextParamsVec() {}

policy_rules_t::~policy_rules_t() {}

//a copy constructor: X(const X&)
policy_rules_t::policy_rules_t(const policy_rules_t& o) :
    m_description(o.m_description),
    m_securityPolicyRuleId(o.m_securityPolicyRuleId),
    m_securityRole(o.m_securityRole),
    m_securitySource(o.m_securitySource),
    m_bundleSource(o.m_bundleSource),
    m_bundleFinalDestination(o.m_bundleFinalDestination),
    m_securityTargetBlockTypes(o.m_securityTargetBlockTypes),
    m_securityService(o.m_securityService),
    m_securityContext(o.m_securityContext),
    m_securityFailureEventSetReferenceName(o.m_securityFailureEventSetReferenceName),
    m_securityFailureEventSetReferencePtr(o.m_securityFailureEventSetReferencePtr),
    m_securityContextParamsVec(o.m_securityContextParamsVec)
{ }

//a move constructor: X(X&&)
policy_rules_t::policy_rules_t(policy_rules_t&& o) noexcept :
    m_description(std::move(o.m_description)),
    m_securityPolicyRuleId(o.m_securityPolicyRuleId),
    m_securityRole(std::move(o.m_securityRole)),
    m_securitySource(std::move(o.m_securitySource)),
    m_bundleSource(std::move(o.m_bundleSource)),
    m_bundleFinalDestination(std::move(o.m_bundleFinalDestination)),
    m_securityTargetBlockTypes(std::move(o.m_securityTargetBlockTypes)),
    m_securityService(std::move(o.m_securityService)),
    m_securityContext(std::move(o.m_securityContext)),
    m_securityFailureEventSetReferenceName(std::move(o.m_securityFailureEventSetReferenceName)),
    m_securityFailureEventSetReferencePtr(o.m_securityFailureEventSetReferencePtr),
    m_securityContextParamsVec(std::move(o.m_securityContextParamsVec))
{ }

//a copy assignment: operator=(const X&)
policy_rules_t& policy_rules_t::operator=(const policy_rules_t& o) {
    m_description = o.m_description;
    m_securityPolicyRuleId = o.m_securityPolicyRuleId;
    m_securityRole = o.m_securityRole;
    m_securitySource = o.m_securitySource;
    m_bundleSource = o.m_bundleSource;
    m_bundleFinalDestination = o.m_bundleFinalDestination;
    m_securityTargetBlockTypes = o.m_securityTargetBlockTypes;
    m_securityService = o.m_securityService;
    m_securityContext = o.m_securityContext;
    m_securityFailureEventSetReferenceName = o.m_securityFailureEventSetReferenceName;
    m_securityFailureEventSetReferencePtr = o.m_securityFailureEventSetReferencePtr;
    m_securityContextParamsVec = o.m_securityContextParamsVec;
    return *this;
}

//a move assignment: operator=(X&&)
policy_rules_t& policy_rules_t::operator=(policy_rules_t&& o) noexcept {
    m_description = std::move(o.m_description);
    m_securityPolicyRuleId = o.m_securityPolicyRuleId;
    m_securityRole = std::move(o.m_securityRole);
    m_securitySource = std::move(o.m_securitySource);
    m_bundleSource = std::move(o.m_bundleSource);
    m_bundleFinalDestination = std::move(o.m_bundleFinalDestination);
    m_securityTargetBlockTypes = std::move(o.m_securityTargetBlockTypes);
    m_securityService = std::move(o.m_securityService);
    m_securityContext = std::move(o.m_securityContext);
    m_securityFailureEventSetReferenceName = std::move(o.m_securityFailureEventSetReferenceName);
    m_securityFailureEventSetReferencePtr = o.m_securityFailureEventSetReferencePtr;
    m_securityContextParamsVec = std::move(o.m_securityContextParamsVec);
    return *this;
}

bool policy_rules_t::operator==(const policy_rules_t& o) const {
    return (m_description == o.m_description) &&
        (m_securityPolicyRuleId == o.m_securityPolicyRuleId) &&
        (m_securityRole == o.m_securityRole) &&
        (m_securitySource == o.m_securitySource) &&
        (m_bundleSource == o.m_bundleSource) &&
        (m_bundleFinalDestination == o.m_bundleFinalDestination) &&
        (m_securityTargetBlockTypes == o.m_securityTargetBlockTypes) &&
        (m_securityService == o.m_securityService) &&
        (m_securityContext == o.m_securityContext) &&
        (m_securityFailureEventSetReferenceName == o.m_securityFailureEventSetReferenceName) &&
        //don't check m_securityFailureEventSetReferencePtr
        (m_securityContextParamsVec == o.m_securityContextParamsVec);
}

static bool IsValidUri(const std::string& uri) {
    uint64_t eidNodeNumber;
    uint64_t eidServiceNumber;
    bool serviceNumberIsWildCard;
    return (uri == "ipn:*.*") || Uri::ParseIpnUriString(uri, eidNodeNumber, eidServiceNumber, &serviceNumberIsWildCard);
}

bool policy_rules_t::SetValuesFromPropertyTree(const boost::property_tree::ptree& pt) {

    try {
        m_description = pt.get<std::string>("description");
        m_securityPolicyRuleId = pt.get<uint64_t>("securityPolicyRuleId");
        m_securityRole = pt.get<std::string>("securityRole");
        if ((m_securityRole != "source") && (m_securityRole != "verifier") && (m_securityRole != "acceptor")) {
            LOG_ERROR(subprocess) << "error parsing JSON policy rules: security role ("
                << m_securityRole << ") is not any of the following: [source, verifier, acceptor].";
            return false;
        }
        m_securitySource = pt.get<std::string>("securitySource");
        if (!IsValidUri(m_securitySource)) {
            LOG_ERROR(subprocess) << "error parsing JSON policy rules: invalid Security Source uri " << m_securitySource;
            return false;
        }
        const boost::property_tree::ptree& bundleSourcePt = pt.get_child("bundleSource", EMPTY_PTREE); //non-throw version
        m_bundleSource.clear();
        BOOST_FOREACH(const boost::property_tree::ptree::value_type & bundleSourceValuePt, bundleSourcePt) {
            const std::string uri = bundleSourceValuePt.second.get_value<std::string>();
            if (!IsValidUri(uri)) {
                LOG_ERROR(subprocess) << "error parsing JSON policy rules: invalid bundle Source uri " << uri;
                return false;
            }
            else if (m_bundleSource.insert(uri).second == false) { //not inserted
                LOG_ERROR(subprocess) << "error parsing JSON policy rules: duplicate bundle Source " << uri;
                return false;
            }
        }
        const boost::property_tree::ptree& bundleFinalDestPt = pt.get_child("bundleFinalDestination", EMPTY_PTREE); //non-throw version
        m_bundleFinalDestination.clear();
        BOOST_FOREACH(const boost::property_tree::ptree::value_type & bundleFinalDestValuePt, bundleFinalDestPt) {
            const std::string uri = bundleFinalDestValuePt.second.get_value<std::string>();
            if (!IsValidUri(uri)) {
                LOG_ERROR(subprocess) << "error parsing JSON policy rules: invalid bundle Final Destination uri " << uri;
                return false;
            }
            else if (m_bundleFinalDestination.insert(uri).second == false) { //not inserted
                LOG_ERROR(subprocess) << "error parsing JSON policy rules: duplicate bundle Final Destination " << uri;
                return false;
            }
        }

        if (m_securityRole == "source") {
            const boost::property_tree::ptree& securityTargetBlockTypesPt = pt.get_child("securityTargetBlockTypes", EMPTY_PTREE); //non-throw version
            m_securityTargetBlockTypes.clear();
            BOOST_FOREACH(const boost::property_tree::ptree::value_type & securityTargetBlockTypesValuePt, securityTargetBlockTypesPt) {
                const uint64_t securityTargetBlockTypeU64 = securityTargetBlockTypesValuePt.second.get_value<uint64_t>();
                if (m_securityTargetBlockTypes.insert(securityTargetBlockTypeU64).second == false) { //not inserted
                    LOG_ERROR(subprocess) << "error parsing JSON policy rules: duplicate securityTargetBlockType " << securityTargetBlockTypeU64;
                    return false;
                }
            }
        }

        m_securityService = pt.get<std::string>("securityService");
        m_securityContext = pt.get<std::string>("securityContext");
        if (m_securityService == "confidentiality") {
            if (m_securityContext != "aesGcm") {
                LOG_ERROR(subprocess) << "error parsing JSON policy rules: securityContext ("
                    << m_securityContext << ") must be aesGcm when securityService=confidentiality";
                return false;
            }
        }
        else if (m_securityService == "integrity") {
            if (m_securityContext != "hmacSha") {
                LOG_ERROR(subprocess) << "error parsing JSON policy rules: securityContext ("
                    << m_securityContext << ") must be hmacSha when securityService=integrity";
                return false;
            }
        }
        else {
            LOG_ERROR(subprocess) << "error parsing JSON policy rules: securityService ("
                << m_securityService << ") must be confidentiality or integrity";
            return false;
        }
        

        m_securityFailureEventSetReferenceName = pt.get<std::string>("securityFailureEventSetReference");
        const boost::property_tree::ptree& securityContextParamsVectorPt = pt.get_child("securityContextParams", EMPTY_PTREE); //non-throw version
        m_securityContextParamsVec.resize(securityContextParamsVectorPt.size());
        unsigned int securityContextParamsVectorIndex = 0;
        BOOST_FOREACH(const boost::property_tree::ptree::value_type & securityContextParamsConfigPt, securityContextParamsVectorPt) {
            security_context_param_t& securityContextParam = m_securityContextParamsVec[securityContextParamsVectorIndex++];
            if (!securityContextParam.SetValuesFromPropertyTree(securityContextParamsConfigPt.second)) {
                return false;
            }
            static const std::set<BPSEC_SECURITY_CONTEXT_PARAM_NAME> allowedVerifierAcceptorParamNames = {
                BPSEC_SECURITY_CONTEXT_PARAM_NAME::KEY_ENCRYPTION_KEY_FILE,
                BPSEC_SECURITY_CONTEXT_PARAM_NAME::KEY_FILE 
            };
            if ((m_securityRole != "source") && (allowedVerifierAcceptorParamNames.count(securityContextParam.m_paramName) == 0)) {
                LOG_ERROR(subprocess) << "error parsing JSON policy rules: acceptors and verifiers are only allowed to have file paths to keys in the securityContextParams but "
                    << paramToStringNameLut[static_cast<uint64_t>(securityContextParam.m_paramName)] << " param name was found";
                return false;
            }
        }
    }
    catch (const boost::property_tree::ptree_error& e) {
        LOG_ERROR(subprocess) << "parsing JSON policy_rules_t: " << e.what();
        return false;
    }
    return true;
}

boost::property_tree::ptree policy_rules_t::GetNewPropertyTree() const {
    boost::property_tree::ptree pt;
    pt.put("description", m_description);
    pt.put("securityPolicyRuleId", m_securityPolicyRuleId);
    pt.put("securityRole", m_securityRole);
    pt.put("securitySource", m_securitySource);
    boost::property_tree::ptree& bundleSourcePt = pt.put_child("bundleSource",
        m_bundleSource.empty() ? boost::property_tree::ptree("[]") : boost::property_tree::ptree());
    for (std::set<std::string>::const_iterator bundleSourceIt = m_bundleSource.cbegin();
        bundleSourceIt != m_bundleSource.cend(); ++bundleSourceIt)
    {
        bundleSourcePt.push_back(std::make_pair("", boost::property_tree::ptree(*bundleSourceIt))); //using "" as key creates json array
    }

    boost::property_tree::ptree& bundleFinalDestPt = pt.put_child("bundleFinalDestination",
        m_bundleFinalDestination.empty() ? boost::property_tree::ptree("[]") : boost::property_tree::ptree());
    for (std::set<std::string>::const_iterator bundleFinalDestIt = m_bundleFinalDestination.cbegin();
        bundleFinalDestIt != m_bundleFinalDestination.cend(); ++bundleFinalDestIt)
    {
        bundleFinalDestPt.push_back(std::make_pair("", boost::property_tree::ptree(*bundleFinalDestIt))); //using "" as key creates json array
    }

    if (m_securityRole == "source") {
        boost::property_tree::ptree& securityTargetBlockTypesPt = pt.put_child("securityTargetBlockTypes",
            m_securityTargetBlockTypes.empty() ? boost::property_tree::ptree("[]") : boost::property_tree::ptree());
        for (std::set<uint64_t>::const_iterator securityTargetBlockTypesIt = m_securityTargetBlockTypes.cbegin();
            securityTargetBlockTypesIt != m_securityTargetBlockTypes.cend(); ++securityTargetBlockTypesIt)
        {
            securityTargetBlockTypesPt.push_back(std::make_pair("", boost::property_tree::ptree(boost::lexical_cast<std::string>(*securityTargetBlockTypesIt))));
        }
    }

    pt.put("securityService", m_securityService);
    pt.put("securityContext", m_securityContext);
    pt.put("securityFailureEventSetReference", m_securityFailureEventSetReferenceName);
    boost::property_tree::ptree& securityContextParamsVectorPt = pt.put_child("securityContextParams", m_securityContextParamsVec.empty() ? boost::property_tree::ptree("[]") : boost::property_tree::ptree());

    for (security_context_params_vector_t::const_iterator securityContextParamsVectorIt = m_securityContextParamsVec.cbegin();
        securityContextParamsVectorIt != m_securityContextParamsVec.cend(); ++securityContextParamsVectorIt)
    {
        const security_context_param_t& securityContextParam = *securityContextParamsVectorIt;
        securityContextParamsVectorPt.push_back(std::make_pair("", securityContextParam.GetNewPropertyTree())); //using "" as key creates json array
    }
    
    return pt;
}

security_failure_event_sets_t::security_failure_event_sets_t() :
    m_name(""),
    m_description(""),
    m_securityOperationEventsVec() {}

security_failure_event_sets_t::~security_failure_event_sets_t() {}

//a copy constructor: X(const X&)
security_failure_event_sets_t::security_failure_event_sets_t(const security_failure_event_sets_t& o) :
    m_name(o.m_name),
    m_description(o.m_description),
    m_securityOperationEventsVec(o.m_securityOperationEventsVec)
{ }

//a move constructor: X(X&&)
security_failure_event_sets_t::security_failure_event_sets_t(security_failure_event_sets_t&& o) noexcept :
    m_name(std::move(o.m_name)),
    m_description(std::move(o.m_description)),
    m_securityOperationEventsVec(std::move(o.m_securityOperationEventsVec))
{ }

//a copy assignment: operator=(const X&)
security_failure_event_sets_t& security_failure_event_sets_t::operator=(const security_failure_event_sets_t& o) {
    m_name = o.m_name;
    m_description = o.m_description;
    m_securityOperationEventsVec = o.m_securityOperationEventsVec;
    return *this;
}

//a move assignment: operator=(X&&)
security_failure_event_sets_t& security_failure_event_sets_t::operator=(security_failure_event_sets_t&& o) noexcept {
    m_name = std::move(o.m_name);
    m_description = std::move(o.m_description);
    m_securityOperationEventsVec = std::move(o.m_securityOperationEventsVec);
    return *this;
}

bool security_failure_event_sets_t::operator==(const security_failure_event_sets_t& o) const {
    return (m_name == o.m_name) &&
        (m_description == o.m_description) &&
        (m_securityOperationEventsVec == o.m_securityOperationEventsVec);
}
bool security_failure_event_sets_t::operator<(const security_failure_event_sets_t& o) const {
    return (m_name < o.m_name);
}
bool security_failure_event_sets_t::SetValuesFromPropertyTree(const boost::property_tree::ptree& pt) {

    try {
        m_name = pt.get<std::string>("name");
        m_description = pt.get<std::string>("description");
        
        const boost::property_tree::ptree& securityOperationEventsVectorPt = pt.get_child("securityOperationEvents", EMPTY_PTREE); //non-throw version
        m_securityOperationEventsVec.resize(securityOperationEventsVectorPt.size());
        unsigned int m_securityOperationEventsVectorIndex = 0;
        BOOST_FOREACH(const boost::property_tree::ptree::value_type & securityOperationEventsPt, securityOperationEventsVectorPt) {
            security_operation_event_plus_actions_pair_t& sopEventActionPair = m_securityOperationEventsVec[m_securityOperationEventsVectorIndex++];
            if (!sopEventActionPair.SetValuesFromPropertyTree(securityOperationEventsPt.second)) {
                LOG_ERROR(subprocess) << "error parsing JSON securityOperationEvents[" << (m_securityOperationEventsVectorIndex - 1) << "]";
                return false;
            }
        }
    }
    catch (const boost::property_tree::ptree_error& e) {
        LOG_ERROR(subprocess) << "parsing JSON security_context_param_t: " << e.what();
        return false;
    }
    return true;
}

boost::property_tree::ptree security_failure_event_sets_t::GetNewPropertyTree() const {
    boost::property_tree::ptree pt;
    pt.put("name", m_name);
    pt.put("description", m_description);
    boost::property_tree::ptree& sopEventsPt = pt.put_child("securityOperationEvents",
        m_securityOperationEventsVec.empty() ? boost::property_tree::ptree("[]") : boost::property_tree::ptree());
    for (security_operation_event_plus_actions_pairs_vec_t::const_iterator it = m_securityOperationEventsVec.cbegin();
        it != m_securityOperationEventsVec.cend(); ++it)
    {
        sopEventsPt.push_back(std::make_pair("", it->GetNewPropertyTree())); //using "" as key creates json array
    }
    return pt;
}

BpSecConfig::BpSecConfig() :
    m_bpsecConfigName("unnamed BpSec config"),
    m_policyRulesVector(),
    m_securityFailureEventSetsSet()
{}

BpSecConfig::~BpSecConfig() {
}

//a copy constructor: X(const X&)
BpSecConfig::BpSecConfig(const BpSecConfig& o) :
    m_bpsecConfigName(o.m_bpsecConfigName),
    m_policyRulesVector(o.m_policyRulesVector),
    m_securityFailureEventSetsSet(o.m_securityFailureEventSetsSet) {}
//a move constructor: X(X&&)
BpSecConfig::BpSecConfig(BpSecConfig&& o) noexcept :
    m_bpsecConfigName(std::move(o.m_bpsecConfigName)),
    m_policyRulesVector(std::move(o.m_policyRulesVector)),
    m_securityFailureEventSetsSet(std::move(o.m_securityFailureEventSetsSet)) {}

//a copy assignment: operator=(const X&)
BpSecConfig& BpSecConfig::operator=(const BpSecConfig& o) {
    m_bpsecConfigName = o.m_bpsecConfigName;
    m_policyRulesVector = o.m_policyRulesVector;
    m_securityFailureEventSetsSet = o.m_securityFailureEventSetsSet;
    return *this;
}

//a move assignment: operator=(X&&)
BpSecConfig& BpSecConfig::operator=(BpSecConfig&& o) noexcept {
    m_bpsecConfigName = std::move(o.m_bpsecConfigName);
    m_policyRulesVector = std::move(o.m_policyRulesVector);
    m_securityFailureEventSetsSet = std::move(o.m_securityFailureEventSetsSet);
    return *this;
}

bool BpSecConfig::operator==(const BpSecConfig& o) const {
    return (m_bpsecConfigName == o.m_bpsecConfigName) &&
        (m_policyRulesVector == o.m_policyRulesVector) &&
        (m_securityFailureEventSetsSet == o.m_securityFailureEventSetsSet);
}

bool BpSecConfig::SetValuesFromPropertyTree(const boost::property_tree::ptree& pt) {
    try {
        m_bpsecConfigName = pt.get<std::string>("bpsecConfigName");
        if (m_bpsecConfigName == "") {
            LOG_ERROR(subprocess) << "parsing JSON BpSec config: bpsecConfigName must be defined and not empty string";
            return false;
        }
    }
    catch (const boost::property_tree::ptree_error& e) {
        LOG_ERROR(subprocess) << "parsing JSON HDTN config: " << e.what();
        return false;
    }

    const boost::property_tree::ptree& policyRulesVectorPt = pt.get_child("policyRules", EMPTY_PTREE); //non-throw version
    m_policyRulesVector.resize(policyRulesVectorPt.size());
    unsigned int policyRulesVectorIndex = 0;
    BOOST_FOREACH(const boost::property_tree::ptree::value_type & policyRulesConfigPt, policyRulesVectorPt) {
        policy_rules_t& policyRulesConfig = m_policyRulesVector[policyRulesVectorIndex++];
        if (!policyRulesConfig.SetValuesFromPropertyTree(policyRulesConfigPt.second)) {
            LOG_ERROR(subprocess) << "error parsing JSON policyRules[" << (policyRulesVectorIndex - 1) << "]";
            return false;
        }
    }

    const boost::property_tree::ptree& eventSetsSetPt = pt.get_child("securityFailureEventSets", EMPTY_PTREE); //non-throw version
    m_securityFailureEventSetsSet.clear();
    unsigned int eventSetsVectorIndex = 0;
    BOOST_FOREACH(const boost::property_tree::ptree::value_type & eventSetsPt, eventSetsSetPt) {
        security_failure_event_sets_t eventSets;
        if (!eventSets.SetValuesFromPropertyTree(eventSetsPt.second)) {
            LOG_ERROR(subprocess) << "error parsing JSON securityFailureEventSets[" << eventSetsVectorIndex << "]";
            return false;
        }
        std::pair<security_failure_event_sets_set_t::iterator, bool> ret = m_securityFailureEventSetsSet.emplace(std::move(eventSets));
        if (ret.second == false) {
            LOG_ERROR(subprocess) << "error parsing JSON securityFailureEventSets[" << eventSetsVectorIndex
                << "]: name (" << ret.first->m_name << ") already exists";
            return false;
        }
        ++eventSetsVectorIndex;
    }

    security_failure_event_sets_t tmpSop;
    for (std::size_t i = 0; i < m_policyRulesVector.size(); ++i) {
        policy_rules_t& rule = m_policyRulesVector[i];
        tmpSop.m_name = rule.m_securityFailureEventSetReferenceName;
        security_failure_event_sets_set_t::const_iterator it = m_securityFailureEventSetsSet.find(tmpSop);
        if (it == m_securityFailureEventSetsSet.cend()) {
            LOG_ERROR(subprocess) << "error parsing JSON policyRules[" 
                << i << "]: securityFailureEventSetReference (" << rule.m_securityFailureEventSetReferenceName
                << ") does not match a name in the securityFailureEventSets";
            return false;
        }
        rule.m_securityFailureEventSetReferencePtr = &(*it);
    }

    return true;
}

BpSecConfig_ptr BpSecConfig::CreateFromJson(const std::string& jsonString, bool verifyNoUnusedJsonKeys) {
    boost::property_tree::ptree pt;
    BpSecConfig_ptr config; //NULL
    if (GetPropertyTreeFromJsonString(jsonString, pt)) { //prints message if failed
        config = CreateFromPtree(pt);
        //verify that there are no unused variables within the original json
        if (config && verifyNoUnusedJsonKeys) {
            std::string returnedErrorMessage;
            if (JsonSerializable::HasUnusedJsonVariablesInString(*config, jsonString, returnedErrorMessage)) {
                LOG_ERROR(subprocess) << returnedErrorMessage;
                config.reset(); //NULL
            }
        }
    }

    return config;
}

BpSecConfig_ptr BpSecConfig::CreateFromJsonFilePath(const boost::filesystem::path& jsonFilePath, bool verifyNoUnusedJsonKeys) {
    boost::property_tree::ptree pt;
    BpSecConfig_ptr config; //NULL
    if (GetPropertyTreeFromJsonFilePath(jsonFilePath, pt)) { //prints message if failed
        config = CreateFromPtree(pt);
        //verify that there are no unused variables within the original json
        if (config && verifyNoUnusedJsonKeys) {
            std::string returnedErrorMessage;
            if (JsonSerializable::HasUnusedJsonVariablesInFilePath(*config, jsonFilePath, returnedErrorMessage)) {
                LOG_ERROR(subprocess) << returnedErrorMessage;
                config.reset(); //NULL
            }
        }
    }
    return config;
}

BpSecConfig_ptr BpSecConfig::CreateFromPtree(const boost::property_tree::ptree& pt) {

    BpSecConfig_ptr ptrBpSecConfig = std::make_shared<BpSecConfig>();
    if (!ptrBpSecConfig->SetValuesFromPropertyTree(pt)) {
        ptrBpSecConfig = BpSecConfig_ptr(); //failed, so delete and set it NULL
    }
    return ptrBpSecConfig;
}

boost::property_tree::ptree BpSecConfig::GetNewPropertyTree() const {
    boost::property_tree::ptree pt;
    pt.put("bpsecConfigName", m_bpsecConfigName);

    boost::property_tree::ptree& policyRulesVectorPt = pt.put_child("policyRules",
        m_policyRulesVector.empty() ? boost::property_tree::ptree("[]") : boost::property_tree::ptree());
    for (policy_rules_vector_t::const_iterator policyRulesVectorIt = m_policyRulesVector.cbegin();
        policyRulesVectorIt != m_policyRulesVector.cend(); ++policyRulesVectorIt)
    {
        const policy_rules_t& policyRulesConfig = *policyRulesVectorIt;
        policyRulesVectorPt.push_back(std::make_pair("", policyRulesConfig.GetNewPropertyTree())); //using "" as key creates json array
    }

    boost::property_tree::ptree& securityFailureEventSetsVectorPt = pt.put_child("securityFailureEventSets",
        m_securityFailureEventSetsSet.empty() ? boost::property_tree::ptree("[]") : boost::property_tree::ptree());

    for (security_failure_event_sets_set_t::const_iterator eventSetsSetIt = m_securityFailureEventSetsSet.cbegin();
        eventSetsSetIt != m_securityFailureEventSetsSet.cend(); ++eventSetsSetIt)
    {
        const security_failure_event_sets_t& eventSets = *eventSetsSetIt;
        securityFailureEventSetsVectorPt.push_back(std::make_pair("", eventSets.GetNewPropertyTree())); //using "" as key creates json array
    }

    return pt;
}
