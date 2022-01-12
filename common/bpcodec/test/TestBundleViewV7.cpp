#include <boost/test/unit_test.hpp>
#include <boost/thread.hpp>
#include "codec/BundleViewV7.h"
#include <iostream>
#include <string>
#include <inttypes.h>
#include <vector>
#include "Uri.h"
#include <boost/next_prior.hpp>

static const uint64_t PRIMARY_SRC_NODE = 100;
static const uint64_t PRIMARY_SRC_SVC = 1;
static const uint64_t PRIMARY_DEST_NODE = 200;
static const uint64_t PRIMARY_DEST_SVC = 2;
static const uint64_t PRIMARY_TIME = 10000;
static const uint64_t PRIMARY_LIFETIME = 2000;
static const uint64_t PRIMARY_SEQ = 1;

static void AppendCanonicalBlockAndRender(BundleViewV7 & bv, uint8_t newType, std::string & newBlockBody, uint64_t blockNumber) {
    //std::cout << "append " << (int)newType << "\n";
    Bpv7CanonicalBlock block;
    block.m_blockTypeCode = newType;
    block.m_blockProcessingControlFlags = BPV7_BLOCKFLAG_REMOVE_BLOCK_IF_IT_CANT_BE_PROCESSED; //something for checking against
    block.m_dataLength = newBlockBody.size();
    block.m_dataPtr = (uint8_t*)newBlockBody.data(); //blockBodyAsVecUint8 must remain in scope until after render
    block.m_crcType = BPV7_CRC_TYPE_CRC32C;
    block.m_blockNumber = blockNumber;
    bv.AppendCanonicalBlock(block);
    BOOST_REQUIRE(bv.Render(5000));
}
static void PrependCanonicalBlockAndRender(BundleViewV7 & bv, uint8_t newType, std::string & newBlockBody, uint64_t blockNumber) {
    //std::cout << "append " << (int)newType << "\n";
    Bpv7CanonicalBlock block;
    block.m_blockTypeCode = newType;
    block.m_blockProcessingControlFlags = BPV7_BLOCKFLAG_REMOVE_BLOCK_IF_IT_CANT_BE_PROCESSED; //something for checking against
    block.m_dataLength = newBlockBody.size();
    block.m_dataPtr = (uint8_t*)newBlockBody.data(); //blockBodyAsVecUint8 must remain in scope until after render
    block.m_crcType = BPV7_CRC_TYPE_CRC32C;
    block.m_blockNumber = blockNumber;
    bv.PrependCanonicalBlock(block);
    BOOST_REQUIRE(bv.Render(5000));
}
static void PrependCanonicalBlockAndRender_AllocateOnly(BundleViewV7 & bv, uint8_t newType, uint64_t dataLengthToAllocate, uint64_t blockNumber) {
    //std::cout << "append " << (int)newType << "\n";
    Bpv7CanonicalBlock block;
    block.m_blockTypeCode = newType;
    block.m_blockProcessingControlFlags = BPV7_BLOCKFLAG_REMOVE_BLOCK_IF_IT_CANT_BE_PROCESSED; //something for checking against
    block.m_dataLength = dataLengthToAllocate;
    block.m_dataPtr = NULL;
    block.m_crcType = BPV7_CRC_TYPE_CRC32C;
    block.m_blockNumber = blockNumber;
    bv.PrependCanonicalBlock(block);
    BOOST_REQUIRE(bv.Render(5000));
}

static void ChangeCanonicalBlockAndRender(BundleViewV7 & bv, uint8_t oldType, uint8_t newType, std::string & newBlockBody) {
    
    //std::cout << "change " << (int)oldType << " to " << (int)newType << "\n";
    std::vector<BundleViewV7::Bpv7CanonicalBlockView*> blocks;
    bv.GetCanonicalBlocksByType(oldType, blocks);
    BOOST_REQUIRE_EQUAL(blocks.size(), 1);
    Bpv7CanonicalBlock & block = blocks[0]->header;
    block.m_blockTypeCode = newType;
    block.m_dataLength = newBlockBody.size();
    block.m_dataPtr = (uint8_t*)newBlockBody.data(); //blockBodyAsVecUint8 must remain in scope until after render
    blocks[0]->SetManuallyModified();

    BOOST_REQUIRE(bv.Render(5000));
    
}

static void GenerateBundle(const std::vector<uint8_t> & canonicalTypesVec, const std::vector<std::string> & canonicalBodyStringsVec, BundleViewV7 & bv) {

    Bpv7CbhePrimaryBlock & primary = bv.m_primaryBlockView.header;
    primary.SetZero();
    

    primary.m_bundleProcessingControlFlags = BPV7_BUNDLEFLAG_NOFRAGMENT;  //All BP endpoints identified by ipn-scheme endpoint IDs are singleton endpoints.
    primary.m_sourceNodeId.Set(PRIMARY_SRC_NODE, PRIMARY_SRC_SVC);
    primary.m_destinationEid.Set(PRIMARY_DEST_NODE, PRIMARY_DEST_SVC);
    primary.m_reportToEid.Set(0, 0);
    primary.m_creationTimestamp.millisecondsSinceStartOfYear2000 = PRIMARY_TIME;
    primary.m_lifetimeMilliseconds = PRIMARY_LIFETIME;
    primary.m_creationTimestamp.sequenceNumber = PRIMARY_SEQ;
    primary.m_crcType = BPV7_CRC_TYPE_CRC32C;
    bv.m_primaryBlockView.SetManuallyModified();
    
    for (std::size_t i = 0; i < canonicalTypesVec.size(); ++i) {
        Bpv7CanonicalBlock block;
        block.SetZero();

        block.m_blockTypeCode = canonicalTypesVec[i];
        block.m_blockProcessingControlFlags = BPV7_BLOCKFLAG_REMOVE_BLOCK_IF_IT_CANT_BE_PROCESSED; //something for checking against
        block.m_blockNumber = i;
        block.m_crcType = BPV7_CRC_TYPE_CRC32C;
        const std::string & blockBody = canonicalBodyStringsVec[i];
        block.m_dataLength = blockBody.size();
        block.m_dataPtr = (uint8_t*)blockBody.data(); //blockBodyAsVecUint8 must remain in scope until after render
        bv.AppendCanonicalBlock(block);

    }

    BOOST_REQUIRE(bv.Render(5000));
}

BOOST_AUTO_TEST_CASE(BundleViewV7TestCase)
{
    
    {
        const std::vector<uint8_t> canonicalTypesVec = { 5,3,2,BPV7_BLOCKTYPE_PAYLOAD }; //last block must be payload block
        const std::vector<std::string> canonicalBodyStringsVec = { "The ", "quick ", " brown", " fox" };
        
        BundleViewV7 bv;
        GenerateBundle(canonicalTypesVec, canonicalBodyStringsVec, bv);
        std::vector<uint8_t> bundleSerializedOriginal(bv.m_frontBuffer);
        //std::cout << "renderedsize: " << bv.m_frontBuffer.size() << "\n";

        BOOST_REQUIRE_GT(bundleSerializedOriginal.size(), 0);
        std::vector<uint8_t> bundleSerializedCopy(bundleSerializedOriginal); //the copy can get modified by bundle view on first load
        BOOST_REQUIRE(bundleSerializedOriginal == bundleSerializedCopy);
        bv.Reset();
        //std::cout << "sz " << bundleSerializedCopy.size() << std::endl;
        BOOST_REQUIRE(bv.LoadBundle(&bundleSerializedCopy[0], bundleSerializedCopy.size()));
        BOOST_REQUIRE(bv.m_backBuffer != bundleSerializedCopy);
        BOOST_REQUIRE(bv.m_frontBuffer != bundleSerializedCopy);

        Bpv7CbhePrimaryBlock & primary = bv.m_primaryBlockView.header;
        BOOST_REQUIRE_EQUAL(primary.m_sourceNodeId, cbhe_eid_t(PRIMARY_SRC_NODE, PRIMARY_SRC_SVC));
        BOOST_REQUIRE_EQUAL(primary.m_destinationEid, cbhe_eid_t(PRIMARY_DEST_NODE, PRIMARY_DEST_SVC));
        BOOST_REQUIRE_EQUAL(primary.m_creationTimestamp, TimestampUtil::bpv7_creation_timestamp_t(PRIMARY_TIME, PRIMARY_SEQ));
        BOOST_REQUIRE_EQUAL(primary.m_lifetimeMilliseconds, PRIMARY_LIFETIME);

        BOOST_REQUIRE_EQUAL(bv.GetNumCanonicalBlocks(), canonicalTypesVec.size());
        BOOST_REQUIRE_EQUAL(bv.GetCanonicalBlockCountByType(10), 0);
        for (std::size_t i = 0; i < canonicalTypesVec.size(); ++i) {
            BOOST_REQUIRE_EQUAL(bv.GetCanonicalBlockCountByType(canonicalTypesVec[i]), 1);
            std::vector<BundleViewV7::Bpv7CanonicalBlockView*> blocks;
            bv.GetCanonicalBlocksByType(canonicalTypesVec[i], blocks);
            BOOST_REQUIRE_EQUAL(blocks.size(), 1);
            const char * strPtr = (const char *)blocks[0]->header.m_dataPtr;
            std::string s(strPtr, strPtr + blocks[0]->header.m_dataLength);
            BOOST_REQUIRE_EQUAL(s, canonicalBodyStringsVec[i]);
            BOOST_REQUIRE_EQUAL(blocks[0]->header.m_blockTypeCode, canonicalTypesVec[i]);
        }

        BOOST_REQUIRE(bv.Render(5000));
        BOOST_REQUIRE(bv.m_backBuffer != bundleSerializedCopy);
        BOOST_REQUIRE_EQUAL(bv.m_frontBuffer.size(), bundleSerializedCopy.size());
        BOOST_REQUIRE(bv.m_frontBuffer == bundleSerializedCopy);

        //change 2nd block from quick to slow and type from 3 to 6 and render
        const uint32_t quickCrc = boost::next(bv.m_listCanonicalBlockView.begin())->header.m_computedCrc32;
        ChangeCanonicalBlockAndRender(bv, 3, 6, std::string("slow "));
        const uint32_t slowCrc = boost::next(bv.m_listCanonicalBlockView.begin())->header.m_computedCrc32;
        BOOST_REQUIRE_NE(quickCrc, slowCrc);
        BOOST_REQUIRE_EQUAL(bv.m_frontBuffer.size(), bv.m_backBuffer.size() - 1); //"quick" to "slow"
        BOOST_REQUIRE(bv.m_frontBuffer != bundleSerializedOriginal);
        BOOST_REQUIRE_EQUAL(bv.m_frontBuffer.size(), bv.m_renderedBundle.size());
        BOOST_REQUIRE_EQUAL(bv.GetNumCanonicalBlocks(), canonicalTypesVec.size());

        //Render again
        //std::cout << "render again\n";
        BOOST_REQUIRE(bv.Render(5000));
        BOOST_REQUIRE(bv.m_frontBuffer == bv.m_backBuffer);
        
        //revert 2nd block
        ChangeCanonicalBlockAndRender(bv, 6, 3, std::string("quick "));
        const uint32_t quickCrc2 = boost::next(bv.m_listCanonicalBlockView.begin())->header.m_computedCrc32;
        BOOST_REQUIRE_EQUAL(quickCrc, quickCrc2);
        BOOST_REQUIRE_EQUAL(bv.m_frontBuffer.size(), bundleSerializedOriginal.size());
        BOOST_REQUIRE_EQUAL(bv.m_frontBuffer.size(), bv.m_renderedBundle.size());
        BOOST_REQUIRE(bv.m_frontBuffer == bundleSerializedOriginal);

        
        
        
        {
            //change PRIMARY_SEQ from 1 to 65539 (adding 4 bytes)
            bv.m_primaryBlockView.header.m_creationTimestamp.sequenceNumber = 65539;
            bv.m_primaryBlockView.SetManuallyModified();
            BOOST_REQUIRE(bv.m_primaryBlockView.dirty);
            //std::cout << "render increase primary seq\n";
            BOOST_REQUIRE(bv.Render(5000));
            BOOST_REQUIRE_EQUAL(bv.m_frontBuffer.size(), bundleSerializedOriginal.size() + 4);
            BOOST_REQUIRE(!bv.m_primaryBlockView.dirty); //render removed dirty
            BOOST_REQUIRE_EQUAL(primary.m_lifetimeMilliseconds, PRIMARY_LIFETIME);
            BOOST_REQUIRE_EQUAL(primary.m_creationTimestamp.sequenceNumber, 65539);

            //restore PRIMARY_SEQ
            bv.m_primaryBlockView.header.m_creationTimestamp.sequenceNumber = PRIMARY_SEQ;
            bv.m_primaryBlockView.SetManuallyModified();
            //std::cout << "render restore primary seq\n";
            BOOST_REQUIRE(bv.Render(5000));
            BOOST_REQUIRE_EQUAL(bv.m_frontBuffer.size(), bundleSerializedOriginal.size());
            BOOST_REQUIRE(bv.m_frontBuffer == bundleSerializedOriginal); //back to equal
        }

        //delete and re-add 1st block
        {
            std::vector<BundleViewV7::Bpv7CanonicalBlockView*> blocks;
            bv.GetCanonicalBlocksByType(canonicalTypesVec.front(), blocks);
            BOOST_REQUIRE_EQUAL(blocks.size(), 1);
            blocks[0]->markedForDeletion = true;
            //std::cout << "render delete last block\n";
            BOOST_REQUIRE(bv.Render(5000));
            BOOST_REQUIRE_EQUAL(bv.GetNumCanonicalBlocks(), canonicalTypesVec.size() - 1);
            const uint64_t canonicalSize = //uint64_t bufferSize
                1 + //cbor initial byte denoting cbor array
                1 + //block type code byte
                1 + //block number
                1 + //m_blockProcessingControlFlags
                1 + //crc type code byte
                1 + //byte string header
                canonicalBodyStringsVec.front().length() + //data = len("The ")
                5; //crc32 byte array
            BOOST_REQUIRE_EQUAL(bv.m_frontBuffer.size(), bundleSerializedOriginal.size() - canonicalSize);

            PrependCanonicalBlockAndRender(bv, canonicalTypesVec.front(), std::string(canonicalBodyStringsVec.front()), 0); //0 was block number 0 from GenerateBundle
            BOOST_REQUIRE_EQUAL(bv.m_frontBuffer.size(), bundleSerializedOriginal.size());
            BOOST_REQUIRE(bv.m_frontBuffer == bundleSerializedOriginal); //back to equal
        }

        //delete and re-add 1st block by preallocation
        {
            std::vector<BundleViewV7::Bpv7CanonicalBlockView*> blocks;
            bv.GetCanonicalBlocksByType(canonicalTypesVec.front(), blocks);
            BOOST_REQUIRE_EQUAL(blocks.size(), 1);
            blocks[0]->markedForDeletion = true;
            //std::cout << "render delete last block\n";
            BOOST_REQUIRE(bv.Render(5000));
            BOOST_REQUIRE_EQUAL(bv.GetNumCanonicalBlocks(), canonicalTypesVec.size() - 1);
            const uint64_t canonicalSize = //uint64_t bufferSize
                1 + //cbor initial byte denoting cbor array
                1 + //block type code byte
                1 + //block number
                1 + //m_blockProcessingControlFlags
                1 + //crc type code byte
                1 + //byte string header
                canonicalBodyStringsVec.front().length() + //data = len("The ")
                5; //crc32 byte array
            BOOST_REQUIRE_EQUAL(bv.m_frontBuffer.size(), bundleSerializedOriginal.size() - canonicalSize);

            bv.m_backBuffer.assign(bv.m_backBuffer.size(), 0); //make sure zeroed out
            PrependCanonicalBlockAndRender_AllocateOnly(bv, canonicalTypesVec.front(), canonicalBodyStringsVec.front().length(), 0); //0 was block number 0 from GenerateBundle
            BOOST_REQUIRE_EQUAL(bv.m_frontBuffer.size(), bundleSerializedOriginal.size());
            BOOST_REQUIRE(bv.m_frontBuffer != bundleSerializedOriginal); //still not equal, need to copy data
            bv.GetCanonicalBlocksByType(canonicalTypesVec.front(), blocks); //get new preallocated block
            BOOST_REQUIRE_EQUAL(blocks.size(), 1);
            BOOST_REQUIRE_EQUAL(blocks[0]->header.m_dataLength, canonicalBodyStringsVec.front().length()); //make sure preallocated
            BOOST_REQUIRE_EQUAL((unsigned int)blocks[0]->header.m_dataPtr[0], 0); //make sure was zeroed out from above
            memcpy(blocks[0]->header.m_dataPtr, canonicalBodyStringsVec.front().data(), canonicalBodyStringsVec.front().length()); //copy data
            BOOST_REQUIRE(bv.m_frontBuffer != bundleSerializedOriginal); //still not equal, need to recompute crc
            blocks[0]->header.RecomputeCrcAfterDataModification((uint8_t*)blocks[0]->actualSerializedBlockPtr.data(), blocks[0]->actualSerializedBlockPtr.size()); //recompute crc
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

