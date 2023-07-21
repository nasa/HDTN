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
#include "codec/Bpv6Fragment.h"

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
    BundleViewV6 bv;

    Bpv6CbhePrimaryBlock & primary = bv.m_primaryBlockView.header;
    buildPrimaryBlock(primary);
    bv.m_primaryBlockView.SetManuallyModified();

    std::string body = "Bundle contents";
    bv.AppendMoveCanonicalBlock(std::move(buildPrimaryBlock(body)));

    BOOST_REQUIRE(bv.Render(5000));

    std::list<BundleViewV6> fragments;
    bool ret = fragment(bv, 0, fragments);
    BOOST_REQUIRE(ret == false);
}

BOOST_AUTO_TEST_CASE(FragmentBundleLength)
{
    BundleViewV6 bv;

    Bpv6CbhePrimaryBlock & primary = bv.m_primaryBlockView.header;
    buildPrimaryBlock(primary);
    bv.m_primaryBlockView.SetManuallyModified();

    std::string body = "Bundle contents";
    bv.AppendMoveCanonicalBlock(std::move(buildPrimaryBlock(body)));

    BOOST_REQUIRE(bv.Render(5000));
    size_t sz = bv.m_renderedBundle.size();
    BOOST_REQUIRE_GT(sz, 0);

    std::list<BundleViewV6> fragments;
    bool ret = fragment(bv, sz, fragments);
    BOOST_REQUIRE(ret == false);
}

BOOST_AUTO_TEST_CASE(FragmentFlagNoFrag)
{
    BundleViewV6 bv;

    Bpv6CbhePrimaryBlock & primary = bv.m_primaryBlockView.header;
    buildPrimaryBlock(primary);
    primary.m_bundleProcessingControlFlags |= BPV6_BUNDLEFLAG::NOFRAGMENT;
    bv.m_primaryBlockView.SetManuallyModified();

    std::string body = "Bundle contents";
    bv.AppendMoveCanonicalBlock(std::move(buildPrimaryBlock(body)));

    BOOST_REQUIRE(bv.Render(5000));
    size_t sz = bv.m_renderedBundle.size();
    BOOST_REQUIRE_GT(sz, 0);

    std::list<BundleViewV6> fragments;
    bool ret = fragment(bv, sz, fragments);
    BOOST_REQUIRE(ret == false);
}

BOOST_AUTO_TEST_CASE(FragmentPayload)
{
    BundleViewV6 bv;

    Bpv6CbhePrimaryBlock & primary = bv.m_primaryBlockView.header;
    buildPrimaryBlock(primary);
    primary.m_bundleProcessingControlFlags |= BPV6_BUNDLEFLAG::SINGLETON | BPV6_BUNDLEFLAG::PRIORITY_NORMAL;
    bv.m_primaryBlockView.SetManuallyModified();

    std::string body = "helloworld";
    bv.AppendMoveCanonicalBlock(std::move(buildPrimaryBlock(body)));

    BOOST_REQUIRE(bv.Render(5000));
    size_t sz = 5;
    BOOST_REQUIRE_GT(sz, 0);

    std::list<BundleViewV6> fragments;
    bool ret = fragment(bv, sz, fragments);
    BOOST_REQUIRE(ret == true);

    if(fragments.size() != 2) {
        BOOST_REQUIRE(fragments.size() == 2);
    }

    BundleViewV6 & a = fragments.front();
    BundleViewV6 & b = fragments.back();

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
    BOOST_REQUIRE(ap.m_totalApplicationDataUnitLength == 10);
 
    // Check b Header
    Bpv6CbhePrimaryBlock & bp = b.m_primaryBlockView.header;
    BOOST_REQUIRE(bp.m_bundleProcessingControlFlags == (BPV6_BUNDLEFLAG::SINGLETON | BPV6_BUNDLEFLAG::PRIORITY_NORMAL | BPV6_BUNDLEFLAG::ISFRAGMENT));
    BOOST_REQUIRE(bp.m_destinationEid == cbhe_eid_t(PRIMARY_DEST_NODE, PRIMARY_DEST_SVC));
    BOOST_REQUIRE(bp.m_sourceNodeId == cbhe_eid_t(PRIMARY_SRC_NODE, PRIMARY_SRC_SVC));
    BOOST_REQUIRE(bp.m_reportToEid == cbhe_eid_t(0, 0));
    BOOST_REQUIRE(bp.m_custodianEid == cbhe_eid_t(0, 0));
    BOOST_REQUIRE(bp.m_creationTimestamp == TimestampUtil::bpv6_creation_timestamp_t(PRIMARY_TIME, PRIMARY_SEQ));
    BOOST_REQUIRE(bp.m_lifetimeSeconds == PRIMARY_LIFETIME);
    std::cout << "Fragment offset: " <<bp.m_fragmentOffset << std::endl;
    BOOST_REQUIRE(bp.m_fragmentOffset == 5);
    BOOST_REQUIRE(bp.m_totalApplicationDataUnitLength == 10);

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
    BOOST_REQUIRE(bPayloadLen == 5);
    BOOST_REQUIRE(memcmp(bPayloadBuf, "world", 5) == 0);
}

BOOST_AUTO_TEST_CASE(FragmentPayloadMultiple)
{
    BundleViewV6 bv;

    Bpv6CbhePrimaryBlock & primary = bv.m_primaryBlockView.header;
    buildPrimaryBlock(primary);
    primary.m_bundleProcessingControlFlags |= BPV6_BUNDLEFLAG::SINGLETON | BPV6_BUNDLEFLAG::PRIORITY_NORMAL;
    bv.m_primaryBlockView.SetManuallyModified();

    std::string body = "helloBigworld!";
    bv.AppendMoveCanonicalBlock(std::move(buildPrimaryBlock(body)));

    BOOST_REQUIRE(bv.Render(5000));
    size_t sz = 6;
    BOOST_REQUIRE_GT(sz, 0);

    std::list<BundleViewV6> fragments;
    bool ret = fragment(bv, sz, fragments);
    BOOST_REQUIRE(ret == true);

    if(fragments.size() != 3) {
        BOOST_REQUIRE(fragments.size() == 3);
    }

    std::list<BundleViewV6>::iterator it = fragments.begin();
    BundleViewV6 & a = *(it++);
    BundleViewV6 & b = *(it++);
    BundleViewV6 & c = *(it++);

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
    BOOST_REQUIRE(ap.m_totalApplicationDataUnitLength == 14);
 
    // Check b Header
    Bpv6CbhePrimaryBlock & bp = b.m_primaryBlockView.header;
    BOOST_REQUIRE(bp.m_bundleProcessingControlFlags == (BPV6_BUNDLEFLAG::SINGLETON | BPV6_BUNDLEFLAG::PRIORITY_NORMAL | BPV6_BUNDLEFLAG::ISFRAGMENT));
    BOOST_REQUIRE(bp.m_destinationEid == cbhe_eid_t(PRIMARY_DEST_NODE, PRIMARY_DEST_SVC));
    BOOST_REQUIRE(bp.m_sourceNodeId == cbhe_eid_t(PRIMARY_SRC_NODE, PRIMARY_SRC_SVC));
    BOOST_REQUIRE(bp.m_reportToEid == cbhe_eid_t(0, 0));
    BOOST_REQUIRE(bp.m_custodianEid == cbhe_eid_t(0, 0));
    BOOST_REQUIRE(bp.m_creationTimestamp == TimestampUtil::bpv6_creation_timestamp_t(PRIMARY_TIME, PRIMARY_SEQ));
    BOOST_REQUIRE(bp.m_lifetimeSeconds == PRIMARY_LIFETIME);
    BOOST_REQUIRE(bp.m_fragmentOffset == 6);
    BOOST_REQUIRE(bp.m_totalApplicationDataUnitLength == 14);

    // Check c Header
    Bpv6CbhePrimaryBlock & cp = c.m_primaryBlockView.header;
    BOOST_REQUIRE(cp.m_bundleProcessingControlFlags == (BPV6_BUNDLEFLAG::SINGLETON | BPV6_BUNDLEFLAG::PRIORITY_NORMAL | BPV6_BUNDLEFLAG::ISFRAGMENT));
    BOOST_REQUIRE(cp.m_destinationEid == cbhe_eid_t(PRIMARY_DEST_NODE, PRIMARY_DEST_SVC));
    BOOST_REQUIRE(cp.m_sourceNodeId == cbhe_eid_t(PRIMARY_SRC_NODE, PRIMARY_SRC_SVC));
    BOOST_REQUIRE(cp.m_reportToEid == cbhe_eid_t(0, 0));
    BOOST_REQUIRE(cp.m_custodianEid == cbhe_eid_t(0, 0));
    BOOST_REQUIRE(cp.m_creationTimestamp == TimestampUtil::bpv6_creation_timestamp_t(PRIMARY_TIME, PRIMARY_SEQ));
    BOOST_REQUIRE(cp.m_lifetimeSeconds == PRIMARY_LIFETIME);
    BOOST_REQUIRE(cp.m_fragmentOffset == 12);
    BOOST_REQUIRE(cp.m_totalApplicationDataUnitLength == 14);

    // Check a num blocks
    BOOST_REQUIRE(a.m_listCanonicalBlockView.size() == 1);
    // Check b num blocks
    BOOST_REQUIRE(b.m_listCanonicalBlockView.size() == 1);
    // Check c num blocks
    BOOST_REQUIRE(c.m_listCanonicalBlockView.size() == 1);

    // Check a payload
    std::vector<BundleViewV6::Bpv6CanonicalBlockView *> aBlocks;
    a.GetCanonicalBlocksByType(BPV6_BLOCK_TYPE_CODE::PAYLOAD, aBlocks);
    BOOST_REQUIRE(aBlocks.size() == 1);

    BundleViewV6::Bpv6CanonicalBlockView &aPayload = *aBlocks[0];
    const uint8_t* aPayloadBuf = aPayload.headerPtr->m_blockTypeSpecificDataPtr;
    const uint64_t  aPayloadLen = aPayload.headerPtr->m_blockTypeSpecificDataLength;
    BOOST_REQUIRE(aPayloadLen == 6);
    BOOST_REQUIRE(memcmp(aPayloadBuf, "helloB", 6) == 0);

    // Check b payload
    std::vector<BundleViewV6::Bpv6CanonicalBlockView *> bBlocks;
    b.GetCanonicalBlocksByType(BPV6_BLOCK_TYPE_CODE::PAYLOAD, bBlocks);
    BOOST_REQUIRE(bBlocks.size() == 1);

    BundleViewV6::Bpv6CanonicalBlockView &bPayload = *bBlocks[0];
    const uint8_t* bPayloadBuf = bPayload.headerPtr->m_blockTypeSpecificDataPtr;
    const uint64_t  bPayloadLen = bPayload.headerPtr->m_blockTypeSpecificDataLength;
    BOOST_REQUIRE(bPayloadLen == 6);
    BOOST_REQUIRE(memcmp(bPayloadBuf, "igworl", 6) == 0);

    // Check c payload
    std::vector<BundleViewV6::Bpv6CanonicalBlockView *> cBlocks;
    c.GetCanonicalBlocksByType(BPV6_BLOCK_TYPE_CODE::PAYLOAD, cBlocks);
    BOOST_REQUIRE(cBlocks.size() == 1);

    BundleViewV6::Bpv6CanonicalBlockView &cPayload = *cBlocks[0];
    const uint8_t* cPayloadBuf = cPayload.headerPtr->m_blockTypeSpecificDataPtr;
    const uint64_t  cPayloadLen = cPayload.headerPtr->m_blockTypeSpecificDataLength;
    BOOST_REQUIRE(cPayloadLen == 2);
    BOOST_REQUIRE(memcmp(cPayloadBuf, "d!", 2) == 0);
}

// TODO add test cases for:
// + blocks before payload
// + blocks after payload
// + repeat-in-all blocks

BOOST_AUTO_TEST_SUITE_END()

