/**
 * @file Telemetry.cpp
 * @author  Blake LaFuente
 *
 * @copyright Copyright � 2021 United States Government as represented by
 * the National Aeronautics and Space Administration.
 * No copyright is claimed in the United States under Title 17, U.S.Code.
 * All Other Rights Reserved.
 *
 * @section LICENSE
 * Released under the NASA Open Source Agreement (NOSA)
 * See LICENSE.md in the source root directory for more information.
 */

#include "Telemetry.h"
#include "Logger.h"
#include <boost/endian/conversion.hpp>

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

IngressTelemetry_t::IngressTelemetry_t() : type(1) {}
EgressTelemetry_t::EgressTelemetry_t() : type(2) {}
StorageTelemetry_t::StorageTelemetry_t() : type(3) {}
StorageTelemetryRequest_t::StorageTelemetryRequest_t() : type(10) {}
StorageExpiringBeforeThresholdTelemetry_t::StorageExpiringBeforeThresholdTelemetry_t() : type(10) {}
OutductTelemetry_t::OutductTelemetry_t() : type(4), totalBundlesAcked(0), totalBundleBytesAcked(0), totalBundlesSent(0), totalBundleBytesSent(0), totalBundlesFailedToSend(0) {}
StcpOutductTelemetry_t::StcpOutductTelemetry_t() : OutductTelemetry_t(), totalStcpBytesSent(0) {
    convergenceLayerType = 1;
}
LtpOutductTelemetry_t::LtpOutductTelemetry_t() : OutductTelemetry_t(),
    numCheckpointsExpired(0), numDiscretionaryCheckpointsNotResent(0), countUdpPacketsSent(0), countRxUdpCircularBufferOverruns(0), countTxUdpPacketsLimitedByRate(0)
{
    convergenceLayerType = 2;
}

uint64_t IngressTelemetry_t::SerializeToLittleEndian(uint8_t* data, uint64_t bufferSize) const {
    static constexpr uint64_t NUM_BYTES = sizeof(IngressTelemetry_t);
    static constexpr uint64_t NUM_ELEMENTS = NUM_BYTES / sizeof(uint64_t);
    if (bufferSize < NUM_BYTES) {
        return 0; //failure
    }
    SerializeUint64ArrayToLittleEndian(reinterpret_cast<uint64_t*>(data), reinterpret_cast<const uint64_t*>(this), NUM_ELEMENTS);
    return NUM_BYTES;
}

uint64_t EgressTelemetry_t::SerializeToLittleEndian(uint8_t* data, uint64_t bufferSize) const {
    static constexpr uint64_t NUM_BYTES = sizeof(EgressTelemetry_t);
    static constexpr uint64_t NUM_ELEMENTS = NUM_BYTES / sizeof(uint64_t);
    if (bufferSize < NUM_BYTES) {
        return 0; //failure
    }
    SerializeUint64ArrayToLittleEndian(reinterpret_cast<uint64_t*>(data), reinterpret_cast<const uint64_t*>(this), NUM_ELEMENTS);
    return NUM_BYTES;
}

uint64_t StorageTelemetry_t::SerializeToLittleEndian(uint8_t* data, uint64_t bufferSize) const {
    static constexpr uint64_t NUM_BYTES = sizeof(StorageTelemetry_t);
    static constexpr uint64_t NUM_ELEMENTS = NUM_BYTES / sizeof(uint64_t);
    if (bufferSize < NUM_BYTES) {
        return 0; //failure
    }
    SerializeUint64ArrayToLittleEndian(reinterpret_cast<uint64_t*>(data), reinterpret_cast<const uint64_t*>(this), NUM_ELEMENTS);
    return NUM_BYTES;
}

uint64_t StorageTelemetryRequest_t::SerializeToLittleEndian(uint8_t* data, uint64_t bufferSize) const {
    static constexpr uint64_t NUM_BYTES = sizeof(StorageTelemetryRequest_t);
    static constexpr uint64_t NUM_ELEMENTS = NUM_BYTES / sizeof(uint64_t);
    if (bufferSize < NUM_BYTES) {
        return 0; //failure
    }
    SerializeUint64ArrayToLittleEndian(reinterpret_cast<uint64_t*>(data), reinterpret_cast<const uint64_t*>(this), NUM_ELEMENTS);
    return NUM_BYTES;
}

uint64_t StorageExpiringBeforeThresholdTelemetry_t::SerializeToLittleEndian(uint8_t* data, uint64_t bufferSize) const {
    const uint8_t* const dataStartPtr = data;
    uint64_t* data64Ptr = reinterpret_cast<uint64_t*>(data);
    if (bufferSize < 32) {
        return 0; //failure
    }
    bufferSize -= 32;
    *data64Ptr++ = boost::endian::native_to_little(type);
    *data64Ptr++ = boost::endian::native_to_little(priority);
    *data64Ptr++ = boost::endian::native_to_little(thresholdSecondsSinceStartOfYear2000);
    *data64Ptr++ = boost::endian::native_to_little(static_cast<uint64_t>(map_node_id_to_expiring_before_threshold_count.size()));
    
    //typedef std::pair<uint64_t, uint64_t> bundle_count_plus_bundle_bytes_pair_t;
    for (std::map<uint64_t, bundle_count_plus_bundle_bytes_pair_t>::const_iterator it = map_node_id_to_expiring_before_threshold_count.cbegin();
        it != map_node_id_to_expiring_before_threshold_count.cend();
        ++it)
    {
        if (bufferSize < 24) {
            return 0; //failure
        }
        bufferSize -= 24;
        *data64Ptr++ = boost::endian::native_to_little(it->first); //node id
        *data64Ptr++ = boost::endian::native_to_little(it->second.first); //bundle count
        *data64Ptr++ = boost::endian::native_to_little(it->second.second); //total bundle bytes
    }
    return (reinterpret_cast<uint8_t*>(data64Ptr)) - dataStartPtr;
}

uint64_t StcpOutductTelemetry_t::SerializeToLittleEndian(uint8_t* data, uint64_t bufferSize) const {
    static constexpr uint64_t NUM_BYTES = sizeof(StcpOutductTelemetry_t);
    static constexpr uint64_t NUM_ELEMENTS = NUM_BYTES / sizeof(uint64_t);
    if (bufferSize < NUM_BYTES) {
        return 0; //failure
    }
    SerializeUint64ArrayToLittleEndian(reinterpret_cast<uint64_t*>(data), reinterpret_cast<const uint64_t*>(this), NUM_ELEMENTS);
    return NUM_BYTES;
}

uint64_t LtpOutductTelemetry_t::SerializeToLittleEndian(uint8_t* data, uint64_t bufferSize) const {
    static constexpr uint64_t NUM_BYTES = sizeof(LtpOutductTelemetry_t);
    static constexpr uint64_t NUM_ELEMENTS = NUM_BYTES / sizeof(uint64_t);
    if (bufferSize < NUM_BYTES) {
        return 0; //failure
    }
    SerializeUint64ArrayToLittleEndian(reinterpret_cast<uint64_t*>(data), reinterpret_cast<const uint64_t*>(this), NUM_ELEMENTS);
    return NUM_BYTES;
}

uint64_t OutductTelemetry_t::GetTotalBundlesQueued() const {
    return totalBundlesSent - totalBundlesAcked;
}

uint64_t OutductTelemetry_t::GetTotalBundleBytesQueued() const {
    return totalBundleBytesSent - totalBundleBytesAcked;
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

            LOG_INFO(subprocess) << " totalBundlesErasedFromStorage: " << totalBundlesErasedFromStorage;
            LOG_INFO(subprocess) << " totalBundlesSentToEgressFromStorage: " << totalBundlesSentToEgressFromStorage;
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
