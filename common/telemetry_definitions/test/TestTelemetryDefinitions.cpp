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
#include <boost/make_unique.hpp>


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
    BOOST_REQUIRE(t2.SetValuesFromJson(tJson));
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
        
        oct.maxBundlesInPipeline = 50;
        oct.maxBundleSizeBytesInPipeline = 5000;
        oct.outductArrayIndex = 2;
        oct.nextHopNodeId = 10;
        oct.finalDestinationEidList = { cbhe_eid_t(1,1), cbhe_eid_t(2,1) };
        oct.finalDestinationNodeIdList = { 3, 4, 5 };

        

        OutductCapabilityTelemetry_t octFromJson;
        BOOST_REQUIRE(octFromJson.SetValuesFromJson(oct.ToJson()));
        BOOST_REQUIRE(oct == octFromJson);
        //std::cout << oct.ToJson() << "\n\n";
        
        //misc
        BOOST_REQUIRE(!(oct != octFromJson));
        OutductCapabilityTelemetry_t octCopy = oct;
        OutductCapabilityTelemetry_t octCopy2(oct);
        OutductCapabilityTelemetry_t oct2Moved = std::move(octFromJson);
        BOOST_REQUIRE(oct != octFromJson); //oct2 moved
        BOOST_REQUIRE(oct == oct2Moved);
        BOOST_REQUIRE(oct == octCopy);
        BOOST_REQUIRE(oct == octCopy2);
        OutductCapabilityTelemetry_t oct2Moved2(std::move(oct2Moved));
        BOOST_REQUIRE(oct != oct2Moved); //oct2 moved
        BOOST_REQUIRE(oct == oct2Moved2);

        
    }

    {
        AllOutductCapabilitiesTelemetry_t aoct;

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
        }

        AllOutductCapabilitiesTelemetry_t aoctFromJson;
        BOOST_REQUIRE(aoctFromJson.SetValuesFromJson(aoct.ToJson()));
        BOOST_REQUIRE(aoct == aoctFromJson);
        //std::cout << aoct.ToJson() << "\n\n";

        //misc
        BOOST_REQUIRE(!(aoct != aoctFromJson));
        AllOutductCapabilitiesTelemetry_t aoctCopy = aoct;
        AllOutductCapabilitiesTelemetry_t aoctCopy2(aoct);
        AllOutductCapabilitiesTelemetry_t aoct2Moved = std::move(aoctFromJson);
        BOOST_REQUIRE(aoct != aoctFromJson); //oct2 moved
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

    
    StorageExpiringBeforeThresholdTelemetry_t telemFromJson;
    const std::string telemJson = telem.ToJson();
    //std::cout << telemJson << "\n";
    BOOST_REQUIRE(telemFromJson.SetValuesFromJson(telemJson));
    BOOST_REQUIRE(telem == telemFromJson);
    BOOST_REQUIRE_EQUAL(telemJson, telemFromJson.ToJson());
    
}

BOOST_AUTO_TEST_CASE(AllInductTelemetryTestCase)
{
    AllInductTelemetry_t ait;
    ait.m_bundleCountEgress = 101;
    ait.m_bundleCountStorage = 102;
    ait.m_bundleByteCountEgress = 103;
    ait.m_bundleByteCountStorage = 104;

    {
        ait.m_listAllInducts.emplace_back();
        InductTelemetry_t& inductTelem = ait.m_listAllInducts.back();
        inductTelem.m_convergenceLayer = "ltp_over_udp";
        for (std::size_t j = 0; j < 2; ++j) {
            std::unique_ptr<LtpInductConnectionTelemetry_t> ptr = boost::make_unique<LtpInductConnectionTelemetry_t>();
            LtpInductConnectionTelemetry_t& conn = *ptr;
            conn.m_connectionName = inductTelem.m_convergenceLayer + boost::lexical_cast<std::string>(j);
            conn.m_inputName = conn.m_connectionName + "_input";
            conn.m_totalBundleBytesReceived = conn.m_connectionName.size() + j + 100;
            conn.m_totalBundlesReceived = conn.m_connectionName.size() + j;

            //session receiver stats
            conn.m_numReportSegmentTimerExpiredCallbacks = 1000 + j*1000;
            conn.m_numReportSegmentsUnableToBeIssued = 1001 + j * 1000;
            conn.m_numReportSegmentsTooLargeAndNeedingSplit = 1002 + j * 1000;
            conn.m_numReportSegmentsCreatedViaSplit = 1003 + j * 1000;
            conn.m_numGapsFilledByOutOfOrderDataSegments = 1004 + j * 1000;
            conn.m_numDelayedFullyClaimedPrimaryReportSegmentsSent = 1005 + j * 1000;
            conn.m_numDelayedFullyClaimedSecondaryReportSegmentsSent = 1006 + j * 1000;
            conn.m_numDelayedPartiallyClaimedPrimaryReportSegmentsSent = 1007 + j * 1000;
            conn.m_numDelayedPartiallyClaimedSecondaryReportSegmentsSent = 1008 + j * 1000;

            //ltp udp engine
            conn.m_countUdpPacketsSent = 1009 + j * 1000;
            conn.m_countRxUdpCircularBufferOverruns = 1010 + j * 1000;
            conn.m_countTxUdpPacketsLimitedByRate = 1011 + j * 1000;

            inductTelem.m_listInductConnections.emplace_back(std::move(ptr));
        }
    }

    {
        ait.m_listAllInducts.emplace_back();
        InductTelemetry_t& inductTelem = ait.m_listAllInducts.back();
        inductTelem.m_convergenceLayer = "tcpcl_v3";
        for (std::size_t j = 0; j < 2; ++j) {
            std::unique_ptr<TcpclV3InductConnectionTelemetry_t> ptr = boost::make_unique<TcpclV3InductConnectionTelemetry_t>();
            TcpclV3InductConnectionTelemetry_t& conn = *ptr;
            conn.m_connectionName = inductTelem.m_convergenceLayer + boost::lexical_cast<std::string>(j);
            conn.m_inputName = conn.m_connectionName + "_input";
            conn.m_totalBundleBytesReceived = conn.m_connectionName.size() + j + 100;
            conn.m_totalBundlesReceived = conn.m_connectionName.size() + j;

            conn.m_totalIncomingFragmentsAcked = 1000 + j * 1000;
            conn.m_totalOutgoingFragmentsSent = 1001 + j * 1000;
            //bidirectionality (identical to OutductTelemetry_t)
            conn.m_totalBundlesSentAndAcked = 1002 + j * 1000;
            conn.m_totalBundleBytesSentAndAcked = 1003 + j * 1000;
            conn.m_totalBundlesSent = 1004 + j * 1000;
            conn.m_totalBundleBytesSent = 1005 + j * 1000;
            conn.m_totalBundlesFailedToSend = 1006 + j * 1000;

            inductTelem.m_listInductConnections.emplace_back(std::move(ptr));
        }
    }

    {
        ait.m_listAllInducts.emplace_back();
        InductTelemetry_t& inductTelem = ait.m_listAllInducts.back();
        inductTelem.m_convergenceLayer = "tcpcl_v4";
        for (std::size_t j = 0; j < 2; ++j) {
            std::unique_ptr<TcpclV4InductConnectionTelemetry_t> ptr = boost::make_unique<TcpclV4InductConnectionTelemetry_t>();
            TcpclV4InductConnectionTelemetry_t& conn = *ptr;
            conn.m_connectionName = inductTelem.m_convergenceLayer + boost::lexical_cast<std::string>(j);
            conn.m_inputName = conn.m_connectionName + "_input";
            conn.m_totalBundleBytesReceived = conn.m_connectionName.size() + j + 100;
            conn.m_totalBundlesReceived = conn.m_connectionName.size() + j;

            conn.m_totalIncomingFragmentsAcked = 1000 + j * 1000;
            conn.m_totalOutgoingFragmentsSent = 1001 + j * 1000;
            //bidirectionality (identical to OutductTelemetry_t)
            conn.m_totalBundlesSentAndAcked = 1002 + j * 1000;
            conn.m_totalBundleBytesSentAndAcked = 1003 + j * 1000;
            conn.m_totalBundlesSent = 1004 + j * 1000;
            conn.m_totalBundleBytesSent = 1005 + j * 1000;
            conn.m_totalBundlesFailedToSend = 1006 + j * 1000;

            inductTelem.m_listInductConnections.emplace_back(std::move(ptr));
        }
    }

    {
        ait.m_listAllInducts.emplace_back();
        InductTelemetry_t& inductTelem = ait.m_listAllInducts.back();
        inductTelem.m_convergenceLayer = "stcp";
        for (std::size_t j = 0; j < 2; ++j) {
            std::unique_ptr<StcpInductConnectionTelemetry_t> ptr = boost::make_unique<StcpInductConnectionTelemetry_t>();
            StcpInductConnectionTelemetry_t& conn = *ptr;
            conn.m_connectionName = inductTelem.m_convergenceLayer + boost::lexical_cast<std::string>(j);
            conn.m_inputName = conn.m_connectionName + "_input";
            conn.m_totalBundleBytesReceived = conn.m_connectionName.size() + j + 100;
            conn.m_totalBundlesReceived = conn.m_connectionName.size() + j;

            conn.m_totalStcpBytesReceived = 1000 + j * 1000;

            inductTelem.m_listInductConnections.emplace_back(std::move(ptr));
        }
    }

    {
        ait.m_listAllInducts.emplace_back();
        InductTelemetry_t& inductTelem = ait.m_listAllInducts.back();
        inductTelem.m_convergenceLayer = "udp";
        for (std::size_t j = 0; j < 2; ++j) {
            std::unique_ptr<UdpInductConnectionTelemetry_t> ptr = boost::make_unique<UdpInductConnectionTelemetry_t>();
            UdpInductConnectionTelemetry_t& conn = *ptr;
            conn.m_connectionName = inductTelem.m_convergenceLayer + boost::lexical_cast<std::string>(j);
            conn.m_inputName = conn.m_connectionName + "_input";
            conn.m_totalBundleBytesReceived = conn.m_connectionName.size() + j + 100;
            conn.m_totalBundlesReceived = conn.m_connectionName.size() + j;

            conn.m_countCircularBufferOverruns = 1000 + j * 1000;

            inductTelem.m_listInductConnections.emplace_back(std::move(ptr));
        }
    }

    const std::string aitJson = ait.ToJson();
    //std::cout << aitJson << "\n";
    AllInductTelemetry_t ait2;
    BOOST_REQUIRE(ait2.SetValuesFromJson(aitJson));
    BOOST_REQUIRE(ait == ait2);
    BOOST_REQUIRE_EQUAL(aitJson, ait2.ToJson());
}

BOOST_AUTO_TEST_CASE(AllOutductTelemetryTestCase)
{
    AllOutductTelemetry_t aot;
    aot.m_timestampMilliseconds = 1;
    aot.m_totalBundlesGivenToOutducts = 2;
    aot.m_totalBundleBytesGivenToOutducts = 3;
    aot.m_totalTcpclBundlesReceived = 4;
    aot.m_totalTcpclBundleBytesReceived = 5;
    aot.m_totalStorageToIngressOpportunisticBundles = 6;
    aot.m_totalStorageToIngressOpportunisticBundleBytes = 7;
    aot.m_totalBundlesSuccessfullySent = 8;
    aot.m_totalBundleBytesSuccessfullySent = 9;
    {
        std::unique_ptr<LtpOutductTelemetry_t> ptr = boost::make_unique<LtpOutductTelemetry_t>();
        ptr->m_countRxUdpCircularBufferOverruns = 10;
        ptr->m_countTxUdpPacketsLimitedByRate = 11;
        ptr->m_countUdpPacketsSent = 12;
        ptr->m_numCheckpointsExpired = 13;
        ptr->m_numDiscretionaryCheckpointsNotResent = 14;
        ptr->m_numDeletedFullyClaimedPendingReports = 15;
        aot.m_listAllOutducts.emplace_back(std::move(ptr));
    }
    {
        std::unique_ptr<StcpOutductTelemetry_t> ptr = boost::make_unique<StcpOutductTelemetry_t>();
        ptr->m_totalStcpBytesSent = 20;
        aot.m_listAllOutducts.emplace_back(std::move(ptr));
    }
    {
        std::unique_ptr<TcpclV3OutductTelemetry_t> ptr = boost::make_unique<TcpclV3OutductTelemetry_t>();
        ptr->m_totalFragmentsAcked = 30;
        ptr->m_totalFragmentsSent = 31;
        ptr->m_totalBundlesReceived = 32;
        ptr->m_totalBundleBytesReceived = 33;
        aot.m_listAllOutducts.emplace_back(std::move(ptr));
    }
    {
        std::unique_ptr<TcpclV4OutductTelemetry_t> ptr = boost::make_unique<TcpclV4OutductTelemetry_t>();
        ptr->m_totalFragmentsAcked = 40;
        ptr->m_totalFragmentsSent = 41;
        ptr->m_totalBundlesReceived = 42;
        ptr->m_totalBundleBytesReceived = 43;
        aot.m_listAllOutducts.emplace_back(std::move(ptr));
    }
    {
        std::unique_ptr<UdpOutductTelemetry_t> ptr = boost::make_unique<UdpOutductTelemetry_t>();
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
        ot.m_linkIsUpPhysically = (ot.m_convergenceLayer == "stcp");
        ot.m_linkIsUpPerTimeSchedule = (ot.m_convergenceLayer == "udp");
    }
    const std::string aotJson = aot.ToJson();
    //std::cout << aotJson << "\n";
    AllOutductTelemetry_t aot2;
    BOOST_REQUIRE(aot2.SetValuesFromJson(aotJson));
    BOOST_REQUIRE(aot == aot2);
    BOOST_REQUIRE(!(aot != aot2));
    BOOST_REQUIRE_EQUAL(aotJson, aot2.ToJson());
    aot.m_listAllOutducts.back()->m_totalBundleBytesAcked = 5000;
    BOOST_REQUIRE(aot != aot2);
}
