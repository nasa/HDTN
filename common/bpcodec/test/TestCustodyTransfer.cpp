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
static const std::string PRIMARY_SRC_URI = "ipn:100.1";
static const uint64_t PRIMARY_HDTN_NODE = 200;
static const uint64_t PRIMARY_HDTN_SVC = 2;
static const std::string PRIMARY_HDTN_URI = "ipn:200.2";
static const uint64_t PRIMARY_DEST_NODE = 300;
static const uint64_t PRIMARY_DEST_SVC = 3;
static const std::string PRIMARY_DEST_URI = "ipn:300.3";
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
        //create bundle from bundle originator
        const uint64_t ctebCustodyId = 10;
        const std::string bundleDataStr = "bundle data!!!";
        std::vector<uint8_t> bundleData(2000);
        //make valid cteb (primary custodian matches cteb custodian) from bundle originator
        uint64_t len = GenerateBundleWithCteb(
            PRIMARY_SRC_NODE, PRIMARY_SRC_SVC, //primary custodian
            PRIMARY_SRC_NODE, PRIMARY_SRC_SVC, ctebCustodyId, //cteb custodian
            bundleDataStr, &bundleData[0]);
        BOOST_REQUIRE_GT(len, 0);
        bundleData.resize(len);
        BundleViewV6 bv;
        //std::cout << "sz " << bundleSerializedCopy.size() << std::endl;
        BOOST_REQUIRE(bv.SwapInAndLoadBundle(bundleData));
        { //check primary
            bpv6_primary_block & primary = bv.m_primaryBlockView.header;
            const uint64_t requiredPrimaryFlags = BPV6_BUNDLEFLAG_SINGLETON | BPV6_BUNDLEFLAG_NOFRAGMENT | BPV6_BUNDLEFLAG_CUSTODY;
            BOOST_REQUIRE((primary.flags & requiredPrimaryFlags) == requiredPrimaryFlags);
            BOOST_REQUIRE_EQUAL(primary.custodian_node, PRIMARY_SRC_NODE);
            BOOST_REQUIRE_EQUAL(primary.custodian_svc, PRIMARY_SRC_SVC);
        }
        { //check cteb
            BOOST_REQUIRE_EQUAL(bv.GetNumCanonicalBlocks(), 2); //payload + cteb
            std::vector<BundleViewV6::Bpv6CanonicalBlockView*> blocks;
            bv.GetCanonicalBlocksByType(BPV6_BLOCKTYPE_CUST_TRANSFER_EXT, blocks);
            BOOST_REQUIRE_EQUAL(blocks.size(), 1);
            CustodyTransferEnhancementBlock cteb;
            BOOST_REQUIRE_EQUAL(cteb.DeserializeCtebCanonicalBlock((const uint8_t*)blocks[0]->actualSerializedHeaderAndBodyPtr.data()),
                blocks[0]->actualSerializedHeaderAndBodyPtr.size());
            BOOST_REQUIRE_EQUAL(cteb.m_ctebCreatorCustodianEidString, PRIMARY_SRC_URI); //cteb matches primary custodian
        }

        //hdtn node accept custody with acs
        const bool isAcsAware = true;
        CustodyTransferManager ctmHdtn(isAcsAware, PRIMARY_HDTN_NODE, PRIMARY_HDTN_SVC);
        BOOST_REQUIRE_EQUAL(ctmHdtn.GetAcsConstRef(BPV6_ACS_STATUS_REASON_INDICES::SUCCESS__NO_ADDITIONAL_INFORMATION).m_custodyIdFills.size(), 0);
        ctmHdtn.ProcessCustodyOfBundle(bv, true, BPV6_ACS_STATUS_REASON_INDICES::SUCCESS__NO_ADDITIONAL_INFORMATION);
        BOOST_REQUIRE_EQUAL(ctmHdtn.GetAcsConstRef(BPV6_ACS_STATUS_REASON_INDICES::SUCCESS__NO_ADDITIONAL_INFORMATION).m_custodyIdFills.size(), 1);
        //hdtn modifies bundle for next hop
        BOOST_REQUIRE(bv.Render(2000));
        bundleData.swap(bv.m_frontBuffer); //bundleData is now hdtn's modified bundle for next hop
        BOOST_REQUIRE(bv.SwapInAndLoadBundle(bundleData)); //bv is now hdtn's
        { //check new primary
            bpv6_primary_block & primary = bv.m_primaryBlockView.header;
            const uint64_t requiredPrimaryFlags = BPV6_BUNDLEFLAG_SINGLETON | BPV6_BUNDLEFLAG_NOFRAGMENT | BPV6_BUNDLEFLAG_CUSTODY;
            BOOST_REQUIRE((primary.flags & requiredPrimaryFlags) == requiredPrimaryFlags);
            BOOST_REQUIRE_EQUAL(primary.custodian_node, PRIMARY_HDTN_NODE); //hdtn is new custodian
            BOOST_REQUIRE_EQUAL(primary.custodian_svc, PRIMARY_HDTN_SVC);
        }
        { //check new cteb
            BOOST_REQUIRE_EQUAL(bv.GetNumCanonicalBlocks(), 2); //payload + cteb
            std::vector<BundleViewV6::Bpv6CanonicalBlockView*> blocks;
            bv.GetCanonicalBlocksByType(BPV6_BLOCKTYPE_CUST_TRANSFER_EXT, blocks);
            BOOST_REQUIRE_EQUAL(blocks.size(), 1);
            CustodyTransferEnhancementBlock cteb;
            BOOST_REQUIRE_EQUAL(cteb.DeserializeCtebCanonicalBlock((const uint8_t*)blocks[0]->actualSerializedHeaderAndBodyPtr.data()),
                blocks[0]->actualSerializedHeaderAndBodyPtr.size());
            BOOST_REQUIRE_EQUAL(cteb.m_ctebCreatorCustodianEidString, PRIMARY_HDTN_URI); //cteb matches new hdtn custodian
        }
    }



}

