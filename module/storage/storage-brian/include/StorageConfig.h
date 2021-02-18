#ifndef STORAGE_CONFIG_H
#define STORAGE_CONFIG_H 1

#include <string>
#include <boost/shared_ptr.hpp>
#include <boost/integer.hpp>
#include <map>
#include <vector>
#include <utility>
#include <tuple>
#include "JsonSerializable.h"


	

struct storage_disk_config_t {
	storage_disk_config_t();
	~storage_disk_config_t();

	std::string name;
	std::string storeFilePath;

	storage_disk_config_t(const std::string & paramName, const std::string & paramStoreFilePath);
	bool operator==(const storage_disk_config_t & other) const;
};

typedef std::vector<storage_disk_config_t> storage_disk_config_vector_t;



class StorageConfig;
typedef boost::shared_ptr<StorageConfig> StorageConfig_ptr;

class StorageConfig : public JsonSerializable {


public:
	StorageConfig();
	~StorageConfig();

	bool operator==(const StorageConfig & other) const;
	
	static StorageConfig_ptr CreateFromPtree(const boost::property_tree::ptree & pt);
	static StorageConfig_ptr CreateFromJson(const std::string & jsonString);
	static StorageConfig_ptr CreateFromJsonFile(const std::string & jsonFileName);
	virtual boost::property_tree::ptree GetNewPropertyTree() const;
	virtual bool SetValuesFromPropertyTree(const boost::property_tree::ptree & pt);

	void AddDisk(const std::string & name, const std::string & storeFilePath);
public:
	
	boost::uint64_t m_totalStorageCapacityBytes;
	storage_disk_config_vector_t m_storageDiskConfigVector;
};

#endif // STORAGE_CONFIG_H

