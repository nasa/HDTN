/***************************************************************************
 * NASA Glenn Research Center, Cleveland, OH
 * Released under the NASA Open Source Agreement (NOSA)
 * May  2021
 ****************************************************************************
 */

#include "StorageConfig.h"
#include <boost/make_shared.hpp>
#include <boost/foreach.hpp>
#include <iostream>

static const std::vector<std::string> VALID_STORAGE_IMPLEMENTATION_NAMES = { "stdio_multi_threaded", "asio_single_threaded" };

storage_disk_config_t::storage_disk_config_t() : name(""), storeFilePath("") {}
storage_disk_config_t::~storage_disk_config_t() {}

storage_disk_config_t::storage_disk_config_t(const std::string & paramName, const std::string & paramStoreFilePath) :
    name(paramName), storeFilePath(paramStoreFilePath) {}

//a copy constructor: X(const X&)
storage_disk_config_t::storage_disk_config_t(const storage_disk_config_t& o) :
    name(o.name), storeFilePath(o.storeFilePath) { }

//a move constructor: X(X&&)
storage_disk_config_t::storage_disk_config_t(storage_disk_config_t&& o) :
    name(std::move(o.name)), storeFilePath(std::move(o.storeFilePath)) { }

//a copy assignment: operator=(const X&)
storage_disk_config_t& storage_disk_config_t::operator=(const storage_disk_config_t& o) {
    name = o.name;
    storeFilePath = o.storeFilePath;
    return *this;
}

//a move assignment: operator=(X&&)
storage_disk_config_t& storage_disk_config_t::operator=(storage_disk_config_t&& o) {
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
    m_storageDiskConfigVector() { }

StorageConfig::~StorageConfig() {
}

//a copy constructor: X(const X&)
StorageConfig::StorageConfig(const StorageConfig& o) :
    m_storageImplementation(o.m_storageImplementation),
    m_tryToRestoreFromDisk(o.m_tryToRestoreFromDisk),
    m_autoDeleteFilesOnExit(o.m_autoDeleteFilesOnExit),
    m_totalStorageCapacityBytes(o.m_totalStorageCapacityBytes),
    m_storageDiskConfigVector(o.m_storageDiskConfigVector) { }

//a move constructor: X(X&&)
StorageConfig::StorageConfig(StorageConfig&& o) :
    m_storageImplementation(std::move(o.m_storageImplementation)),
    m_tryToRestoreFromDisk(o.m_tryToRestoreFromDisk),
    m_autoDeleteFilesOnExit(o.m_autoDeleteFilesOnExit),
    m_totalStorageCapacityBytes(o.m_totalStorageCapacityBytes),
    m_storageDiskConfigVector(std::move(o.m_storageDiskConfigVector)) { }

//a copy assignment: operator=(const X&)
StorageConfig& StorageConfig::operator=(const StorageConfig& o) {
    m_storageImplementation = o.m_storageImplementation;
    m_tryToRestoreFromDisk = o.m_tryToRestoreFromDisk;
    m_autoDeleteFilesOnExit = o.m_autoDeleteFilesOnExit;
    m_totalStorageCapacityBytes = o.m_totalStorageCapacityBytes;
    m_storageDiskConfigVector = o.m_storageDiskConfigVector;
    return *this;
}

//a move assignment: operator=(X&&)
StorageConfig& StorageConfig::operator=(StorageConfig&& o) {
    m_storageImplementation = std::move(o.m_storageImplementation);
    m_tryToRestoreFromDisk = o.m_tryToRestoreFromDisk;
    m_autoDeleteFilesOnExit = o.m_autoDeleteFilesOnExit;
    m_totalStorageCapacityBytes = o.m_totalStorageCapacityBytes;
    m_storageDiskConfigVector = std::move(o.m_storageDiskConfigVector);
    return *this;
}

bool StorageConfig::operator==(const StorageConfig & other) const {
    return 
        (m_storageImplementation == other.m_storageImplementation) &&
        (m_tryToRestoreFromDisk == other.m_tryToRestoreFromDisk) &&
        (m_autoDeleteFilesOnExit == other.m_autoDeleteFilesOnExit) &&
        (m_totalStorageCapacityBytes == other.m_totalStorageCapacityBytes) &&
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
                std::cerr << "error parsing JSON Storage config:: invalid storage implementation " << m_storageImplementation << std::endl;
                return false;
            }
        }
        m_tryToRestoreFromDisk = pt.get<bool>("tryToRestoreFromDisk");
        m_autoDeleteFilesOnExit = pt.get<bool>("autoDeleteFilesOnExit");
        m_totalStorageCapacityBytes = pt.get<uint64_t>("totalStorageCapacityBytes");
    }
    catch (const boost::property_tree::ptree_error & e) {
        std::cerr << "error parsing JSON Storage config: " << e.what() << std::endl;
        return false;
    }

    if (m_totalStorageCapacityBytes == 0) {
        std::cerr << "error parsing JSON Storage config: totalStorageCapacityBytes must be defined and non-zero\n";
        return false;
    }
    const boost::property_tree::ptree & storageDiskConfigVectorPt = pt.get_child("storageDiskConfigVector", boost::property_tree::ptree()); //non-throw version
    m_storageDiskConfigVector.resize(storageDiskConfigVectorPt.size());
    unsigned int storageDiskConfigVectorIndex = 0;
    BOOST_FOREACH(const boost::property_tree::ptree::value_type & storageDiskConfigPt, storageDiskConfigVectorPt) {
        storage_disk_config_t & storageDiskConfig = m_storageDiskConfigVector[storageDiskConfigVectorIndex++];
        try {
            storageDiskConfig.name = storageDiskConfigPt.second.get<std::string>("name");
            storageDiskConfig.storeFilePath = storageDiskConfigPt.second.get<std::string>("storeFilePath");
        }
        catch (const boost::property_tree::ptree_error & e) {
            std::cerr << "error parsing JSON storageDiskConfigVector[" << (storageDiskConfigVectorIndex - 1) << "]: " << e.what() << std::endl;
            return false;
        }
        if (storageDiskConfig.storeFilePath == "") {
            std::cerr << "error parsing JSON storageDiskConfigVector[" << (storageDiskConfigVectorIndex - 1) << "]: storeFilePath must be defined\n";
            return false;
        }
    }

    return true;
}

StorageConfig_ptr StorageConfig::CreateFromJson(const std::string & jsonString) {
    try {
        return StorageConfig::CreateFromPtree(JsonSerializable::GetPropertyTreeFromJsonString(jsonString));
    }
    catch (boost::property_tree::json_parser_error & e) {
        const std::string message = "In StorageConfig::CreateFromJson. Error: " + std::string(e.what());
        hdtn::Logger::getInstance()->logError("storage", message);
        std::cerr << message << std::endl;
    }

    return StorageConfig_ptr(); //NULL
}

StorageConfig_ptr StorageConfig::CreateFromJsonFile(const std::string & jsonFileName) {
    try {
        return StorageConfig::CreateFromPtree(JsonSerializable::GetPropertyTreeFromJsonFile(jsonFileName));
    }
    catch (boost::property_tree::json_parser_error & e) {
        const std::string message = "In StorageConfig::CreateFromJsonFile. Error: " + std::string(e.what());
        hdtn::Logger::getInstance()->logError("storage", message);
        std::cerr << message << std::endl;
    }

    return StorageConfig_ptr(); //NULL
}

StorageConfig_ptr StorageConfig::CreateFromPtree(const boost::property_tree::ptree & pt) {

    StorageConfig_ptr ptrStorageConfig = boost::make_shared<StorageConfig>();
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
