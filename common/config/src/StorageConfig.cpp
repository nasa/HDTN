/**
 * @file StorageConfig.cpp
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

#include "StorageConfig.h"
#include "Logger.h"
#include <memory>
#include <boost/foreach.hpp>

static constexpr hdtn::Logger::SubProcess subprocess = hdtn::Logger::SubProcess::none;

static const std::vector<std::string> VALID_STORAGE_IMPLEMENTATION_NAMES = { "stdio_multi_threaded", "asio_single_threaded" };
static const std::vector<std::string> VALID_STORAGE_DELETION_POLICIES = { "never", "on_expiration", "on_storage_full" };

storage_disk_config_t::storage_disk_config_t() : name(""), storeFilePath("") {}
storage_disk_config_t::~storage_disk_config_t() {}

storage_disk_config_t::storage_disk_config_t(const std::string & paramName, const std::string & paramStoreFilePath) :
    name(paramName), storeFilePath(paramStoreFilePath) {}

//a copy constructor: X(const X&)
storage_disk_config_t::storage_disk_config_t(const storage_disk_config_t& o) :
    name(o.name), storeFilePath(o.storeFilePath) { }

//a move constructor: X(X&&)
storage_disk_config_t::storage_disk_config_t(storage_disk_config_t&& o) noexcept :
    name(std::move(o.name)), storeFilePath(std::move(o.storeFilePath)) { }

//a copy assignment: operator=(const X&)
storage_disk_config_t& storage_disk_config_t::operator=(const storage_disk_config_t& o) {
    name = o.name;
    storeFilePath = o.storeFilePath;
    return *this;
}

//a move assignment: operator=(X&&)
storage_disk_config_t& storage_disk_config_t::operator=(storage_disk_config_t&& o) noexcept {
    name = std::move(o.name);
    storeFilePath = std::move(o.storeFilePath);
    return *this;
}

bool storage_disk_config_t::operator==(const storage_disk_config_t & other) const {
    return (name == other.name) && (storeFilePath == other.storeFilePath);
}

StorageConfig::StorageConfig() :
    m_storageImplementation("stdio_multi_threaded"),
    m_tryToRestoreFromDisk(false),
    m_autoDeleteFilesOnExit(true),
    m_totalStorageCapacityBytes(1),
    m_storageDeletionPolicy("never"),
    m_storageDiskConfigVector() { }

StorageConfig::~StorageConfig() {
}

//a copy constructor: X(const X&)
StorageConfig::StorageConfig(const StorageConfig& o) :
    m_storageImplementation(o.m_storageImplementation),
    m_tryToRestoreFromDisk(o.m_tryToRestoreFromDisk),
    m_autoDeleteFilesOnExit(o.m_autoDeleteFilesOnExit),
    m_totalStorageCapacityBytes(o.m_totalStorageCapacityBytes),
    m_storageDeletionPolicy(o.m_storageDeletionPolicy),
    m_storageDiskConfigVector(o.m_storageDiskConfigVector) { }

//a move constructor: X(X&&)
StorageConfig::StorageConfig(StorageConfig&& o) noexcept :
    m_storageImplementation(std::move(o.m_storageImplementation)),
    m_tryToRestoreFromDisk(o.m_tryToRestoreFromDisk),
    m_autoDeleteFilesOnExit(o.m_autoDeleteFilesOnExit),
    m_totalStorageCapacityBytes(o.m_totalStorageCapacityBytes),
    m_storageDeletionPolicy(std::move(o.m_storageDeletionPolicy)),
    m_storageDiskConfigVector(std::move(o.m_storageDiskConfigVector)) { }

//a copy assignment: operator=(const X&)
StorageConfig& StorageConfig::operator=(const StorageConfig& o) {
    m_storageImplementation = o.m_storageImplementation;
    m_tryToRestoreFromDisk = o.m_tryToRestoreFromDisk;
    m_autoDeleteFilesOnExit = o.m_autoDeleteFilesOnExit;
    m_totalStorageCapacityBytes = o.m_totalStorageCapacityBytes;
    m_storageDeletionPolicy = o.m_storageDeletionPolicy;
    m_storageDiskConfigVector = o.m_storageDiskConfigVector;
    return *this;
}

//a move assignment: operator=(X&&)
StorageConfig& StorageConfig::operator=(StorageConfig&& o) noexcept {
    m_storageImplementation = std::move(o.m_storageImplementation);
    m_tryToRestoreFromDisk = o.m_tryToRestoreFromDisk;
    m_autoDeleteFilesOnExit = o.m_autoDeleteFilesOnExit;
    m_totalStorageCapacityBytes = o.m_totalStorageCapacityBytes;
    m_storageDeletionPolicy = std::move(o.m_storageDeletionPolicy);
    m_storageDiskConfigVector = std::move(o.m_storageDiskConfigVector);
    return *this;
}

bool StorageConfig::operator==(const StorageConfig & other) const {
    return 
        (m_storageImplementation == other.m_storageImplementation) &&
        (m_tryToRestoreFromDisk == other.m_tryToRestoreFromDisk) &&
        (m_autoDeleteFilesOnExit == other.m_autoDeleteFilesOnExit) &&
        (m_totalStorageCapacityBytes == other.m_totalStorageCapacityBytes) &&
        (m_storageDeletionPolicy == other.m_storageDeletionPolicy) &&
        (m_storageDiskConfigVector == other.m_storageDiskConfigVector);
}

bool StorageConfig::SetValuesFromPropertyTree(const boost::property_tree::ptree & pt) {
    try {
        m_storageImplementation = pt.get<std::string>("storageImplementation");
        {
            bool found = false;
            for (std::vector<std::string>::const_iterator it = VALID_STORAGE_IMPLEMENTATION_NAMES.cbegin(); it != VALID_STORAGE_IMPLEMENTATION_NAMES.cend(); ++it) {
                if (m_storageImplementation == *it) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                LOG_ERROR(subprocess) << "error parsing JSON Storage config:: invalid storage implementation " << m_storageImplementation;
                return false;
            }
        }
        m_storageDeletionPolicy = pt.get<std::string>("storageDeletionPolicy");
        {
            bool found = false;
            for (std::vector<std::string>::const_iterator it = VALID_STORAGE_DELETION_POLICIES.cbegin(); it != VALID_STORAGE_DELETION_POLICIES.cend(); ++it) {
                if (m_storageDeletionPolicy == *it) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                LOG_ERROR(subprocess) << "error parsing JSON Storage config:: invalid storage deletion policy" << m_storageDeletionPolicy;
                return false;
            }
        }
        m_tryToRestoreFromDisk = pt.get<bool>("tryToRestoreFromDisk");
        m_autoDeleteFilesOnExit = pt.get<bool>("autoDeleteFilesOnExit");
        m_totalStorageCapacityBytes = pt.get<uint64_t>("totalStorageCapacityBytes");
    }
    catch (const boost::property_tree::ptree_error & e) {
        LOG_ERROR(subprocess) << "error parsing JSON Storage config: " << e.what();
        return false;
    }

    if (m_totalStorageCapacityBytes == 0) {
        LOG_ERROR(subprocess) << "error parsing JSON Storage config: totalStorageCapacityBytes must be defined and non-zero";
        return false;
    }

    //for non-throw versions of get_child which return a reference to the second parameter
    static const boost::property_tree::ptree EMPTY_PTREE;

    const boost::property_tree::ptree & storageDiskConfigVectorPt = pt.get_child("storageDiskConfigVector", EMPTY_PTREE); //non-throw version
    m_storageDiskConfigVector.resize(storageDiskConfigVectorPt.size());
    unsigned int storageDiskConfigVectorIndex = 0;
    BOOST_FOREACH(const boost::property_tree::ptree::value_type & storageDiskConfigPt, storageDiskConfigVectorPt) {
        storage_disk_config_t & storageDiskConfig = m_storageDiskConfigVector[storageDiskConfigVectorIndex++];
        try {
            storageDiskConfig.name = storageDiskConfigPt.second.get<std::string>("name");
            storageDiskConfig.storeFilePath = storageDiskConfigPt.second.get<std::string>("storeFilePath");
        }
        catch (const boost::property_tree::ptree_error & e) {
            LOG_ERROR(subprocess) << "error parsing JSON storageDiskConfigVector[" << (storageDiskConfigVectorIndex - 1) << "]: " << e.what();
            return false;
        }
        if (storageDiskConfig.storeFilePath == "") {
            LOG_ERROR(subprocess) << "error parsing JSON storageDiskConfigVector[" << (storageDiskConfigVectorIndex - 1) << "]: storeFilePath must be defined";
            return false;
        }
    }

    return true;
}

StorageConfig_ptr StorageConfig::CreateFromJson(const std::string& jsonString, bool verifyNoUnusedJsonKeys) {
    boost::property_tree::ptree pt;
    StorageConfig_ptr config; //NULL
    if (GetPropertyTreeFromJsonString(jsonString, pt)) { //prints message if failed
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

StorageConfig_ptr StorageConfig::CreateFromJsonFilePath(const boost::filesystem::path& jsonFilePath, bool verifyNoUnusedJsonKeys) {
    boost::property_tree::ptree pt;
    StorageConfig_ptr config; //NULL
    if (GetPropertyTreeFromJsonFilePath(jsonFilePath, pt)) { //prints message if failed
        config = CreateFromPtree(pt);
        //verify that there are no unused variables within the original json
        if (config && verifyNoUnusedJsonKeys) {
            std::string returnedErrorMessage;
            if (JsonSerializable::HasUnusedJsonVariablesInFilePath(*config, jsonFilePath, returnedErrorMessage)) {
                LOG_ERROR(subprocess) << returnedErrorMessage;
                config.reset(); //NULL
            }
        }
    }
    return config;
}

StorageConfig_ptr StorageConfig::CreateFromPtree(const boost::property_tree::ptree & pt) {

    StorageConfig_ptr ptrStorageConfig = std::make_shared<StorageConfig>();
    if (!ptrStorageConfig->SetValuesFromPropertyTree(pt)) {
        ptrStorageConfig = StorageConfig_ptr(); //failed, so delete and set it NULL
    }
    return ptrStorageConfig;
}

boost::property_tree::ptree StorageConfig::GetNewPropertyTree() const {
    boost::property_tree::ptree pt;
    pt.put("storageImplementation", m_storageImplementation);
    pt.put("tryToRestoreFromDisk", m_tryToRestoreFromDisk);
    pt.put("autoDeleteFilesOnExit", m_autoDeleteFilesOnExit);
    pt.put("totalStorageCapacityBytes", m_totalStorageCapacityBytes);
    pt.put("storageDeletionPolicy", m_storageDeletionPolicy);
    boost::property_tree::ptree & storageDiskConfigVectorPt = pt.put_child("storageDiskConfigVector", m_storageDiskConfigVector.empty() ? boost::property_tree::ptree("[]") : boost::property_tree::ptree());
    for (storage_disk_config_vector_t::const_iterator storageDiskConfigVectorIt = m_storageDiskConfigVector.cbegin(); storageDiskConfigVectorIt != m_storageDiskConfigVector.cend(); ++storageDiskConfigVectorIt) {
        const storage_disk_config_t & storageDiskConfig = *storageDiskConfigVectorIt;
        boost::property_tree::ptree & storageDiskConfigPt = (storageDiskConfigVectorPt.push_back(std::make_pair("", boost::property_tree::ptree())))->second; //using "" as key creates json array
        storageDiskConfigPt.put("name", storageDiskConfig.name);
        storageDiskConfigPt.put("storeFilePath", storageDiskConfig.storeFilePath);
    }

    return pt;
}


void StorageConfig::AddDisk(const std::string & name, const std::string & storeFilePath) {
    m_storageDiskConfigVector.push_back(storage_disk_config_t(name, storeFilePath));
}
