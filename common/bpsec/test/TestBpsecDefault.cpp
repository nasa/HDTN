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
#include "codec/BundleViewV7.h"
#include <iostream>
#include <string>
#include <inttypes.h>
#include <vector>
#include "Uri.h"
#include "PaddedVectorUint8.h"
#include "BinaryConversions.h"
#include "BPSecManager.h"
#include <boost/algorithm/string.hpp>



BOOST_AUTO_TEST_CASE(HmacShaTestCase)
{
    /*Key         : h'1a2b1a2b1a2b1a2b1a2b1a2b1a2b1a2b'
    SHA Variant : HMAC 512/512
    Scope Flags : 0x00
    Payload Data: h'526561647920746f2067656e65726174
                    6520612033322d62797465207061796c
                    6f6164'
    IPPT        : h'005823526561647920746f2067656e65
                    7261746520612033322d627974652070
                    61796c6f6164'
    Signature   : h'3bdc69b3a34a2b5d3a8554368bd1e808
                    f606219d2a10a846eae3886ae4ecc83c
                    4ee550fdfb1cc636b904e2f1a73e303d
                    cd4b6ccece003e95e8164dcc89a156e1'*/
    std::vector<uint8_t> keyBytes;
    static const std::string keyString(
        "1a2b1a2b1a2b1a2b1a2b1a2b1a2b1a2b"
    );
    BOOST_REQUIRE(BinaryConversions::HexStringToBytes(keyString, keyBytes));

    std::vector<uint8_t> ipptPart0Bytes;
    static const std::string ipptPart0String(
        "00"
    );
    BOOST_REQUIRE(BinaryConversions::HexStringToBytes(ipptPart0String, ipptPart0Bytes));

    std::vector<uint8_t> ipptPart1Bytes;
    static const std::string ipptPart1String(
        "5823526561647920746f2067656e65"
        "7261746520612033322d627974652070"
        "61796c6f6164"
    );
    BOOST_REQUIRE(BinaryConversions::HexStringToBytes(ipptPart1String, ipptPart1Bytes));

    static const std::string expectedSha(
        "3bdc69b3a34a2b5d3a8554368bd1e808"
        "f606219d2a10a846eae3886ae4ecc83c"
        "4ee550fdfb1cc636b904e2f1a73e303d"
        "cd4b6ccece003e95e8164dcc89a156e1"
    );

    std::vector<boost::asio::const_buffer> ipptParts;
    ipptParts.emplace_back(boost::asio::buffer(ipptPart0Bytes));
    ipptParts.emplace_back(boost::asio::buffer(ipptPart1Bytes));

    static constexpr unsigned int expectedShaLengthBytes = 64; //64*8 = 512bits

    BPSecManager::HmacCtxWrapper ctxWrapper; //reuse this in loop below

    for (unsigned int i = 0; i < 3; ++i) { //Get message digest
        std::vector<uint8_t> messageDigestBytes(expectedShaLengthBytes + 10, 'b'); //paint extra bytes (should be unmodified)

        unsigned int messageDigestOutSize = 0;
        //not inplace test (separate in and out buffers)
        BOOST_REQUIRE(BPSecManager::HmacSha(ctxWrapper,
            COSE_ALGORITHMS::HMAC_512_512,
            ipptParts,
            keyBytes.data(), keyBytes.size(),
            messageDigestBytes.data(), messageDigestOutSize));

        BOOST_REQUIRE_EQUAL(messageDigestOutSize, expectedShaLengthBytes);
        BOOST_REQUIRE_EQUAL(memcmp(&messageDigestBytes[expectedShaLengthBytes],
            "bbbbbbbbbbbbb", 10), 0); //messageDigestBytes should not have overrun

        messageDigestBytes.resize(messageDigestOutSize);
        std::string messageDigestHexString;
        BinaryConversions::BytesToHexString(messageDigestBytes, messageDigestHexString);
        boost::to_lower(messageDigestHexString);
        BOOST_REQUIRE_EQUAL(messageDigestHexString, expectedSha);
    }
}

BOOST_AUTO_TEST_CASE(HmacShaVerifyBundleSimpleTestCase)
{
    padded_vector_uint8_t bibSerializedBundle;
    static const std::string bibSerializedBundleString(
        "9f88070000820282010282028202018202820201820018281a000f4240850b0200"
        "005856810101018202820201828201078203008181820158403bdc69b3a34a2b5d3a"
        "8554368bd1e808f606219d2a10a846eae3886ae4ecc83c4ee550fdfb1cc636b904e2"
        "f1a73e303dcd4b6ccece003e95e8164dcc89a156e185010100005823526561647920"
        "746f2067656e657261746520612033322d62797465207061796c6f6164ff"
    );
    BOOST_REQUIRE(BinaryConversions::HexStringToBytes(bibSerializedBundleString, bibSerializedBundle));

    std::vector<uint8_t> hmacKeyBytes;
    static const std::string hmacKeyString(
        "1a2b1a2b1a2b1a2b1a2b1a2b1a2b1a2b"
    );
    BOOST_REQUIRE(BinaryConversions::HexStringToBytes(hmacKeyString, hmacKeyBytes));

    BPSecManager::HmacCtxWrapper ctxWrapper;
    std::vector<boost::asio::const_buffer> ipptPartsTemporaryMemory;

    std::string nobibSerializedBundleString; //used later when bib is removed

    //load bundle to test deserialize
    {
        padded_vector_uint8_t toSwapIn(bibSerializedBundle);
        BundleViewV7 bv;
        BOOST_REQUIRE(bv.SwapInAndLoadBundle(toSwapIn, false));

        BOOST_REQUIRE(BPSecManager::TryVerifyBundleIntegrity(ctxWrapper,
            bv,
            NULL, 0, //NULL if not present (for unwrapping hmac key only)
            hmacKeyBytes.data(), static_cast<const unsigned int>(hmacKeyBytes.size()), //NULL if not present (when no wrapped key is present)
            ipptPartsTemporaryMemory,
            true,
            true));
        BinaryConversions::BytesToHexString(bv.m_renderedBundle, nobibSerializedBundleString);
        boost::to_lower(nobibSerializedBundleString);
        BOOST_REQUIRE_NE(bibSerializedBundleString, nobibSerializedBundleString);
    }
    
    //load nobib bundle to test adding integrity
    {
        padded_vector_uint8_t nobibSerializedBundle;
        BOOST_REQUIRE(BinaryConversions::HexStringToBytes(nobibSerializedBundleString, nobibSerializedBundle));
        
        BundleViewV7 bv;
        BOOST_REQUIRE(bv.SwapInAndLoadBundle(nobibSerializedBundle, false));

        static const uint64_t targetBlockNumbers[1] = { 1 };
        BOOST_REQUIRE(BPSecManager::TryAddBundleIntegrity(ctxWrapper,
            bv,
            BPSEC_BIB_HMAC_SHA2_INTEGRITY_SCOPE_MASKS::NO_ADDITIONAL_SCOPE,
            COSE_ALGORITHMS::HMAC_512_512,
            BPV7_CRC_TYPE::NONE,
            cbhe_eid_t(2,1),
            targetBlockNumbers, 1,
            NULL, 0, //NULL if not present (for unwrapping hmac key only)
            hmacKeyBytes.data(), static_cast<const unsigned int>(hmacKeyBytes.size()), //NULL if not present (when no wrapped key is present)
            ipptPartsTemporaryMemory,
            NULL,
            true));
        std::string newBibSerializedBundleString;
        BinaryConversions::BytesToHexString(bv.m_renderedBundle, newBibSerializedBundleString);
        boost::to_lower(newBibSerializedBundleString);
        BOOST_REQUIRE_EQUAL(bibSerializedBundleString, newBibSerializedBundleString);
    }
}

BOOST_AUTO_TEST_CASE(HmacShaVerifyBundleMultipleSourcesTestCase)
{
    padded_vector_uint8_t bibSerializedBundle;
    static const std::string bibSerializedBundleString(
        "9f88070000820282010282028202018202820201820018281a000f4240850b0300"
        "00585c8200020101820282030082820105820300828182015820cac6ce8e4c5dae57"
        "988b757e49a6dd1431dc04763541b2845098265bc817241b81820158203ed614c0d9"
        "7f49b3633627779aa18a338d212bf3c92b97759d9739cd50725596850c0401005834"
        "8101020182028202018382014c5477656c7665313231323132820201820400818182"
        "0150efa4b5ac0108e3816c5606479801bc0485070200004319012c85010100005823"
        "3a09c1e63fe23a7f66a59c7303837241e070b02619fc59c5214a22f08cd70795e73e"
        "9aff"
    );
    BOOST_REQUIRE(BinaryConversions::HexStringToBytes(bibSerializedBundleString, bibSerializedBundle));

    std::vector<uint8_t> hmacKeyBytes;
    static const std::string hmacKeyString(
        "1a2b1a2b1a2b1a2b1a2b1a2b1a2b1a2b"
    );
    BOOST_REQUIRE(BinaryConversions::HexStringToBytes(hmacKeyString, hmacKeyBytes));

    BPSecManager::HmacCtxWrapper ctxWrapper;
    std::vector<boost::asio::const_buffer> ipptPartsTemporaryMemory;

    std::string nobibSerializedBundleString; //used later when bib is removed

    //load bundle to test deserialize
    {
        padded_vector_uint8_t toSwapIn(bibSerializedBundle);
        BundleViewV7 bv;
        BOOST_REQUIRE(bv.SwapInAndLoadBundle(toSwapIn, false));

        BOOST_REQUIRE(BPSecManager::TryVerifyBundleIntegrity(ctxWrapper,
            bv,
            NULL, 0, //NULL if not present (for unwrapping hmac key only)
            hmacKeyBytes.data(), static_cast<const unsigned int>(hmacKeyBytes.size()), //NULL if not present (when no wrapped key is present)
            ipptPartsTemporaryMemory,
            true,
            true));
        BinaryConversions::BytesToHexString(bv.m_renderedBundle, nobibSerializedBundleString);
        boost::to_lower(nobibSerializedBundleString);
        BOOST_REQUIRE_NE(bibSerializedBundleString, nobibSerializedBundleString);
    }

    //load nobib bundle to test adding integrity
    {
        padded_vector_uint8_t nobibSerializedBundle;
        BOOST_REQUIRE(BinaryConversions::HexStringToBytes(nobibSerializedBundleString, nobibSerializedBundle));
        
        BundleViewV7 bv;
        BOOST_REQUIRE(bv.SwapInAndLoadBundle(nobibSerializedBundle, false));

        static const uint64_t targetBlockNumbers[2] = { 0, 2 };
        BOOST_REQUIRE(BPSecManager::TryAddBundleIntegrity(ctxWrapper,
            bv,
            BPSEC_BIB_HMAC_SHA2_INTEGRITY_SCOPE_MASKS::NO_ADDITIONAL_SCOPE,
            COSE_ALGORITHMS::HMAC_256_256,
            BPV7_CRC_TYPE::NONE,
            cbhe_eid_t(3, 0),
            targetBlockNumbers, 2,
            NULL, 0, //NULL if not present (for unwrapping hmac key only)
            hmacKeyBytes.data(), static_cast<const unsigned int>(hmacKeyBytes.size()), //NULL if not present (when no wrapped key is present)
            ipptPartsTemporaryMemory,
            NULL,
            true));
        std::string newBibSerializedBundleString;
        BinaryConversions::BytesToHexString(bv.m_renderedBundle, newBibSerializedBundleString);
        boost::to_lower(newBibSerializedBundleString);
        BOOST_REQUIRE_EQUAL(bibSerializedBundleString, newBibSerializedBundleString);
    }
}

BOOST_AUTO_TEST_CASE(HmacShaVerifyBundleFullScopeTestCase)
{
    //Generated from TestBpsecDefaultSecurityContexts.cpp with comment:
    //  dump the payload, bib, and primary as a full bundle for "encryption+add_bcb" unit test 
    padded_vector_uint8_t bibSerializedBundle;
    static const std::string bibSerializedBundleString(
        "9f88070000820282010282028202018202820201820018281a000f4240850b030000"
        "584681010101820282020182820106820307818182015830f75fe4c37f76f0461658"
        "55bd5ff72fbfd4e3a64b4695c40e2b787da005ae819f0a2e30a2e8b325527de8aefb"
        "52e73d7185010100005823526561647920746f2067656e657261746520612033322d"
        "62797465207061796c6f6164ff"
    );
    BOOST_REQUIRE(BinaryConversions::HexStringToBytes(bibSerializedBundleString, bibSerializedBundle));

    std::vector<uint8_t> hmacKeyBytes;
    static const std::string hmacKeyString(
        "1a2b1a2b1a2b1a2b1a2b1a2b1a2b1a2b"
    );
    BOOST_REQUIRE(BinaryConversions::HexStringToBytes(hmacKeyString, hmacKeyBytes));

    BPSecManager::HmacCtxWrapper ctxWrapper;
    std::vector<boost::asio::const_buffer> ipptPartsTemporaryMemory;

    std::string nobibSerializedBundleString; //used later when bib is removed

    //load bundle to test deserialize
    {
        padded_vector_uint8_t toSwapIn(bibSerializedBundle);
        BundleViewV7 bv;
        BOOST_REQUIRE(bv.SwapInAndLoadBundle(toSwapIn, false));

        BOOST_REQUIRE(BPSecManager::TryVerifyBundleIntegrity(ctxWrapper,
            bv,
            NULL, 0, //NULL if not present (for unwrapping hmac key only)
            hmacKeyBytes.data(), static_cast<const unsigned int>(hmacKeyBytes.size()), //NULL if not present (when no wrapped key is present)
            ipptPartsTemporaryMemory,
            true,
            true));
        BinaryConversions::BytesToHexString(bv.m_renderedBundle, nobibSerializedBundleString);
        boost::to_lower(nobibSerializedBundleString);
        BOOST_REQUIRE_NE(bibSerializedBundleString, nobibSerializedBundleString);
    }

    //load nobib bundle to test adding integrity
    {
        padded_vector_uint8_t nobibSerializedBundle;
        BOOST_REQUIRE(BinaryConversions::HexStringToBytes(nobibSerializedBundleString, nobibSerializedBundle));
        
        BundleViewV7 bv;
        BOOST_REQUIRE(bv.SwapInAndLoadBundle(nobibSerializedBundle, false));

        static const uint64_t targetBlockNumbers[1] = { 1 };
        bv.ReserveBlockNumber(2); //force bib block number to be 3 to match test
        BOOST_REQUIRE(BPSecManager::TryAddBundleIntegrity(ctxWrapper,
            bv,
            BPSEC_BIB_HMAC_SHA2_INTEGRITY_SCOPE_MASKS::ALL_FLAGS_SET,
            COSE_ALGORITHMS::HMAC_384_384,
            BPV7_CRC_TYPE::NONE,
            cbhe_eid_t(2, 1),
            targetBlockNumbers, 1,
            NULL, 0, //NULL if not present (for unwrapping hmac key only)
            hmacKeyBytes.data(), static_cast<const unsigned int>(hmacKeyBytes.size()), //NULL if not present (when no wrapped key is present)
            ipptPartsTemporaryMemory,
            NULL,
            true));
        std::string newBibSerializedBundleString;
        BinaryConversions::BytesToHexString(bv.m_renderedBundle, newBibSerializedBundleString);
        boost::to_lower(newBibSerializedBundleString);
        BOOST_REQUIRE_EQUAL(bibSerializedBundleString, newBibSerializedBundleString);
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
    padded_vector_uint8_t encryptedSerializedBundle;
    static const std::string encryptedSerializedBundleString(
        "9f88070000820282010282028202018202820201820018281a000f4240850c0201"
        "0058508101020182028202018482014c5477656c7665313231323132820201820358"
        "1869c411276fecddc4780df42c8a2af89296fabf34d7fae7008204008181820150ef"
        "a4b5ac0108e3816c5606479801bc04850101000058233a09c1e63fe23a7f66a59c73"
        "03837241e070b02619fc59c5214a22f08cd70795e73e9aff"
    );
    BOOST_REQUIRE(BinaryConversions::HexStringToBytes(encryptedSerializedBundleString, encryptedSerializedBundle));
    padded_vector_uint8_t toSwapIn(encryptedSerializedBundle);
    BundleViewV7 bv;
    BOOST_REQUIRE(bv.SwapInAndLoadBundle(toSwapIn, false));

    //decrypt

    std::vector<uint8_t> keyEncryptionKeyBytes; //KEK
    static const std::string keyEncryptionKeyString(
        "6162636465666768696a6b6c6d6e6f70"
    );
    BOOST_REQUIRE(BinaryConversions::HexStringToBytes(keyEncryptionKeyString, keyEncryptionKeyBytes));

    std::vector<boost::asio::const_buffer> aadPartsTemporaryMemory;

    BPSecManager::EvpCipherCtxWrapper ctxWrapper;
    BOOST_REQUIRE(BPSecManager::TryDecryptBundle(ctxWrapper,
        bv,
        keyEncryptionKeyBytes.data(), static_cast<const unsigned int>(keyEncryptionKeyBytes.size()),
        NULL, 0, //no DEK (using KEK instead)
        aadPartsTemporaryMemory,
        true));
    padded_vector_uint8_t decryptedBundleCopy(
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
        
        BundleViewV7 bv2;
        BOOST_REQUIRE(bv2.SwapInAndLoadBundle(decryptedBundleCopy, false));

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
        BOOST_REQUIRE(BPSecManager::TryEncryptBundle(ctxWrapper,
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
            true));

        {
            std::string actualHex;
            BinaryConversions::BytesToHexString(bv2.m_renderedBundle, actualHex);
            boost::to_lower(actualHex);
            BOOST_REQUIRE_EQUAL(actualHex, encryptedSerializedBundleString);
        }
    }
}

//from TestBpsecDefaultSecurityContextsSecurityBlocksWithFullScopeTestCase
BOOST_AUTO_TEST_CASE(DecryptThenEncryptBundleFullScopeTestCase)
{
    padded_vector_uint8_t encryptedSerializedBundle;
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
    padded_vector_uint8_t toSwapIn(encryptedSerializedBundle);
    BundleViewV7 bv;
    BOOST_REQUIRE(bv.SwapInAndLoadBundle(toSwapIn, false));

    //decrypt

    std::vector<uint8_t> dataEncryptionKeyBytes; //DEK
    static const std::string dataEncryptionKeyString(
        "71776572747975696f70617364666768"
        "71776572747975696f70617364666768"
    );
    BOOST_REQUIRE(BinaryConversions::HexStringToBytes(dataEncryptionKeyString, dataEncryptionKeyBytes));

    std::vector<boost::asio::const_buffer> aadPartsTemporaryMemory;

    BPSecManager::EvpCipherCtxWrapper ctxWrapper;
    BOOST_REQUIRE(BPSecManager::TryDecryptBundle(ctxWrapper,
        bv,
        NULL, 0, //not using KEK
        dataEncryptionKeyBytes.data(), static_cast<const unsigned int>(dataEncryptionKeyBytes.size()),
        aadPartsTemporaryMemory,
        true));


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
        padded_vector_uint8_t serializedBundleWithBib;
        BOOST_REQUIRE(BinaryConversions::HexStringToBytes(expectedSerializedBundleWithBibString, serializedBundleWithBib));
        
        BundleViewV7 bv2;
        BOOST_REQUIRE(bv2.SwapInAndLoadBundle(serializedBundleWithBib, false));
        
        std::vector<uint8_t> expectedInitializationVector;
        static const std::string expectedInitializationVectorString(
            "5477656c7665313231323132"
        );
        BOOST_REQUIRE(BinaryConversions::HexStringToBytes(expectedInitializationVectorString, expectedInitializationVector));

        static const uint64_t targetBlockNumbers[2] = { 3, 1 };

        const uint64_t insertBcbBeforeThisBlockNumber = 1;
        BOOST_REQUIRE(BPSecManager::TryEncryptBundle(ctxWrapper,
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
            true));

        {
            std::string actualHex;
            BinaryConversions::BytesToHexString(bv2.m_renderedBundle, actualHex);
            boost::to_lower(actualHex);
            BOOST_REQUIRE_EQUAL(actualHex, encryptedSerializedBundleString);
        }
    }

}
