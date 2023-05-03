/**
 * @file TestBpsecDefault.cpp
 * @author  Nadia Kortas <nadia.kortas@nasa.gov>
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
#if 0
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
        bv.AppendMoveCanonicalBlock(std::move(blockPtr));
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
        BOOST_REQUIRE_EQUAL(blocks[0]->actualSerializedBlockPtr.size(), blocks[0]->headerPtr->GetSerializationSize(false));

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
        bv.AppendMoveCanonicalBlock(std::move(blockPtr));
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
        BOOST_REQUIRE_EQUAL(blocks[0]->actualSerializedBlockPtr.size(), blocks[0]->headerPtr->GetSerializationSize(false));

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
#endif
BOOST_AUTO_TEST_CASE(EncryptDecryptDataTestCase)
{
    static const std::string payloadString("Ready to generate a 32-byte payload");
    

    //Security operations
    std::vector<uint8_t> initializationVectorBytes;
    static const std::string initializationVectorString(
        "5477656c7665313231323132"
    );
    BOOST_REQUIRE(BinaryConversions::HexStringToBytes(initializationVectorString, initializationVectorBytes));

    std::vector<uint8_t> keyBytes;
    static const std::string keyString(
        "71776572747975696f70617364666768"
    );
    BOOST_REQUIRE(BinaryConversions::HexStringToBytes(keyString, keyBytes));

    std::vector<uint8_t> keyEncryptionKeyBytes; //KEK
    static const std::string keyEncryptionKeyString(
        "6162636465666768696a6b6c6d6e6f70"
    );
    BOOST_REQUIRE(BinaryConversions::HexStringToBytes(keyEncryptionKeyString, keyEncryptionKeyBytes));

    std::vector<uint8_t> expectedAesWrappedKeyBytes;
    static const std::string expectedAesWrappedKeyString(
        "69c411276fecddc4780df42c8a2af89296fabf34d7fae700"
    );
    BOOST_REQUIRE(BinaryConversions::HexStringToBytes(expectedAesWrappedKeyString, expectedAesWrappedKeyBytes));

    BOOST_REQUIRE_EQUAL(expectedAesWrappedKeyBytes.size(), keyEncryptionKeyBytes.size() + 8);

    //wrap key
    std::vector<uint8_t> aesWrappedKeyBytes(expectedAesWrappedKeyBytes.size() + 100);
    unsigned int wrappedKeyOutSize;
    BOOST_REQUIRE(BPSecManager::AesWrapKey(
        keyEncryptionKeyBytes.data(), static_cast<const unsigned int>(keyEncryptionKeyBytes.size()),
        keyBytes.data(), static_cast<const unsigned int>(keyBytes.size()),
        aesWrappedKeyBytes.data(), wrappedKeyOutSize));
    BOOST_REQUIRE_EQUAL(wrappedKeyOutSize, expectedAesWrappedKeyBytes.size());
    aesWrappedKeyBytes.resize(wrappedKeyOutSize);
    BOOST_REQUIRE(aesWrappedKeyBytes == expectedAesWrappedKeyBytes);

    //unwrap key: https://gchq.github.io/CyberChef/#recipe=AES_Key_Unwrap(%7B'option':'Hex','string':'6162636465666768696a6b6c6d6e6f70'%7D,%7B'option':'Hex','string':'a6a6a6a6a6a6a6a6'%7D,'Hex','Hex')&input=NjljNDExMjc2ZmVjZGRjNDc4MGRmNDJjOGEyYWY4OTI5NmZhYmYzNGQ3ZmFlNzAw
    std::vector<uint8_t> unwrappedKeyBytes(keyBytes.size() + 100);
    unsigned int unwrappedKeyOutSize;
    BOOST_REQUIRE(BPSecManager::AesUnwrapKey(
        keyEncryptionKeyBytes.data(), static_cast<const unsigned int>(keyEncryptionKeyBytes.size()),
        aesWrappedKeyBytes.data(), static_cast<const unsigned int>(aesWrappedKeyBytes.size()),
        unwrappedKeyBytes.data(), unwrappedKeyOutSize));
    BOOST_REQUIRE_EQUAL(unwrappedKeyOutSize, keyBytes.size());
    unwrappedKeyBytes.resize(unwrappedKeyOutSize);
    BOOST_REQUIRE(unwrappedKeyBytes == keyBytes);

    std::vector<uint8_t> gcmAadBytes;
    static const std::string gcmAadString("00");
    BOOST_REQUIRE(BinaryConversions::HexStringToBytes(gcmAadString, gcmAadBytes));
    std::vector<boost::asio::const_buffer> aadParts;
    aadParts.emplace_back(boost::asio::buffer(gcmAadBytes));
    

    //Encrypt payload data (not in place)  
    std::vector<uint8_t> cipherTextBytes(payloadString.size() + EVP_MAX_BLOCK_LENGTH, 'b'); //paint extra bytes (should be unmodified)
    std::vector<uint8_t> tagBytes(EVP_GCM_TLS_TAG_LEN + 10, 'a'); //paint/add 10 extra bytes to make sure they are unmodified
    BPSecManager::EvpCipherCtxWrapper ctxWrapper;
    uint64_t cipherTextOutSize = 0;
    //not inplace test (separate in and out buffers)
    BOOST_REQUIRE(BPSecManager::AesGcmEncrypt(ctxWrapper,
        (const uint8_t *)payloadString.data(), payloadString.size(),
        keyBytes.data(), keyBytes.size(),
        initializationVectorBytes.data(), initializationVectorBytes.size(),
        aadParts, //affects tag only
        cipherTextBytes.data(), cipherTextOutSize, tagBytes.data()));

    BOOST_REQUIRE_EQUAL(memcmp(&cipherTextBytes[payloadString.size()],
        "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb", EVP_MAX_BLOCK_LENGTH), 0); //cipherTextBytes should not have overrun

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
    memset(inplaceData.data() + payloadString.size(), 'c', 5); //paint 5 bytes (in padding area) after payload to make sure unmodified
    tagBytes.assign(EVP_GCM_TLS_TAG_LEN + 10, 'a'); //add 10 extra bytes to make sure they are unmodified
    cipherTextOutSize = 0;
    //inplace test (same in and out buffers)
    BOOST_REQUIRE(BPSecManager::AesGcmEncrypt(ctxWrapper,
        inplaceData.data(), inplaceData.size(),
        keyBytes.data(), keyBytes.size(),
        initializationVectorBytes.data(), initializationVectorBytes.size(),
        aadParts,
        inplaceData.data(), cipherTextOutSize, tagBytes.data()));

    BOOST_REQUIRE_EQUAL(memcmp(&inplaceData[payloadString.size()],
        "cccccc", 5), 0); //inplaceData should not have overrun

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
            keyBytes.data(), keyBytes.size(),
            initializationVectorBytes.data(), initializationVectorBytes.size(),
            aadParts, //affects tag only
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
        std::vector<boost::asio::const_buffer> aadParts;
        aadParts.emplace_back(boost::asio::buffer(gcmAadBytes));
        //not inplace test (separate in and out buffers)
        BOOST_REQUIRE(BPSecManager::AesGcmDecrypt(ctxWrapper,
            inplaceDataToDecrypt.data(), inplaceDataToDecrypt.size(),
            keyBytes.data(), keyBytes.size(),
            initializationVectorBytes.data(), initializationVectorBytes.size(),
            aadParts, //affects tag only
            tagBytes.data(),
            inplaceDataToDecrypt.data(), decryptedDataOutSize));

        std::string decryptedStringFromInplace( //make a copy into a string object for comparisons
            reinterpret_cast<const char*>(inplaceDataToDecrypt.data()),
            (reinterpret_cast<const char*>(inplaceDataToDecrypt.data())) + decryptedDataOutSize //safely go out of bounds
        );
        BOOST_REQUIRE_EQUAL(decryptedStringFromInplace, payloadString);
    }
}

BOOST_AUTO_TEST_CASE(DecryptThenEncryptBundleWithKeyWrapTestCase)
{
    std::vector<uint8_t> encryptedSerializedBundle;
    static const std::string encryptedSerializedBundleString(
        "9f88070000820282010282028202018202820201820018281a000f4240850c0201"
        "0058508101020182028202018482014c5477656c7665313231323132820201820358"
        "1869c411276fecddc4780df42c8a2af89296fabf34d7fae7008204008181820150ef"
        "a4b5ac0108e3816c5606479801bc04850101000058233a09c1e63fe23a7f66a59c73"
        "03837241e070b02619fc59c5214a22f08cd70795e73e9aff"
    );
    BOOST_REQUIRE(BinaryConversions::HexStringToBytes(encryptedSerializedBundleString, encryptedSerializedBundle));
    padded_vector_uint8_t encryptedSerializedBundlePadded(
        encryptedSerializedBundle.data(),
        encryptedSerializedBundle.data() + encryptedSerializedBundle.size()
    );
    BundleViewV7 bv;
    BOOST_REQUIRE(bv.LoadBundle(encryptedSerializedBundlePadded.data(), encryptedSerializedBundlePadded.size(), false));

    //decrypt

    std::vector<uint8_t> keyEncryptionKeyBytes; //KEK
    static const std::string keyEncryptionKeyString(
        "6162636465666768696a6b6c6d6e6f70"
    );
    BOOST_REQUIRE(BinaryConversions::HexStringToBytes(keyEncryptionKeyString, keyEncryptionKeyBytes));

    std::vector<boost::asio::const_buffer> aadPartsTemporaryMemory;

    BPSecManager::EvpCipherCtxWrapper ctxWrapper;
    bool hadError;
    bool decryptionSuccessful;
    BPSecManager::TryDecryptBundle(ctxWrapper,
        bv,
        keyEncryptionKeyBytes.data(), static_cast<const unsigned int>(keyEncryptionKeyBytes.size()),
        NULL, 0, //no DEK (using KEK instead)
        aadPartsTemporaryMemory,
        hadError, decryptionSuccessful);
    BOOST_REQUIRE(!hadError);
    BOOST_REQUIRE(decryptionSuccessful);
    const std::vector<uint8_t> decryptedBundleCopy(
        (uint8_t*)bv.m_renderedBundle.data(),
        ((uint8_t*)bv.m_renderedBundle.data()) + bv.m_renderedBundle.size()
    );
    std::string decryptedBundleHexString;
    BinaryConversions::BytesToHexString(decryptedBundleCopy, decryptedBundleHexString);
    boost::to_lower(decryptedBundleHexString);
    //std::cout << "decrypted bundle: " << decryptedBundleHexString << "\n";
    static const std::string expectedSerializedBundleString(
        "9f88070000820282010282028202018202820201820018281a000f424085010100"
        "005823526561647920746f2067656e657261746520612033322d6279746520706179"
        "6c6f6164ff"
    );
    BOOST_REQUIRE_EQUAL(expectedSerializedBundleString, decryptedBundleHexString);

    { //take new bundle and encrypt
        padded_vector_uint8_t decryptedBundleCopyPadded(
            decryptedBundleCopy.data(),
            decryptedBundleCopy.data() + decryptedBundleCopy.size()
        );
        BundleViewV7 bv2;
        BOOST_REQUIRE(bv2.LoadBundle(decryptedBundleCopyPadded.data(), decryptedBundleCopyPadded.size(), false));

        std::vector<uint8_t> expectedInitializationVector;
        static const std::string expectedInitializationVectorString(
            "5477656c7665313231323132"
        );
        BOOST_REQUIRE(BinaryConversions::HexStringToBytes(expectedInitializationVectorString, expectedInitializationVector));

        std::vector<uint8_t> dataEncryptionKeyBytes; //DEK
        static const std::string dataEncryptionKeyString(
            "71776572747975696f70617364666768"
        );
        BOOST_REQUIRE(BinaryConversions::HexStringToBytes(dataEncryptionKeyString, dataEncryptionKeyBytes));

        static const uint64_t targetBlockNumbers[1] = { 1 };

        const uint64_t insertBcbBeforeThisBlockNumber = 1;
        bool encryptionSuccessful;
        BPSecManager::TryEncryptBundle(ctxWrapper,
            bv2,
            BPSEC_BCB_AES_GCM_AAD_SCOPE_MASKS::NO_ADDITIONAL_SCOPE,
            COSE_ALGORITHMS::A128GCM,
            BPV7_CRC_TYPE::NONE,
            cbhe_eid_t(2, 1),
            targetBlockNumbers, 1,
            expectedInitializationVector.data(), static_cast<unsigned int>(expectedInitializationVector.size()),
            keyEncryptionKeyBytes.data(), static_cast<unsigned int>(keyEncryptionKeyBytes.size()), //NULL if not present (for wrapping DEK only)
            dataEncryptionKeyBytes.data(), static_cast<unsigned int>(dataEncryptionKeyBytes.size()), //NULL if not present (when no wrapped key is present)
            aadPartsTemporaryMemory,
            &insertBcbBeforeThisBlockNumber,
            hadError, encryptionSuccessful);

        BOOST_REQUIRE(!hadError);
        BOOST_REQUIRE(encryptionSuccessful);

        {
            std::string actualHex;
            BinaryConversions::BytesToHexString(bv2.m_renderedBundle, actualHex);
            boost::to_lower(actualHex);
            BOOST_REQUIRE_EQUAL(actualHex, encryptedSerializedBundleString);
            //std::cout << "decrypted bundle: " << actualHex << "\n";
        }
    }
}

//from TestBpsecDefaultSecurityContextsSecurityBlocksWithFullScopeTestCase
BOOST_AUTO_TEST_CASE(DecryptThenEncryptBundleFullScopeTestCase)
{
    std::vector<uint8_t> encryptedSerializedBundle;
    static const std::string encryptedSerializedBundleString(
        "9f88070000820282010282028202018202820201820018281a000f4240850b0300"
        "005846438ed6208eb1c1ffb94d952175167df0902902064a2983910c4fb2340790bf"
        "420a7d1921d5bf7c4721e02ab87a93ab1e0b75cf62e4948727c8b5dae46ed2af0543"
        "9b88029191850c0201005849820301020182028202018382014c5477656c76653132"
        "313231328202038204078281820150220ffc45c8a901999ecc60991dd78b29818201"
        "50d2c51cb2481792dae8b21d848cede99b8501010000582390eab6457593379298a8"
        "724e16e61f837488e127212b59ac91f8a86287b7d07630a122ff"
    );
    BOOST_REQUIRE(BinaryConversions::HexStringToBytes(encryptedSerializedBundleString, encryptedSerializedBundle));
    padded_vector_uint8_t encryptedSerializedBundlePadded(
        encryptedSerializedBundle.data(),
        encryptedSerializedBundle.data() + encryptedSerializedBundle.size()
    );
    BundleViewV7 bv;
    BOOST_REQUIRE(bv.LoadBundle(encryptedSerializedBundlePadded.data(), encryptedSerializedBundlePadded.size(), false));

    //decrypt

    std::vector<uint8_t> dataEncryptionKeyBytes; //DEK
    static const std::string dataEncryptionKeyString(
        "71776572747975696f70617364666768"
        "71776572747975696f70617364666768"
    );
    BOOST_REQUIRE(BinaryConversions::HexStringToBytes(dataEncryptionKeyString, dataEncryptionKeyBytes));

    std::vector<boost::asio::const_buffer> aadPartsTemporaryMemory;

    BPSecManager::EvpCipherCtxWrapper ctxWrapper;
    bool hadError;
    bool decryptionSuccessful;
    BPSecManager::TryDecryptBundle(ctxWrapper,
        bv,
        NULL, 0, //not using KEK
        dataEncryptionKeyBytes.data(), static_cast<const unsigned int>(dataEncryptionKeyBytes.size()),
        aadPartsTemporaryMemory,
        hadError, decryptionSuccessful);
    BOOST_REQUIRE(!hadError);
    BOOST_REQUIRE(decryptionSuccessful);


    static const std::string expectedSerializedPayloadBlockString(
        "85010100005823526561647920746f2067656e657261746520612033322d62797465207061796c6f6164"
    );
    {
        std::vector<BundleViewV7::Bpv7CanonicalBlockView*> blocks;
        bv.GetCanonicalBlocksByType(BPV7_BLOCK_TYPE_CODE::PAYLOAD, blocks);
        BOOST_REQUIRE_EQUAL(blocks.size(), 1);
        std::string actualHex;
        BinaryConversions::BytesToHexString(blocks[0]->actualSerializedBlockPtr.data(), blocks[0]->actualSerializedBlockPtr.size(), actualHex);
        boost::to_lower(actualHex);
        BOOST_REQUIRE_EQUAL(actualHex, expectedSerializedPayloadBlockString);
        //std::cout << "payload decrypted: " << actualHex << "\n";

        BinaryConversions::BytesToHexString(blocks[0]->headerPtr->m_dataPtr, blocks[0]->headerPtr->m_dataLength, actualHex);
        boost::to_lower(actualHex);
        //std::cout << "payload data decrypted: " << actualHex << "\n";
    }

    
    static const std::string expectedSerializedBibBlockString(
        "850b030000584681010101820282020182820106820307818182015830f75fe4c3"
        "7f76f046165855bd5ff72fbfd4e3a64b4695c40e2b787da005ae819f0a2e30a2e8b3"
        "25527de8aefb52e73d71"
    );
    {
        std::vector<BundleViewV7::Bpv7CanonicalBlockView*> blocks;
        bv.GetCanonicalBlocksByType(BPV7_BLOCK_TYPE_CODE::INTEGRITY, blocks);
        BOOST_REQUIRE_EQUAL(blocks.size(), 1);
        std::string actualHex;
        BinaryConversions::BytesToHexString(blocks[0]->actualSerializedBlockPtr.data(), blocks[0]->actualSerializedBlockPtr.size(), actualHex);
        boost::to_lower(actualHex);
        BOOST_REQUIRE_EQUAL(actualHex, expectedSerializedBibBlockString);

        //remove the decrypted bib
        blocks[0]->markedForDeletion = true;
        bv.RenderInPlace(128);
    }
    
    //all decrypted, no bib nor bcb, just primary and payload
    static const std::string expectedSerializedBundleString(
        "9f88070000820282010282028202018202820201820018281a000f424085010100"
        "005823526561647920746f2067656e657261746520612033322d6279746520706179"
        "6c6f6164ff"
    );
    {
        std::string actualHex;
        BinaryConversions::BytesToHexString(bv.m_renderedBundle, actualHex);
        boost::to_lower(actualHex);
        BOOST_REQUIRE_EQUAL(actualHex, expectedSerializedBundleString);
        //std::cout << "decrypted bundle: " << actualHex << "\n";
    }

    { //take new bundle and encrypt
        //Generated from TestBpsecDefaultSecurityContexts.cpp with comment:
        //  dump the payload, bib, and primary as a full bundle for "encryption+add_bcb" unit test 
        static const std::string expectedSerializedBundleWithBibString(
            "9f88070000820282010282028202018202820201820018281a000f4240850b030000"
            "584681010101820282020182820106820307818182015830f75fe4c37f76f0461658"
            "55bd5ff72fbfd4e3a64b4695c40e2b787da005ae819f0a2e30a2e8b325527de8aefb"
            "52e73d7185010100005823526561647920746f2067656e657261746520612033322d"
            "62797465207061796c6f6164ff"
        );
        std::vector<uint8_t> expectedSerializedBundleWithBib;
        BOOST_REQUIRE(BinaryConversions::HexStringToBytes(expectedSerializedBundleWithBibString, expectedSerializedBundleWithBib));
        padded_vector_uint8_t serializedBundleWithBibPadded(
            expectedSerializedBundleWithBib.data(),
            expectedSerializedBundleWithBib.data() + expectedSerializedBundleWithBib.size()
        );
        BundleViewV7 bv2;
        BOOST_REQUIRE(bv2.LoadBundle(serializedBundleWithBibPadded.data(), serializedBundleWithBibPadded.size(), false));
        
        std::vector<uint8_t> expectedInitializationVector;
        static const std::string expectedInitializationVectorString(
            "5477656c7665313231323132"
        );
        BOOST_REQUIRE(BinaryConversions::HexStringToBytes(expectedInitializationVectorString, expectedInitializationVector));

        static const uint64_t targetBlockNumbers[2] = { 3, 1 };

        const uint64_t insertBcbBeforeThisBlockNumber = 1;
        bool encryptionSuccessful;
        BPSecManager::TryEncryptBundle(ctxWrapper,
            bv2,
            BPSEC_BCB_AES_GCM_AAD_SCOPE_MASKS::ALL_FLAGS_SET,
            COSE_ALGORITHMS::A256GCM,
            BPV7_CRC_TYPE::NONE,
            cbhe_eid_t(2, 1),
            targetBlockNumbers, 2,
            expectedInitializationVector.data(), static_cast<unsigned int>(expectedInitializationVector.size()),
            NULL, 0, //NULL if not present (for wrapping DEK only)
            dataEncryptionKeyBytes.data(), static_cast<unsigned int>(dataEncryptionKeyBytes.size()), //NULL if not present (when no wrapped key is present)
            aadPartsTemporaryMemory,
            &insertBcbBeforeThisBlockNumber,
            hadError, encryptionSuccessful);

        BOOST_REQUIRE(!hadError);
        BOOST_REQUIRE(encryptionSuccessful);

        {
            std::string actualHex;
            BinaryConversions::BytesToHexString(bv2.m_renderedBundle, actualHex);
            boost::to_lower(actualHex);
            BOOST_REQUIRE_EQUAL(actualHex, encryptedSerializedBundleString);
            //std::cout << "decrypted bundle: " << actualHex << "\n";
        }
    }

}
