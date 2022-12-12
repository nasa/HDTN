/**
 * @file JsonSerializable.h
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
 *
 * @section DESCRIPTION
 *
 * This JsonSerializable virtual base class provides methods to 
 * use a boost::property_tree::ptree for the serialization and deserialization
 * of C++ classes between JSON or XML.
 * Inheriting from this class helps to overcome some of the limitations
 * of property_tree such as C++ numerical values being serialized into JSON strings.
 */

#ifndef JSON_SERIALIZABLE_H
#define JSON_SERIALIZABLE_H 1

#include <string>
#include <set>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser/error.hpp>
#include <boost/version.hpp>
#include "hdtn_util_export.h"

//since boost versions below 1.59 use boost spirit classic to do json parsing, and boost spirit is not thread safe by default,
//we need to make sure that BOOST_SPIRIT_THREADSAFE is defined globally if using boost version 1.58 and below
//to prevent runtime errors in a multi-threaded environment.
#if BOOST_VERSION < 105900 && !defined(BOOST_SPIRIT_THREADSAFE)
#error "Boost version is below 1.59.0 and BOOST_SPIRIT_THREADSAFE is not defined"
#endif


class HDTN_UTIL_EXPORT JsonSerializable {
public:
    static bool LoadTextFileIntoString(const std::string& fileName, std::string& fileContentsAsString);
    static void GetAllJsonKeys(const std::string& jsonText, std::set<std::string> & jsonKeysNoQuotesSetToAppend);
    static void GetAllJsonKeysLineByLine(std::istream& stream, std::set<std::string>& jsonKeysNoQuotesSetToAppend);
    static bool HasUnusedJsonVariablesInFile(const JsonSerializable& config, const std::string& originalUserJsonFileName, std::string& returnedErrorMessage);
    static bool HasUnusedJsonVariablesInString(const JsonSerializable& config, const std::string& originalUserJsonString, std::string& returnedErrorMessage);
    static bool HasUnusedJsonVariablesInStream(const JsonSerializable& config, std::istream& originalUserJsonStream, std::string& returnedErrorMessage);
    
    //warning: reading [] from Json then writing back out using this function will replace the [] with "", example: "inductVector": ""
    static std::string PtToJsonString(const boost::property_tree::ptree& pt, bool pretty = true);

    std::string ToJson(bool pretty = true) const;
    bool ToJsonFile(const std::string & fileName, bool pretty = true) const;
    static bool GetPropertyTreeFromJsonCharArray(char* data, const std::size_t size, boost::property_tree::ptree& pt);
    static bool GetPropertyTreeFromJsonStream(std::istream& jsonStream, boost::property_tree::ptree& pt);
    static bool GetPropertyTreeFromJsonString(const std::string & jsonStr, boost::property_tree::ptree& pt);
    static bool GetPropertyTreeFromJsonFile(const std::string & jsonFileName, boost::property_tree::ptree& pt);

    static std::string PtToXmlString(const boost::property_tree::ptree& pt);
    std::string ToXml() const;
    bool ToXmlFile(const std::string & fileName, char indentCharacter = ' ', int indentCount = 2) const;
    static boost::property_tree::ptree GetPropertyTreeFromXmlString(const std::string & jsonStr);
    static boost::property_tree::ptree GetPropertyTreeFromXmlFile(const std::string & xmlFileName);

    virtual boost::property_tree::ptree GetNewPropertyTree() const = 0;
    virtual bool SetValuesFromPropertyTree(const boost::property_tree::ptree & pt) = 0;
    bool SetValuesFromJson(const std::string & jsonString);


protected:
    JsonSerializable();
};

#endif // JSON_SERIALIZABLE_H

