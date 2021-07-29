#include <boost/test/unit_test.hpp>
#include <boost/thread.hpp>
#include "codec/CustodyTransferEnhancementBlock.h"
#include <iostream>
#include <string>
#include <inttypes.h>
#include <vector>

BOOST_AUTO_TEST_CASE(CustodyTransferEnhancementBlockTestCase)
{
    
    //CustodyTransferEnhancementBlock
    {
        
        const std::string eidStr = "ipn:2.3";
        std::vector<uint8_t> serialization(CustodyTransferEnhancementBlock::CBHE_MAX_SERIALIZATION_SIZE);
        CustodyTransferEnhancementBlock cteb;
        cteb.m_custodyId =  150; //size 2 sdnv
        cteb.m_ctebCreatorCustodianEidString = eidStr;
        BOOST_REQUIRE(!cteb.HasCanonicalBlockProcessingControlFlagSet(BLOCK_PROCESSING_CONTROL_FLAGS::DELETE_BUNDLE_IF_BLOCK_CANT_BE_PROCESSED));
        BOOST_REQUIRE(!cteb.HasCanonicalBlockProcessingControlFlagSet(BLOCK_PROCESSING_CONTROL_FLAGS::BLOCK_WAS_FORWARDED_WITHOUT_BEING_PROCESSED));
        cteb.AddCanonicalBlockProcessingControlFlag(BLOCK_PROCESSING_CONTROL_FLAGS::DELETE_BUNDLE_IF_BLOCK_CANT_BE_PROCESSED);
        BOOST_REQUIRE(cteb.HasCanonicalBlockProcessingControlFlagSet(BLOCK_PROCESSING_CONTROL_FLAGS::DELETE_BUNDLE_IF_BLOCK_CANT_BE_PROCESSED));
        BOOST_REQUIRE(!cteb.HasCanonicalBlockProcessingControlFlagSet(BLOCK_PROCESSING_CONTROL_FLAGS::BLOCK_WAS_FORWARDED_WITHOUT_BEING_PROCESSED));
        
        uint64_t sizeSerialized = cteb.SerializeCtebCanonicalBlock(&serialization[0]);
        uint16_t expectedSerializationSize =
            1 + //block type
            1 + //block flags sdnv +
            1 + //block length (1-byte-min-sized-sdnv) +
            2 + //custody id sdnv
            static_cast<uint16_t>(eidStr.length());
        BOOST_REQUIRE_EQUAL(sizeSerialized, expectedSerializationSize);
        CustodyTransferEnhancementBlock cteb2;
        uint16_t numBytesTakenToDecode = cteb2.DeserializeCtebCanonicalBlock(serialization.data());
        BOOST_REQUIRE_NE(numBytesTakenToDecode, 0);
        BOOST_REQUIRE_EQUAL(numBytesTakenToDecode, expectedSerializationSize);
        BOOST_REQUIRE(cteb == cteb2);


        //misc
        BOOST_REQUIRE(!(cteb != cteb2));
        CustodyTransferEnhancementBlock ctebCopy = cteb;
        CustodyTransferEnhancementBlock ctebCopy2(cteb);
        CustodyTransferEnhancementBlock cteb2Moved = std::move(cteb2);
        BOOST_REQUIRE(cteb != cteb2); //cteb2 moved
        BOOST_REQUIRE(cteb == cteb2Moved);
        BOOST_REQUIRE(cteb == ctebCopy);
        BOOST_REQUIRE(cteb == ctebCopy2);
        CustodyTransferEnhancementBlock cteb2Moved2(std::move(cteb2Moved));
        BOOST_REQUIRE(cteb != cteb2Moved); //cteb2 moved
        BOOST_REQUIRE(cteb == cteb2Moved2);
    }



}

