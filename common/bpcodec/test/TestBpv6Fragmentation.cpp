/**
 * @file TestBundleViewV6.cpp
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

#include <boost/test/unit_test.hpp>
#include <boost/thread.hpp>
#include "codec/BundleViewV6.h"
#include <iostream>
#include <string>
#include <inttypes.h>
#include <vector>
#include "Uri.h"
#include <boost/next_prior.hpp>
#include <boost/make_unique.hpp>
#include "BinaryConversions.h"
#include "codec/bpv6.h"

BOOST_AUTO_TEST_SUITE(TestBpv6Fragmentation)

static const uint64_t PRIMARY_SRC_NODE = 1;
static const uint64_t PRIMARY_SRC_SVC = 2;
static const uint64_t PRIMARY_DEST_NODE = 3;
static const uint64_t PRIMARY_DEST_SVC = 4;
static const uint64_t PRIMARY_TIME = 1000;
static const uint64_t PRIMARY_LIFETIME = 2000;
static const uint64_t PRIMARY_SEQ = 5;

static void buildPrimaryBlock(Bpv6CbhePrimaryBlock & primary) {
    primary.SetZero();

    primary.m_bundleProcessingControlFlags = BPV6_BUNDLEFLAG::SINGLETON;
    primary.m_sourceNodeId.Set(PRIMARY_SRC_NODE, PRIMARY_SRC_SVC);
    primary.m_destinationEid.Set(PRIMARY_DEST_NODE, PRIMARY_DEST_SVC);
    primary.m_custodianEid.SetZero();
    primary.m_reportToEid.SetZero();
    primary.m_creationTimestamp.secondsSinceStartOfYear2000 = PRIMARY_TIME; //(uint64_t)bpv6_unix_to_5050(curr_time);
    primary.m_lifetimeSeconds = PRIMARY_LIFETIME;
    primary.m_creationTimestamp.sequenceNumber = PRIMARY_SEQ;
}

static std::unique_ptr<Bpv6CanonicalBlock> buildPrimaryBlock(std::string & blockBody) {
        std::unique_ptr<Bpv6CanonicalBlock> p = boost::make_unique<Bpv6CanonicalBlock>();
        Bpv6CanonicalBlock & block = *p;

        block.m_blockTypeCode = BPV6_BLOCK_TYPE_CODE::PAYLOAD,
        block.m_blockProcessingControlFlags = BPV6_BLOCKFLAG::NO_FLAGS_SET; //something for checking against
        block.m_blockTypeSpecificDataLength = blockBody.length();
        block.m_blockTypeSpecificDataPtr = (uint8_t*)blockBody.data(); //blockBody must remain in scope until after render

        return p;
}

BOOST_AUTO_TEST_CASE(FragmentZero)
{
    BundleViewV6 bv, a, b;

    Bpv6CbhePrimaryBlock & primary = bv.m_primaryBlockView.header;
    buildPrimaryBlock(primary);
    bv.m_primaryBlockView.SetManuallyModified();

    std::string body = "Bundle contents";
    bv.AppendMoveCanonicalBlock(std::move(buildPrimaryBlock(body)));

    BOOST_REQUIRE(bv.Render(5000));

    bool ret = fragment(bv, 0, a, b);
    BOOST_REQUIRE(ret == false);
}

BOOST_AUTO_TEST_CASE(FragmentBundleLength)
{
    BundleViewV6 bv, a, b;

    Bpv6CbhePrimaryBlock & primary = bv.m_primaryBlockView.header;
    buildPrimaryBlock(primary);
    bv.m_primaryBlockView.SetManuallyModified();

    std::string body = "Bundle contents";
    bv.AppendMoveCanonicalBlock(std::move(buildPrimaryBlock(body)));

    BOOST_REQUIRE(bv.Render(5000));
    size_t sz = bv.m_renderedBundle.size();
    BOOST_REQUIRE_GT(sz, 0);

    bool ret = fragment(bv, sz, a, b);
    BOOST_REQUIRE(ret == false);
}

BOOST_AUTO_TEST_CASE(FragmentFlagNoFrag)
{
    BundleViewV6 bv, a, b;

    Bpv6CbhePrimaryBlock & primary = bv.m_primaryBlockView.header;
    buildPrimaryBlock(primary);
    primary.m_bundleProcessingControlFlags |= BPV6_BUNDLEFLAG::NOFRAGMENT;
    bv.m_primaryBlockView.SetManuallyModified();

    std::string body = "Bundle contents";
    bv.AppendMoveCanonicalBlock(std::move(buildPrimaryBlock(body)));

    BOOST_REQUIRE(bv.Render(5000));
    size_t sz = bv.m_renderedBundle.size();
    BOOST_REQUIRE_GT(sz, 0);

    bool ret = fragment(bv, sz, a, b);
    BOOST_REQUIRE(ret == false);
}

BOOST_AUTO_TEST_CASE(FragmentPayload)
{
    BundleViewV6 bv, a, b;

    Bpv6CbhePrimaryBlock & primary = bv.m_primaryBlockView.header;
    buildPrimaryBlock(primary);
    primary.m_bundleProcessingControlFlags |= BPV6_BUNDLEFLAG::SINGLETON | BPV6_BUNDLEFLAG::PRIORITY_NORMAL;
    bv.m_primaryBlockView.SetManuallyModified();

    std::string body = "helloworld!";
    bv.AppendMoveCanonicalBlock(std::move(buildPrimaryBlock(body)));

    BOOST_REQUIRE(bv.Render(5000));
    size_t sz = 5;
    BOOST_REQUIRE_GT(sz, 0);

    bool ret = fragment(bv, sz, a, b);
    BOOST_REQUIRE(ret == true);

    // Check a Header
    Bpv6CbhePrimaryBlock & ap = a.m_primaryBlockView.header;
    BOOST_REQUIRE(ap.m_bundleProcessingControlFlags == (BPV6_BUNDLEFLAG::SINGLETON | BPV6_BUNDLEFLAG::PRIORITY_NORMAL | BPV6_BUNDLEFLAG::ISFRAGMENT));
    BOOST_REQUIRE(ap.m_destinationEid == cbhe_eid_t(PRIMARY_DEST_NODE, PRIMARY_DEST_SVC));
    BOOST_REQUIRE(ap.m_sourceNodeId == cbhe_eid_t(PRIMARY_SRC_NODE, PRIMARY_SRC_SVC));
    BOOST_REQUIRE(ap.m_reportToEid == cbhe_eid_t(0, 0));
    BOOST_REQUIRE(ap.m_custodianEid == cbhe_eid_t(0, 0));
    BOOST_REQUIRE(ap.m_creationTimestamp == TimestampUtil::bpv6_creation_timestamp_t(PRIMARY_TIME, PRIMARY_SEQ));
    BOOST_REQUIRE(ap.m_lifetimeSeconds == PRIMARY_LIFETIME);
    BOOST_REQUIRE(ap.m_fragmentOffset == 0);
    BOOST_REQUIRE(ap.m_totalApplicationDataUnitLength == 11);
 
    // Check b Header
    Bpv6CbhePrimaryBlock & bp = b.m_primaryBlockView.header;
    BOOST_REQUIRE(bp.m_bundleProcessingControlFlags == (BPV6_BUNDLEFLAG::SINGLETON | BPV6_BUNDLEFLAG::PRIORITY_NORMAL | BPV6_BUNDLEFLAG::ISFRAGMENT));
    BOOST_REQUIRE(bp.m_destinationEid == cbhe_eid_t(PRIMARY_DEST_NODE, PRIMARY_DEST_SVC));
    BOOST_REQUIRE(bp.m_sourceNodeId == cbhe_eid_t(PRIMARY_SRC_NODE, PRIMARY_SRC_SVC));
    BOOST_REQUIRE(bp.m_reportToEid == cbhe_eid_t(0, 0));
    BOOST_REQUIRE(bp.m_custodianEid == cbhe_eid_t(0, 0));
    BOOST_REQUIRE(bp.m_creationTimestamp == TimestampUtil::bpv6_creation_timestamp_t(PRIMARY_TIME, PRIMARY_SEQ));
    BOOST_REQUIRE(bp.m_lifetimeSeconds == PRIMARY_LIFETIME);
    BOOST_REQUIRE(bp.m_fragmentOffset == 5);
    BOOST_REQUIRE(bp.m_totalApplicationDataUnitLength == 11);

    // Check a num blocks
    BOOST_REQUIRE(a.m_listCanonicalBlockView.size() == 1);
    // Check b num blocks
    BOOST_REQUIRE(b.m_listCanonicalBlockView.size() == 1);

    // Check a payload
    std::vector<BundleViewV6::Bpv6CanonicalBlockView *> aBlocks;
    a.GetCanonicalBlocksByType(BPV6_BLOCK_TYPE_CODE::PAYLOAD, aBlocks);
    BOOST_REQUIRE(aBlocks.size() == 1);

    BundleViewV6::Bpv6CanonicalBlockView &aPayload = *aBlocks[0];
    const uint8_t* aPayloadBuf = aPayload.headerPtr->m_blockTypeSpecificDataPtr;
    const uint64_t  aPayloadLen = aPayload.headerPtr->m_blockTypeSpecificDataLength;
    BOOST_REQUIRE(aPayloadLen == 5);
    BOOST_REQUIRE(memcmp(aPayloadBuf, "hello", 5) == 0);

    // Check b payload
    std::vector<BundleViewV6::Bpv6CanonicalBlockView *> bBlocks;
    b.GetCanonicalBlocksByType(BPV6_BLOCK_TYPE_CODE::PAYLOAD, bBlocks);
    BOOST_REQUIRE(bBlocks.size() == 1);

    BundleViewV6::Bpv6CanonicalBlockView &bPayload = *bBlocks[0];
    const uint8_t* bPayloadBuf = bPayload.headerPtr->m_blockTypeSpecificDataPtr;
    const uint64_t  bPayloadLen = bPayload.headerPtr->m_blockTypeSpecificDataLength;
    BOOST_REQUIRE(bPayloadLen == 6);
    BOOST_REQUIRE(memcmp(bPayloadBuf, "world!", 6) == 0);
}


// TODO add test cases for:
// + blocks before payload
// + blocks after payload
// + repeat-in-all blocks

BOOST_AUTO_TEST_SUITE_END()

