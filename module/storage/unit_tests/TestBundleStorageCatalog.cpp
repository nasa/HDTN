#include <boost/test/unit_test.hpp>
#include "BundleStorageCatalog.h"
#include <iostream>
#include <string>
#include <inttypes.h>
#include <set>


bpv6_primary_block CreatePrimary(const cbhe_eid_t & srcEid, const cbhe_eid_t & destEid, bool reqCustody, uint64_t creation, uint64_t sequence) {
    bpv6_primary_block p;
    p.version = 6;
    
    p.flags = 0;
    if (reqCustody) {
        p.flags |= BPV6_BUNDLEFLAG_CUSTODY;
    }
    p.block_length = 1000;
    p.creation = creation;
    p.sequence = sequence;
    p.lifetime = 1000;
    p.fragment_offset = 0;
    p.data_length = 0;

    p.dst_node = destEid.nodeId;
    p.dst_svc = destEid.serviceId;
    p.src_node = srcEid.nodeId;
    p.src_svc = srcEid.serviceId;
    p.report_node = 0;
    p.report_svc = 0;
    p.custodian_node = 1;
    p.custodian_svc = 1;    // 64 bytes
    return p;
}


    
BOOST_AUTO_TEST_CASE(BundleStorageCatalogTestCase)
{
    

    //standard usage, same expiry, different sequence numbers
    {
        BundleStorageCatalog bsc;
        std::vector<bpv6_primary_block> primaries;
        std::vector<catalog_entry_t> catalogEntryCopiesForVerification;
        for (std::size_t i = 0; i < 10; ++i) {
            primaries.push_back(CreatePrimary(cbhe_eid_t(500, 500), cbhe_eid_t(501, 501), true, 1000, i));
            catalog_entry_t catalogEntryToTake;
            catalogEntryToTake.Init(primaries[i], 1000 + i, 1, NULL);
            catalogEntryToTake.segmentIdChainVec = { static_cast<segment_id_t>(i) };
            catalogEntryCopiesForVerification.push_back(catalogEntryToTake); //make a copy for verification
            const uint64_t custodyId = i;
            BOOST_REQUIRE_EQUAL(catalogEntryToTake.segmentIdChainVec.size(), 1); //verify before move
            BOOST_REQUIRE(bsc.CatalogIncomingBundleForStore(catalogEntryToTake, primaries[i], custodyId, BundleStorageCatalog::DUPLICATE_EXPIRY_ORDER::FIFO));
            catalogEntryCopiesForVerification.back().ptrUuidKeyInMap = catalogEntryToTake.ptrUuidKeyInMap; //was potentially modified at CatalogIncomingBundleForStore
            BOOST_REQUIRE_EQUAL(catalogEntryToTake.segmentIdChainVec.size(), 0); //verify was moved
        }
        const std::vector<cbhe_eid_t> availableDestinationEids({ cbhe_eid_t(501, 501) });
        for (std::size_t i = 0; i < 10; ++i) {
            const uint64_t expectedCustodyId = i;
            uint64_t custodyId;
            catalog_entry_t * entryPtr = bsc.PopEntryFromAwaitingSend(custodyId, availableDestinationEids);
            BOOST_REQUIRE(entryPtr != NULL);
            BOOST_REQUIRE_EQUAL(custodyId, expectedCustodyId);
            BOOST_REQUIRE(catalogEntryCopiesForVerification[i] == *entryPtr);
            //return it and take it right back out
            BOOST_REQUIRE(bsc.ReturnEntryToAwaitingSend(*entryPtr, custodyId));
            entryPtr = bsc.PopEntryFromAwaitingSend(custodyId, availableDestinationEids);
            BOOST_REQUIRE(entryPtr != NULL);
            BOOST_REQUIRE_EQUAL(custodyId, expectedCustodyId);
            BOOST_REQUIRE(catalogEntryCopiesForVerification[i] == *entryPtr);
            //since request custody was set in the non-fragmented bundle, the custody id shall be retrievable by uuid in a classic rfc5050 custody signal
            uint64_t * cidFromUuidPtr = bsc.GetCustodyIdFromUuid(cbhe_bundle_uuid_nofragment_t(primaries[i]));
            BOOST_REQUIRE(cidFromUuidPtr != NULL);
            BOOST_REQUIRE_EQUAL(*cidFromUuidPtr, expectedCustodyId);
            //make sure the fragmented uuid version above doesn't exist
            BOOST_REQUIRE(bsc.GetCustodyIdFromUuid(cbhe_bundle_uuid_t(primaries[i])) == NULL);
            //since request custody was set in the bundle, the catalog entry shall be retrievable by CTEB/ACS custody Id
            catalog_entry_t * entryFromCustodyIdPtr = bsc.GetEntryFromCustodyId(expectedCustodyId);
            BOOST_REQUIRE(entryFromCustodyIdPtr != NULL);
            BOOST_REQUIRE(catalogEntryCopiesForVerification[i] == *entryFromCustodyIdPtr);
            //Remove the sent bundle from a custody signal (already removed from the pending send)
            //In actual storage implementation, the primary must be retrieved from actual storage
            //return pair<success, numSuccessfulRemovals>
            std::pair<bool, uint16_t> expectedRet(true, 2); //2 removals, m_custodyIdToCatalogEntryHashmap and m_uuidNoFragToCustodyIdHashMap
            BOOST_REQUIRE(expectedRet == bsc.Remove(expectedCustodyId, false));
            //make sure remove again fails
            std::pair<bool, uint16_t> expectedRetFail(false, 0);
            BOOST_REQUIRE(expectedRetFail == bsc.Remove(expectedCustodyId, false));

        }
        {
            //verify catalog empty
            uint64_t custodyId; 
            BOOST_REQUIRE(bsc.PopEntryFromAwaitingSend(custodyId, availableDestinationEids) == NULL);
        }
    }

    
}
