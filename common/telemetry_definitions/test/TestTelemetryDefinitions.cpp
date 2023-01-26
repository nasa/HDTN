/**
 * @file TestTelemetryDefinitions.cpp
 * @author  Brian Tomko <brian.j.tomko@nasa.gov>
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

#include <boost/test/unit_test.hpp>
#include "TelemetryDefinitions.h"

BOOST_AUTO_TEST_CASE(TelemetryDefinitionsFactoryTestCase)
{
    // Ingress
    IngressTelemetry_t ingressTelem;
    ingressTelem.bundleCountEgress = 5;
    ingressTelem.bundleCountStorage = 10;
    ingressTelem.totalDataBytes = 100;

    std::vector<uint8_t> serialized = std::vector<uint8_t>(ingressTelem.GetSerializationSize());
    ingressTelem.SerializeToLittleEndian(serialized.data(), serialized.size());
    std::vector<std::unique_ptr<Telemetry_t>> telem = TelemetryFactory::DeserializeFromLittleEndian(serialized.data(), serialized.size());
    BOOST_REQUIRE_EQUAL(telem.size(), 1);
    IngressTelemetry_t* ingressTelem2 = (IngressTelemetry_t*)telem[0].get();
    BOOST_REQUIRE_EQUAL(ingressTelem.totalDataBytes, ingressTelem2->totalDataBytes);
    BOOST_REQUIRE_EQUAL(ingressTelem.bundleCountEgress, ingressTelem2->bundleCountEgress);
    BOOST_REQUIRE_EQUAL(ingressTelem.bundleCountStorage, ingressTelem2->bundleCountStorage);

    // Egress
    EgressTelemetry_t egressTelem;
    egressTelem.egressBundleCount = 5;
    egressTelem.egressMessageCount = 10;
    egressTelem.totalDataBytes = 100;

    std::vector<uint8_t> serializedEgress = std::vector<uint8_t>(egressTelem.GetSerializationSize());
    egressTelem.SerializeToLittleEndian(serializedEgress.data(), serializedEgress.size());
    telem = TelemetryFactory::DeserializeFromLittleEndian(serializedEgress.data(), serializedEgress.size());
    BOOST_REQUIRE_EQUAL(telem.size(), 1);
    EgressTelemetry_t* egressTelem2 = (EgressTelemetry_t*)telem[0].get();
    BOOST_REQUIRE_EQUAL(egressTelem.totalDataBytes, egressTelem2->totalDataBytes);
    BOOST_REQUIRE_EQUAL(egressTelem.egressBundleCount, egressTelem2->egressBundleCount);
    BOOST_REQUIRE_EQUAL(egressTelem.egressMessageCount, egressTelem2->egressMessageCount);

    // Storage
    StorageTelemetry_t storageTelem;
    storageTelem.totalBundlesErasedFromStorage = 5;
    storageTelem.totalBundlesSentToEgressFromStorage = 10;
    storageTelem.usedSpaceBytes = 100;
    storageTelem.freeSpaceBytes = 200;

    std::vector<uint8_t> serializedStorage = std::vector<uint8_t>(storageTelem.GetSerializationSize());
    storageTelem.SerializeToLittleEndian(serializedStorage.data(), serializedStorage.size());
    telem = TelemetryFactory::DeserializeFromLittleEndian(serializedStorage.data(), serializedStorage.size());
    BOOST_REQUIRE_EQUAL(telem.size(), 1);
    StorageTelemetry_t* storageTelem2 = (StorageTelemetry_t*)telem[0].get();
    BOOST_REQUIRE_EQUAL(storageTelem.totalBundlesErasedFromStorage, storageTelem2->totalBundlesErasedFromStorage);
    BOOST_REQUIRE_EQUAL(storageTelem.totalBundlesSentToEgressFromStorage, storageTelem2->totalBundlesSentToEgressFromStorage);
    BOOST_REQUIRE_EQUAL(storageTelem.usedSpaceBytes, storageTelem2->usedSpaceBytes);
    BOOST_REQUIRE_EQUAL(storageTelem.freeSpaceBytes, storageTelem2->freeSpaceBytes);
}

BOOST_AUTO_TEST_CASE(TelemetryDefinitionsFactoryTestCombinedCase)
{
    EgressTelemetry_t egressTelem;
    egressTelem.egressBundleCount = 5;
    egressTelem.egressMessageCount = 10;
    egressTelem.totalDataBytes = 100;
    std::vector<uint8_t> result1 = std::vector<uint8_t>(egressTelem.GetSerializationSize());
    egressTelem.SerializeToLittleEndian(result1.data(), result1.size());

    StcpOutductTelemetry_t stcpTelem;
    stcpTelem.totalBundlesAcked = 1;
    stcpTelem.totalBundleBytesAcked = 2;
    stcpTelem.totalBundlesSent = 3;
    stcpTelem.totalBundleBytesSent = 4;
    stcpTelem.totalBundlesFailedToSend = 5;
    stcpTelem.totalStcpBytesSent = 6;
    std::vector<uint8_t> result2 = std::vector<uint8_t>(stcpTelem.GetSerializationSize());
    stcpTelem.SerializeToLittleEndian(result2.data(), result2.size());

    result1.insert(result1.end(), result2.begin(), result2.end());
    std::vector<std::unique_ptr<Telemetry_t>> telemList = TelemetryFactory::DeserializeFromLittleEndian(result1.data(), result1.size());
    BOOST_REQUIRE_EQUAL(telemList.size(), 2);

    EgressTelemetry_t* egressTelem2 = dynamic_cast<EgressTelemetry_t*>(telemList[0].get());
    BOOST_TEST(egressTelem2 != nullptr);
    BOOST_REQUIRE_EQUAL(egressTelem2->GetType(), TelemetryType::egress);
    BOOST_REQUIRE_EQUAL(egressTelem.egressBundleCount, egressTelem2->egressBundleCount);
    BOOST_REQUIRE_EQUAL(egressTelem.egressMessageCount, egressTelem2->egressMessageCount);
    BOOST_REQUIRE_EQUAL(egressTelem.totalDataBytes, egressTelem2->totalDataBytes);

    StcpOutductTelemetry_t* stcpTelem2 = dynamic_cast<StcpOutductTelemetry_t*>(telemList[1].get());
    BOOST_TEST(stcpTelem2 != nullptr);
    BOOST_REQUIRE_EQUAL(stcpTelem2->GetType(), TelemetryType::stcpoutduct);
    BOOST_REQUIRE_EQUAL(stcpTelem.totalBundleBytesAcked, stcpTelem2->totalBundleBytesAcked);
    BOOST_REQUIRE_EQUAL(stcpTelem.totalBundlesAcked, stcpTelem2->totalBundlesAcked);
    BOOST_REQUIRE_EQUAL(stcpTelem.totalBundlesSent, stcpTelem2->totalBundlesSent);
    BOOST_REQUIRE_EQUAL(stcpTelem.totalBundleBytesSent, stcpTelem2->totalBundleBytesSent);
    BOOST_REQUIRE_EQUAL(stcpTelem.totalBundlesFailedToSend, stcpTelem2->totalBundlesFailedToSend);
    BOOST_REQUIRE_EQUAL(stcpTelem.totalStcpBytesSent, stcpTelem2->totalStcpBytesSent);
}

BOOST_AUTO_TEST_CASE(TelemetryDefinitionsIngressTestCase)
{
    // Test default constructor
    IngressTelemetry_t def;
    BOOST_REQUIRE_EQUAL(def.GetSerializationSize(), 32);
    BOOST_REQUIRE_EQUAL(def.totalDataBytes, 0);
    BOOST_REQUIRE_EQUAL(def.bundleCountEgress, 0);
    BOOST_REQUIRE_EQUAL(def.bundleCountStorage, 0);

    // Test serialize
    IngressTelemetry_t telem;
    telem.bundleCountEgress = 5;
    telem.bundleCountStorage = 10;
    telem.totalDataBytes = 100;
    std::vector<uint8_t> actual = std::vector<uint8_t>(telem.GetSerializationSize());
    telem.SerializeToLittleEndian(actual.data(), actual.size());
    BOOST_REQUIRE_EQUAL(actual.size(), 32);

    std::vector<uint8_t> expected;
    expected.insert(expected.end(), {
        1,   0, 0, 0, 0, 0, 0, 0,
        100, 0, 0, 0, 0, 0, 0, 0,
        5,   0, 0, 0, 0, 0, 0, 0,
        10,  0, 0, 0, 0, 0, 0, 0
    });
    BOOST_REQUIRE_EQUAL_COLLECTIONS(actual.begin(), actual.end(), expected.begin(), expected.end());

    // Test deserialize
    std::vector<uint8_t> serialized;
    serialized.insert(serialized.end(), {
        1,   0, 0, 0, 0, 0, 0, 0,
        200, 0, 0, 0, 0, 0, 0, 0,
        10,  0, 0, 0, 0, 0, 0, 0,
        15,  0, 0, 0, 0, 0, 0, 0
    });

    IngressTelemetry_t telem2;
    telem2.DeserializeFromLittleEndian(serialized.data(), serialized.size());
    BOOST_REQUIRE_EQUAL(telem2.totalDataBytes, 200);
    BOOST_REQUIRE_EQUAL(telem2.bundleCountEgress, 10);
    BOOST_REQUIRE_EQUAL(telem2.bundleCountStorage, 15);
}

BOOST_AUTO_TEST_CASE(TelemetryDefinitionsEgressTestCase)
{
    // Test default constructor
    EgressTelemetry_t def;
    BOOST_REQUIRE_EQUAL(def.GetSerializationSize(), 32);
    BOOST_REQUIRE_EQUAL(def.egressBundleCount, 0);
    BOOST_REQUIRE_EQUAL(def.totalDataBytes, 0);
    BOOST_REQUIRE_EQUAL(def.egressMessageCount, 0);

    // Test serialize
    EgressTelemetry_t telem;
    telem.egressBundleCount = 5;
    telem.totalDataBytes = 100;
    telem.egressMessageCount = 10;
    std::vector<uint8_t> actual = std::vector<uint8_t>(telem.GetSerializationSize());
    telem.SerializeToLittleEndian(actual.data(), actual.size());
    std::vector<uint8_t> expected;
    expected.insert(expected.end(), {
        2,   0, 0, 0, 0, 0, 0, 0,
        5,   0, 0, 0, 0, 0, 0, 0,
        100, 0, 0, 0, 0, 0, 0, 0,
        10,  0, 0, 0, 0, 0, 0, 0
    });

    BOOST_REQUIRE_EQUAL(actual.size(), 32);
    BOOST_REQUIRE_EQUAL_COLLECTIONS(actual.begin(), actual.end(), expected.begin(), expected.end());

    // Test deserialize
    std::vector<uint8_t> serialized;
    serialized.insert(serialized.end(), {
        2,   0, 0, 0, 0, 0, 0, 0,
        10,  0, 0, 0, 0, 0, 0, 0,
        200, 0, 0, 0, 0, 0, 0, 0,
        15,  0, 0, 0, 0, 0, 0, 0
    });

    EgressTelemetry_t telem2;
    telem2.DeserializeFromLittleEndian(serialized.data(), serialized.size());
    BOOST_REQUIRE_EQUAL(telem2.egressBundleCount, 10);
    BOOST_REQUIRE_EQUAL(telem2.totalDataBytes, 200);
    BOOST_REQUIRE_EQUAL(telem2.egressMessageCount, 15);
}

BOOST_AUTO_TEST_CASE(TelemetryDefinitionsStorageTestCase)
{
    // Test default constructor
    StorageTelemetry_t def;
    BOOST_REQUIRE_EQUAL(def.GetSerializationSize(), 40);
    BOOST_REQUIRE_EQUAL(def.totalBundlesErasedFromStorage, 0);
    BOOST_REQUIRE_EQUAL(def.totalBundlesSentToEgressFromStorage, 0);
    BOOST_REQUIRE_EQUAL(def.usedSpaceBytes, 0);
    BOOST_REQUIRE_EQUAL(def.freeSpaceBytes, 0);

    // Test serialize
    StorageTelemetry_t telem;
    telem.totalBundlesErasedFromStorage = 50;
    telem.totalBundlesSentToEgressFromStorage = 100;
    telem.usedSpaceBytes = 30;
    telem.freeSpaceBytes = 10;
    std::vector<uint8_t> actual = std::vector<uint8_t>(telem.GetSerializationSize());
    telem.SerializeToLittleEndian(actual.data(), actual.size());
    std::vector<uint8_t> expected;
    expected.insert(expected.end(), {
        3,   0, 0, 0, 0, 0, 0, 0,
        50,  0, 0, 0, 0, 0, 0, 0,
        100, 0, 0, 0, 0, 0, 0, 0,
        30,  0, 0, 0, 0, 0, 0, 0,
        10,  0, 0, 0, 0, 0, 0, 0
    });

    BOOST_REQUIRE_EQUAL(actual.size(), 40);
    BOOST_REQUIRE_EQUAL_COLLECTIONS(actual.begin(), actual.end(), expected.begin(), expected.end());

    // Test deserialize
    std::vector<uint8_t> serialized;
    serialized.insert(serialized.end(), {
        3,   0, 0, 0, 0, 0, 0, 0,
        200, 0, 0, 0, 0, 0, 0, 0,
        10,  0, 0, 0, 0, 0, 0, 0,
        15,  0, 0, 0, 0, 0, 0, 0,
        12,  0, 0, 0, 0, 0, 0, 0
    });

    StorageTelemetry_t telem2;
    telem2.DeserializeFromLittleEndian(serialized.data(), serialized.size());
    BOOST_REQUIRE_EQUAL(telem2.totalBundlesErasedFromStorage, 200);
    BOOST_REQUIRE_EQUAL(telem2.totalBundlesSentToEgressFromStorage, 10);
    BOOST_REQUIRE_EQUAL(telem2.usedSpaceBytes, 15);
    BOOST_REQUIRE_EQUAL(telem2.freeSpaceBytes, 12);
}

BOOST_AUTO_TEST_CASE(TelemetryDefinitionsLtpOutductTestCase)
{
    // Test default constructor
    LtpOutductTelemetry_t def;
    BOOST_REQUIRE_EQUAL(def.GetSerializationSize(), 96);
    BOOST_REQUIRE_EQUAL(def.totalBundleBytesAcked, 0);
    BOOST_REQUIRE_EQUAL(def.totalBundleBytesSent, 0);
    BOOST_REQUIRE_EQUAL(def.totalBundlesAcked, 0);
    BOOST_REQUIRE_EQUAL(def.totalBundlesSent, 0);
    BOOST_REQUIRE_EQUAL(def.totalBundlesFailedToSend, 0);
    BOOST_REQUIRE_EQUAL(def.numCheckpointsExpired, 0);
    BOOST_REQUIRE_EQUAL(def.numDiscretionaryCheckpointsNotResent, 0);
    BOOST_REQUIRE_EQUAL(def.countUdpPacketsSent, 0);
    BOOST_REQUIRE_EQUAL(def.countRxUdpCircularBufferOverruns, 0);
    BOOST_REQUIRE_EQUAL(def.countTxUdpPacketsLimitedByRate, 0);

    // Test serialize
    LtpOutductTelemetry_t telem;
    telem.totalBundlesAcked = 1;
    telem.totalBundleBytesAcked = 2;
    telem.totalBundlesSent = 3;
    telem.totalBundleBytesSent = 4;
    telem.totalBundlesFailedToSend = 5;
    telem.numCheckpointsExpired = 6;
    telem.numDiscretionaryCheckpointsNotResent = 7;
    telem.countUdpPacketsSent = 8;
    telem.countRxUdpCircularBufferOverruns = 9;
    telem.countTxUdpPacketsLimitedByRate = 10;

    std::vector<uint8_t> actual = std::vector<uint8_t>(telem.GetSerializationSize());
    telem.SerializeToLittleEndian(actual.data(), actual.size());
    std::vector<uint8_t> expected;
    expected.insert(expected.end(), {
        4,  0, 0, 0, 0, 0, 0, 0,
        2,  0, 0, 0, 0, 0, 0, 0,
        1,  0, 0, 0, 0, 0, 0, 0,
        2,  0, 0, 0, 0, 0, 0, 0,
        3,  0, 0, 0, 0, 0, 0, 0,
        4,  0, 0, 0, 0, 0, 0, 0,
        5,  0, 0, 0, 0, 0, 0, 0,
        6,  0, 0, 0, 0, 0, 0, 0,
        7,  0, 0, 0, 0, 0, 0, 0,
        8,  0, 0, 0, 0, 0, 0, 0,
        9,  0, 0, 0, 0, 0, 0, 0,
        10, 0, 0, 0, 0, 0, 0, 0
    });

    BOOST_REQUIRE_EQUAL(actual.size(), 96);
    BOOST_REQUIRE_EQUAL_COLLECTIONS(actual.begin(), actual.end(), expected.begin(), expected.end());

    // Test deserialize
    std::vector<uint8_t> serialized;
    serialized.insert(serialized.end(), {
        4,  0, 0, 0, 0, 0, 0, 0,
        2,  0, 0, 0, 0, 0, 0, 0,
        1,  0, 0, 0, 0, 0, 0, 0,
        2,  0, 0, 0, 0, 0, 0, 0,
        3,  0, 0, 0, 0, 0, 0, 0,
        4,  0, 0, 0, 0, 0, 0, 0,
        5,  0, 0, 0, 0, 0, 0, 0,
        6,  0, 0, 0, 0, 0, 0, 0,
        7,  0, 0, 0, 0, 0, 0, 0,
        8,  0, 0, 0, 0, 0, 0, 0,
        9,  0, 0, 0, 0, 0, 0, 0,
        10, 0, 0, 0, 0, 0, 0, 0
    });

    LtpOutductTelemetry_t telem2;
    telem2.DeserializeFromLittleEndian(serialized.data(), serialized.size());
    BOOST_REQUIRE_EQUAL(telem2.totalBundlesAcked, 1);
    BOOST_REQUIRE_EQUAL(telem2.totalBundleBytesAcked, 2);
    BOOST_REQUIRE_EQUAL(telem2.totalBundlesSent, 3);
    BOOST_REQUIRE_EQUAL(telem2.totalBundleBytesSent, 4);
    BOOST_REQUIRE_EQUAL(telem2.totalBundlesFailedToSend, 5);
    BOOST_REQUIRE_EQUAL(telem.numCheckpointsExpired, 6);
    BOOST_REQUIRE_EQUAL(telem.numDiscretionaryCheckpointsNotResent, 7);
    BOOST_REQUIRE_EQUAL(telem.countUdpPacketsSent, 8);
    BOOST_REQUIRE_EQUAL(telem.countRxUdpCircularBufferOverruns, 9);
    BOOST_REQUIRE_EQUAL(telem.countTxUdpPacketsLimitedByRate, 10);
}

BOOST_AUTO_TEST_CASE(TelemetryDefinitionsStcpOutductTestCase)
{
    // Test default constructor
    StcpOutductTelemetry_t def;
    BOOST_REQUIRE_EQUAL(def.GetSerializationSize(), 64);
    BOOST_REQUIRE_EQUAL(def.totalBundleBytesAcked, 0);
    BOOST_REQUIRE_EQUAL(def.totalBundleBytesSent, 0);
    BOOST_REQUIRE_EQUAL(def.totalBundlesAcked, 0);
    BOOST_REQUIRE_EQUAL(def.totalBundlesSent, 0);
    BOOST_REQUIRE_EQUAL(def.totalBundlesFailedToSend, 0);
    BOOST_REQUIRE_EQUAL(def.totalStcpBytesSent, 0);

    // Test serialize
    StcpOutductTelemetry_t telem;
    telem.totalBundlesAcked = 1;
    telem.totalBundleBytesAcked = 2;
    telem.totalBundlesSent = 3;
    telem.totalBundleBytesSent = 4;
    telem.totalBundlesFailedToSend = 5;
    telem.totalStcpBytesSent = 6;
    std::vector<uint8_t> actual = std::vector<uint8_t>(telem.GetSerializationSize());
    telem.SerializeToLittleEndian(actual.data(), actual.size());
    std::vector<uint8_t> expected;
    expected.insert(expected.end(), {
        5, 0, 0, 0, 0, 0, 0, 0,
        1,  0, 0, 0, 0, 0, 0, 0,
        1, 0, 0, 0, 0, 0, 0, 0,
        2, 0, 0, 0, 0, 0, 0, 0,
        3, 0, 0, 0, 0, 0, 0, 0,
        4, 0, 0, 0, 0, 0, 0, 0,
        5, 0, 0, 0, 0, 0, 0, 0,
        6, 0, 0, 0, 0, 0, 0, 0
    });

    BOOST_REQUIRE_EQUAL(actual.size(), 64);
    BOOST_REQUIRE_EQUAL_COLLECTIONS(actual.begin(), actual.end(), expected.begin(), expected.end());

    // Test deserialize
    std::vector<uint8_t> serialized;
    serialized.insert(serialized.end(), {
        5, 0, 0, 0, 0, 0, 0, 0,
        1,  0, 0, 0, 0, 0, 0, 0,
        1, 0, 0, 0, 0, 0, 0, 0,
        2, 0, 0, 0, 0, 0, 0, 0,
        3, 0, 0, 0, 0, 0, 0, 0,
        4, 0, 0, 0, 0, 0, 0, 0,
        5, 0, 0, 0, 0, 0, 0, 0,
        6, 0, 0, 0, 0, 0, 0, 0
    });

    StcpOutductTelemetry_t telem2;
    telem2.DeserializeFromLittleEndian(serialized.data(), serialized.size());
    BOOST_REQUIRE_EQUAL(telem2.totalBundlesAcked, 1);
    BOOST_REQUIRE_EQUAL(telem2.totalBundleBytesAcked, 2);
    BOOST_REQUIRE_EQUAL(telem2.totalBundlesSent, 3);
    BOOST_REQUIRE_EQUAL(telem2.totalBundleBytesSent, 4);
    BOOST_REQUIRE_EQUAL(telem2.totalBundlesFailedToSend, 5);
    BOOST_REQUIRE_EQUAL(telem2.totalStcpBytesSent, 6);
}

BOOST_AUTO_TEST_CASE(TelemetryDefinitionsOutductTestCase)
{
    
    {
        OutductCapabilityTelemetry_t oct;
        
        BOOST_REQUIRE_EQUAL(oct.type, 5);
        oct.maxBundlesInPipeline = 50;
        oct.maxBundleSizeBytesInPipeline = 5000;
        oct.outductArrayIndex = 2;
        oct.nextHopNodeId = 10;
        oct.finalDestinationEidList = { cbhe_eid_t(1,1), cbhe_eid_t(2,1) };
        oct.finalDestinationNodeIdList = { 3, 4, 5 };

        const uint64_t serializationSize = oct.GetSerializationSize();
        BOOST_REQUIRE_EQUAL(serializationSize, (7*8) + (2*16) + (3*8));

        std::vector<uint8_t> serialized(serializationSize);
        BOOST_REQUIRE_EQUAL(oct.SerializeToLittleEndian(serialized.data(), serializationSize - 1), 0); //fail due to too small of buffer size
        BOOST_REQUIRE_EQUAL(oct.SerializeToLittleEndian(serialized.data(), serializationSize), serializationSize);
        OutductCapabilityTelemetry_t oct2;
        uint64_t numBytesTakenToDecode;
        BOOST_REQUIRE(!oct2.DeserializeFromLittleEndian(serialized.data(), numBytesTakenToDecode, serializationSize - 1)); //fail due to too small of buffer size
        BOOST_REQUIRE(oct2.DeserializeFromLittleEndian(serialized.data(), numBytesTakenToDecode, serializationSize));
        
        BOOST_REQUIRE(oct == oct2);
        
        //misc
        BOOST_REQUIRE(!(oct != oct2));
        OutductCapabilityTelemetry_t octCopy = oct;
        OutductCapabilityTelemetry_t octCopy2(oct);
        OutductCapabilityTelemetry_t oct2Moved = std::move(oct2);
        BOOST_REQUIRE(oct != oct2); //oct2 moved
        BOOST_REQUIRE(oct == oct2Moved);
        BOOST_REQUIRE(oct == octCopy);
        BOOST_REQUIRE(oct == octCopy2);
        OutductCapabilityTelemetry_t oct2Moved2(std::move(oct2Moved));
        BOOST_REQUIRE(oct != oct2Moved); //oct2 moved
        BOOST_REQUIRE(oct == oct2Moved2);
    }

    {
        AllOutductCapabilitiesTelemetry_t aoct;

        BOOST_REQUIRE_EQUAL(aoct.type, 6);
        uint64_t expectedSerializationSize = 2 * sizeof(uint64_t);
        for (unsigned int i = 0; i < 10; ++i) {
            OutductCapabilityTelemetry_t& oct = aoct.outductCapabilityTelemetryList.emplace_back();
            oct.maxBundlesInPipeline = 50 + i;
            oct.maxBundleSizeBytesInPipeline = 5000 + i;
            oct.outductArrayIndex = i;
            oct.nextHopNodeId = 10 + i;
            const unsigned int base = i * 100;
            oct.finalDestinationEidList = { cbhe_eid_t(base + 1,1), cbhe_eid_t(base + 2,1) };
            oct.finalDestinationNodeIdList = { base + 3, base + 4, base + 5 };

            const uint64_t serializationSize = oct.GetSerializationSize();
            BOOST_REQUIRE_EQUAL(serializationSize, (7 * 8) + (2 * 16) + (3 * 8));
            expectedSerializationSize += serializationSize;
        }

        const uint64_t serializationSize = aoct.GetSerializationSize();
        BOOST_REQUIRE_EQUAL(serializationSize, expectedSerializationSize);

        std::vector<uint8_t> serialized(serializationSize);
        BOOST_REQUIRE_EQUAL(aoct.SerializeToLittleEndian(serialized.data(), serializationSize - 1), 0); //fail due to too small of buffer size
        BOOST_REQUIRE_EQUAL(aoct.SerializeToLittleEndian(serialized.data(), serializationSize), serializationSize);
        AllOutductCapabilitiesTelemetry_t aoct2;
        uint64_t numBytesTakenToDecode;
        BOOST_REQUIRE(!aoct2.DeserializeFromLittleEndian(serialized.data(), numBytesTakenToDecode, serializationSize - 1)); //fail due to too small of buffer size
        BOOST_REQUIRE(aoct2.DeserializeFromLittleEndian(serialized.data(), numBytesTakenToDecode, serializationSize));

        BOOST_REQUIRE(aoct == aoct2);
        //std::cout << aoct2 << std::endl;

        //misc
        BOOST_REQUIRE(!(aoct != aoct2));
        AllOutductCapabilitiesTelemetry_t aoctCopy = aoct;
        AllOutductCapabilitiesTelemetry_t aoctCopy2(aoct);
        AllOutductCapabilitiesTelemetry_t aoct2Moved = std::move(aoct2);
        BOOST_REQUIRE(aoct != aoct2); //oct2 moved
        BOOST_REQUIRE(aoct == aoct2Moved);
        BOOST_REQUIRE(aoct == aoctCopy);
        BOOST_REQUIRE(aoct == aoctCopy2);
        AllOutductCapabilitiesTelemetry_t aoct2Moved2(std::move(aoct2Moved));
        BOOST_REQUIRE(aoct != aoct2Moved); //oct2 moved
        BOOST_REQUIRE(aoct == aoct2Moved2);
    }

}

BOOST_AUTO_TEST_CASE(TelemetryDefinitionsStorageExpiringBeforeThresholdTestCase)
{
    StorageExpiringBeforeThresholdTelemetry_t telem;
    telem.priority = 1;
    telem.thresholdSecondsSinceStartOfYear2000 = 100;
    StorageExpiringBeforeThresholdTelemetry_t::bundle_count_plus_bundle_bytes_pair_t bundleCountAndBytes;
    bundleCountAndBytes.first = 90;
    bundleCountAndBytes.second = 2000;
    telem.map_node_id_to_expiring_before_threshold_count[4] = bundleCountAndBytes;

    {
        std::vector<uint8_t>* actual = new std::vector<uint8_t>(1000); //will be 64-bit aligned
        uint8_t* telemPtr = actual->data();
        const uint8_t* const telemSerializationBase = telemPtr;
        uint64_t telemBufferSize = actual->size();

        //start zmq message with telemetry
        const uint64_t storageTelemSize = telem.SerializeToLittleEndian(telemPtr, telemBufferSize);
        telemBufferSize -= storageTelemSize;
        telemPtr += storageTelemSize;

        actual->resize(telemPtr - telemSerializationBase);
        std::vector<uint8_t> expected;
        expected.insert(expected.end(), {
            10,  0, 0, 0, 0, 0, 0, 0,
            1,   0, 0, 0, 0, 0, 0, 0,
            100, 0, 0, 0, 0, 0, 0, 0,
            1,   0, 0, 0, 0, 0, 0, 0,
            4,   0, 0, 0, 0, 0, 0, 0,
            90,  0, 0, 0, 0, 0, 0, 0,
            208, 7, 0, 0, 0, 0, 0, 0
        });

        BOOST_REQUIRE_EQUAL(actual->size(), 56);
        BOOST_REQUIRE_EQUAL_COLLECTIONS(actual->begin(), actual->end(), expected.begin(), expected.end());
        delete actual;
    }

    {
        BOOST_REQUIRE_EQUAL(telem.GetSerializationSize(), 56);
        telem.map_node_id_to_expiring_before_threshold_count[9] = bundleCountAndBytes;
        BOOST_REQUIRE_EQUAL(telem.GetSerializationSize(), 80);
    }
}
