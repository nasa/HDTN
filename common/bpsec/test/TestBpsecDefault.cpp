/**
 * @file TestBpsecDefaultSecurityContexts.cpp
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
#include "codec/BundleViewV7.h"
#include <iostream>
#include <string>
#include <inttypes.h>
#include <vector>
#include "Uri.h"
#include <boost/make_unique.hpp>
#include "PaddedVectorUint8.h"
#include "BinaryConversions.h"
#include <boost/algorithm/string.hpp>
#include "../include/BPSecManager.h"
#include <boost/algorithm/hex.hpp>
#include <iostream>
#include <string>
#include <boost/algorithm/string.hpp>

using std::string;
using std::vector;
using std::cout;
using std::endl;
using namespace boost::algorithm;

BOOST_AUTO_TEST_CASE(TestBpsecDefaultSecurityContextsIntegrityTestCase)
{
    //Add Primary block
    BundleViewV7 bv;
    Bpv7CbhePrimaryBlock & primary = bv.m_primaryBlockView.header;
    primary.SetZero();
    primary.m_destinationEid.Set(1, 2);
    primary.m_sourceNodeId.Set(2, 1);
    primary.m_reportToEid.Set(2, 1);
    primary.m_creationTimestamp.millisecondsSinceStartOfYear2000 = 0;
    primary.m_creationTimestamp.sequenceNumber = 40;
    primary.m_lifetimeMilliseconds = 1000000;
    bv.m_primaryBlockView.SetManuallyModified();
    {
        std::vector<uint8_t> expectedSerializedPrimary;
        static const std::string expectedSerializedPrimaryString(
            "88070000820282010282028202018202820201820018281a000f4240"
        );
        BOOST_REQUIRE(BinaryConversions::HexStringToBytes(expectedSerializedPrimaryString, expectedSerializedPrimary));
        std::vector<uint8_t> actualSerializedPrimary(500);
        const uint64_t actualSerializedPrimarySize = primary.SerializeBpv7(actualSerializedPrimary.data());
        BOOST_REQUIRE_GT(actualSerializedPrimarySize, 0);
        actualSerializedPrimary.resize(actualSerializedPrimarySize);
        BOOST_REQUIRE(expectedSerializedPrimary == actualSerializedPrimary);
        //double check BinaryConversions utility BytesToHexString
        std::string actualHex;
        BinaryConversions::BytesToHexString(actualSerializedPrimary, actualHex);
        boost::to_lower(actualHex);
        BOOST_REQUIRE_EQUAL(actualHex, expectedSerializedPrimaryString);
    }

    //Add Payload Block
    static const std::string payloadString("Ready to generate a 32-byte payload");
    {

        std::unique_ptr<Bpv7CanonicalBlock> blockPtr = boost::make_unique<Bpv7CanonicalBlock>();
        Bpv7CanonicalBlock & block = *blockPtr;
        //block.SetZero();

        block.m_blockTypeCode = BPV7_BLOCK_TYPE_CODE::PAYLOAD;
        block.m_blockProcessingControlFlags = BPV7_BLOCKFLAG::NO_FLAGS_SET; //something for checking against
        block.m_blockNumber = 1; //must be 1
        block.m_crcType = BPV7_CRC_TYPE::NONE;

        block.m_dataLength = payloadString.size();
        block.m_dataPtr = (uint8_t*)payloadString.data(); //payloadString must remain in scope until after render
        bv.AppendMoveCanonicalBlock(blockPtr);
        BOOST_REQUIRE(bv.Render(500));
    }

    //get payload and verify
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

        //verify from example
        std::vector<uint8_t> expectedSerializedPayloadBlock;
        static const std::string expectedSerializedPayloadBlockString(
            "85010100005823526561647920746f2067656e657261746520612033322d62797465207061796c6f6164");
        BOOST_REQUIRE(BinaryConversions::HexStringToBytes(expectedSerializedPayloadBlockString, expectedSerializedPayloadBlock));
        uint8_t * ptrPayloadBlockSerialized = (uint8_t *)blocks[0]->actualSerializedBlockPtr.data();
        std::vector<uint8_t> actualSerializedPayloadBlock(ptrPayloadBlockSerialized, ptrPayloadBlockSerialized + blocks[0]->actualSerializedBlockPtr.size());
        BOOST_REQUIRE(expectedSerializedPayloadBlock == actualSerializedPayloadBlock);
        //double check BinaryConversions utility BytesToHexString
        std::string actualHex;
        BinaryConversions::BytesToHexString(actualSerializedPayloadBlock, actualHex);
        boost::to_lower(actualHex);
        BOOST_REQUIRE_EQUAL(actualHex, expectedSerializedPayloadBlockString);
    }

    {
        std::vector<uint8_t> expectedSerializedBundle;
        static const std::string expectedSerializedBundleString("9f88070000820282010282028202018202820201820"
			"018281a000f424085010100005823526561647920746f2067656e657261746"
			"520612033322d62797465207061796c6f6164ff");
        BOOST_REQUIRE(BinaryConversions::HexStringToBytes(expectedSerializedBundleString, expectedSerializedBundle));
        BOOST_REQUIRE_EQUAL(bv.m_renderedBundle.size(), bv.m_frontBuffer.size());
        BOOST_REQUIRE_EQUAL(expectedSerializedBundle.size(), bv.m_frontBuffer.size());
        BOOST_REQUIRE(expectedSerializedBundle == bv.m_frontBuffer); //front buffer valid because render() was called above
        //double check BinaryConversions utility BytesToHexString
        std::string actualHex;
        BinaryConversions::BytesToHexString(bv.m_frontBuffer, actualHex);
        boost::to_lower(actualHex);
        BOOST_REQUIRE_EQUAL(actualHex, expectedSerializedBundleString);
    }

    //Security Operations

    //Compute HMAC Hash
    BPSecManager bpsecHdtn(true);
    std::cout << "Calling function to compute HMAC for Payload Target block \n";
    unsigned char md[1024];
    memset(md, '\0', 1024);
    uint64_t md_len;
    static const std::string Key(
        "1a2b1a2b1a2b1a2b1a2b1a2b1a2b1a2b"
    );
    static const std::string ippt("005823526561647920746f2067656e65"
                                  "7261746520612033322d627974652070"
                                  "61796c6f6164");
    bpsecHdtn.hmac_sha(EVP_sha512(),
                       unhex(Key), unhex(ippt), md,
                       (unsigned int*) &md_len);

    //HMAC (Result)
    std::string mdStr(md, md + md_len);
    std::string HEXmd = hex(mdStr);
    boost::to_lower(HEXmd);
    std::cout << "HMAC " << HEXmd << '\n';
    std::vector<uint8_t> resultHMAC;
    BinaryConversions::HexStringToBytes(HEXmd, resultHMAC);

    //Add BIB
    {
        std::unique_ptr<Bpv7CanonicalBlock> blockPtr = boost::make_unique<Bpv7BlockIntegrityBlock>();
        Bpv7BlockIntegrityBlock & bib = *(reinterpret_cast<Bpv7BlockIntegrityBlock*>(blockPtr.get()));
        //bib.SetZero();
        bib.m_blockProcessingControlFlags = BPV7_BLOCKFLAG::NO_FLAGS_SET;
        bib.m_blockNumber = 2;
        bib.m_crcType = BPV7_CRC_TYPE::NONE;
        bib.m_securityTargets = Bpv7AbstractSecurityBlock::security_targets_t({ 1 });
        //bib.m_securityContextId = static_cast<uint64_t>(BPSEC_SECURITY_CONTEXT_IDENTIFIERS::BIB_HMAC_SHA2); //handled by constructor
        bib.m_securityContextFlags = 0;
        bib.SetSecurityContextParametersPresent();
        bib.m_securitySource.Set(2, 1);
        BOOST_REQUIRE(bib.AddOrUpdateSecurityParameterShaVariant(COSE_ALGORITHMS::HMAC_512_512));
        BOOST_REQUIRE(bib.AddSecurityParameterIntegrityScope(BPSEC_BIB_HMAX_SHA2_INTEGRITY_SCOPE_MASKS::NO_ADDITIONAL_SCOPE));
        std::vector<uint8_t> * result1Ptr = bib.AppendAndGetExpectedHmacPtr();
        BOOST_REQUIRE(result1Ptr);
        *result1Ptr = resultHMAC; //use std::move in production
        bv.PrependMoveCanonicalBlock(blockPtr);
        BOOST_REQUIRE(bv.Render(5000));
    }

    //get BIB and verify
    Bpv7BlockIntegrityBlock* bibPtrOriginal;
    {
        std::vector<BundleViewV7::Bpv7CanonicalBlockView*> blocks;
        bv.GetCanonicalBlocksByType(BPV7_BLOCK_TYPE_CODE::INTEGRITY, blocks);
        BOOST_REQUIRE_EQUAL(blocks.size(), 1);
        bibPtrOriginal = dynamic_cast<Bpv7BlockIntegrityBlock*>(blocks[0]->headerPtr.get());
        BOOST_REQUIRE(bibPtrOriginal);
        BOOST_REQUIRE(*bibPtrOriginal == *bibPtrOriginal);

        //verify from example
        std::vector<uint8_t> expectedSerializedBib;
        static const std::string expectedSerializedBibString(
                        "810101018202820201828201078203008181820158403bdc69b3a34a2b5d3a8554368b"
                        "d1e808f606219d2a10a846eae3886ae4ecc83c4ee550fdfb1cc636b904e2f1a73e303dcd4b6ccece003"
                        "e95e8164dcc89a156e1");
        BOOST_REQUIRE(BinaryConversions::HexStringToBytes(expectedSerializedBibString, expectedSerializedBib));
        std::vector<uint8_t> actualSerializedBib(bibPtrOriginal->m_dataPtr,
			bibPtrOriginal->m_dataPtr + bibPtrOriginal->m_dataLength);
	BOOST_REQUIRE(expectedSerializedBib == actualSerializedBib);
        //double check BinaryConversions utility BytesToHexString
        std::string actualHex;
        BinaryConversions::BytesToHexString(actualSerializedBib, actualHex);
        boost::to_lower(actualHex);
        BOOST_REQUIRE_EQUAL(actualHex, expectedSerializedBibString);
        //verify whole bib block from example
        std::vector<uint8_t> expectedSerializedBibBlock;
        static const std::string expectedSerializedBibBlockString(
                         "850b0200005856810101018202820201828201078203008181820158403bdc69b3a34a"
                         "2b5d3a8554368bd1e808f606219d2a10a846eae3886ae4ecc83c4ee550fdfb1cc636b904e2f1a73e303"
                         "dcd4b6ccece003e95e8164dcc89a156e1");
        BOOST_REQUIRE(BinaryConversions::HexStringToBytes(expectedSerializedBibBlockString, expectedSerializedBibBlock));
        uint8_t * ptrBibBlockSerialized = (uint8_t *)blocks[0]->actualSerializedBlockPtr.data();
        std::vector<uint8_t> actualSerializedBibBlock(ptrBibBlockSerialized,
			ptrBibBlockSerialized + blocks[0]->actualSerializedBlockPtr.size());
        BOOST_REQUIRE(expectedSerializedBibBlock == actualSerializedBibBlock);
        //double check BinaryConversions utility BytesToHexString
        actualHex.clear();
        BinaryConversions::BytesToHexString(actualSerializedBibBlock, actualHex);
        boost::to_lower(actualHex);
        BOOST_REQUIRE_EQUAL(actualHex, expectedSerializedBibBlockString);
    }

    //Final bundle
    {
        std::vector<uint8_t> expectedSerializedBundle;
        static const std::string expectedSerializedBundleString(
            "9f88070000820282010282028202018202820201820018281a000f4240850b0200"
            "005856810101018202820201828201078203008181820158403bdc69b3a34a2b5d3a"
            "8554368bd1e808f606219d2a10a846eae3886ae4ecc83c4ee550fdfb1cc636b904e2"
            "f1a73e303dcd4b6ccece003e95e8164dcc89a156e185010100005823526561647920"
            "746f2067656e657261746520612033322d62797465207061796c6f6164ff");
        BOOST_REQUIRE(BinaryConversions::HexStringToBytes(expectedSerializedBundleString, expectedSerializedBundle));
        BOOST_REQUIRE_EQUAL(bv.m_renderedBundle.size(), bv.m_frontBuffer.size());
        BOOST_REQUIRE_EQUAL(expectedSerializedBundle.size(), bv.m_frontBuffer.size());
        BOOST_REQUIRE(expectedSerializedBundle == bv.m_frontBuffer); //front buffer valid because render() was called above
        //double check BinaryConversions utility BytesToHexString
        std::string actualHex;
        BinaryConversions::BytesToHexString(bv.m_frontBuffer, actualHex);
        boost::to_lower(actualHex);
        BOOST_REQUIRE_EQUAL(actualHex, expectedSerializedBundleString);
        //load bundle to test deserialize
        {
            BundleViewV7 bv2;
            std::vector<uint8_t> toSwapIn(expectedSerializedBundle);
            BOOST_REQUIRE(bv2.SwapInAndLoadBundle(toSwapIn)); //swap in a copy
            //get bib and verify
            {
                std::vector<BundleViewV7::Bpv7CanonicalBlockView*> blocks2;
                bv2.GetCanonicalBlocksByType(BPV7_BLOCK_TYPE_CODE::INTEGRITY, blocks2);
                BOOST_REQUIRE_EQUAL(blocks2.size(), 1);
                Bpv7BlockIntegrityBlock* bibPtr2 = dynamic_cast<Bpv7BlockIntegrityBlock*>(blocks2[0]->headerPtr.get());
                BOOST_REQUIRE(bibPtr2);
                BOOST_REQUIRE(*bibPtrOriginal == *bibPtr2);
                BOOST_REQUIRE_EQUAL(bibPtr2->m_blockTypeCode, BPV7_BLOCK_TYPE_CODE::INTEGRITY);
                BOOST_REQUIRE_EQUAL(bibPtr2->m_blockNumber, 2);
                std::vector<std::vector<uint8_t>*> hmacPtrs = bibPtr2->GetAllExpectedHmacPtrs();
                BOOST_REQUIRE_EQUAL(hmacPtrs.size(), 1);
                BOOST_REQUIRE(resultHMAC == *(hmacPtrs[0]));
                //rerender
                blocks2[0]->SetManuallyModified();
                bv2.m_primaryBlockView.SetManuallyModified();
                BOOST_REQUIRE(bv2.Render(5000));
                BOOST_REQUIRE(expectedSerializedBundle == bv2.m_frontBuffer); //front buffer valid because render() was called above
            }
        }
    }
}

BOOST_AUTO_TEST_CASE(TestBpsecDefaultSecurityContextsSimpleConfidentialityTestCase)
{
    //Primary block	
    BundleViewV7 bv;
    Bpv7CbhePrimaryBlock & primary = bv.m_primaryBlockView.header;
    primary.SetZero();
    primary.m_destinationEid.Set(1, 2);
    primary.m_sourceNodeId.Set(2, 1);
    primary.m_reportToEid.Set(2, 1);
    primary.m_creationTimestamp.millisecondsSinceStartOfYear2000 = 0;
    primary.m_creationTimestamp.sequenceNumber = 40;
    primary.m_lifetimeMilliseconds = 1000000;
    bv.m_primaryBlockView.SetManuallyModified();
    {
        std::vector<uint8_t> expectedSerializedPrimary;
        static const std::string expectedSerializedPrimaryString(
            "88070000820282010282028202018202820201820018281a000f4240" //no changes in rfc
        );
        BOOST_REQUIRE(BinaryConversions::HexStringToBytes(expectedSerializedPrimaryString, expectedSerializedPrimary));
        std::vector<uint8_t> actualSerializedPrimary(500);
        const uint64_t actualSerializedPrimarySize = primary.SerializeBpv7(actualSerializedPrimary.data());
        BOOST_REQUIRE_GT(actualSerializedPrimarySize, 0);
        actualSerializedPrimary.resize(actualSerializedPrimarySize);
        BOOST_REQUIRE(expectedSerializedPrimary == actualSerializedPrimary);
        //double check BinaryConversions utility BytesToHexString
        std::string actualHex;
        BinaryConversions::BytesToHexString(actualSerializedPrimary, actualHex);
        boost::to_lower(actualHex);
        BOOST_REQUIRE_EQUAL(actualHex, expectedSerializedPrimaryString);
    }


    //add payload block
    static const std::string payloadString("Ready to generate a 32-byte payload");
    {

        std::unique_ptr<Bpv7CanonicalBlock> blockPtr = boost::make_unique<Bpv7CanonicalBlock>();
        Bpv7CanonicalBlock & block = *blockPtr;
        //block.SetZero();

        block.m_blockTypeCode = BPV7_BLOCK_TYPE_CODE::PAYLOAD;
        block.m_blockProcessingControlFlags = BPV7_BLOCKFLAG::NO_FLAGS_SET; //something for checking against
        block.m_blockNumber = 1; //must be 1
        block.m_crcType = BPV7_CRC_TYPE::NONE;

        block.m_dataLength = payloadString.size();
        block.m_dataPtr = (uint8_t*)payloadString.data(); //payloadString must remain in scope until after render
        bv.AppendMoveCanonicalBlock(blockPtr);
        BOOST_REQUIRE(bv.Render(500));
    }
    
    //get payload and verify
    {
        std::vector<BundleViewV7::Bpv7CanonicalBlockView*> blocks;
        bv.GetCanonicalBlocksByType(BPV7_BLOCK_TYPE_CODE::PAYLOAD, blocks);
        BOOST_REQUIRE_EQUAL(blocks.size(), 1);
        const char * strPtr = (const char *)blocks[0]->headerPtr->m_dataPtr;
        std::string s(strPtr, strPtr + blocks[0]->headerPtr->m_dataLength);
        std::cout << "Payload block: " << s << "\n";
        BOOST_REQUIRE_EQUAL(s, payloadString);
        BOOST_REQUIRE_EQUAL(blocks[0]->headerPtr->m_blockTypeCode, BPV7_BLOCK_TYPE_CODE::PAYLOAD);
        BOOST_REQUIRE_EQUAL(blocks[0]->headerPtr->m_blockNumber, 1);
        BOOST_REQUIRE_EQUAL(blocks[0]->actualSerializedBlockPtr.size(), blocks[0]->headerPtr->GetSerializationSize());

        //verify from example
        std::vector<uint8_t> expectedSerializedPayloadBlock;
        static const std::string expectedSerializedPayloadBlockString(
            "85010100005823526561647920746f2067656e657261746520612033322d62797465207061796c6f6164"
        );
        BOOST_REQUIRE(BinaryConversions::HexStringToBytes(expectedSerializedPayloadBlockString, expectedSerializedPayloadBlock));
        uint8_t * ptrPayloadBlockSerialized = (uint8_t *)blocks[0]->actualSerializedBlockPtr.data();
        std::vector<uint8_t> actualSerializedPayloadBlock(ptrPayloadBlockSerialized, ptrPayloadBlockSerialized + blocks[0]->actualSerializedBlockPtr.size());
        BOOST_REQUIRE(expectedSerializedPayloadBlock == actualSerializedPayloadBlock);
        //double check BinaryConversions utility BytesToHexString
        std::string actualHex;
        BinaryConversions::BytesToHexString(actualSerializedPayloadBlock, actualHex);
        boost::to_lower(actualHex);
        BOOST_REQUIRE_EQUAL(actualHex, expectedSerializedPayloadBlockString);
    }

    //CBOR encoding 
    {
        std::vector<uint8_t> expectedSerializedBundle;
        static const std::string expectedSerializedBundleString(
            "9f88070000820282010282028202018202820201820018281a000f424085010100"
            "005823526561647920746f2067656e657261746520612033322d6279746520706179"
            "6c6f6164ff"
        );
        BOOST_REQUIRE(BinaryConversions::HexStringToBytes(expectedSerializedBundleString, expectedSerializedBundle));
        BOOST_REQUIRE_EQUAL(bv.m_renderedBundle.size(), bv.m_frontBuffer.size());
        BOOST_REQUIRE_EQUAL(expectedSerializedBundle.size(), bv.m_frontBuffer.size());
        BOOST_REQUIRE(expectedSerializedBundle == bv.m_frontBuffer); //front buffer valid because render() was called above
        //double check BinaryConversions utility BytesToHexString
        std::string actualHex;
        BinaryConversions::BytesToHexString(bv.m_frontBuffer, actualHex);
        boost::to_lower(actualHex);
        BOOST_REQUIRE_EQUAL(actualHex, expectedSerializedBundleString);
    }

    //Security operations
    std::vector<uint8_t> expectedInitializationVector;
    static const std::string expectedInitializationVectorString(
        "5477656c7665313231323132"
    );
    BOOST_REQUIRE(BinaryConversions::HexStringToBytes(expectedInitializationVectorString, expectedInitializationVector));
    std::vector<uint8_t> expectedAesWrappedKey;
    static const std::string expectedAesWrappedKeyString(
        "69c411276fecddc4780df42c8a2af89296fabf34d7fae700"
    );
    BOOST_REQUIRE(BinaryConversions::HexStringToBytes(expectedAesWrappedKeyString, expectedAesWrappedKey));
    static const std::string Key(
        "71776572747975696f70617364666768"
    );
     
    static const std::string gcm_aad("00");

    //get payload target block to be encrypted
    std::vector<BundleViewV7::Bpv7CanonicalBlockView*> blocks;
    bv.GetCanonicalBlocksByType(BPV7_BLOCK_TYPE_CODE::PAYLOAD, blocks);
    BOOST_REQUIRE_EQUAL(blocks.size(), 1);
    const uint64_t payloadDataLength = blocks[0]->headerPtr->m_dataLength;
    const uint8_t * payloadDataPtr = blocks[0]->headerPtr->m_dataPtr;
    const char * strPtr = (const char *)blocks[0]->headerPtr->m_dataPtr;
    std::string s(strPtr, strPtr + payloadDataLength);

    //Encrypt payload data   
    BPSecManager bpsecHdtn(true);
    unsigned char ciphertext[512];
    memset(ciphertext, '\0', 512);
    int len;
    unsigned char tag[512];
    memset(tag, '\0', 512);
    std::cout << "Calling function to encrypt Payload Target block: \n";
    BOOST_REQUIRE(bpsecHdtn.aes_gcm_encrypt(s, unhex(Key), 
				            unhex(expectedInitializationVectorString), 
			                    unhex(gcm_aad), ciphertext,
                                            tag, &len));
    //Ciphertext
    std::string ciphertextStr((char*) ciphertext);
    std::string HEXCipher = hex(ciphertextStr);
    boost::to_lower(HEXCipher);
    std::cout << "Ciphertext " << HEXCipher << '\n';
    std::vector<uint8_t> payloadCipherText;
    BinaryConversions::HexStringToBytes(HEXCipher, payloadCipherText);
    //Replace payload with the ciphertext
    blocks[0]->headerPtr->m_dataPtr = payloadCipherText.data();
    blocks[0]->headerPtr->m_dataLength = payloadCipherText.size();
    blocks[0]->SetManuallyModified();
    BOOST_REQUIRE(bv.Render(5000));

    //Authentication tag (Result)
    static const std::string tagStr((char*) tag);
    std::string HEX = hex(tagStr);
    boost::to_lower(HEX);
    std::cout << "Authentication Tag " << HEX << '\n';
    std::vector<uint8_t> tagHex;
    BinaryConversions::HexStringToBytes(HEX, tagHex);

    //Test Decryption
    unsigned char plaintext[512];
    memset(plaintext, '\0', 512);
    int outlen;
    bpsecHdtn.aes_gcm_decrypt(unhex(HEXCipher), unhex(HEX),
		              unhex(Key),
                              unhex(expectedInitializationVectorString),
                              unhex(gcm_aad), plaintext, &outlen);
     
    static const std::string ptStr((char*) plaintext);
    std::cout << "Decrypted text: " << ptStr << "\n";
    BOOST_REQUIRE_EQUAL(ptStr, s);

    { 
        std::unique_ptr<Bpv7CanonicalBlock> blockPtr = boost::make_unique<Bpv7BlockConfidentialityBlock>();
        Bpv7BlockConfidentialityBlock & bcb = *(reinterpret_cast<Bpv7BlockConfidentialityBlock*>(blockPtr.get()));
        bcb.m_blockProcessingControlFlags = BPV7_BLOCKFLAG::MUST_BE_REPLICATED;
        bcb.m_blockNumber = 2;
        bcb.m_crcType = BPV7_CRC_TYPE::NONE;
        bcb.m_securityTargets = Bpv7AbstractSecurityBlock::security_targets_t({ 1 });
        //bcb.m_securityContextId = static_cast<uint64_t>(BPSEC_SECURITY_CONTEXT_IDENTIFIERS::BCB_AES_GCM); //handled by constructor
        bcb.m_securityContextFlags = 0;
        bcb.SetSecurityContextParametersPresent();
        bcb.m_securitySource.Set(2, 1);
        std::vector<uint8_t> * ivPtr = bcb.AddAndGetInitializationVectorPtr();
        BOOST_REQUIRE(ivPtr);
        *ivPtr = expectedInitializationVector; //use std::move in production
        BOOST_REQUIRE(bcb.AddOrUpdateSecurityParameterAesVariant(COSE_ALGORITHMS::A128GCM));
        std::vector<uint8_t> * aesWrappedKeyPtr = bcb.AddAndGetAesWrappedKeyPtr();
        BOOST_REQUIRE(aesWrappedKeyPtr);
        *aesWrappedKeyPtr = expectedAesWrappedKey; //use std::move in production
        BOOST_REQUIRE(bcb.AddSecurityParameterScope(BPSEC_BCB_AES_GCM_AAD_SCOPE_MASKS::NO_ADDITIONAL_SCOPE));
        std::vector<uint8_t> * result1Ptr = bcb.AppendAndGetPayloadAuthenticationTagPtr();
        BOOST_REQUIRE(result1Ptr);
        *result1Ptr = tagHex;
        bv.PrependMoveCanonicalBlock(blockPtr);
        BOOST_REQUIRE(bv.Render(5000));
    }

    //get bcb and verify
    Bpv7BlockConfidentialityBlock* bcbPtrOriginal;
    {
        std::vector<BundleViewV7::Bpv7CanonicalBlockView*> blocks;
        bv.GetCanonicalBlocksByType(BPV7_BLOCK_TYPE_CODE::CONFIDENTIALITY, blocks);
        BOOST_REQUIRE_EQUAL(blocks.size(), 1);
        bcbPtrOriginal = dynamic_cast<Bpv7BlockConfidentialityBlock*>(blocks[0]->headerPtr.get());
        BOOST_REQUIRE(bcbPtrOriginal);
        BOOST_REQUIRE(*bcbPtrOriginal == *bcbPtrOriginal);
        //verify from example
        std::vector<uint8_t> expectedSerializedBcb;
        static const std::string expectedSerializedBcbString(
            "8101020182028202018482014c5477656c76653132313231328202018203581869"
            "c411276fecddc4780df42c8a2af89296fabf34d7fae7008204008181820150efa4b5"
            "ac0108e3816c5606479801bc04"
        );
        BOOST_REQUIRE(BinaryConversions::HexStringToBytes(expectedSerializedBcbString, expectedSerializedBcb));
        std::vector<uint8_t> actualSerializedBcb(bcbPtrOriginal->m_dataPtr, 
			                         bcbPtrOriginal->m_dataPtr + bcbPtrOriginal->m_dataLength);
        BOOST_REQUIRE(expectedSerializedBcb == actualSerializedBcb);
        //double check BinaryConversions utility BytesToHexString
        std::string actualHex;
        BinaryConversions::BytesToHexString(actualSerializedBcb, actualHex);
        boost::to_lower(actualHex);
	//std::cout << "actual serialized bcb " << actualHex << std::endl;
	BOOST_REQUIRE_EQUAL(actualHex, expectedSerializedBcbString);
        //verify from example
        std::vector<uint8_t> expectedSerializedBcbBlock;
        static const std::string expectedSerializedBcbBlockString(
            "850c02010058508101020182028202018482014c5477656c766531323132313282"
            "02018203581869c411276fecddc4780df42c8a2af89296fabf34d7fae70082040081"
            "81820150efa4b5ac0108e3816c5606479801bc04"
        );
        BOOST_REQUIRE(BinaryConversions::HexStringToBytes(expectedSerializedBcbBlockString, expectedSerializedBcbBlock));
        uint8_t * ptrBcbBlockSerialized = (uint8_t *)blocks[0]->actualSerializedBlockPtr.data();
        std::vector<uint8_t> actualSerializedBcbBlock(ptrBcbBlockSerialized, ptrBcbBlockSerialized + blocks[0]->actualSerializedBlockPtr.size());
        BOOST_REQUIRE(expectedSerializedBcbBlock == actualSerializedBcbBlock);
        //double check BinaryConversions utility BytesToHexString
        actualHex.clear();
        BinaryConversions::BytesToHexString(actualSerializedBcbBlock, actualHex);
        boost::to_lower(actualHex);
        BOOST_REQUIRE_EQUAL(actualHex, expectedSerializedBcbBlockString);
    }

    //Final bundle
    {
        //verify from example
        std::vector<uint8_t> expectedSerializedBundle;
        static const std::string expectedSerializedBundleString(
            "9f88070000820282010282028202018202820201820018281a000f4240850c0201"
            "0058508101020182028202018482014c5477656c7665313231323132820201820358"
            "1869c411276fecddc4780df42c8a2af89296fabf34d7fae7008204008181820150ef"
            "a4b5ac0108e3816c5606479801bc04850101000058233a09c1e63fe23a7f66a59c73"
            "03837241e070b02619fc59c5214a22f08cd70795e73e9aff"
        );
        BOOST_REQUIRE(BinaryConversions::HexStringToBytes(expectedSerializedBundleString, expectedSerializedBundle));
        BOOST_REQUIRE_EQUAL(bv.m_renderedBundle.size(), bv.m_frontBuffer.size());
        BOOST_REQUIRE_EQUAL(expectedSerializedBundle.size(), bv.m_frontBuffer.size());
        BOOST_REQUIRE(expectedSerializedBundle == bv.m_frontBuffer); //front buffer valid because render() was called above
        //double check BinaryConversions utility BytesToHexString
        std::string actualHex;
        BinaryConversions::BytesToHexString(bv.m_frontBuffer, actualHex);
        boost::to_lower(actualHex);
        BOOST_REQUIRE_EQUAL(actualHex, expectedSerializedBundleString);
	//load bundle to test deserialize
        {
            BundleViewV7 bv2;
            std::vector<uint8_t> toSwapIn(expectedSerializedBundle);
            BOOST_REQUIRE(bv2.SwapInAndLoadBundle(toSwapIn)); //swap in a copy
            //get bib and verify
            {
                std::vector<BundleViewV7::Bpv7CanonicalBlockView*> blocks2;
                bv2.GetCanonicalBlocksByType(BPV7_BLOCK_TYPE_CODE::CONFIDENTIALITY, blocks2);
                BOOST_REQUIRE_EQUAL(blocks2.size(), 1);
                Bpv7BlockConfidentialityBlock* bcbPtr2 = dynamic_cast<Bpv7BlockConfidentialityBlock*>(blocks2[0]->headerPtr.get());
                BOOST_REQUIRE(bcbPtr2);
                BOOST_REQUIRE(*bcbPtrOriginal == *bcbPtr2);
                BOOST_REQUIRE_EQUAL(bcbPtr2->m_blockTypeCode, BPV7_BLOCK_TYPE_CODE::CONFIDENTIALITY);
                BOOST_REQUIRE_EQUAL(bcbPtr2->m_blockNumber, 2);
                std::vector<std::vector<uint8_t>*> patPtrs = bcbPtr2->GetAllPayloadAuthenticationTagPtrs();
                BOOST_REQUIRE_EQUAL(patPtrs.size(), 1);
                BOOST_REQUIRE(tagHex == *(patPtrs[0]));
                //rerender
                blocks2[0]->SetManuallyModified();
                bv2.m_primaryBlockView.SetManuallyModified();
                BOOST_REQUIRE(bv2.Render(5000));
                BOOST_REQUIRE(expectedSerializedBundle == bv2.m_frontBuffer); //front buffer valid because render() was called above
            }
        }
    }
}

BOOST_AUTO_TEST_CASE(EncryptDecryptDataTestCase)
{
    static const std::string payloadString("Ready to generate a 32-byte payload");
    

    //Security operations
    std::vector<uint8_t> initializationVectorBytes;
    static const std::string initializationVectorString(
        "5477656c7665313231323132"
    );
    BOOST_REQUIRE(BinaryConversions::HexStringToBytes(initializationVectorString, initializationVectorBytes));

    std::vector<uint8_t> aesWrappedKeyBytes;
    static const std::string aesWrappedKeyString(
        "71776572747975696f70617364666768"
    );
    BOOST_REQUIRE(BinaryConversions::HexStringToBytes(aesWrappedKeyString, aesWrappedKeyBytes));

    std::vector<uint8_t> gcmAadBytes;
    static const std::string gcmAadString("00");
    BOOST_REQUIRE(BinaryConversions::HexStringToBytes(gcmAadString, gcmAadBytes));
    

    //Encrypt payload data (not in place)  
    std::vector<uint8_t> cipherTextBytes(payloadString.size() + EVP_MAX_BLOCK_LENGTH, 0);
    std::vector<uint8_t> tagBytes(EVP_GCM_TLS_TAG_LEN + 10, 'a'); //add 10 extra bytes to make sure they are unmodified
    BPSecManager::EvpCipherCtxWrapper ctxWrapper;
    uint64_t cipherTextOutSize = 0;
    //not inplace test (separate in and out buffers)
    BOOST_REQUIRE(BPSecManager::AesGcmEncrypt(ctxWrapper,
        (const uint8_t *)payloadString.data(), payloadString.size(),
        aesWrappedKeyBytes.data(), aesWrappedKeyBytes.size(),
        initializationVectorBytes.data(), initializationVectorBytes.size(),
        gcmAadBytes.data(), gcmAadBytes.size(), //affects tag only
        cipherTextBytes.data(), cipherTextOutSize, tagBytes.data()));

    cipherTextBytes.resize(cipherTextOutSize);
    std::string cipherTextHexString;
    BinaryConversions::BytesToHexString(cipherTextBytes, cipherTextHexString);
    boost::to_lower(cipherTextHexString);

    BOOST_REQUIRE_EQUAL(memcmp(&tagBytes[EVP_GCM_TLS_TAG_LEN], "aaaaaaaaaa", 10), 0); //tag should not have overrun
    tagBytes.resize(EVP_GCM_TLS_TAG_LEN);
    std::string tagHexString;
    BinaryConversions::BytesToHexString(tagBytes, tagHexString);
    boost::to_lower(tagHexString);

    //https://gchq.github.io/CyberChef/#recipe=AES_Encrypt(%7B'option':'Hex','string':'71776572747975696f70617364666768'%7D,%7B'option':'Hex','string':'5477656c7665313231323132'%7D,'GCM','Hex','Hex',%7B'option':'Hex','string':'00'%7D)&input=NTI2NTYxNjQ3OTIwNzQ2ZjIwNjc2NTZlNjU3MjYxNzQ2NTIwNjEyMDMzMzIyZDYyNzk3NDY1MjA3MDYxNzk2YzZmNjE2NA
    static const std::string expectedCipherTextHexString("3a09c1e63fe23a7f66a59c7303837241e070b02619fc59c5214a22f08cd70795e73e9a");
    static const std::string expectedTagHexString("efa4b5ac0108e3816c5606479801bc04");
    BOOST_REQUIRE_EQUAL(expectedCipherTextHexString, cipherTextHexString);
    BOOST_REQUIRE_EQUAL(expectedTagHexString, tagHexString);

    //Encrypt payload data (in place) (also reuse context)
    padded_vector_uint8_t inplaceData( //PADDING_ELEMENTS_AFTER should be more than EVP_MAX_BLOCK_LENGTH
        reinterpret_cast<const uint8_t*>(payloadString.data()),
        (reinterpret_cast<const uint8_t*>(payloadString.data())) + payloadString.size()
    );
    tagBytes.assign(EVP_GCM_TLS_TAG_LEN + 10, 'a'); //add 10 extra bytes to make sure they are unmodified
    cipherTextOutSize = 0;
    //inplace test (same in and out buffers)
    BOOST_REQUIRE(BPSecManager::AesGcmEncrypt(ctxWrapper,
        inplaceData.data(), inplaceData.size(),
        aesWrappedKeyBytes.data(), aesWrappedKeyBytes.size(),
        initializationVectorBytes.data(), initializationVectorBytes.size(),
        gcmAadBytes.data(), gcmAadBytes.size(),
        inplaceData.data(), cipherTextOutSize, tagBytes.data()));

    std::vector<uint8_t> inplaceDataEncryptedCopy(inplaceData.data(), inplaceData.data() + cipherTextOutSize); //safely go out of bounds
    std::string inplaceDataEncryptedHexString;
    BinaryConversions::BytesToHexString(inplaceDataEncryptedCopy, inplaceDataEncryptedHexString);
    boost::to_lower(inplaceDataEncryptedHexString);
    BOOST_REQUIRE_EQUAL(expectedCipherTextHexString, inplaceDataEncryptedHexString);

    BOOST_REQUIRE_EQUAL(memcmp(&tagBytes[EVP_GCM_TLS_TAG_LEN], "aaaaaaaaaa", 10), 0); //tag should not have overrun
    tagBytes.resize(EVP_GCM_TLS_TAG_LEN);
    tagHexString.clear();
    BinaryConversions::BytesToHexString(tagBytes, tagHexString);
    boost::to_lower(tagHexString);
    BOOST_REQUIRE_EQUAL(expectedTagHexString, tagHexString);


    //Decrypt payload data (not in place) (reuse context)
    {
        std::vector<uint8_t> decryptedBytes(payloadString.size() + EVP_MAX_BLOCK_LENGTH, 0);
        uint64_t decryptedDataOutSize = 0;
        //not inplace test (separate in and out buffers)
        BOOST_REQUIRE(BPSecManager::AesGcmDecrypt(ctxWrapper,
            cipherTextBytes.data(), cipherTextBytes.size(),
            aesWrappedKeyBytes.data(), aesWrappedKeyBytes.size(),
            initializationVectorBytes.data(), initializationVectorBytes.size(),
            gcmAadBytes.data(), gcmAadBytes.size(), //affects tag only
            tagBytes.data(),
            decryptedBytes.data(), decryptedDataOutSize));

        decryptedBytes.resize(decryptedDataOutSize);
        std::string decryptedString( //make a copy into a string object for comparisons
            reinterpret_cast<const char*>(decryptedBytes.data()),
            (reinterpret_cast<const char*>(decryptedBytes.data())) + decryptedBytes.size()
        );
        BOOST_REQUIRE_EQUAL(decryptedString, payloadString);
    }

    //Decrypt payload data (in place) (reuse context)
    {
        padded_vector_uint8_t inplaceDataToDecrypt( //PADDING_ELEMENTS_AFTER should be more than EVP_MAX_BLOCK_LENGTH
            cipherTextBytes.data(),
            cipherTextBytes.data() + cipherTextBytes.size()
        );
        uint64_t decryptedDataOutSize = 0;
        //not inplace test (separate in and out buffers)
        BOOST_REQUIRE(BPSecManager::AesGcmDecrypt(ctxWrapper,
            inplaceDataToDecrypt.data(), inplaceDataToDecrypt.size(),
            aesWrappedKeyBytes.data(), aesWrappedKeyBytes.size(),
            initializationVectorBytes.data(), initializationVectorBytes.size(),
            gcmAadBytes.data(), gcmAadBytes.size(), //affects tag only
            tagBytes.data(),
            inplaceDataToDecrypt.data(), decryptedDataOutSize));

        std::string decryptedStringFromInplace( //make a copy into a string object for comparisons
            reinterpret_cast<const char*>(inplaceDataToDecrypt.data()),
            (reinterpret_cast<const char*>(inplaceDataToDecrypt.data())) + decryptedDataOutSize //safely go out of bounds
        );
        BOOST_REQUIRE_EQUAL(decryptedStringFromInplace, payloadString);
    }
}
