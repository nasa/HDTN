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
#include "Logger.h"
#include "config_lib_export.h"

struct storage_disk_config_t {
    std::string name;
    std::string storeFilePath;

    CONFIG_LIB_EXPORT storage_disk_config_t();
    CONFIG_LIB_EXPORT ~storage_disk_config_t();

    CONFIG_LIB_EXPORT storage_disk_config_t(const std::string & paramName, const std::string & paramStoreFilePath);
    CONFIG_LIB_EXPORT bool operator==(const storage_disk_config_t & other) const;


    //a copy constructor: X(const X&)
    CONFIG_LIB_EXPORT storage_disk_config_t(const storage_disk_config_t& o);

    //a move constructor: X(X&&)
    CONFIG_LIB_EXPORT storage_disk_config_t(storage_disk_config_t&& o);

    //a copy assignment: operator=(const X&)
    CONFIG_LIB_EXPORT storage_disk_config_t& operator=(const storage_disk_config_t& o);

    //a move assignment: operator=(X&&)
    CONFIG_LIB_EXPORT storage_disk_config_t& operator=(storage_disk_config_t&& o);
};

typedef std::vector<storage_disk_config_t> storage_disk_config_vector_t;



class StorageConfig;
typedef boost::shared_ptr<StorageConfig> StorageConfig_ptr;

class StorageConfig : public JsonSerializable {


public:
    CONFIG_LIB_EXPORT StorageConfig();
    CONFIG_LIB_EXPORT ~StorageConfig();

    //a copy constructor: X(const X&)
    CONFIG_LIB_EXPORT StorageConfig(const StorageConfig& o);

    //a move constructor: X(X&&)
    CONFIG_LIB_EXPORT StorageConfig(StorageConfig&& o);

    //a copy assignment: operator=(const X&)
    CONFIG_LIB_EXPORT StorageConfig& operator=(const StorageConfig& o);

    //a move assignment: operator=(X&&)
    CONFIG_LIB_EXPORT StorageConfig& operator=(StorageConfig&& o);

    CONFIG_LIB_EXPORT bool operator==(const StorageConfig & other) const;

    CONFIG_LIB_EXPORT static StorageConfig_ptr CreateFromPtree(const boost::property_tree::ptree & pt);
    CONFIG_LIB_EXPORT static StorageConfig_ptr CreateFromJson(const std::string & jsonString);
    CONFIG_LIB_EXPORT static StorageConfig_ptr CreateFromJsonFile(const std::string & jsonFileName);
    CONFIG_LIB_EXPORT virtual boost::property_tree::ptree GetNewPropertyTree() const;
    CONFIG_LIB_EXPORT virtual bool SetValuesFromPropertyTree(const boost::property_tree::ptree & pt);

    CONFIG_LIB_EXPORT void AddDisk(const std::string & name, const std::string & storeFilePath);
public:

    std::string m_storageImplementation;
    bool m_tryToRestoreFromDisk;
    bool m_autoDeleteFilesOnExit;
    uint64_t m_totalStorageCapacityBytes;
    storage_disk_config_vector_t m_storageDiskConfigVector;
};

#endif // STORAGE_CONFIG_H

