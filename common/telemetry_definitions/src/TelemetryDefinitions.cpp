/**
 * @file Telemetry.cpp
 * @author  Blake LaFuente
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

#include <boost/endian/conversion.hpp>

#include "TelemetryDefinitions.h"
#include "Logger.h"
#include "Uri.h"
#include <boost/make_unique.hpp>
#include <boost/foreach.hpp>

static constexpr hdtn::Logger::SubProcess subprocess = hdtn::Logger::SubProcess::none;

static constexpr std::size_t numTypes = static_cast<std::size_t>(TelemetryType::none) + 1;
static const std::string typeToString[numTypes] =
{
    "undefined",
    "ingress",
    "egress",
    "storage",
    "ltpoutduct",
    "stcpoutduct",
    "unused6",
    "unused7",
    "unused8",
    "unused9",
    "storageExpiringBeforeThreshold",
    "outductCapability",
    "allOutductCapability",
    ""
};
std::string TypeToString(TelemetryType t) {
    std::size_t val = static_cast<std::size_t>(t);
    if (val >= numTypes) {
        return "";
    }
    return typeToString[val];
}
static const TelemetryType typeFromString(const std::string& typeStr) {
    for (std::size_t i = 0; i < numTypes; ++i) {
        if (typeStr == typeToString[i]) {
            return TelemetryType(i);
        }
    }
    return TelemetryType::none;
}

static void SerializeUint64ArrayToLittleEndian(uint64_t* dest, const uint64_t* src, const uint64_t numElements) {
    for (uint64_t i = 0; i < numElements; ++i) {
        const uint64_t nativeSrc = *src++;
        const uint64_t littleDest = boost::endian::native_to_little(nativeSrc);
        *dest++ = littleDest;
    }
}

static double LittleToNativeDouble(const uint64_t* doubleAsUint64Little) {
    const uint64_t doubleAsUint64Native = boost::endian::little_to_native(*doubleAsUint64Little);
    return *(reinterpret_cast<const double*>(&doubleAsUint64Native));
}

/////////////////////////////////////
//Telemetry_t (base class)
/////////////////////////////////////
Telemetry_t::Telemetry_t() : Telemetry_t(TelemetryType::undefined) {}
Telemetry_t::Telemetry_t(TelemetryType type) : m_type(type) {
    m_fieldsToSerialize.push_back(reinterpret_cast<uint64_t*>(&m_type));
}

Telemetry_t::~Telemetry_t() {};
bool Telemetry_t::operator==(const Telemetry_t& o) const {
    return (m_type == o.m_type);
}
bool Telemetry_t::operator!=(const Telemetry_t& o) const {
    return !(*this == o);
}

uint64_t Telemetry_t::SerializeToLittleEndian(uint8_t* buffer, uint64_t bufferSize) const {
    uint64_t numBytes = Telemetry_t::GetSerializationSize();
    if (numBytes > bufferSize) {
        return 0;
    }
    uint64_t* buffer64 = reinterpret_cast<uint64_t*>(buffer);
    for (uint64_t* field : m_fieldsToSerialize) {
        const uint64_t fieldLittleEndian = boost::endian::native_to_little(*field);
        *buffer64++ = fieldLittleEndian;
    }
    return numBytes;
}

TelemetryType Telemetry_t::GetType()
{
    return TelemetryType(m_type); 
}

uint64_t Telemetry_t::GetSerializationSize() const {
    return m_fieldsToSerialize.size() * sizeof(uint64_t);
}

bool Telemetry_t::DeserializeFromLittleEndian(const uint8_t* serialization, uint64_t& numBytesTakenToDecode, uint64_t bufferSize) {
    numBytesTakenToDecode = Telemetry_t::GetSerializationSize();
    if (bufferSize < numBytesTakenToDecode) {
        return false;
    }
    const uint64_t* buffer64 = reinterpret_cast<const uint64_t*>(serialization);
    for (uint64_t i = 0; i < m_fieldsToSerialize.size(); ++i) {
        *m_fieldsToSerialize[i] = boost::endian::little_to_native(*buffer64);
        buffer64++;
    }
    return true;
}

bool Telemetry_t::SetValuesFromPropertyTree(const boost::property_tree::ptree& pt) {
    try {
        m_type = typeFromString(pt.get<std::string>("type"));
    }
    catch (const boost::property_tree::ptree_error& e) {
        LOG_ERROR(subprocess) << "parsing JSON Telemetry_t: " << e.what();
        return false;
    }
    return true;
}

boost::property_tree::ptree Telemetry_t::GetNewPropertyTree() const {
    boost::property_tree::ptree pt;
    pt.put("type", TypeToString(m_type));
    return pt;
}

/////////////////////////////////////
//IngressTelemetry_t
/////////////////////////////////////
IngressTelemetry_t::IngressTelemetry_t() :
    Telemetry_t(TelemetryType::ingress),
    totalDataBytes(0),
    bundleCountEgress(0),
    bundleCountStorage(0)
{
    Telemetry_t::m_fieldsToSerialize.insert(m_fieldsToSerialize.end(), {
        &totalDataBytes,
        &bundleCountEgress,
        &bundleCountStorage
    });
}

IngressTelemetry_t::~IngressTelemetry_t() {};

bool IngressTelemetry_t::operator==(const IngressTelemetry_t& o) const {
    return Telemetry_t::operator==(o)
        && (totalDataBytes == o.totalDataBytes)
        && (bundleCountEgress == o.bundleCountEgress)
        && (bundleCountStorage == o.bundleCountStorage);
}
bool IngressTelemetry_t::operator!=(const IngressTelemetry_t& o) const {
    return !(*this == o);
}

bool IngressTelemetry_t::SetValuesFromPropertyTree(const boost::property_tree::ptree& pt) {
    if (!Telemetry_t::SetValuesFromPropertyTree(pt)) {
        return false;
    }
    try {
        totalDataBytes = pt.get<uint64_t>("totalDataBytes");
        bundleCountEgress = pt.get<uint64_t>("bundleCountEgress");
        bundleCountStorage = pt.get<uint64_t>("bundleCountStorage");
    }
    catch (const boost::property_tree::ptree_error& e) {
        LOG_ERROR(subprocess) << "parsing JSON IngressTelemetry_t: " << e.what();
        return false;
    }
    return true;
}

boost::property_tree::ptree IngressTelemetry_t::GetNewPropertyTree() const {
    boost::property_tree::ptree pt = Telemetry_t::GetNewPropertyTree();
    pt.put("totalDataBytes", totalDataBytes);
    pt.put("bundleCountEgress", bundleCountEgress);
    pt.put("bundleCountStorage", bundleCountStorage);
    return pt;
}

/////////////////////////////////////
//EgressTelemetry_t
/////////////////////////////////////
EgressTelemetry_t::EgressTelemetry_t() :
    Telemetry_t(TelemetryType::egress),
    totalDataBytes(0),
    egressBundleCount(0),
    egressMessageCount(0)
{
    Telemetry_t::m_fieldsToSerialize.insert(m_fieldsToSerialize.end(), {
        &egressBundleCount,
        &totalDataBytes,
        &egressMessageCount
    });
}

EgressTelemetry_t::~EgressTelemetry_t() {};
bool EgressTelemetry_t::operator==(const EgressTelemetry_t& o) const {
    return Telemetry_t::operator==(o)
        && (totalDataBytes == o.totalDataBytes)
        && (egressBundleCount == o.egressBundleCount)
        && (egressMessageCount == o.egressMessageCount);
}
bool EgressTelemetry_t::operator!=(const EgressTelemetry_t& o) const {
    return !(*this == o);
}

bool EgressTelemetry_t::SetValuesFromPropertyTree(const boost::property_tree::ptree& pt) {
    if (!Telemetry_t::SetValuesFromPropertyTree(pt)) {
        return false;
    }
    try {
        totalDataBytes = pt.get<uint64_t>("totalDataBytes");
        egressBundleCount = pt.get<uint64_t>("egressBundleCount");
        egressMessageCount = pt.get<uint64_t>("egressMessageCount");
    }
    catch (const boost::property_tree::ptree_error& e) {
        LOG_ERROR(subprocess) << "parsing JSON EgressTelemetry_t: " << e.what();
        return false;
    }
    return true;
}

boost::property_tree::ptree EgressTelemetry_t::GetNewPropertyTree() const {
    boost::property_tree::ptree pt = Telemetry_t::GetNewPropertyTree();
    pt.put("totalDataBytes", totalDataBytes);
    pt.put("egressBundleCount", egressBundleCount);
    pt.put("egressMessageCount", egressMessageCount);
    return pt;
}

/////////////////////////////////////
//StorageTelemetry_t
/////////////////////////////////////
StorageTelemetry_t::StorageTelemetry_t() :
    Telemetry_t(TelemetryType::storage),
    totalBundlesErasedFromStorage(0),
    totalBundlesSentToEgressFromStorage(0),
    usedSpaceBytes(0),
    freeSpaceBytes(0)
{
    Telemetry_t::m_fieldsToSerialize.insert(m_fieldsToSerialize.end(), {
        &totalBundlesErasedFromStorage,
        &totalBundlesSentToEgressFromStorage,
        &usedSpaceBytes,
        &freeSpaceBytes
    });
}

StorageTelemetry_t::~StorageTelemetry_t() {};
bool StorageTelemetry_t::operator==(const StorageTelemetry_t& o) const {
    return Telemetry_t::operator==(o)
        && (totalBundlesErasedFromStorage == o.totalBundlesErasedFromStorage)
        && (totalBundlesSentToEgressFromStorage == o.totalBundlesSentToEgressFromStorage)
        && (usedSpaceBytes == o.usedSpaceBytes)
        && (freeSpaceBytes == o.freeSpaceBytes);
}
bool StorageTelemetry_t::operator!=(const StorageTelemetry_t& o) const {
    return !(*this == o);
}

bool StorageTelemetry_t::SetValuesFromPropertyTree(const boost::property_tree::ptree& pt) {
    if (!Telemetry_t::SetValuesFromPropertyTree(pt)) {
        return false;
    }
    try {
        totalBundlesErasedFromStorage = pt.get<uint64_t>("totalBundlesErasedFromStorage");
        totalBundlesSentToEgressFromStorage = pt.get<uint64_t>("totalBundlesSentToEgressFromStorage");
        usedSpaceBytes = pt.get<uint64_t>("usedSpaceBytes");
        freeSpaceBytes = pt.get<uint64_t>("freeSpaceBytes");
    }
    catch (const boost::property_tree::ptree_error& e) {
        LOG_ERROR(subprocess) << "parsing JSON StorageTelemetry_t: " << e.what();
        return false;
    }
    return true;
}

boost::property_tree::ptree StorageTelemetry_t::GetNewPropertyTree() const {
    boost::property_tree::ptree pt = Telemetry_t::GetNewPropertyTree();
    pt.put("totalBundlesErasedFromStorage", totalBundlesErasedFromStorage);
    pt.put("totalBundlesSentToEgressFromStorage", totalBundlesSentToEgressFromStorage);
    pt.put("usedSpaceBytes", usedSpaceBytes);
    pt.put("freeSpaceBytes", freeSpaceBytes);
    return pt;
}

/////////////////////////////////////
//OutductTelemetry_t
/////////////////////////////////////
OutductTelemetry_t::OutductTelemetry_t(TelemetryType type) :
    Telemetry_t(type),
    convergenceLayerType(0),
    totalBundlesAcked(0),
    totalBundleBytesAcked(0),
    totalBundlesSent(0),
    totalBundleBytesSent(0),
    totalBundlesFailedToSend(0)
{
    Telemetry_t::m_fieldsToSerialize.insert(m_fieldsToSerialize.end(), {
        &convergenceLayerType,
        &totalBundlesAcked,
        &totalBundleBytesAcked,
        &totalBundlesSent,
        &totalBundleBytesSent,
        &totalBundlesFailedToSend
    });
}

OutductTelemetry_t::~OutductTelemetry_t() {};
bool OutductTelemetry_t::operator==(const OutductTelemetry_t& o) const {
    return Telemetry_t::operator==(o)
        && (convergenceLayerType == o.convergenceLayerType)
        && (totalBundlesAcked == o.totalBundlesAcked)
        && (totalBundleBytesAcked == o.totalBundleBytesAcked)
        && (totalBundlesSent == o.totalBundlesSent)
        && (totalBundleBytesSent == o.totalBundleBytesSent)
        && (totalBundlesFailedToSend == o.totalBundlesFailedToSend);
}
bool OutductTelemetry_t::operator!=(const OutductTelemetry_t& o) const {
    return !(*this == o);
}

bool OutductTelemetry_t::SetValuesFromPropertyTree(const boost::property_tree::ptree& pt) {
    if (!Telemetry_t::SetValuesFromPropertyTree(pt)) {
        return false;
    }
    try {
        convergenceLayerType = pt.get<uint64_t>("convergenceLayerType");
        totalBundlesAcked = pt.get<uint64_t>("totalBundlesAcked");
        totalBundleBytesAcked = pt.get<uint64_t>("totalBundleBytesAcked");
        totalBundlesSent = pt.get<uint64_t>("totalBundlesSent");
        totalBundleBytesSent = pt.get<uint64_t>("totalBundleBytesSent");
        totalBundlesFailedToSend = pt.get<uint64_t>("totalBundlesFailedToSend");
    }
    catch (const boost::property_tree::ptree_error& e) {
        LOG_ERROR(subprocess) << "parsing JSON OutductTelemetry_t: " << e.what();
        return false;
    }
    return true;
}

boost::property_tree::ptree OutductTelemetry_t::GetNewPropertyTree() const {
    boost::property_tree::ptree pt = Telemetry_t::GetNewPropertyTree();
    pt.put("convergenceLayerType", convergenceLayerType);
    pt.put("totalBundlesAcked", totalBundlesAcked);
    pt.put("totalBundleBytesAcked", totalBundleBytesAcked);
    pt.put("totalBundlesSent", totalBundlesSent);
    pt.put("totalBundleBytesSent", totalBundleBytesSent);
    pt.put("totalBundlesFailedToSend", totalBundlesFailedToSend);
    return pt;
}

StcpOutductTelemetry_t::StcpOutductTelemetry_t() :
    OutductTelemetry_t(TelemetryType::stcpoutduct),
    totalStcpBytesSent(0)
{
    OutductTelemetry_t::convergenceLayerType = 1;
    Telemetry_t::m_fieldsToSerialize.insert(m_fieldsToSerialize.end(), {
        &totalStcpBytesSent
    });
}

StcpOutductTelemetry_t::~StcpOutductTelemetry_t() {};
bool StcpOutductTelemetry_t::operator==(const StcpOutductTelemetry_t& o) const {
    return OutductTelemetry_t::operator==(o)
        && (totalStcpBytesSent == o.totalStcpBytesSent);
}
bool StcpOutductTelemetry_t::operator!=(const StcpOutductTelemetry_t& o) const {
    return !(*this == o);
}

bool StcpOutductTelemetry_t::SetValuesFromPropertyTree(const boost::property_tree::ptree& pt) {
    if (!OutductTelemetry_t::SetValuesFromPropertyTree(pt)) {
        return false;
    }
    try {
        totalStcpBytesSent = pt.get<uint64_t>("totalStcpBytesSent");
    }
    catch (const boost::property_tree::ptree_error& e) {
        LOG_ERROR(subprocess) << "parsing JSON StcpOutductTelemetry_t: " << e.what();
        return false;
    }
    return true;
}

boost::property_tree::ptree StcpOutductTelemetry_t::GetNewPropertyTree() const {
    boost::property_tree::ptree pt = OutductTelemetry_t::GetNewPropertyTree();
    pt.put("totalStcpBytesSent", totalStcpBytesSent);
    return pt;
}

LtpOutductTelemetry_t::LtpOutductTelemetry_t() :
    OutductTelemetry_t(TelemetryType::ltpoutduct),
    numCheckpointsExpired(0), numDiscretionaryCheckpointsNotResent(0), countUdpPacketsSent(0),
    countRxUdpCircularBufferOverruns(0), countTxUdpPacketsLimitedByRate(0)
{
    OutductTelemetry_t::convergenceLayerType = 2;
    Telemetry_t::m_fieldsToSerialize.insert(m_fieldsToSerialize.end(), {
        &numCheckpointsExpired,
        &numDiscretionaryCheckpointsNotResent,
        &countUdpPacketsSent,
        &countRxUdpCircularBufferOverruns,
        &countTxUdpPacketsLimitedByRate,
    });
}

LtpOutductTelemetry_t::~LtpOutductTelemetry_t() {};
bool LtpOutductTelemetry_t::operator==(const LtpOutductTelemetry_t& o) const {
    return OutductTelemetry_t::operator==(o)
        && (numCheckpointsExpired == o.numCheckpointsExpired)
        && (numDiscretionaryCheckpointsNotResent == o.numDiscretionaryCheckpointsNotResent)
        && (countUdpPacketsSent == o.countUdpPacketsSent)
        && (countRxUdpCircularBufferOverruns == o.countRxUdpCircularBufferOverruns)
        && (countTxUdpPacketsLimitedByRate == o.countTxUdpPacketsLimitedByRate);
}
bool LtpOutductTelemetry_t::operator!=(const LtpOutductTelemetry_t& o) const {
    return !(*this == o);
}

bool LtpOutductTelemetry_t::SetValuesFromPropertyTree(const boost::property_tree::ptree& pt) {
    if (!OutductTelemetry_t::SetValuesFromPropertyTree(pt)) {
        return false;
    }
    try {
        numCheckpointsExpired = pt.get<uint64_t>("numCheckpointsExpired");
        numDiscretionaryCheckpointsNotResent = pt.get<uint64_t>("numDiscretionaryCheckpointsNotResent");
        countUdpPacketsSent = pt.get<uint64_t>("countUdpPacketsSent");
        countRxUdpCircularBufferOverruns = pt.get<uint64_t>("countRxUdpCircularBufferOverruns");
        countTxUdpPacketsLimitedByRate = pt.get<uint64_t>("countTxUdpPacketsLimitedByRate");
    }
    catch (const boost::property_tree::ptree_error& e) {
        LOG_ERROR(subprocess) << "parsing JSON StcpOutductTelemetry_t: " << e.what();
        return false;
    }
    return true;
}

boost::property_tree::ptree LtpOutductTelemetry_t::GetNewPropertyTree() const {
    boost::property_tree::ptree pt = OutductTelemetry_t::GetNewPropertyTree();
    pt.put("numCheckpointsExpired", numCheckpointsExpired);
    pt.put("numDiscretionaryCheckpointsNotResent", numDiscretionaryCheckpointsNotResent);
    pt.put("countUdpPacketsSent", countUdpPacketsSent);
    pt.put("countRxUdpCircularBufferOverruns", countRxUdpCircularBufferOverruns);
    pt.put("countTxUdpPacketsLimitedByRate", countTxUdpPacketsLimitedByRate);
    return pt;
}

/////////////////////////////////////
//StorageExpiringBeforeThresholdTelemetry_t
/////////////////////////////////////
StorageExpiringBeforeThresholdTelemetry_t::StorageExpiringBeforeThresholdTelemetry_t() :
    Telemetry_t(TelemetryType::storageExpiringBeforeThreshold),
    priority(0),
    thresholdSecondsSinceStartOfYear2000(0)
{
    Telemetry_t::m_fieldsToSerialize.insert(m_fieldsToSerialize.end(), {
        &priority,
        &thresholdSecondsSinceStartOfYear2000,
    });
}

StorageExpiringBeforeThresholdTelemetry_t::~StorageExpiringBeforeThresholdTelemetry_t() {}
bool StorageExpiringBeforeThresholdTelemetry_t::operator==(const StorageExpiringBeforeThresholdTelemetry_t& o) const {
    return Telemetry_t::operator==(o)
        && (priority == o.priority)
        && (thresholdSecondsSinceStartOfYear2000 == o.thresholdSecondsSinceStartOfYear2000)
        && (mapNodeIdToExpiringBeforeThresholdCount == o.mapNodeIdToExpiringBeforeThresholdCount);
}
bool StorageExpiringBeforeThresholdTelemetry_t::operator!=(const StorageExpiringBeforeThresholdTelemetry_t& o) const {
    return !(*this == o);
}

static const uint64_t mapEntrySize = sizeof(uint64_t) +
    sizeof(StorageExpiringBeforeThresholdTelemetry_t::bundle_count_plus_bundle_bytes_pair_t);

uint64_t StorageExpiringBeforeThresholdTelemetry_t::SerializeToLittleEndian(uint8_t* data, uint64_t bufferSize) const {
    // Let the base method handle the uint64_t fields
    uint64_t bytesSerialized = Telemetry_t::SerializeToLittleEndian(data, bufferSize);
    bufferSize -= bytesSerialized;
    data += bytesSerialized;

    // Now, handle the map
    uint64_t* data64Ptr = reinterpret_cast<uint64_t*>(data);
    if (bufferSize < (8 + mapEntrySize)) {
        return 0; //failure
    }

    // Serialize the map size
    *data64Ptr++ = boost::endian::native_to_little(static_cast<uint64_t>(mapNodeIdToExpiringBeforeThresholdCount.size()));
    bufferSize -= 8;
    bytesSerialized += 8;

    // Serialize the map values
    for (std::map<uint64_t, bundle_count_plus_bundle_bytes_pair_t>::const_iterator it = mapNodeIdToExpiringBeforeThresholdCount.cbegin();
        it != mapNodeIdToExpiringBeforeThresholdCount.cend();
        ++it)
    {
        if (bufferSize < mapEntrySize) {
            return 0; //failure
        }
        bufferSize -= mapEntrySize;
        bytesSerialized += mapEntrySize;
        *data64Ptr++ = boost::endian::native_to_little(it->first); //node id
        *data64Ptr++ = boost::endian::native_to_little(it->second.first); //bundle count
        *data64Ptr++ = boost::endian::native_to_little(it->second.second); //total bundle bytes
    }
    return bytesSerialized;
}

uint64_t StorageExpiringBeforeThresholdTelemetry_t::GetSerializationSize() const {
    uint64_t size = Telemetry_t::GetSerializationSize();
    size += sizeof(uint64_t);
    size += mapNodeIdToExpiringBeforeThresholdCount.size() * mapEntrySize;
    return size;
}

bool StorageExpiringBeforeThresholdTelemetry_t::SetValuesFromPropertyTree(const boost::property_tree::ptree& pt) {
    if (!Telemetry_t::SetValuesFromPropertyTree(pt)) {
        return false;
    }
    try {
        priority = pt.get<uint64_t>("priority");
        thresholdSecondsSinceStartOfYear2000 = pt.get<uint64_t>("thresholdSecondsSinceStartOfYear2000");
        mapNodeIdToExpiringBeforeThresholdCount.clear();
        const boost::property_tree::ptree& nodeIdMapPt = pt.get_child("mapNodeIdToExpiringBeforeThresholdCount", boost::property_tree::ptree()); //non-throw version
        BOOST_FOREACH(const boost::property_tree::ptree::value_type & nodePt, nodeIdMapPt) {
            const uint64_t nodeIdKey = boost::lexical_cast<uint64_t>(nodePt.first);
            bundle_count_plus_bundle_bytes_pair_t& p = mapNodeIdToExpiringBeforeThresholdCount[nodeIdKey];
            const uint64_t nodeId = nodePt.second.get<uint64_t>("nodeId");
            if (nodeId != nodeIdKey) {
                LOG_ERROR(subprocess) << "parsing JSON StorageExpiringBeforeThresholdTelemetry_t: nodeId != nodeIdKey";
                return false;
            }
            p.first = nodePt.second.get<uint64_t>("bundleCount");
            p.second = nodePt.second.get<uint64_t>("totalBundleBytes");
        }
    }
    catch (const boost::bad_lexical_cast& e) {
        LOG_ERROR(subprocess) << "parsing JSON StorageExpiringBeforeThresholdTelemetry_t: " << e.what();
    }
    catch (const boost::property_tree::ptree_error& e) {
        LOG_ERROR(subprocess) << "parsing JSON StorageExpiringBeforeThresholdTelemetry_t: " << e.what();
        return false;
    }
    return true;
}

boost::property_tree::ptree StorageExpiringBeforeThresholdTelemetry_t::GetNewPropertyTree() const {
    boost::property_tree::ptree pt = Telemetry_t::GetNewPropertyTree();
    pt.put("priority", priority);
    pt.put("thresholdSecondsSinceStartOfYear2000", thresholdSecondsSinceStartOfYear2000);
    boost::property_tree::ptree& mapNodeIdToExpiringBeforeThresholdCountPt = pt.put_child("mapNodeIdToExpiringBeforeThresholdCount",
        mapNodeIdToExpiringBeforeThresholdCount.empty() ? boost::property_tree::ptree("{}") : boost::property_tree::ptree());
    for (std::map<uint64_t, bundle_count_plus_bundle_bytes_pair_t>::const_iterator it = mapNodeIdToExpiringBeforeThresholdCount.cbegin();
        it != mapNodeIdToExpiringBeforeThresholdCount.cend(); ++it)
    {
        const std::pair<const uint64_t, bundle_count_plus_bundle_bytes_pair_t>& elPair = *it;
        boost::property_tree::ptree& elPt = mapNodeIdToExpiringBeforeThresholdCountPt.put_child(
            boost::lexical_cast<std::string>(elPair.first), boost::property_tree::ptree());
        elPt.put("nodeId", elPair.first);
        elPt.put("bundleCount", elPair.second.first);
        elPt.put("totalBundleBytes", elPair.second.second);
    }
    return pt;
}

/////////////////////////////////////
//TelemetryFactory
/////////////////////////////////////
std::vector<std::unique_ptr<Telemetry_t> > TelemetryFactory::DeserializeFromLittleEndian(const uint8_t* buffer, uint64_t bufferSize) {
    std::vector<std::unique_ptr<Telemetry_t> > telemList;
    while (bufferSize > 0) {
        // First determine the type
        const uint64_t* buffer64 = reinterpret_cast<const uint64_t*>(buffer);
        TelemetryType type = TelemetryType(*buffer64);

        // Attempt to deserialize
        std::unique_ptr<Telemetry_t> telem;
        switch (type) {
            case TelemetryType::ingress:
                telem = boost::make_unique<IngressTelemetry_t>();
                break;
            case TelemetryType::egress:
                telem = boost::make_unique<EgressTelemetry_t>();
                break;
            case TelemetryType::storage:
                telem = boost::make_unique<StorageTelemetry_t>();
                break;
            case TelemetryType::stcpoutduct:
                telem = boost::make_unique<StcpOutductTelemetry_t>();
                break;
            case TelemetryType::ltpoutduct:
                telem = boost::make_unique<LtpOutductTelemetry_t>();
                break;
            case TelemetryType::storageExpiringBeforeThreshold:
                telem = boost::make_unique<StorageExpiringBeforeThresholdTelemetry_t>();
                break;
            case TelemetryType::outductCapability:
                telem = boost::make_unique<OutductCapabilityTelemetry_t>();
                break;
            case TelemetryType::allOutductCapability:
                telem = boost::make_unique<AllOutductCapabilitiesTelemetry_t>();
                break;
            default:
                throw TelemetryFactory::TelemetryDeserializeUnknownTypeException();
        };
        uint64_t numBytesTakenToDecode;
        if (!telem->DeserializeFromLittleEndian(buffer, numBytesTakenToDecode, bufferSize)) {
            // We know the buffer isn't empty. So if nothing was written, there is
            // a formatting issue.
            throw TelemetryFactory::TelemetryDeserializeInvalidFormatException();
        }
        telemList.push_back(std::move(telem));

        bufferSize -= numBytesTakenToDecode;
        buffer += numBytesTakenToDecode;
    }
    return telemList;
}

StorageTelemetryRequest_t::StorageTelemetryRequest_t() : type(10) {}
OutductCapabilityTelemetry_t::OutductCapabilityTelemetry_t() :
    Telemetry_t(TelemetryType::outductCapability),
    outductArrayIndex(0),
    maxBundlesInPipeline(0),
    maxBundleSizeBytesInPipeline(0),
    nextHopNodeId(0) {}
AllOutductCapabilitiesTelemetry_t::AllOutductCapabilitiesTelemetry_t() :
    Telemetry_t(TelemetryType::allOutductCapability) {}

uint64_t OutductTelemetry_t::GetTotalBundlesQueued() const {
    return totalBundlesSent - totalBundlesAcked;
}

uint64_t OutductTelemetry_t::GetTotalBundleBytesQueued() const {
    return totalBundleBytesSent - totalBundleBytesAcked;
}

/////////////////////////////////////
//OutductCapabilityTelemetry_t
/////////////////////////////////////
uint64_t OutductCapabilityTelemetry_t::GetSerializationSize() const {
    return (7 * sizeof(uint64_t)) + (finalDestinationEidList.size() * sizeof(cbhe_eid_t)) + (finalDestinationNodeIdList.size() * sizeof(uint64_t));
}
uint64_t OutductCapabilityTelemetry_t::SerializeToLittleEndian(uint8_t* data, uint64_t bufferSize) const {
    const uint8_t* const dataStartPtr = data;
    const uint64_t sizeSerialized = GetSerializationSize();
    uint64_t* data64Ptr = reinterpret_cast<uint64_t*>(data);
    if (bufferSize < sizeSerialized) {
        return 0; //failure
    }
    //bufferSize -= sizeSerialized;
    *data64Ptr++ = boost::endian::native_to_little(static_cast<uint64_t>(m_type));
    *data64Ptr++ = boost::endian::native_to_little(outductArrayIndex);
    *data64Ptr++ = boost::endian::native_to_little(maxBundlesInPipeline);
    *data64Ptr++ = boost::endian::native_to_little(maxBundleSizeBytesInPipeline);
    *data64Ptr++ = boost::endian::native_to_little(nextHopNodeId);
    *data64Ptr++ = boost::endian::native_to_little(static_cast<uint64_t>(finalDestinationEidList.size()));
    for (std::list<cbhe_eid_t>::const_iterator it = finalDestinationEidList.cbegin(); it != finalDestinationEidList.cend(); ++it) {
        *data64Ptr++ = boost::endian::native_to_little(it->nodeId);
        *data64Ptr++ = boost::endian::native_to_little(it->serviceId);
    }
    *data64Ptr++ = boost::endian::native_to_little(static_cast<uint64_t>(finalDestinationNodeIdList.size()));
    for (std::list<uint64_t>::const_iterator it = finalDestinationNodeIdList.cbegin(); it != finalDestinationNodeIdList.cend(); ++it) {
        *data64Ptr++ = boost::endian::native_to_little(*it);
    }
    //return (reinterpret_cast<uint8_t*>(data64Ptr)) - dataStartPtr;
    return sizeSerialized;
}
bool OutductCapabilityTelemetry_t::DeserializeFromLittleEndian(const uint8_t* serialization, uint64_t& numBytesTakenToDecode, uint64_t bufferSize) {
    const uint8_t* const serializationBase = serialization;

    if (bufferSize < (7 * sizeof(uint64_t))) { //minimum size (if both lists are empty)
        return false;
    }
    bufferSize -= (7 * sizeof(uint64_t));
    //must be aligned
    m_type = TelemetryType(boost::endian::little_to_native(*(reinterpret_cast<const uint64_t*>(serialization))));
    serialization += sizeof(uint64_t);
    outductArrayIndex = boost::endian::little_to_native(*(reinterpret_cast<const uint64_t*>(serialization)));
    serialization += sizeof(uint64_t);
    maxBundlesInPipeline = boost::endian::little_to_native(*(reinterpret_cast<const uint64_t*>(serialization)));
    serialization += sizeof(uint64_t);
    maxBundleSizeBytesInPipeline = boost::endian::little_to_native(*(reinterpret_cast<const uint64_t*>(serialization)));
    serialization += sizeof(uint64_t);
    nextHopNodeId = boost::endian::little_to_native(*(reinterpret_cast<const uint64_t*>(serialization)));
    serialization += sizeof(uint64_t);
    const uint64_t finalDestinationEidListSize = boost::endian::little_to_native(*(reinterpret_cast<const uint64_t*>(serialization)));
    serialization += sizeof(uint64_t);
    finalDestinationEidList.clear();
    for (uint64_t i = 0; i < finalDestinationEidListSize; ++i) {
        if (bufferSize < sizeof(cbhe_eid_t)) {
            return false;
        }
        bufferSize -= sizeof(cbhe_eid_t);
        const uint64_t nodeId = boost::endian::little_to_native(*(reinterpret_cast<const uint64_t*>(serialization)));
        serialization += sizeof(uint64_t);
        const uint64_t serviceId = boost::endian::little_to_native(*(reinterpret_cast<const uint64_t*>(serialization)));
        serialization += sizeof(uint64_t);
        finalDestinationEidList.emplace_back(nodeId, serviceId);
    }
    const uint64_t finalDestinationNodeIdListSize = boost::endian::little_to_native(*(reinterpret_cast<const uint64_t*>(serialization)));
    serialization += sizeof(uint64_t);
    finalDestinationNodeIdList.clear();
    for (uint64_t i = 0; i < finalDestinationNodeIdListSize; ++i) {
        if (bufferSize < sizeof(uint64_t)) { //minimum size (if both lists are empty)
            return false;
        }
        bufferSize -= sizeof(uint64_t);
        const uint64_t nodeId = boost::endian::little_to_native(*(reinterpret_cast<const uint64_t*>(serialization)));
        serialization += sizeof(uint64_t);
        finalDestinationNodeIdList.emplace_back(nodeId);
    }

    numBytesTakenToDecode = serialization - serializationBase;
    return true;
}
bool OutductCapabilityTelemetry_t::operator==(const OutductCapabilityTelemetry_t& o) const {
    return Telemetry_t::operator==(o)
        && (outductArrayIndex == o.outductArrayIndex)
        && (maxBundlesInPipeline == o.maxBundlesInPipeline)
        && (maxBundleSizeBytesInPipeline == o.maxBundleSizeBytesInPipeline)
        && (nextHopNodeId == o.nextHopNodeId)
        && (finalDestinationEidList == o.finalDestinationEidList)
        && (finalDestinationNodeIdList == o.finalDestinationNodeIdList);
}
bool OutductCapabilityTelemetry_t::operator!=(const OutductCapabilityTelemetry_t& o) const {
    return !(*this == o);
}
OutductCapabilityTelemetry_t::OutductCapabilityTelemetry_t(const OutductCapabilityTelemetry_t& o) :
    Telemetry_t(o),
    outductArrayIndex(o.outductArrayIndex),
    maxBundlesInPipeline(o.maxBundlesInPipeline),
    maxBundleSizeBytesInPipeline(o.maxBundleSizeBytesInPipeline),
    nextHopNodeId(o.nextHopNodeId),
    finalDestinationEidList(o.finalDestinationEidList),
    finalDestinationNodeIdList(o.finalDestinationNodeIdList) { } //a copy constructor: X(const X&)
OutductCapabilityTelemetry_t::OutductCapabilityTelemetry_t(OutductCapabilityTelemetry_t&& o) :
    Telemetry_t(std::move(o)),
    outductArrayIndex(o.outductArrayIndex),
    maxBundlesInPipeline(o.maxBundlesInPipeline),
    maxBundleSizeBytesInPipeline(o.maxBundleSizeBytesInPipeline),
    nextHopNodeId(o.nextHopNodeId),
    finalDestinationEidList(std::move(o.finalDestinationEidList)),
    finalDestinationNodeIdList(std::move(o.finalDestinationNodeIdList)) { } //a move constructor: X(X&&)
OutductCapabilityTelemetry_t& OutductCapabilityTelemetry_t::operator=(const OutductCapabilityTelemetry_t& o) { //a copy assignment: operator=(const X&)
    Telemetry_t::operator=(o);
    outductArrayIndex = o.outductArrayIndex;
    maxBundlesInPipeline = o.maxBundlesInPipeline;
    maxBundleSizeBytesInPipeline = o.maxBundleSizeBytesInPipeline;
    nextHopNodeId = o.nextHopNodeId;
    finalDestinationEidList = o.finalDestinationEidList;
    finalDestinationNodeIdList = o.finalDestinationNodeIdList;
    return *this;
}
OutductCapabilityTelemetry_t& OutductCapabilityTelemetry_t::operator=(OutductCapabilityTelemetry_t&& o) { //a move assignment: operator=(X&&)
    Telemetry_t::operator=(std::move(o));
    outductArrayIndex = o.outductArrayIndex;
    maxBundlesInPipeline = o.maxBundlesInPipeline;
    maxBundleSizeBytesInPipeline = o.maxBundleSizeBytesInPipeline;
    nextHopNodeId = o.nextHopNodeId;
    finalDestinationEidList = std::move(o.finalDestinationEidList);
    finalDestinationNodeIdList = std::move(o.finalDestinationNodeIdList);
    return *this;
}
std::ostream& operator<<(std::ostream& os, const OutductCapabilityTelemetry_t& o) {
    os << "outductArrayIndex=" << o.outductArrayIndex << " "
        << " maxBundlesInPipeline=" << o.maxBundlesInPipeline << " "
        << " maxBundleSizeBytesInPipeline=" << o.maxBundleSizeBytesInPipeline
        << " nextHopNodeId=" << o.nextHopNodeId << std::endl;
    os << " finalDestinationEidList:" << std::endl;
    for (std::list<cbhe_eid_t>::const_iterator it = o.finalDestinationEidList.cbegin(); it != o.finalDestinationEidList.cend(); ++it) {
        os << "  " << (*it) << std::endl;
    }
    os << " finalDestinationNodeIdList:" << std::endl;
    for (std::list<uint64_t>::const_iterator it = o.finalDestinationNodeIdList.cbegin(); it != o.finalDestinationNodeIdList.cend(); ++it) {
        os << "  " << Uri::GetIpnUriStringAnyServiceNumber(*it) << std::endl;
    }
    return os;
}

bool OutductCapabilityTelemetry_t::SetValuesFromPropertyTree(const boost::property_tree::ptree& pt) {
    if (!Telemetry_t::SetValuesFromPropertyTree(pt)) {
        return false;
    }
    try {
        outductArrayIndex = pt.get<uint64_t>("outductArrayIndex");
        maxBundlesInPipeline = pt.get<uint64_t>("maxBundlesInPipeline");
        maxBundleSizeBytesInPipeline = pt.get<uint64_t>("maxBundleSizeBytesInPipeline");
        nextHopNodeId = pt.get<uint64_t>("nextHopNodeId");

        //for non-throw versions of get_child which return a reference to the second parameter
        static const boost::property_tree::ptree EMPTY_PTREE;
        const boost::property_tree::ptree& finalDestinationEidsListPt = pt.get_child("finalDestinationEidsList", EMPTY_PTREE); //non-throw version
        finalDestinationEidList.clear();
        finalDestinationNodeIdList.clear();
        BOOST_FOREACH(const boost::property_tree::ptree::value_type & finalDestinationEidValuePt, finalDestinationEidsListPt) {
            const std::string uriStr = finalDestinationEidValuePt.second.get_value<std::string>();
            uint64_t node, svc;
            bool serviceNumberIsWildCard;
            if (!Uri::ParseIpnUriString(uriStr, node, svc, &serviceNumberIsWildCard)) {
                LOG_ERROR(subprocess) << "error parsing JSON OutductCapabilityTelemetry_t: invalid final destination eid uri " << uriStr;
                return false;
            }
            if (serviceNumberIsWildCard) {
                finalDestinationNodeIdList.emplace_back(node);
            }
            else {
                finalDestinationEidList.emplace_back(node, svc);
            }
        }
    }
    catch (const boost::property_tree::ptree_error& e) {
        LOG_ERROR(subprocess) << "parsing JSON StorageTelemetry_t: " << e.what();
        return false;
    }
    return true;
}

boost::property_tree::ptree OutductCapabilityTelemetry_t::GetNewPropertyTree() const {
    boost::property_tree::ptree pt = Telemetry_t::GetNewPropertyTree();
    pt.put("outductArrayIndex", outductArrayIndex);
    pt.put("maxBundlesInPipeline", maxBundlesInPipeline);
    pt.put("maxBundleSizeBytesInPipeline", maxBundleSizeBytesInPipeline);
    pt.put("nextHopNodeId", nextHopNodeId);
    boost::property_tree::ptree& eidListPt = pt.put_child("finalDestinationEidsList",
        (finalDestinationEidList.empty() && finalDestinationNodeIdList.empty()) ? boost::property_tree::ptree("[]") : boost::property_tree::ptree());
    for (std::list<cbhe_eid_t>::const_iterator it = finalDestinationEidList.cbegin(); it != finalDestinationEidList.cend(); ++it) {
        const cbhe_eid_t& eid = *it;
        eidListPt.push_back(std::make_pair("", boost::property_tree::ptree(Uri::GetIpnUriString(eid.nodeId, eid.serviceId)))); //using "" as key creates json array
    }
    for (std::list<uint64_t>::const_iterator it = finalDestinationNodeIdList.cbegin(); it != finalDestinationNodeIdList.cend(); ++it) {
        const uint64_t nodeId = *it;
        eidListPt.push_back(std::make_pair("", boost::property_tree::ptree(Uri::GetIpnUriStringAnyServiceNumber(nodeId)))); //using "" as key creates json array
    }
    return pt;
}

/////////////////////////////////////
//AllOutductCapabilitiesTelemetry_t
/////////////////////////////////////
uint64_t AllOutductCapabilitiesTelemetry_t::GetSerializationSize() const {
    uint64_t serializationSize = 2 * sizeof(uint64_t);
    for (std::list<OutductCapabilityTelemetry_t>::const_iterator it = outductCapabilityTelemetryList.cbegin(); it != outductCapabilityTelemetryList.cend(); ++it) {
        serializationSize += it->GetSerializationSize();
    }
    return serializationSize;
}
uint64_t AllOutductCapabilitiesTelemetry_t::SerializeToLittleEndian(uint8_t* data, uint64_t bufferSize) const {
    const uint8_t* const dataStartPtr = data;
    const uint64_t sizeSerialized = GetSerializationSize();
    uint64_t* data64Ptr = reinterpret_cast<uint64_t*>(data);
    if (bufferSize < sizeSerialized) {
        return 0; //failure
    }
    //bufferSize -= sizeSerialized;
    *data64Ptr++ = boost::endian::native_to_little(static_cast<uint64_t>(m_type));
    *data64Ptr++ = boost::endian::native_to_little(static_cast<uint64_t>(outductCapabilityTelemetryList.size()));
    for (std::list<OutductCapabilityTelemetry_t>::const_iterator it = outductCapabilityTelemetryList.cbegin(); it != outductCapabilityTelemetryList.cend(); ++it) {
        data64Ptr += (it->SerializeToLittleEndian((uint8_t*)data64Ptr, bufferSize) >> 3); // n >> 3 same as n / sizeof(uint64_t)
    }
    //return (reinterpret_cast<uint8_t*>(data64Ptr)) - dataStartPtr;
    return sizeSerialized;
}
bool AllOutductCapabilitiesTelemetry_t::DeserializeFromLittleEndian(const uint8_t* serialization, uint64_t& numBytesTakenToDecode, uint64_t bufferSize) {
    const uint8_t* const serializationBase = serialization;

    if (bufferSize < (2 * sizeof(uint64_t))) { //minimum size (if list IS empty)
        return false;
    }
    bufferSize -= (2 * sizeof(uint64_t));
    //must be aligned
    m_type = TelemetryType(boost::endian::little_to_native(*(reinterpret_cast<const uint64_t*>(serialization))));
    serialization += sizeof(uint64_t);
    const uint64_t outductCapabilityTelemetryListSize = boost::endian::little_to_native(*(reinterpret_cast<const uint64_t*>(serialization)));
    serialization += sizeof(uint64_t);
    outductCapabilityTelemetryList.clear();
    for (uint64_t i = 0; i < outductCapabilityTelemetryListSize; ++i) {
        uint64_t thisNumBytesTakenToDecode;
        outductCapabilityTelemetryList.emplace_back();
        OutductCapabilityTelemetry_t& oct = outductCapabilityTelemetryList.back();
        if (!oct.DeserializeFromLittleEndian(serialization, thisNumBytesTakenToDecode, bufferSize)) {
            return false;
        }
        bufferSize -= thisNumBytesTakenToDecode;
        serialization += thisNumBytesTakenToDecode;
    }

    numBytesTakenToDecode = serialization - serializationBase;
    return true;
}
bool AllOutductCapabilitiesTelemetry_t::SetValuesFromPropertyTree(const boost::property_tree::ptree& pt) {
    if (!Telemetry_t::SetValuesFromPropertyTree(pt)) {
        return false;
    }
    try {
        //for non-throw versions of get_child which return a reference to the second parameter
        static const boost::property_tree::ptree EMPTY_PTREE;
        const boost::property_tree::ptree& outductCapabilityTelemetryListPt = pt.get_child("outductCapabilityTelemetryList", EMPTY_PTREE); //non-throw version
        outductCapabilityTelemetryList.clear();
        BOOST_FOREACH(const boost::property_tree::ptree::value_type & outductCapabilityTelemetryValuePt, outductCapabilityTelemetryListPt) {
            outductCapabilityTelemetryList.emplace_back();
            outductCapabilityTelemetryList.back().SetValuesFromPropertyTree(outductCapabilityTelemetryValuePt.second);
        }
    }
    catch (const boost::property_tree::ptree_error& e) {
        LOG_ERROR(subprocess) << "parsing JSON StorageTelemetry_t: " << e.what();
        return false;
    }
    return true;
}

boost::property_tree::ptree AllOutductCapabilitiesTelemetry_t::GetNewPropertyTree() const {
    boost::property_tree::ptree pt = Telemetry_t::GetNewPropertyTree();
    boost::property_tree::ptree& outductCapabilityTelemetryListPt = pt.put_child("outductCapabilityTelemetryList",
        outductCapabilityTelemetryList.empty() ? boost::property_tree::ptree("[]") : boost::property_tree::ptree());
    for (std::list<OutductCapabilityTelemetry_t>::const_iterator it = outductCapabilityTelemetryList.cbegin(); it != outductCapabilityTelemetryList.cend(); ++it) {
        const OutductCapabilityTelemetry_t& oct = *it;
        outductCapabilityTelemetryListPt.push_back(std::make_pair("", oct.GetNewPropertyTree())); //using "" as key creates json array
    }
    return pt;
}
bool AllOutductCapabilitiesTelemetry_t::operator==(const AllOutductCapabilitiesTelemetry_t& o) const {
    return Telemetry_t::operator==(o) && (outductCapabilityTelemetryList == o.outductCapabilityTelemetryList);
}
bool AllOutductCapabilitiesTelemetry_t::operator!=(const AllOutductCapabilitiesTelemetry_t& o) const {
    return !(*this == o);
}
AllOutductCapabilitiesTelemetry_t::AllOutductCapabilitiesTelemetry_t(const AllOutductCapabilitiesTelemetry_t& o) :
    Telemetry_t(o),
    outductCapabilityTelemetryList(o.outductCapabilityTelemetryList) { } //a copy constructor: X(const X&)
AllOutductCapabilitiesTelemetry_t::AllOutductCapabilitiesTelemetry_t(AllOutductCapabilitiesTelemetry_t&& o) :
    Telemetry_t(std::move(o)),
    outductCapabilityTelemetryList(std::move(o.outductCapabilityTelemetryList)) { } //a move constructor: X(X&&)
AllOutductCapabilitiesTelemetry_t& AllOutductCapabilitiesTelemetry_t::operator=(const AllOutductCapabilitiesTelemetry_t& o) { //a copy assignment: operator=(const X&)
    Telemetry_t::operator=(o);
    outductCapabilityTelemetryList = o.outductCapabilityTelemetryList;
    return *this;
}
AllOutductCapabilitiesTelemetry_t& AllOutductCapabilitiesTelemetry_t::operator=(AllOutductCapabilitiesTelemetry_t&& o) { //a move assignment: operator=(X&&)
    Telemetry_t::operator=(std::move(o));
    outductCapabilityTelemetryList = std::move(o.outductCapabilityTelemetryList);
    return *this;
}
std::ostream& operator<<(std::ostream& os, const AllOutductCapabilitiesTelemetry_t& o) {
    os << "AllOutductCapabilitiesTelemetry:" << std::endl;
    for (std::list<OutductCapabilityTelemetry_t>::const_iterator it = o.outductCapabilityTelemetryList.cbegin(); it != o.outductCapabilityTelemetryList.cend(); ++it) {
        os << (*it);
    }
    return os;
}














/////////////////////////////////////
//InductConnectionTelemetry_t
/////////////////////////////////////

InductConnectionTelemetry_t::InductConnectionTelemetry_t() : m_totalBundlesReceived(0), m_totalBundleBytesReceived(0) {}
bool InductConnectionTelemetry_t::operator==(const InductConnectionTelemetry_t& o) const {
    return (m_connectionName == o.m_connectionName)
        && (m_totalBundlesReceived == o.m_totalBundlesReceived)
        && (m_totalBundleBytesReceived == o.m_totalBundleBytesReceived);
}
bool InductConnectionTelemetry_t::operator!=(const InductConnectionTelemetry_t& o) const {
    return !(*this == o);
}
bool InductConnectionTelemetry_t::SetValuesFromPropertyTree(const boost::property_tree::ptree& pt) {
    try {
        m_connectionName = pt.get<std::string>("connectionName");
        m_inputName = pt.get<std::string>("inputName");
        m_totalBundlesReceived = pt.get<uint64_t>("totalBundlesReceived");
        m_totalBundleBytesReceived = pt.get<uint64_t>("totalBundleBytesReceived");
    }
    catch (const boost::property_tree::ptree_error& e) {
        LOG_ERROR(subprocess) << "parsing JSON InductConnectionTelemetry_t: " << e.what();
        return false;
    }
    return true;
}
boost::property_tree::ptree InductConnectionTelemetry_t::GetNewPropertyTree() const {
    boost::property_tree::ptree pt;
    pt.put("connectionName", m_connectionName);
    pt.put("inputName", m_inputName);
    pt.put("totalBundlesReceived", m_totalBundlesReceived);
    pt.put("totalBundleBytesReceived", m_totalBundleBytesReceived);
    return pt;
}


InductTelemetry_t::InductTelemetry_t() {}
bool InductTelemetry_t::operator==(const InductTelemetry_t& o) const {
    return (m_listInductConnections == o.m_listInductConnections);
}
bool InductTelemetry_t::operator!=(const InductTelemetry_t& o) const {
    return !(*this == o);
}
bool InductTelemetry_t::SetValuesFromPropertyTree(const boost::property_tree::ptree& pt) {
    try {
        m_convergenceLayer = pt.get<std::string>("convergenceLayer");
        //for non-throw versions of get_child which return a reference to the second parameter
        static const boost::property_tree::ptree EMPTY_PTREE;
        const boost::property_tree::ptree& inductConnectionsPt = pt.get_child("inductConnections", EMPTY_PTREE); //non-throw version
        m_listInductConnections.clear();
        BOOST_FOREACH(const boost::property_tree::ptree::value_type & inductConnectionPt, inductConnectionsPt) {
            m_listInductConnections.emplace_back();
            m_listInductConnections.back().SetValuesFromPropertyTree(inductConnectionPt.second);
        }
    }
    catch (const boost::property_tree::ptree_error& e) {
        LOG_ERROR(subprocess) << "parsing JSON InductTelemetry_t: " << e.what();
        return false;
    }
    return true;
}
boost::property_tree::ptree InductTelemetry_t::GetNewPropertyTree() const {
    boost::property_tree::ptree pt;
    pt.put("convergenceLayer", m_convergenceLayer);
    boost::property_tree::ptree& inductConnectionsPt = pt.put_child("inductConnections",
        m_listInductConnections.empty() ? boost::property_tree::ptree("[]") : boost::property_tree::ptree());
    for (std::list<InductConnectionTelemetry_t>::const_iterator it = m_listInductConnections.cbegin(); it != m_listInductConnections.cend(); ++it) {
        const InductConnectionTelemetry_t& ict = *it;
        inductConnectionsPt.push_back(std::make_pair("", ict.GetNewPropertyTree())); //using "" as key creates json array
    }
    return pt;
}


AllInductTelemetry_t::AllInductTelemetry_t() : m_timestampMilliseconds(0) {}
bool AllInductTelemetry_t::operator==(const AllInductTelemetry_t& o) const {
    return (m_listAllInducts == o.m_listAllInducts);
}
bool AllInductTelemetry_t::operator!=(const AllInductTelemetry_t& o) const {
    return !(*this == o);
}
bool AllInductTelemetry_t::SetValuesFromPropertyTree(const boost::property_tree::ptree& pt) {
    try {
        m_timestampMilliseconds = pt.get<uint64_t>("timestampMilliseconds");
        //for non-throw versions of get_child which return a reference to the second parameter
        static const boost::property_tree::ptree EMPTY_PTREE;
        const boost::property_tree::ptree& allInductsPt = pt.get_child("allInducts", EMPTY_PTREE); //non-throw version
        m_listAllInducts.clear();
        BOOST_FOREACH(const boost::property_tree::ptree::value_type & inductPt, allInductsPt) {
            m_listAllInducts.emplace_back();
            m_listAllInducts.back().SetValuesFromPropertyTree(inductPt.second);
        }
    }
    catch (const boost::property_tree::ptree_error& e) {
        LOG_ERROR(subprocess) << "parsing JSON AllInductTelemetry_t: " << e.what();
        return false;
    }
    return true;
}
boost::property_tree::ptree AllInductTelemetry_t::GetNewPropertyTree() const {
    boost::property_tree::ptree pt;
    pt.put("timestampMilliseconds", m_timestampMilliseconds);
    boost::property_tree::ptree& allInductsPt = pt.put_child("allInducts",
        m_listAllInducts.empty() ? boost::property_tree::ptree("[]") : boost::property_tree::ptree());
    for (std::list<InductTelemetry_t>::const_iterator it = m_listAllInducts.cbegin(); it != m_listAllInducts.cend(); ++it) {
        const InductTelemetry_t& induct = *it;
        allInductsPt.push_back(std::make_pair("", induct.GetNewPropertyTree())); //using "" as key creates json array
    }
    return pt;
}




/////////////////////////////////////
//OutductTelemetry2_t
/////////////////////////////////////
OutductTelemetry2_t::OutductTelemetry2_t() :
    m_totalBundlesAcked(0),
    m_totalBundleBytesAcked(0),
    m_totalBundlesSent(0),
    m_totalBundleBytesSent(0),
    m_totalBundlesFailedToSend(0)
{
}

OutductTelemetry2_t::~OutductTelemetry2_t() {};
bool OutductTelemetry2_t::operator==(const OutductTelemetry2_t& o) const {
    return (m_convergenceLayer == o.m_convergenceLayer)
        && (m_totalBundlesAcked == o.m_totalBundlesAcked)
        && (m_totalBundleBytesAcked == o.m_totalBundleBytesAcked)
        && (m_totalBundlesSent == o.m_totalBundlesSent)
        && (m_totalBundleBytesSent == o.m_totalBundleBytesSent)
        && (m_totalBundlesFailedToSend == o.m_totalBundlesFailedToSend);
}
bool OutductTelemetry2_t::operator!=(const OutductTelemetry2_t& o) const {
    return !(*this == o);
}

bool OutductTelemetry2_t::SetValuesFromPropertyTree(const boost::property_tree::ptree& pt) {
    try {
        m_convergenceLayer = pt.get<std::string>("convergenceLayer");
        m_totalBundlesAcked = pt.get<uint64_t>("totalBundlesAcked");
        m_totalBundleBytesAcked = pt.get<uint64_t>("totalBundleBytesAcked");
        m_totalBundlesSent = pt.get<uint64_t>("totalBundlesSent");
        m_totalBundleBytesSent = pt.get<uint64_t>("totalBundleBytesSent");
        m_totalBundlesFailedToSend = pt.get<uint64_t>("totalBundlesFailedToSend");
    }
    catch (const boost::property_tree::ptree_error& e) {
        LOG_ERROR(subprocess) << "parsing JSON OutductTelemetry_t: " << e.what();
        return false;
    }
    return true;
}

boost::property_tree::ptree OutductTelemetry2_t::GetNewPropertyTree() const {
    boost::property_tree::ptree pt;
    pt.put("convergenceLayer", m_convergenceLayer);
    pt.put("totalBundlesAcked", m_totalBundlesAcked);
    pt.put("totalBundleBytesAcked", m_totalBundleBytesAcked);
    pt.put("totalBundlesSent", m_totalBundlesSent);
    pt.put("totalBundleBytesSent", m_totalBundleBytesSent);
    pt.put("totalBundlesFailedToSend", m_totalBundlesFailedToSend);
    return pt;
}

StcpOutductTelemetry2_t::StcpOutductTelemetry2_t() :
    OutductTelemetry2_t(),
    m_totalStcpBytesSent(0)
{
    m_convergenceLayer = "stcp";
}

StcpOutductTelemetry2_t::~StcpOutductTelemetry2_t() {};
bool StcpOutductTelemetry2_t::operator==(const OutductTelemetry2_t& o) const {
    if (const StcpOutductTelemetry2_t* oPtr = dynamic_cast<const StcpOutductTelemetry2_t*>(&o)) {
        return OutductTelemetry2_t::operator==(o)
            && (m_totalStcpBytesSent == oPtr->m_totalStcpBytesSent);
    }
    return false;
}
bool StcpOutductTelemetry2_t::operator!=(const OutductTelemetry2_t& o) const {
    return !(*this == o);
}

bool StcpOutductTelemetry2_t::SetValuesFromPropertyTree(const boost::property_tree::ptree& pt) {
    if (!OutductTelemetry2_t::SetValuesFromPropertyTree(pt)) {
        return false;
    }
    try {
        m_totalStcpBytesSent = pt.get<uint64_t>("totalStcpBytesSent");
    }
    catch (const boost::property_tree::ptree_error& e) {
        LOG_ERROR(subprocess) << "parsing JSON StcpOutductTelemetry_t: " << e.what();
        return false;
    }
    return true;
}

boost::property_tree::ptree StcpOutductTelemetry2_t::GetNewPropertyTree() const {
    boost::property_tree::ptree pt = OutductTelemetry2_t::GetNewPropertyTree();
    pt.put("totalStcpBytesSent", m_totalStcpBytesSent);
    return pt;
}

LtpOutductTelemetry2_t::LtpOutductTelemetry2_t() :
    OutductTelemetry2_t(),
    m_numCheckpointsExpired(0), m_numDiscretionaryCheckpointsNotResent(0), m_countUdpPacketsSent(0),
    m_countRxUdpCircularBufferOverruns(0), m_countTxUdpPacketsLimitedByRate(0)
{
    m_convergenceLayer = "ltp_over_udp";
}

LtpOutductTelemetry2_t::~LtpOutductTelemetry2_t() {};
bool LtpOutductTelemetry2_t::operator==(const OutductTelemetry2_t& o) const {
    if (const LtpOutductTelemetry2_t* oPtr = dynamic_cast<const LtpOutductTelemetry2_t*>(&o)) {
        return OutductTelemetry2_t::operator==(o)
            && (m_numCheckpointsExpired == oPtr->m_numCheckpointsExpired)
            && (m_numDiscretionaryCheckpointsNotResent == oPtr->m_numDiscretionaryCheckpointsNotResent)
            && (m_countUdpPacketsSent == oPtr->m_countUdpPacketsSent)
            && (m_countRxUdpCircularBufferOverruns == oPtr->m_countRxUdpCircularBufferOverruns)
            && (m_countTxUdpPacketsLimitedByRate == oPtr->m_countTxUdpPacketsLimitedByRate);
    }
    return false;
}
bool LtpOutductTelemetry2_t::operator!=(const OutductTelemetry2_t& o) const {
    return !(*this == o);
}

bool LtpOutductTelemetry2_t::SetValuesFromPropertyTree(const boost::property_tree::ptree& pt) {
    if (!OutductTelemetry2_t::SetValuesFromPropertyTree(pt)) {
        return false;
    }
    try {
        m_numCheckpointsExpired = pt.get<uint64_t>("numCheckpointsExpired");
        m_numDiscretionaryCheckpointsNotResent = pt.get<uint64_t>("numDiscretionaryCheckpointsNotResent");
        m_countUdpPacketsSent = pt.get<uint64_t>("countUdpPacketsSent");
        m_countRxUdpCircularBufferOverruns = pt.get<uint64_t>("countRxUdpCircularBufferOverruns");
        m_countTxUdpPacketsLimitedByRate = pt.get<uint64_t>("countTxUdpPacketsLimitedByRate");
    }
    catch (const boost::property_tree::ptree_error& e) {
        LOG_ERROR(subprocess) << "parsing JSON StcpOutductTelemetry_t: " << e.what();
        return false;
    }
    return true;
}

boost::property_tree::ptree LtpOutductTelemetry2_t::GetNewPropertyTree() const {
    boost::property_tree::ptree pt = OutductTelemetry2_t::GetNewPropertyTree();
    pt.put("numCheckpointsExpired", m_numCheckpointsExpired);
    pt.put("numDiscretionaryCheckpointsNotResent", m_numDiscretionaryCheckpointsNotResent);
    pt.put("countUdpPacketsSent", m_countUdpPacketsSent);
    pt.put("countRxUdpCircularBufferOverruns", m_countRxUdpCircularBufferOverruns);
    pt.put("countTxUdpPacketsLimitedByRate", m_countTxUdpPacketsLimitedByRate);
    return pt;
}


TcpclV3OutductTelemetry_t::TcpclV3OutductTelemetry_t() :
    OutductTelemetry2_t(),
    m_totalFragmentsAcked(0), m_totalFragmentsSent(0), m_totalBundlesReceived(0),
    m_totalBundleBytesReceived(0)
{
    m_convergenceLayer = "tcpcl_v3";
}
TcpclV3OutductTelemetry_t::~TcpclV3OutductTelemetry_t() {};
bool TcpclV3OutductTelemetry_t::operator==(const OutductTelemetry2_t& o) const {
    if (const TcpclV3OutductTelemetry_t* oPtr = dynamic_cast<const TcpclV3OutductTelemetry_t*>(&o)) {
        return OutductTelemetry2_t::operator==(o)
            && (m_totalFragmentsAcked == oPtr->m_totalFragmentsAcked)
            && (m_totalFragmentsSent == oPtr->m_totalFragmentsSent)
            && (m_totalBundlesReceived == oPtr->m_totalBundlesReceived)
            && (m_totalBundleBytesReceived == oPtr->m_totalBundleBytesReceived);
    }
    return false;
}
bool TcpclV3OutductTelemetry_t::operator!=(const OutductTelemetry2_t& o) const {
    return !(*this == o);
}
bool TcpclV3OutductTelemetry_t::SetValuesFromPropertyTree(const boost::property_tree::ptree& pt) {
    if (!OutductTelemetry2_t::SetValuesFromPropertyTree(pt)) {
        return false;
    }
    try {
        m_totalFragmentsAcked = pt.get<uint64_t>("totalFragmentsAcked");
        m_totalFragmentsSent = pt.get<uint64_t>("totalFragmentsSent");
        m_totalBundlesReceived = pt.get<uint64_t>("totalBundlesReceived");
        m_totalBundleBytesReceived = pt.get<uint64_t>("totalBundleBytesReceived");
    }
    catch (const boost::property_tree::ptree_error& e) {
        LOG_ERROR(subprocess) << "parsing JSON StcpOutductTelemetry_t: " << e.what();
        return false;
    }
    return true;
}
boost::property_tree::ptree TcpclV3OutductTelemetry_t::GetNewPropertyTree() const {
    boost::property_tree::ptree pt = OutductTelemetry2_t::GetNewPropertyTree();
    pt.put("totalFragmentsAcked", m_totalFragmentsAcked);
    pt.put("totalFragmentsSent", m_totalFragmentsSent);
    pt.put("totalBundlesReceived", m_totalBundlesReceived);
    pt.put("totalBundleBytesReceived", m_totalBundleBytesReceived);
    return pt;
}

TcpclV4OutductTelemetry_t::TcpclV4OutductTelemetry_t() :
    OutductTelemetry2_t(),
    m_totalFragmentsAcked(0), m_totalFragmentsSent(0), m_totalBundlesReceived(0),
    m_totalBundleBytesReceived(0)
{
    m_convergenceLayer = "tcpcl_v4";
}
TcpclV4OutductTelemetry_t::~TcpclV4OutductTelemetry_t() {};
bool TcpclV4OutductTelemetry_t::operator==(const OutductTelemetry2_t& o) const {
    if (const TcpclV4OutductTelemetry_t* oPtr = dynamic_cast<const TcpclV4OutductTelemetry_t*>(&o)) {
        return OutductTelemetry2_t::operator==(o)
            && (m_totalFragmentsAcked == oPtr->m_totalFragmentsAcked)
            && (m_totalFragmentsSent == oPtr->m_totalFragmentsSent)
            && (m_totalBundlesReceived == oPtr->m_totalBundlesReceived)
            && (m_totalBundleBytesReceived == oPtr->m_totalBundleBytesReceived);
    }
    return false;
}
bool TcpclV4OutductTelemetry_t::operator!=(const OutductTelemetry2_t& o) const {
    return !(*this == o);
}
bool TcpclV4OutductTelemetry_t::SetValuesFromPropertyTree(const boost::property_tree::ptree& pt) {
    if (!OutductTelemetry2_t::SetValuesFromPropertyTree(pt)) {
        return false;
    }
    try {
        m_totalFragmentsAcked = pt.get<uint64_t>("totalFragmentsAcked");
        m_totalFragmentsSent = pt.get<uint64_t>("totalFragmentsSent");
        m_totalBundlesReceived = pt.get<uint64_t>("totalBundlesReceived");
        m_totalBundleBytesReceived = pt.get<uint64_t>("totalBundleBytesReceived");
    }
    catch (const boost::property_tree::ptree_error& e) {
        LOG_ERROR(subprocess) << "parsing JSON StcpOutductTelemetry_t: " << e.what();
        return false;
    }
    return true;
}
boost::property_tree::ptree TcpclV4OutductTelemetry_t::GetNewPropertyTree() const {
    boost::property_tree::ptree pt = OutductTelemetry2_t::GetNewPropertyTree();
    pt.put("totalFragmentsAcked", m_totalFragmentsAcked);
    pt.put("totalFragmentsSent", m_totalFragmentsSent);
    pt.put("totalBundlesReceived", m_totalBundlesReceived);
    pt.put("totalBundleBytesReceived", m_totalBundleBytesReceived);
    return pt;
}


UdpOutductTelemetry_t::UdpOutductTelemetry_t() :
    OutductTelemetry2_t(),
    m_totalPacketsSent(0), m_totalPacketBytesSent(0), m_totalPacketsDequeuedForSend(0),
    m_totalPacketBytesDequeuedForSend(0), m_totalPacketsLimitedByRate(0)
{
    m_convergenceLayer = "udp";
}
UdpOutductTelemetry_t::~UdpOutductTelemetry_t() {};
bool UdpOutductTelemetry_t::operator==(const OutductTelemetry2_t& o) const {
    if (const UdpOutductTelemetry_t* oPtr = dynamic_cast<const UdpOutductTelemetry_t*>(&o)) {
        return OutductTelemetry2_t::operator==(o)
            && (m_totalPacketsSent == oPtr->m_totalPacketsSent)
            && (m_totalPacketBytesSent == oPtr->m_totalPacketBytesSent)
            && (m_totalPacketsDequeuedForSend == oPtr->m_totalPacketsDequeuedForSend)
            && (m_totalPacketBytesDequeuedForSend == oPtr->m_totalPacketBytesDequeuedForSend)
            && (m_totalPacketsLimitedByRate == oPtr->m_totalPacketsLimitedByRate);
    }
    return false;
}
bool UdpOutductTelemetry_t::operator!=(const OutductTelemetry2_t& o) const {
    return !(*this == o);
}
bool UdpOutductTelemetry_t::SetValuesFromPropertyTree(const boost::property_tree::ptree& pt) {
    if (!OutductTelemetry2_t::SetValuesFromPropertyTree(pt)) {
        return false;
    }
    try {
        m_totalPacketsSent = pt.get<uint64_t>("totalPacketsSent");
        m_totalPacketBytesSent = pt.get<uint64_t>("totalPacketBytesSent");
        m_totalPacketsDequeuedForSend = pt.get<uint64_t>("totalPacketsDequeuedForSend");
        m_totalPacketBytesDequeuedForSend = pt.get<uint64_t>("totalPacketBytesDequeuedForSend");
        m_totalPacketsLimitedByRate = pt.get<uint64_t>("totalPacketsLimitedByRate");
    }
    catch (const boost::property_tree::ptree_error& e) {
        LOG_ERROR(subprocess) << "parsing JSON StcpOutductTelemetry_t: " << e.what();
        return false;
    }
    return true;
}
boost::property_tree::ptree UdpOutductTelemetry_t::GetNewPropertyTree() const {
    boost::property_tree::ptree pt = OutductTelemetry2_t::GetNewPropertyTree();
    pt.put("totalPacketsSent", m_totalPacketsSent);
    pt.put("totalPacketBytesSent", m_totalPacketBytesSent);
    pt.put("totalPacketsDequeuedForSend", m_totalPacketsDequeuedForSend);
    pt.put("totalPacketBytesDequeuedForSend", m_totalPacketBytesDequeuedForSend);
    pt.put("totalPacketsLimitedByRate", m_totalPacketsLimitedByRate);
    return pt;
}

AllOutductTelemetry_t::AllOutductTelemetry_t() : m_timestampMilliseconds(0) {}
static bool UniquePtrOutductTelemEquivalent(const std::unique_ptr<OutductTelemetry2_t>& a, const std::unique_ptr<OutductTelemetry2_t>& b) {
    if ((!a) && (!b)) return true; //both null
    if (!a) return false;
    if (!b) return false;
    return ((*a) == (*b));
}
bool AllOutductTelemetry_t::operator==(const AllOutductTelemetry_t& o) const {
    return std::equal(m_listAllOutducts.begin(), m_listAllOutducts.end(), o.m_listAllOutducts.begin(), UniquePtrOutductTelemEquivalent);
}
bool AllOutductTelemetry_t::operator!=(const AllOutductTelemetry_t& o) const {
    return !(*this == o);
}
bool AllOutductTelemetry_t::SetValuesFromPropertyTree(const boost::property_tree::ptree& pt) {
    try {
        m_timestampMilliseconds = pt.get<uint64_t>("timestampMilliseconds");
        //for non-throw versions of get_child which return a reference to the second parameter
        static const boost::property_tree::ptree EMPTY_PTREE;
        const boost::property_tree::ptree& allOutductsPt = pt.get_child("allOutducts", EMPTY_PTREE); //non-throw version
        m_listAllOutducts.clear();
        BOOST_FOREACH(const boost::property_tree::ptree::value_type & outductPt, allOutductsPt) {
            const std::string convergenceLayer = outductPt.second.get<std::string>("convergenceLayer");
            
            if (convergenceLayer == "ltp_over_udp") {
                m_listAllOutducts.emplace_back(std::make_unique<LtpOutductTelemetry2_t>());
            }
            else if (convergenceLayer == "udp") {
                m_listAllOutducts.emplace_back(std::make_unique<UdpOutductTelemetry_t>());
            }
            else if (convergenceLayer == "tcpcl_v3") {
                m_listAllOutducts.emplace_back(std::make_unique<TcpclV3OutductTelemetry_t>());
            }
            else if (convergenceLayer == "tcpcl_v4") {
                m_listAllOutducts.emplace_back(std::make_unique<TcpclV4OutductTelemetry_t>());
            }
            else if (convergenceLayer == "stcp") {
                m_listAllOutducts.emplace_back(std::make_unique<StcpOutductTelemetry2_t>());
            }
            else {
                return false;
            }
            m_listAllOutducts.back()->SetValuesFromPropertyTree(outductPt.second);
        }
    }
    catch (const boost::property_tree::ptree_error& e) {
        LOG_ERROR(subprocess) << "parsing JSON AllInductTelemetry_t: " << e.what();
        return false;
    }
    return true;
}
boost::property_tree::ptree AllOutductTelemetry_t::GetNewPropertyTree() const {
    boost::property_tree::ptree pt;
    pt.put("timestampMilliseconds", m_timestampMilliseconds);
    boost::property_tree::ptree& allInductsPt = pt.put_child("allOutducts",
        m_listAllOutducts.empty() ? boost::property_tree::ptree("[]") : boost::property_tree::ptree());
    for (std::list<std::unique_ptr<OutductTelemetry2_t> >::const_iterator it = m_listAllOutducts.cbegin(); it != m_listAllOutducts.cend(); ++it) {
        const std::unique_ptr<OutductTelemetry2_t>& outductPtr = *it;
        if (!outductPtr) {
            break;
        }
        allInductsPt.push_back(std::make_pair("", outductPtr->GetNewPropertyTree())); //using "" as key creates json array
    }
    return pt;
}
