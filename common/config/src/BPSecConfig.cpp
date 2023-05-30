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
#include <sstream>
#include <set>

static constexpr hdtn::Logger::SubProcess subprocess = hdtn::Logger::SubProcess::none;

security_context_params_config_t::security_context_params_config_t() :
    id(0), 
    paramName(""),
    value(0) {}

security_context_params_config_t::~security_context_params_config_t() {}

security_context_params_config_t::security_context_params_config_t(const security_context_params_config_t& o) :
    id(o.id),
    paramName(o.paramName),
    value(o.value)
{ }

//a move constructor: X(X&&)
security_context_params_config_t::security_context_params_config_t(security_context_params_config_t&& o) noexcept :
    id(std::move(o.id)),
    paramName(std::move(o.paramName)),
    value(std::move(o.value))
{ }

//a copy assignment: operator=(const X&)
security_context_params_config_t& security_context_params_config_t::operator=(const security_context_params_config_t& o) {
    id = o.id;
    paramName = o.paramName;
    value = o.value;
    return *this;
}

//a move assignment: operator=(X&&)
security_context_params_config_t& security_context_params_config_t::operator=(security_context_params_config_t&& o) noexcept {
    id = std::move(o.id);
    paramName = std::move(o.paramName);
    value = std::move(o.value);
    return *this;
}

bool security_context_params_config_t::operator==(const security_context_params_config_t & other) const {
    return (id == other.id) &&
           (paramName == other.paramName) && 
	   (value == other.value);
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

bool security_operation_events_config_t::operator==(const security_operation_events_config_t & other) const {
    return (eventId == other.eventId) &&
           (actions == other.actions);
}


//////
policy_rules_config_t::policy_rules_config_t() :
    desc(""),
    securityPolicyRuleId(0),
    securityRole(""),
    securitySource(""),
    bundleSource(""),
    finalDest(""),
    securityTargetBlockType(""),
    securityService(""),
    securityContextId(""),
    securityFailureEventSetReference(""),
    securityContextParams() {}

policy_rules_config_t::~policy_rules_config_t() {}

//a copy constructor: X(const X&)
policy_rules_config_t::policy_rules_config_t(const policy_rules_config_t& o) :
    desc(o.desc),
    securityPolicyRuleId(o.securityPolicyRuleId),
    securityRole(o.securityRole),
    securitySource(o.securitySource),
    bundleSource(o.bundleSource),
    finalDest(o.finalDest),
    securityTargetBlockType(o.securityTargetBlockType),
    securityService(o.securityService),
    securityContextId(o.securityContextId),
    securityFailureEventSetReference(o.securityFailureEventSetReference),
    securityContextParams(o.securityContextParams) 
{ }

//a move constructor: X(X&&)
policy_rules_config_t::policy_rules_config_t(policy_rules_config_t&& o) noexcept :
    desc(std::move(o.desc)), 
    securityPolicyRuleId(std::move(o.securityPolicyRuleId)), 
    securityRole(std::move(o.securityRole)), 
    securitySource(std::move(o.securitySource)),
    bundleSource(std::move(o.bundleSource)),
    finalDest(std::move(o.finalDest)),
    securityTargetBlockType(std::move(o.securityTargetBlockType)),
    securityService(std::move(o.securityService)),
    securityContextId(std::move(o.securityContextId)),
    securityFailureEventSetReference(std::move(o.securityFailureEventSetReference)),
    securityContextParams(std::move(o.securityContextParams))
{ }

//a copy assignment: operator=(const X&)
policy_rules_config_t& policy_rules_config_t::operator=(const policy_rules_config_t& o) {
    desc = o.desc;
    securityPolicyRuleId = o.securityPolicyRuleId;
    securityRole = o.securityRole;
    securitySource = o.securitySource;
    bundleSource = o.bundleSource;
    finalDest = o.finalDest;
    securityTargetBlockType = o.securityTargetBlockType;
    securityService = o.securityService;
    securityContextId = o.securityContextId;
    securityFailureEventSetReference = o.securityFailureEventSetReference;
    securityContextParams = o.securityContextParams;
    return *this;
}

//a move assignment: operator=(X&&)
policy_rules_config_t& policy_rules_config_t::operator=(policy_rules_config_t&& o) noexcept {
    desc = std::move(o.desc);
    securityPolicyRuleId = std::move(o.securityPolicyRuleId);
    securityRole = std::move(o.securityRole);
    securitySource = std::move(o.securitySource);
    bundleSource = std::move(o.bundleSource);
    finalDest = std::move(o.finalDest);
    securityTargetBlockType = std::move(o.securityTargetBlockType);
    securityService = std::move(o.securityService);
    securityContextId = std::move(o.securityContextId);
    securityFailureEventSetReference = std::move(o.securityFailureEventSetReference);
    securityContextParams = std::move(o.securityContextParams);
    return *this;
}

bool policy_rules_config_t::operator==(const policy_rules_config_t & other) const {
    return (desc == other.desc) && 
	   (securityPolicyRuleId == other.securityPolicyRuleId) &&
	   (securityRole == other.securityRole) &&
	   (securitySource == other.securitySource) &&
	   (bundleSource == other.bundleSource) &&
	   (finalDest == other.finalDest) &&
	   (securityTargetBlockType == other.securityTargetBlockType) &&
	   (securityService == other.securityService) &&
	   (securityContextId == other.securityContextId) &&
	   (securityFailureEventSetReference == other.securityFailureEventSetReference) &&
	   (securityContextParams == other.securityContextParams);
}

//////
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

bool security_failure_eventSets_config_t::operator==(const security_failure_eventSets_config_t & other) const {
    return (name == other.name) && 
	   (desc == other.desc) &&
	   (securityOperationEvents == other.securityOperationEvents); 
}


BPSecConfig::BPSecConfig() :
    m_bpsecConfigName("unnamed BPSec config"),
    m_policyRulesConfigVector(),
    m_securityFailureEventSetsConfigVector(), 
    m_securityContextParamsVector(),
    m_securityOperationEventsConfigVector()
{}

BPSecConfig::~BPSecConfig() {
}

//a copy constructor: X(const X&)
BPSecConfig::BPSecConfig(const BPSecConfig& o) :
    m_bpsecConfigName(o.m_bpsecConfigName),
    m_policyRulesConfigVector(o.m_policyRulesConfigVector),	
    m_securityFailureEventSetsConfigVector(o.m_securityFailureEventSetsConfigVector),
    m_securityContextParamsVector(o.m_securityContextParamsVector),
    m_securityOperationEventsConfigVector(o.m_securityOperationEventsConfigVector)
{ }

//a move constructor: X(X&&)
BPSecConfig::BPSecConfig(BPSecConfig&& o) noexcept :
    m_bpsecConfigName(std::move(o.m_bpsecConfigName)),
    m_policyRulesConfigVector(std::move(o.m_policyRulesConfigVector)),
    m_securityFailureEventSetsConfigVector(std::move(o.m_securityFailureEventSetsConfigVector)), 
    m_securityContextParamsVector(std::move(o.m_securityContextParamsVector)),
    m_securityOperationEventsConfigVector(std::move(o.m_securityOperationEventsConfigVector))

{ }

//a copy assignment: operator=(const X&)
BPSecConfig& BPSecConfig::operator=(const BPSecConfig& o) {
    m_bpsecConfigName = o.m_bpsecConfigName;
    m_policyRulesConfigVector = o.m_policyRulesConfigVector;
    m_securityFailureEventSetsConfigVector = o.m_securityFailureEventSetsConfigVector;
    m_securityContextParamsVector = o.m_securityContextParamsVector;
    m_securityOperationEventsConfigVector = o.m_securityOperationEventsConfigVector;
    return *this;
}

//a move assignment: operator=(X&&)
BPSecConfig& BPSecConfig::operator=(BPSecConfig&& o) noexcept {
    m_bpsecConfigName = std::move(o.m_bpsecConfigName);
    m_policyRulesConfigVector = std::move(o.m_policyRulesConfigVector);
    m_securityFailureEventSetsConfigVector = std::move(o.m_securityFailureEventSetsConfigVector);
    m_securityContextParamsVector = std::move(o.m_securityContextParamsVector);
    m_securityOperationEventsConfigVector = std::move(o.m_securityOperationEventsConfigVector);
    return *this;
}

bool BPSecConfig::operator==(const BPSecConfig & o) const {
    return (m_bpsecConfigName == o.m_bpsecConfigName) &&
	   (m_policyRulesConfigVector == o.m_policyRulesConfigVector) &&
        (m_securityFailureEventSetsConfigVector == o.m_securityFailureEventSetsConfigVector) &&
	(m_securityContextParamsVector == o.m_securityContextParamsVector) &&
        (m_securityOperationEventsConfigVector == o.m_securityOperationEventsConfigVector);
}

bool BPSecConfig::SetValuesFromPropertyTree(const boost::property_tree::ptree & pt) {
    try {
        m_bpsecConfigName = pt.get<std::string>("bpsecConfigName");
        if (m_bpsecConfigName == "") {
            LOG_ERROR(subprocess) << "parsing JSON BPSec config: bpsecConfigName must be defined and not empty string";
            return false;
        }
    }
    catch (const boost::property_tree::ptree_error & e) {
        LOG_ERROR(subprocess) << "parsing JSON HDTN config: " << e.what();
        return false;
    }

    //for non-throw versions of get_child which return a reference to the second parameter
    static const boost::property_tree::ptree EMPTY_PTREE;

    const boost::property_tree::ptree & policyRulesConfigVectorPt = pt.get_child("policyRules", EMPTY_PTREE); //non-throw version
    m_policyRulesConfigVector.resize(policyRulesConfigVectorPt.size());
    unsigned int policyRulesConfigVectorIndex = 0;
    BOOST_FOREACH(const boost::property_tree::ptree::value_type & policyRulesConfigPt, policyRulesConfigVectorPt) {
        policy_rules_config_t & policyRulesConfig = m_policyRulesConfigVector[policyRulesConfigVectorIndex++];
        try {
            policyRulesConfig.desc = policyRulesConfigPt.second.get<std::string>("desc");
	    policyRulesConfig.securityPolicyRuleId = policyRulesConfigPt.second.get<uint64_t>("securityPolicyRuleId");
            policyRulesConfig.securityRole = policyRulesConfigPt.second.get<std::string>("securityRole");
	    policyRulesConfig.securitySource = policyRulesConfigPt.second.get<std::string>("securitySource");
	    policyRulesConfig.bundleSource = policyRulesConfigPt.second.get<std::string>("bundleSource");
	    policyRulesConfig.finalDest = policyRulesConfigPt.second.get<std::string>("finalDest");
	    policyRulesConfig.securityTargetBlockType = policyRulesConfigPt.second.get<std::string>("securityTargetBlockType");
	    policyRulesConfig.securityService = policyRulesConfigPt.second.get<std::string>("securityService");
	    policyRulesConfig.securityContextId = policyRulesConfigPt.second.get<std::string>("securityContextId");
	    policyRulesConfig.securityFailureEventSetReference = policyRulesConfigPt.second.get<std::string>("securityFailureEventSetReference");
            const boost::property_tree::ptree & securityContextParamsVectorPt = policyRulesConfigPt.second.get_child("securityContextParams", EMPTY_PTREE); //non-throw version
            unsigned int securityContextParamsVectorIndex = 0;
	    BOOST_FOREACH(const boost::property_tree::ptree::value_type & securityContextParamsConfigPt, securityContextParamsVectorPt) {
                security_context_params_config_t & securityContextParamsConfig = m_securityContextParamsVector[securityContextParamsVectorIndex++];
                securityContextParamsConfig.id = securityContextParamsConfigPt.second.get<uint64_t>("id");
		securityContextParamsConfig.paramName = securityContextParamsConfigPt.second.get<std::string>("paramName");
		securityContextParamsConfig.value= securityContextParamsConfigPt.second.get<uint64_t>("value");
	    }
	}
        catch (const boost::property_tree::ptree_error & e) {
            LOG_ERROR(subprocess) << "error parsing JSON PolicyRulesConfigVector[" << (policyRulesConfigVectorIndex++ - 1) << "]: " << e.what();
            return false;
       }

    }

    const boost::property_tree::ptree & eventSetsConfigVectorPt = pt.get_child("securityFailureEventSets", EMPTY_PTREE); //non-throw version
    m_securityFailureEventSetsConfigVector.resize(eventSetsConfigVectorPt.size());
    unsigned int eventSetsConfigVectorIndex = 0;
    BOOST_FOREACH(const boost::property_tree::ptree::value_type & eventSetsConfigPt, eventSetsConfigVectorPt) {
        security_failure_eventSets_config_t & eventSetsConfig = m_securityFailureEventSetsConfigVector[eventSetsConfigVectorIndex++];
        try {
            eventSetsConfig.name = eventSetsConfigPt.second.get<std::string>("name");
            eventSetsConfig.desc = eventSetsConfigPt.second.get<std::string>("desc");
            const boost::property_tree::ptree & securityOperationEventsVectorPt = eventSetsConfigPt.second.get_child("securityOperationEvents", EMPTY_PTREE); //non-throw version
            unsigned int securityOperationEventsVectorIndex = 0;
            BOOST_FOREACH(const boost::property_tree::ptree::value_type & securityOperationEventsConfigPt, securityOperationEventsVectorPt) {
                security_operation_events_config_t & securityOperationEventsConfig = m_securityOperationEventsConfigVector[securityOperationEventsVectorIndex++];
                securityOperationEventsConfig.eventId = securityOperationEventsConfigPt.second.get<std::string>("eventId");
		const boost::property_tree::ptree & actionsPt = securityOperationEventsConfigPt.second.get_child("actions", EMPTY_PTREE); //non-throw version
                securityOperationEventsConfig.actions.clear();
            	BOOST_FOREACH(const boost::property_tree::ptree::value_type & actionsValuePt, actionsPt) {
                    const std::string actionStr = actionsValuePt.second.get_value<std::string>();
            	}
            }
	}
        catch (const boost::property_tree::ptree_error & e) {
            LOG_ERROR(subprocess) << "error parsing JSON EventSetsConfigVector[" << (eventSetsConfigVectorIndex++ - 1) << "]: " << e.what();
            return false;
        }
    }
  

    return true;
}

BPSecConfig_ptr BPSecConfig::CreateFromJson(const std::string & jsonString, bool verifyNoUnusedJsonKeys) {
    boost::property_tree::ptree pt;
    BPSecConfig_ptr config; //NULL
    if (GetPropertyTreeFromJsonString(jsonString, pt)) { //prints message if failed
	    std::cout << "Creating config from PTree " << std::endl;
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
	 std::cout << "  Calling GetPropertyTreeFromJsonString " << std::endl;

    if (GetPropertyTreeFromJsonFilePath(jsonFilePath, pt)) { //prints message if failed
        config = CreateFromPtree(pt);
        std::cout << "Created config from PTree " << std::endl;	
        //verify that there are no unused variables within the original json
        if (config && verifyNoUnusedJsonKeys) {

            std::string returnedErrorMessage;
            if (JsonSerializable::HasUnusedJsonVariablesInFilePath(*config, jsonFilePath, returnedErrorMessage)) {
                LOG_ERROR(subprocess) << returnedErrorMessage;
                config.reset(); //NULL
            }
        }
	std::cout << "config from PTree is null!! " << std::endl;
    }
    return config;
}

BPSecConfig_ptr BPSecConfig::CreateFromPtree(const boost::property_tree::ptree & pt) {

    BPSecConfig_ptr ptrBPSecConfig = std::make_shared<BPSecConfig>();
    if (!ptrBPSecConfig->SetValuesFromPropertyTree(pt)) {
        ptrBPSecConfig = BPSecConfig_ptr(); //failed, so delete and set it NULL
    }
    return ptrBPSecConfig;
}

boost::property_tree::ptree BPSecConfig::GetNewPropertyTree() const {
    boost::property_tree::ptree pt;
    pt.put("bpsecConfigName", m_bpsecConfigName);

     boost::property_tree::ptree & policyRulesConfigVectorPt = pt.put_child("policyRules", m_policyRulesConfigVector.empty() ? boost::property_tree::ptree("[]") : boost::property_tree::ptree());
    for (policy_rules_config_vector_t::const_iterator policyRulesConfigVectorIt = m_policyRulesConfigVector.cbegin(); policyRulesConfigVectorIt != m_policyRulesConfigVector.cend(); ++policyRulesConfigVectorIt) {
        const policy_rules_config_t & policyRulesConfig = *policyRulesConfigVectorIt;
        boost::property_tree::ptree & policyRulesConfigPt = (policyRulesConfigVectorPt.push_back(std::make_pair("", boost::property_tree::ptree())))->second; //using "" as key creates json array
        policyRulesConfigPt.put("desc", policyRulesConfig.desc);
        policyRulesConfigPt.put("securityPolicyRuleId", policyRulesConfig.securityPolicyRuleId);
        policyRulesConfigPt.put("securityRole", policyRulesConfig.securityRole);
        policyRulesConfigPt.put("securitySource", policyRulesConfig.securitySource);
        policyRulesConfigPt.put("bundleSource", policyRulesConfig.bundleSource);
        policyRulesConfigPt.put("finalDest", policyRulesConfig.finalDest);
        policyRulesConfigPt.put("securityTargetBlockType", policyRulesConfig.securityTargetBlockType);
	policyRulesConfigPt.put("securityService", policyRulesConfig.securityService);
	policyRulesConfigPt.put("securityContextId", policyRulesConfig.securityContextId);
	policyRulesConfigPt.put("securityFailureEventSetReference", policyRulesConfig.securityFailureEventSetReference);
        boost::property_tree::ptree & securityContextParamsVectorPt =  policyRulesConfigPt.put_child("securityContextParams", m_securityContextParamsVector.empty() ? boost::property_tree::ptree("[]") : boost::property_tree::ptree());
 
	for (security_context_params_vector_t::const_iterator securityContextParamsVectorIt = m_securityContextParamsVector.cbegin(); securityContextParamsVectorIt != m_securityContextParamsVector.cend(); ++securityContextParamsVectorIt) {
	    const security_context_params_config_t & securityContextParams = *securityContextParamsVectorIt;
            boost::property_tree::ptree & securityContextParamsPt = (securityContextParamsVectorPt.push_back(std::make_pair("", boost::property_tree::ptree())))->second; //using "" as key creates json array
            securityContextParamsPt.put("id", securityContextParams.id);
	    securityContextParamsPt.put("paramName", securityContextParams.paramName);
	    securityContextParamsPt.put("value", securityContextParams.value);
	}
    }	

    boost::property_tree::ptree & securityFailureEventSetsConfigVectorPt = pt.put_child("securityFailureEventSets", 
         m_securityFailureEventSetsConfigVector.empty() ? boost::property_tree::ptree("[]") : boost::property_tree::ptree());
    
    for (security_failure_eventSets_config_vector_t::const_iterator eventSetsConfigVectorIt = m_securityFailureEventSetsConfigVector.cbegin();
         eventSetsConfigVectorIt != m_securityFailureEventSetsConfigVector.cend(); ++eventSetsConfigVectorIt) {
        const security_failure_eventSets_config_t & eventSetsConfig = *eventSetsConfigVectorIt;
        boost::property_tree::ptree & eventSetsConfigPt = (securityFailureEventSetsConfigVectorPt.push_back(std::make_pair("", boost::property_tree::ptree())))->second; //using "" as key creates json array
        eventSetsConfigPt.put("name", eventSetsConfig.name);
	eventSetsConfigPt.put("desc", eventSetsConfig.desc);

	boost::property_tree::ptree & securityOperationEventsVectorPt =  eventSetsConfigPt.put_child("securityOperationEvents", 
			  m_securityOperationEventsConfigVector.empty() ? boost::property_tree::ptree("[]") : boost::property_tree::ptree());
        for (security_operation_events_config_vector_t::const_iterator securityOperationEventsVectorIt = m_securityOperationEventsConfigVector.cbegin(); 
	    securityOperationEventsVectorIt != m_securityOperationEventsConfigVector.cend(); ++securityOperationEventsVectorIt) {
            const security_operation_events_config_t & securityOperationEvents = *securityOperationEventsVectorIt;
            boost::property_tree::ptree & securityOperationEventsPt = (securityOperationEventsVectorPt.push_back(std::make_pair("", boost::property_tree::ptree())))->second; //using "" as key creates json array
            securityOperationEventsPt.put("eventId", securityOperationEvents.eventId);
            boost::property_tree::ptree & actionsPt = securityOperationEventsPt.put_child("actions", 
			    securityOperationEvents.actions.empty() ? boost::property_tree::ptree("[]") : boost::property_tree::ptree());
	    for (std::set<std::string>::const_iterator actionsIt = securityOperationEvents.actions.cbegin(); actionsIt != securityOperationEvents.actions.cend(); ++actionsIt) {
                actionsPt.push_back(std::make_pair("", boost::property_tree::ptree(*actionsIt))); //using "" as key creates json array
            }
        }

    }
    
    return pt;
}
