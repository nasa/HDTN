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

static constexpr hdtn::Logger::SubProcess subprocess = hdtn::Logger::SubProcess::none;


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
Telemetry_t::Telemetry_t()
    :m_type(0)
{
    m_fieldsToSerialize.push_back(&m_type);
}

Telemetry_t::~Telemetry_t() {};

uint64_t Telemetry_t::SerializeToLittleEndian(uint8_t* buffer, uint64_t bufferSize)
{
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

uint64_t Telemetry_t::GetSerializationSize()
{
    return m_fieldsToSerialize.size() * sizeof(uint64_t);
}

uint64_t Telemetry_t::DeserializeFromLittleEndian(uint8_t* buffer, uint64_t bufferSize)
{
    uint64_t numBytes = Telemetry_t::GetSerializationSize();
    if (bufferSize < numBytes) {
        return 0;
    }
    uint64_t* buffer64 = reinterpret_cast<uint64_t*>(buffer);
    for (uint64_t i = 0; i < m_fieldsToSerialize.size(); ++i) {
        *m_fieldsToSerialize[i] = boost::endian::little_to_native(*buffer64);
        buffer64++;
    }
    return numBytes;
}

/////////////////////////////////////
//IngressTelemetry_t
/////////////////////////////////////
IngressTelemetry_t::IngressTelemetry_t() 
    :totalDataBytes(0), bundleCountEgress(0), bundleCountStorage(0)
{
    Telemetry_t::m_type = uint64_t(TelemetryType::ingress);
    Telemetry_t::m_fieldsToSerialize.insert(m_fieldsToSerialize.end(), {
        &totalDataBytes,
        &bundleCountEgress,
        &bundleCountStorage
    });
}

IngressTelemetry_t::~IngressTelemetry_t() {};

/////////////////////////////////////
//EgressTelemetry_t
/////////////////////////////////////
EgressTelemetry_t::EgressTelemetry_t()
    :totalDataBytes(0), egressBundleCount(0), egressMessageCount(0)
{
    Telemetry_t::m_type = uint64_t(TelemetryType::egress);
    Telemetry_t::m_fieldsToSerialize.insert(m_fieldsToSerialize.end(), {
        &egressBundleCount,
        &totalDataBytes,
        &egressMessageCount
    });
}

EgressTelemetry_t::~EgressTelemetry_t() {};

/////////////////////////////////////
//StorageTelemetry_t
/////////////////////////////////////
StorageTelemetry_t::StorageTelemetry_t() 
    :totalBundlesErasedFromStorage(0), totalBundlesSentToEgressFromStorage(0),
        usedSpaceBytes(0), freeSpaceBytes(0)
{
    Telemetry_t::m_type = uint64_t(TelemetryType::storage);
    Telemetry_t::m_fieldsToSerialize.insert(m_fieldsToSerialize.end(), {
        &totalBundlesErasedFromStorage,
        &totalBundlesSentToEgressFromStorage,
        &usedSpaceBytes,
        &freeSpaceBytes
    });
}

StorageTelemetry_t::~StorageTelemetry_t() {};

/////////////////////////////////////
//OutductTelemetry_t
/////////////////////////////////////
OutductTelemetry_t::OutductTelemetry_t()
    : totalBundlesAcked(0), totalBundleBytesAcked(0), totalBundlesSent(0), totalBundleBytesSent(0),
    totalBundlesFailedToSend(0), convergenceLayerType(0)
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

StcpOutductTelemetry_t::StcpOutductTelemetry_t()
    : OutductTelemetry_t(), totalStcpBytesSent(0)
{
    OutductTelemetry_t::convergenceLayerType = 1;
    Telemetry_t::m_type = uint64_t(TelemetryType::stcpoutduct);
    Telemetry_t::m_fieldsToSerialize.insert(m_fieldsToSerialize.end(), {
        &totalStcpBytesSent
    });
}

StcpOutductTelemetry_t::~StcpOutductTelemetry_t() {};

LtpOutductTelemetry_t::LtpOutductTelemetry_t() : OutductTelemetry_t(),
    numCheckpointsExpired(0), numDiscretionaryCheckpointsNotResent(0), countUdpPacketsSent(0),
    countRxUdpCircularBufferOverruns(0), countTxUdpPacketsLimitedByRate(0)
{
    OutductTelemetry_t::convergenceLayerType = 2;
    Telemetry_t::m_type = uint64_t(TelemetryType::ltpoutduct);
    Telemetry_t::m_fieldsToSerialize.insert(m_fieldsToSerialize.end(), {
        &numCheckpointsExpired,
        &numDiscretionaryCheckpointsNotResent,
        &countUdpPacketsSent,
        &countRxUdpCircularBufferOverruns,
        &countTxUdpPacketsLimitedByRate,
    });
}

LtpOutductTelemetry_t::~LtpOutductTelemetry_t() {};

/////////////////////////////////////
//StorageExpiringBeforeThresholdTelemetry_t
/////////////////////////////////////
StorageExpiringBeforeThresholdTelemetry_t::StorageExpiringBeforeThresholdTelemetry_t()
    : priority(0), thresholdSecondsSinceStartOfYear2000(0)
{
    Telemetry_t::m_type = uint64_t(TelemetryType::storageExpiringBeforeThreshold);
    Telemetry_t::m_fieldsToSerialize.insert(m_fieldsToSerialize.end(), {
        &priority,
        &thresholdSecondsSinceStartOfYear2000,
    });
}

StorageExpiringBeforeThresholdTelemetry_t::~StorageExpiringBeforeThresholdTelemetry_t() {}

static const uint64_t mapEntrySize = sizeof(uint64_t) +
    sizeof(StorageExpiringBeforeThresholdTelemetry_t::bundle_count_plus_bundle_bytes_pair_t);

uint64_t StorageExpiringBeforeThresholdTelemetry_t::SerializeToLittleEndian(uint8_t* data, uint64_t bufferSize)
{
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
    *data64Ptr++ = boost::endian::native_to_little(static_cast<uint64_t>(map_node_id_to_expiring_before_threshold_count.size()));
    bufferSize -= 8;
    bytesSerialized += 8;

    // Serialize the map values
    for (std::map<uint64_t, bundle_count_plus_bundle_bytes_pair_t>::const_iterator it = map_node_id_to_expiring_before_threshold_count.cbegin();
        it != map_node_id_to_expiring_before_threshold_count.cend();
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

uint64_t StorageExpiringBeforeThresholdTelemetry_t::GetSerializationSize()
{
    uint64_t size = Telemetry_t::GetSerializationSize();
    size += sizeof(uint64_t);
    for (std::map<uint64_t, bundle_count_plus_bundle_bytes_pair_t>::const_iterator it = map_node_id_to_expiring_before_threshold_count.cbegin();
        it != map_node_id_to_expiring_before_threshold_count.cend();
        ++it)
    {
        size += mapEntrySize;
    }
    return size;
}

/////////////////////////////////////
//TelemetryFactory
/////////////////////////////////////
std::vector<std::unique_ptr<Telemetry_t> > TelemetryFactory::DeserializeFromLittleEndian(uint8_t* buffer, uint64_t bufferSize)
{
    std::vector<std::unique_ptr<Telemetry_t> > telemList;
    while (bufferSize > 0) {
        // First determine the type
        uint64_t* buffer64 = reinterpret_cast<uint64_t*>(buffer);
        TelemetryType type = TelemetryType(*buffer64);

        // Attempt to deserialize
        std::unique_ptr<Telemetry_t> telem;
        switch (type) {
            case ingress:
                telem = boost::make_unique<IngressTelemetry_t>();
                break;
            case egress:
                telem = boost::make_unique<EgressTelemetry_t>();
                break;
            case storage:
                telem = boost::make_unique<StorageTelemetry_t>();
                break;
            case stcpoutduct:
                telem = boost::make_unique<StcpOutductTelemetry_t>();
                break;
            case ltpoutduct:
                telem = boost::make_unique<LtpOutductTelemetry_t>();
                break;
            default:
                throw TelemetryFactory::TelemetryDeserializeUnknownTypeException();
        };
        uint64_t bytesWritten = telem->DeserializeFromLittleEndian(buffer, bufferSize);
        if (bytesWritten == 0) {
            // We know the buffer isn't empty. So if nothing was written, there is
            // a formatting issue.
            throw TelemetryFactory::TelemetryDeserializeInvalidFormatException();
        }
        telemList.push_back(std::move(telem));

        bufferSize -= bytesWritten;
        buffer += bytesWritten;
    }
    return telemList;
}

StorageTelemetryRequest_t::StorageTelemetryRequest_t() : type(10) {}
OutductCapabilityTelemetry_t::OutductCapabilityTelemetry_t() : type(5), outductArrayIndex(0), maxBundlesInPipeline(0), maxBundleSizeBytesInPipeline(0) {}
AllOutductCapabilitiesTelemetry_t::AllOutductCapabilitiesTelemetry_t() : type(6) {}

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
    *data64Ptr++ = boost::endian::native_to_little(type);
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
    type = boost::endian::little_to_native(*(reinterpret_cast<const uint64_t*>(serialization)));
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
    return (type == o.type)
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
    type(o.type),
    outductArrayIndex(o.outductArrayIndex),
    maxBundlesInPipeline(o.maxBundlesInPipeline),
    maxBundleSizeBytesInPipeline(o.maxBundleSizeBytesInPipeline),
    nextHopNodeId(o.nextHopNodeId),
    finalDestinationEidList(o.finalDestinationEidList),
    finalDestinationNodeIdList(o.finalDestinationNodeIdList) { } //a copy constructor: X(const X&)
OutductCapabilityTelemetry_t::OutductCapabilityTelemetry_t(OutductCapabilityTelemetry_t&& o) :
    type(o.type),
    outductArrayIndex(o.outductArrayIndex),
    maxBundlesInPipeline(o.maxBundlesInPipeline),
    maxBundleSizeBytesInPipeline(o.maxBundleSizeBytesInPipeline),
    nextHopNodeId(o.nextHopNodeId),
    finalDestinationEidList(std::move(o.finalDestinationEidList)),
    finalDestinationNodeIdList(std::move(o.finalDestinationNodeIdList)) { } //a move constructor: X(X&&)
OutductCapabilityTelemetry_t& OutductCapabilityTelemetry_t::operator=(const OutductCapabilityTelemetry_t& o) { //a copy assignment: operator=(const X&)
    type = o.type;
    outductArrayIndex = o.outductArrayIndex;
    maxBundlesInPipeline = o.maxBundlesInPipeline;
    maxBundleSizeBytesInPipeline = o.maxBundleSizeBytesInPipeline;
    nextHopNodeId = o.nextHopNodeId;
    finalDestinationEidList = o.finalDestinationEidList;
    finalDestinationNodeIdList = o.finalDestinationNodeIdList;
    return *this;
}
OutductCapabilityTelemetry_t& OutductCapabilityTelemetry_t::operator=(OutductCapabilityTelemetry_t&& o) { //a move assignment: operator=(X&&)
    type = o.type;
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
    *data64Ptr++ = boost::endian::native_to_little(type);
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
    type = boost::endian::little_to_native(*(reinterpret_cast<const uint64_t*>(serialization)));
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
bool AllOutductCapabilitiesTelemetry_t::operator==(const AllOutductCapabilitiesTelemetry_t& o) const {
    return (type == o.type) && (outductCapabilityTelemetryList == o.outductCapabilityTelemetryList);
}
bool AllOutductCapabilitiesTelemetry_t::operator!=(const AllOutductCapabilitiesTelemetry_t& o) const {
    return !(*this == o);
}
AllOutductCapabilitiesTelemetry_t::AllOutductCapabilitiesTelemetry_t(const AllOutductCapabilitiesTelemetry_t& o) :
    type(o.type),
    outductCapabilityTelemetryList(o.outductCapabilityTelemetryList) { } //a copy constructor: X(const X&)
AllOutductCapabilitiesTelemetry_t::AllOutductCapabilitiesTelemetry_t(AllOutductCapabilitiesTelemetry_t&& o) :
    type(o.type),
    outductCapabilityTelemetryList(std::move(o.outductCapabilityTelemetryList)) { } //a move constructor: X(X&&)
AllOutductCapabilitiesTelemetry_t& AllOutductCapabilitiesTelemetry_t::operator=(const AllOutductCapabilitiesTelemetry_t& o) { //a copy assignment: operator=(const X&)
    type = o.type;
    outductCapabilityTelemetryList = o.outductCapabilityTelemetryList;
    return *this;
}
AllOutductCapabilitiesTelemetry_t& AllOutductCapabilitiesTelemetry_t::operator=(AllOutductCapabilitiesTelemetry_t&& o) { //a move assignment: operator=(X&&)
    type = o.type;
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

bool PrintSerializedTelemetry(const uint8_t* serialized, uint64_t size) {
    while (size) {
        if (size < sizeof(uint64_t)) return false;
        const uint64_t type = boost::endian::little_to_native(*(reinterpret_cast<const uint64_t*>(serialized)));
        serialized += sizeof(uint64_t);

        if (type == 1) { //ingress
            LOG_INFO(subprocess) << "Ingress Telem:";
            if (size < sizeof(IngressTelemetry_t)) return false;
            size -= sizeof(IngressTelemetry_t);

            const double bundleDataRate = LittleToNativeDouble(reinterpret_cast<const uint64_t*>(serialized));
            serialized += sizeof(uint64_t);
            const double averageDataRate = LittleToNativeDouble(reinterpret_cast<const uint64_t*>(serialized));
            serialized += sizeof(uint64_t);
            const double totalData = LittleToNativeDouble(reinterpret_cast<const uint64_t*>(serialized));
            serialized += sizeof(uint64_t);
            const uint64_t bundleCountEgress = boost::endian::little_to_native(*(reinterpret_cast<const uint64_t*>(serialized)));
            serialized += sizeof(uint64_t);
            const uint64_t bundleCountStorage = boost::endian::little_to_native(*(reinterpret_cast<const uint64_t*>(serialized)));
            serialized += sizeof(uint64_t);

            LOG_INFO(subprocess) << " bundleDataRate: " << bundleDataRate;
            LOG_INFO(subprocess) << " averageDataRate: " << averageDataRate;
            LOG_INFO(subprocess) << " totalData: " << totalData;
            LOG_INFO(subprocess) << " bundleCountEgress: " << bundleCountEgress;
            LOG_INFO(subprocess) << " bundleCountStorage: " << bundleCountStorage;
        }
        else if (type == 2) { //egress
            LOG_INFO(subprocess) << "Egress Telem:";
            if (size < sizeof(EgressTelemetry_t)) return false;
            size -= sizeof(EgressTelemetry_t);

            const uint64_t egressBundleCount = boost::endian::little_to_native(*(reinterpret_cast<const uint64_t*>(serialized)));
            serialized += sizeof(uint64_t);
            const double egressBundleData = LittleToNativeDouble((reinterpret_cast<const uint64_t*>(serialized)));
            serialized += sizeof(uint64_t);
            const uint64_t egressMessageCount = boost::endian::little_to_native(*(reinterpret_cast<const uint64_t*>(serialized)));
            serialized += sizeof(uint64_t);

            LOG_INFO(subprocess) << " egressBundleCount: " << egressBundleCount;
            LOG_INFO(subprocess) << " egressBundleData: " << egressBundleData;
            LOG_INFO(subprocess) << " egressMessageCount: " << egressMessageCount;
        }
        else if (type == 3) { //storage
            LOG_INFO(subprocess) << "Storage Telem:";
            if (size < sizeof(StorageTelemetry_t)) return false;
            size -= sizeof(StorageTelemetry_t);

            const uint64_t totalBundlesErasedFromStorage = boost::endian::little_to_native(*(reinterpret_cast<const uint64_t*>(serialized)));
            serialized += sizeof(uint64_t);
            const uint64_t totalBundlesSentToEgressFromStorage = boost::endian::little_to_native(*(reinterpret_cast<const uint64_t*>(serialized)));
            serialized += sizeof(uint64_t);
            const uint64_t usedSpaceBytes = boost::endian::little_to_native(*(reinterpret_cast<const uint64_t*>(serialized)));
            serialized += sizeof(uint64_t);
            const uint64_t freeSpaceBytes = boost::endian::little_to_native(*(reinterpret_cast<const uint64_t*>(serialized)));
            serialized += sizeof(uint64_t);

            LOG_INFO(subprocess) << " totalBundlesErasedFromStorage: " << totalBundlesErasedFromStorage;
            LOG_INFO(subprocess) << " totalBundlesSentToEgressFromStorage: " << totalBundlesSentToEgressFromStorage;
            LOG_INFO(subprocess) << " usedSpaceBytes: " << usedSpaceBytes;
            LOG_INFO(subprocess) << " freeSpaceBytes: " << freeSpaceBytes;
        }
        else if (type == 10) { //StorageExpiringBeforeThresholdTelemetry_t
            LOG_INFO(subprocess) << "StorageExpiringBeforeThreshold Telem:";
            if (size < 32) return false;
            size -= 32;

            const uint64_t priority = boost::endian::little_to_native(*(reinterpret_cast<const uint64_t*>(serialized)));
            serialized += sizeof(uint64_t);
            const uint64_t thresholdSecondsSinceStartOfYear2000 = boost::endian::little_to_native(*(reinterpret_cast<const uint64_t*>(serialized)));
            serialized += sizeof(uint64_t);
            const uint64_t numNodes = boost::endian::little_to_native(*(reinterpret_cast<const uint64_t*>(serialized)));
            serialized += sizeof(uint64_t);

            LOG_INFO(subprocess) << " priority: " << priority;
            LOG_INFO(subprocess) << " thresholdSecondsSinceStartOfYear2000: " << thresholdSecondsSinceStartOfYear2000;
            const uint64_t remainingBytes = numNodes * 24;
            if (size < remainingBytes) return false;
            size -= remainingBytes;
            for (uint64_t i = 0; i < numNodes; ++i) {
                const uint64_t nodeId = boost::endian::little_to_native(*(reinterpret_cast<const uint64_t*>(serialized)));
                serialized += sizeof(uint64_t);
                const uint64_t bundleCount = boost::endian::little_to_native(*(reinterpret_cast<const uint64_t*>(serialized)));
                serialized += sizeof(uint64_t);
                const uint64_t totalBundleBytes = boost::endian::little_to_native(*(reinterpret_cast<const uint64_t*>(serialized)));
                serialized += sizeof(uint64_t);
                LOG_INFO(subprocess) << " finalDestNode: " << nodeId << " : bundleCount=" << bundleCount << " totalBundleBytes=" << totalBundleBytes;
            }
        }
        else if (type == 4) { //a single outduct
            if (size < (2 * sizeof(uint64_t))) return false; //type + convergenceLayerType
            const uint64_t convergenceLayerType = boost::endian::little_to_native(*(reinterpret_cast<const uint64_t*>(serialized)));
            serialized += sizeof(uint64_t);

            if (convergenceLayerType == 1) { //a single stcp outduct
                LOG_INFO(subprocess) << "STCP Outduct Telem:";
                if (size < sizeof(StcpOutductTelemetry_t)) return false;
                size -= sizeof(StcpOutductTelemetry_t);
            }
            else if (convergenceLayerType == 2) { //a single ltp outduct
                LOG_INFO(subprocess) << "LTP Outduct Telem:";
                if (size < sizeof(LtpOutductTelemetry_t)) return false;
                size -= sizeof(LtpOutductTelemetry_t);
            }
            //else if (convergenceLayerType == 2) { //a single tcpclv3 outduct
            //else if (convergenceLayerType == 3) { //a single tcpclv4 outduct
            //else if (convergenceLayerType == 4) { //a single ltp outduct
            //else if (convergenceLayerType == 5) { //a single udp outduct
            else {
                LOG_INFO(subprocess) << "Invalid telemetry convergence layer type (" << convergenceLayerType << ")";
                return false;
            }

            //common to all convergence layers (base class OutductTelemetry_t)
            const uint64_t totalBundlesAcked = boost::endian::little_to_native(*(reinterpret_cast<const uint64_t*>(serialized)));
            serialized += sizeof(uint64_t);
            const uint64_t totalBundleBytesAcked = boost::endian::little_to_native(*(reinterpret_cast<const uint64_t*>(serialized)));
            serialized += sizeof(uint64_t);
            const uint64_t totalBundlesSent = boost::endian::little_to_native(*(reinterpret_cast<const uint64_t*>(serialized)));
            serialized += sizeof(uint64_t);
            const uint64_t totalBundleBytesSent = boost::endian::little_to_native(*(reinterpret_cast<const uint64_t*>(serialized)));
            serialized += sizeof(uint64_t);
            const uint64_t totalBundlesFailedToSend = boost::endian::little_to_native(*(reinterpret_cast<const uint64_t*>(serialized)));
            serialized += sizeof(uint64_t);
            const uint64_t totalBundlesQueued = totalBundlesSent - totalBundlesAcked;
            const uint64_t totalBundleBytessQueued = totalBundleBytesSent - totalBundleBytesAcked;
            LOG_INFO(subprocess) << " totalBundlesAcked: " << totalBundlesAcked;
            LOG_INFO(subprocess) << " totalBundleBytesAcked: " << totalBundleBytesAcked;
            LOG_INFO(subprocess) << " totalBundlesSent: " << totalBundlesSent;
            LOG_INFO(subprocess) << " totalBundleBytesSent: " << totalBundleBytesSent;
            LOG_INFO(subprocess) << " totalBundlesFailedToSend: " << totalBundlesFailedToSend;
            LOG_INFO(subprocess) << " totalBundlesQueued: " << totalBundlesQueued;
            LOG_INFO(subprocess) << " totalBundleBytessQueued: " << totalBundleBytessQueued;
            
            if (convergenceLayerType == 1) { //a single stcp outduct
                const uint64_t totalStcpBytesSent = boost::endian::little_to_native(*(reinterpret_cast<const uint64_t*>(serialized)));
                serialized += sizeof(uint64_t);
                LOG_INFO(subprocess) << "  Specific to STCP:";
                LOG_INFO(subprocess) << "  totalStcpBytesSent: " << totalStcpBytesSent;
            }
            else if (convergenceLayerType == 2) { //a single ltp outduct
                const uint64_t numCheckpointsExpired = boost::endian::little_to_native(*(reinterpret_cast<const uint64_t*>(serialized)));
                serialized += sizeof(uint64_t);
                const uint64_t numDiscretionaryCheckpointsNotResent = boost::endian::little_to_native(*(reinterpret_cast<const uint64_t*>(serialized)));
                serialized += sizeof(uint64_t);
                const uint64_t countUdpPacketsSent = boost::endian::little_to_native(*(reinterpret_cast<const uint64_t*>(serialized)));
                serialized += sizeof(uint64_t);
                const uint64_t countRxUdpCircularBufferOverruns = boost::endian::little_to_native(*(reinterpret_cast<const uint64_t*>(serialized)));
                serialized += sizeof(uint64_t);
                const uint64_t countTxUdpPacketsLimitedByRate = boost::endian::little_to_native(*(reinterpret_cast<const uint64_t*>(serialized)));
                serialized += sizeof(uint64_t);
                LOG_INFO(subprocess) << "  Specific to LTP:";
                LOG_INFO(subprocess) << "  numCheckpointsExpired: " << numCheckpointsExpired;
                LOG_INFO(subprocess) << "  numDiscretionaryCheckpointsNotResent: " << numDiscretionaryCheckpointsNotResent;
                LOG_INFO(subprocess) << "  countUdpPacketsSent: " << countUdpPacketsSent;
                LOG_INFO(subprocess) << "  countRxUdpCircularBufferOverruns: " << countRxUdpCircularBufferOverruns;
                LOG_INFO(subprocess) << "  countTxUdpPacketsLimitedByRate: " << countTxUdpPacketsLimitedByRate;
            }
            //else if (convergenceLayerType == 3) { //a single tcpclv4 outduct
            //else if (convergenceLayerType == 4) { //a single ltp outduct
            //else if (convergenceLayerType == 5) { //a single udp outduct
        }
    }
    return true;
}
