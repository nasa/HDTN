#include <boost/test/unit_test.hpp>
#include <boost/thread.hpp>
#include "codec/BundleViewV7.h"
#include <iostream>
#include <string>
#include <inttypes.h>
#include <vector>
#include "Uri.h"
#include <boost/next_prior.hpp>
#include <boost/make_unique.hpp>
#include "PaddedVectorUint8.h"

static const uint64_t PRIMARY_SRC_NODE = 100;
static const uint64_t PRIMARY_SRC_SVC = 1;
static const uint64_t PRIMARY_DEST_NODE = 200;
static const uint64_t PRIMARY_DEST_SVC = 2;
static const uint64_t PRIMARY_TIME = 10000;
static const uint64_t PRIMARY_LIFETIME = 2000;
static const uint64_t PRIMARY_SEQ = 1;

static void AppendCanonicalBlockAndRender(BundleViewV7 & bv, BPV7_BLOCK_TYPE_CODE newType, std::string & newBlockBody, uint64_t blockNumber, const BPV7_CRC_TYPE crcTypeToUse) {
    //std::cout << "append " << (int)newType << "\n";
    std::unique_ptr<Bpv7CanonicalBlock> blockPtr = boost::make_unique<Bpv7CanonicalBlock>();
    Bpv7CanonicalBlock & block = *blockPtr;
    block.m_blockTypeCode = newType;
    block.m_blockProcessingControlFlags = BPV7_BLOCKFLAG::REMOVE_BLOCK_IF_IT_CANT_BE_PROCESSED; //something for checking against
    block.m_dataLength = newBlockBody.size();
    block.m_dataPtr = (uint8_t*)newBlockBody.data(); //blockBodyAsVecUint8 must remain in scope until after render
    block.m_crcType = crcTypeToUse;
    block.m_blockNumber = blockNumber;
    bv.AppendMoveCanonicalBlock(blockPtr);
    uint64_t expectedRenderSize;
    BOOST_REQUIRE(bv.GetSerializationSize(expectedRenderSize));
    BOOST_REQUIRE(bv.Render(5000));
    BOOST_REQUIRE_EQUAL(bv.m_frontBuffer.size(), expectedRenderSize);
    //check again after render
    BOOST_REQUIRE(bv.GetSerializationSize(expectedRenderSize));
    BOOST_REQUIRE_EQUAL(bv.m_frontBuffer.size(), expectedRenderSize);
}
static void PrependCanonicalBlockAndRender(BundleViewV7 & bv, BPV7_BLOCK_TYPE_CODE newType, std::string & newBlockBody, uint64_t blockNumber, const BPV7_CRC_TYPE crcTypeToUse) {
    std::unique_ptr<Bpv7CanonicalBlock> blockPtr = boost::make_unique<Bpv7CanonicalBlock>();
    Bpv7CanonicalBlock & block = *blockPtr;
    block.m_blockTypeCode = newType;
    block.m_blockProcessingControlFlags = BPV7_BLOCKFLAG::REMOVE_BLOCK_IF_IT_CANT_BE_PROCESSED; //something for checking against
    block.m_dataLength = newBlockBody.size();
    block.m_dataPtr = (uint8_t*)newBlockBody.data(); //blockBodyAsVecUint8 must remain in scope until after render
    block.m_crcType = crcTypeToUse;
    block.m_blockNumber = blockNumber;
    bv.PrependMoveCanonicalBlock(blockPtr);
    uint64_t expectedRenderSize;
    BOOST_REQUIRE(bv.GetSerializationSize(expectedRenderSize));
    BOOST_REQUIRE(bv.Render(5000));
    BOOST_REQUIRE_EQUAL(bv.m_frontBuffer.size(), expectedRenderSize);
    //check again after render
    BOOST_REQUIRE(bv.GetSerializationSize(expectedRenderSize));
    BOOST_REQUIRE_EQUAL(bv.m_frontBuffer.size(), expectedRenderSize);
}
static void PrependCanonicalBlockAndRender_AllocateOnly(BundleViewV7 & bv, BPV7_BLOCK_TYPE_CODE newType, uint64_t dataLengthToAllocate, uint64_t blockNumber, const BPV7_CRC_TYPE crcTypeToUse) {
    //std::cout << "append " << (int)newType << "\n";
    std::unique_ptr<Bpv7CanonicalBlock> blockPtr = boost::make_unique<Bpv7CanonicalBlock>();
    Bpv7CanonicalBlock & block = *blockPtr;
    block.m_blockTypeCode = newType;
    block.m_blockProcessingControlFlags = BPV7_BLOCKFLAG::REMOVE_BLOCK_IF_IT_CANT_BE_PROCESSED; //something for checking against
    block.m_dataLength = dataLengthToAllocate;
    block.m_dataPtr = NULL;
    block.m_crcType = crcTypeToUse;
    block.m_blockNumber = blockNumber;
    bv.PrependMoveCanonicalBlock(blockPtr);
    uint64_t expectedRenderSize;
    BOOST_REQUIRE(bv.GetSerializationSize(expectedRenderSize));
    BOOST_REQUIRE(bv.Render(5000));
    BOOST_REQUIRE_EQUAL(bv.m_frontBuffer.size(), expectedRenderSize);
    //check again after render
    BOOST_REQUIRE(bv.GetSerializationSize(expectedRenderSize));
    BOOST_REQUIRE_EQUAL(bv.m_frontBuffer.size(), expectedRenderSize);
}

static void ChangeCanonicalBlockAndRender(BundleViewV7 & bv, BPV7_BLOCK_TYPE_CODE oldType, BPV7_BLOCK_TYPE_CODE newType, std::string & newBlockBody) {

    //std::cout << "change " << (int)oldType << " to " << (int)newType << "\n";
    std::vector<BundleViewV7::Bpv7CanonicalBlockView*> blocks;
    bv.GetCanonicalBlocksByType(oldType, blocks);
    BOOST_REQUIRE_EQUAL(blocks.size(), 1);
    Bpv7CanonicalBlock & block = *(blocks[0]->headerPtr);
    block.m_blockTypeCode = newType;
    block.m_dataLength = newBlockBody.size();
    block.m_dataPtr = (uint8_t*)newBlockBody.data(); //blockBodyAsVecUint8 must remain in scope until after render
    blocks[0]->SetManuallyModified();

    uint64_t expectedRenderSize;
    BOOST_REQUIRE(bv.GetSerializationSize(expectedRenderSize));
    BOOST_REQUIRE(bv.Render(5000));
    BOOST_REQUIRE_EQUAL(bv.m_frontBuffer.size(), expectedRenderSize);
    //check again after render
    BOOST_REQUIRE(bv.GetSerializationSize(expectedRenderSize));
    BOOST_REQUIRE_EQUAL(bv.m_frontBuffer.size(), expectedRenderSize);
}

static void GenerateBundle(const std::vector<BPV7_BLOCK_TYPE_CODE> & canonicalTypesVec, const std::vector<uint8_t> & blockNumbersToUse,
    const std::vector<std::string> & canonicalBodyStringsVec, BundleViewV7 & bv, const BPV7_CRC_TYPE crcTypeToUse)
{

    Bpv7CbhePrimaryBlock & primary = bv.m_primaryBlockView.header;
    primary.SetZero();


    primary.m_bundleProcessingControlFlags = BPV7_BUNDLEFLAG::NOFRAGMENT;  //All BP endpoints identified by ipn-scheme endpoint IDs are singleton endpoints.
    primary.m_sourceNodeId.Set(PRIMARY_SRC_NODE, PRIMARY_SRC_SVC);
    primary.m_destinationEid.Set(PRIMARY_DEST_NODE, PRIMARY_DEST_SVC);
    primary.m_reportToEid.Set(0, 0);
    primary.m_creationTimestamp.millisecondsSinceStartOfYear2000 = PRIMARY_TIME;
    primary.m_lifetimeMilliseconds = PRIMARY_LIFETIME;
    primary.m_creationTimestamp.sequenceNumber = PRIMARY_SEQ;
    primary.m_crcType = crcTypeToUse;
    bv.m_primaryBlockView.SetManuallyModified();

    for (std::size_t i = 0; i < canonicalTypesVec.size(); ++i) {
        std::unique_ptr<Bpv7CanonicalBlock> blockPtr = boost::make_unique<Bpv7CanonicalBlock>();
        Bpv7CanonicalBlock & block = *blockPtr;
        //block.SetZero();

        block.m_blockTypeCode = canonicalTypesVec[i];
        block.m_blockProcessingControlFlags = BPV7_BLOCKFLAG::REMOVE_BLOCK_IF_IT_CANT_BE_PROCESSED; //something for checking against
        block.m_blockNumber = blockNumbersToUse[i];
        block.m_crcType = crcTypeToUse;
        const std::string & blockBody = canonicalBodyStringsVec[i];
        block.m_dataLength = blockBody.size();
        block.m_dataPtr = (uint8_t*)blockBody.data(); //blockBodyAsVecUint8 must remain in scope until after render
        BOOST_REQUIRE(blockPtr);
        bv.AppendMoveCanonicalBlock(blockPtr);
        BOOST_REQUIRE(!blockPtr);
    }

    BOOST_REQUIRE(bv.Render(5000));
}

BOOST_AUTO_TEST_CASE(BundleViewV7TestCase)
{
    const std::vector<BPV7_BLOCK_TYPE_CODE> canonicalTypesVec = {
        BPV7_BLOCK_TYPE_CODE::UNUSED_5,
        BPV7_BLOCK_TYPE_CODE::UNUSED_3,
        BPV7_BLOCK_TYPE_CODE::UNUSED_2,
        BPV7_BLOCK_TYPE_CODE::PAYLOAD //last block must be payload block
    }; 
    const std::vector<uint8_t> blockNumbersToUse = { 2,3,4,1 }; //The block number of the payload block is always 1.
    const std::vector<std::string> canonicalBodyStringsVec = { "The ", "quick ", " brown", " fox" };
    const std::vector<BPV7_CRC_TYPE> crcTypesVec = { BPV7_CRC_TYPE::NONE, BPV7_CRC_TYPE::CRC16_X25, BPV7_CRC_TYPE::CRC32C };
    for (std::size_t crcI = 0; crcI < crcTypesVec.size(); ++crcI) {
        const BPV7_CRC_TYPE crcTypeToUse = crcTypesVec[crcI];

        BundleViewV7 bv;
        GenerateBundle(canonicalTypesVec, blockNumbersToUse, canonicalBodyStringsVec, bv, crcTypeToUse);
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
        BOOST_REQUIRE_EQUAL(bv.m_primaryBlockView.actualSerializedPrimaryBlockPtr.size(), primary.GetSerializationSize());

        BOOST_REQUIRE_EQUAL(bv.GetNumCanonicalBlocks(), canonicalTypesVec.size());
        BOOST_REQUIRE_EQUAL(bv.GetCanonicalBlockCountByType(BPV7_BLOCK_TYPE_CODE::HOP_COUNT), 0);
        for (std::size_t i = 0; i < canonicalTypesVec.size(); ++i) {
            BOOST_REQUIRE_EQUAL(bv.GetCanonicalBlockCountByType(canonicalTypesVec[i]), 1);
            std::vector<BundleViewV7::Bpv7CanonicalBlockView*> blocks;
            bv.GetCanonicalBlocksByType(canonicalTypesVec[i], blocks);
            BOOST_REQUIRE_EQUAL(blocks.size(), 1);
            const char * strPtr = (const char *)blocks[0]->headerPtr->m_dataPtr;
            std::string s(strPtr, strPtr + blocks[0]->headerPtr->m_dataLength);
            BOOST_REQUIRE_EQUAL(s, canonicalBodyStringsVec[i]);
            BOOST_REQUIRE_EQUAL(blocks[0]->headerPtr->m_blockTypeCode, canonicalTypesVec[i]);
        }
        uint64_t expectedRenderSize;
        BOOST_REQUIRE(bv.GetSerializationSize(expectedRenderSize));
        BOOST_REQUIRE(bv.Render(5000));
        BOOST_REQUIRE(bv.m_backBuffer != bundleSerializedCopy);
        BOOST_REQUIRE_EQUAL(bv.m_frontBuffer.size(), bundleSerializedCopy.size());
        BOOST_REQUIRE_EQUAL(bv.m_frontBuffer.size(), expectedRenderSize);
        //check again after render
        BOOST_REQUIRE(bv.GetSerializationSize(expectedRenderSize));
        BOOST_REQUIRE_EQUAL(bv.m_frontBuffer.size(), expectedRenderSize);
        BOOST_REQUIRE(bv.m_frontBuffer == bundleSerializedCopy);

        //change 2nd block from quick to slow and type from 3 to 4 and render
        const uint32_t quickCrc = (crcTypeToUse == BPV7_CRC_TYPE::NONE) ? 0 : 
            (crcTypeToUse == BPV7_CRC_TYPE::CRC16_X25) ? boost::next(bv.m_listCanonicalBlockView.begin())->headerPtr->m_computedCrc16 :
            boost::next(bv.m_listCanonicalBlockView.begin())->headerPtr->m_computedCrc32;
        std::string slowString("slow ");
        ChangeCanonicalBlockAndRender(bv, BPV7_BLOCK_TYPE_CODE::UNUSED_3, BPV7_BLOCK_TYPE_CODE::UNUSED_4, slowString);
        const uint32_t slowCrc = (crcTypeToUse == BPV7_CRC_TYPE::NONE) ? 1 :
            (crcTypeToUse == BPV7_CRC_TYPE::CRC16_X25) ? boost::next(bv.m_listCanonicalBlockView.begin())->headerPtr->m_computedCrc16 :
            boost::next(bv.m_listCanonicalBlockView.begin())->headerPtr->m_computedCrc32;
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
        std::string quickString("quick ");
        ChangeCanonicalBlockAndRender(bv, BPV7_BLOCK_TYPE_CODE::UNUSED_4, BPV7_BLOCK_TYPE_CODE::UNUSED_3, quickString);
        const uint32_t quickCrc2 = (crcTypeToUse == BPV7_CRC_TYPE::NONE) ? 0 :
            (crcTypeToUse == BPV7_CRC_TYPE::CRC16_X25) ? boost::next(bv.m_listCanonicalBlockView.begin())->headerPtr->m_computedCrc16 :
            boost::next(bv.m_listCanonicalBlockView.begin())->headerPtr->m_computedCrc32;
        //std::cout << "crc=" << quickCrc << "\n";
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
            const uint64_t blockNumber = blocks[0]->headerPtr->m_blockNumber;
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
                ((crcTypeToUse == BPV7_CRC_TYPE::NONE) ? 0 : (crcTypeToUse == BPV7_CRC_TYPE::CRC16_X25) ? 3 : 5); //crc byte array
            //std::cout << "canonicalSize=" << canonicalSize << "\n";
            BOOST_REQUIRE_EQUAL(bv.m_frontBuffer.size(), bundleSerializedOriginal.size() - canonicalSize);

            std::string frontString(canonicalBodyStringsVec.front());
            PrependCanonicalBlockAndRender(bv, canonicalTypesVec.front(), frontString, blockNumber, crcTypeToUse); //0 was block number 0 from GenerateBundle
            BOOST_REQUIRE_EQUAL(bv.m_frontBuffer.size(), bundleSerializedOriginal.size());
            BOOST_REQUIRE(bv.m_frontBuffer == bundleSerializedOriginal); //back to equal
        }

        //delete and re-add 1st block by preallocation
        {
            std::vector<BundleViewV7::Bpv7CanonicalBlockView*> blocks;
            bv.GetCanonicalBlocksByType(canonicalTypesVec.front(), blocks);
            BOOST_REQUIRE_EQUAL(blocks.size(), 1);
            const uint64_t blockNumber = blocks[0]->headerPtr->m_blockNumber;
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
                ((crcTypeToUse == BPV7_CRC_TYPE::NONE) ? 0 : (crcTypeToUse == BPV7_CRC_TYPE::CRC16_X25) ? 3 : 5); //crc byte array
            BOOST_REQUIRE_EQUAL(bv.m_frontBuffer.size(), bundleSerializedOriginal.size() - canonicalSize);

            bv.m_backBuffer.assign(bv.m_backBuffer.size(), 0); //make sure zeroed out
            PrependCanonicalBlockAndRender_AllocateOnly(bv, canonicalTypesVec.front(), canonicalBodyStringsVec.front().length(), blockNumber, crcTypeToUse); //0 was block number 0 from GenerateBundle
            BOOST_REQUIRE_EQUAL(bv.m_frontBuffer.size(), bundleSerializedOriginal.size());
            BOOST_REQUIRE(bv.m_frontBuffer != bundleSerializedOriginal); //still not equal, need to copy data
            bv.GetCanonicalBlocksByType(canonicalTypesVec.front(), blocks); //get new preallocated block
            BOOST_REQUIRE_EQUAL(blocks.size(), 1);
            BOOST_REQUIRE_EQUAL(blocks[0]->headerPtr->m_dataLength, canonicalBodyStringsVec.front().length()); //make sure preallocated
            BOOST_REQUIRE_EQUAL((unsigned int)blocks[0]->headerPtr->m_dataPtr[0], 0); //make sure was zeroed out from above
            memcpy(blocks[0]->headerPtr->m_dataPtr, canonicalBodyStringsVec.front().data(), canonicalBodyStringsVec.front().length()); //copy data
            if (crcTypeToUse != BPV7_CRC_TYPE::NONE) {
                BOOST_REQUIRE(bv.m_frontBuffer != bundleSerializedOriginal); //still not equal, need to recompute crc
                blocks[0]->headerPtr->RecomputeCrcAfterDataModification((uint8_t*)blocks[0]->actualSerializedBlockPtr.data(), blocks[0]->actualSerializedBlockPtr.size()); //recompute crc
            }
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



BOOST_AUTO_TEST_CASE(Bpv7ExtensionBlocksTestCase)
{
    static const uint64_t PREVIOUS_NODE = 12345;
    static const uint64_t PREVIOUS_SVC = 678910;
    static const uint64_t BUNDLE_AGE_MS = 135791113;
    static const uint8_t HOP_LIMIT = 250;
    static const uint8_t HOP_COUNT = 200;
    const std::string payloadString = { "This is the data inside the bpv7 payload block!!!" };
    
    
    const std::vector<BPV7_CRC_TYPE> crcTypesVec = { BPV7_CRC_TYPE::NONE, BPV7_CRC_TYPE::CRC16_X25, BPV7_CRC_TYPE::CRC32C };
    for (std::size_t crcI = 0; crcI < crcTypesVec.size(); ++crcI) {
        const BPV7_CRC_TYPE crcTypeToUse = crcTypesVec[crcI];

        BundleViewV7 bv;
        Bpv7CbhePrimaryBlock & primary = bv.m_primaryBlockView.header;
        primary.SetZero();


        primary.m_bundleProcessingControlFlags = BPV7_BUNDLEFLAG::NOFRAGMENT;  //All BP endpoints identified by ipn-scheme endpoint IDs are singleton endpoints.
        primary.m_sourceNodeId.Set(PRIMARY_SRC_NODE, PRIMARY_SRC_SVC);
        primary.m_destinationEid.Set(PRIMARY_DEST_NODE, PRIMARY_DEST_SVC);
        primary.m_reportToEid.Set(0, 0);
        primary.m_creationTimestamp.millisecondsSinceStartOfYear2000 = PRIMARY_TIME;
        primary.m_lifetimeMilliseconds = PRIMARY_LIFETIME;
        primary.m_creationTimestamp.sequenceNumber = PRIMARY_SEQ;
        primary.m_crcType = crcTypeToUse;
        bv.m_primaryBlockView.SetManuallyModified();

        //add previous node block
        {
            std::unique_ptr<Bpv7CanonicalBlock> blockPtr = boost::make_unique<Bpv7PreviousNodeCanonicalBlock>();
            Bpv7PreviousNodeCanonicalBlock & block = *(reinterpret_cast<Bpv7PreviousNodeCanonicalBlock*>(blockPtr.get()));
            //block.SetZero();

            block.m_blockProcessingControlFlags = BPV7_BLOCKFLAG::REMOVE_BLOCK_IF_IT_CANT_BE_PROCESSED; //something for checking against
            block.m_blockNumber = 2;
            block.m_crcType = crcTypeToUse;
            block.m_previousNode.Set(PREVIOUS_NODE, PREVIOUS_SVC);
            bv.AppendMoveCanonicalBlock(blockPtr);
            BOOST_REQUIRE_EQUAL(bv.GetNextFreeCanonicalBlockNumber(), 3);
        }

        //add bundle age block
        {
            std::unique_ptr<Bpv7CanonicalBlock> blockPtr = boost::make_unique<Bpv7BundleAgeCanonicalBlock>();
            Bpv7BundleAgeCanonicalBlock & block = *(reinterpret_cast<Bpv7BundleAgeCanonicalBlock*>(blockPtr.get()));
            //block.SetZero();

            block.m_blockProcessingControlFlags = BPV7_BLOCKFLAG::REMOVE_BLOCK_IF_IT_CANT_BE_PROCESSED; //something for checking against
            block.m_blockNumber = 3;
            block.m_crcType = crcTypeToUse;
            block.m_bundleAgeMilliseconds = BUNDLE_AGE_MS;
            bv.AppendMoveCanonicalBlock(blockPtr);
        }

        //add hop count block
        {
            std::unique_ptr<Bpv7CanonicalBlock> blockPtr = boost::make_unique<Bpv7HopCountCanonicalBlock>();
            Bpv7HopCountCanonicalBlock & block = *(reinterpret_cast<Bpv7HopCountCanonicalBlock*>(blockPtr.get()));
            //block.SetZero();

            block.m_blockProcessingControlFlags = BPV7_BLOCKFLAG::REMOVE_BLOCK_IF_IT_CANT_BE_PROCESSED; //something for checking against
            block.m_blockNumber = 4;
            block.m_crcType = crcTypeToUse;
            block.m_hopLimit = HOP_LIMIT;
            block.m_hopCount = HOP_COUNT;
            bv.AppendMoveCanonicalBlock(blockPtr);
        }
        
        //add payload block
        {
            
            std::unique_ptr<Bpv7CanonicalBlock> blockPtr = boost::make_unique<Bpv7CanonicalBlock>();
            Bpv7CanonicalBlock & block = *blockPtr;
            //block.SetZero();

            block.m_blockTypeCode = BPV7_BLOCK_TYPE_CODE::PAYLOAD;
            block.m_blockProcessingControlFlags = BPV7_BLOCKFLAG::REMOVE_BLOCK_IF_IT_CANT_BE_PROCESSED; //something for checking against
            block.m_blockNumber = 1; //must be 1
            block.m_crcType = crcTypeToUse;
            block.m_dataLength = payloadString.size();
            block.m_dataPtr = (uint8_t*)payloadString.data(); //payloadString must remain in scope until after render
            bv.AppendMoveCanonicalBlock(blockPtr);

        }

        BOOST_REQUIRE(bv.Render(5000));

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

        BOOST_REQUIRE_EQUAL(primary.m_sourceNodeId, cbhe_eid_t(PRIMARY_SRC_NODE, PRIMARY_SRC_SVC));
        BOOST_REQUIRE_EQUAL(primary.m_destinationEid, cbhe_eid_t(PRIMARY_DEST_NODE, PRIMARY_DEST_SVC));
        BOOST_REQUIRE_EQUAL(primary.m_creationTimestamp, TimestampUtil::bpv7_creation_timestamp_t(PRIMARY_TIME, PRIMARY_SEQ));
        BOOST_REQUIRE_EQUAL(primary.m_lifetimeMilliseconds, PRIMARY_LIFETIME);
        BOOST_REQUIRE_EQUAL(bv.m_primaryBlockView.actualSerializedPrimaryBlockPtr.size(), primary.GetSerializationSize());

        BOOST_REQUIRE_EQUAL(bv.GetNumCanonicalBlocks(), 4);
        BOOST_REQUIRE_EQUAL(bv.GetCanonicalBlockCountByType(BPV7_BLOCK_TYPE_CODE::PREVIOUS_NODE), 1);
        BOOST_REQUIRE_EQUAL(bv.GetCanonicalBlockCountByType(BPV7_BLOCK_TYPE_CODE::BUNDLE_AGE), 1);
        BOOST_REQUIRE_EQUAL(bv.GetCanonicalBlockCountByType(BPV7_BLOCK_TYPE_CODE::HOP_COUNT), 1);
        BOOST_REQUIRE_EQUAL(bv.GetCanonicalBlockCountByType(BPV7_BLOCK_TYPE_CODE::PAYLOAD), 1);
        BOOST_REQUIRE_EQUAL(bv.GetCanonicalBlockCountByType(BPV7_BLOCK_TYPE_CODE::UNUSED_4), 0);

        BOOST_REQUIRE_EQUAL(bv.GetNextFreeCanonicalBlockNumber(), 5);
        
        //get previous node
        {
            std::vector<BundleViewV7::Bpv7CanonicalBlockView*> blocks;
            bv.GetCanonicalBlocksByType(BPV7_BLOCK_TYPE_CODE::PREVIOUS_NODE, blocks);
            BOOST_REQUIRE_EQUAL(blocks.size(), 1);
            Bpv7PreviousNodeCanonicalBlock* previousNodeBlockPtr = dynamic_cast<Bpv7PreviousNodeCanonicalBlock*>(blocks[0]->headerPtr.get());
            BOOST_REQUIRE(previousNodeBlockPtr);
            BOOST_REQUIRE_EQUAL(previousNodeBlockPtr->m_blockTypeCode, BPV7_BLOCK_TYPE_CODE::PREVIOUS_NODE);
            BOOST_REQUIRE_EQUAL(previousNodeBlockPtr->m_blockNumber, 2);
            BOOST_REQUIRE_EQUAL(previousNodeBlockPtr->m_previousNode.nodeId, PREVIOUS_NODE);
            BOOST_REQUIRE_EQUAL(previousNodeBlockPtr->m_previousNode.serviceId, PREVIOUS_SVC);
            BOOST_REQUIRE_EQUAL(blocks[0]->actualSerializedBlockPtr.size(), previousNodeBlockPtr->GetSerializationSize());
            BOOST_REQUIRE(!blocks[0]->isEncrypted); //not encrypted
        }

        //get bundle age
        {
            std::vector<BundleViewV7::Bpv7CanonicalBlockView*> blocks;
            bv.GetCanonicalBlocksByType(BPV7_BLOCK_TYPE_CODE::BUNDLE_AGE, blocks);
            BOOST_REQUIRE_EQUAL(blocks.size(), 1);
            Bpv7BundleAgeCanonicalBlock* bundleAgeBlockPtr = dynamic_cast<Bpv7BundleAgeCanonicalBlock*>(blocks[0]->headerPtr.get());
            BOOST_REQUIRE(bundleAgeBlockPtr);
            BOOST_REQUIRE_EQUAL(bundleAgeBlockPtr->m_blockTypeCode, BPV7_BLOCK_TYPE_CODE::BUNDLE_AGE);
            BOOST_REQUIRE_EQUAL(bundleAgeBlockPtr->m_blockNumber, 3);
            BOOST_REQUIRE_EQUAL(bundleAgeBlockPtr->m_bundleAgeMilliseconds, BUNDLE_AGE_MS);
            BOOST_REQUIRE_EQUAL(blocks[0]->actualSerializedBlockPtr.size(), bundleAgeBlockPtr->GetSerializationSize());
            BOOST_REQUIRE(!blocks[0]->isEncrypted); //not encrypted
        }

        //get hop count
        {
            std::vector<BundleViewV7::Bpv7CanonicalBlockView*> blocks;
            bv.GetCanonicalBlocksByType(BPV7_BLOCK_TYPE_CODE::HOP_COUNT, blocks);
            BOOST_REQUIRE_EQUAL(blocks.size(), 1);
            Bpv7HopCountCanonicalBlock* hopCountBlockPtr = dynamic_cast<Bpv7HopCountCanonicalBlock*>(blocks[0]->headerPtr.get());
            BOOST_REQUIRE(hopCountBlockPtr);
            BOOST_REQUIRE_EQUAL(hopCountBlockPtr->m_blockTypeCode, BPV7_BLOCK_TYPE_CODE::HOP_COUNT);
            BOOST_REQUIRE_EQUAL(hopCountBlockPtr->m_blockNumber, 4);
            BOOST_REQUIRE_EQUAL(hopCountBlockPtr->m_hopLimit, HOP_LIMIT);
            BOOST_REQUIRE_EQUAL(hopCountBlockPtr->m_hopCount, HOP_COUNT);
            BOOST_REQUIRE_EQUAL(blocks[0]->actualSerializedBlockPtr.size(), hopCountBlockPtr->GetSerializationSize());
            BOOST_REQUIRE(!blocks[0]->isEncrypted); //not encrypted
        }
        
        //get payload
        {
            std::vector<BundleViewV7::Bpv7CanonicalBlockView*> blocks;
            bv.GetCanonicalBlocksByType(BPV7_BLOCK_TYPE_CODE::PAYLOAD, blocks);
            BOOST_REQUIRE_EQUAL(blocks.size(), 1);
            const char * strPtr = (const char *)blocks[0]->headerPtr->m_dataPtr;
            std::string s(strPtr, strPtr + blocks[0]->headerPtr->m_dataLength);
            //std::cout << "s: " << s << "\n";
            BOOST_REQUIRE_EQUAL(s, payloadString);
            BOOST_REQUIRE_EQUAL(blocks[0]->headerPtr->m_blockTypeCode, BPV7_BLOCK_TYPE_CODE::PAYLOAD);
            BOOST_REQUIRE_EQUAL(blocks[0]->headerPtr->m_blockNumber, 1);
            BOOST_REQUIRE_EQUAL(blocks[0]->actualSerializedBlockPtr.size(), blocks[0]->headerPtr->GetSerializationSize());
            BOOST_REQUIRE(!blocks[0]->isEncrypted); //not encrypted
        }

        //attempt to increase hop count without rerendering whole bundle
        {
            std::vector<BundleViewV7::Bpv7CanonicalBlockView*> blocks;
            bv.GetCanonicalBlocksByType(BPV7_BLOCK_TYPE_CODE::HOP_COUNT, blocks);
            BOOST_REQUIRE_EQUAL(blocks.size(), 1);
            Bpv7HopCountCanonicalBlock* hopCountBlockPtr = dynamic_cast<Bpv7HopCountCanonicalBlock*>(blocks[0]->headerPtr.get());
            BOOST_REQUIRE(hopCountBlockPtr);
            BOOST_REQUIRE_EQUAL(hopCountBlockPtr->m_blockTypeCode, BPV7_BLOCK_TYPE_CODE::HOP_COUNT);
            BOOST_REQUIRE_EQUAL(hopCountBlockPtr->m_blockNumber, 4);
            BOOST_REQUIRE_EQUAL(hopCountBlockPtr->m_hopLimit, HOP_LIMIT);
            BOOST_REQUIRE_EQUAL(hopCountBlockPtr->m_hopCount, HOP_COUNT);

            uint8_t * bundlePtrStart = (uint8_t*)(bv.m_renderedBundle.data());
            std::size_t bundleSize = bv.m_renderedBundle.size();
            uint8_t * blockPtrStart = (uint8_t*)(blocks[0]->actualSerializedBlockPtr.data());
            std::size_t blockSize = blocks[0]->actualSerializedBlockPtr.size();
            std::vector<uint8_t> bundleUnmodified(bundlePtrStart, bundlePtrStart + bundleSize);
            ++hopCountBlockPtr->m_hopCount;
            BOOST_REQUIRE(hopCountBlockPtr->TryReserializeExtensionBlockDataWithoutResizeBpv7());
            blocks[0]->headerPtr->RecomputeCrcAfterDataModification(blockPtrStart, blockSize);
            std::vector<uint8_t> bundleModifiedButSameSize(bundlePtrStart, bundlePtrStart + bundleSize);
            BOOST_REQUIRE(bundleSerializedOriginal == bundleUnmodified);
            BOOST_REQUIRE(bundleSerializedOriginal != bundleModifiedButSameSize);
            BOOST_REQUIRE_EQUAL(bundleSerializedOriginal.size(), bundleModifiedButSameSize.size());
            {
                BundleViewV7 bv2;
                BOOST_REQUIRE(bv2.LoadBundle(&bundleModifiedButSameSize[0], bundleModifiedButSameSize.size()));
                //get hop count
                {
                    std::vector<BundleViewV7::Bpv7CanonicalBlockView*> blocks2;
                    bv2.GetCanonicalBlocksByType(BPV7_BLOCK_TYPE_CODE::HOP_COUNT, blocks2);
                    BOOST_REQUIRE_EQUAL(blocks2.size(), 1);
                    Bpv7HopCountCanonicalBlock* hopCountBlockPtr2 = dynamic_cast<Bpv7HopCountCanonicalBlock*>(blocks2[0]->headerPtr.get());
                    BOOST_REQUIRE(hopCountBlockPtr2);
                    BOOST_REQUIRE_EQUAL(hopCountBlockPtr2->m_blockTypeCode, BPV7_BLOCK_TYPE_CODE::HOP_COUNT);
                    BOOST_REQUIRE_EQUAL(hopCountBlockPtr2->m_blockNumber, 4);
                    BOOST_REQUIRE_EQUAL(hopCountBlockPtr2->m_hopLimit, HOP_LIMIT);
                    BOOST_REQUIRE_EQUAL(hopCountBlockPtr2->m_hopCount, HOP_COUNT + 1); //increased by 1
                }
            }
        }
    }
}




BOOST_AUTO_TEST_CASE(Bpv7PrependExtensionBlockToPaddedBundleTestCase)
{
    static const uint64_t PREVIOUS_NODE = 12345;
    static const uint64_t PREVIOUS_SVC = 678910;
    static const uint64_t BUNDLE_AGE_MS = 135791113;
    static const uint8_t HOP_LIMIT = 250;
    static const uint8_t HOP_COUNT = 200;
    const std::string payloadString = { "This is the data inside the bpv7 payload block!!!" };

    const std::vector<BPV7_CRC_TYPE> crcTypesVec = { BPV7_CRC_TYPE::NONE, BPV7_CRC_TYPE::CRC16_X25, BPV7_CRC_TYPE::CRC32C };
    for (std::size_t crcI = 0; crcI < crcTypesVec.size(); ++crcI) {
        const BPV7_CRC_TYPE crcTypeToUse = crcTypesVec[crcI];

        BundleViewV7 bv;
        Bpv7CbhePrimaryBlock & primary = bv.m_primaryBlockView.header;
        primary.SetZero();


        primary.m_bundleProcessingControlFlags = BPV7_BUNDLEFLAG::NOFRAGMENT;  //All BP endpoints identified by ipn-scheme endpoint IDs are singleton endpoints.
        primary.m_sourceNodeId.Set(PRIMARY_SRC_NODE, PRIMARY_SRC_SVC);
        primary.m_destinationEid.Set(PRIMARY_DEST_NODE, PRIMARY_DEST_SVC);
        primary.m_reportToEid.Set(0, 0);
        primary.m_creationTimestamp.millisecondsSinceStartOfYear2000 = PRIMARY_TIME;
        primary.m_lifetimeMilliseconds = PRIMARY_LIFETIME;
        primary.m_creationTimestamp.sequenceNumber = PRIMARY_SEQ;
        primary.m_crcType = crcTypeToUse;
        bv.m_primaryBlockView.SetManuallyModified();

        
        //add only a payload block
        {

            std::unique_ptr<Bpv7CanonicalBlock> blockPtr = boost::make_unique<Bpv7CanonicalBlock>();
            Bpv7CanonicalBlock & block = *blockPtr;
            //block.SetZero();

            block.m_blockTypeCode = BPV7_BLOCK_TYPE_CODE::PAYLOAD;
            block.m_blockProcessingControlFlags = BPV7_BLOCKFLAG::REMOVE_BLOCK_IF_IT_CANT_BE_PROCESSED; //something for checking against
            block.m_blockNumber = 1; //must be 1
            block.m_crcType = crcTypeToUse;
            block.m_dataLength = payloadString.size();
            block.m_dataPtr = (uint8_t*)payloadString.data(); //payloadString must remain in scope until after render
            bv.AppendMoveCanonicalBlock(blockPtr);

        }

        BOOST_REQUIRE(bv.Render(5000));

        std::vector<uint8_t> bundleSerializedOriginal(bv.m_frontBuffer);
        //std::cout << "renderedsize: " << bv.m_frontBuffer.size() << "\n";

        BOOST_REQUIRE_GT(bundleSerializedOriginal.size(), 0);
        padded_vector_uint8_t bundleSerializedPadded(bundleSerializedOriginal.data(), bundleSerializedOriginal.data() + bundleSerializedOriginal.size()); //the copy can get modified by bundle view on first load
        BOOST_REQUIRE_EQUAL(bundleSerializedOriginal.size(), bundleSerializedPadded.size());
        bv.Reset();
        //std::cout << "sz " << bundleSerializedPadded.size() << std::endl;
        BOOST_REQUIRE(bv.LoadBundle(&bundleSerializedPadded[0], bundleSerializedPadded.size()));

        BOOST_REQUIRE_EQUAL(primary.m_sourceNodeId, cbhe_eid_t(PRIMARY_SRC_NODE, PRIMARY_SRC_SVC));
        BOOST_REQUIRE_EQUAL(primary.m_destinationEid, cbhe_eid_t(PRIMARY_DEST_NODE, PRIMARY_DEST_SVC));
        BOOST_REQUIRE_EQUAL(primary.m_creationTimestamp, TimestampUtil::bpv7_creation_timestamp_t(PRIMARY_TIME, PRIMARY_SEQ));
        BOOST_REQUIRE_EQUAL(primary.m_lifetimeMilliseconds, PRIMARY_LIFETIME);

        BOOST_REQUIRE_EQUAL(bv.GetNumCanonicalBlocks(), 1);
        BOOST_REQUIRE_EQUAL(bv.GetCanonicalBlockCountByType(BPV7_BLOCK_TYPE_CODE::PREVIOUS_NODE), 0);
        BOOST_REQUIRE_EQUAL(bv.GetCanonicalBlockCountByType(BPV7_BLOCK_TYPE_CODE::BUNDLE_AGE), 0);
        BOOST_REQUIRE_EQUAL(bv.GetCanonicalBlockCountByType(BPV7_BLOCK_TYPE_CODE::HOP_COUNT), 0);
        BOOST_REQUIRE_EQUAL(bv.GetCanonicalBlockCountByType(BPV7_BLOCK_TYPE_CODE::PAYLOAD), 1);

        
        //get payload
        {
            std::vector<BundleViewV7::Bpv7CanonicalBlockView*> blocks;
            bv.GetCanonicalBlocksByType(BPV7_BLOCK_TYPE_CODE::PAYLOAD, blocks);
            BOOST_REQUIRE_EQUAL(blocks.size(), 1);
            const char * strPtr = (const char *)blocks[0]->headerPtr->m_dataPtr;
            std::string s(strPtr, strPtr + blocks[0]->headerPtr->m_dataLength);
            //std::cout << "s: " << s << "\n";
            BOOST_REQUIRE_EQUAL(s, payloadString);
            BOOST_REQUIRE_EQUAL(blocks[0]->headerPtr->m_blockTypeCode, BPV7_BLOCK_TYPE_CODE::PAYLOAD);
            BOOST_REQUIRE_EQUAL(blocks[0]->headerPtr->m_blockNumber, 1);
            BOOST_REQUIRE(!blocks[0]->isEncrypted); //not encrypted
        }

        //prepend previous node block
        {
            std::unique_ptr<Bpv7CanonicalBlock> blockPtr = boost::make_unique<Bpv7PreviousNodeCanonicalBlock>();
            Bpv7PreviousNodeCanonicalBlock & block = *(reinterpret_cast<Bpv7PreviousNodeCanonicalBlock*>(blockPtr.get()));
            //block.SetZero();

            block.m_blockProcessingControlFlags = BPV7_BLOCKFLAG::REMOVE_BLOCK_IF_IT_CANT_BE_PROCESSED; //something for checking against
            block.m_blockNumber = 2;
            block.m_crcType = crcTypeToUse;
            block.m_previousNode.Set(PREVIOUS_NODE, PREVIOUS_SVC);
            bv.PrependMoveCanonicalBlock(blockPtr);
        }

        BOOST_REQUIRE(bv.RenderInPlace(bundleSerializedPadded.get_allocator().PADDING_ELEMENTS_BEFORE));

        //reload new bundle
        {
            uint8_t * newStartPtr = (uint8_t *)bv.m_renderedBundle.data();
            std::size_t newSize = bv.m_renderedBundle.size();
            std::vector<uint8_t> newBundleWithPrependedBlock(newStartPtr, newStartPtr + newSize);
            BOOST_REQUIRE(bundleSerializedOriginal != newBundleWithPrependedBlock);
            bv.Reset();
            BOOST_REQUIRE(bv.LoadBundle(newStartPtr, newSize));
        }

        //get previous node, mark for deletion
        {
            std::vector<BundleViewV7::Bpv7CanonicalBlockView*> blocks;
            bv.GetCanonicalBlocksByType(BPV7_BLOCK_TYPE_CODE::PREVIOUS_NODE, blocks);
            BOOST_REQUIRE_EQUAL(blocks.size(), 1);
            Bpv7PreviousNodeCanonicalBlock* previousNodeBlockPtr = dynamic_cast<Bpv7PreviousNodeCanonicalBlock*>(blocks[0]->headerPtr.get());
            BOOST_REQUIRE(previousNodeBlockPtr);
            BOOST_REQUIRE_EQUAL(previousNodeBlockPtr->m_blockTypeCode, BPV7_BLOCK_TYPE_CODE::PREVIOUS_NODE);
            BOOST_REQUIRE_EQUAL(previousNodeBlockPtr->m_blockNumber, 2);
            BOOST_REQUIRE_EQUAL(previousNodeBlockPtr->m_previousNode.nodeId, PREVIOUS_NODE);
            BOOST_REQUIRE_EQUAL(previousNodeBlockPtr->m_previousNode.serviceId, PREVIOUS_SVC);
            BOOST_REQUIRE_EQUAL(blocks[0]->actualSerializedBlockPtr.size(), previousNodeBlockPtr->GetSerializationSize());
            BOOST_REQUIRE(!blocks[0]->isEncrypted); //not encrypted

            blocks[0]->markedForDeletion = true;
        }

        //get payload
        {
            std::vector<BundleViewV7::Bpv7CanonicalBlockView*> blocks;
            bv.GetCanonicalBlocksByType(BPV7_BLOCK_TYPE_CODE::PAYLOAD, blocks);
            BOOST_REQUIRE_EQUAL(blocks.size(), 1);
            const char * strPtr = (const char *)blocks[0]->headerPtr->m_dataPtr;
            std::string s(strPtr, strPtr + blocks[0]->headerPtr->m_dataLength);
            //std::cout << "s: " << s << "\n";
            BOOST_REQUIRE_EQUAL(s, payloadString);
            BOOST_REQUIRE_EQUAL(blocks[0]->headerPtr->m_blockTypeCode, BPV7_BLOCK_TYPE_CODE::PAYLOAD);
            BOOST_REQUIRE_EQUAL(blocks[0]->headerPtr->m_blockNumber, 1);
            BOOST_REQUIRE(!blocks[0]->isEncrypted); //not encrypted
        }

        BOOST_REQUIRE(bv.RenderInPlace(0)); //0 => expecting not to go into left padded space due to removal
        {
            uint8_t * newStartPtr = (uint8_t *)bv.m_renderedBundle.data();
            std::size_t newSize = bv.m_renderedBundle.size();
            std::vector<uint8_t> newOriginalBundle(newStartPtr, newStartPtr + newSize);
            BOOST_REQUIRE(bundleSerializedOriginal == newOriginalBundle);
        }
    }
}

BOOST_AUTO_TEST_CASE(Bpv7BundleStatusReportTestCase)
{

    const std::vector<BPV7_CRC_TYPE> crcTypesVec = { BPV7_CRC_TYPE::NONE, BPV7_CRC_TYPE::CRC16_X25, BPV7_CRC_TYPE::CRC32C };
    for (std::size_t crcI = 0; crcI < crcTypesVec.size(); ++crcI) {
        const BPV7_CRC_TYPE crcTypeToUse = crcTypesVec[crcI];
        for (unsigned int useFragI = 0; useFragI < 2; ++useFragI) {
            const bool useFrag = (useFragI != 0);
            for (unsigned int timeFlagSetI = 0; timeFlagSetI < 2; ++timeFlagSetI) {
                const bool useReportStatusTime = (timeFlagSetI != 0);
                for (unsigned int assertionsMask = 1; assertionsMask < 16; ++assertionsMask) { //start at 1 because you must assert at least one of the four items
                    const bool assert0 = ((assertionsMask & 1) != 0);
                    const bool assert1 = ((assertionsMask & 2) != 0);
                    const bool assert2 = ((assertionsMask & 4) != 0);
                    const bool assert3 = ((assertionsMask & 8) != 0);
                    //std::cout << assert3 << assert2 << assert1 << assert0 << "\n";
                    BundleViewV7 bv;
                    Bpv7CbhePrimaryBlock & primary = bv.m_primaryBlockView.header;
                    primary.SetZero();


                    primary.m_bundleProcessingControlFlags = BPV7_BUNDLEFLAG::NOFRAGMENT | BPV7_BUNDLEFLAG::ADMINRECORD;
                    primary.m_sourceNodeId.Set(PRIMARY_SRC_NODE, PRIMARY_SRC_SVC);
                    primary.m_destinationEid.Set(PRIMARY_DEST_NODE, PRIMARY_DEST_SVC);
                    primary.m_reportToEid.Set(0, 0);
                    primary.m_creationTimestamp.millisecondsSinceStartOfYear2000 = PRIMARY_TIME;
                    primary.m_lifetimeMilliseconds = PRIMARY_LIFETIME;
                    primary.m_creationTimestamp.sequenceNumber = PRIMARY_SEQ;
                    primary.m_crcType = crcTypeToUse;
                    bv.m_primaryBlockView.SetManuallyModified();

                    uint64_t bsrSerializationSize = 0;
                    //add bundle status report payload block
                    {
                        std::unique_ptr<Bpv7CanonicalBlock> blockPtr = boost::make_unique<Bpv7AdministrativeRecord>();
                        Bpv7AdministrativeRecord & block = *(reinterpret_cast<Bpv7AdministrativeRecord*>(blockPtr.get()));

                        //block.m_blockTypeCode = BPV7_BLOCK_TYPE_CODE::PAYLOAD; //not needed because handled by Bpv7AdministrativeRecord constructor
                        block.m_blockProcessingControlFlags = BPV7_BLOCKFLAG::REMOVE_BLOCK_IF_IT_CANT_BE_PROCESSED; //something for checking against
                        //block.m_blockNumber = 1; //must be 1 //not needed because handled by Bpv7AdministrativeRecord constructor
                        block.m_crcType = crcTypeToUse;

                        block.m_adminRecordTypeCode = BPV7_ADMINISTRATIVE_RECORD_TYPE_CODE::BUNDLE_STATUS_REPORT;
                        block.m_adminRecordContentPtr = boost::make_unique<Bpv7AdministrativeRecordContentBundleStatusReport>();
                        Bpv7AdministrativeRecordContentBundleStatusReport & bsr = *(reinterpret_cast<Bpv7AdministrativeRecordContentBundleStatusReport*>(block.m_adminRecordContentPtr.get()));

                        bsr.m_reportStatusTimeFlagWasSet = useReportStatusTime;
                        //reporting-node-received-bundle
                        bsr.m_bundleStatusInfo[0].first = assert0;
                        bsr.m_bundleStatusInfo[0].second = (bsr.m_reportStatusTimeFlagWasSet) ? 10000 : 0; //time asserted, 0 if don't care

                        //reporting-node-forwarded-bundle
                        bsr.m_bundleStatusInfo[1].first = assert1;
                        bsr.m_bundleStatusInfo[1].second = (bsr.m_reportStatusTimeFlagWasSet) ? 10001 : 0; //time asserted, all don't cares

                        //reporting-node-delivered-bundle
                        bsr.m_bundleStatusInfo[2].first = assert2;
                        bsr.m_bundleStatusInfo[2].second = (bsr.m_reportStatusTimeFlagWasSet) ? 10002 : 0; //time asserted, 0 if don't care

                        //reporting-node-deleted-bundle
                        bsr.m_bundleStatusInfo[3].first = assert3;
                        bsr.m_bundleStatusInfo[3].second = (bsr.m_reportStatusTimeFlagWasSet) ? 10003 : 0; //time asserted, all don't cares

                        bsr.m_statusReportReasonCode = BPV7_STATUS_REPORT_REASON_CODE::DEPLETED_STORAGE;

                        bsr.m_sourceNodeEid.Set(PRIMARY_SRC_NODE, PRIMARY_SRC_SVC);

                        bsr.m_creationTimestamp.millisecondsSinceStartOfYear2000 = 5000;
                        bsr.m_creationTimestamp.sequenceNumber = 10;

                        bsr.m_subjectBundleIsFragment = useFrag;
                        bsr.m_optionalSubjectPayloadFragmentOffset = 200;
                        bsr.m_optionalSubjectPayloadFragmentLength = 100;

                        bsrSerializationSize = bsr.GetSerializationSize();

                        bv.AppendMoveCanonicalBlock(blockPtr);
                    }


                    BOOST_REQUIRE(bv.Render(5000));

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

                    BOOST_REQUIRE_EQUAL(primary.m_sourceNodeId, cbhe_eid_t(PRIMARY_SRC_NODE, PRIMARY_SRC_SVC));
                    BOOST_REQUIRE_EQUAL(primary.m_destinationEid, cbhe_eid_t(PRIMARY_DEST_NODE, PRIMARY_DEST_SVC));
                    BOOST_REQUIRE_EQUAL(primary.m_creationTimestamp, TimestampUtil::bpv7_creation_timestamp_t(PRIMARY_TIME, PRIMARY_SEQ));
                    BOOST_REQUIRE_EQUAL(primary.m_lifetimeMilliseconds, PRIMARY_LIFETIME);
                    BOOST_REQUIRE_EQUAL(bv.m_primaryBlockView.actualSerializedPrimaryBlockPtr.size(), primary.GetSerializationSize());
                    BOOST_REQUIRE_EQUAL(primary.m_bundleProcessingControlFlags, BPV7_BUNDLEFLAG::NOFRAGMENT | BPV7_BUNDLEFLAG::ADMINRECORD);

                    BOOST_REQUIRE_EQUAL(bv.GetNumCanonicalBlocks(), 1);
                    BOOST_REQUIRE_EQUAL(bv.GetCanonicalBlockCountByType(BPV7_BLOCK_TYPE_CODE::PREVIOUS_NODE), 0);
                    BOOST_REQUIRE_EQUAL(bv.GetCanonicalBlockCountByType(BPV7_BLOCK_TYPE_CODE::BUNDLE_AGE), 0);
                    BOOST_REQUIRE_EQUAL(bv.GetCanonicalBlockCountByType(BPV7_BLOCK_TYPE_CODE::HOP_COUNT), 0);
                    BOOST_REQUIRE_EQUAL(bv.GetCanonicalBlockCountByType(BPV7_BLOCK_TYPE_CODE::PAYLOAD), 1);



                    //get bundle status report payload block
                    {
                        std::vector<BundleViewV7::Bpv7CanonicalBlockView*> blocks;
                        bv.GetCanonicalBlocksByType(BPV7_BLOCK_TYPE_CODE::PAYLOAD, blocks);
                        BOOST_REQUIRE_EQUAL(blocks.size(), 1);
                        Bpv7AdministrativeRecord* adminRecordBlockPtr = dynamic_cast<Bpv7AdministrativeRecord*>(blocks[0]->headerPtr.get());
                        BOOST_REQUIRE(adminRecordBlockPtr);
                        BOOST_REQUIRE_EQUAL(adminRecordBlockPtr->m_blockTypeCode, BPV7_BLOCK_TYPE_CODE::PAYLOAD);
                        BOOST_REQUIRE_EQUAL(adminRecordBlockPtr->m_blockNumber, 1);
                        BOOST_REQUIRE_EQUAL(adminRecordBlockPtr->m_adminRecordTypeCode, BPV7_ADMINISTRATIVE_RECORD_TYPE_CODE::BUNDLE_STATUS_REPORT);

                        BOOST_REQUIRE_EQUAL(blocks[0]->actualSerializedBlockPtr.size(), adminRecordBlockPtr->GetSerializationSize());
                        //std::cout << "adminRecordBlockPtr->GetSerializationSize() " << adminRecordBlockPtr->GetSerializationSize() << "\n";
                        BOOST_REQUIRE(!blocks[0]->isEncrypted); //not encrypted

                        Bpv7AdministrativeRecordContentBundleStatusReport * bsrPtr = dynamic_cast<Bpv7AdministrativeRecordContentBundleStatusReport*>(adminRecordBlockPtr->m_adminRecordContentPtr.get());
                        BOOST_REQUIRE(bsrPtr);

                        Bpv7AdministrativeRecordContentBundleStatusReport & bsr = *(reinterpret_cast<Bpv7AdministrativeRecordContentBundleStatusReport*>(bsrPtr));

                        BOOST_REQUIRE_EQUAL(bsr.GetSerializationSize(), bsrSerializationSize);

                        BOOST_REQUIRE_EQUAL(bsr.m_reportStatusTimeFlagWasSet, useReportStatusTime);
                        //reporting-node-received-bundle
                        BOOST_REQUIRE_EQUAL(bsr.m_bundleStatusInfo[0].first, assert0);
                        if (bsr.m_bundleStatusInfo[0].first && bsr.m_reportStatusTimeFlagWasSet) {
                            BOOST_REQUIRE_EQUAL(bsr.m_bundleStatusInfo[0].second, 10000);
                        }

                        //reporting-node-forwarded-bundle
                        BOOST_REQUIRE_EQUAL(bsr.m_bundleStatusInfo[1].first, assert1);
                        if (bsr.m_bundleStatusInfo[1].first && bsr.m_reportStatusTimeFlagWasSet) {
                            BOOST_REQUIRE_EQUAL(bsr.m_bundleStatusInfo[1].second, 10001);
                        }

                        //reporting-node-delivered-bundle
                        BOOST_REQUIRE_EQUAL(bsr.m_bundleStatusInfo[2].first, assert2);
                        if (bsr.m_bundleStatusInfo[2].first && bsr.m_reportStatusTimeFlagWasSet) {
                            BOOST_REQUIRE_EQUAL(bsr.m_bundleStatusInfo[2].second, 10002);
                        }

                        //reporting-node-deleted-bundle
                        BOOST_REQUIRE_EQUAL(bsr.m_bundleStatusInfo[3].first, assert3);
                        if (bsr.m_bundleStatusInfo[3].first && bsr.m_reportStatusTimeFlagWasSet) {
                            BOOST_REQUIRE_EQUAL(bsr.m_bundleStatusInfo[3].second, 10003);
                        }

                        BOOST_REQUIRE_EQUAL(bsr.m_statusReportReasonCode, BPV7_STATUS_REPORT_REASON_CODE::DEPLETED_STORAGE);

                        BOOST_REQUIRE_EQUAL(bsr.m_sourceNodeEid, cbhe_eid_t(PRIMARY_SRC_NODE, PRIMARY_SRC_SVC));

                        BOOST_REQUIRE_EQUAL(bsr.m_creationTimestamp.millisecondsSinceStartOfYear2000, 5000);
                        BOOST_REQUIRE_EQUAL(bsr.m_creationTimestamp.sequenceNumber, 10);

                        BOOST_REQUIRE_EQUAL(bsr.m_subjectBundleIsFragment, useFrag);
                        if (bsr.m_subjectBundleIsFragment) {
                            BOOST_REQUIRE_EQUAL(bsr.m_optionalSubjectPayloadFragmentOffset, 200);
                            BOOST_REQUIRE_EQUAL(bsr.m_optionalSubjectPayloadFragmentLength, 100);
                        }
                    }
                }
            }
        }

    }
}

BOOST_AUTO_TEST_CASE(Bpv7BibeTestCase)
{
    const std::string payloadString = { "This is the data inside the encapsulated bpv7 bundle!!!" };

    const std::vector<BPV7_CRC_TYPE> crcTypesVec = { BPV7_CRC_TYPE::NONE, BPV7_CRC_TYPE::CRC16_X25, BPV7_CRC_TYPE::CRC32C };
    for (std::size_t crcI = 0; crcI < crcTypesVec.size(); ++crcI) {
        const BPV7_CRC_TYPE crcTypeToUse = crcTypesVec[crcI];

        //create the encapsulated (inner) bundle
        std::vector<uint8_t> encapsulatedBundleVersion7;
        {
            BundleViewV7 bv;
            Bpv7CbhePrimaryBlock & primary = bv.m_primaryBlockView.header;
            primary.SetZero();


            primary.m_bundleProcessingControlFlags = BPV7_BUNDLEFLAG::NOFRAGMENT;  //All BP endpoints identified by ipn-scheme endpoint IDs are singleton endpoints.
            primary.m_sourceNodeId.Set(PRIMARY_SRC_NODE + 10, PRIMARY_SRC_SVC + 10);
            primary.m_destinationEid.Set(PRIMARY_DEST_NODE + 10, PRIMARY_DEST_SVC + 10);
            primary.m_reportToEid.Set(0, 0);
            primary.m_creationTimestamp.millisecondsSinceStartOfYear2000 = PRIMARY_TIME;
            primary.m_lifetimeMilliseconds = PRIMARY_LIFETIME;
            primary.m_creationTimestamp.sequenceNumber = PRIMARY_SEQ;
            primary.m_crcType = crcTypeToUse;
            bv.m_primaryBlockView.SetManuallyModified();


            //add only a payload block
            {

                std::unique_ptr<Bpv7CanonicalBlock> blockPtr = boost::make_unique<Bpv7CanonicalBlock>();
                Bpv7CanonicalBlock & block = *blockPtr;
                //block.SetZero();

                block.m_blockTypeCode = BPV7_BLOCK_TYPE_CODE::PAYLOAD;
                block.m_blockProcessingControlFlags = BPV7_BLOCKFLAG::REMOVE_BLOCK_IF_IT_CANT_BE_PROCESSED; //something for checking against
                block.m_blockNumber = 1; //must be 1
                block.m_crcType = crcTypeToUse;
                block.m_dataLength = payloadString.size();
                block.m_dataPtr = (uint8_t*)payloadString.data(); //payloadString must remain in scope until after render
                bv.AppendMoveCanonicalBlock(blockPtr);

            }

            BOOST_REQUIRE(bv.Render(5000));

            encapsulatedBundleVersion7.assign(bv.m_frontBuffer.begin(), bv.m_frontBuffer.end());
        }

        //create the encapsulating (outer) bundle (admin record)
        std::vector<uint8_t> encapsulatingBundleVersion7;
        uint64_t bibePduMessageSerializationSize = 0;
        {
            BundleViewV7 bv;
            Bpv7CbhePrimaryBlock & primary = bv.m_primaryBlockView.header;
            primary.SetZero();


            primary.m_bundleProcessingControlFlags = BPV7_BUNDLEFLAG::NOFRAGMENT | BPV7_BUNDLEFLAG::ADMINRECORD;
            primary.m_sourceNodeId.Set(PRIMARY_SRC_NODE, PRIMARY_SRC_SVC);
            primary.m_destinationEid.Set(PRIMARY_DEST_NODE, PRIMARY_DEST_SVC);
            primary.m_reportToEid.Set(0, 0);
            primary.m_creationTimestamp.millisecondsSinceStartOfYear2000 = PRIMARY_TIME;
            primary.m_lifetimeMilliseconds = PRIMARY_LIFETIME;
            primary.m_creationTimestamp.sequenceNumber = PRIMARY_SEQ;
            primary.m_crcType = crcTypeToUse;
            bv.m_primaryBlockView.SetManuallyModified();

            
            //add bundle status report payload block
            {
                std::unique_ptr<Bpv7CanonicalBlock> blockPtr = boost::make_unique<Bpv7AdministrativeRecord>();
                Bpv7AdministrativeRecord & block = *(reinterpret_cast<Bpv7AdministrativeRecord*>(blockPtr.get()));

                //block.m_blockTypeCode = BPV7_BLOCK_TYPE_CODE::PAYLOAD; //not needed because handled by Bpv7AdministrativeRecord constructor
                block.m_blockProcessingControlFlags = BPV7_BLOCKFLAG::REMOVE_BLOCK_IF_IT_CANT_BE_PROCESSED; //something for checking against
                //block.m_blockNumber = 1; //must be 1 //not needed because handled by Bpv7AdministrativeRecord constructor
                block.m_crcType = crcTypeToUse;

                block.m_adminRecordTypeCode = BPV7_ADMINISTRATIVE_RECORD_TYPE_CODE::BIBE_PDU;
                block.m_adminRecordContentPtr = boost::make_unique<Bpv7AdministrativeRecordContentBibePduMessage>();
                Bpv7AdministrativeRecordContentBibePduMessage & bibe = *(reinterpret_cast<Bpv7AdministrativeRecordContentBibePduMessage*>(block.m_adminRecordContentPtr.get()));

                bibe.m_transmissionId = 49;
                bibe.m_custodyRetransmissionTime = 0;
                bibe.m_encapsulatedBundlePtr = encapsulatedBundleVersion7.data();
                bibe.m_encapsulatedBundleLength = encapsulatedBundleVersion7.size();

                

                bibePduMessageSerializationSize = bibe.GetSerializationSize();

                bv.AppendMoveCanonicalBlock(blockPtr);
            }


            BOOST_REQUIRE(bv.Render(5000));
            encapsulatingBundleVersion7.assign(bv.m_frontBuffer.begin(), bv.m_frontBuffer.end());
        }

        //load the encapsulating (outer) bundle (admin record)
        std::vector<uint8_t> encapsulatingBundleVersion7ForLoad(encapsulatingBundleVersion7);
        {
            BundleViewV7 bv;
            BOOST_REQUIRE(bv.SwapInAndLoadBundle(encapsulatingBundleVersion7ForLoad));
            const Bpv7CbhePrimaryBlock & primaryOuter = bv.m_primaryBlockView.header;
            
            BOOST_REQUIRE_EQUAL(primaryOuter.m_sourceNodeId, cbhe_eid_t(PRIMARY_SRC_NODE, PRIMARY_SRC_SVC));
            BOOST_REQUIRE_EQUAL(primaryOuter.m_destinationEid, cbhe_eid_t(PRIMARY_DEST_NODE, PRIMARY_DEST_SVC));
            BOOST_REQUIRE_EQUAL(primaryOuter.m_creationTimestamp, TimestampUtil::bpv7_creation_timestamp_t(PRIMARY_TIME, PRIMARY_SEQ));
            BOOST_REQUIRE_EQUAL(primaryOuter.m_lifetimeMilliseconds, PRIMARY_LIFETIME);
            BOOST_REQUIRE_EQUAL(bv.m_primaryBlockView.actualSerializedPrimaryBlockPtr.size(), primaryOuter.GetSerializationSize());
            BOOST_REQUIRE_EQUAL(primaryOuter.m_bundleProcessingControlFlags, BPV7_BUNDLEFLAG::NOFRAGMENT | BPV7_BUNDLEFLAG::ADMINRECORD);
            BOOST_REQUIRE_EQUAL(primaryOuter.m_crcType, crcTypeToUse);

            BOOST_REQUIRE_EQUAL(bv.GetNumCanonicalBlocks(), 1);
            BOOST_REQUIRE_EQUAL(bv.GetCanonicalBlockCountByType(BPV7_BLOCK_TYPE_CODE::PREVIOUS_NODE), 0);
            BOOST_REQUIRE_EQUAL(bv.GetCanonicalBlockCountByType(BPV7_BLOCK_TYPE_CODE::BUNDLE_AGE), 0);
            BOOST_REQUIRE_EQUAL(bv.GetCanonicalBlockCountByType(BPV7_BLOCK_TYPE_CODE::HOP_COUNT), 0);
            BOOST_REQUIRE_EQUAL(bv.GetCanonicalBlockCountByType(BPV7_BLOCK_TYPE_CODE::PAYLOAD), 1);

            //get BIBE payload block (inner bundle)
            {
                std::vector<BundleViewV7::Bpv7CanonicalBlockView*> blocks;
                bv.GetCanonicalBlocksByType(BPV7_BLOCK_TYPE_CODE::PAYLOAD, blocks);
                BOOST_REQUIRE_EQUAL(blocks.size(), 1);
                Bpv7AdministrativeRecord* adminRecordBlockPtr = dynamic_cast<Bpv7AdministrativeRecord*>(blocks[0]->headerPtr.get());
                BOOST_REQUIRE(adminRecordBlockPtr);
                BOOST_REQUIRE_EQUAL(adminRecordBlockPtr->m_blockTypeCode, BPV7_BLOCK_TYPE_CODE::PAYLOAD);
                BOOST_REQUIRE_EQUAL(adminRecordBlockPtr->m_blockNumber, 1);
                BOOST_REQUIRE_EQUAL(adminRecordBlockPtr->m_adminRecordTypeCode, BPV7_ADMINISTRATIVE_RECORD_TYPE_CODE::BIBE_PDU);

                BOOST_REQUIRE_EQUAL(blocks[0]->actualSerializedBlockPtr.size(), adminRecordBlockPtr->GetSerializationSize());
                //std::cout << "adminRecordBlockPtr->GetSerializationSize() " << adminRecordBlockPtr->GetSerializationSize() << "\n";
                BOOST_REQUIRE(!blocks[0]->isEncrypted); //not encrypted

                Bpv7AdministrativeRecordContentBibePduMessage * bibePduMessagePtr = dynamic_cast<Bpv7AdministrativeRecordContentBibePduMessage*>(adminRecordBlockPtr->m_adminRecordContentPtr.get());
                BOOST_REQUIRE(bibePduMessagePtr);

                Bpv7AdministrativeRecordContentBibePduMessage & bibe = *(reinterpret_cast<Bpv7AdministrativeRecordContentBibePduMessage*>(bibePduMessagePtr));

                BOOST_REQUIRE_EQUAL(bibe.GetSerializationSize(), bibePduMessageSerializationSize);

                BOOST_REQUIRE_EQUAL(bibe.m_transmissionId, 49);
                BOOST_REQUIRE_EQUAL(bibe.m_custodyRetransmissionTime, 0);
                BOOST_REQUIRE_EQUAL(bibe.m_encapsulatedBundleLength, encapsulatedBundleVersion7.size());
                BOOST_REQUIRE_EQUAL(memcmp(bibe.m_encapsulatedBundlePtr, encapsulatedBundleVersion7.data(), bibe.m_encapsulatedBundleLength), 0);

                BundleViewV7 bvInner;
                BOOST_REQUIRE(bvInner.LoadBundle(bibe.m_encapsulatedBundlePtr, bibe.m_encapsulatedBundleLength));
                const Bpv7CbhePrimaryBlock & primaryInner = bvInner.m_primaryBlockView.header;

                BOOST_REQUIRE_EQUAL(primaryInner.m_sourceNodeId, cbhe_eid_t(PRIMARY_SRC_NODE + 10, PRIMARY_SRC_SVC + 10));
                BOOST_REQUIRE_EQUAL(primaryInner.m_destinationEid, cbhe_eid_t(PRIMARY_DEST_NODE + 10, PRIMARY_DEST_SVC + 10));
                BOOST_REQUIRE_EQUAL(primaryInner.m_creationTimestamp, TimestampUtil::bpv7_creation_timestamp_t(PRIMARY_TIME, PRIMARY_SEQ));
                BOOST_REQUIRE_EQUAL(primaryInner.m_lifetimeMilliseconds, PRIMARY_LIFETIME);
                BOOST_REQUIRE_EQUAL(bvInner.m_primaryBlockView.actualSerializedPrimaryBlockPtr.size(), primaryInner.GetSerializationSize());
                BOOST_REQUIRE_EQUAL(primaryInner.m_bundleProcessingControlFlags, BPV7_BUNDLEFLAG::NOFRAGMENT);
                BOOST_REQUIRE_EQUAL(primaryInner.m_crcType, crcTypeToUse);

                BOOST_REQUIRE_EQUAL(bvInner.GetNumCanonicalBlocks(), 1);
                BOOST_REQUIRE_EQUAL(bvInner.GetCanonicalBlockCountByType(BPV7_BLOCK_TYPE_CODE::PREVIOUS_NODE), 0);
                BOOST_REQUIRE_EQUAL(bvInner.GetCanonicalBlockCountByType(BPV7_BLOCK_TYPE_CODE::BUNDLE_AGE), 0);
                BOOST_REQUIRE_EQUAL(bvInner.GetCanonicalBlockCountByType(BPV7_BLOCK_TYPE_CODE::HOP_COUNT), 0);
                BOOST_REQUIRE_EQUAL(bvInner.GetCanonicalBlockCountByType(BPV7_BLOCK_TYPE_CODE::PAYLOAD), 1);

                //get inner bundle payload block
                {
                    std::vector<BundleViewV7::Bpv7CanonicalBlockView*> blocksInner;
                    bvInner.GetCanonicalBlocksByType(BPV7_BLOCK_TYPE_CODE::PAYLOAD, blocksInner);
                    BOOST_REQUIRE_EQUAL(blocksInner.size(), 1);
                    const char * strPtr = (const char *)blocksInner[0]->headerPtr->m_dataPtr;
                    std::string s(strPtr, strPtr + blocksInner[0]->headerPtr->m_dataLength);
                    //std::cout << "s: " << s << "\n";
                    BOOST_REQUIRE_EQUAL(s, payloadString);
                    BOOST_REQUIRE_EQUAL(blocksInner[0]->headerPtr->m_blockTypeCode, BPV7_BLOCK_TYPE_CODE::PAYLOAD);
                    BOOST_REQUIRE_EQUAL(blocksInner[0]->headerPtr->m_blockNumber, 1);
                    BOOST_REQUIRE(!blocksInner[0]->isEncrypted); //not encrypted
                }
            }
        }
    }
}
