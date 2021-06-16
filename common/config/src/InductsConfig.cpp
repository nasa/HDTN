/***************************************************************************
 * NASA Glenn Research Center, Cleveland, OH
 * Released under the NASA Open Source Agreement (NOSA)
 * May  2021
 ****************************************************************************
 */

#include "InductsConfig.h"
#include <boost/make_shared.hpp>
#include <boost/foreach.hpp>
#include <iostream>

static const std::vector<std::string> VALID_CONVERGENCE_LAYER_NAMES = { "ltp_over_udp", "udp", "stcp", "tcpcl" };

induct_element_config_t::induct_element_config_t() :
    name(""),
    convergenceLayer(""),
    endpointIdStr(""),
    boundPort(0),
    numRxCircularBufferElements(0),
    numRxCircularBufferBytesPerElement(0),
    thisLtpEngineId(0),
    ltpReportSegmentMtu(0),
    oneWayLightTimeMs(0),
    oneWayMarginTimeMs(0),
    clientServiceId(0),
    preallocatedRedDataBytes(0),
    ltpMaxRetriesPerSerialNumber(0),
    keepAliveIntervalSeconds(0) {}
induct_element_config_t::~induct_element_config_t() {}


//a copy constructor: X(const X&)
induct_element_config_t::induct_element_config_t(const induct_element_config_t& o) :
    name(o.name),
    convergenceLayer(o.convergenceLayer),
    endpointIdStr(o.endpointIdStr),
    boundPort(o.boundPort),
    numRxCircularBufferElements(o.numRxCircularBufferElements),
    numRxCircularBufferBytesPerElement(o.numRxCircularBufferBytesPerElement),
    thisLtpEngineId(o.thisLtpEngineId),
    ltpReportSegmentMtu(o.ltpReportSegmentMtu),
    oneWayLightTimeMs(o.oneWayLightTimeMs),
    oneWayMarginTimeMs(o.oneWayMarginTimeMs),
    clientServiceId(o.clientServiceId),
    preallocatedRedDataBytes(o.preallocatedRedDataBytes),
    ltpMaxRetriesPerSerialNumber(o.ltpMaxRetriesPerSerialNumber),
    keepAliveIntervalSeconds(o.keepAliveIntervalSeconds) { }

//a move constructor: X(X&&)
induct_element_config_t::induct_element_config_t(induct_element_config_t&& o) :
    name(std::move(o.name)),
    convergenceLayer(std::move(o.convergenceLayer)),
    endpointIdStr(std::move(o.endpointIdStr)),
    boundPort(o.boundPort),
    numRxCircularBufferElements(o.numRxCircularBufferElements),
    numRxCircularBufferBytesPerElement(o.numRxCircularBufferBytesPerElement),
    thisLtpEngineId(o.thisLtpEngineId),
    ltpReportSegmentMtu(o.ltpReportSegmentMtu),
    oneWayLightTimeMs(o.oneWayLightTimeMs),
    oneWayMarginTimeMs(o.oneWayMarginTimeMs),
    clientServiceId(o.clientServiceId),
    preallocatedRedDataBytes(o.preallocatedRedDataBytes),
    ltpMaxRetriesPerSerialNumber(o.ltpMaxRetriesPerSerialNumber),
    keepAliveIntervalSeconds(o.keepAliveIntervalSeconds) { }

//a copy assignment: operator=(const X&)
induct_element_config_t& induct_element_config_t::operator=(const induct_element_config_t& o) {
    name = o.name;
    convergenceLayer = o.convergenceLayer;
    endpointIdStr = o.endpointIdStr;
    boundPort = o.boundPort;
    numRxCircularBufferElements = o.numRxCircularBufferElements;
    numRxCircularBufferBytesPerElement = o.numRxCircularBufferBytesPerElement;
    thisLtpEngineId = o.thisLtpEngineId;
    ltpReportSegmentMtu = o.ltpReportSegmentMtu;
    oneWayLightTimeMs = o.oneWayLightTimeMs;
    oneWayMarginTimeMs = o.oneWayMarginTimeMs;
    clientServiceId = o.clientServiceId;
    preallocatedRedDataBytes = o.preallocatedRedDataBytes;
    ltpMaxRetriesPerSerialNumber = o.ltpMaxRetriesPerSerialNumber;
    keepAliveIntervalSeconds = o.keepAliveIntervalSeconds;
    return *this;
}

//a move assignment: operator=(X&&)
induct_element_config_t& induct_element_config_t::operator=(induct_element_config_t&& o) {
    name = std::move(o.name);
    convergenceLayer = std::move(o.convergenceLayer);
    endpointIdStr = std::move(o.endpointIdStr);
    boundPort = o.boundPort;
    numRxCircularBufferElements = o.numRxCircularBufferElements;
    numRxCircularBufferBytesPerElement = o.numRxCircularBufferBytesPerElement;
    thisLtpEngineId = o.thisLtpEngineId;
    ltpReportSegmentMtu = o.ltpReportSegmentMtu;
    oneWayLightTimeMs = o.oneWayLightTimeMs;
    oneWayMarginTimeMs = o.oneWayMarginTimeMs;
    clientServiceId = o.clientServiceId;
    preallocatedRedDataBytes = o.preallocatedRedDataBytes;
    ltpMaxRetriesPerSerialNumber = o.ltpMaxRetriesPerSerialNumber;
    keepAliveIntervalSeconds = o.keepAliveIntervalSeconds;
    return *this;
}

bool induct_element_config_t::operator==(const induct_element_config_t & o) const {
    return (name == o.name) &&
        (convergenceLayer == o.convergenceLayer) &&
        (endpointIdStr == o.endpointIdStr) &&
        (boundPort == o.boundPort) &&
        (numRxCircularBufferElements == o.numRxCircularBufferElements) &&
        (numRxCircularBufferBytesPerElement == o.numRxCircularBufferBytesPerElement) &&
        (thisLtpEngineId == o.thisLtpEngineId) &&
        (ltpReportSegmentMtu == o.ltpReportSegmentMtu) &&
        (oneWayLightTimeMs == o.oneWayLightTimeMs) &&
        (oneWayMarginTimeMs == o.oneWayMarginTimeMs) &&
        (clientServiceId == o.clientServiceId) &&
        (preallocatedRedDataBytes == o.preallocatedRedDataBytes) &&
        (ltpMaxRetriesPerSerialNumber == o.ltpMaxRetriesPerSerialNumber) &&
        (keepAliveIntervalSeconds == o.keepAliveIntervalSeconds);
}

InductsConfig::InductsConfig() {
}

InductsConfig::~InductsConfig() {
}

//a copy constructor: X(const X&)
InductsConfig::InductsConfig(const InductsConfig& o) :
    m_inductConfigName(o.m_inductConfigName), m_inductElementConfigVector(o.m_inductElementConfigVector) { }

//a move constructor: X(X&&)
InductsConfig::InductsConfig(InductsConfig&& o) :
    m_inductConfigName(std::move(o.m_inductConfigName)), m_inductElementConfigVector(std::move(o.m_inductElementConfigVector)) { }

//a copy assignment: operator=(const X&)
InductsConfig& InductsConfig::operator=(const InductsConfig& o) {
    m_inductConfigName = o.m_inductConfigName;
    m_inductElementConfigVector = o.m_inductElementConfigVector;
    return *this;
}

//a move assignment: operator=(X&&)
InductsConfig& InductsConfig::operator=(InductsConfig&& o) {
    m_inductConfigName = std::move(o.m_inductConfigName);
    m_inductElementConfigVector = std::move(o.m_inductElementConfigVector);
    return *this;
}

bool InductsConfig::operator==(const InductsConfig & o) const {
    return (m_inductConfigName == o.m_inductConfigName) && (m_inductElementConfigVector == o.m_inductElementConfigVector);
}

bool InductsConfig::SetValuesFromPropertyTree(const boost::property_tree::ptree & pt) {
    m_inductConfigName = pt.get<std::string>("inductConfigName", ""); //non-throw version
    if (m_inductConfigName == "") {
        std::cerr << "error: inductConfigName must be defined and not empty string\n";
        return false;
    }
    const boost::property_tree::ptree & inductElementConfigVectorPt = pt.get_child("inductVector", boost::property_tree::ptree()); //non-throw version
    m_inductElementConfigVector.resize(inductElementConfigVectorPt.size());
    unsigned int vectorIndex = 0;
    BOOST_FOREACH(const boost::property_tree::ptree::value_type & inductElementConfigPt, inductElementConfigVectorPt) {
        induct_element_config_t & inductElementConfig = m_inductElementConfigVector[vectorIndex++];
        inductElementConfig.name = inductElementConfigPt.second.get<std::string>("name", "unnamed_induct"); //non-throw version
        inductElementConfig.convergenceLayer = inductElementConfigPt.second.get<std::string>("convergenceLayer", "unnamed_convergence_layer"); //non-throw version
        {
            bool found = false;
            for (std::vector<std::string>::const_iterator it = VALID_CONVERGENCE_LAYER_NAMES.cbegin(); it != VALID_CONVERGENCE_LAYER_NAMES.cend(); ++it) {
                if (inductElementConfig.convergenceLayer == *it) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                std::cerr << "error: invalid convergence layer " << inductElementConfig.convergenceLayer << std::endl;
                return false;
            }
        }
        inductElementConfig.endpointIdStr = inductElementConfigPt.second.get<std::string>("endpointIdStr", "unnamed_endpoint_id"); //non-throw version
        inductElementConfig.boundPort = inductElementConfigPt.second.get<uint16_t>("boundPort", 0); //non-throw version
        if (inductElementConfig.boundPort == 0) {
            std::cerr << "error: invalid boundPort must be non-zero";
            return false;
        }
        inductElementConfig.numRxCircularBufferElements = inductElementConfigPt.second.get<uint32_t>("numRxCircularBufferElements", 100); //non-throw version
        inductElementConfig.numRxCircularBufferBytesPerElement = inductElementConfigPt.second.get<uint32_t>("numRxCircularBufferBytesPerElement", UINT16_MAX); //non-throw version

        if (inductElementConfig.convergenceLayer == "ltp_over_udp") {
            inductElementConfig.thisLtpEngineId = inductElementConfigPt.second.get<uint64_t>("thisLtpEngineId", 0); //non-throw version
            inductElementConfig.ltpReportSegmentMtu = inductElementConfigPt.second.get<uint32_t>("ltpReportSegmentMtu", 1000); //non-throw version
            inductElementConfig.oneWayLightTimeMs = inductElementConfigPt.second.get<uint64_t>("oneWayLightTimeMs", 1000); //non-throw version
            inductElementConfig.oneWayMarginTimeMs = inductElementConfigPt.second.get<uint64_t>("oneWayMarginTimeMs", 0); //non-throw version
            inductElementConfig.clientServiceId = inductElementConfigPt.second.get<uint64_t>("clientServiceId", 0); //non-throw version
            inductElementConfig.preallocatedRedDataBytes = inductElementConfigPt.second.get<uint64_t>("preallocatedRedDataBytes", 100); //non-throw version
            inductElementConfig.ltpMaxRetriesPerSerialNumber = inductElementConfigPt.second.get<uint32_t>("ltpMaxRetriesPerSerialNumber", 5); //non-throw version
        }

        if ((inductElementConfig.convergenceLayer == "stcp") || (inductElementConfig.convergenceLayer == "tcpcl")) {
            inductElementConfig.keepAliveIntervalSeconds = inductElementConfigPt.second.get<uint32_t>("keepAliveIntervalSeconds", 15); //non-throw version
        }
    }

    return true;
}

InductsConfig_ptr InductsConfig::CreateFromJson(const std::string & jsonString) {
    try {
        return InductsConfig::CreateFromPtree(JsonSerializable::GetPropertyTreeFromJsonString(jsonString));
    }
    catch (boost::property_tree::json_parser_error & e) {
        const std::string message = "In InductsConfig::CreateFromJson. Error: " + std::string(e.what());
        std::cerr << message << std::endl;
    }

    return InductsConfig_ptr(); //NULL
}

InductsConfig_ptr InductsConfig::CreateFromJsonFile(const std::string & jsonFileName) {
    try {
        return InductsConfig::CreateFromPtree(JsonSerializable::GetPropertyTreeFromJsonFile(jsonFileName));
    }
    catch (boost::property_tree::json_parser_error & e) {
        const std::string message = "In InductsConfig::CreateFromJsonFile. Error: " + std::string(e.what());
        std::cerr << message << std::endl;
    }

    return InductsConfig_ptr(); //NULL
}

InductsConfig_ptr InductsConfig::CreateFromPtree(const boost::property_tree::ptree & pt) {

    InductsConfig_ptr ptrInductsConfig = boost::make_shared<InductsConfig>();
    if (!ptrInductsConfig->SetValuesFromPropertyTree(pt)) {
        ptrInductsConfig = InductsConfig_ptr(); //failed, so delete and set it NULL
    }
    return ptrInductsConfig;
}

boost::property_tree::ptree InductsConfig::GetNewPropertyTree() const {
    boost::property_tree::ptree pt;
    pt.put("inductConfigName", m_inductConfigName);
    boost::property_tree::ptree & inductElementConfigVectorPt = pt.put_child("inductVector", m_inductElementConfigVector.empty() ? boost::property_tree::ptree("[]") : boost::property_tree::ptree());
    for (induct_element_config_vector_t::const_iterator inductElementConfigVectorIt = m_inductElementConfigVector.cbegin(); inductElementConfigVectorIt != m_inductElementConfigVector.cend(); ++inductElementConfigVectorIt) {
        const induct_element_config_t & inductElementConfig = *inductElementConfigVectorIt;
        boost::property_tree::ptree & inductElementConfigPt = (inductElementConfigVectorPt.push_back(std::make_pair("", boost::property_tree::ptree())))->second; //using "" as key creates json array
        inductElementConfigPt.put("name", inductElementConfig.name);
        inductElementConfigPt.put("convergenceLayer", inductElementConfig.convergenceLayer);
        inductElementConfigPt.put("endpointIdStr", inductElementConfig.endpointIdStr);
        inductElementConfigPt.put("boundPort", inductElementConfig.boundPort);
        inductElementConfigPt.put("numRxCircularBufferElements", inductElementConfig.numRxCircularBufferElements);
        inductElementConfigPt.put("numRxCircularBufferBytesPerElement", inductElementConfig.numRxCircularBufferBytesPerElement);
        if (inductElementConfig.convergenceLayer == "ltp_over_udp") {
            inductElementConfigPt.put("thisLtpEngineId", inductElementConfig.thisLtpEngineId);
            inductElementConfigPt.put("ltpReportSegmentMtu", inductElementConfig.ltpReportSegmentMtu);
            inductElementConfigPt.put("oneWayLightTimeMs", inductElementConfig.oneWayLightTimeMs);
            inductElementConfigPt.put("oneWayMarginTimeMs", inductElementConfig.oneWayMarginTimeMs);
            inductElementConfigPt.put("clientServiceId", inductElementConfig.clientServiceId);
            inductElementConfigPt.put("preallocatedRedDataBytes", inductElementConfig.preallocatedRedDataBytes);
            inductElementConfigPt.put("ltpMaxRetriesPerSerialNumber", inductElementConfig.ltpMaxRetriesPerSerialNumber);
        }
        if ((inductElementConfig.convergenceLayer == "stcp") || (inductElementConfig.convergenceLayer == "tcpcl")) {
            inductElementConfigPt.put("keepAliveIntervalSeconds", inductElementConfig.keepAliveIntervalSeconds);
        }
    }

    return pt;
}
