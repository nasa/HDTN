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
#include <boost/endian/conversion.hpp>
#include <iostream>

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
            std::cout << "Ingress Telem:\n";
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

            std::cout << " bundleDataRate: " << bundleDataRate << "\n";
            std::cout << " averageDataRate: " << averageDataRate << "\n";
            std::cout << " totalData: " << totalData << "\n";
            std::cout << " bundleCountEgress: " << bundleCountEgress << "\n";
            std::cout << " bundleCountStorage: " << bundleCountStorage << "\n";
        }
        else if (type == 2) { //egress
            std::cout << "Egress Telem:\n";
            if (size < sizeof(EgressTelemetry_t)) return false;
            size -= sizeof(EgressTelemetry_t);

            const uint64_t egressBundleCount = boost::endian::little_to_native(*(reinterpret_cast<const uint64_t*>(serialized)));
            serialized += sizeof(uint64_t);
            const double egressBundleData = LittleToNativeDouble((reinterpret_cast<const uint64_t*>(serialized)));
            serialized += sizeof(uint64_t);
            const uint64_t egressMessageCount = boost::endian::little_to_native(*(reinterpret_cast<const uint64_t*>(serialized)));
            serialized += sizeof(uint64_t);

            std::cout << " egressBundleCount: " << egressBundleCount << "\n";
            std::cout << " egressBundleData: " << egressBundleData << "\n";
            std::cout << " egressMessageCount: " << egressMessageCount << "\n";
        }
        else if (type == 3) { //storage
            std::cout << "Storage Telem:\n";
            if (size < sizeof(StorageTelemetry_t)) return false;
            size -= sizeof(StorageTelemetry_t);

            const uint64_t totalBundlesErasedFromStorage = boost::endian::little_to_native(*(reinterpret_cast<const uint64_t*>(serialized)));
            serialized += sizeof(uint64_t);
            const uint64_t totalBundlesSentToEgressFromStorage = boost::endian::little_to_native(*(reinterpret_cast<const uint64_t*>(serialized)));
            serialized += sizeof(uint64_t);

            std::cout << " totalBundlesErasedFromStorage: " << totalBundlesErasedFromStorage << "\n";
            std::cout << " totalBundlesSentToEgressFromStorage: " << totalBundlesSentToEgressFromStorage << "\n";
        }
        else if (type == 4) { //a single outduct
            if (size < (2 * sizeof(uint64_t))) return false; //type + convergenceLayerType
            const uint64_t convergenceLayerType = boost::endian::little_to_native(*(reinterpret_cast<const uint64_t*>(serialized)));
            serialized += sizeof(uint64_t);

            if (convergenceLayerType == 1) { //a single stcp outduct
                std::cout << "STCP Outduct Telem:\n";
                if (size < sizeof(StcpOutductTelemetry_t)) return false;
                size -= sizeof(StcpOutductTelemetry_t);
            }
            else if (convergenceLayerType == 2) { //a single ltp outduct
                std::cout << "LTP Outduct Telem:\n";
                if (size < sizeof(LtpOutductTelemetry_t)) return false;
                size -= sizeof(LtpOutductTelemetry_t);
            }
            //else if (convergenceLayerType == 2) { //a single tcpclv3 outduct
            //else if (convergenceLayerType == 3) { //a single tcpclv4 outduct
            //else if (convergenceLayerType == 4) { //a single ltp outduct
            //else if (convergenceLayerType == 5) { //a single udp outduct
            else {
                std::cout << "Invalid telemetry convergence layer type (" << convergenceLayerType << ")\n";
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
            std::cout << " totalBundlesAcked: " << totalBundlesAcked << "\n";
            std::cout << " totalBundleBytesAcked: " << totalBundleBytesAcked << "\n";
            std::cout << " totalBundlesSent: " << totalBundlesSent << "\n";
            std::cout << " totalBundleBytesSent: " << totalBundleBytesSent << "\n";
            std::cout << " totalBundlesFailedToSend: " << totalBundlesFailedToSend << "\n";
            std::cout << " totalBundlesQueued: " << totalBundlesQueued << "\n";
            std::cout << " totalBundleBytessQueued: " << totalBundleBytessQueued << "\n";
            
            if (convergenceLayerType == 1) { //a single stcp outduct
                const uint64_t totalStcpBytesSent = boost::endian::little_to_native(*(reinterpret_cast<const uint64_t*>(serialized)));
                serialized += sizeof(uint64_t);
                std::cout << "  Specific to STCP:\n";
                std::cout << "  totalStcpBytesSent: " << totalStcpBytesSent << "\n";
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
                std::cout << "  Specific to LTP:\n";
                std::cout << "  numCheckpointsExpired: " << numCheckpointsExpired << "\n";
                std::cout << "  numDiscretionaryCheckpointsNotResent: " << numDiscretionaryCheckpointsNotResent << "\n";
                std::cout << "  countUdpPacketsSent: " << countUdpPacketsSent << "\n";
                std::cout << "  countRxUdpCircularBufferOverruns: " << countRxUdpCircularBufferOverruns << "\n";
                std::cout << "  countTxUdpPacketsLimitedByRate: " << countTxUdpPacketsLimitedByRate << "\n";
            }
            //else if (convergenceLayerType == 3) { //a single tcpclv4 outduct
            //else if (convergenceLayerType == 4) { //a single ltp outduct
            //else if (convergenceLayerType == 5) { //a single udp outduct
        }
    }
    return true;
}
