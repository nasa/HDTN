/**
 * @file Bpv6Fragment.cpp
 * @author  Evan Danish <evan.j.danish@nasa.gov>
 *
 * @copyright Copyright © 2023 United States Government as represented by
 * the National Aeronautics and Space Administration.
 * No copyright is claimed in the United States under Title 17, U.S.Code.
 * All Other Rights Reserved.
 *
 * @section LICENSE
 * Released under the NASA Open Source Agreement (NOSA)
 * See LICENSE.md in the source root directory for more information.
 */

#include "codec/BundleViewV6.h"
#include "Logger.h"
#include <iostream>
#include <boost/next_prior.hpp>
#include <boost/make_unique.hpp>
#include "Uri.h"
#include "codec/bpv6.h"
#include <vector>
#include <cstdint>

static constexpr hdtn::Logger::SubProcess subprocess = hdtn::Logger::SubProcess::none;

static bool getPayloadSize(BundleViewV6& bv, uint64_t& sz) {
    std::vector<BundleViewV6::Bpv6CanonicalBlockView*> blocks;
    bv.GetCanonicalBlocksByType(BPV6_BLOCK_TYPE_CODE::PAYLOAD, blocks);
    if(blocks.size() != 1 || !blocks[0]) {
        return false;
    }
    BundleViewV6::Bpv6CanonicalBlockView & payload = *blocks[0];
    // TODO do we need to check if dirty or marked for deletion?
    if(!payload.headerPtr) {
        return false;
    }

    sz = payload.headerPtr->m_blockTypeSpecificDataLength;
    return true;
}

static void copyPrimary(const BundleViewV6& orig, BundleViewV6& copy, uint64_t offset, uint64_t aduLength) {
    const Bpv6CbhePrimaryBlock & origHdr = orig.m_primaryBlockView.header;
    Bpv6CbhePrimaryBlock & copyHdr = copy.m_primaryBlockView.header;

    copyHdr = origHdr;

    copyHdr.m_bundleProcessingControlFlags |= BPV6_BUNDLEFLAG::ISFRAGMENT;
    copyHdr.m_fragmentOffset = offset;
    LOG_INFO(subprocess) << "Set fragment offset to " << copyHdr.m_fragmentOffset;
    copyHdr.m_totalApplicationDataUnitLength = aduLength;

    copy.m_primaryBlockView.SetManuallyModified(); // TODO needed?
}


// start inclusive, end exclusive (e.g. startOffset=0, endOffset=2 consists of offsets 0 and 1)
static void appendPayloadBlock(BundleViewV6::Bpv6CanonicalBlockView & block, BundleViewV6& bv, uint64_t startOffset, uint64_t endOffset) {
    bv.m_listCanonicalBlockView.emplace_back();
    BundleViewV6::Bpv6CanonicalBlockView & copyBlockView = bv.m_listCanonicalBlockView.back();

    copyBlockView.headerPtr = boost::make_unique<Bpv6CanonicalBlock>();

    Bpv6CanonicalBlock& copyBlock = *copyBlockView.headerPtr;
    Bpv6CanonicalBlock& origBlock = *block.headerPtr;

    copyBlock.m_blockTypeCode = origBlock.m_blockTypeCode;
    copyBlock.m_blockProcessingControlFlags = origBlock.m_blockProcessingControlFlags;
    copyBlock.m_blockTypeSpecificDataLength = endOffset - startOffset;
    copyBlock.m_blockTypeSpecificDataPtr = origBlock.m_blockTypeSpecificDataPtr + startOffset;

    copyBlockView.SetManuallyModified(); // TODO needed?
}

static void appendBlock(BundleViewV6::Bpv6CanonicalBlockView & block, BundleViewV6& bv) {
    bv.m_listCanonicalBlockView.emplace_back();
    BundleViewV6::Bpv6CanonicalBlockView & copy = bv.m_listCanonicalBlockView.back();

    uint64_t decodeSize = 0;
    bool isAdminRecord = /*TODO*/ false;
    Bpv6CanonicalBlock::DeserializeBpv6(copy.headerPtr, static_cast<const uint8_t*>(block.actualSerializedBlockPtr.data()), decodeSize,
            block.actualSerializedBlockPtr.size(), false, bv.m_blockNumberToRecycledCanonicalBlockArray);

    copy.SetManuallyModified(); // TODO needed?
}

bool fragment(BundleViewV6& orig, uint64_t sz, BundleViewV6& a, BundleViewV6& b) {
    if(sz == 0) {
        LOG_ERROR(subprocess) << "Cannot fragment: fragment size must be greater than zero";
        return false;
    }
    BPV6_BUNDLEFLAG flags = orig.m_primaryBlockView.header.m_bundleProcessingControlFlags;
    if((flags & BPV6_BUNDLEFLAG::NOFRAGMENT) == BPV6_BUNDLEFLAG::NOFRAGMENT) {
        LOG_ERROR(subprocess) << "Cannot fragment: bundle has NOFRAGMENT flag set";
        return false;
    }
    uint64_t origPayloadSize;
    bool success = getPayloadSize(orig, origPayloadSize);
    if(!success) {
        LOG_ERROR(subprocess) << "Cannot fragment: cannot determine payload size";
        return false;
    }
    if(sz >= origPayloadSize) {
        LOG_ERROR(subprocess) << "Cannot fragment: fragment size " << sz << " exceeds payload size " << origPayloadSize;
        return false;
    }
    // TODO need original to be  freshly rendered


    // The offset and total application data unit values in the primary block
    // are "global", i.e. if there's already a fragment, then we need to take
    // the existing offset and total length into account. This is different
    // than when we later copy over the primary block. Those offsets are relative
    // to the bundle that we're fragmenting.

    bool origIsFragment = ((flags & BPV6_BUNDLEFLAG::ISFRAGMENT) == BPV6_BUNDLEFLAG::ISFRAGMENT);

    uint64_t firstOffset, secondOffset, totalApplicationDataUnitLength;
    if(origIsFragment) {
        uint64_t origOffset = orig.m_primaryBlockView.header.m_fragmentOffset;
        firstOffset = origOffset;
        secondOffset = origOffset + sz;
        totalApplicationDataUnitLength = orig.m_primaryBlockView.header.m_totalApplicationDataUnitLength;
    } else {
        firstOffset = 0;
        secondOffset = sz;
        totalApplicationDataUnitLength = origPayloadSize;

    }

    copyPrimary(orig, a, firstOffset, totalApplicationDataUnitLength);
    copyPrimary(orig, b, secondOffset, totalApplicationDataUnitLength);

    bool beforePayload = true;
    int i = 0;
    for(BundleViewV6::Bpv6CanonicalBlockView & block : orig.m_listCanonicalBlockView) {
        if(!block.headerPtr) {
            LOG_ERROR(subprocess) << "Cannot fragment: Bundle block header is null";
            return false;
        }
        bool isPayload = block.headerPtr->m_blockTypeCode == BPV6_BLOCK_TYPE_CODE::PAYLOAD;
        bool replicateInAll = block.headerPtr->m_blockProcessingControlFlags == BPV6_BLOCKFLAG::MUST_BE_REPLICATED_IN_EVERY_FRAGMENT;
        if(isPayload) {
            appendPayloadBlock(block, a, 0, sz);
            appendPayloadBlock(block, b, sz, origPayloadSize);
            beforePayload = false;
        } else if(replicateInAll) {
            appendBlock(block, a);
            appendBlock(block, b);
        } else if(beforePayload) {
            appendBlock(block, a);
        } else if(!beforePayload) {
            appendBlock(block, b);
        } else {
            //SANITY CHECK - cannot logically get here
            LOG_ERROR(subprocess) << "Logic error while fragmenting";
            return false;
        }
    }

    // TODO how do we determine the render size for this?
    a.Render(orig.m_renderedBundle.size());
    b.Render(orig.m_renderedBundle.size());

    return true;
}
