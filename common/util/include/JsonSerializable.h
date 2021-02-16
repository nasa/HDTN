
#ifndef JSON_SERIALIZABLE_H
#define JSON_SERIALIZABLE_H

#include <string>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include <boost/version.hpp>

//since boost versions below 1.59 use boost spirit classic to do json parsing, and boost spirit is not thread safe by default,
//we need to make sure that BOOST_SPIRIT_THREADSAFE is defined globally if using boost version 1.58 and below
//to prevent runtime errors in a multi-threaded environment.
#if BOOST_VERSION < 105900 && !defined(BOOST_SPIRIT_THREADSAFE)
#error "Boost version is below 1.59.0 and BOOST_SPIRIT_THREADSAFE is not defined"
#endif


class JsonSerializable {
public:
	std::string ToJson(bool pretty = true);
	bool ToJsonFile(const std::string & fileName, bool pretty = true);
	static boost::property_tree::ptree GetPropertyTreeFromJsonString(const std::string & jsonStr);
	static boost::property_tree::ptree GetPropertyTreeFromJsonFile(const std::string & jsonFileName);

	std::string ToXml();
	bool ToXmlFile(const std::string & fileName, char indentCharacter = ' ', int indentCount = 2);
	static boost::property_tree::ptree GetPropertyTreeFromXmlString(const std::string & jsonStr);
	static boost::property_tree::ptree GetPropertyTreeFromXmlFile(const std::string & xmlFileName);

	virtual boost::property_tree::ptree GetNewPropertyTree() const = 0;
	virtual bool SetValuesFromPropertyTree(const boost::property_tree::ptree & pt) = 0;
	bool SetValuesFromJson(const std::string & jsonString);


protected:
	JsonSerializable();
};

#endif // JSON_SERIALIZABLE_H

