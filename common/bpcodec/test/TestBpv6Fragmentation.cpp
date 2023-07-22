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
#include <boost/test/data/test_case.hpp>
#include <boost/test/data/monomorphic.hpp>
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

static std::unique_ptr<Bpv6CanonicalBlock> buildCanonicalBlock(std::string & blockBody, BPV6_BLOCK_TYPE_CODE type, BPV6_BLOCKFLAG flags) {
    std::unique_ptr<Bpv6CanonicalBlock> p = boost::make_unique<Bpv6CanonicalBlock>();
    Bpv6CanonicalBlock & block = *p;

    block.m_blockTypeCode = type;
    block.m_blockProcessingControlFlags = flags;
    block.m_blockTypeSpecificDataLength = blockBody.length();
    block.m_blockTypeSpecificDataPtr = (uint8_t*)blockBody.data(); //blockBody must remain in scope until after render

    return p;
}

static std::unique_ptr<Bpv6CanonicalBlock> buildPrimaryBlock(std::string & blockBody) {
        return buildCanonicalBlock(blockBody, BPV6_BLOCK_TYPE_CODE::PAYLOAD, BPV6_BLOCKFLAG::NO_FLAGS_SET);
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

static void CheckPrimaryBlock(Bpv6CbhePrimaryBlock &p, uint64_t offset, uint64_t aduLen) {

    BOOST_REQUIRE(p.m_bundleProcessingControlFlags == (BPV6_BUNDLEFLAG::SINGLETON | BPV6_BUNDLEFLAG::PRIORITY_NORMAL | BPV6_BUNDLEFLAG::ISFRAGMENT));
    BOOST_REQUIRE(p.m_destinationEid == cbhe_eid_t(PRIMARY_DEST_NODE, PRIMARY_DEST_SVC));
    BOOST_REQUIRE(p.m_sourceNodeId == cbhe_eid_t(PRIMARY_SRC_NODE, PRIMARY_SRC_SVC));
    BOOST_REQUIRE(p.m_reportToEid == cbhe_eid_t(0, 0));
    BOOST_REQUIRE(p.m_custodianEid == cbhe_eid_t(0, 0));
    BOOST_REQUIRE(p.m_creationTimestamp == TimestampUtil::bpv6_creation_timestamp_t(PRIMARY_TIME, PRIMARY_SEQ));
    BOOST_REQUIRE(p.m_lifetimeSeconds == PRIMARY_LIFETIME);
    BOOST_REQUIRE(p.m_fragmentOffset == offset);
    BOOST_REQUIRE(p.m_totalApplicationDataUnitLength == aduLen);
 
}

static void CheckCanonicalBlock(BundleViewV6::Bpv6CanonicalBlockView &block, size_t expectedLen, const void * expectedData, BPV6_BLOCK_TYPE_CODE type, BPV6_BLOCKFLAG flags) {
    BOOST_REQUIRE(block.headerPtr->m_blockTypeSpecificDataLength == expectedLen);
    BOOST_REQUIRE(memcmp(block.headerPtr->m_blockTypeSpecificDataPtr, expectedData, expectedLen) == 0);
    BOOST_REQUIRE(block.headerPtr->m_blockTypeCode == type);
    BOOST_REQUIRE(block.headerPtr->m_blockProcessingControlFlags == flags);
}

static void CheckPayload(BundleViewV6 & bv, size_t expectedLen, const void * expectedData) {
    std::vector<BundleViewV6::Bpv6CanonicalBlockView *> blocks;
    bv.GetCanonicalBlocksByType(BPV6_BLOCK_TYPE_CODE::PAYLOAD, blocks);
    BOOST_REQUIRE(blocks.size() == 1);
    BundleViewV6::Bpv6CanonicalBlockView &payload = *blocks[0];

    BPV6_BLOCKFLAG flags = BPV6_BLOCKFLAG::NO_FLAGS_SET;
    if(bv.m_listCanonicalBlockView.back().headerPtr->m_blockTypeCode == BPV6_BLOCK_TYPE_CODE::PAYLOAD) {
        flags = BPV6_BLOCKFLAG::IS_LAST_BLOCK;
    }
    CheckCanonicalBlock(payload, expectedLen, expectedData, BPV6_BLOCK_TYPE_CODE::PAYLOAD, flags);
}

const char *FragmentPayloadData[] = {"helloworld", "helloworld", "helloworld", "longerhelloworld"};
const uint64_t FragmentPayloadSizes[] = {5, 6, 2, 4};

BOOST_DATA_TEST_CASE(
        FragmentPayload,
        boost::unit_test::data::make(FragmentPayloadData) ^ FragmentPayloadSizes,
        payload,
        fragmentSize)
{
    BundleViewV6 bv;

    Bpv6CbhePrimaryBlock & primary = bv.m_primaryBlockView.header;
    buildPrimaryBlock(primary);
    primary.m_bundleProcessingControlFlags |= BPV6_BUNDLEFLAG::SINGLETON | BPV6_BUNDLEFLAG::PRIORITY_NORMAL;
    bv.m_primaryBlockView.SetManuallyModified();

    std::string body(payload);
    bv.AppendMoveCanonicalBlock(std::move(buildPrimaryBlock(body)));

    BOOST_REQUIRE(bv.Render(5000));

    std::list<BundleViewV6> fragments;
    bool ret = fragment(bv, fragmentSize, fragments);
    BOOST_REQUIRE(ret == true);

    uint64_t expectedAduLen = body.size();
    uint64_t expectedNumFragments = (expectedAduLen + (fragmentSize - 1)) / fragmentSize;
    uint64_t expectedLastFragmentSize = expectedAduLen % fragmentSize == 0 ? fragmentSize : expectedAduLen % fragmentSize;

    BOOST_TEST_MESSAGE("fragmenting " << payload << " into " 
            << expectedNumFragments << " fragments of size " << fragmentSize
            << " (last fragment size: " << expectedLastFragmentSize << ")");

    BOOST_REQUIRE(fragments.size() == expectedNumFragments);

    std::list<BundleViewV6>::iterator it = fragments.begin();
    for(uint64_t i = 0, numFragments = fragments.size(); i < numFragments; i++, it++) {
        BundleViewV6 & b = *it;

        uint64_t expectedOffset = i * fragmentSize;

        CheckPrimaryBlock(b.m_primaryBlockView.header, expectedOffset, expectedAduLen);
        BOOST_REQUIRE(b.m_listCanonicalBlockView.size() == 1);

        uint64_t expectedFragmentSize = (i < (numFragments - 1)) ? fragmentSize : expectedLastFragmentSize;
        const void * expectedData = body.data() + (i * fragmentSize);
        CheckPayload(b, expectedFragmentSize, expectedData);

    }
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

    BOOST_REQUIRE(fragments.size() == 3);

    std::list<BundleViewV6>::iterator it = fragments.begin();
    BundleViewV6 & a = *(it++);
    BundleViewV6 & b = *(it++);
    BundleViewV6 & c = *(it++);

    CheckPrimaryBlock(a.m_primaryBlockView.header, 0, 14);
    CheckPrimaryBlock(b.m_primaryBlockView.header, 6, 14);
    CheckPrimaryBlock(c.m_primaryBlockView.header, 12, 14);

    BOOST_REQUIRE(a.m_listCanonicalBlockView.size() == 1);
    BOOST_REQUIRE(b.m_listCanonicalBlockView.size() == 1);
    BOOST_REQUIRE(c.m_listCanonicalBlockView.size() == 1);

    CheckPayload(a, 6, "helloB");
    CheckPayload(b, 6, "igworl");
    CheckPayload(c, 2, "d!");
}

struct BlockTestInfo {
    std::string body;
    BPV6_BLOCK_TYPE_CODE type;
    BPV6_BLOCKFLAG flags;
};

struct MultiBlockTestInfo {
    std::vector<BlockTestInfo> beforeBlocks;
    std::vector<BlockTestInfo> afterBlocks;
};

MultiBlockTestInfo MultiBlockTestInfos[] = {
    { // MultiBlockTestInfo
        { // beforeBlocks
            BlockTestInfo{"before", BPV6_BLOCK_TYPE_CODE::UNUSED_11, BPV6_BLOCKFLAG::NO_FLAGS_SET},
        },
        { // afterBlocks
        },
    },
    { // MultiBlockTestInfo
        { // beforeBlocks
        },
        { // afterBlocks
            BlockTestInfo{"after", BPV6_BLOCK_TYPE_CODE::UNUSED_11, BPV6_BLOCKFLAG::NO_FLAGS_SET},
        },
    },
    { // MultiBlockTestInfo
        { // beforeBlocks
            BlockTestInfo{"before", BPV6_BLOCK_TYPE_CODE::UNUSED_11, BPV6_BLOCKFLAG::NO_FLAGS_SET},
        },
        { // afterBlocks
            BlockTestInfo{"after", BPV6_BLOCK_TYPE_CODE::UNUSED_11, BPV6_BLOCKFLAG::NO_FLAGS_SET},
        },
    },
};

BOOST_AUTO_TEST_CASE(FragmentBlockBefore)
{
    BundleViewV6 bv;

    Bpv6CbhePrimaryBlock & primary = bv.m_primaryBlockView.header;
    buildPrimaryBlock(primary);
    primary.m_bundleProcessingControlFlags |= BPV6_BUNDLEFLAG::SINGLETON | BPV6_BUNDLEFLAG::PRIORITY_NORMAL;
    bv.m_primaryBlockView.SetManuallyModified();

    std::string beforeBlockBody = "before block";
    bv.AppendMoveCanonicalBlock(
            std::move(
                buildCanonicalBlock(
                    beforeBlockBody,
                    BPV6_BLOCK_TYPE_CODE::UNUSED_11,
                    BPV6_BLOCKFLAG::STATUS_REPORT_REQUESTED_IF_BLOCK_CANT_BE_PROCESSED)));

    std::string body = "helloBigworld!";
    bv.AppendMoveCanonicalBlock(std::move(buildPrimaryBlock(body)));

    BOOST_REQUIRE(bv.Render(5000));
    size_t sz = 6;
    BOOST_REQUIRE_GT(sz, 0);

    std::list<BundleViewV6> fragments;
    bool ret = fragment(bv, sz, fragments);
    BOOST_REQUIRE(ret == true);

    BOOST_REQUIRE(fragments.size() == 3);

    std::list<BundleViewV6>::iterator it = fragments.begin();
    BundleViewV6 & a = *(it++);
    BundleViewV6 & b = *(it++);
    BundleViewV6 & c = *(it++);

    CheckPrimaryBlock(a.m_primaryBlockView.header, 0, 14);
    CheckPrimaryBlock(b.m_primaryBlockView.header, 6, 14);
    CheckPrimaryBlock(c.m_primaryBlockView.header, 12, 14);

    BOOST_REQUIRE(a.m_listCanonicalBlockView.size() == 2);
    BOOST_REQUIRE(b.m_listCanonicalBlockView.size() == 1);
    BOOST_REQUIRE(c.m_listCanonicalBlockView.size() == 1);

    CheckCanonicalBlock(a.m_listCanonicalBlockView.front(),
            beforeBlockBody.size(),
            beforeBlockBody.data(),
            BPV6_BLOCK_TYPE_CODE::UNUSED_11,
            BPV6_BLOCKFLAG::STATUS_REPORT_REQUESTED_IF_BLOCK_CANT_BE_PROCESSED);

    CheckPayload(a, 6, "helloB");
    CheckPayload(b, 6, "igworl");
    CheckPayload(c, 2, "d!");
}

// TODO add test cases for:
// + blocks before payload
// + blocks after payload
// + repeat-in-all blocks

BOOST_AUTO_TEST_SUITE_END()

