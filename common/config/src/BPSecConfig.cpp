/**
 * @file BPSecConfig.cpp
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

#include "BPSecConfig.h"
#include "Logger.h"
#include <memory>
#include <boost/foreach.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string.hpp>
#include <sstream>
#include <set>
#include <map>
#include "Uri.h"

static constexpr hdtn::Logger::SubProcess subprocess = hdtn::Logger::SubProcess::none;

//for non-throw versions of get_child which return a reference to the second parameter
static const boost::property_tree::ptree EMPTY_PTREE;

security_context_params_config_t::security_context_params_config_t() :
    m_paramName(BPSEC_SECURITY_CONTEXT_PARAM_NAME::UNDEFINED),
    m_valueUint(0),
    m_valuePath() {}
security_context_params_config_t::~security_context_params_config_t() {}
security_context_params_config_t::security_context_params_config_t(const security_context_params_config_t& o) :
    m_paramName(o.m_paramName),
    m_valueUint(o.m_valueUint),
    m_valuePath(o.m_valuePath) {}
//a move constructor: X(X&&)
security_context_params_config_t::security_context_params_config_t(security_context_params_config_t&& o) noexcept :
    m_paramName(o.m_paramName),
    m_valueUint(o.m_valueUint),
    m_valuePath(std::move(o.m_valuePath)) {}
//a copy assignment: operator=(const X&)
security_context_params_config_t& security_context_params_config_t::operator=(const security_context_params_config_t& o) {
    m_paramName = o.m_paramName;
    m_valueUint = o.m_valueUint;
    m_valuePath = o.m_valuePath;
    return *this;
}
//a move assignment: operator=(X&&)
security_context_params_config_t& security_context_params_config_t::operator=(security_context_params_config_t&& o) noexcept {
    m_paramName = o.m_paramName;
    m_valueUint = o.m_valueUint;
    m_valuePath = std::move(o.m_valuePath);
    return *this;
}
bool security_context_params_config_t::operator==(const security_context_params_config_t& o) const {
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
bool security_context_params_config_t::SetValuesFromPropertyTree(const boost::property_tree::ptree& pt) {
    
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
        LOG_ERROR(subprocess) << "parsing JSON security_context_params_config_t: " << e.what();
        return false;
    }
    return true;
}

boost::property_tree::ptree security_context_params_config_t::GetNewPropertyTree() const {
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

/////
security_operation_events_config_t::security_operation_events_config_t() :
    eventId(""),
    actions() {}

security_operation_events_config_t::~security_operation_events_config_t() {}

security_operation_events_config_t::security_operation_events_config_t(const security_operation_events_config_t& o) :
    eventId(o.eventId),
    actions(o.actions)
{ }

//a move constructor: X(X&&)
security_operation_events_config_t::security_operation_events_config_t(security_operation_events_config_t&& o) noexcept :
    eventId(std::move(o.eventId)),
    actions(std::move(o.actions))
{ }

//a copy assignment: operator=(const X&)
security_operation_events_config_t& security_operation_events_config_t::operator=(const security_operation_events_config_t& o) {
    eventId = o.eventId;
    actions = o.actions;
    return *this;
}

//a move assignment: operator=(X&&)
security_operation_events_config_t& security_operation_events_config_t::operator=(security_operation_events_config_t&& o) noexcept {
    eventId = std::move(o.eventId);
    actions = std::move(o.actions);
    return *this;
}

bool security_operation_events_config_t::operator==(const security_operation_events_config_t& other) const {
    return (eventId == other.eventId) &&
        (actions == other.actions);
}


//////
policy_rules_config_t::policy_rules_config_t() :
    m_description(""),
    m_securityPolicyRuleId(0),
    m_securityRole(""),
    m_securitySource(""),
    m_bundleSource(),
    m_bundleFinalDestination(),
    m_securityTargetBlockTypes(),
    m_securityService(""),
    m_securityContext(""),
    m_securityFailureEventSetReference(""),
    m_securityContextParamsVec() {}

policy_rules_config_t::~policy_rules_config_t() {}

//a copy constructor: X(const X&)
policy_rules_config_t::policy_rules_config_t(const policy_rules_config_t& o) :
    m_description(o.m_description),
    m_securityPolicyRuleId(o.m_securityPolicyRuleId),
    m_securityRole(o.m_securityRole),
    m_securitySource(o.m_securitySource),
    m_bundleSource(o.m_bundleSource),
    m_bundleFinalDestination(o.m_bundleFinalDestination),
    m_securityTargetBlockTypes(o.m_securityTargetBlockTypes),
    m_securityService(o.m_securityService),
    m_securityContext(o.m_securityContext),
    m_securityFailureEventSetReference(o.m_securityFailureEventSetReference),
    m_securityContextParamsVec(o.m_securityContextParamsVec)
{ }

//a move constructor: X(X&&)
policy_rules_config_t::policy_rules_config_t(policy_rules_config_t&& o) noexcept :
    m_description(std::move(o.m_description)),
    m_securityPolicyRuleId(o.m_securityPolicyRuleId),
    m_securityRole(std::move(o.m_securityRole)),
    m_securitySource(std::move(o.m_securitySource)),
    m_bundleSource(std::move(o.m_bundleSource)),
    m_bundleFinalDestination(std::move(o.m_bundleFinalDestination)),
    m_securityTargetBlockTypes(std::move(o.m_securityTargetBlockTypes)),
    m_securityService(std::move(o.m_securityService)),
    m_securityContext(std::move(o.m_securityContext)),
    m_securityFailureEventSetReference(std::move(o.m_securityFailureEventSetReference)),
    m_securityContextParamsVec(std::move(o.m_securityContextParamsVec))
{ }

//a copy assignment: operator=(const X&)
policy_rules_config_t& policy_rules_config_t::operator=(const policy_rules_config_t& o) {
    m_description = o.m_description;
    m_securityPolicyRuleId = o.m_securityPolicyRuleId;
    m_securityRole = o.m_securityRole;
    m_securitySource = o.m_securitySource;
    m_bundleSource = o.m_bundleSource;
    m_bundleFinalDestination = o.m_bundleFinalDestination;
    m_securityTargetBlockTypes = o.m_securityTargetBlockTypes;
    m_securityService = o.m_securityService;
    m_securityContext = o.m_securityContext;
    m_securityFailureEventSetReference = o.m_securityFailureEventSetReference;
    m_securityContextParamsVec = o.m_securityContextParamsVec;
    return *this;
}

//a move assignment: operator=(X&&)
policy_rules_config_t& policy_rules_config_t::operator=(policy_rules_config_t&& o) noexcept {
    m_description = std::move(o.m_description);
    m_securityPolicyRuleId = o.m_securityPolicyRuleId;
    m_securityRole = std::move(o.m_securityRole);
    m_securitySource = std::move(o.m_securitySource);
    m_bundleSource = std::move(o.m_bundleSource);
    m_bundleFinalDestination = std::move(o.m_bundleFinalDestination);
    m_securityTargetBlockTypes = std::move(o.m_securityTargetBlockTypes);
    m_securityService = std::move(o.m_securityService);
    m_securityContext = std::move(o.m_securityContext);
    m_securityFailureEventSetReference = std::move(o.m_securityFailureEventSetReference);
    m_securityContextParamsVec = std::move(o.m_securityContextParamsVec);
    return *this;
}

bool policy_rules_config_t::operator==(const policy_rules_config_t& o) const {
    return (m_description == o.m_description) &&
        (m_securityPolicyRuleId == o.m_securityPolicyRuleId) &&
        (m_securityRole == o.m_securityRole) &&
        (m_securitySource == o.m_securitySource) &&
        (m_bundleSource == o.m_bundleSource) &&
        (m_bundleFinalDestination == o.m_bundleFinalDestination) &&
        (m_securityTargetBlockTypes == o.m_securityTargetBlockTypes) &&
        (m_securityService == o.m_securityService) &&
        (m_securityContext == o.m_securityContext) &&
        (m_securityFailureEventSetReference == o.m_securityFailureEventSetReference) &&
        (m_securityContextParamsVec == o.m_securityContextParamsVec);
}

static bool IsValidUri(const std::string& uri) {
    uint64_t eidNodeNumber;
    uint64_t eidServiceNumber;
    bool serviceNumberIsWildCard;
    return (uri == "ipn:*.*") || Uri::ParseIpnUriString(uri, eidNodeNumber, eidServiceNumber, &serviceNumberIsWildCard);
}

bool policy_rules_config_t::SetValuesFromPropertyTree(const boost::property_tree::ptree& pt) {

    try {
        m_description = pt.get<std::string>("description");
        m_securityPolicyRuleId = pt.get<uint64_t>("securityPolicyRuleId");
        m_securityRole = pt.get<std::string>("securityRole");
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

        const boost::property_tree::ptree& securityTargetBlockTypesPt = pt.get_child("securityTargetBlockTypes", EMPTY_PTREE); //non-throw version
        m_securityTargetBlockTypes.clear();
        BOOST_FOREACH(const boost::property_tree::ptree::value_type & securityTargetBlockTypesValuePt, securityTargetBlockTypesPt) {
            const uint64_t securityTargetBlockTypeU64 = securityTargetBlockTypesValuePt.second.get_value<uint64_t>();
            if (m_securityTargetBlockTypes.insert(securityTargetBlockTypeU64).second == false) { //not inserted
                LOG_ERROR(subprocess) << "error parsing JSON policy rules: duplicate securityTargetBlockType " << securityTargetBlockTypeU64;
                return false;
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
        

        m_securityFailureEventSetReference = pt.get<std::string>("securityFailureEventSetReference");
        const boost::property_tree::ptree& securityContextParamsVectorPt = pt.get_child("securityContextParams", EMPTY_PTREE); //non-throw version
        m_securityContextParamsVec.resize(securityContextParamsVectorPt.size());
        unsigned int securityContextParamsVectorIndex = 0;
        BOOST_FOREACH(const boost::property_tree::ptree::value_type & securityContextParamsConfigPt, securityContextParamsVectorPt) {
            security_context_params_config_t& securityContextParamsConfig = m_securityContextParamsVec[securityContextParamsVectorIndex++];
            securityContextParamsConfig.SetValuesFromPropertyTree(securityContextParamsConfigPt.second);
        }
    }
    catch (const boost::property_tree::ptree_error& e) {
        LOG_ERROR(subprocess) << "parsing JSON policy_rules_config_t: " << e.what();
        return false;
    }
    return true;
}

boost::property_tree::ptree policy_rules_config_t::GetNewPropertyTree() const {
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

    boost::property_tree::ptree& securityTargetBlockTypesPt = pt.put_child("securityTargetBlockTypes",
        m_securityTargetBlockTypes.empty() ? boost::property_tree::ptree("[]") : boost::property_tree::ptree());
    for (std::set<uint64_t>::const_iterator securityTargetBlockTypesIt = m_securityTargetBlockTypes.cbegin();
        securityTargetBlockTypesIt != m_securityTargetBlockTypes.cend(); ++securityTargetBlockTypesIt)
    {
        securityTargetBlockTypesPt.push_back(std::make_pair("", boost::property_tree::ptree(boost::lexical_cast<std::string>(*securityTargetBlockTypesIt))));
    }

    pt.put("securityService", m_securityService);
    pt.put("securityContext", m_securityContext);
    pt.put("securityFailureEventSetReference", m_securityFailureEventSetReference);
    boost::property_tree::ptree& securityContextParamsVectorPt = pt.put_child("securityContextParams", m_securityContextParamsVec.empty() ? boost::property_tree::ptree("[]") : boost::property_tree::ptree());

    for (security_context_params_vector_t::const_iterator securityContextParamsVectorIt = m_securityContextParamsVec.cbegin();
        securityContextParamsVectorIt != m_securityContextParamsVec.cend(); ++securityContextParamsVectorIt)
    {
        const security_context_params_config_t& securityContextParams = *securityContextParamsVectorIt;
        securityContextParamsVectorPt.push_back(std::make_pair("", securityContextParams.GetNewPropertyTree())); //using "" as key creates json array
    }
    
    return pt;
}

security_failure_eventSets_config_t::security_failure_eventSets_config_t() :
    name(""),
    desc(""),
    securityOperationEvents() {}

security_failure_eventSets_config_t::~security_failure_eventSets_config_t() {}

//a copy constructor: X(const X&)
security_failure_eventSets_config_t::security_failure_eventSets_config_t(const security_failure_eventSets_config_t& o) :
    name(o.name),
    desc(o.desc),
    securityOperationEvents(o.securityOperationEvents)
{ }

//a move constructor: X(X&&)
security_failure_eventSets_config_t::security_failure_eventSets_config_t(security_failure_eventSets_config_t&& o) noexcept :
    name(std::move(o.name)),
    desc(std::move(o.desc)),
    securityOperationEvents(std::move(o.securityOperationEvents))
{ }

//a copy assignment: operator=(const X&)
security_failure_eventSets_config_t& security_failure_eventSets_config_t::operator=(const security_failure_eventSets_config_t& o) {
    name = o.name;
    desc = o.desc;
    securityOperationEvents = o.securityOperationEvents;
    return *this;
}

//a move assignment: operator=(X&&)
security_failure_eventSets_config_t& security_failure_eventSets_config_t::operator=(security_failure_eventSets_config_t&& o) noexcept {
    name = std::move(o.name);
    desc = std::move(o.desc);
    securityOperationEvents = std::move(o.securityOperationEvents);
    return *this;
}

bool security_failure_eventSets_config_t::operator==(const security_failure_eventSets_config_t& other) const {
    return (name == other.name) &&
        (desc == other.desc) &&
        (securityOperationEvents == other.securityOperationEvents);
}

BPSecConfig::BPSecConfig() :
    m_bpsecConfigName("unnamed BPSec config"),
    m_policyRulesConfigVector(),
    m_securityFailureEventSetsConfigVector(),
    m_securityOperationEventsConfigVector()
{}

BPSecConfig::~BPSecConfig() {
}

//a copy constructor: X(const X&)
BPSecConfig::BPSecConfig(const BPSecConfig& o) :
    m_bpsecConfigName(o.m_bpsecConfigName),
    m_policyRulesConfigVector(o.m_policyRulesConfigVector),
    m_securityFailureEventSetsConfigVector(o.m_securityFailureEventSetsConfigVector),
    m_securityOperationEventsConfigVector(o.m_securityOperationEventsConfigVector)
{ }

//a move constructor: X(X&&)
BPSecConfig::BPSecConfig(BPSecConfig&& o) noexcept :
    m_bpsecConfigName(std::move(o.m_bpsecConfigName)),
    m_policyRulesConfigVector(std::move(o.m_policyRulesConfigVector)),
    m_securityFailureEventSetsConfigVector(std::move(o.m_securityFailureEventSetsConfigVector)),
    m_securityOperationEventsConfigVector(std::move(o.m_securityOperationEventsConfigVector))
{ }

//a copy assignment: operator=(const X&)
BPSecConfig& BPSecConfig::operator=(const BPSecConfig& o) {
    m_bpsecConfigName = o.m_bpsecConfigName;
    m_policyRulesConfigVector = o.m_policyRulesConfigVector;
    m_securityFailureEventSetsConfigVector = o.m_securityFailureEventSetsConfigVector;
    m_securityOperationEventsConfigVector = o.m_securityOperationEventsConfigVector;
    return *this;
}

//a move assignment: operator=(X&&)
BPSecConfig& BPSecConfig::operator=(BPSecConfig&& o) noexcept {
    m_bpsecConfigName = std::move(o.m_bpsecConfigName);
    m_policyRulesConfigVector = std::move(o.m_policyRulesConfigVector);
    m_securityFailureEventSetsConfigVector = std::move(o.m_securityFailureEventSetsConfigVector);
    m_securityOperationEventsConfigVector = std::move(o.m_securityOperationEventsConfigVector);
    return *this;
}

bool BPSecConfig::operator==(const BPSecConfig& o) const {
    return (m_bpsecConfigName == o.m_bpsecConfigName) &&
        (m_policyRulesConfigVector == o.m_policyRulesConfigVector) &&
        (m_securityFailureEventSetsConfigVector == o.m_securityFailureEventSetsConfigVector) &&
        (m_securityOperationEventsConfigVector == o.m_securityOperationEventsConfigVector);
}

bool BPSecConfig::SetValuesFromPropertyTree(const boost::property_tree::ptree& pt) {
    try {
        m_bpsecConfigName = pt.get<std::string>("bpsecConfigName");
        if (m_bpsecConfigName == "") {
            LOG_ERROR(subprocess) << "parsing JSON BPSec config: bpsecConfigName must be defined and not empty string";
            return false;
        }
    }
    catch (const boost::property_tree::ptree_error& e) {
        LOG_ERROR(subprocess) << "parsing JSON HDTN config: " << e.what();
        return false;
    }

    

    const boost::property_tree::ptree& policyRulesConfigVectorPt = pt.get_child("policyRules", EMPTY_PTREE); //non-throw version
    m_policyRulesConfigVector.resize(policyRulesConfigVectorPt.size());
    unsigned int policyRulesConfigVectorIndex = 0;
    BOOST_FOREACH(const boost::property_tree::ptree::value_type & policyRulesConfigPt, policyRulesConfigVectorPt) {
        policy_rules_config_t& policyRulesConfig = m_policyRulesConfigVector[policyRulesConfigVectorIndex++];
        if (!policyRulesConfig.SetValuesFromPropertyTree(policyRulesConfigPt.second)) {
            LOG_ERROR(subprocess) << "error parsing JSON PolicyRulesConfigVector[" << (policyRulesConfigVectorIndex - 1) << "]";
            return false;
        }
    }

    const boost::property_tree::ptree& eventSetsConfigVectorPt = pt.get_child("securityFailureEventSets", EMPTY_PTREE); //non-throw version
    m_securityFailureEventSetsConfigVector.resize(eventSetsConfigVectorPt.size());
    unsigned int eventSetsConfigVectorIndex = 0;
    BOOST_FOREACH(const boost::property_tree::ptree::value_type & eventSetsConfigPt, eventSetsConfigVectorPt) {
        security_failure_eventSets_config_t& eventSetsConfig = m_securityFailureEventSetsConfigVector[eventSetsConfigVectorIndex++];
        try {
            eventSetsConfig.name = eventSetsConfigPt.second.get<std::string>("name");
            eventSetsConfig.desc = eventSetsConfigPt.second.get<std::string>("desc");
            const boost::property_tree::ptree& securityOperationEventsVectorPt = eventSetsConfigPt.second.get_child("securityOperationEvents", EMPTY_PTREE); //non-throw version
            m_securityOperationEventsConfigVector.resize(securityOperationEventsVectorPt.size());
            unsigned int securityOperationEventsVectorIndex = 0;
            BOOST_FOREACH(const boost::property_tree::ptree::value_type & securityOperationEventsConfigPt, securityOperationEventsVectorPt) {
                security_operation_events_config_t& securityOperationEventsConfig = m_securityOperationEventsConfigVector[securityOperationEventsVectorIndex++];
                securityOperationEventsConfig.eventId = securityOperationEventsConfigPt.second.get<std::string>("eventId");
                const boost::property_tree::ptree& actionsPt = securityOperationEventsConfigPt.second.get_child("actions", EMPTY_PTREE); //non-throw version
                securityOperationEventsConfig.actions.clear();
                BOOST_FOREACH(const boost::property_tree::ptree::value_type & actionsValuePt, actionsPt) {
                    const std::string actionStr = actionsValuePt.second.get_value<std::string>();
                    if (securityOperationEventsConfig.actions.insert(actionStr).second == false) { //not inserted
                        LOG_ERROR(subprocess) << "error parsing JSON security_operation_events: duplicate action " << actionStr;
                        return false;
                    }
                    
                }
            }
        }
        catch (const boost::property_tree::ptree_error& e) {
            LOG_ERROR(subprocess) << "error parsing JSON EventSetsConfigVector[" << (eventSetsConfigVectorIndex++ - 1) << "]: " << e.what();
            return false;
        }
    }


    return true;
}

BPSecConfig_ptr BPSecConfig::CreateFromJson(const std::string& jsonString, bool verifyNoUnusedJsonKeys) {
    boost::property_tree::ptree pt;
    BPSecConfig_ptr config; //NULL
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

BPSecConfig_ptr BPSecConfig::CreateFromJsonFilePath(const boost::filesystem::path& jsonFilePath, bool verifyNoUnusedJsonKeys) {
    boost::property_tree::ptree pt;
    BPSecConfig_ptr config; //NULL
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

BPSecConfig_ptr BPSecConfig::CreateFromPtree(const boost::property_tree::ptree& pt) {

    BPSecConfig_ptr ptrBPSecConfig = std::make_shared<BPSecConfig>();
    if (!ptrBPSecConfig->SetValuesFromPropertyTree(pt)) {
        ptrBPSecConfig = BPSecConfig_ptr(); //failed, so delete and set it NULL
    }
    return ptrBPSecConfig;
}

boost::property_tree::ptree BPSecConfig::GetNewPropertyTree() const {
    boost::property_tree::ptree pt;
    pt.put("bpsecConfigName", m_bpsecConfigName);

    boost::property_tree::ptree& policyRulesConfigVectorPt = pt.put_child("policyRules", m_policyRulesConfigVector.empty() ? boost::property_tree::ptree("[]") : boost::property_tree::ptree());
    for (policy_rules_config_vector_t::const_iterator policyRulesConfigVectorIt = m_policyRulesConfigVector.cbegin(); policyRulesConfigVectorIt != m_policyRulesConfigVector.cend(); ++policyRulesConfigVectorIt) {
        const policy_rules_config_t& policyRulesConfig = *policyRulesConfigVectorIt;
        policyRulesConfigVectorPt.push_back(std::make_pair("", policyRulesConfig.GetNewPropertyTree())); //using "" as key creates json array
    }

    boost::property_tree::ptree& securityFailureEventSetsConfigVectorPt = pt.put_child("securityFailureEventSets",
        m_securityFailureEventSetsConfigVector.empty() ? boost::property_tree::ptree("[]") : boost::property_tree::ptree());

    for (security_failure_eventSets_config_vector_t::const_iterator eventSetsConfigVectorIt = m_securityFailureEventSetsConfigVector.cbegin();
        eventSetsConfigVectorIt != m_securityFailureEventSetsConfigVector.cend(); ++eventSetsConfigVectorIt) {
        const security_failure_eventSets_config_t& eventSetsConfig = *eventSetsConfigVectorIt;
        boost::property_tree::ptree& eventSetsConfigPt = (securityFailureEventSetsConfigVectorPt.push_back(std::make_pair("", boost::property_tree::ptree())))->second; //using "" as key creates json array
        eventSetsConfigPt.put("name", eventSetsConfig.name);
        eventSetsConfigPt.put("desc", eventSetsConfig.desc);

        boost::property_tree::ptree& securityOperationEventsVectorPt = eventSetsConfigPt.put_child("securityOperationEvents",
            m_securityOperationEventsConfigVector.empty() ? boost::property_tree::ptree("[]") : boost::property_tree::ptree());
        for (security_operation_events_config_vector_t::const_iterator securityOperationEventsVectorIt = m_securityOperationEventsConfigVector.cbegin();
            securityOperationEventsVectorIt != m_securityOperationEventsConfigVector.cend(); ++securityOperationEventsVectorIt) {
            const security_operation_events_config_t& securityOperationEvents = *securityOperationEventsVectorIt;
            boost::property_tree::ptree& securityOperationEventsPt = (securityOperationEventsVectorPt.push_back(std::make_pair("", boost::property_tree::ptree())))->second; //using "" as key creates json array
            securityOperationEventsPt.put("eventId", securityOperationEvents.eventId);
            boost::property_tree::ptree& actionsPt = securityOperationEventsPt.put_child("actions",
                securityOperationEvents.actions.empty() ? boost::property_tree::ptree("[]") : boost::property_tree::ptree());
            for (std::set<std::string>::const_iterator actionsIt = securityOperationEvents.actions.cbegin(); actionsIt != securityOperationEvents.actions.cend(); ++actionsIt) {
                actionsPt.push_back(std::make_pair("", boost::property_tree::ptree(*actionsIt))); //using "" as key creates json array
            }
        }

    }

    return pt;
}
