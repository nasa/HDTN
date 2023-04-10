/**
 * @file StorageConfig.h
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
 * The StorageConfig class contains all the config parameters for
 * instantiating a single HDTN storage module, and it
 * provides JSON serialization and deserialization capability.
 */

#ifndef STORAGE_CONFIG_H
#define STORAGE_CONFIG_H 1

#include <string>
#include <memory>
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
    CONFIG_LIB_EXPORT storage_disk_config_t(storage_disk_config_t&& o) noexcept;

    //a copy assignment: operator=(const X&)
    CONFIG_LIB_EXPORT storage_disk_config_t& operator=(const storage_disk_config_t& o);

    //a move assignment: operator=(X&&)
    CONFIG_LIB_EXPORT storage_disk_config_t& operator=(storage_disk_config_t&& o) noexcept;
};

typedef std::vector<storage_disk_config_t> storage_disk_config_vector_t;



class StorageConfig;
typedef std::shared_ptr<StorageConfig> StorageConfig_ptr;

class StorageConfig : public JsonSerializable {


public:
    CONFIG_LIB_EXPORT StorageConfig();
    CONFIG_LIB_EXPORT ~StorageConfig();

    //a copy constructor: X(const X&)
    CONFIG_LIB_EXPORT StorageConfig(const StorageConfig& o);

    //a move constructor: X(X&&)
    CONFIG_LIB_EXPORT StorageConfig(StorageConfig&& o) noexcept;

    //a copy assignment: operator=(const X&)
    CONFIG_LIB_EXPORT StorageConfig& operator=(const StorageConfig& o);

    //a move assignment: operator=(X&&)
    CONFIG_LIB_EXPORT StorageConfig& operator=(StorageConfig&& o) noexcept;

    CONFIG_LIB_EXPORT bool operator==(const StorageConfig & other) const;

    CONFIG_LIB_EXPORT static StorageConfig_ptr CreateFromPtree(const boost::property_tree::ptree & pt);
    CONFIG_LIB_EXPORT static StorageConfig_ptr CreateFromJson(const std::string & jsonString, bool verifyNoUnusedJsonKeys = true);
    CONFIG_LIB_EXPORT static StorageConfig_ptr CreateFromJsonFilePath(const boost::filesystem::path& jsonFilePath, bool verifyNoUnusedJsonKeys = true);
    CONFIG_LIB_EXPORT virtual boost::property_tree::ptree GetNewPropertyTree() const override;
    CONFIG_LIB_EXPORT virtual bool SetValuesFromPropertyTree(const boost::property_tree::ptree & pt) override;

    CONFIG_LIB_EXPORT void AddDisk(const std::string & name, const std::string & storeFilePath);
public:

    std::string m_storageImplementation;
    bool m_tryToRestoreFromDisk;
    bool m_autoDeleteFilesOnExit;
    uint64_t m_totalStorageCapacityBytes;
    std::string m_storageDeletionPolicy;
    storage_disk_config_vector_t m_storageDiskConfigVector;
};

#endif // STORAGE_CONFIG_H

