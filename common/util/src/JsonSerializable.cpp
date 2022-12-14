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
#include <boost/filesystem/fstream.hpp>
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

//https://stackoverflow.com/questions/43778916/regex-to-find-keys-in-json
//Regex to match any word character that may be between quotes(" ") succeed by a : (ignoring whitespaces).
//"([^"]+?)"\s*:
static const boost::regex regexMatchAllJsonKeys("\\\"([^\"]+?)\\\"\\s*:");
void JsonSerializable::GetAllJsonKeys(const std::string& jsonText, std::set<std::string>& jsonKeysNoQuotesSetToAppend) {
    for (boost::sregex_iterator it(jsonText.begin(), jsonText.end(), regexMatchAllJsonKeys), words_end; it != words_end; ++it) {
        const boost::smatch & match = *it;
        jsonKeysNoQuotesSetToAppend.emplace(match[1].str());
    }
}
void JsonSerializable::GetAllJsonKeysLineByLine(std::istream& stream, std::set<std::string>& jsonKeysNoQuotesSetToAppend) {
    std::string lineOfJsonText;
    while (std::getline(stream, lineOfJsonText)) {
        GetAllJsonKeys(lineOfJsonText, jsonKeysNoQuotesSetToAppend);
    }
}
bool JsonSerializable::HasUnusedJsonVariablesInFilePath(const JsonSerializable& config, const boost::filesystem::path& originalUserJsonFilePath, std::string& returnedErrorMessage) {
    boost::filesystem::ifstream ifs(originalUserJsonFilePath);
    if (!ifs.good()) {
        returnedErrorMessage = "cannot load originalUserJsonFileName: " + originalUserJsonFilePath.string();
        return false;
    }
    return HasUnusedJsonVariablesInStream(config, ifs, returnedErrorMessage);
}
bool JsonSerializable::HasUnusedJsonVariablesInString(const JsonSerializable& config, const std::string& originalUserJsonString, std::string& returnedErrorMessage) {
    std::istringstream iss(originalUserJsonString);
    return HasUnusedJsonVariablesInStream(config, iss, returnedErrorMessage);
}
bool JsonSerializable::HasUnusedJsonVariablesInStream(const JsonSerializable& config, std::istream& originalUserJsonStream, std::string& returnedErrorMessage) {
    returnedErrorMessage.clear();

    //get a set of only valid keys
    const std::string jsonNoUnusedVars = config.ToJson();
    std::set<std::string> validKeysSet; //support out of order
    JsonSerializable::GetAllJsonKeys(jsonNoUnusedVars, validKeysSet);


    std::string linePotentiallyUnused;
    unsigned int lineNumber = 0;
    while (std::getline(originalUserJsonStream, linePotentiallyUnused).good()) {
        ++lineNumber;

        //get a set of the json keys on the current line of text of the user's json file
        std::set<std::string> jsonKeysOnThisLineSet; //support out of order
        JsonSerializable::GetAllJsonKeys(linePotentiallyUnused, jsonKeysOnThisLineSet);

        //interate through the keys on this line, check them against the "all valid keys set"
        for (std::set<std::string>::const_iterator it = jsonKeysOnThisLineSet.cbegin(); it != jsonKeysOnThisLineSet.cend(); ++it) {
            const std::string& thisKeyPotentiallyNotNeeded = *it;
            if (validKeysSet.count(thisKeyPotentiallyNotNeeded) == 0) {
                returnedErrorMessage = "line " + boost::lexical_cast<std::string>(lineNumber) + ": unused JSON key: " + thisKeyPotentiallyNotNeeded;
                return true;
            }
        }
    }
    return false;
}

bool JsonSerializable::LoadTextFileIntoString(const boost::filesystem::path& filePath, std::string& fileContentsAsString) {
    boost::filesystem::ifstream ifs(filePath);

    if (!ifs.good()) {
        return false;
    }

    // get length of file:
    ifs.seekg(0, ifs.end);
    std::size_t length = ifs.tellg();
    ifs.seekg(0, ifs.beg);
    fileContentsAsString.resize(length, ' ');
    ifs.read(&fileContentsAsString[0], length);
    ifs.close();
    return true;
}

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

bool JsonSerializable::ToJsonFile(const boost::filesystem::path& filePath, bool pretty) const {
    boost::filesystem::ofstream out(filePath);
    if (!out.good()) {
        LOG_ERROR(subprocess) << "In JsonSerializable::ToJsonFile. Error opening file for writing: " << filePath;
        return false;
    }
    out << ToJson(pretty);

    out.close();

    return true;
}

bool JsonSerializable::GetPropertyTreeFromJsonCharArray(char * data, const std::size_t size, boost::property_tree::ptree& pt) {
    //https://stackoverflow.com/questions/7781898/get-an-istream-from-a-char
    struct membuf : std::streambuf
    {
        membuf(char* begin, char* end) {
            this->setg(begin, begin, end);
        }
    };
    membuf sbuf(data, data + size);
    std::istream is(&sbuf);
    return GetPropertyTreeFromJsonStream(is, pt);
}

bool JsonSerializable::GetPropertyTreeFromJsonStream(std::istream& jsonStream, boost::property_tree::ptree& pt) {
    try {
        boost::property_tree::read_json(jsonStream, pt);
    }
    catch (const boost::property_tree::json_parser::json_parser_error & e) {
        LOG_ERROR(subprocess) << "Error parsing JSON: " << e.what();
        return false;
    }
    return true;
}

bool JsonSerializable::GetPropertyTreeFromJsonString(const std::string & jsonStr, boost::property_tree::ptree& pt) {
    std::istringstream iss(jsonStr);
    return GetPropertyTreeFromJsonStream(iss, pt);
}

bool JsonSerializable::GetPropertyTreeFromJsonFilePath(const boost::filesystem::path& jsonFilePath, boost::property_tree::ptree& pt) {
    boost::filesystem::ifstream ifs(jsonFilePath);

    if (!ifs.good()) {
        LOG_ERROR(subprocess) << "Error loading JSON file: " << jsonFilePath;
        return false;
    }

    return GetPropertyTreeFromJsonStream(ifs, pt);
}

std::string JsonSerializable::PtToXmlString(const boost::property_tree::ptree& pt) {
    std::ostringstream oss;
    static const boost::property_tree::xml_writer_settings<std::string> settings = boost::property_tree::xml_writer_make_settings<std::string>(' ', 2);
    boost::property_tree::write_xml(oss, pt, settings);
    return oss.str();
}
std::string JsonSerializable::ToXml() const {
    return PtToXmlString(GetNewPropertyTree()); //virtual function call
}

bool JsonSerializable::ToXmlFile(const std::string & fileName, char indentCharacter, int indentCount) const {
    static const boost::property_tree::xml_writer_settings<std::string> settings = boost::property_tree::xml_writer_make_settings<std::string>(indentCharacter, indentCount);
    try {
        boost::property_tree::write_xml(fileName, GetNewPropertyTree(), std::locale(), settings);
    }
    catch (const boost::property_tree::xml_parser_error & e) {
        LOG_ERROR(subprocess) << "In JsonSerializable::ToXmlFile. Error: " << e.what();
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
    boost::property_tree::ptree pt;
    if (!GetPropertyTreeFromJsonString(jsonString, pt)) {
        return false; //prints message
    }
    return SetValuesFromPropertyTree(pt); //virtual function call
}
