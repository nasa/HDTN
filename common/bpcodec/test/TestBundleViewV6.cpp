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

static void AppendCanonicalBlockAndRender(BundleViewV6 & bv, uint8_t newType, const std::string & newBlockBody) {

    //std::cout << "append " << (int)newType << "\n";
    bpv6_canonical_block block;
    block.type = newType;
    block.flags = 0; //don't worry about block.flags as last block because Render will take care of that automatically
    block.length = newBlockBody.length();
    std::vector<uint8_t> blockBodyAsVecUint8(newBlockBody.data(), newBlockBody.data() + newBlockBody.size());
    bv.AppendCanonicalBlock(block, blockBodyAsVecUint8);
    BOOST_REQUIRE(bv.Render(5000));
    
}

static void ChangeCanonicalBlockAndRender(BundleViewV6 & bv, uint8_t oldType, uint8_t newType, const std::string & newBlockBody) {
    
    //std::cout << "change " << (int)oldType << " to " << (int)newType << "\n";
    std::vector<BundleViewV6::Bpv6CanonicalBlockView*> blocks;
    bv.GetCanonicalBlocksByType(oldType, blocks);
    BOOST_REQUIRE_EQUAL(blocks.size(), 1);
    bpv6_canonical_block & block = blocks[0]->header;
    block.type = newType;
    //don't worry about block.flags as last block because Render will take care of that automatically
    block.length = newBlockBody.length();
    blocks[0]->replacementBlockBodyData = std::vector<uint8_t>(newBlockBody.data(), newBlockBody.data() + newBlockBody.size());
    blocks[0]->SetManuallyModified();

    BOOST_REQUIRE(bv.Render(5000));
    
}

static uint64_t GenerateBundle(const std::vector<uint8_t> & canonicalTypesVec, const std::vector<std::string> & canonicalBodyStringsVec, uint8_t * buffer) {
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
    primary.custodian_node = 0;
    primary.custodian_svc = 0;
    primary.creation = PRIMARY_TIME; //(uint64_t)bpv6_unix_to_5050(curr_time);
    primary.lifetime = PRIMARY_LIFETIME;
    primary.sequence = PRIMARY_SEQ;
    uint64_t retVal;
    retVal = primary.cbhe_bpv6_primary_block_encode((char *)buffer, 0, BP_MSG_BUFSZ);
    if (retVal == 0) {
        return 0;
    }
    buffer += retVal;

    for (std::size_t i = 0; i < canonicalTypesVec.size(); ++i) {
        block.type = canonicalTypesVec[i];
        block.flags = (i == (canonicalTypesVec.size() - 1)) ? BPV6_BLOCKFLAG_LAST_BLOCK : 0;
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
        const std::vector<uint8_t> canonicalTypesVec = { 1,3,2,5 };
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

        bpv6_primary_block & primary = bv.m_primaryBlockView.header;
        BOOST_REQUIRE_EQUAL(primary.src_node, PRIMARY_SRC_NODE);
        BOOST_REQUIRE_EQUAL(primary.src_svc, PRIMARY_SRC_SVC);
        BOOST_REQUIRE_EQUAL(primary.dst_node, PRIMARY_DEST_NODE);
        BOOST_REQUIRE_EQUAL(primary.dst_svc, PRIMARY_DEST_SVC);
        BOOST_REQUIRE_EQUAL(primary.creation, PRIMARY_TIME);
        BOOST_REQUIRE_EQUAL(primary.lifetime, PRIMARY_LIFETIME);
        BOOST_REQUIRE_EQUAL(primary.sequence, PRIMARY_SEQ);

        BOOST_REQUIRE_EQUAL(bv.GetNumCanonicalBlocks(), canonicalTypesVec.size());
        BOOST_REQUIRE_EQUAL(bv.GetCanonicalBlockCountByType(10), 0);
        for (std::size_t i = 0; i < canonicalTypesVec.size(); ++i) {
            BOOST_REQUIRE_EQUAL(bv.GetCanonicalBlockCountByType(canonicalTypesVec[i]), 1);
            std::vector<BundleViewV6::Bpv6CanonicalBlockView*> blocks;
            bv.GetCanonicalBlocksByType(canonicalTypesVec[i], blocks);
            BOOST_REQUIRE_EQUAL(blocks.size(), 1);
            const char * strPtr = (const char *)blocks[0]->actualSerializedBodyPtr.data();
            std::string s(strPtr, strPtr + blocks[0]->actualSerializedBodyPtr.size());
            BOOST_REQUIRE_EQUAL(s, canonicalBodyStringsVec[i]);
            const uint8_t * typePtr = (const uint8_t *)blocks[0]->actualSerializedHeaderAndBodyPtr.data();
            BOOST_REQUIRE_EQUAL(*typePtr, canonicalTypesVec[i]);
        }

        BOOST_REQUIRE(bv.Render(5000));
        BOOST_REQUIRE(bv.m_backBuffer != bundleSerializedCopy);
        BOOST_REQUIRE_EQUAL(bv.m_frontBuffer.size(), bundleSerializedCopy.size());
        BOOST_REQUIRE(bv.m_frontBuffer == bundleSerializedCopy);

        //change 2nd block from quick to slow and type from 3 to 6 and render
        ChangeCanonicalBlockAndRender(bv, 3, 6, "slow ");
        BOOST_REQUIRE_EQUAL(bv.m_frontBuffer.size(), bv.m_backBuffer.size() - 1); //"quick" to "slow"
        BOOST_REQUIRE_EQUAL(bv.m_frontBuffer.size(), bv.m_renderedBundle.size());
        BOOST_REQUIRE_EQUAL(bv.GetNumCanonicalBlocks(), canonicalTypesVec.size());

        //Render again
        //std::cout << "render again\n";
        BOOST_REQUIRE(bv.Render(5000));
        BOOST_REQUIRE(bv.m_frontBuffer == bv.m_backBuffer);
        
        //revert 2nd block
        ChangeCanonicalBlockAndRender(bv, 6, 3, "quick ");
        BOOST_REQUIRE_EQUAL(bv.m_frontBuffer.size(), bundleSerializedOriginal.size());
        BOOST_REQUIRE_EQUAL(bv.m_frontBuffer.size(), bv.m_renderedBundle.size());
        BOOST_REQUIRE(bv.m_frontBuffer == bundleSerializedOriginal);

        //change type 2 flags
        {
            std::vector<BundleViewV6::Bpv6CanonicalBlockView*> blocks;
            bv.GetCanonicalBlocksByType(2, blocks);
            BOOST_REQUIRE_EQUAL(blocks.size(), 1);
            BOOST_REQUIRE(!blocks[0]->dirty);
            BOOST_REQUIRE(!blocks[0]->HasBlockProcessingControlFlagSet(BPV6_BLOCKFLAG_DISCARD_BLOCK_FAILURE));
            blocks[0]->SetBlockProcessingControlFlagAndDirtyIfNecessary(BPV6_BLOCKFLAG_DISCARD_BLOCK_FAILURE);
            BOOST_REQUIRE(!blocks[0]->dirty); //no render required
            BOOST_REQUIRE(blocks[0]->HasBlockProcessingControlFlagSet(BPV6_BLOCKFLAG_DISCARD_BLOCK_FAILURE));
            BOOST_REQUIRE_EQUAL(bv.m_frontBuffer.size(), bv.m_renderedBundle.size()); //currently rendering to front buffer
            BOOST_REQUIRE(bv.m_frontBuffer != bundleSerializedOriginal); //differ by flag only

            blocks[0]->ClearBlockProcessingControlFlagAndDirtyIfNecessary(BPV6_BLOCKFLAG_DISCARD_BLOCK_FAILURE);
            BOOST_REQUIRE(!blocks[0]->dirty); //no render required
            BOOST_REQUIRE(!blocks[0]->HasBlockProcessingControlFlagSet(BPV6_BLOCKFLAG_DISCARD_BLOCK_FAILURE));
            BOOST_REQUIRE_EQUAL(bv.m_frontBuffer.size(), bv.m_renderedBundle.size()); //currently rendering to front buffer
            BOOST_REQUIRE(bv.m_frontBuffer == bundleSerializedOriginal); //back to equal


            //add big flag > 127 requiring rerender
            static const uint64_t bigFlag = 1 << 26;
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
            BOOST_REQUIRE_EQUAL(*typePtr, canonicalTypesVec[2]);
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
            bv.m_primaryBlockView.header.sequence = 65539;
            bv.m_primaryBlockView.SetManuallyModified();
            BOOST_REQUIRE(bv.m_primaryBlockView.dirty);
            //std::cout << "render increase primary seq\n";
            BOOST_REQUIRE(bv.Render(5000));
            BOOST_REQUIRE_EQUAL(bv.m_frontBuffer.size(), bundleSerializedOriginal.size() + 2);
            BOOST_REQUIRE(!bv.m_primaryBlockView.dirty); //render removed dirty
            BOOST_REQUIRE_EQUAL(primary.lifetime, PRIMARY_LIFETIME);
            BOOST_REQUIRE_EQUAL(primary.sequence, 65539);

            //restore PRIMARY_SEQ
            bv.m_primaryBlockView.header.sequence = PRIMARY_SEQ;
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

