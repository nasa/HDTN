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
#include <boost/lexical_cast.hpp>

BOOST_AUTO_TEST_CASE(TelemetryDefinitionsFactoryTestCase)
{

    // Egress
    EgressTelemetry_t egressTelem;
    egressTelem.egressBundleCount = 5;
    egressTelem.egressMessageCount = 10;
    egressTelem.totalDataBytes = 100;

    std::vector<uint8_t> serializedEgress = std::vector<uint8_t>(egressTelem.GetSerializationSize());
    egressTelem.SerializeToLittleEndian(serializedEgress.data(), serializedEgress.size());
    std::vector<std::unique_ptr<Telemetry_t>> telem = TelemetryFactory::DeserializeFromLittleEndian(serializedEgress.data(), serializedEgress.size());
    BOOST_REQUIRE_EQUAL(telem.size(), 1);
    EgressTelemetry_t* egressTelem2 = (EgressTelemetry_t*)telem[0].get();
    BOOST_REQUIRE_EQUAL(egressTelem.totalDataBytes, egressTelem2->totalDataBytes);
    BOOST_REQUIRE_EQUAL(egressTelem.egressBundleCount, egressTelem2->egressBundleCount);
    BOOST_REQUIRE_EQUAL(egressTelem.egressMessageCount, egressTelem2->egressMessageCount);

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
    uint64_t numBytesTakenToDecode;
    BOOST_REQUIRE(telem2.DeserializeFromLittleEndian(serialized.data(), numBytesTakenToDecode, serialized.size()));
    BOOST_REQUIRE_EQUAL(numBytesTakenToDecode, serialized.size());
    BOOST_REQUIRE_EQUAL(telem2.egressBundleCount, 10);
    BOOST_REQUIRE_EQUAL(telem2.totalDataBytes, 200);
    BOOST_REQUIRE_EQUAL(telem2.egressMessageCount, 15);

    EgressTelemetry_t telemFromJson;
    BOOST_REQUIRE(telemFromJson.SetValuesFromJson(telem2.ToJson()));
    BOOST_REQUIRE(telem2 == telemFromJson);
}

BOOST_AUTO_TEST_CASE(TelemetryDefinitionsStorageTestCase)
{
    StorageTelemetry_t t;
    t.m_timestampMilliseconds = 10000;
    t.m_totalBundlesErasedFromStorageNoCustodyTransfer = 10;
    t.m_totalBundlesErasedFromStorageWithCustodyTransfer = 20;
    t.m_totalBundlesRewrittenToStorageFromFailedEgressSend = 30;
    t.m_totalBundlesSentToEgressFromStorageReadFromDisk = 40;
    t.m_totalBundleBytesSentToEgressFromStorageReadFromDisk = 45;
    t.m_totalBundlesSentToEgressFromStorageForwardCutThrough = 50;
    t.m_totalBundleBytesSentToEgressFromStorageForwardCutThrough = 55;
    t.m_numRfc5050CustodyTransfers = 60;
    t.m_numAcsCustodyTransfers = 70;
    t.m_numAcsPacketsReceived = 80;

    //from BundleStorageCatalog
    t.m_numBundlesOnDisk = 90;
    t.m_numBundleBytesOnDisk = 100;
    t.m_totalBundleWriteOperationsToDisk = 110;
    t.m_totalBundleByteWriteOperationsToDisk = 120;
    t.m_totalBundleEraseOperationsFromDisk = 130;
    t.m_totalBundleByteEraseOperationsFromDisk = 140;

    //from BundleStorageManagerBase's MemoryManager
    t.m_usedSpaceBytes = 150;
    t.m_freeSpaceBytes = 160;

    const std::string tJson = t.ToJson();
    //std::cout << tJson << "\n";
    StorageTelemetry_t t2;
    t2.SetValuesFromJson(tJson);
    BOOST_REQUIRE(t == t2);
    BOOST_REQUIRE(!(t != t2));
    BOOST_REQUIRE_EQUAL(tJson, t2.ToJson());
    t.m_totalBundleWriteOperationsToDisk += 1000;
    BOOST_REQUIRE(t != t2);
}


BOOST_AUTO_TEST_CASE(TelemetryDefinitionsOutductTestCase)
{
    
    {
        OutductCapabilityTelemetry_t oct;
        
        BOOST_REQUIRE_EQUAL(oct.GetType(), TelemetryType::outductCapability);
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

        OutductCapabilityTelemetry_t octFromJson;
        BOOST_REQUIRE(octFromJson.SetValuesFromJson(oct.ToJson()));
        BOOST_REQUIRE(oct == octFromJson);
        //std::cout << oct.ToJson() << "\n\n";
        
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

        BOOST_REQUIRE_EQUAL(aoct.GetType(), TelemetryType::allOutductCapability);
        uint64_t expectedSerializationSize = 2 * sizeof(uint64_t);
        for (unsigned int i = 0; i < 10; ++i) {
            aoct.outductCapabilityTelemetryList.emplace_back();
            OutductCapabilityTelemetry_t& oct = aoct.outductCapabilityTelemetryList.back();
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

        AllOutductCapabilitiesTelemetry_t aoctFromJson;
        BOOST_REQUIRE(aoctFromJson.SetValuesFromJson(aoct.ToJson()));
        BOOST_REQUIRE(aoct == aoctFromJson);
        //std::cout << aoct.ToJson() << "\n\n";
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
    telem.mapNodeIdToExpiringBeforeThresholdCount[4] = bundleCountAndBytes;

    //std::cout << telem.ToJson() << "\n";
    StorageExpiringBeforeThresholdTelemetry_t telemFromJson;
    BOOST_REQUIRE(telemFromJson.SetValuesFromJson(telem.ToJson()));
    BOOST_REQUIRE(telem == telemFromJson);

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
        telem.mapNodeIdToExpiringBeforeThresholdCount[9] = bundleCountAndBytes;
        BOOST_REQUIRE_EQUAL(telem.GetSerializationSize(), 80);
    }
}

BOOST_AUTO_TEST_CASE(AllInductTelemetryTestCase)
{
    AllInductTelemetry_t ait;
    ait.m_bundleCountEgress = 101;
    ait.m_bundleCountStorage = 102;
    ait.m_bundleByteCountEgress = 103;
    ait.m_bundleByteCountStorage = 104;
    for (std::size_t i = 0; i < 10; ++i) {
        ait.m_listAllInducts.emplace_back();
        InductTelemetry_t& inductTelem = ait.m_listAllInducts.back();
        inductTelem.m_convergenceLayer.assign(i+1, 'a' + ((char)i));
        for (std::size_t j = 0; j < 2; ++j) {
            inductTelem.m_listInductConnections.emplace_back();
            InductConnectionTelemetry_t& conn = inductTelem.m_listInductConnections.back();
            conn.m_connectionName = boost::lexical_cast<std::string>(i) + " " + boost::lexical_cast<std::string>(j);
            conn.m_totalBundleBytesReceived = i;
            conn.m_totalBundlesReceived = j;
        }
    }
    const std::string aitJson = ait.ToJson();
    std::cout << aitJson << "\n";
    AllInductTelemetry_t ait2;
    ait2.SetValuesFromJson(aitJson);
    BOOST_REQUIRE(ait == ait2);
    BOOST_REQUIRE_EQUAL(aitJson, ait2.ToJson());
}

BOOST_AUTO_TEST_CASE(AllOutductTelemetryTestCase)
{
    AllOutductTelemetry_t aot;
    {
        std::unique_ptr<LtpOutductTelemetry_t> ptr = std::make_unique<LtpOutductTelemetry_t>();
        ptr->m_countRxUdpCircularBufferOverruns = 10;
        ptr->m_countTxUdpPacketsLimitedByRate = 11;
        ptr->m_countUdpPacketsSent = 12;
        ptr->m_numCheckpointsExpired = 13;
        ptr->m_numDiscretionaryCheckpointsNotResent = 14;
        aot.m_listAllOutducts.emplace_back(std::move(ptr));
    }
    {
        std::unique_ptr<StcpOutductTelemetry_t> ptr = std::make_unique<StcpOutductTelemetry_t>();
        ptr->m_totalStcpBytesSent = 20;
        aot.m_listAllOutducts.emplace_back(std::move(ptr));
    }
    {
        std::unique_ptr<TcpclV3OutductTelemetry_t> ptr = std::make_unique<TcpclV3OutductTelemetry_t>();
        ptr->m_totalFragmentsAcked = 30;
        ptr->m_totalFragmentsSent = 31;
        ptr->m_totalBundlesReceived = 32;
        ptr->m_totalBundleBytesReceived = 33;
        aot.m_listAllOutducts.emplace_back(std::move(ptr));
    }
    {
        std::unique_ptr<TcpclV4OutductTelemetry_t> ptr = std::make_unique<TcpclV4OutductTelemetry_t>();
        ptr->m_totalFragmentsAcked = 40;
        ptr->m_totalFragmentsSent = 41;
        ptr->m_totalBundlesReceived = 42;
        ptr->m_totalBundleBytesReceived = 43;
        aot.m_listAllOutducts.emplace_back(std::move(ptr));
    }
    {
        std::unique_ptr<UdpOutductTelemetry_t> ptr = std::make_unique<UdpOutductTelemetry_t>();
        ptr->m_totalPacketsSent = 50;
        ptr->m_totalPacketBytesSent = 51;
        ptr->m_totalPacketsDequeuedForSend = 52;
        ptr->m_totalPacketBytesDequeuedForSend = 53;
        ptr->m_totalPacketsLimitedByRate = 54;
        aot.m_listAllOutducts.emplace_back(std::move(ptr));
    }
    for (std::list<std::unique_ptr<OutductTelemetry_t> >::iterator it = aot.m_listAllOutducts.begin(); it != aot.m_listAllOutducts.end(); ++it) {
        OutductTelemetry_t& ot = *(it->get());
        ot.m_totalBundlesAcked = ot.m_convergenceLayer.size();
        ot.m_totalBundleBytesAcked = ot.m_convergenceLayer.size() + 1;
        ot.m_totalBundlesSent = ot.m_convergenceLayer.size() + 2;
        ot.m_totalBundleBytesSent = ot.m_convergenceLayer.size() + 3;
        ot.m_totalBundlesFailedToSend = ot.m_convergenceLayer.size() + 4;
    }
    const std::string aotJson = aot.ToJson();
    std::cout << aotJson << "\n";
    AllOutductTelemetry_t aot2;
    aot2.SetValuesFromJson(aotJson);
    BOOST_REQUIRE(aot == aot2);
    BOOST_REQUIRE(!(aot != aot2));
    BOOST_REQUIRE_EQUAL(aotJson, aot2.ToJson());
    aot.m_listAllOutducts.back()->m_totalBundleBytesAcked = 5000;
    BOOST_REQUIRE(aot != aot2);
}
