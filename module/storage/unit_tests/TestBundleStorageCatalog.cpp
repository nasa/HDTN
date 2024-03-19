/**
 * @file TestBundleStorageCatalog.cpp
 * @author  Brian Tomko <brian.j.tomko@nasa.gov>
 *
 * @copyright Copyright (c) 2021 United States Government as represented by
 * the National Aeronautics and Space Administration.
 * No copyright is claimed in the United States under Title 17, U.S.Code.
 * All Other Rights Reserved.
 *
 * @section LICENSE
 * Released under the NASA Open Source Agreement (NOSA)
 * See LICENSE.md in the source root directory for more information.
 */

#include <boost/test/unit_test.hpp>
#include "BundleStorageCatalog.h"
#include <iostream>
#include <string>
#include <inttypes.h>
#include <set>
#include "codec/bpv6.h"
#include "codec/bpv7.h"

static void CreatePrimaryV6(Bpv6CbhePrimaryBlock & p, const cbhe_eid_t & srcEid, const cbhe_eid_t & destEid, bool reqCustody, uint64_t creation, uint64_t sequence, BPV6_BUNDLEFLAG priority = BPV6_BUNDLEFLAG::PRIORITY_BULK) {
    
    p.m_bundleProcessingControlFlags = BPV6_BUNDLEFLAG::NO_FLAGS_SET | priority;
    if (reqCustody) {
        p.m_bundleProcessingControlFlags |= BPV6_BUNDLEFLAG::CUSTODY_REQUESTED;
    }
    p.m_blockLength = 1000;
    p.m_creationTimestamp.secondsSinceStartOfYear2000 = creation;
    p.m_creationTimestamp.sequenceNumber = sequence;
    p.m_lifetimeSeconds = 1000;
    p.m_fragmentOffset = 0;
    p.m_totalApplicationDataUnitLength = 0;

    p.m_destinationEid = destEid;
    p.m_sourceNodeId = srcEid;
    p.m_reportToEid.SetZero();
    p.m_custodianEid.Set(1, 1);
}

static void CreatePrimaryV7(Bpv7CbhePrimaryBlock & p, const cbhe_eid_t & srcEid, const cbhe_eid_t & destEid, uint64_t creation, uint64_t sequence) {

    p.m_bundleProcessingControlFlags = BPV7_BUNDLEFLAG::NO_FLAGS_SET;

    p.m_totalApplicationDataUnitLength = 1000;
    p.m_creationTimestamp.millisecondsSinceStartOfYear2000 = creation * 1000;
    p.m_creationTimestamp.sequenceNumber = sequence;
    p.m_lifetimeMilliseconds = 1000 * 1000;
    p.m_fragmentOffset = 0;

    p.m_destinationEid = destEid;
    p.m_sourceNodeId = srcEid;
    p.m_reportToEid.SetZero();
}

static void TestBundleStorage(const BundleStorageCatalog::DUPLICATE_EXPIRY_ORDER order) {
    const std::size_t idx1 = 8, idx2 = 9;
    BPV6_BUNDLEFLAG bpv6PriArr[3] = { BPV6_BUNDLEFLAG::PRIORITY_BULK, BPV6_BUNDLEFLAG::PRIORITY_NORMAL, BPV6_BUNDLEFLAG::PRIORITY_EXPEDITED};

    //standard usage, same expiry, different sequence numbers
    for(unsigned int whichBundleVersion = 6; whichBundleVersion <= 7; ++whichBundleVersion) {
        BundleStorageCatalog bsc;
        std::vector<Bpv6CbhePrimaryBlock> primariesV6;
        std::vector<Bpv7CbhePrimaryBlock> primariesV7;
        std::vector<PrimaryBlock*> primaries;
        std::vector<catalog_entry_t> catalogEntryCopiesForVerification;
        primariesV6.resize(10);
        primariesV7.resize(10);
        catalogEntryCopiesForVerification.reserve(10);
        uint64_t sumBundleBytes = 0;
        for (std::size_t i = 0; i < 10; ++i) {
            std::size_t seq = i;
            const BPV6_BUNDLEFLAG pri = bpv6PriArr[2-((i + 1) / 4)];
            if (order == BundleStorageCatalog::DUPLICATE_EXPIRY_ORDER::SEQUENCE_NUMBER) { // Test out of order sequence
                if (i == idx1) seq = idx2;
                else if (i == idx2) seq = idx1;
            }            
            if (whichBundleVersion == 6) {
                // Create bundles with different priorities
                CreatePrimaryV6(primariesV6[i], cbhe_eid_t(500, 500), cbhe_eid_t(501, 501), true, 1000, seq, pri);
                primaries.push_back(&primariesV6[i]);
            }
            else {             
                CreatePrimaryV7(primariesV7[i], cbhe_eid_t(500, 500), cbhe_eid_t(501, 501), 1000, seq);
                primaries.push_back(&primariesV7[i]);
            }
            catalog_entry_t catalogEntryToTake;
            catalogEntryToTake.Init(*primaries[i], 1000 + i, 800 + i, 1, NULL);
            sumBundleBytes += 1000 + i;
            catalogEntryToTake.segmentIdChainVec = { static_cast<segment_id_t>(i) };
            catalogEntryCopiesForVerification.push_back(catalogEntryToTake); //make a copy for verification

            // misc catalog_entry_t tests
            const catalog_entry_t catalogEntryToTakeConstCopy(catalogEntryToTake);
            catalog_entry_t catalogEntryToTakeCopy;

            // test catalog_entry_t::Init()
            if (whichBundleVersion == 6) {
                uint64_t paramPtrUuidKeyInMap = 0xdeadbeef;
                Bpv6CbhePrimaryBlock primaryTest;
                CreatePrimaryV6(primaryTest, cbhe_eid_t(500, 500), cbhe_eid_t(501, 501), true, 1000, seq, pri);
                primaryTest.m_bundleProcessingControlFlags |= BPV6_BUNDLEFLAG::ISFRAGMENT;
                cbhe_eid_t bundleEidMaskPtr(500, 500);
                catalogEntryToTakeCopy.Init(primaryTest, 100, 100, 1, &paramPtrUuidKeyInMap, &bundleEidMaskPtr);

                // do it again with NULL bundleEidMaskPtr
                catalogEntryToTakeCopy.Init(primaryTest, 100, 100, 1, &paramPtrUuidKeyInMap, NULL);
            }

            catalogEntryToTakeCopy = catalogEntryToTakeConstCopy; // test const assignment operator
            BOOST_REQUIRE_EQUAL(catalogEntryToTake != catalogEntryToTakeCopy, false);
            BOOST_REQUIRE_EQUAL(catalogEntryToTake < catalogEntryToTakeCopy, false);

            BOOST_REQUIRE_EQUAL(catalogEntryToTakeCopy.HasCustody(), (whichBundleVersion == 6));
         
            // Test each branch of comparison operators
            uint64_t fakeptr = 0xbeef;
            const void* tmpPtr = catalogEntryToTakeCopy.ptrUuidKeyInMap;
            catalogEntryToTakeCopy.ptrUuidKeyInMap = (const void*)fakeptr;
            BOOST_REQUIRE_EQUAL(catalogEntryToTake == catalogEntryToTakeCopy, false);
            BOOST_REQUIRE_EQUAL(catalogEntryToTake != catalogEntryToTakeCopy, true);
            catalogEntryToTakeCopy.ptrUuidKeyInMap = tmpPtr;

            catalogEntryToTakeCopy.sequence++;
            BOOST_REQUIRE_EQUAL(catalogEntryToTake == catalogEntryToTakeCopy, false);
            BOOST_REQUIRE_EQUAL(catalogEntryToTake != catalogEntryToTakeCopy, true);
            catalogEntryToTakeCopy.sequence--;

            catalogEntryToTakeCopy.encodedAbsExpirationAndCustodyAndPriority++;
            BOOST_REQUIRE_EQUAL(catalogEntryToTake == catalogEntryToTakeCopy, false);
            BOOST_REQUIRE_EQUAL(catalogEntryToTake != catalogEntryToTakeCopy, true);
            catalogEntryToTakeCopy.encodedAbsExpirationAndCustodyAndPriority--;

            const cbhe_eid_t tmpDestEid = catalogEntryToTakeCopy.destEid;
            catalogEntryToTakeCopy.destEid = cbhe_eid_t(502, 502);
            BOOST_REQUIRE_EQUAL(catalogEntryToTake == catalogEntryToTakeCopy, false);
            BOOST_REQUIRE_EQUAL(catalogEntryToTake != catalogEntryToTakeCopy, true);
            catalogEntryToTakeCopy.destEid = tmpDestEid;

            catalogEntryToTakeCopy.segmentIdChainVec[0]++;
            BOOST_REQUIRE_EQUAL(catalogEntryToTake == catalogEntryToTakeCopy, false);
            BOOST_REQUIRE_EQUAL(catalogEntryToTake != catalogEntryToTakeCopy, true);
            BOOST_REQUIRE_EQUAL(catalogEntryToTake < catalogEntryToTakeCopy, true);
            catalogEntryToTakeCopy.segmentIdChainVec[0]--;

            catalogEntryToTake.payloadSizeBytes++;
            BOOST_REQUIRE_EQUAL(catalogEntryToTake == catalogEntryToTakeCopy, false);
            BOOST_REQUIRE_EQUAL(catalogEntryToTake != catalogEntryToTakeCopy, true);
            catalogEntryToTake.payloadSizeBytes--;

            catalogEntryToTake.bundleSizeBytes++;
            BOOST_REQUIRE_EQUAL(catalogEntryToTake == catalogEntryToTakeCopy, false);
            BOOST_REQUIRE_EQUAL(catalogEntryToTake != catalogEntryToTakeCopy, true);           
            catalogEntryToTake.bundleSizeBytes--;

            const uint64_t custodyId = i;
            BOOST_REQUIRE_EQUAL(catalogEntryToTake.segmentIdChainVec.size(), 1); //verify before move
            BOOST_REQUIRE(bsc.CatalogIncomingBundleForStore(catalogEntryToTake, *primaries[i], custodyId, order));
            if (order == BundleStorageCatalog::DUPLICATE_EXPIRY_ORDER::SEQUENCE_NUMBER && i == 2) {
                // Test duplicate insertion
                BOOST_REQUIRE(false == bsc.CatalogIncomingBundleForStore(catalogEntryToTake, *primaries[i], custodyId, order));
            }

            BOOST_REQUIRE_EQUAL(bsc.GetNumBundlesInCatalog(), i + 1);
            BOOST_REQUIRE_EQUAL(bsc.GetNumBundleBytesInCatalog(), sumBundleBytes);
            BOOST_REQUIRE_EQUAL(bsc.GetNumBundlesInCatalog(), bsc.GetTotalBundleWriteOperationsToCatalog());
            BOOST_REQUIRE_EQUAL(bsc.GetNumBundleBytesInCatalog(), bsc.GetTotalBundleByteWriteOperationsToCatalog());
            BOOST_REQUIRE_EQUAL(bsc.GetTotalBundleEraseOperationsFromCatalog(), 0);
            BOOST_REQUIRE_EQUAL(bsc.GetTotalBundleByteEraseOperationsFromCatalog(), 0);
            catalogEntryCopiesForVerification.back().ptrUuidKeyInMap = catalogEntryToTake.ptrUuidKeyInMap; //was potentially modified at CatalogIncomingBundleForStore
            BOOST_REQUIRE_EQUAL(catalogEntryToTake.segmentIdChainVec.size(), 0); //verify was moved
        }
        const uint64_t highestSumBundleBytes = sumBundleBytes;
        uint64_t sumBundleBytesDeleted = 0;
        const std::vector<cbhe_eid_t> availableDestinationEids({ cbhe_eid_t(501, 501) });
        for (std::size_t i = 0; i < 10; ++i) {
            uint64_t custodyId;
            std::size_t seq = i;
            if (order == BundleStorageCatalog::DUPLICATE_EXPIRY_ORDER::SEQUENCE_NUMBER) { // Test out of order sequence
                if (i == idx1) seq = idx2;
                else if (i == idx2) seq = idx1;
            }
            const uint64_t expectedCustodyId = seq;

            catalog_entry_t * entryPtr = bsc.PopEntryFromAwaitingSend(custodyId, availableDestinationEids);
            BOOST_REQUIRE(entryPtr != NULL);
            //std::cout << "entryPtr->sequence: " << entryPtr->sequence << " seq: " << seq << "\n";
            BOOST_REQUIRE_EQUAL(entryPtr->sequence, i);
            BOOST_REQUIRE_EQUAL(entryPtr->sequence, primaries[seq]->GetSequenceForSecondsScale()); //bpv7 created where ms/1000 = seconds exactly
            BOOST_REQUIRE_EQUAL(entryPtr->GetAbsExpiration(), primaries[seq]->GetExpirationSeconds());
            BOOST_REQUIRE_EQUAL(custodyId, expectedCustodyId);
            BOOST_REQUIRE(catalogEntryCopiesForVerification[seq] == *entryPtr);
            //return it and take it right back out
            BOOST_REQUIRE(bsc.ReturnEntryToAwaitingSend(*entryPtr, custodyId));
            entryPtr = bsc.PopEntryFromAwaitingSend(custodyId, availableDestinationEids);
            BOOST_REQUIRE(entryPtr != NULL);
            BOOST_REQUIRE_EQUAL(custodyId, expectedCustodyId);
            BOOST_REQUIRE(catalogEntryCopiesForVerification[seq] == *entryPtr);
            //return it and take it right back out using just the node id
            BOOST_REQUIRE(bsc.ReturnEntryToAwaitingSend(*entryPtr, custodyId));

            // Do it again to test RemoveEntryFromAwaitingSend():
            BOOST_REQUIRE(bsc.RemoveEntryFromAwaitingSend(*entryPtr, custodyId));
            BOOST_REQUIRE(bsc.ReturnEntryToAwaitingSend(*entryPtr, custodyId));

            entryPtr = bsc.PopEntryFromAwaitingSend(custodyId, std::vector<uint64_t>({ 100 })); //does not exist
            BOOST_REQUIRE(entryPtr == NULL);
            entryPtr = bsc.PopEntryFromAwaitingSend(custodyId, std::vector<uint64_t>({ 1000 })); //does not exist
            BOOST_REQUIRE(entryPtr == NULL);
            entryPtr = bsc.PopEntryFromAwaitingSend(custodyId, std::vector<uint64_t>({ 501 })); //exists
            BOOST_REQUIRE(entryPtr != NULL);
            BOOST_REQUIRE_EQUAL(custodyId, expectedCustodyId);
            BOOST_REQUIRE(catalogEntryCopiesForVerification[seq] == *entryPtr);

            if (whichBundleVersion == 6) {
                //since request custody was set in the non-fragmented bundle, the custody id shall be retrievable by uuid in a classic rfc5050 custody signal
                uint64_t * cidFromUuidPtr = bsc.GetCustodyIdFromUuid(primaries[seq]->GetCbheBundleUuidNoFragmentFromPrimary());
                BOOST_REQUIRE(cidFromUuidPtr != NULL);
                BOOST_REQUIRE_EQUAL(*cidFromUuidPtr, expectedCustodyId);
                //make sure the fragmented uuid version above doesn't exist
                BOOST_REQUIRE(bsc.GetCustodyIdFromUuid(primaries[i]->GetCbheBundleUuidFragmentFromPrimary(800 + i)) == NULL);
            
                //since request custody was set in the bundle, the catalog entry shall be retrievable by CTEB/ACS custody Id
                catalog_entry_t * entryFromCustodyIdPtr = bsc.GetEntryFromCustodyId(expectedCustodyId);
                BOOST_REQUIRE(entryFromCustodyIdPtr != NULL);
                BOOST_REQUIRE(catalogEntryCopiesForVerification[seq] == *entryFromCustodyIdPtr);
                const uint64_t entryFromCustodyId_bundleSizeBytes = entryFromCustodyIdPtr->bundleSizeBytes;
                //Remove the sent bundle from a custody signal (already removed from the pending send)
                //In actual storage implementation, the primary must be retrieved from actual storage
                //return pair<success, numSuccessfulRemovals>
                std::pair<bool, uint16_t> expectedRet(true, 2); //2 removals, m_custodyIdToCatalogEntryHashmap and m_uuidNoFragToCustodyIdHashMap
                BOOST_REQUIRE_EQUAL(bsc.GetNumBundlesInCatalog(), 10 - i);
                BOOST_REQUIRE_EQUAL(bsc.GetNumBundleBytesInCatalog(), sumBundleBytes);
                BOOST_REQUIRE(expectedRet == bsc.Remove(expectedCustodyId, false));
                entryFromCustodyIdPtr = NULL; //entryFromCustodyIdPtr invalid at this point
                BOOST_REQUIRE_EQUAL(bsc.GetNumBundlesInCatalog(), 10 - (i+1));
                BOOST_REQUIRE_GE(entryFromCustodyId_bundleSizeBytes, 1000);
                sumBundleBytes -= entryFromCustodyId_bundleSizeBytes;
                BOOST_REQUIRE_EQUAL(bsc.GetNumBundleBytesInCatalog(), sumBundleBytes);
                BOOST_REQUIRE_EQUAL(bsc.GetTotalBundleWriteOperationsToCatalog(), 10);
                BOOST_REQUIRE_EQUAL(bsc.GetTotalBundleByteWriteOperationsToCatalog(), highestSumBundleBytes);
                BOOST_REQUIRE_EQUAL(bsc.GetTotalBundleEraseOperationsFromCatalog(), i + 1);
                sumBundleBytesDeleted += 1000 + seq;
                BOOST_REQUIRE_EQUAL(bsc.GetTotalBundleByteEraseOperationsFromCatalog(), sumBundleBytesDeleted);
                //make sure remove again fails
                std::pair<bool, uint16_t> expectedRetFail(false, 0);
                BOOST_REQUIRE(expectedRetFail == bsc.Remove(expectedCustodyId, false));
            }
        }
        {
            //verify catalog empty
            uint64_t custodyId; 
            BOOST_REQUIRE(bsc.PopEntryFromAwaitingSend(custodyId, availableDestinationEids) == NULL);
        }
    }
}
    
BOOST_AUTO_TEST_CASE(BundleStorageCatalogTestCaseFifo)
{
    TestBundleStorage(BundleStorageCatalog::DUPLICATE_EXPIRY_ORDER::FIFO);
}

BOOST_AUTO_TEST_CASE(BundleStorageCatalogTestCaseSeq)
{
    TestBundleStorage(BundleStorageCatalog::DUPLICATE_EXPIRY_ORDER::SEQUENCE_NUMBER);
}

BOOST_AUTO_TEST_CASE(BundleStorageCatalogExpiredCase)
{
    const uint64_t bundleSize = 1000;
    const uint64_t payloadSize = 800;
    const uint64_t bundleRequiredSegments = 1;
    const uint64_t creation = 0;

    for(unsigned int whichBundleVersion = 6; whichBundleVersion <= 7; ++whichBundleVersion) {
        BundleStorageCatalog bsc;

        for(int i = 0; i < 10; i++) {
            std::unique_ptr<PrimaryBlock> primary;
            if(whichBundleVersion == 6) {
                std::unique_ptr<Bpv6CbhePrimaryBlock> p(new Bpv6CbhePrimaryBlock());
                CreatePrimaryV6(*p, cbhe_eid_t(500, 500), cbhe_eid_t(501, 501), true, creation, i);
                primary = std::move(p);
            } else {
                std::unique_ptr<Bpv7CbhePrimaryBlock> p(new Bpv7CbhePrimaryBlock());
                CreatePrimaryV7(*p, cbhe_eid_t(500, 500), cbhe_eid_t(501, 501), creation, i);
                primary = std::move(p);
            }

            catalog_entry_t catalogEntryToTake;
            catalogEntryToTake.Init(*primary, bundleSize, payloadSize, bundleRequiredSegments, NULL);
            catalogEntryToTake.segmentIdChainVec = { static_cast<segment_id_t>(i) };

            bool ret = bsc.CatalogIncomingBundleForStore(
                    catalogEntryToTake,
                    *primary,
                    i,
                    BundleStorageCatalog::DUPLICATE_EXPIRY_ORDER::FIFO);

            BOOST_REQUIRE(ret);
        }


        uint64_t expiry = creation + 2000; // lifetime is 1000
        std::vector<uint64_t> ids;
        bsc.GetExpiredBundleIds(expiry, 0, ids);
        BOOST_REQUIRE_EQUAL(bsc.GetNumBundlesInCatalog(), 10);

        std::sort(ids.begin(), ids.end());
        BOOST_REQUIRE(ids == std::vector<uint64_t>({0, 1, 2, 3, 4, 5, 6, 7, 8, 9}));

        // Test GetStorageExpiringBeforeThresholdTelemetry()
        StorageExpiringBeforeThresholdTelemetry_t expiringTelem;
        expiringTelem.priority = 0;
        expiringTelem.thresholdSecondsSinceStartOfYear2000 = TimestampUtil::GetSecondsSinceEpochRfc5050(
            boost::posix_time::microsec_clock::universal_time() + boost::posix_time::seconds(expiry));
        BOOST_REQUIRE(bsc.GetStorageExpiringBeforeThresholdTelemetry(expiringTelem));

        // Test again for unknown priority index
        expiringTelem.priority = 10000;
        BOOST_REQUIRE(false == bsc.GetStorageExpiringBeforeThresholdTelemetry(expiringTelem));
    }
}

BOOST_AUTO_TEST_CASE(BundleStorageCatalogNoExpiredTestCase)
{

    const uint64_t bundleSize = 1000;
    const uint64_t payloadSize = 800;
    const uint64_t bundleRequiredSegments = 1;
    const uint64_t custodyId = 1;
    const uint64_t creation = 0;

    for(unsigned int whichBundleVersion = 6; whichBundleVersion <= 7; ++whichBundleVersion) {
        BundleStorageCatalog bsc;

        std::unique_ptr<PrimaryBlock> primary;
        if(whichBundleVersion == 6) {
            std::unique_ptr<Bpv6CbhePrimaryBlock> p(new Bpv6CbhePrimaryBlock());
            CreatePrimaryV6(*p, cbhe_eid_t(500, 500), cbhe_eid_t(501, 501), true, creation, 1);
            primary = std::move(p);
        } else {
            std::unique_ptr<Bpv7CbhePrimaryBlock> p(new Bpv7CbhePrimaryBlock());
            CreatePrimaryV7(*p, cbhe_eid_t(500, 500), cbhe_eid_t(501, 501), creation, 1);
            primary = std::move(p);
        }


        catalog_entry_t catalogEntryToTake;
        catalogEntryToTake.Init(*primary, bundleSize, payloadSize, bundleRequiredSegments, NULL);
        catalogEntryToTake.segmentIdChainVec = { static_cast<segment_id_t>(0) };

        bool ret = bsc.CatalogIncomingBundleForStore(
                catalogEntryToTake,
                *primary,
                custodyId,
                BundleStorageCatalog::DUPLICATE_EXPIRY_ORDER::FIFO);

        BOOST_REQUIRE_EQUAL(ret, true);
        uint64_t expiry = creation + 500; // lifetime is 1000, so we check at 500
        std::vector<uint64_t> ids;
        bsc.GetExpiredBundleIds(expiry, 0, ids);
        BOOST_REQUIRE_EQUAL(bsc.GetNumBundlesInCatalog(), 1);

        BOOST_REQUIRE_EQUAL(ids.size(), 0);
    }
}

BOOST_AUTO_TEST_CASE(BundleStorageCatalogMaxExpiredCase)
{
    const uint64_t bundleSize = 1000;
    const uint64_t payloadSize = 800;
    const uint64_t bundleRequiredSegments = 1;
    const uint64_t creation = 0;

    for(unsigned int whichBundleVersion = 6; whichBundleVersion <= 7; ++whichBundleVersion) {
        BundleStorageCatalog bsc;

        for(int i = 0; i < 10; i++) {
            std::unique_ptr<PrimaryBlock> primary;
            if(whichBundleVersion == 6) {
                std::unique_ptr<Bpv6CbhePrimaryBlock> p(new Bpv6CbhePrimaryBlock());
                CreatePrimaryV6(*p, cbhe_eid_t(500, 500), cbhe_eid_t(501, 501), true, creation, i);
                primary = std::move(p);
            } else {
                std::unique_ptr<Bpv7CbhePrimaryBlock> p(new Bpv7CbhePrimaryBlock());
                CreatePrimaryV7(*p, cbhe_eid_t(500, 500), cbhe_eid_t(501, 501), creation, i);
                primary = std::move(p);
            }

            catalog_entry_t catalogEntryToTake;
            catalogEntryToTake.Init(*primary, bundleSize, payloadSize, bundleRequiredSegments, NULL);
            catalogEntryToTake.segmentIdChainVec = { static_cast<segment_id_t>(i) };

            bool ret = bsc.CatalogIncomingBundleForStore(
                    catalogEntryToTake,
                    *primary,
                    i,
                    BundleStorageCatalog::DUPLICATE_EXPIRY_ORDER::FIFO);

            BOOST_REQUIRE_EQUAL(ret, true);
        }
        uint64_t expiry = creation + 2000; // lifetime is 1000
        std::vector<uint64_t> ids;
        bsc.GetExpiredBundleIds(expiry, 5, ids);
        BOOST_REQUIRE_EQUAL(bsc.GetNumBundlesInCatalog(), 10);

        BOOST_REQUIRE_EQUAL(ids.size(), 5);
    }
}

