#include <boost/test/unit_test.hpp>
#include <boost/thread.hpp>
#include "codec/CustodyTransferEnhancementBlock.h"
#include "codec/BundleViewV6.h"
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

static void AppendCanonicalBlockAndRender(BundleViewV6 & bv, BPV6_BLOCK_TYPE_CODE newType, const std::string & newBlockBody) {

    //std::cout << "append " << (int)newType << "\n";
    bpv6_canonical_block block;
    block.m_blockTypeCode = newType;
    block.m_blockProcessingControlFlags = BPV6_BLOCKFLAG::NO_FLAGS_SET; //don't worry about block.flags as last block because Render will take care of that automatically
    block.length = newBlockBody.length();
    std::vector<uint8_t> blockBodyAsVecUint8(newBlockBody.data(), newBlockBody.data() + newBlockBody.size());
    bv.AppendCanonicalBlock(block, blockBodyAsVecUint8);
    BOOST_REQUIRE(bv.Render(5000));
    
}

static void ChangeCanonicalBlockAndRender(BundleViewV6 & bv, BPV6_BLOCK_TYPE_CODE oldType, BPV6_BLOCK_TYPE_CODE newType, const std::string & newBlockBody) {
    
    //std::cout << "change " << (int)oldType << " to " << (int)newType << "\n";
    std::vector<BundleViewV6::Bpv6CanonicalBlockView*> blocks;
    bv.GetCanonicalBlocksByType(oldType, blocks);
    BOOST_REQUIRE_EQUAL(blocks.size(), 1);
    bpv6_canonical_block & block = blocks[0]->header;
    block.m_blockTypeCode = newType;
    //don't worry about block.flags as last block because Render will take care of that automatically
    block.length = newBlockBody.length();
    blocks[0]->replacementBlockBodyData = std::vector<uint8_t>(newBlockBody.data(), newBlockBody.data() + newBlockBody.size());
    blocks[0]->SetManuallyModified();

    BOOST_REQUIRE(bv.Render(5000));
    
}

static uint64_t GenerateBundle(const std::vector<BPV6_BLOCK_TYPE_CODE> & canonicalTypesVec, const std::vector<std::string> & canonicalBodyStringsVec, uint8_t * buffer) {
    uint8_t * const serializationBase = buffer;

    Bpv6CbhePrimaryBlock primary;
    primary.SetZero();
    bpv6_canonical_block block;
    block.SetZero();

    primary.m_bundleProcessingControlFlags = BPV6_BUNDLEFLAG::PRIORITY_EXPEDITED | BPV6_BUNDLEFLAG::SINGLETON | BPV6_BUNDLEFLAG::NOFRAGMENT | BPV6_BUNDLEFLAG::CUSTODY_REQUESTED;
    primary.m_sourceNodeId.Set(PRIMARY_SRC_NODE, PRIMARY_SRC_SVC);
    primary.m_destinationEid.Set(PRIMARY_DEST_NODE, PRIMARY_DEST_SVC);
    primary.m_custodianEid.SetZero();
    primary.m_creationTimestamp.secondsSinceStartOfYear2000 = PRIMARY_TIME; //(uint64_t)bpv6_unix_to_5050(curr_time);
    primary.m_lifetimeSeconds = PRIMARY_LIFETIME;
    primary.m_creationTimestamp.sequenceNumber = PRIMARY_SEQ;
    uint64_t retVal;
    retVal = primary.SerializeBpv6(buffer);
    if (retVal == 0) {
        return 0;
    }
    buffer += retVal;

    for (std::size_t i = 0; i < canonicalTypesVec.size(); ++i) {
        block.m_blockTypeCode = canonicalTypesVec[i];
        block.m_blockProcessingControlFlags = (i == (canonicalTypesVec.size() - 1)) ? BPV6_BLOCKFLAG::IS_LAST_BLOCK : BPV6_BLOCKFLAG::NO_FLAGS_SET;
        const std::string & blockBody = canonicalBodyStringsVec[i];
        block.length = blockBody.length();


        retVal = block.bpv6_canonical_block_encode((char *)buffer, 0, BP_MSG_BUFSZ);
        if (retVal == 0) {
            return 0;
        }
        buffer += retVal;

        memcpy(buffer, blockBody.data(), blockBody.length());
        buffer += blockBody.length();
    }

    return buffer - serializationBase;
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
        std::vector<uint8_t> bundleSerializedOriginal(2000);
        uint64_t len = GenerateBundle(canonicalTypesVec, canonicalBodyStringsVec, &bundleSerializedOriginal[0]);


        BOOST_REQUIRE_GT(len, 0);
        bundleSerializedOriginal.resize(len);
        std::vector<uint8_t> bundleSerializedCopy(bundleSerializedOriginal); //the copy can get modified by bundle view on first load
        BundleViewV6 bv;
        //std::cout << "sz " << bundleSerializedCopy.size() << std::endl;
        BOOST_REQUIRE(bv.LoadBundle(&bundleSerializedCopy[0], bundleSerializedCopy.size()));
        BOOST_REQUIRE(bv.m_backBuffer != bundleSerializedCopy);
        BOOST_REQUIRE(bv.m_frontBuffer != bundleSerializedCopy);

        Bpv6CbhePrimaryBlock & primary = bv.m_primaryBlockView.header;
        BOOST_REQUIRE_EQUAL(primary.m_sourceNodeId, cbhe_eid_t(PRIMARY_SRC_NODE, PRIMARY_SRC_SVC));
        BOOST_REQUIRE_EQUAL(primary.m_destinationEid, cbhe_eid_t(PRIMARY_DEST_NODE, PRIMARY_DEST_SVC));
        BOOST_REQUIRE_EQUAL(primary.m_creationTimestamp.secondsSinceStartOfYear2000, PRIMARY_TIME);
        BOOST_REQUIRE_EQUAL(primary.m_lifetimeSeconds, PRIMARY_LIFETIME);
        BOOST_REQUIRE_EQUAL(primary.m_creationTimestamp.sequenceNumber, PRIMARY_SEQ);

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
            const char * strPtr = (const char *)blocks[0]->actualSerializedBodyPtr.data();
            std::string s(strPtr, strPtr + blocks[0]->actualSerializedBodyPtr.size());
            BOOST_REQUIRE_EQUAL(s, canonicalBodyStringsVec[i]);
            const uint8_t * typePtr = (const uint8_t *)blocks[0]->actualSerializedHeaderAndBodyPtr.data();
            BOOST_REQUIRE_EQUAL(static_cast<BPV6_BLOCK_TYPE_CODE>(*typePtr), canonicalTypesVec[i]);
        }

        BOOST_REQUIRE(bv.Render(5000));
        BOOST_REQUIRE(bv.m_backBuffer != bundleSerializedCopy);
        BOOST_REQUIRE_EQUAL(bv.m_frontBuffer.size(), bundleSerializedCopy.size());
        BOOST_REQUIRE(bv.m_frontBuffer == bundleSerializedCopy);

        //change 2nd block from quick to slow and type from 7 to 12 and render
        ChangeCanonicalBlockAndRender(bv, BPV6_BLOCK_TYPE_CODE::UNUSED_7, BPV6_BLOCK_TYPE_CODE::UNUSED_12, "slow ");
        BOOST_REQUIRE_EQUAL(bv.m_frontBuffer.size(), bv.m_backBuffer.size() - 1); //"quick" to "slow"
        BOOST_REQUIRE_EQUAL(bv.m_frontBuffer.size(), bv.m_renderedBundle.size());
        BOOST_REQUIRE_EQUAL(bv.GetNumCanonicalBlocks(), canonicalTypesVec.size());

        //Render again
        //std::cout << "render again\n";
        BOOST_REQUIRE(bv.Render(5000));
        BOOST_REQUIRE(bv.m_frontBuffer == bv.m_backBuffer);
        
        //revert 2nd block
        ChangeCanonicalBlockAndRender(bv, BPV6_BLOCK_TYPE_CODE::UNUSED_12, BPV6_BLOCK_TYPE_CODE::UNUSED_7, "quick ");
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
            const char * strPtr = (const char *)blocks[0]->actualSerializedBodyPtr.data();
            std::string s(strPtr, strPtr + blocks[0]->actualSerializedBodyPtr.size());
            BOOST_REQUIRE_EQUAL(s, canonicalBodyStringsVec[2]);
            const uint8_t * typePtr = (const uint8_t *)blocks[0]->actualSerializedHeaderAndBodyPtr.data();
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

        //test loads.
        {
            BOOST_REQUIRE(bundleSerializedCopy == bundleSerializedOriginal); //back to equal
            BOOST_REQUIRE(bv.CopyAndLoadBundle(&bundleSerializedCopy[0], bundleSerializedCopy.size())); //calls reset
            BOOST_REQUIRE(bv.m_frontBuffer == bundleSerializedCopy);
            BOOST_REQUIRE(bv.SwapInAndLoadBundle(bundleSerializedCopy)); //calls reset
            BOOST_REQUIRE(bv.m_frontBuffer != bundleSerializedCopy);
            BOOST_REQUIRE(bv.m_frontBuffer == bundleSerializedOriginal);
        }
    }



}

