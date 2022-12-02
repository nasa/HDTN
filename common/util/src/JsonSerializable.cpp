/**
 * @file JsonSerializable.cpp
 * @author  Brian Tomko <brian.j.tomko@nasa.gov>
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

#include "JsonSerializable.h"
#include "Logger.h"
#include <sstream>
#include <boost/regex.hpp>
//since boost versions below 1.76 use deprecated bind.hpp in its property_tree/json_parser/detail/parser.hpp,
//and since BOOST_BIND_GLOBAL_PLACEHOLDERS was introduced in 1.73
//the following fixes warning:  The practice of declaring the Bind placeholders (_1, _2, ...) in the global namespace is deprecated....
//this fixes the warning caused by boost/property_tree/json_parser/detail/parser.hpp
#if (BOOST_VERSION < 107600) && (BOOST_VERSION >= 107300) && !defined(BOOST_BIND_GLOBAL_PLACEHOLDERS)
#define BOOST_BIND_GLOBAL_PLACEHOLDERS
#endif
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/xml_parser.hpp>


static constexpr hdtn::Logger::SubProcess subprocess = hdtn::Logger::SubProcess::none;

//regex to match "number", "true", "false", "{}", or "[]" (that do not precede a colon) and remove the quotes around them
static const boost::regex regexMatchInQuotes("\\\"(-?\\d*\\.{0,1}\\d+|true|false|\\{\\}|\\[\\])\\\"(?!:)"); //, boost::regex_constants::icase); //json boolean is lower case only

JsonSerializable::JsonSerializable() {
}

std::string JsonSerializable::PtToJsonString(const boost::property_tree::ptree& pt, bool pretty) {
    std::ostringstream oss;
    boost::property_tree::write_json(oss, pt, pretty);
    return boost::regex_replace(oss.str(), regexMatchInQuotes, "$1");
}

std::string JsonSerializable::ToJson(bool pretty) const {
    return PtToJsonString(GetNewPropertyTree(), pretty);
}

bool JsonSerializable::ToJsonFile(const std::string & fileName, bool pretty) const {
    std::ofstream out(fileName);
    if (!out.good()) {
        const std::string message = "In JsonSerializable::ToJsonFile. Error opening file for writing: " + fileName;
        LOG_ERROR(subprocess) << message;
        return false;
    }
    out << ToJson(pretty);

    out.close();

    return true;
}

boost::property_tree::ptree JsonSerializable::GetPropertyTreeFromCharArray(char * data, const std::size_t size) {
    //https://stackoverflow.com/questions/7781898/get-an-istream-from-a-char
    struct membuf : std::streambuf
    {
        membuf(char* begin, char* end) {
            this->setg(begin, begin, end);
        }
    };
    membuf sbuf(data, data + size);
    std::istream is(&sbuf);
    boost::property_tree::ptree pt;
    try {
        boost::property_tree::read_json(is, pt);
    }
    catch (boost::property_tree::json_parser::json_parser_error e) {
        LOG_ERROR(subprocess) << "In " << __FUNCTION__ << ": Error parsing JSON String.  jsonStr: " << data;
    }
    return pt;
}

boost::property_tree::ptree JsonSerializable::GetPropertyTreeFromJsonString(const std::string & jsonStr) {
    std::istringstream iss(jsonStr);
    boost::property_tree::ptree pt;
    try {
        boost::property_tree::read_json(iss, pt);
    }
    catch (boost::property_tree::json_parser::json_parser_error e) {
        LOG_ERROR(subprocess) << "In " << __FUNCTION__ << ": Error parsing JSON String.  jsonStr: " << jsonStr;
    }
    return pt;
}

boost::property_tree::ptree JsonSerializable::GetPropertyTreeFromJsonFile(const std::string & jsonFileName) {
    boost::property_tree::ptree pt;
    boost::property_tree::read_json(jsonFileName, pt);
    return pt;
}

std::string JsonSerializable::PtToXmlString(const boost::property_tree::ptree& pt) {
    std::ostringstream oss;
    static const boost::property_tree::xml_writer_settings<std::string> settings = boost::property_tree::xml_writer_make_settings<std::string>(' ', 2);
    boost::property_tree::write_xml(oss, pt, settings);
    return oss.str();
}
std::string JsonSerializable::ToXml() const {
    return PtToXmlString(GetNewPropertyTree());
}

bool JsonSerializable::ToXmlFile(const std::string & fileName, char indentCharacter, int indentCount) const {
    static const boost::property_tree::xml_writer_settings<std::string> settings = boost::property_tree::xml_writer_make_settings<std::string>(indentCharacter, indentCount);
    try {
        boost::property_tree::write_xml(fileName, GetNewPropertyTree(), std::locale(), settings);
    }
    catch (boost::property_tree::xml_parser_error & e) {
        const std::string message = "In JsonSerializable::ToXmlFile. Error: " + std::string(e.what());
        LOG_ERROR(subprocess) << message;
        return false;
    }

    return true;
}

boost::property_tree::ptree JsonSerializable::GetPropertyTreeFromXmlString(const std::string & jsonStr) {
    std::istringstream iss(jsonStr);
    boost::property_tree::ptree pt;
    boost::property_tree::read_xml(iss, pt, boost::property_tree::xml_parser::trim_whitespace);
    return pt;
}

boost::property_tree::ptree JsonSerializable::GetPropertyTreeFromXmlFile(const std::string & xmlFileName) {
    boost::property_tree::ptree pt;
    boost::property_tree::read_xml(xmlFileName, pt, boost::property_tree::xml_parser::trim_whitespace);
    return pt;
}

bool JsonSerializable::SetValuesFromJson(const std::string & jsonString) {
    try {
        return SetValuesFromPropertyTree(GetPropertyTreeFromJsonString(jsonString));
    }
    catch (boost::property_tree::json_parser::json_parser_error & e) {
        const std::string message = "In JsonSerializable::SetValuesFromJson. Error: " + std::string(e.what());
        LOG_ERROR(subprocess) << message;
    }
    return false;
}
