/**
 * @file TestBundleViewV6.cpp
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

static const uint64_t PRIMARY_SRC_NODE = 100;
static const uint64_t PRIMARY_SRC_SVC = 1;
static const uint64_t PRIMARY_DEST_NODE = 200;
static const uint64_t PRIMARY_DEST_SVC = 2;
static const uint64_t PRIMARY_TIME = 1000;
static const uint64_t PRIMARY_LIFETIME = 2000;
static const uint64_t PRIMARY_SEQ = 1;
#define BP_MSG_BUFSZ 2000 //don't care, not used

static void AppendCanonicalBlockAndRender(BundleViewV6 & bv, BPV6_BLOCK_TYPE_CODE newType, const std::string & newBlockBody) {

    //std::cout << "append " << (int)newType << "\n";
    std::unique_ptr<Bpv6CanonicalBlock> blockPtr = boost::make_unique<Bpv6CanonicalBlock>();
    Bpv6CanonicalBlock & block = *blockPtr;
    block.m_blockTypeCode = newType;
    block.m_blockProcessingControlFlags = BPV6_BLOCKFLAG::NO_FLAGS_SET; //don't worry about block.flags as last block because Render will take care of that automatically
    block.m_blockTypeSpecificDataLength = newBlockBody.length();
    block.m_blockTypeSpecificDataPtr = (uint8_t*)newBlockBody.data(); //blockBodyAsVecUint8 must remain in scope until after render
    bv.AppendMoveCanonicalBlock(std::move(blockPtr));
    uint64_t expectedRenderSize;
    BOOST_REQUIRE(bv.GetSerializationSize(expectedRenderSize));
    BOOST_REQUIRE(bv.Render(5000));
    BOOST_REQUIRE_EQUAL(bv.m_frontBuffer.size(), expectedRenderSize);
    //check again after render
    BOOST_REQUIRE(bv.GetSerializationSize(expectedRenderSize));
    BOOST_REQUIRE_EQUAL(bv.m_frontBuffer.size(), expectedRenderSize);
    
}
static void PrependCanonicalBlockAndRender_AllocateOnly(BundleViewV6 & bv, BPV6_BLOCK_TYPE_CODE newType, uint64_t dataLengthToAllocate) {
    std::unique_ptr<Bpv6CanonicalBlock> blockPtr = boost::make_unique<Bpv6CanonicalBlock>();
    Bpv6CanonicalBlock & block = *blockPtr;
    block.m_blockTypeCode = newType;
    block.m_blockProcessingControlFlags = BPV6_BLOCKFLAG::NO_FLAGS_SET; //don't worry about block.flags as last block because Render will take care of that automatically
    block.m_blockTypeSpecificDataLength = dataLengthToAllocate;
    block.m_blockTypeSpecificDataPtr = NULL;
    bv.PrependMoveCanonicalBlock(std::move(blockPtr));
    uint64_t expectedRenderSize;
    BOOST_REQUIRE(bv.GetSerializationSize(expectedRenderSize));
    BOOST_REQUIRE(bv.Render(5000));
    BOOST_REQUIRE_EQUAL(bv.m_frontBuffer.size(), expectedRenderSize);
    //check again after render
    BOOST_REQUIRE(bv.GetSerializationSize(expectedRenderSize));
    BOOST_REQUIRE_EQUAL(bv.m_frontBuffer.size(), expectedRenderSize);
}

static void ChangeCanonicalBlockAndRender(BundleViewV6 & bv, BPV6_BLOCK_TYPE_CODE oldType, BPV6_BLOCK_TYPE_CODE newType, const std::string & newBlockBody) {
    
    //std::cout << "change " << (int)oldType << " to " << (int)newType << "\n";
    std::vector<BundleViewV6::Bpv6CanonicalBlockView*> blocks;
    bv.GetCanonicalBlocksByType(oldType, blocks);
    BOOST_REQUIRE_EQUAL(blocks.size(), 1);
    Bpv6CanonicalBlock & block = *(blocks[0]->headerPtr);
    block.m_blockTypeCode = newType;
    //don't worry about block.flags as last block because Render will take care of that automatically
    block.m_blockTypeSpecificDataLength = newBlockBody.size();
    block.m_blockTypeSpecificDataPtr = (uint8_t*)newBlockBody.data(); //newBlockBody must remain in scope until after render
    blocks[0]->SetManuallyModified();

    uint64_t expectedRenderSize;
    BOOST_REQUIRE(bv.GetSerializationSize(expectedRenderSize));
    BOOST_REQUIRE(bv.Render(5000));
    BOOST_REQUIRE_EQUAL(bv.m_frontBuffer.size(), expectedRenderSize);
    //check again after render
    BOOST_REQUIRE(bv.GetSerializationSize(expectedRenderSize));
    BOOST_REQUIRE_EQUAL(bv.m_frontBuffer.size(), expectedRenderSize);
    
}


static void GenerateBundle(const std::vector<BPV6_BLOCK_TYPE_CODE> & canonicalTypesVec, const std::vector<std::string> & canonicalBodyStringsVec, BundleViewV6 & bv) {
    Bpv6CbhePrimaryBlock & primary = bv.m_primaryBlockView.header;
    primary.SetZero();

    primary.m_bundleProcessingControlFlags = BPV6_BUNDLEFLAG::PRIORITY_EXPEDITED | BPV6_BUNDLEFLAG::SINGLETON | BPV6_BUNDLEFLAG::NOFRAGMENT | BPV6_BUNDLEFLAG::CUSTODY_REQUESTED;
    primary.m_sourceNodeId.Set(PRIMARY_SRC_NODE, PRIMARY_SRC_SVC);
    primary.m_destinationEid.Set(PRIMARY_DEST_NODE, PRIMARY_DEST_SVC);
    primary.m_custodianEid.SetZero();
    primary.m_reportToEid.SetZero();
    primary.m_creationTimestamp.secondsSinceStartOfYear2000 = PRIMARY_TIME; //(uint64_t)bpv6_unix_to_5050(curr_time);
    primary.m_lifetimeSeconds = PRIMARY_LIFETIME;
    primary.m_creationTimestamp.sequenceNumber = PRIMARY_SEQ;
    bv.m_primaryBlockView.SetManuallyModified();

    for (std::size_t i = 0; i < canonicalTypesVec.size(); ++i) {
        std::unique_ptr<Bpv6CanonicalBlock> blockPtr = boost::make_unique<Bpv6CanonicalBlock>();
        Bpv6CanonicalBlock & block = *blockPtr;
        //block.SetZero();

        block.m_blockTypeCode = canonicalTypesVec[i];
        block.m_blockProcessingControlFlags = BPV6_BLOCKFLAG::NO_FLAGS_SET; //something for checking against
        const std::string & blockBody = canonicalBodyStringsVec[i];
        block.m_blockTypeSpecificDataLength = blockBody.length();
        block.m_blockTypeSpecificDataPtr = (uint8_t*)blockBody.data(); //blockBody must remain in scope until after render

        BOOST_REQUIRE(blockPtr);
        bv.AppendMoveCanonicalBlock(std::move(blockPtr));
        BOOST_REQUIRE(!blockPtr);
    }

    BOOST_REQUIRE(bv.Render(5000));
}

BOOST_AUTO_TEST_CASE(BundleViewV6TestCase)
{
    
    {
        const std::vector<BPV6_BLOCK_TYPE_CODE> canonicalTypesVec = { 
            BPV6_BLOCK_TYPE_CODE::PAYLOAD,
            BPV6_BLOCK_TYPE_CODE::UNUSED_7,
            BPV6_BLOCK_TYPE_CODE::UNUSED_6,
            BPV6_BLOCK_TYPE_CODE::UNUSED_11
        };
        const std::vector<std::string> canonicalBodyStringsVec = { "The ", "quick ", " brown", " fox" };
        BundleViewV6 bv;
        GenerateBundle(canonicalTypesVec, canonicalBodyStringsVec, bv);
        padded_vector_uint8_t bundleSerializedOriginal(bv.m_frontBuffer);


        BOOST_REQUIRE_GT(bundleSerializedOriginal.size(), 0);
        padded_vector_uint8_t bundleSerializedCopy(bundleSerializedOriginal); //the copy can get modified by bundle view on first load
        BOOST_REQUIRE(bundleSerializedOriginal == bundleSerializedCopy);
        bv.Reset();
        //std::cout << "sz " << bundleSerializedCopy.size() << std::endl;
        BOOST_REQUIRE(bv.LoadBundle(&bundleSerializedCopy[0], bundleSerializedCopy.size()));
        BOOST_REQUIRE(bv.m_backBuffer != bundleSerializedCopy);
        BOOST_REQUIRE(bv.m_frontBuffer != bundleSerializedCopy);

        Bpv6CbhePrimaryBlock & primary = bv.m_primaryBlockView.header;
        BOOST_REQUIRE_EQUAL(primary.m_sourceNodeId, cbhe_eid_t(PRIMARY_SRC_NODE, PRIMARY_SRC_SVC));
        BOOST_REQUIRE_EQUAL(primary.m_destinationEid, cbhe_eid_t(PRIMARY_DEST_NODE, PRIMARY_DEST_SVC));
        BOOST_REQUIRE_EQUAL(primary.m_creationTimestamp, TimestampUtil::bpv6_creation_timestamp_t(PRIMARY_TIME, PRIMARY_SEQ));
        BOOST_REQUIRE_EQUAL(primary.m_lifetimeSeconds, PRIMARY_LIFETIME);
        BOOST_REQUIRE_EQUAL(bv.m_primaryBlockView.actualSerializedPrimaryBlockPtr.size(), primary.GetSerializationSize());

        {
            //constructor, equality, assignment tests
            Bpv6CbhePrimaryBlock p2;
            p2.SetZero();
            BOOST_REQUIRE(primary != p2);
            p2 = primary;
            BOOST_REQUIRE(primary == p2);
            Bpv6CbhePrimaryBlock p3(p2);
            BOOST_REQUIRE(p3 == p2);
            Bpv6CbhePrimaryBlock p4(std::move(p3));
            BOOST_REQUIRE(p4 == p3); //no heap variables contained so move constructor acts like copy constructor
            Bpv6CbhePrimaryBlock p5; p5 = std::move(p4);
            BOOST_REQUIRE(p5 == p4); //no heap variables contained so move assignment acts like copy assignment
            Bpv6CbhePrimaryBlock p6; p6 = p5;
            BOOST_REQUIRE(p6 == p5);
        }

        BOOST_REQUIRE_EQUAL(bv.GetNumCanonicalBlocks(), canonicalTypesVec.size());
        BOOST_REQUIRE_EQUAL(bv.GetCanonicalBlockCountByType(BPV6_BLOCK_TYPE_CODE::UNUSED_12), 0);
        for (std::size_t i = 0; i < canonicalTypesVec.size(); ++i) {
            BOOST_REQUIRE_EQUAL(bv.GetCanonicalBlockCountByType(canonicalTypesVec[i]), 1);
            std::vector<BundleViewV6::Bpv6CanonicalBlockView*> blocks;
            bv.GetCanonicalBlocksByType(canonicalTypesVec[i], blocks);
            BOOST_REQUIRE_EQUAL(blocks.size(), 1);
            const char * strPtr = (const char *)blocks[0]->headerPtr->m_blockTypeSpecificDataPtr;
            std::string s(strPtr, strPtr + blocks[0]->headerPtr->m_blockTypeSpecificDataLength);
            BOOST_REQUIRE_EQUAL(s, canonicalBodyStringsVec[i]);
            BOOST_REQUIRE_EQUAL(blocks[0]->headerPtr->m_blockTypeCode, canonicalTypesVec[i]);
        }

        uint64_t expectedRenderSize;
        BOOST_REQUIRE(bv.GetSerializationSize(expectedRenderSize));
        BOOST_REQUIRE(bv.Render(5000));
        BOOST_REQUIRE(bv.m_backBuffer != bundleSerializedCopy);
        BOOST_REQUIRE_EQUAL(bv.m_frontBuffer.size(), bundleSerializedCopy.size());
        BOOST_REQUIRE_EQUAL(bv.m_frontBuffer.size(), expectedRenderSize);
        BOOST_REQUIRE(bv.m_frontBuffer == bundleSerializedCopy);

        //change 2nd block from quick to slow and type from 7 to 12 and render
        std::string slowString("slow ");
        ChangeCanonicalBlockAndRender(bv, BPV6_BLOCK_TYPE_CODE::UNUSED_7, BPV6_BLOCK_TYPE_CODE::UNUSED_12, slowString);
        BOOST_REQUIRE_EQUAL(bv.m_frontBuffer.size(), bv.m_backBuffer.size() - 1); //"quick" to "slow"
        BOOST_REQUIRE(bv.m_frontBuffer != bundleSerializedOriginal);
        BOOST_REQUIRE_EQUAL(bv.m_frontBuffer.size(), bv.m_renderedBundle.size());
        BOOST_REQUIRE_EQUAL(bv.GetNumCanonicalBlocks(), canonicalTypesVec.size());

        //Render again
        //std::cout << "render again\n";
        BOOST_REQUIRE(bv.Render(5000));
        BOOST_REQUIRE(bv.m_frontBuffer == bv.m_backBuffer);
        
        //revert 2nd block
        std::string quickString("quick ");
        ChangeCanonicalBlockAndRender(bv, BPV6_BLOCK_TYPE_CODE::UNUSED_12, BPV6_BLOCK_TYPE_CODE::UNUSED_7, quickString);
        BOOST_REQUIRE_EQUAL(bv.m_frontBuffer.size(), bundleSerializedOriginal.size());
        BOOST_REQUIRE_EQUAL(bv.m_frontBuffer.size(), bv.m_renderedBundle.size());
        BOOST_REQUIRE(bv.m_frontBuffer == bundleSerializedOriginal);

        //change type 6 flags
        {
            std::vector<BundleViewV6::Bpv6CanonicalBlockView*> blocks;
            bv.GetCanonicalBlocksByType(BPV6_BLOCK_TYPE_CODE::UNUSED_6, blocks);
            BOOST_REQUIRE_EQUAL(blocks.size(), 1);
            BOOST_REQUIRE(!blocks[0]->dirty);
            BOOST_REQUIRE(!blocks[0]->HasBlockProcessingControlFlagSet(BPV6_BLOCKFLAG::DISCARD_BLOCK_IF_IT_CANT_BE_PROCESSED));
            blocks[0]->SetBlockProcessingControlFlagAndDirtyIfNecessary(BPV6_BLOCKFLAG::DISCARD_BLOCK_IF_IT_CANT_BE_PROCESSED);
            BOOST_REQUIRE(!blocks[0]->dirty); //no render required
            BOOST_REQUIRE(blocks[0]->HasBlockProcessingControlFlagSet(BPV6_BLOCKFLAG::DISCARD_BLOCK_IF_IT_CANT_BE_PROCESSED));
            BOOST_REQUIRE_EQUAL(bv.m_frontBuffer.size(), bv.m_renderedBundle.size()); //currently rendering to front buffer
            BOOST_REQUIRE(bv.m_frontBuffer != bundleSerializedOriginal); //differ by flag only

            blocks[0]->ClearBlockProcessingControlFlagAndDirtyIfNecessary(BPV6_BLOCKFLAG::DISCARD_BLOCK_IF_IT_CANT_BE_PROCESSED);
            BOOST_REQUIRE(!blocks[0]->dirty); //no render required
            BOOST_REQUIRE(!blocks[0]->HasBlockProcessingControlFlagSet(BPV6_BLOCKFLAG::DISCARD_BLOCK_IF_IT_CANT_BE_PROCESSED));
            BOOST_REQUIRE_EQUAL(bv.m_frontBuffer.size(), bv.m_renderedBundle.size()); //currently rendering to front buffer
            BOOST_REQUIRE(bv.m_frontBuffer == bundleSerializedOriginal); //back to equal


            //add big flag > 127 requiring rerender
            static const BPV6_BLOCKFLAG bigFlag = static_cast<BPV6_BLOCKFLAG>(1 << 26);
            BOOST_REQUIRE(!blocks[0]->HasBlockProcessingControlFlagSet(bigFlag));
            blocks[0]->SetBlockProcessingControlFlagAndDirtyIfNecessary(bigFlag);
            BOOST_REQUIRE(blocks[0]->dirty); //render required
            BOOST_REQUIRE(blocks[0]->HasBlockProcessingControlFlagSet(bigFlag));

            //std::cout << "render bigflag in 3rd block\n";
            BOOST_REQUIRE(bv.Render(5000));
            BOOST_REQUIRE_EQUAL(bv.m_frontBuffer.size(), bundleSerializedOriginal.size() + 3);
            const char * strPtr = (const char *)blocks[0]->headerPtr->m_blockTypeSpecificDataPtr;
            std::string s(strPtr, strPtr + blocks[0]->headerPtr->m_blockTypeSpecificDataLength);
            BOOST_REQUIRE_EQUAL(s, canonicalBodyStringsVec[2]);
            const uint8_t * typePtr = (const uint8_t *)blocks[0]->actualSerializedBlockPtr.data();
            BOOST_REQUIRE_EQUAL(static_cast<BPV6_BLOCK_TYPE_CODE>(*typePtr), canonicalTypesVec[2]);
            BOOST_REQUIRE(!blocks[0]->dirty);
            BOOST_REQUIRE(blocks[0]->HasBlockProcessingControlFlagSet(bigFlag));

            //remove big flag requiring rerender
            blocks[0]->ClearBlockProcessingControlFlagAndDirtyIfNecessary(bigFlag);
            BOOST_REQUIRE(blocks[0]->dirty); //render required
            BOOST_REQUIRE(!blocks[0]->HasBlockProcessingControlFlagSet(bigFlag));

            //std::cout << "render remove bigflag in 3rd block\n";
            BOOST_REQUIRE(bv.Render(5000));
            BOOST_REQUIRE_EQUAL(bv.m_frontBuffer.size(), bundleSerializedOriginal.size());
            BOOST_REQUIRE(bv.m_frontBuffer == bundleSerializedOriginal); //back to equal
        }

        
        {
            //change PRIMARY_SEQ from 1 to 65539 (adding 2 bytes)
            bv.m_primaryBlockView.header.m_creationTimestamp.sequenceNumber = 65539;
            bv.m_primaryBlockView.SetManuallyModified();
            BOOST_REQUIRE(bv.m_primaryBlockView.dirty);
            //std::cout << "render increase primary seq\n";
            BOOST_REQUIRE(bv.Render(5000));
            BOOST_REQUIRE_EQUAL(bv.m_frontBuffer.size(), bundleSerializedOriginal.size() + 2);
            BOOST_REQUIRE(!bv.m_primaryBlockView.dirty); //render removed dirty
            BOOST_REQUIRE_EQUAL(primary.m_lifetimeSeconds, PRIMARY_LIFETIME);
            BOOST_REQUIRE_EQUAL(primary.m_creationTimestamp.sequenceNumber, 65539);

            //restore PRIMARY_SEQ
            bv.m_primaryBlockView.header.m_creationTimestamp.sequenceNumber = PRIMARY_SEQ;
            bv.m_primaryBlockView.SetManuallyModified();
            //std::cout << "render restore primary seq\n";
            BOOST_REQUIRE(bv.Render(5000));
            BOOST_REQUIRE_EQUAL(bv.m_frontBuffer.size(), bundleSerializedOriginal.size());
            BOOST_REQUIRE(bv.m_frontBuffer == bundleSerializedOriginal); //back to equal
        }

        //delete and re-add 4th (last) block
        {
            std::vector<BundleViewV6::Bpv6CanonicalBlockView*> blocks;
            bv.GetCanonicalBlocksByType(canonicalTypesVec.back(), blocks);
            BOOST_REQUIRE_EQUAL(blocks.size(), 1);
            blocks[0]->markedForDeletion = true;
            //std::cout << "render delete last block\n";
            BOOST_REQUIRE(bv.Render(5000));
            BOOST_REQUIRE_EQUAL(bv.GetNumCanonicalBlocks(), canonicalTypesVec.size() - 1);
            BOOST_REQUIRE_EQUAL(bv.m_frontBuffer.size(), bundleSerializedOriginal.size() - (3 + canonicalBodyStringsVec.back().length())); //3 + len("fox ")

            AppendCanonicalBlockAndRender(bv, canonicalTypesVec.back(), canonicalBodyStringsVec.back());
            BOOST_REQUIRE_EQUAL(bv.m_frontBuffer.size(), bundleSerializedOriginal.size());
            BOOST_REQUIRE(bv.m_frontBuffer == bundleSerializedOriginal); //back to equal
        }

        //delete and re-add 1st block by preallocation
        {
            std::vector<BundleViewV6::Bpv6CanonicalBlockView*> blocks;
            bv.GetCanonicalBlocksByType(canonicalTypesVec.front(), blocks);
            BOOST_REQUIRE_EQUAL(blocks.size(), 1);
            blocks[0]->markedForDeletion = true;
            //std::cout << "render delete last block\n";
            BOOST_REQUIRE(bv.Render(5000));
            BOOST_REQUIRE_EQUAL(bv.GetNumCanonicalBlocks(), canonicalTypesVec.size() - 1);
            const uint64_t canonicalSize = //uint64_t bufferSize
                1 + //block type code byte
                1 + //m_blockProcessingControlFlags
                1 + //block length
                canonicalBodyStringsVec.front().length(); //data = len("The ")
            BOOST_REQUIRE_EQUAL(bv.m_frontBuffer.size(), bundleSerializedOriginal.size() - canonicalSize);

            bv.m_backBuffer.assign(bv.m_backBuffer.size(), 0); //make sure zeroed out
            PrependCanonicalBlockAndRender_AllocateOnly(bv, canonicalTypesVec.front(), canonicalBodyStringsVec.front().length()); //0 was block number 0 from GenerateBundle
            BOOST_REQUIRE_EQUAL(bv.m_frontBuffer.size(), bundleSerializedOriginal.size());
            BOOST_REQUIRE(bv.m_frontBuffer != bundleSerializedOriginal); //still not equal, need to copy data
            bv.GetCanonicalBlocksByType(canonicalTypesVec.front(), blocks); //get new preallocated block
            BOOST_REQUIRE_EQUAL(blocks.size(), 1);
            BOOST_REQUIRE_EQUAL(blocks[0]->headerPtr->m_blockTypeSpecificDataLength, canonicalBodyStringsVec.front().length()); //make sure preallocated
            BOOST_REQUIRE_EQUAL((unsigned int)blocks[0]->headerPtr->m_blockTypeSpecificDataPtr[0], 0); //make sure was zeroed out from above
            memcpy(blocks[0]->headerPtr->m_blockTypeSpecificDataPtr, canonicalBodyStringsVec.front().data(), canonicalBodyStringsVec.front().length()); //copy data
            BOOST_REQUIRE(bv.m_frontBuffer == bundleSerializedOriginal); //back to equal
        }

        //test loads.
        {
            BOOST_REQUIRE(bundleSerializedCopy == bundleSerializedOriginal); //back to equal
            BOOST_REQUIRE(bv.CopyAndLoadBundle(&bundleSerializedCopy[0], bundleSerializedCopy.size())); //calls reset
            BOOST_REQUIRE(bv.m_frontBuffer == bundleSerializedCopy);
            BOOST_REQUIRE(bv.SwapInAndLoadBundle(bundleSerializedCopy)); //calls reset
            BOOST_REQUIRE(bv.m_frontBuffer != bundleSerializedCopy);
            BOOST_REQUIRE(bv.m_frontBuffer == bundleSerializedOriginal);
        }

        //reload bundle many times to test BundleViewV6 block recycler (using same BundleViewV6 obj)
        {
            BundleViewV6 bvRecycled;
            BOOST_REQUIRE_EQUAL(bvRecycled.m_listCanonicalBlockView.size(), 0);
            BOOST_REQUIRE(!bvRecycled.m_recycledAdminRecord);
            std::vector<Bpv6CanonicalBlock*> lastBlockPtrs;
            for (unsigned int i = 0; i < 4; ++i) {
                padded_vector_uint8_t toSwapIn(bundleSerializedOriginal);
                BOOST_REQUIRE(bvRecycled.SwapInAndLoadBundle(toSwapIn, false)); //load resets the bundleview
                BOOST_REQUIRE_EQUAL(bvRecycled.m_listCanonicalBlockView.size(), 4);
                if (i) {
                    //make sure blocks were recycled
                    std::vector<Bpv6CanonicalBlock*> nowBlockPtrs;
                    for (BundleViewV6::canonical_block_view_list_t::const_iterator it = bvRecycled.m_listCanonicalBlockView.cbegin();
                        it != bvRecycled.m_listCanonicalBlockView.cend(); ++it)
                    {
                        nowBlockPtrs.push_back(it->headerPtr.get());
                    }
                    BOOST_REQUIRE(lastBlockPtrs == nowBlockPtrs);
                }
                lastBlockPtrs.clear();
                for (BundleViewV6::canonical_block_view_list_t::const_iterator it = bvRecycled.m_listCanonicalBlockView.cbegin();
                    it != bvRecycled.m_listCanonicalBlockView.cend(); ++it)
                {
                    lastBlockPtrs.push_back(it->headerPtr.get());
                }
                BOOST_REQUIRE(!bvRecycled.m_recycledAdminRecord);
            }
        }
    }
}

BOOST_AUTO_TEST_CASE(Bpv6ExtensionBlocksTestCase)
{
    static const uint64_t PREVIOUS_NODE = 12345;
    static const uint64_t PREVIOUS_SVC = 678910;
    static const uint64_t BUNDLE_AGE_MICROSECONDS = 135791113;
    const std::string payloadString = { "This is the data inside the bpv6 payload block!!!" };


    
    BundleViewV6 bv;
    Bpv6CbhePrimaryBlock & primary = bv.m_primaryBlockView.header;
    primary.SetZero();


    primary.m_bundleProcessingControlFlags = BPV6_BUNDLEFLAG::PRIORITY_EXPEDITED | BPV6_BUNDLEFLAG::SINGLETON | BPV6_BUNDLEFLAG::NOFRAGMENT | BPV6_BUNDLEFLAG::CUSTODY_REQUESTED;
    primary.m_sourceNodeId.Set(PRIMARY_SRC_NODE, PRIMARY_SRC_SVC);
    primary.m_destinationEid.Set(PRIMARY_DEST_NODE, PRIMARY_DEST_SVC);
    primary.m_reportToEid.Set(0, 0);
    primary.m_creationTimestamp.secondsSinceStartOfYear2000 = PRIMARY_TIME;
    primary.m_lifetimeSeconds = PRIMARY_LIFETIME;
    primary.m_creationTimestamp.sequenceNumber = PRIMARY_SEQ;
    bv.m_primaryBlockView.SetManuallyModified();

    //add cteb
    {
        std::unique_ptr<Bpv6CanonicalBlock> blockPtr = boost::make_unique<Bpv6CustodyTransferEnhancementBlock>();
        Bpv6CustodyTransferEnhancementBlock & block = *(reinterpret_cast<Bpv6CustodyTransferEnhancementBlock*>(blockPtr.get()));
        //block.SetZero();

        block.m_blockProcessingControlFlags = BPV6_BLOCKFLAG::DISCARD_BLOCK_IF_IT_CANT_BE_PROCESSED; //something for checking against
        block.m_custodyId = 150; //size 2 sdnv
        block.m_ctebCreatorCustodianEidString = "ipn:2.3";
        bv.AppendMoveCanonicalBlock(std::move(blockPtr));
    }

    //add previous hop insertion
    {
        std::unique_ptr<Bpv6CanonicalBlock> blockPtr = boost::make_unique<Bpv6PreviousHopInsertionCanonicalBlock>();
        Bpv6PreviousHopInsertionCanonicalBlock & block = *(reinterpret_cast<Bpv6PreviousHopInsertionCanonicalBlock*>(blockPtr.get()));
        //block.SetZero();

        //block.m_blockProcessingControlFlags = DISCARD_BLOCK_IF_IT_CANT_BE_PROCESSED set by Bpv6PreviousHopInsertionCanonicalBlock constructor
        block.m_previousNode.Set(PREVIOUS_NODE, PREVIOUS_SVC);
        bv.AppendMoveCanonicalBlock(std::move(blockPtr));
    }

    uint64_t metaUriListSerializationSize = 0;
    //add bundle metadata with 3 uris
    {
        std::unique_ptr<Bpv6CanonicalBlock> blockPtr = boost::make_unique<Bpv6MetadataCanonicalBlock>();
        Bpv6MetadataCanonicalBlock & block = *(reinterpret_cast<Bpv6MetadataCanonicalBlock*>(blockPtr.get()));
        //block.SetZero();

        block.m_metadataTypeCode = BPV6_METADATA_TYPE_CODE::URI;
        block.m_metadataContentPtr = boost::make_unique<Bpv6MetadataContentUriList>();
        Bpv6MetadataContentUriList & meta = *(reinterpret_cast<Bpv6MetadataContentUriList*>(block.m_metadataContentPtr.get()));

        meta.m_uriArray.resize(3);
        meta.m_uriArray[0].Set(525, 60001);
        meta.m_uriArray[1].Set(5250, 600010);
        meta.m_uriArray[2].Set(52500, 6000100);

        metaUriListSerializationSize = meta.GetSerializationSize();

        bv.AppendMoveCanonicalBlock(std::move(blockPtr));
    }

    uint64_t metaGenericSerializationSize = 0;
    //add bundle metadata with user defined data
    {
        std::unique_ptr<Bpv6CanonicalBlock> blockPtr = boost::make_unique<Bpv6MetadataCanonicalBlock>();
        Bpv6MetadataCanonicalBlock & block = *(reinterpret_cast<Bpv6MetadataCanonicalBlock*>(blockPtr.get()));
        //block.SetZero();

        block.m_metadataTypeCode = BPV6_METADATA_TYPE_CODE::UNDEFINED_ZERO;
        block.m_metadataContentPtr = boost::make_unique<Bpv6MetadataContentGeneric>();
        Bpv6MetadataContentGeneric & meta = *(reinterpret_cast<Bpv6MetadataContentGeneric*>(block.m_metadataContentPtr.get()));

        meta.m_genericRawMetadata = { 0xd, 0xe, 0xa, 0xd, 0xb, 0xe, 0xe, 0xf };

        metaGenericSerializationSize = meta.GetSerializationSize();

        bv.AppendMoveCanonicalBlock(std::move(blockPtr));
    }

    //add bundle age
    {
        std::unique_ptr<Bpv6CanonicalBlock> blockPtr = boost::make_unique<Bpv6BundleAgeCanonicalBlock>();
        Bpv6BundleAgeCanonicalBlock & block = *(reinterpret_cast<Bpv6BundleAgeCanonicalBlock*>(blockPtr.get()));
        //block.SetZero();

        //block.m_blockProcessingControlFlags = MUST_BE_REPLICATED_IN_EVERY_FRAGMENT set by Bpv6BundleAgeCanonicalBlock constructor
        block.m_bundleAgeMicroseconds = BUNDLE_AGE_MICROSECONDS;
        bv.AppendMoveCanonicalBlock(std::move(blockPtr));
    }

    //add payload block
    {

        std::unique_ptr<Bpv6CanonicalBlock> blockPtr = boost::make_unique<Bpv6CanonicalBlock>();
        Bpv6CanonicalBlock & block = *blockPtr;
        //block.SetZero();

        block.m_blockTypeCode = BPV6_BLOCK_TYPE_CODE::PAYLOAD;
        block.m_blockProcessingControlFlags = BPV6_BLOCKFLAG::DISCARD_BLOCK_IF_IT_CANT_BE_PROCESSED; //something for checking against
        block.m_blockTypeSpecificDataLength = payloadString.size();
        block.m_blockTypeSpecificDataPtr = (uint8_t*)payloadString.data(); //payloadString must remain in scope until after render
        bv.AppendMoveCanonicalBlock(std::move(blockPtr));

    }

    BOOST_REQUIRE(bv.Render(5000));

    padded_vector_uint8_t bundleSerializedOriginal(bv.m_frontBuffer);
    //std::cout << "renderedsize: " << bv.m_frontBuffer.size() << "\n";

    BOOST_REQUIRE_GT(bundleSerializedOriginal.size(), 0);
    padded_vector_uint8_t bundleSerializedCopy(bundleSerializedOriginal); //the copy can get modified by bundle view on first load
    BOOST_REQUIRE(bundleSerializedOriginal == bundleSerializedCopy);
    bv.Reset();
    //std::cout << "sz " << bundleSerializedCopy.size() << std::endl;
    BOOST_REQUIRE(bv.LoadBundle(&bundleSerializedCopy[0], bundleSerializedCopy.size()));
    BOOST_REQUIRE(bv.m_backBuffer != bundleSerializedCopy);
    BOOST_REQUIRE(bv.m_frontBuffer != bundleSerializedCopy);

    BOOST_REQUIRE_EQUAL(primary.m_sourceNodeId, cbhe_eid_t(PRIMARY_SRC_NODE, PRIMARY_SRC_SVC));
    BOOST_REQUIRE_EQUAL(primary.m_destinationEid, cbhe_eid_t(PRIMARY_DEST_NODE, PRIMARY_DEST_SVC));
    BOOST_REQUIRE_EQUAL(primary.m_creationTimestamp, TimestampUtil::bpv6_creation_timestamp_t(PRIMARY_TIME, PRIMARY_SEQ));
    BOOST_REQUIRE_EQUAL(primary.m_lifetimeSeconds, PRIMARY_LIFETIME);
    BOOST_REQUIRE_EQUAL(bv.m_primaryBlockView.actualSerializedPrimaryBlockPtr.size(), primary.GetSerializationSize());

    BOOST_REQUIRE_EQUAL(bv.GetNumCanonicalBlocks(), 6);
    BOOST_REQUIRE_EQUAL(bv.GetCanonicalBlockCountByType(BPV6_BLOCK_TYPE_CODE::CUSTODY_TRANSFER_ENHANCEMENT), 1);
    BOOST_REQUIRE_EQUAL(bv.GetCanonicalBlockCountByType(BPV6_BLOCK_TYPE_CODE::PREVIOUS_HOP_INSERTION), 1);
    BOOST_REQUIRE_EQUAL(bv.GetCanonicalBlockCountByType(BPV6_BLOCK_TYPE_CODE::METADATA_EXTENSION), 2);
    BOOST_REQUIRE_EQUAL(bv.GetCanonicalBlockCountByType(BPV6_BLOCK_TYPE_CODE::BUNDLE_AGE), 1);
    BOOST_REQUIRE_EQUAL(bv.GetCanonicalBlockCountByType(BPV6_BLOCK_TYPE_CODE::PAYLOAD), 1);
    BOOST_REQUIRE_EQUAL(bv.GetCanonicalBlockCountByType(BPV6_BLOCK_TYPE_CODE::UNUSED_11), 0);

    //get cteb
    {
        std::vector<BundleViewV6::Bpv6CanonicalBlockView*> blocks;
        bv.GetCanonicalBlocksByType(BPV6_BLOCK_TYPE_CODE::CUSTODY_TRANSFER_ENHANCEMENT, blocks);
        BOOST_REQUIRE_EQUAL(blocks.size(), 1);
        Bpv6CustodyTransferEnhancementBlock* ctebBlockPtr = dynamic_cast<Bpv6CustodyTransferEnhancementBlock*>(blocks[0]->headerPtr.get());
        BOOST_REQUIRE(ctebBlockPtr);
        if (ctebBlockPtr) {
            BOOST_REQUIRE_EQUAL(ctebBlockPtr->m_blockTypeCode, BPV6_BLOCK_TYPE_CODE::CUSTODY_TRANSFER_ENHANCEMENT);
            BOOST_REQUIRE(!blocks[0]->HasBlockProcessingControlFlagSet(BPV6_BLOCKFLAG::IS_LAST_BLOCK));
            BOOST_REQUIRE(blocks[0]->HasBlockProcessingControlFlagSet(BPV6_BLOCKFLAG::DISCARD_BLOCK_IF_IT_CANT_BE_PROCESSED));
            BOOST_REQUIRE_EQUAL(ctebBlockPtr->m_custodyId, 150);
            BOOST_REQUIRE_EQUAL(ctebBlockPtr->m_ctebCreatorCustodianEidString, "ipn:2.3");
            BOOST_REQUIRE_EQUAL(blocks[0]->actualSerializedBlockPtr.size(), ctebBlockPtr->GetSerializationSize());

            //misc
            {
                const Bpv6CustodyTransferEnhancementBlock& cteb = *ctebBlockPtr;
                Bpv6CustodyTransferEnhancementBlock cteb2 = *ctebBlockPtr;
                BOOST_REQUIRE(!(cteb != cteb2));
                Bpv6CustodyTransferEnhancementBlock ctebCopy = cteb;
                Bpv6CustodyTransferEnhancementBlock ctebCopy2(cteb);
                Bpv6CustodyTransferEnhancementBlock cteb2Moved = std::move(cteb2);
                BOOST_REQUIRE(cteb != cteb2); //cteb2 moved
                BOOST_REQUIRE(cteb == cteb2Moved);
                BOOST_REQUIRE(cteb == ctebCopy);
                BOOST_REQUIRE(cteb == ctebCopy2);
                Bpv6CustodyTransferEnhancementBlock cteb2Moved2(std::move(cteb2Moved));
                BOOST_REQUIRE(cteb != cteb2Moved); //cteb2 moved
                BOOST_REQUIRE(cteb == cteb2Moved2);
            }
        }
    }

    //get previous hop insertion
    {
        std::vector<BundleViewV6::Bpv6CanonicalBlockView*> blocks;
        bv.GetCanonicalBlocksByType(BPV6_BLOCK_TYPE_CODE::PREVIOUS_HOP_INSERTION, blocks);
        BOOST_REQUIRE_EQUAL(blocks.size(), 1);
        Bpv6PreviousHopInsertionCanonicalBlock* phibPtr = dynamic_cast<Bpv6PreviousHopInsertionCanonicalBlock*>(blocks[0]->headerPtr.get());
        BOOST_REQUIRE(phibPtr);
        if (phibPtr) {
            BOOST_REQUIRE_EQUAL(phibPtr->m_blockTypeCode, BPV6_BLOCK_TYPE_CODE::PREVIOUS_HOP_INSERTION);
            BOOST_REQUIRE(!blocks[0]->HasBlockProcessingControlFlagSet(BPV6_BLOCKFLAG::IS_LAST_BLOCK));
            BOOST_REQUIRE(blocks[0]->HasBlockProcessingControlFlagSet(BPV6_BLOCKFLAG::DISCARD_BLOCK_IF_IT_CANT_BE_PROCESSED));
            BOOST_REQUIRE(!blocks[0]->HasBlockProcessingControlFlagSet(BPV6_BLOCKFLAG::MUST_BE_REPLICATED_IN_EVERY_FRAGMENT));
            BOOST_REQUIRE_EQUAL(phibPtr->m_previousNode, cbhe_eid_t(PREVIOUS_NODE, PREVIOUS_SVC));
            BOOST_REQUIRE_EQUAL(blocks[0]->actualSerializedBlockPtr.size(), phibPtr->GetSerializationSize());
        }
    }

    //get bundle metadata blocks, first block has 3 uris, second block is generic user-defined
    {
        std::vector<BundleViewV6::Bpv6CanonicalBlockView*> blocks;
        bv.GetCanonicalBlocksByType(BPV6_BLOCK_TYPE_CODE::METADATA_EXTENSION, blocks);
        BOOST_REQUIRE_EQUAL(blocks.size(), 2);

        //first block with 3 uris
        {
            Bpv6MetadataCanonicalBlock* metaPtr = dynamic_cast<Bpv6MetadataCanonicalBlock*>(blocks[0]->headerPtr.get());
            BOOST_REQUIRE(metaPtr);
            if (metaPtr) {
                BOOST_REQUIRE_EQUAL(metaPtr->m_blockTypeCode, BPV6_BLOCK_TYPE_CODE::METADATA_EXTENSION);
                BOOST_REQUIRE_EQUAL(blocks[0]->actualSerializedBlockPtr.size(), metaPtr->GetSerializationSize());
                BOOST_REQUIRE_EQUAL(metaPtr->m_metadataTypeCode, BPV6_METADATA_TYPE_CODE::URI);
                Bpv6MetadataContentUriList* uriListPtr = dynamic_cast<Bpv6MetadataContentUriList*>(metaPtr->m_metadataContentPtr.get());
                BOOST_REQUIRE(uriListPtr);
                if (uriListPtr) {
                    Bpv6MetadataContentUriList& uriMeta = *(reinterpret_cast<Bpv6MetadataContentUriList*>(uriListPtr));

                    BOOST_REQUIRE_EQUAL(uriMeta.GetSerializationSize(), metaUriListSerializationSize);
                    BOOST_REQUIRE_EQUAL(uriMeta.m_uriArray.size(), 3);
                    BOOST_REQUIRE_EQUAL(uriMeta.m_uriArray[0], cbhe_eid_t(525, 60001));
                    BOOST_REQUIRE_EQUAL(uriMeta.m_uriArray[1], cbhe_eid_t(5250, 600010));
                    BOOST_REQUIRE_EQUAL(uriMeta.m_uriArray[2], cbhe_eid_t(52500, 6000100));
                }
            }
        }

        //second block with generic data
        {
            Bpv6MetadataCanonicalBlock* metaPtr = dynamic_cast<Bpv6MetadataCanonicalBlock*>(blocks[1]->headerPtr.get());
            BOOST_REQUIRE(metaPtr);
            if (metaPtr) {
                BOOST_REQUIRE_EQUAL(metaPtr->m_blockTypeCode, BPV6_BLOCK_TYPE_CODE::METADATA_EXTENSION);
                BOOST_REQUIRE_EQUAL(blocks[1]->actualSerializedBlockPtr.size(), metaPtr->GetSerializationSize());
                BOOST_REQUIRE_EQUAL(metaPtr->m_metadataTypeCode, BPV6_METADATA_TYPE_CODE::UNDEFINED_ZERO);
                Bpv6MetadataContentGeneric* genericPtr = dynamic_cast<Bpv6MetadataContentGeneric*>(metaPtr->m_metadataContentPtr.get());
                BOOST_REQUIRE(genericPtr);
                if (genericPtr) {
                    Bpv6MetadataContentGeneric& genericMeta = *(reinterpret_cast<Bpv6MetadataContentGeneric*>(genericPtr));

                    BOOST_REQUIRE_EQUAL(genericMeta.GetSerializationSize(), metaGenericSerializationSize);
                    BOOST_REQUIRE_EQUAL(metaGenericSerializationSize, 8);
                    BOOST_REQUIRE(genericMeta.m_genericRawMetadata == std::vector<uint8_t>({ 0xd, 0xe, 0xa, 0xd, 0xb, 0xe, 0xe, 0xf }));
                }
            }
        }
    }

    //get bundle age
    {
        std::vector<BundleViewV6::Bpv6CanonicalBlockView*> blocks;
        bv.GetCanonicalBlocksByType(BPV6_BLOCK_TYPE_CODE::BUNDLE_AGE, blocks);
        BOOST_REQUIRE_EQUAL(blocks.size(), 1);
        Bpv6BundleAgeCanonicalBlock* agePtr = dynamic_cast<Bpv6BundleAgeCanonicalBlock*>(blocks[0]->headerPtr.get());
        BOOST_REQUIRE(agePtr);
        if (agePtr) {
            BOOST_REQUIRE_EQUAL(agePtr->m_blockTypeCode, BPV6_BLOCK_TYPE_CODE::BUNDLE_AGE);
            BOOST_REQUIRE(!blocks[0]->HasBlockProcessingControlFlagSet(BPV6_BLOCKFLAG::IS_LAST_BLOCK));
            BOOST_REQUIRE(!blocks[0]->HasBlockProcessingControlFlagSet(BPV6_BLOCKFLAG::DISCARD_BLOCK_IF_IT_CANT_BE_PROCESSED));
            BOOST_REQUIRE(blocks[0]->HasBlockProcessingControlFlagSet(BPV6_BLOCKFLAG::MUST_BE_REPLICATED_IN_EVERY_FRAGMENT));
            BOOST_REQUIRE_EQUAL(agePtr->m_bundleAgeMicroseconds, BUNDLE_AGE_MICROSECONDS);
            BOOST_REQUIRE_EQUAL(blocks[0]->actualSerializedBlockPtr.size(), agePtr->GetSerializationSize());
        }
    }
    
    //get payload
    {
        std::vector<BundleViewV6::Bpv6CanonicalBlockView*> blocks;
        bv.GetCanonicalBlocksByType(BPV6_BLOCK_TYPE_CODE::PAYLOAD, blocks);
        BOOST_REQUIRE_EQUAL(blocks.size(), 1);
        const char * strPtr = (const char *)blocks[0]->headerPtr->m_blockTypeSpecificDataPtr;
        std::string s(strPtr, strPtr + blocks[0]->headerPtr->m_blockTypeSpecificDataLength);
        //std::cout << "s: " << s << "\n";
        BOOST_REQUIRE_EQUAL(s, payloadString);
        BOOST_REQUIRE_EQUAL(blocks[0]->headerPtr->m_blockTypeCode, BPV6_BLOCK_TYPE_CODE::PAYLOAD);
        BOOST_REQUIRE(blocks[0]->HasBlockProcessingControlFlagSet(BPV6_BLOCKFLAG::IS_LAST_BLOCK));
        BOOST_REQUIRE(blocks[0]->HasBlockProcessingControlFlagSet(BPV6_BLOCKFLAG::DISCARD_BLOCK_IF_IT_CANT_BE_PROCESSED));
        BOOST_REQUIRE_EQUAL(blocks[0]->actualSerializedBlockPtr.size(), blocks[0]->headerPtr->GetSerializationSize());
    }

}

BOOST_AUTO_TEST_CASE(Bpv6BundleStatusReportTestCase)
{
    static const TimestampUtil::dtn_time_t t0(1000, 65535 + 0);
    static const TimestampUtil::dtn_time_t t1(1001, 65535 + 1);
    static const TimestampUtil::dtn_time_t t2(1002, 65535 + 2);
    static const TimestampUtil::dtn_time_t t3(1003, 65535 + 3);
    static const TimestampUtil::dtn_time_t t4(1004, 65535 + 4);
    static const std::string bundleSourceEidStr = "ipn:2.3";
    Bpv6AdministrativeRecordContentBundleStatusReport lastBsr;
    lastBsr.Reset();
    for (unsigned int useFragI = 0; useFragI < 2; ++useFragI) {
        const bool useFrag = (useFragI != 0);
        
        for (unsigned int assertionsMask = 1; assertionsMask < 32; ++assertionsMask) { //start at 1 because you must assert at least one of the four items
            const bool assert0 = ((assertionsMask & 1) != 0);
            const bool assert1 = ((assertionsMask & 2) != 0);
            const bool assert2 = ((assertionsMask & 4) != 0);
            const bool assert3 = ((assertionsMask & 8) != 0);
            const bool assert4 = ((assertionsMask & 16) != 0);
            //std::cout << assert3 << assert2 << assert1 << assert0 << "\n";
            BundleViewV6 bv;
            Bpv6CbhePrimaryBlock & primary = bv.m_primaryBlockView.header;
            primary.SetZero();


            primary.m_bundleProcessingControlFlags = BPV6_BUNDLEFLAG::NOFRAGMENT | BPV6_BUNDLEFLAG::ADMINRECORD;
            primary.m_sourceNodeId.Set(PRIMARY_SRC_NODE, PRIMARY_SRC_SVC);
            primary.m_destinationEid.Set(PRIMARY_DEST_NODE, PRIMARY_DEST_SVC);
            primary.m_reportToEid.Set(0, 0);
            primary.m_creationTimestamp.secondsSinceStartOfYear2000 = PRIMARY_TIME;
            primary.m_lifetimeSeconds = PRIMARY_LIFETIME;
            primary.m_creationTimestamp.sequenceNumber = PRIMARY_SEQ;
            bv.m_primaryBlockView.SetManuallyModified();

            uint64_t bsrSerializationSize = 0;
            //add bundle status report payload block
            {
                std::unique_ptr<Bpv6CanonicalBlock> blockPtr = boost::make_unique<Bpv6AdministrativeRecord>();
                Bpv6AdministrativeRecord & block = *(reinterpret_cast<Bpv6AdministrativeRecord*>(blockPtr.get()));

                //block.m_blockTypeCode = BPV7_BLOCK_TYPE_CODE::PAYLOAD; //not needed because handled by Bpv7AdministrativeRecord constructor
                block.m_blockProcessingControlFlags = BPV6_BLOCKFLAG::DISCARD_BLOCK_IF_IT_CANT_BE_PROCESSED; //something for checking against

                block.m_adminRecordTypeCode = BPV6_ADMINISTRATIVE_RECORD_TYPE_CODE::BUNDLE_STATUS_REPORT;
                block.m_adminRecordContentPtr = boost::make_unique<Bpv6AdministrativeRecordContentBundleStatusReport>();
                Bpv6AdministrativeRecordContentBundleStatusReport & bsr = *(reinterpret_cast<Bpv6AdministrativeRecordContentBundleStatusReport*>(block.m_adminRecordContentPtr.get()));

                /*
                enum class BPV6_BUNDLE_STATUS_REPORT_STATUS_FLAGS : uint8_t {
                NO_FLAGS_SET                                 = 0,
                REPORTING_NODE_RECEIVED_BUNDLE               = (1 << 0),
                REPORTING_NODE_ACCEPTED_CUSTODY_OF_BUNDLE    = (1 << 1),
                REPORTING_NODE_FORWARDED_BUNDLE              = (1 << 2),
                REPORTING_NODE_DELIVERED_BUNDLE              = (1 << 3),
                REPORTING_NODE_DELETED_BUNDLE                = (1 << 4),
                };*/
                //reporting-node-received-bundle
                if (assert0) {
                    bsr.SetTimeOfReceiptOfBundleAndStatusFlag(t0);
                }

                //reporting-node-accept-custody-of-bundle
                if (assert1) {
                    bsr.SetTimeOfCustodyAcceptanceOfBundleAndStatusFlag(t1);
                }

                //reporting-node-forwarded-bundle
                if (assert2) {
                    bsr.SetTimeOfForwardingOfBundleAndStatusFlag(t2);
                }

                //reporting-node-delivered-bundle
                if (assert3) {
                    bsr.SetTimeOfDeliveryOfBundleAndStatusFlag(t3);
                }

                //reporting-node-deleted-bundle
                if (assert4) {
                    bsr.SetTimeOfDeletionOfBundleAndStatusFlag(t4);
                }

                bsr.m_reasonCode = BPV6_BUNDLE_STATUS_REPORT_REASON_CODES::DEPLETED_STORAGE;

                bsr.m_bundleSourceEid = bundleSourceEidStr;

                bsr.m_copyOfBundleCreationTimestamp.secondsSinceStartOfYear2000 = 5000;
                bsr.m_copyOfBundleCreationTimestamp.sequenceNumber = 10;

                //must set both m_isFragment
                block.m_isFragment = useFrag;
                bsr.m_isFragment = useFrag;
                bsr.m_fragmentOffsetIfPresent = 2000;
                bsr.m_fragmentLengthIfPresent = 3000;

                BOOST_REQUIRE(lastBsr != bsr);
                lastBsr = bsr;
                BOOST_REQUIRE(lastBsr == bsr);

                bsrSerializationSize = bsr.GetSerializationSize();

                bv.AppendMoveCanonicalBlock(std::move(blockPtr));
            }


            BOOST_REQUIRE(bv.Render(5000));

            padded_vector_uint8_t bundleSerializedOriginal(bv.m_frontBuffer);
            //std::cout << "renderedsize: " << bv.m_frontBuffer.size() << "\n";

            BOOST_REQUIRE_GT(bundleSerializedOriginal.size(), 0);
            padded_vector_uint8_t bundleSerializedCopy(bundleSerializedOriginal); //the copy can get modified by bundle view on first load
            BOOST_REQUIRE(bundleSerializedOriginal == bundleSerializedCopy);
            BOOST_REQUIRE(!bv.m_recycledAdminRecord);
            bv.Reset();
            BOOST_REQUIRE(bv.m_recycledAdminRecord);
            //std::cout << "sz " << bundleSerializedCopy.size() << std::endl;
            BOOST_REQUIRE(bv.LoadBundle(&bundleSerializedCopy[0], bundleSerializedCopy.size()));
            BOOST_REQUIRE(!bv.m_recycledAdminRecord);
            BOOST_REQUIRE(bv.m_backBuffer != bundleSerializedCopy);
            BOOST_REQUIRE(bv.m_frontBuffer != bundleSerializedCopy);

            BOOST_REQUIRE_EQUAL(primary.m_sourceNodeId, cbhe_eid_t(PRIMARY_SRC_NODE, PRIMARY_SRC_SVC));
            BOOST_REQUIRE_EQUAL(primary.m_destinationEid, cbhe_eid_t(PRIMARY_DEST_NODE, PRIMARY_DEST_SVC));
            BOOST_REQUIRE_EQUAL(primary.m_creationTimestamp, TimestampUtil::bpv6_creation_timestamp_t(PRIMARY_TIME, PRIMARY_SEQ));
            BOOST_REQUIRE_EQUAL(primary.m_lifetimeSeconds, PRIMARY_LIFETIME);
            BOOST_REQUIRE_EQUAL(bv.m_primaryBlockView.actualSerializedPrimaryBlockPtr.size(), primary.GetSerializationSize());
            BOOST_REQUIRE_EQUAL(primary.m_bundleProcessingControlFlags, BPV6_BUNDLEFLAG::NOFRAGMENT | BPV6_BUNDLEFLAG::ADMINRECORD);

            BOOST_REQUIRE_EQUAL(bv.GetNumCanonicalBlocks(), 1);
            BOOST_REQUIRE_EQUAL(bv.GetCanonicalBlockCountByType(BPV6_BLOCK_TYPE_CODE::UNUSED_11), 0);
            BOOST_REQUIRE_EQUAL(bv.GetCanonicalBlockCountByType(BPV6_BLOCK_TYPE_CODE::PAYLOAD), 1);



            //get bundle status report payload block
            {
                std::vector<BundleViewV6::Bpv6CanonicalBlockView*> blocks;
                bv.GetCanonicalBlocksByType(BPV6_BLOCK_TYPE_CODE::PAYLOAD, blocks);
                BOOST_REQUIRE_EQUAL(blocks.size(), 1);
                Bpv6AdministrativeRecord* adminRecordBlockPtr = dynamic_cast<Bpv6AdministrativeRecord*>(blocks[0]->headerPtr.get());
                BOOST_REQUIRE(adminRecordBlockPtr);
                if (adminRecordBlockPtr) {
                    BOOST_REQUIRE_EQUAL(adminRecordBlockPtr->m_blockTypeCode, BPV6_BLOCK_TYPE_CODE::PAYLOAD);
                    BOOST_REQUIRE_EQUAL(adminRecordBlockPtr->m_adminRecordTypeCode, BPV6_ADMINISTRATIVE_RECORD_TYPE_CODE::BUNDLE_STATUS_REPORT);

                    BOOST_REQUIRE_EQUAL(blocks[0]->actualSerializedBlockPtr.size(), adminRecordBlockPtr->GetSerializationSize());

                    Bpv6AdministrativeRecordContentBundleStatusReport* bsrPtr = dynamic_cast<Bpv6AdministrativeRecordContentBundleStatusReport*>(adminRecordBlockPtr->m_adminRecordContentPtr.get());
                    BOOST_REQUIRE(bsrPtr);
                    if (bsrPtr) {
                        Bpv6AdministrativeRecordContentBundleStatusReport& bsr = *(reinterpret_cast<Bpv6AdministrativeRecordContentBundleStatusReport*>(bsrPtr));

                        BOOST_REQUIRE_EQUAL(bsr.GetSerializationSize(), bsrSerializationSize);

                        //reporting-node-received-bundle
                        if (assert0) {
                            BOOST_REQUIRE(bsr.HasBundleStatusReportStatusFlagSet(BPV6_BUNDLE_STATUS_REPORT_STATUS_FLAGS::REPORTING_NODE_RECEIVED_BUNDLE));
                            BOOST_REQUIRE_EQUAL(bsr.m_timeOfReceiptOfBundle, t0);
                        }
                        else {
                            BOOST_REQUIRE(!bsr.HasBundleStatusReportStatusFlagSet(BPV6_BUNDLE_STATUS_REPORT_STATUS_FLAGS::REPORTING_NODE_RECEIVED_BUNDLE));
                        }

                        //reporting-node-accept-custody-of-bundle
                        if (assert1) {
                            BOOST_REQUIRE(bsr.HasBundleStatusReportStatusFlagSet(BPV6_BUNDLE_STATUS_REPORT_STATUS_FLAGS::REPORTING_NODE_ACCEPTED_CUSTODY_OF_BUNDLE));
                            BOOST_REQUIRE_EQUAL(bsr.m_timeOfCustodyAcceptanceOfBundle, t1);
                        }
                        else {
                            BOOST_REQUIRE(!bsr.HasBundleStatusReportStatusFlagSet(BPV6_BUNDLE_STATUS_REPORT_STATUS_FLAGS::REPORTING_NODE_ACCEPTED_CUSTODY_OF_BUNDLE));
                        }

                        //reporting-node-forwarded-bundle
                        if (assert2) {
                            BOOST_REQUIRE(bsr.HasBundleStatusReportStatusFlagSet(BPV6_BUNDLE_STATUS_REPORT_STATUS_FLAGS::REPORTING_NODE_FORWARDED_BUNDLE));
                            BOOST_REQUIRE_EQUAL(bsr.m_timeOfForwardingOfBundle, t2);
                        }
                        else {
                            BOOST_REQUIRE(!bsr.HasBundleStatusReportStatusFlagSet(BPV6_BUNDLE_STATUS_REPORT_STATUS_FLAGS::REPORTING_NODE_FORWARDED_BUNDLE));
                        }

                        //reporting-node-delivered-bundle
                        if (assert3) {
                            BOOST_REQUIRE(bsr.HasBundleStatusReportStatusFlagSet(BPV6_BUNDLE_STATUS_REPORT_STATUS_FLAGS::REPORTING_NODE_DELIVERED_BUNDLE));
                            BOOST_REQUIRE_EQUAL(bsr.m_timeOfDeliveryOfBundle, t3);
                        }
                        else {
                            BOOST_REQUIRE(!bsr.HasBundleStatusReportStatusFlagSet(BPV6_BUNDLE_STATUS_REPORT_STATUS_FLAGS::REPORTING_NODE_DELIVERED_BUNDLE));
                        }

                        //reporting-node-deleted-bundle
                        if (assert4) {
                            BOOST_REQUIRE(bsr.HasBundleStatusReportStatusFlagSet(BPV6_BUNDLE_STATUS_REPORT_STATUS_FLAGS::REPORTING_NODE_DELETED_BUNDLE));
                            BOOST_REQUIRE_EQUAL(bsr.m_timeOfDeletionOfBundle, t4);
                        }
                        else {
                            BOOST_REQUIRE(!bsr.HasBundleStatusReportStatusFlagSet(BPV6_BUNDLE_STATUS_REPORT_STATUS_FLAGS::REPORTING_NODE_DELETED_BUNDLE));
                        }


                        BOOST_REQUIRE_EQUAL(bsr.m_reasonCode, BPV6_BUNDLE_STATUS_REPORT_REASON_CODES::DEPLETED_STORAGE);

                        BOOST_REQUIRE_EQUAL(bsr.m_bundleSourceEid, bundleSourceEidStr);

                        BOOST_REQUIRE_EQUAL(bsr.m_copyOfBundleCreationTimestamp.secondsSinceStartOfYear2000, 5000);
                        BOOST_REQUIRE_EQUAL(bsr.m_copyOfBundleCreationTimestamp.sequenceNumber, 10);

                        BOOST_REQUIRE_EQUAL(bsr.m_isFragment, useFrag);
                        if (bsr.m_isFragment) {
                            BOOST_REQUIRE_EQUAL(bsr.m_fragmentOffsetIfPresent, 2000);
                            BOOST_REQUIRE_EQUAL(bsr.m_fragmentLengthIfPresent, 3000);
                        }

                        BOOST_REQUIRE(lastBsr == bsr);

                        {
                            //misc
                            Bpv6AdministrativeRecordContentBundleStatusReport rpt = bsr;
                            Bpv6AdministrativeRecordContentBundleStatusReport rpt2 = bsr;
                            BOOST_REQUIRE(!(rpt != rpt2));
                            Bpv6AdministrativeRecordContentBundleStatusReport rptCopy = rpt;
                            Bpv6AdministrativeRecordContentBundleStatusReport rptCopy2(rpt);
                            Bpv6AdministrativeRecordContentBundleStatusReport rpt2Moved = std::move(rpt2);
                            BOOST_REQUIRE(rpt != rpt2); //rpt2 moved
                            BOOST_REQUIRE(rpt == rpt2Moved);
                            BOOST_REQUIRE(rpt == rptCopy);
                            BOOST_REQUIRE(rpt == rptCopy2);
                            Bpv6AdministrativeRecordContentBundleStatusReport rpt2Moved2(std::move(rpt2Moved));
                            BOOST_REQUIRE(rpt != rpt2Moved); //rpt2 moved
                            BOOST_REQUIRE(rpt == rpt2Moved2);
                        }
                    }
                }
            }
        }

    }


}

BOOST_AUTO_TEST_CASE(Bpv6PrimaryFragmentTestCase)
{
    Bpv6CbhePrimaryBlock primary;
    primary.SetZero();
    static const BPV6_BUNDLEFLAG BUNDLEFLAGS = BPV6_BUNDLEFLAG::PRIORITY_EXPEDITED | BPV6_BUNDLEFLAG::SINGLETON | BPV6_BUNDLEFLAG::ISFRAGMENT | BPV6_BUNDLEFLAG::CUSTODY_REQUESTED;
    primary.m_bundleProcessingControlFlags = BUNDLEFLAGS;
    //primary.m_blockLength = will be modified by SerializeBpv6;
    primary.m_sourceNodeId.Set(PRIMARY_SRC_NODE, PRIMARY_SRC_SVC);
    primary.m_destinationEid.Set(PRIMARY_DEST_NODE, PRIMARY_DEST_SVC);
    primary.m_custodianEid.Set(3333, 4444);
    primary.m_reportToEid.Set(5555, 6666);
    primary.m_creationTimestamp.Set(PRIMARY_TIME, PRIMARY_SEQ); //(uint64_t)bpv6_unix_to_5050(curr_time);
    primary.m_lifetimeSeconds = PRIMARY_LIFETIME;

    primary.m_fragmentOffset = 10000;
    primary.m_totalApplicationDataUnitLength = 10001;

    std::vector<uint8_t> serialization(1000);
    const uint64_t serializationSize = primary.SerializeBpv6(serialization.data());
    Bpv6CbhePrimaryBlock primary2;
    primary2.SetZero();
    uint64_t numBytesTakenToDecode = 0;
    primary2.DeserializeBpv6(serialization.data(), numBytesTakenToDecode, serialization.size()); //using buffer size 1000 to ensure hardware operations
    BOOST_REQUIRE_EQUAL(numBytesTakenToDecode, serializationSize);
    BOOST_REQUIRE_EQUAL(numBytesTakenToDecode, primary2.GetSerializationSize());

    BOOST_REQUIRE_EQUAL(primary2.m_bundleProcessingControlFlags, BUNDLEFLAGS);
    BOOST_REQUIRE_EQUAL(primary2.m_blockLength, numBytesTakenToDecode - 4); //4 because 1-byte version + flags is 2 byte sdnv + block length must be 1 byte sdnv
    BOOST_REQUIRE_EQUAL(primary2.m_sourceNodeId, cbhe_eid_t(PRIMARY_SRC_NODE, PRIMARY_SRC_SVC));
    BOOST_REQUIRE_EQUAL(primary2.m_destinationEid, cbhe_eid_t(PRIMARY_DEST_NODE, PRIMARY_DEST_SVC));
    BOOST_REQUIRE_EQUAL(primary2.m_custodianEid, cbhe_eid_t(3333, 4444));
    BOOST_REQUIRE_EQUAL(primary2.m_reportToEid, cbhe_eid_t(5555, 6666));
    BOOST_REQUIRE_EQUAL(primary2.m_creationTimestamp, TimestampUtil::bpv6_creation_timestamp_t(PRIMARY_TIME, PRIMARY_SEQ));
    BOOST_REQUIRE_EQUAL(primary2.m_lifetimeSeconds, PRIMARY_LIFETIME);
    BOOST_REQUIRE_EQUAL(primary2.m_fragmentOffset, 10000);
    BOOST_REQUIRE_EQUAL(primary2.m_totalApplicationDataUnitLength, 10001);
    BOOST_REQUIRE(primary == primary2);
}

BOOST_AUTO_TEST_CASE(BundleViewSecondsSinceCreateTestCase) {
    const boost::posix_time::ptime nowTime = boost::posix_time::microsec_clock::universal_time();
    const boost::posix_time::ptime bundleCreateTime = nowTime - boost::posix_time::seconds(50);

    Bpv6CbhePrimaryBlock primary;
    primary.m_creationTimestamp.SetFromPtime(bundleCreateTime);
    uint64_t s = primary.GetSecondsSinceCreate();
    // Provide some buffer
    BOOST_REQUIRE(s >= 50 && s <= 51);
}

BOOST_AUTO_TEST_CASE(BundleViewV6ReadDtnMeRawDataTestCase)
{
    { //an rfc5050 admin record with previous hop and an unspecified block type 19
        static const std::string hexAsString = "0681121882814900828000000000000082dce9d45084ad1d85a3000005100c69706e0033323736382e300013010208ff010819200382dce9d4500082dce9d26c82790969706e3a31312e3634";
        padded_vector_uint8_t bundleRawData;
        BOOST_REQUIRE(BinaryConversions::HexStringToBytes(hexAsString, bundleRawData));
        
        padded_vector_uint8_t bundleRawDataCopy(bundleRawData);
        BundleViewV6 bv;
        BOOST_REQUIRE(bv.SwapInAndLoadBundle(bundleRawDataCopy));
        
        { //get previous hop insertion
            std::vector<BundleViewV6::Bpv6CanonicalBlockView*> blocks;
            bv.GetCanonicalBlocksByType(BPV6_BLOCK_TYPE_CODE::PREVIOUS_HOP_INSERTION, blocks);
            BOOST_REQUIRE_EQUAL(blocks.size(), 1);
            Bpv6PreviousHopInsertionCanonicalBlock* phibPtr = dynamic_cast<Bpv6PreviousHopInsertionCanonicalBlock*>(blocks[0]->headerPtr.get());
            BOOST_REQUIRE(phibPtr);
            if (phibPtr) {
                BOOST_REQUIRE_EQUAL(phibPtr->m_blockTypeCode, BPV6_BLOCK_TYPE_CODE::PREVIOUS_HOP_INSERTION);
                BOOST_REQUIRE(!blocks[0]->HasBlockProcessingControlFlagSet(BPV6_BLOCKFLAG::IS_LAST_BLOCK));
                BOOST_REQUIRE_EQUAL(phibPtr->m_previousNode, cbhe_eid_t(32768, 0));
                BOOST_REQUIRE_EQUAL(blocks[0]->actualSerializedBlockPtr.size(), phibPtr->GetSerializationSize());
            }
        }
        { //get block 19
            std::vector<BundleViewV6::Bpv6CanonicalBlockView*> blocks;
            bv.GetCanonicalBlocksByType(static_cast<BPV6_BLOCK_TYPE_CODE>(19), blocks);
            BOOST_REQUIRE_EQUAL(blocks.size(), 1);
        }
        { //get admin record (payload block)
            std::vector<BundleViewV6::Bpv6CanonicalBlockView*> blocks;
            bv.GetCanonicalBlocksByType(BPV6_BLOCK_TYPE_CODE::PAYLOAD, blocks);
            BOOST_REQUIRE_EQUAL(blocks.size(), 1);
            Bpv6AdministrativeRecord* adminRecordBlockPtr = dynamic_cast<Bpv6AdministrativeRecord*>(blocks[0]->headerPtr.get());
            BOOST_REQUIRE(adminRecordBlockPtr);
            if (adminRecordBlockPtr) {
                const BPV6_ADMINISTRATIVE_RECORD_TYPE_CODE adminRecordType = adminRecordBlockPtr->m_adminRecordTypeCode;
                BOOST_REQUIRE_EQUAL(adminRecordType, BPV6_ADMINISTRATIVE_RECORD_TYPE_CODE::CUSTODY_SIGNAL); //rfc5050 style custody transfer
                Bpv6AdministrativeRecordContentCustodySignal* csPtr = dynamic_cast<Bpv6AdministrativeRecordContentCustodySignal*>(adminRecordBlockPtr->m_adminRecordContentPtr.get());
                BOOST_REQUIRE(csPtr);
                if (csPtr) {
                    Bpv6AdministrativeRecordContentCustodySignal& cs = *(reinterpret_cast<Bpv6AdministrativeRecordContentCustodySignal*>(csPtr));
                    BOOST_REQUIRE(!cs.DidCustodyTransferSucceed());
                    BOOST_REQUIRE_EQUAL(cs.GetReasonCode(), BPV6_CUSTODY_SIGNAL_REASON_CODES_7BIT::REDUNDANT_RECEPTION);
                    BOOST_REQUIRE(!cs.m_isFragment);
                    BOOST_REQUIRE_EQUAL(cs.m_bundleSourceEid, "ipn:11.64");
                }
            }
        }

        bv.m_primaryBlockView.SetManuallyModified();
        bv.Render(bundleRawData.size() + 50);
        BOOST_REQUIRE(bv.m_frontBuffer == bundleRawData);
        BOOST_REQUIRE(bv.m_backBuffer == bundleRawData);
    }

    { //an acs bundle
        static const std::string hexAsString = "0681121882814900828000000000000082dce9e74688aa2085a3000005100c69706e0033323736382e300013010208ff01080b4080000101817f82018116";
        padded_vector_uint8_t bundleRawData;
        BOOST_REQUIRE(BinaryConversions::HexStringToBytes(hexAsString, bundleRawData));

        padded_vector_uint8_t bundleRawDataCopy(bundleRawData);
        BundleViewV6 bv;
        BOOST_REQUIRE(bv.SwapInAndLoadBundle(bundleRawDataCopy));

        { //get previous hop insertion
            std::vector<BundleViewV6::Bpv6CanonicalBlockView*> blocks;
            bv.GetCanonicalBlocksByType(BPV6_BLOCK_TYPE_CODE::PREVIOUS_HOP_INSERTION, blocks);
            BOOST_REQUIRE_EQUAL(blocks.size(), 1);
            Bpv6PreviousHopInsertionCanonicalBlock* phibPtr = dynamic_cast<Bpv6PreviousHopInsertionCanonicalBlock*>(blocks[0]->headerPtr.get());
            BOOST_REQUIRE(phibPtr);
            if (phibPtr) {
                BOOST_REQUIRE_EQUAL(phibPtr->m_blockTypeCode, BPV6_BLOCK_TYPE_CODE::PREVIOUS_HOP_INSERTION);
                BOOST_REQUIRE(!blocks[0]->HasBlockProcessingControlFlagSet(BPV6_BLOCKFLAG::IS_LAST_BLOCK));
                BOOST_REQUIRE_EQUAL(phibPtr->m_previousNode, cbhe_eid_t(32768, 0));
                BOOST_REQUIRE_EQUAL(blocks[0]->actualSerializedBlockPtr.size(), phibPtr->GetSerializationSize());
            }
        }
        { //get block 19
            std::vector<BundleViewV6::Bpv6CanonicalBlockView*> blocks;
            bv.GetCanonicalBlocksByType(static_cast<BPV6_BLOCK_TYPE_CODE>(19), blocks);
            BOOST_REQUIRE_EQUAL(blocks.size(), 1);
        }
        { //get admin record (payload block)
            std::vector<BundleViewV6::Bpv6CanonicalBlockView*> blocks;
            bv.GetCanonicalBlocksByType(BPV6_BLOCK_TYPE_CODE::PAYLOAD, blocks);
            BOOST_REQUIRE_EQUAL(blocks.size(), 1);
            Bpv6AdministrativeRecord* adminRecordBlockPtr = dynamic_cast<Bpv6AdministrativeRecord*>(blocks[0]->headerPtr.get());
            BOOST_REQUIRE(adminRecordBlockPtr);
            if (adminRecordBlockPtr) {
                const BPV6_ADMINISTRATIVE_RECORD_TYPE_CODE adminRecordType = adminRecordBlockPtr->m_adminRecordTypeCode;
                BOOST_REQUIRE_EQUAL(adminRecordType, BPV6_ADMINISTRATIVE_RECORD_TYPE_CODE::AGGREGATE_CUSTODY_SIGNAL);
                Bpv6AdministrativeRecordContentAggregateCustodySignal* acsPtr = dynamic_cast<Bpv6AdministrativeRecordContentAggregateCustodySignal*>(adminRecordBlockPtr->m_adminRecordContentPtr.get());
                BOOST_REQUIRE(acsPtr);
                if (acsPtr) {
                    Bpv6AdministrativeRecordContentAggregateCustodySignal& acs = *(reinterpret_cast<Bpv6AdministrativeRecordContentAggregateCustodySignal*>(acsPtr));
                    BOOST_REQUIRE(acs.DidCustodyTransferSucceed());
                    //wireshark says:
                    // start0 end0
                    //start1 end256
                    //start257 end663
                    FragmentSet::data_fragment_set_t expected = { FragmentSet::data_fragment_t(0, 255), FragmentSet::data_fragment_t(512, 661) };
                    BOOST_REQUIRE(acs.m_custodyIdFills == expected);
                    uint64_t numTransfers = 0;
                    for (FragmentSet::data_fragment_set_t::const_iterator it = acs.m_custodyIdFills.cbegin(); it != acs.m_custodyIdFills.cend(); ++it) {
                        //std::cout << "b " << it->beginIndex << " e " << it->endIndex << "\n";
                        numTransfers += (it->endIndex + 1) - it->beginIndex;
                    }
                    BOOST_REQUIRE_EQUAL(numTransfers, 256 + 150);
                }
            }
        }

        bv.m_primaryBlockView.SetManuallyModified();
        bv.Render(bundleRawData.size() + 50);
        BOOST_REQUIRE(bv.m_frontBuffer == bundleRawData);
        BOOST_REQUIRE(bv.m_backBuffer == bundleRawData);
    }
}

BOOST_AUTO_TEST_CASE(BundleView6SourceEidTestCase) {
    Bpv6CbhePrimaryBlock primary;
    primary.m_sourceNodeId.nodeId = 1;
    primary.m_sourceNodeId.serviceId = 1;
    cbhe_eid_t sourceId = primary.GetSourceEid();
    BOOST_REQUIRE_EQUAL(sourceId.nodeId, 1);
    BOOST_REQUIRE_EQUAL(sourceId.serviceId, 1);
}