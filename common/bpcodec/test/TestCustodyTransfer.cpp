#include <boost/test/unit_test.hpp>
#include <boost/thread.hpp>
#include "codec/CustodyTransferEnhancementBlock.h"
#include "codec/CustodyTransferManager.h"
#include <iostream>
#include <string>
#include <inttypes.h>
#include <vector>
#include "Uri.h"

static const uint64_t PRIMARY_SRC_NODE = 100;
static const uint64_t PRIMARY_SRC_SVC = 1;
static const uint64_t PRIMARY_DEST_NODE = 200;
static const uint64_t PRIMARY_DEST_SVC = 2;
static const uint64_t PRIMARY_TIME = 1000;
static const uint64_t PRIMARY_LIFETIME = 2000;
static const uint64_t PRIMARY_SEQ = 1;
#define BP_MSG_BUFSZ 2000 //don't care, not used

static uint64_t GenerateBundleWithCteb(uint64_t primaryCustodianNode, uint64_t primaryCustodianService,
    uint64_t ctebCustodianNode, uint64_t ctebCustodianService, uint64_t ctebCustodyId,
    const std::string & bundleDataStr, uint8_t * buffer)
{
    uint8_t * const serializationBase = buffer;

    bpv6_primary_block primary;
    memset(&primary, 0, sizeof(bpv6_primary_block));
    primary.version = 6;
    bpv6_canonical_block block;
    memset(&block, 0, sizeof(bpv6_canonical_block));

    primary.flags = bpv6_bundle_set_priority(BPV6_PRIORITY_EXPEDITED) |
        bpv6_bundle_set_gflags(BPV6_BUNDLEFLAG_SINGLETON | BPV6_BUNDLEFLAG_NOFRAGMENT | BPV6_BUNDLEFLAG_CUSTODY);
    primary.src_node = PRIMARY_SRC_NODE;
    primary.src_svc = PRIMARY_SRC_SVC;
    primary.dst_node = PRIMARY_DEST_NODE;
    primary.dst_svc = PRIMARY_DEST_SVC;
    primary.custodian_node = primaryCustodianNode;
    primary.custodian_svc = primaryCustodianService;
    primary.creation = PRIMARY_TIME; //(uint64_t)bpv6_unix_to_5050(curr_time);
    primary.lifetime = PRIMARY_LIFETIME;
    primary.sequence = PRIMARY_SEQ;
    uint64_t retVal;
    retVal = cbhe_bpv6_primary_block_encode(&primary, (char *)buffer, 0, BP_MSG_BUFSZ);
    if (retVal == 0) {
        return 0;
    }
    buffer += retVal;

    block.type = BPV6_BLOCKTYPE_PAYLOAD;
    block.flags = 0;// BPV6_BLOCKFLAG_LAST_BLOCK;
    block.length = bundleDataStr.length();

    retVal = bpv6_canonical_block_encode(&block, (char *)buffer, 0, BP_MSG_BUFSZ);
    if (retVal == 0) {
        return 0;
    }
    buffer += retVal;

    memcpy(buffer, bundleDataStr.data(), bundleDataStr.length());
    buffer += bundleDataStr.length();

    CustodyTransferEnhancementBlock cteb;
    cteb.m_custodyId = ctebCustodyId;
    cteb.m_ctebCreatorCustodianEidString = Uri::GetIpnUriString(ctebCustodianNode, ctebCustodianService);
    cteb.AddCanonicalBlockProcessingControlFlag(BLOCK_PROCESSING_CONTROL_FLAGS::LAST_BLOCK);

    retVal = cteb.SerializeCtebCanonicalBlock(buffer);
    if (retVal == 0) {
        return 0;
    }
    buffer += retVal;

    return buffer - serializationBase;
}

BOOST_AUTO_TEST_CASE(CustodyTransferTestCase)
{
    
    {
        uint64_t primaryCustodianNode = 100;
        uint64_t primaryCustodianService = 5;
        uint64_t ctebCustodianNode = 100;
        uint64_t ctebCustodianService = 5;
        uint64_t ctebCustodyId = 45;
        const std::string bundleDataStr = "bundle data!!!";
        std::vector<uint8_t> buffer(2000);
        uint64_t len = GenerateBundleWithCteb(primaryCustodianNode, primaryCustodianService,
            ctebCustodianNode, ctebCustodianService, ctebCustodyId,
            bundleDataStr, &buffer[0]);
        
        BOOST_REQUIRE_GT(len, 0);

        bpv6_primary_block primary;
        CustodyTransferEnhancementBlock cteb;
        BOOST_REQUIRE(CustodyTransferManager::GetCtebAndPrimaryFromBundleData(buffer.data(), len, primary, cteb));
        BOOST_REQUIRE_EQUAL(primary.custodian_node, primaryCustodianNode);
        BOOST_REQUIRE_EQUAL(primary.custodian_svc, primaryCustodianService);
        BOOST_REQUIRE_EQUAL(primary.src_node, PRIMARY_SRC_NODE);
        BOOST_REQUIRE_EQUAL(primary.src_svc, PRIMARY_SRC_SVC);
        BOOST_REQUIRE_EQUAL(primary.dst_node, PRIMARY_DEST_NODE);
        BOOST_REQUIRE_EQUAL(primary.dst_svc, PRIMARY_DEST_SVC);
        BOOST_REQUIRE_EQUAL(primary.creation, PRIMARY_TIME);
        BOOST_REQUIRE_EQUAL(primary.lifetime, PRIMARY_LIFETIME);
        BOOST_REQUIRE_EQUAL(primary.sequence, PRIMARY_SEQ);
        BOOST_REQUIRE_EQUAL(cteb.m_custodyId, ctebCustodyId);
        uint64_t ctebRxCustEid, ctebRxCustSvc;
        BOOST_REQUIRE(Uri::ParseIpnUriString(cteb.m_ctebCreatorCustodianEidString, ctebRxCustEid, ctebRxCustSvc));
        BOOST_REQUIRE_EQUAL(ctebRxCustEid, ctebCustodianNode);
        BOOST_REQUIRE_EQUAL(ctebRxCustSvc, ctebCustodianService);
    }



}

