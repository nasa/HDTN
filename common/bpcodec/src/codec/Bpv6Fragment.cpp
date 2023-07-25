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
#include "codec/Bpv6Fragment.h"
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

static void copyPrimaryFragment(const BundleViewV6& orig, BundleViewV6& copy, uint64_t offset, uint64_t aduLength) {
    const Bpv6CbhePrimaryBlock & origHdr = orig.m_primaryBlockView.header;
    Bpv6CbhePrimaryBlock & copyHdr = copy.m_primaryBlockView.header;

    copyHdr = origHdr;

    copyHdr.m_bundleProcessingControlFlags |= BPV6_BUNDLEFLAG::ISFRAGMENT;
    copyHdr.m_fragmentOffset = offset;
    LOG_INFO(subprocess) << "Set fragment offset to " << copyHdr.m_fragmentOffset;
    copyHdr.m_totalApplicationDataUnitLength = aduLength;

    copy.m_primaryBlockView.SetManuallyModified(); // TODO needed?
}

static void copyPrimaryNoFragment(const BundleViewV6& orig, BundleViewV6& copy) {
    const Bpv6CbhePrimaryBlock & origHdr = orig.m_primaryBlockView.header;
    Bpv6CbhePrimaryBlock & copyHdr = copy.m_primaryBlockView.header;

    copyHdr = origHdr;

    copyHdr.m_bundleProcessingControlFlags &= ~BPV6_BUNDLEFLAG::ISFRAGMENT;
    copyHdr.m_fragmentOffset = 0;
    copyHdr.m_totalApplicationDataUnitLength = 0;

    copy.m_primaryBlockView.SetManuallyModified(); // TODO needed?
}


// start inclusive, end exclusive (e.g. startOffset=0, endOffset=2 consists of offsets 0 and 1)
static void appendFragmentPayloadBlock(BundleViewV6::Bpv6CanonicalBlockView & block, BundleViewV6& bv, uint64_t startOffset, uint64_t endOffset) {
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

static void createPayloadBlock(BundleViewV6::Bpv6CanonicalBlockView & block, BundleViewV6& bv, std::vector<uint8_t>& adu) {
    bv.m_listCanonicalBlockView.emplace_back();
    BundleViewV6::Bpv6CanonicalBlockView & copyBlockView = bv.m_listCanonicalBlockView.back();

    copyBlockView.headerPtr = boost::make_unique<Bpv6CanonicalBlock>();

    Bpv6CanonicalBlock& copyBlock = *copyBlockView.headerPtr;
    Bpv6CanonicalBlock& origBlock = *block.headerPtr;

    copyBlock.m_blockTypeCode = origBlock.m_blockTypeCode;
    copyBlock.m_blockProcessingControlFlags = origBlock.m_blockProcessingControlFlags;
    copyBlock.m_blockTypeSpecificDataLength = adu.size();
    copyBlock.m_blockTypeSpecificDataPtr = adu.data();

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

static bool MustReplicateInAll(BPV6_BLOCKFLAG flags) {
    BPV6_BLOCKFLAG rep = BPV6_BLOCKFLAG::MUST_BE_REPLICATED_IN_EVERY_FRAGMENT;
    return (flags & rep) == rep;
}

bool fragment(BundleViewV6& orig, uint64_t sz, std::list<BundleViewV6> & fragments) {

    fragments.clear();

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

    const bool origIsFragment = ((flags & BPV6_BUNDLEFLAG::ISFRAGMENT) == BPV6_BUNDLEFLAG::ISFRAGMENT);
    const uint64_t baseAbsoluteOffset = origIsFragment ? orig.m_primaryBlockView.header.m_fragmentOffset : 0;
    const uint64_t totalApplicationDataUnitLength = origIsFragment ? orig.m_primaryBlockView.header.m_totalApplicationDataUnitLength : origPayloadSize;

    const int numFragments = (origPayloadSize + (sz - 1)) / sz;
    LOG_INFO(subprocess) << "Making " << numFragments << " fragments with base offset " << baseAbsoluteOffset << " and adu len " << totalApplicationDataUnitLength;
 
    for(int i = 0; i < numFragments; i++) {
        const bool isFirst = (i == 0);
        const bool isLast = (i == (numFragments - 1));

        // Relative values are used for payload block, absolute values are written to primary
        const uint64_t relativeStartOffset = i * sz;
        const uint64_t relativeEndOffset = std::min(relativeStartOffset + sz, origPayloadSize);
        uint64_t absoluteStartOffset = baseAbsoluteOffset + relativeStartOffset;

        std::string loc = isFirst ? "[first]" : isLast ? "[last]" : "[mid]";
        LOG_INFO(subprocess) << "Building " << i << "/" << numFragments << " " << loc
            << " relative: " << relativeStartOffset << "-" << relativeEndOffset
            << " absolute: " << absoluteStartOffset;

        BundleViewV6 &bv = fragments.emplace_back();

        copyPrimaryFragment(orig, bv, absoluteStartOffset, totalApplicationDataUnitLength);

        bool beforePayload = true;
        for(BundleViewV6::Bpv6CanonicalBlockView & block : orig.m_listCanonicalBlockView) {
            if(!block.headerPtr) {
                LOG_ERROR(subprocess) << "Cannot fragment: Bundle block header is null";
                return false;
            }
            bool isPayload = block.headerPtr->m_blockTypeCode == BPV6_BLOCK_TYPE_CODE::PAYLOAD;
            bool replicateInAll = MustReplicateInAll(block.headerPtr->m_blockProcessingControlFlags);
            if(isPayload) {
                appendFragmentPayloadBlock(block, bv, relativeStartOffset, relativeEndOffset);
                beforePayload = false;
            } else if(replicateInAll) {
                appendBlock(block, bv);
            } else if(isFirst && beforePayload) {
                appendBlock(block, bv);
            } else if(isLast && !beforePayload) {
                appendBlock(block, bv);
            } else {
                // Processing a "middle fragment"; not the payload and not replicating in all
            }
        }

        // TODO how do we determine the render size for this?
        bv.Render(orig.m_renderedBundle.size());
    }

    return true;
}

static bool AssemblePayload(std::list<BundleViewV6>& fragments, std::vector<uint8_t>& adu) {
    if(fragments.size() == 0) {
        LOG_ERROR(subprocess) << "Cannot create payload; fragment vector empty";
        return false;
    }

    FragmentSet::data_fragment_set_t fragmentSet;

    uint64_t size = fragments.front().m_primaryBlockView.header.m_totalApplicationDataUnitLength;
    adu.resize(size);

    for(auto &fragment: fragments) {
        std::vector<BundleViewV6::Bpv6CanonicalBlockView*> blocks;
        fragment.GetCanonicalBlocksByType(BPV6_BLOCK_TYPE_CODE::PAYLOAD, blocks);
        if(blocks.size() != 1 || !blocks[0]) {
            LOG_ERROR(subprocess) << "While assembling fragment unable to find payload block";
            return false;
        }
        BundleViewV6::Bpv6CanonicalBlockView & payload = *blocks[0];

        // TODO do we need to check if dirty or marked for deletion?
        if(!payload.headerPtr) {
            LOG_ERROR(subprocess) << "Header is null while reassembling";
            return false;
        }

        uint64_t o = fragment.m_primaryBlockView.header.m_fragmentOffset;
        uint64_t s = payload.headerPtr->m_blockTypeSpecificDataLength;

        FragmentSet::InsertFragment(fragmentSet, FragmentSet::data_fragment_t(o, o + s));

        const uint8_t *p = payload.headerPtr->m_blockTypeSpecificDataPtr;
        if(o + s > size) {
            LOG_ERROR(subprocess) << "bundle offset and size exceeds total adu size";
            return false;
        }
        memcpy(adu.data() + o, p, s);
    }

    FragmentSet::data_fragment_t expectedDataFragment(0, size);
    if((fragmentSet.size() != 1) || (fragmentSet.count(expectedDataFragment) != 1)) {
        LOG_ERROR(subprocess) << "Fragments do not make up a whole bundle";
        return false;
    }

    return true;
}

bool CompareOffsets(const BundleViewV6 &a, const BundleViewV6 &b) {
    return a.m_primaryBlockView.header.m_fragmentOffset < b.m_primaryBlockView.header.m_fragmentOffset;
}

static bool GetEndFragments(std::list<BundleViewV6> &fragments, BundleViewV6 **first, BundleViewV6 **last) {
    std::pair<std::list<BundleViewV6>::iterator, std::list<BundleViewV6>::iterator> p =
        std::minmax_element(fragments.begin(), fragments.end(), CompareOffsets);
    if (p.first == fragments.end() || p.second == fragments.end()) {
        return false;
    }
    *first = &(*(p.first));
    *last = &(*(p.second));
    return true;
}

static bool validate(std::list<BundleViewV6> &fragments) {
    if (fragments.size() == 0) {
        LOG_ERROR(subprocess) << "cannot reassemble; fragment vector empty";
        return false;
    }

    cbhe_eid_t eid = fragments.front().m_primaryBlockView.header.m_sourceNodeId;
    TimestampUtil::bpv6_creation_timestamp_t ts = fragments.front().m_primaryBlockView.header.m_creationTimestamp;

    for(auto & fragment: fragments) {
        if(fragment.m_primaryBlockView.header.m_sourceNodeId != eid) {
            LOG_ERROR(subprocess) << "while reassembling; source eid does not match";
            return false;
        }
        if(fragment.m_primaryBlockView.header.m_creationTimestamp!= ts) {
            LOG_ERROR(subprocess) << "while reassembling; creation timestamp does not match";
            return false;
        }
        const BPV6_BUNDLEFLAG fragFlag = BPV6_BUNDLEFLAG::ISFRAGMENT;
        if((fragment.m_primaryBlockView.header.m_bundleProcessingControlFlags & fragFlag) != fragFlag) {
            LOG_ERROR(subprocess) << "while reassembling; fragment flag not set on bundle";
            return false;
        }
    }

    return true;
}

bool AssembleFragments(std::list<BundleViewV6>& fragments, BundleViewV6& bundle) {
    if(!validate(fragments)) {
        LOG_ERROR(subprocess) << "Fragments do not have matching IDs";
        return false;
    }

    std::vector<uint8_t> adu;
    if(!AssemblePayload(fragments, adu)) {
        LOG_ERROR(subprocess) << "Failed to assemble payload";
        return false;
    }

    // Need these to grab any non-payload blocks
    BundleViewV6 *first = NULL, *last = NULL;
    if(!GetEndFragments(fragments, &first, &last)) {
        LOG_ERROR(subprocess) << "Failed to find first and last fragments";
    }

    copyPrimaryNoFragment(*first, bundle);

    // Start blocks + payload
    for(BundleViewV6::Bpv6CanonicalBlockView & block : first->m_listCanonicalBlockView) {
        if(!block.headerPtr) {
            LOG_ERROR(subprocess) << "Cannot assemble: Bundle block header is null";
            return false;
        }
        bool isPayload = block.headerPtr->m_blockTypeCode == BPV6_BLOCK_TYPE_CODE::PAYLOAD;
        if(isPayload) {
            // Can use any of the bundle payload blocks to create the payload block
            // Since we already have a reference, do it here
            createPayloadBlock(block, bundle, adu);
            break;
        }
        appendBlock(block, bundle);
    }

    // End blocks
    bool sawPayload = false;
    for(BundleViewV6::Bpv6CanonicalBlockView & block : last->m_listCanonicalBlockView) {
        if(!block.headerPtr) {
            LOG_ERROR(subprocess) << "Cannot assemble: Bundle block header is null";
            return false;
        }
        if(!sawPayload) {
            sawPayload = block.headerPtr->m_blockTypeCode == BPV6_BLOCK_TYPE_CODE::PAYLOAD;
            continue;
        }
        appendBlock(block, bundle);
    }
    bundle.Render(adu.size() + 1024); // TODO how to find render size?
    return true;
}
