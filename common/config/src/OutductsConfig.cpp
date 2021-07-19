/***************************************************************************
 * NASA Glenn Research Center, Cleveland, OH
 * Released under the NASA Open Source Agreement (NOSA)
 * May  2021
 ****************************************************************************
 */

#include "OutductsConfig.h"
#include <boost/make_shared.hpp>
#include <boost/foreach.hpp>
#include <iostream>
#include <boost/lexical_cast.hpp>

static const std::vector<std::string> VALID_CONVERGENCE_LAYER_NAMES = { "ltp_over_udp", "udp", "stcp", "tcpcl" };

outduct_element_config_t::outduct_element_config_t() :
    name(""),
    convergenceLayer(""),
    endpointIdStr(""),
    remoteHostname(""),
    remotePort(0),
    bundlePipelineLimit(0),
    flowIds(),
    
    thisLtpEngineId(0),
    remoteLtpEngineId(0),
    ltpDataSegmentMtu(0),
    oneWayLightTimeMs(0),
    oneWayMarginTimeMs(0),
    clientServiceId(0),
    numRxCircularBufferElements(0),
    ltpMaxRetriesPerSerialNumber(0),
    ltpCheckpointEveryNthDataSegment(0),
    ltpRandomNumberSizeBits(0),
    ltpSenderBoundPort(0),

    udpRateBps(0),

    keepAliveIntervalSeconds(0),
    tcpclAutoFragmentSizeBytes(0) {}
outduct_element_config_t::~outduct_element_config_t() {}


//a copy constructor: X(const X&)
outduct_element_config_t::outduct_element_config_t(const outduct_element_config_t& o) :
    name(o.name),
    convergenceLayer(o.convergenceLayer),
    endpointIdStr(o.endpointIdStr),
    remoteHostname(o.remoteHostname),
    remotePort(o.remotePort),
    bundlePipelineLimit(o.bundlePipelineLimit),
    flowIds(o.flowIds),
    
    thisLtpEngineId(o.thisLtpEngineId),
    remoteLtpEngineId(o.remoteLtpEngineId),
    ltpDataSegmentMtu(o.ltpDataSegmentMtu),
    oneWayLightTimeMs(o.oneWayLightTimeMs),
    oneWayMarginTimeMs(o.oneWayMarginTimeMs),
    clientServiceId(o.clientServiceId),
    numRxCircularBufferElements(o.numRxCircularBufferElements),
    ltpMaxRetriesPerSerialNumber(o.ltpMaxRetriesPerSerialNumber),
    ltpCheckpointEveryNthDataSegment(o.ltpCheckpointEveryNthDataSegment),
    ltpRandomNumberSizeBits(o.ltpRandomNumberSizeBits),
    ltpSenderBoundPort(o.ltpSenderBoundPort),

    udpRateBps(o.udpRateBps),

    keepAliveIntervalSeconds(o.keepAliveIntervalSeconds),
    tcpclAutoFragmentSizeBytes(o.tcpclAutoFragmentSizeBytes) { }

//a move constructor: X(X&&)
outduct_element_config_t::outduct_element_config_t(outduct_element_config_t&& o) :
    name(std::move(o.name)),
    convergenceLayer(std::move(o.convergenceLayer)),
    endpointIdStr(std::move(o.endpointIdStr)),
    remoteHostname(std::move(o.remoteHostname)),
    remotePort(o.remotePort),
    bundlePipelineLimit(o.bundlePipelineLimit),
    flowIds(std::move(o.flowIds)),
    
    thisLtpEngineId(o.thisLtpEngineId),
    remoteLtpEngineId(o.remoteLtpEngineId),
    ltpDataSegmentMtu(o.ltpDataSegmentMtu),
    oneWayLightTimeMs(o.oneWayLightTimeMs),
    oneWayMarginTimeMs(o.oneWayMarginTimeMs),
    clientServiceId(o.clientServiceId),
    numRxCircularBufferElements(o.numRxCircularBufferElements),
    ltpMaxRetriesPerSerialNumber(o.ltpMaxRetriesPerSerialNumber),
    ltpCheckpointEveryNthDataSegment(o.ltpCheckpointEveryNthDataSegment),
    ltpRandomNumberSizeBits(o.ltpRandomNumberSizeBits),
    ltpSenderBoundPort(o.ltpSenderBoundPort),

    udpRateBps(o.udpRateBps),

    keepAliveIntervalSeconds(o.keepAliveIntervalSeconds),
    tcpclAutoFragmentSizeBytes(o.tcpclAutoFragmentSizeBytes) { }

//a copy assignment: operator=(const X&)
outduct_element_config_t& outduct_element_config_t::operator=(const outduct_element_config_t& o) {
    name = o.name;
    convergenceLayer = o.convergenceLayer;
    endpointIdStr = o.endpointIdStr;
    remoteHostname = o.remoteHostname;
    remotePort = o.remotePort;
    bundlePipelineLimit = o.bundlePipelineLimit;
    flowIds = o.flowIds;
    
    thisLtpEngineId = o.thisLtpEngineId;
    remoteLtpEngineId = o.remoteLtpEngineId;
    ltpDataSegmentMtu = o.ltpDataSegmentMtu;
    oneWayLightTimeMs = o.oneWayLightTimeMs;
    oneWayMarginTimeMs = o.oneWayMarginTimeMs;
    clientServiceId = o.clientServiceId;
    numRxCircularBufferElements = o.numRxCircularBufferElements;
    ltpMaxRetriesPerSerialNumber = o.ltpMaxRetriesPerSerialNumber;
    ltpCheckpointEveryNthDataSegment = o.ltpCheckpointEveryNthDataSegment;
    ltpRandomNumberSizeBits = o.ltpRandomNumberSizeBits;
    ltpSenderBoundPort = o.ltpSenderBoundPort;

    udpRateBps = o.udpRateBps;

    keepAliveIntervalSeconds = o.keepAliveIntervalSeconds;

    tcpclAutoFragmentSizeBytes = o.tcpclAutoFragmentSizeBytes;
    return *this;
}

//a move assignment: operator=(X&&)
outduct_element_config_t& outduct_element_config_t::operator=(outduct_element_config_t&& o) {
    name = std::move(o.name);
    convergenceLayer = std::move(o.convergenceLayer);
    endpointIdStr = std::move(o.endpointIdStr);
    remoteHostname = std::move(o.remoteHostname);
    remotePort = o.remotePort;
    bundlePipelineLimit = o.bundlePipelineLimit;
    flowIds = std::move(o.flowIds);

    thisLtpEngineId = o.thisLtpEngineId;
    remoteLtpEngineId = o.remoteLtpEngineId;
    ltpDataSegmentMtu = o.ltpDataSegmentMtu;
    oneWayLightTimeMs = o.oneWayLightTimeMs;
    oneWayMarginTimeMs = o.oneWayMarginTimeMs;
    clientServiceId = o.clientServiceId;
    numRxCircularBufferElements = o.numRxCircularBufferElements;
    ltpMaxRetriesPerSerialNumber = o.ltpMaxRetriesPerSerialNumber;
    ltpCheckpointEveryNthDataSegment = o.ltpCheckpointEveryNthDataSegment;
    ltpRandomNumberSizeBits = o.ltpRandomNumberSizeBits;
    ltpSenderBoundPort = o.ltpSenderBoundPort;

    udpRateBps = o.udpRateBps;

    keepAliveIntervalSeconds = o.keepAliveIntervalSeconds;

    tcpclAutoFragmentSizeBytes = o.tcpclAutoFragmentSizeBytes;
    return *this;
}

bool outduct_element_config_t::operator==(const outduct_element_config_t & o) const {
    return (name == o.name) &&
        (convergenceLayer == o.convergenceLayer) &&
        (endpointIdStr == o.endpointIdStr) &&
        (remoteHostname == o.remoteHostname) &&
        (remotePort == o.remotePort) &&
        (bundlePipelineLimit == o.bundlePipelineLimit) &&
        (flowIds == o.flowIds) &&
        
        (thisLtpEngineId == o.thisLtpEngineId) &&
        (remoteLtpEngineId == o.remoteLtpEngineId) &&
        (ltpDataSegmentMtu == o.ltpDataSegmentMtu) &&
        (oneWayLightTimeMs == o.oneWayLightTimeMs) &&
        (oneWayMarginTimeMs == o.oneWayMarginTimeMs) &&
        (clientServiceId == o.clientServiceId) &&
        (numRxCircularBufferElements == o.numRxCircularBufferElements) &&
        (ltpMaxRetriesPerSerialNumber == o.ltpMaxRetriesPerSerialNumber) &&
        (ltpCheckpointEveryNthDataSegment == o.ltpCheckpointEveryNthDataSegment) &&
        (ltpRandomNumberSizeBits == o.ltpRandomNumberSizeBits) &&
        (ltpSenderBoundPort == o.ltpSenderBoundPort) &&

        (udpRateBps == o.udpRateBps) &&

        (keepAliveIntervalSeconds == o.keepAliveIntervalSeconds) &&
        
        (tcpclAutoFragmentSizeBytes == o.tcpclAutoFragmentSizeBytes);
}

OutductsConfig::OutductsConfig() {
}

OutductsConfig::~OutductsConfig() {
}

//a copy constructor: X(const X&)
OutductsConfig::OutductsConfig(const OutductsConfig& o) :
    m_outductConfigName(o.m_outductConfigName), m_outductElementConfigVector(o.m_outductElementConfigVector) { }

//a move constructor: X(X&&)
OutductsConfig::OutductsConfig(OutductsConfig&& o) :
    m_outductConfigName(std::move(o.m_outductConfigName)), m_outductElementConfigVector(std::move(o.m_outductElementConfigVector)) { }

//a copy assignment: operator=(const X&)
OutductsConfig& OutductsConfig::operator=(const OutductsConfig& o) {
    m_outductConfigName = o.m_outductConfigName;
    m_outductElementConfigVector = o.m_outductElementConfigVector;
    return *this;
}

//a move assignment: operator=(X&&)
OutductsConfig& OutductsConfig::operator=(OutductsConfig&& o) {
    m_outductConfigName = std::move(o.m_outductConfigName);
    m_outductElementConfigVector = std::move(o.m_outductElementConfigVector);
    return *this;
}

bool OutductsConfig::operator==(const OutductsConfig & o) const {
    return (m_outductConfigName == o.m_outductConfigName) && (m_outductElementConfigVector == o.m_outductElementConfigVector);
}

bool OutductsConfig::SetValuesFromPropertyTree(const boost::property_tree::ptree & pt) {
    m_outductConfigName = pt.get<std::string>("outductConfigName", ""); //non-throw version
    if (m_outductConfigName == "") {
        std::cerr << "error: outductConfigName must be defined and not empty string\n";
        return false;
    }
    const boost::property_tree::ptree & outductElementConfigVectorPt = pt.get_child("outductVector", boost::property_tree::ptree()); //non-throw version
    m_outductElementConfigVector.resize(outductElementConfigVectorPt.size());
    unsigned int vectorIndex = 0;
    BOOST_FOREACH(const boost::property_tree::ptree::value_type & outductElementConfigPt, outductElementConfigVectorPt) {
        outduct_element_config_t & outductElementConfig = m_outductElementConfigVector[vectorIndex++];
        outductElementConfig.name = outductElementConfigPt.second.get<std::string>("name", "unnamed_outduct"); //non-throw version
        outductElementConfig.convergenceLayer = outductElementConfigPt.second.get<std::string>("convergenceLayer", "unnamed_convergence_layer"); //non-throw version
        {
            bool found = false;
            for (std::vector<std::string>::const_iterator it = VALID_CONVERGENCE_LAYER_NAMES.cbegin(); it != VALID_CONVERGENCE_LAYER_NAMES.cend(); ++it) {
                if (outductElementConfig.convergenceLayer == *it) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                std::cerr << "error: invalid convergence layer " << outductElementConfig.convergenceLayer << std::endl;
                return false;
            }
        }
        outductElementConfig.endpointIdStr = outductElementConfigPt.second.get<std::string>("endpointIdStr", "unnamed_endpoint_id"); //non-throw version
        outductElementConfig.remoteHostname = outductElementConfigPt.second.get<std::string>("remoteHostname", ""); //non-throw version
        if (outductElementConfig.remoteHostname == "") {
            std::cerr << "error: invalid remoteHostname, must not be empty" << std::endl;
            return false;
        }
        outductElementConfig.remotePort = outductElementConfigPt.second.get<uint16_t>("remotePort", 0); //non-throw version
        if (outductElementConfig.remotePort == 0) {
            std::cerr << "error: invalid remotePort, must be non-zero" << std::endl;
            return false;
        }
        outductElementConfig.bundlePipelineLimit = outductElementConfigPt.second.get<uint32_t>("bundlePipelineLimit", 0); //non-throw version
        const boost::property_tree::ptree & flowIdsPt = outductElementConfigPt.second.get_child("flowIds", boost::property_tree::ptree()); //non-throw version
        outductElementConfig.flowIds.clear();
        BOOST_FOREACH(const boost::property_tree::ptree::value_type & flowIdValuePt, flowIdsPt) {
            if (outductElementConfig.flowIds.insert(flowIdValuePt.second.get_value<uint64_t>()).second == false) { //not inserted
                std::cerr << "error: duplicate flow id " << flowIdValuePt.second.get_value<uint64_t>() << std::endl;
                return false;
            }
        }

        if (outductElementConfig.convergenceLayer == "ltp_over_udp") {
            outductElementConfig.thisLtpEngineId = outductElementConfigPt.second.get<uint64_t>("thisLtpEngineId", 0); //non-throw version
            outductElementConfig.remoteLtpEngineId = outductElementConfigPt.second.get<uint64_t>("remoteLtpEngineId", 0); //non-throw version
            outductElementConfig.ltpDataSegmentMtu = outductElementConfigPt.second.get<uint32_t>("ltpDataSegmentMtu", 1000); //non-throw version
            outductElementConfig.oneWayLightTimeMs = outductElementConfigPt.second.get<uint64_t>("oneWayLightTimeMs", 1000); //non-throw version
            outductElementConfig.oneWayMarginTimeMs = outductElementConfigPt.second.get<uint64_t>("oneWayMarginTimeMs", 0); //non-throw version
            outductElementConfig.clientServiceId = outductElementConfigPt.second.get<uint64_t>("clientServiceId", 0); //non-throw version
            outductElementConfig.numRxCircularBufferElements = outductElementConfigPt.second.get<uint32_t>("numRxCircularBufferElements", 100); //non-throw version
            outductElementConfig.ltpMaxRetriesPerSerialNumber = outductElementConfigPt.second.get<uint32_t>("ltpMaxRetriesPerSerialNumber", 5); //non-throw version
            outductElementConfig.ltpCheckpointEveryNthDataSegment = outductElementConfigPt.second.get<uint32_t>("ltpCheckpointEveryNthDataSegment", 0); //non-throw version
            outductElementConfig.ltpRandomNumberSizeBits = outductElementConfigPt.second.get<uint32_t>("ltpRandomNumberSizeBits", 0); //non-throw version
            if ((outductElementConfig.ltpRandomNumberSizeBits != 32 ) && (outductElementConfig.ltpRandomNumberSizeBits != 64)) { //not inserted
                std::cerr << "error: ltpRandomNumberSizeBits (" << outductElementConfig.ltpRandomNumberSizeBits << ") must be either 32 or 64" << std::endl;
                return false;
            }
            outductElementConfig.ltpSenderBoundPort = outductElementConfigPt.second.get<uint16_t>("ltpSenderBoundPort", 0); //non-throw version
        }

        if (outductElementConfig.convergenceLayer == "udp") {
            outductElementConfig.udpRateBps = outductElementConfigPt.second.get<uint64_t>("udpRateBps", 15); //non-throw version
        }

        if ((outductElementConfig.convergenceLayer == "stcp") || (outductElementConfig.convergenceLayer == "tcpcl")) {
            outductElementConfig.keepAliveIntervalSeconds = outductElementConfigPt.second.get<uint32_t>("keepAliveIntervalSeconds", 15); //non-throw version
        }

        if (outductElementConfig.convergenceLayer == "tcpcl") {
            outductElementConfig.tcpclAutoFragmentSizeBytes = outductElementConfigPt.second.get<uint64_t>("tcpclAutoFragmentSizeBytes", 200000000); //non-throw version
        }
    }

    return true;
}

OutductsConfig_ptr OutductsConfig::CreateFromJson(const std::string & jsonString) {
    try {
        return OutductsConfig::CreateFromPtree(JsonSerializable::GetPropertyTreeFromJsonString(jsonString));
    }
    catch (boost::property_tree::json_parser_error & e) {
        const std::string message = "In OutductsConfig::CreateFromJson. Error: " + std::string(e.what());
        std::cerr << message << std::endl;
    }

    return OutductsConfig_ptr(); //NULL
}

OutductsConfig_ptr OutductsConfig::CreateFromJsonFile(const std::string & jsonFileName) {
    try {
        return OutductsConfig::CreateFromPtree(JsonSerializable::GetPropertyTreeFromJsonFile(jsonFileName));
    }
    catch (boost::property_tree::json_parser_error & e) {
        const std::string message = "In OutductsConfig::CreateFromJsonFile. Error: " + std::string(e.what());
        std::cerr << message << std::endl;
    }

    return OutductsConfig_ptr(); //NULL
}

OutductsConfig_ptr OutductsConfig::CreateFromPtree(const boost::property_tree::ptree & pt) {

    OutductsConfig_ptr ptrOutductsConfig = boost::make_shared<OutductsConfig>();
    if (!ptrOutductsConfig->SetValuesFromPropertyTree(pt)) {
        ptrOutductsConfig = OutductsConfig_ptr(); //failed, so delete and set it NULL
    }
    return ptrOutductsConfig;
}

boost::property_tree::ptree OutductsConfig::GetNewPropertyTree() const {
    boost::property_tree::ptree pt;
    pt.put("outductConfigName", m_outductConfigName);
    boost::property_tree::ptree & outductElementConfigVectorPt = pt.put_child("outductVector", m_outductElementConfigVector.empty() ? boost::property_tree::ptree("[]") : boost::property_tree::ptree());
    for (outduct_element_config_vector_t::const_iterator outductElementConfigVectorIt = m_outductElementConfigVector.cbegin(); outductElementConfigVectorIt != m_outductElementConfigVector.cend(); ++outductElementConfigVectorIt) {
        const outduct_element_config_t & outductElementConfig = *outductElementConfigVectorIt;
        boost::property_tree::ptree & outductElementConfigPt = (outductElementConfigVectorPt.push_back(std::make_pair("", boost::property_tree::ptree())))->second; //using "" as key creates json array
        outductElementConfigPt.put("name", outductElementConfig.name);
        outductElementConfigPt.put("convergenceLayer", outductElementConfig.convergenceLayer);
        outductElementConfigPt.put("endpointIdStr", outductElementConfig.endpointIdStr);
        outductElementConfigPt.put("remoteHostname", outductElementConfig.remoteHostname);
        outductElementConfigPt.put("remotePort", outductElementConfig.remotePort);
        outductElementConfigPt.put("bundlePipelineLimit", outductElementConfig.bundlePipelineLimit);
        boost::property_tree::ptree & flowIdsPt = outductElementConfigPt.put_child("flowIds", outductElementConfig.flowIds.empty() ? boost::property_tree::ptree("[]") : boost::property_tree::ptree());
        for (std::set<uint64_t>::const_iterator flowIdIt = outductElementConfig.flowIds.cbegin(); flowIdIt != outductElementConfig.flowIds.cend(); ++flowIdIt) {
            flowIdsPt.push_back(std::make_pair("", boost::property_tree::ptree(boost::lexical_cast<std::string>((static_cast<unsigned int>(*flowIdIt)))))); //using "" as key creates json array
        }
        
        if (outductElementConfig.convergenceLayer == "ltp_over_udp") {
            outductElementConfigPt.put("thisLtpEngineId", outductElementConfig.thisLtpEngineId);
            outductElementConfigPt.put("remoteLtpEngineId", outductElementConfig.remoteLtpEngineId);
            outductElementConfigPt.put("ltpDataSegmentMtu", outductElementConfig.ltpDataSegmentMtu);
            outductElementConfigPt.put("oneWayLightTimeMs", outductElementConfig.oneWayLightTimeMs);
            outductElementConfigPt.put("oneWayMarginTimeMs", outductElementConfig.oneWayMarginTimeMs);
            outductElementConfigPt.put("clientServiceId", outductElementConfig.clientServiceId);
            outductElementConfigPt.put("numRxCircularBufferElements", outductElementConfig.numRxCircularBufferElements);
            outductElementConfigPt.put("ltpMaxRetriesPerSerialNumber", outductElementConfig.ltpMaxRetriesPerSerialNumber);
            outductElementConfigPt.put("ltpCheckpointEveryNthDataSegment", outductElementConfig.ltpCheckpointEveryNthDataSegment);
            outductElementConfigPt.put("ltpRandomNumberSizeBits", outductElementConfig.ltpRandomNumberSizeBits);
            outductElementConfigPt.put("ltpSenderBoundPort", outductElementConfig.ltpSenderBoundPort);
        }
        if (outductElementConfig.convergenceLayer == "udp") {
            outductElementConfigPt.put("udpRateBps", outductElementConfig.udpRateBps);
        }
        if ((outductElementConfig.convergenceLayer == "stcp") || (outductElementConfig.convergenceLayer == "tcpcl")) {
            outductElementConfigPt.put("keepAliveIntervalSeconds", outductElementConfig.keepAliveIntervalSeconds);
        }
        if (outductElementConfig.convergenceLayer == "tcpcl") {
            outductElementConfigPt.put("tcpclAutoFragmentSizeBytes", outductElementConfig.tcpclAutoFragmentSizeBytes);
        }
    }

    return pt;
}
