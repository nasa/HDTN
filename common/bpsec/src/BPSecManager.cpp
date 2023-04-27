/**
 * @file BPSecManager.cpp
 * @author  Nadia Kortas <nadia.kortas@nasa.gov>
 *
 * @copyright Copyright  2021 United States Government as represented by
 * the National Aeronautics and Space Administration.
 * No copyright is claimed in the United States under Title 17, U.S.Code.
 * All Other Rights Reserved.
 *
 * @section LICENSE
 * Released under the NASA Open Source Agreement (NOSA)
 * See LICENSE.md in the source root directory for more information.
 */

#include "BPSecManager.h"
#include <iostream>
#include "TimestampUtil.h"
#include "Uri.h"
#include <boost/make_unique.hpp>
#include <openssl/hmac.h>
#include <openssl/evp.h>
#include <string>
#include <cstring>
#include <sstream>
#include "codec/BundleViewV7.h"
#include "codec/bpv7.h"
#include "BinaryConversions.h"
#include "PaddedVectorUint8.h"
#include <vector>
#include <iostream>
#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/rand.h>
#include <openssl/ssl.h>
#include <openssl/x509v3.h>
#include <openssl/aes.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <string>
#include <sstream>
#include <vector>
#include <iostream>
#include "Logger.h"
#include "codec/bpv7.h"
#include <openssl/bn.h>
#include <openssl/rsa.h>
#include <openssl/pem.h>
#include <openssl/engine.h>
#include <assert.h>
#include <stdio.h>
#include <boost/algorithm/hex.hpp>
#include <boost/algorithm/string.hpp>
#include "CborUint.h"


static constexpr hdtn::Logger::SubProcess subprocess = hdtn::Logger::SubProcess::none;

BPSecManager::BPSecManager(const bool isSecEnabled) : 
m_isSecEnabled(isSecEnabled)
{
}

BPSecManager::~BPSecManager() {}

unsigned char* BPSecManager::hmac_sha(const EVP_MD *evp_md,
                    std::string key,
                    std::string data, unsigned char *md,
                    unsigned int *md_len)
{
    HMAC_CTX *c = NULL;
    if ((c = HMAC_CTX_new()) == NULL) {
        goto err;
    }

    printf("key:\n");
    BIO_dump_fp(stdout, (const char*) key.c_str(), (int)key.length());
    /*
    std::cout << "Key Length: " << key.length() << std::endl;
    std::cout << "Plaintext: " << data << std::endl;
    std::cout << "Plaintext length: " << data.length()  << std::endl;
    */

    printf("Plaintext:\n");
    BIO_dump_fp(stdout, (const char*) data.c_str(), (int)data.length());

    if (!HMAC_Init_ex(c, (const unsigned char*)key.c_str(), (int)key.length(), evp_md, NULL)) {
        goto err;
    }
    if (!HMAC_Update(c, (const unsigned char*)data.c_str(), data.length())) {
        goto err;
    }
    if (!HMAC_Final(c, md, md_len)) {
        goto err;
    }
    HMAC_CTX_free(c);

    printf("HMAC Digest:\n");
    BIO_dump_fp(stdout, (const char*) md, *md_len);

    return md;
 err:
    HMAC_CTX_free(c);
    return NULL;
}

int BPSecManager::aes_gcm_encrypt(std::string gcm_pt, std::string gcm_key, 
    std::string gcm_iv, std::string gcm_aad, 
    unsigned char* ciphertext, unsigned char* tag, 
    int *outlen) 
{
    int ret = 0;
    EVP_CIPHER_CTX *ctx;

    printf("AES GCM Encrypt:\n");
    printf("Plaintext:\n");
    BIO_dump_fp(stdout, (const char*) gcm_pt.c_str(), (int)gcm_pt.length());
 

    printf("key:\n");
    BIO_dump_fp(stdout, (const char*) gcm_key.c_str(), (int)gcm_key.length());

    printf("IV:\n");
    BIO_dump_fp(stdout, (const char*) gcm_iv.c_str(), (int)gcm_iv.length());

    printf("aad:\n");
    BIO_dump_fp(stdout, (const char*) gcm_aad.c_str(), (int)gcm_aad.length());

    if ((ctx = EVP_CIPHER_CTX_new()) == NULL) {
        goto err;
    }

    /* Set cipher type and mode */
    if (gcm_key.length() == 16) {
        if (!EVP_EncryptInit_ex(ctx, EVP_aes_128_gcm(), NULL, NULL, NULL)) {
            goto err;
        }
    }
    else if (gcm_key.length() == 32) {
        if (!EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, NULL, NULL)) {
            goto err;
        }
    }
    else { 
       printf("Error Incorrect Key length!!\n");
       goto err;    
    }

    /* Set IV length if default 96 bits is not appropriate */
    if (!EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_IVLEN, (int)gcm_iv.length(), NULL)) {
        goto err;
    }
    
    /* Initialise key and IV */
    if (!EVP_EncryptInit_ex(ctx, NULL, NULL, (const unsigned char*)gcm_key.c_str(),
        (const unsigned char*)gcm_iv.c_str()))
    {
        goto err;
    }
    
    /* Zero or more calls to specify any AAD */
    if (!EVP_EncryptUpdate(ctx, NULL, outlen,
        (const unsigned char*)gcm_aad.c_str(),
        (int)gcm_aad.length()))
    {
        goto err;
    }
    
    /* Encrypt plaintext */
    if (!EVP_EncryptUpdate(ctx, ciphertext, outlen, (const unsigned char*)gcm_pt.c_str(),
        (int)gcm_pt.length()))
    {
        goto err;
    }
    
    /* Output encrypted block */
    printf("Ciphertext:\n");
    BIO_dump_fp(stdout, (const char*) ciphertext, *outlen);
    std::cout << "Ciphertext Len "  << *outlen << std::endl;  

    memcpy(tag, ciphertext, EVP_GCM_TLS_TAG_LEN); //The length of the authentication tag, prior to any CBOR encoding, MUST be 128 bits.

    /* Finalise: note get no output for GCM */
    if (!EVP_EncryptFinal_ex(ctx, tag, outlen)) {
        goto err;
    }
    

    /* Get tag */
    if (!EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_GET_TAG, EVP_GCM_TLS_TAG_LEN, tag)) { //The length of the authentication tag, prior to any CBOR encoding, MUST be 128 bits.
        goto err;
    }


    /* Output tag */
    printf("Tag:\n");
    BIO_dump_fp(stdout, (const char*) tag, EVP_GCM_TLS_TAG_LEN);

    ret = 1; 

err:
    if (!ret) {
        ERR_print_errors_fp(stderr);
    }

    EVP_CIPHER_CTX_free(ctx);
    
    return ret;
}

int BPSecManager::aes_gcm_decrypt(std::string gcm_ct, std::string gcm_tag,
    std::string gcm_key, std::string gcm_iv, 
    std::string gcm_aad, unsigned char* plaintext, 
    int *outlen)
{
    int ret = 0;
    EVP_CIPHER_CTX *ctx;
    int rv;
    EVP_CIPHER *cipher = NULL;

    printf("AES GCM Decrypt:\n");
    printf("Ciphertext:\n");
    BIO_dump_fp(stdout, (const char*) gcm_ct.c_str(), (int)gcm_ct.length());
    
    if ((ctx = EVP_CIPHER_CTX_new()) == NULL) {
        goto err;
    }

    // Select cipher 
    if (gcm_key.length() == 16) {
        if (!EVP_DecryptInit_ex(ctx, EVP_aes_128_gcm(), NULL, NULL, NULL)) {
            goto err;
        }
    }
    else if (gcm_key.length() == 32) {
        if (!EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, NULL, NULL)) {
            goto err;
        }
    }
    else {
        printf("Error Incorrect Key length!!\n");
        goto err;
    }

    //Set IV length, omit for 96 bits
    if (!EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_IVLEN, (int)gcm_iv.length(), NULL)) {
        goto err;
    }

    // Specify key and IV 
    if (!EVP_DecryptInit_ex(ctx, NULL, NULL, (const unsigned char*)gcm_key.c_str(),
        (const unsigned char*)gcm_iv.c_str()))
    {
        goto err;
    }

    // Zero or more calls to specify any AAD 
    if (!EVP_DecryptUpdate(ctx, NULL, outlen, (const unsigned char*)gcm_aad.c_str(),
        (int)gcm_aad.length()))
    {
        goto err;
    }

    // Decrypt plaintext 
    if (!EVP_DecryptUpdate(ctx, plaintext, outlen, (const unsigned char*)gcm_ct.c_str(),
        (int)gcm_ct.length()))
    {
        goto err;
    }

    // Output decrypted block 
    printf("Plaintext:\n");
    BIO_dump_fp(stdout, (const char*) plaintext, *outlen);
    std::cout << "plaintext Len "  << *outlen << std::endl;

    printf("Tag :\n");
    BIO_dump_fp(stdout, (const char*)gcm_tag.c_str(), (int)gcm_tag.length());

    printf("Key :\n");
    BIO_dump_fp(stdout, (const char*)gcm_iv.c_str(), (int)gcm_iv.length());

    printf("IV :\n");
    BIO_dump_fp(stdout, (const char*)gcm_key.c_str(), (int)gcm_key.length());

    printf("Ciphertext:\n");
    BIO_dump_fp(stdout, (const char*) gcm_ct.c_str(), (int)gcm_ct.length());


    // Set expected tag value. 
    if (!EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_TAG, (int)gcm_tag.length(),
        (void*)gcm_tag.c_str()))
    {
        goto err;
    }
    
    // Finalise: note get no output for GCM 
    rv = EVP_DecryptFinal_ex(ctx, plaintext, outlen);
    
    printf("***Tag Verify %s\n", rv > 0 ? "Successful!" : "Failed!");
    
    ret = 1; 

err:
 
    if (!ret) {
        ERR_print_errors_fp(stderr);
        std::cout << "Error Decrypt!!! " << std::endl;
    }

    EVP_CIPHER_CTX_free(ctx);

    return ret;
}

//https://www.openssl.org/docs/man3.0/man7/crypto.html
//Performance:
/*
Performance can be gained by upgrading to openssl 3:

If you perform the same operation many times then it is recommended to use "Explicit fetching"
to prefetch an algorithm once initially,
and then pass this created object to any operations that are currently using "Implicit fetching".
See an example of Explicit fetching in "USING ALGORITHMS IN APPLICATIONS".*/



BPSecManager::EvpCipherCtxWrapper::EvpCipherCtxWrapper() : m_ctx(EVP_CIPHER_CTX_new()) {}
BPSecManager::EvpCipherCtxWrapper::~EvpCipherCtxWrapper() {
    if (m_ctx) {
        EVP_CIPHER_CTX_free(m_ctx);
        m_ctx = NULL;
    }
}

//https://www.openssl.org/docs/man1.1.1/man3/EVP_EncryptUpdate.html
    
//buffer size of cipherTextOut must be at least (unencryptedDataLength + EVP_MAX_BLOCK_LENGTH)
bool BPSecManager::AesGcmEncrypt(EvpCipherCtxWrapper& ctxWrapper,
    const uint8_t* unencryptedData, const uint64_t unencryptedDataLength,
    const uint8_t* key, const uint64_t keyLength,
    const uint8_t* iv, const uint64_t ivLength,
    const uint8_t* aad, const uint64_t aadLength,
    uint8_t* cipherTextOut, uint64_t& cipherTextOutSize, uint8_t* tagOut)
{
    //EVP_CIPHER_CTX_new() returns a pointer to a newly created EVP_CIPHER_CTX for success and NULL for failure.
    EVP_CIPHER_CTX* ctx = ctxWrapper.m_ctx;

    cipherTextOutSize = 0;
    uint8_t* const cipherTextOutBase = cipherTextOut;

    //The constant EVP_MAX_BLOCK_LENGTH is also the maximum block length for all ciphers.

    if (ctx == NULL) {
        return false;
    }

    // Set cipher type and mode
    //per https://www.openssl.org/docs/man1.1.1/man3/EVP_EncryptUpdate.html
    // New code should use EVP_EncryptInit_ex(), EVP_EncryptFinal_ex(), EVP_DecryptInit_ex(), EVP_DecryptFinal_ex(),
    // EVP_CipherInit_ex() and EVP_CipherFinal_ex() because they can reuse an existing context without allocating and freeing it up on each call.
    const EVP_CIPHER* cipherPtr;
    if (keyLength == 16) {
        cipherPtr = EVP_aes_128_gcm();
    }
    else if (keyLength == 32) {
        cipherPtr = EVP_aes_256_gcm();
    }
    else {
        printf("Error Incorrect Key length!!\n");
        return false;
    }
    //EVP_EncryptInit_ex(), EVP_EncryptUpdate() and EVP_EncryptFinal_ex() return 1 for success and 0 for failure.
    if (!EVP_EncryptInit_ex(ctx, cipherPtr, NULL, NULL, NULL)) {
        ERR_print_errors_fp(stderr);
        return false;
    }

    // Set IV length if default 96 bits is not appropriate
    // return value unclear in docs
    if (!EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_IVLEN, (int)ivLength, NULL)) {
        ERR_print_errors_fp(stderr);
        return false;
    }

    // Initialise key and IV
    //It is possible to set all parameters to NULL except type in an initial call and supply the remaining parameters in subsequent calls,
    //all of which have type set to NULL. This is done when the default cipher parameters are not appropriate.
    //EVP_EncryptInit_ex(), EVP_EncryptUpdate() and EVP_EncryptFinal_ex() return 1 for success and 0 for failure.
    if (!EVP_EncryptInit_ex(ctx, NULL, NULL, key, iv)) {
        ERR_print_errors_fp(stderr);
        return false;
    }

    
    //Encrypts inl bytes from the buffer in and writes the encrypted version to out.
    //This function can be called multiple times to encrypt successive blocks of data.
    //The amount of data written depends on the block alignment of the encrypted data.
    //For most ciphers and modes, the amount of data written can be anything from zero bytes to (inl + cipher_block_size - 1) bytes.
    //For wrap cipher modes, the amount of data written can be anything from zero bytes to (inl + cipher_block_size) bytes.
    //For stream ciphers, the amount of data written can be anything from zero bytes to inl bytes.
    //Thus, out should contain sufficient room for the operation being performed.
    //The actual number of bytes written is placed in outl.
    //It also checks if in and out are partially overlapping, and if they are 0 is returned to indicate failure.
    //
    // Zero or more calls to specify any AAD
    //AEAD Interface
    //The EVP interface for Authenticated Encryption with Associated Data (AEAD) modes are subtly
    // altered and several additional ctrl operations are supported depending on the mode specified.
    // To specify additional authenticated data (AAD), a call to EVP_CipherUpdate(), EVP_EncryptUpdate() or EVP_DecryptUpdate()
    // should be made with the output parameter out set to NULL.
    int tmpOutLength;
    //EVP_EncryptInit_ex(), EVP_EncryptUpdate() and EVP_EncryptFinal_ex() return 1 for success and 0 for failure.
    if (!EVP_EncryptUpdate(ctx, NULL, &tmpOutLength, aad, (int)aadLength)) {
        ERR_print_errors_fp(stderr);
        return false;
    }
    
    //inplace encryption should work per: https://github.com/openssl/openssl/issues/6498#issuecomment-397865986
    //Encrypt plaintext
    //EVP_EncryptInit_ex(), EVP_EncryptUpdate() and EVP_EncryptFinal_ex() return 1 for success and 0 for failure.
    if (!EVP_EncryptUpdate(ctx, cipherTextOut, &tmpOutLength, unencryptedData, (int)unencryptedDataLength)) {
        ERR_print_errors_fp(stderr);
        return false;
    }
    cipherTextOut += tmpOutLength;


    // Finalise: note get no output for GCM
    //If padding is enabled (the default) then EVP_EncryptFinal_ex() encrypts the "final" data,
    //that is any data that remains in a partial block.
    //It uses standard block padding (aka PKCS padding) as described in the NOTES section, below.
    //The encrypted final data is written to out which should have sufficient space for one cipher block.
    //The number of bytes written is placed in outl.
    //After this function is called the encryption operation is finished
    //and no further calls to EVP_EncryptUpdate() should be made.
    //EVP_EncryptInit_ex(), EVP_EncryptUpdate() and EVP_EncryptFinal_ex() return 1 for success and 0 for failure.
    if (!EVP_EncryptFinal_ex(ctx, cipherTextOut, &tmpOutLength)) {
        ERR_print_errors_fp(stderr);
        return false;
    }
    cipherTextOut += tmpOutLength;

    //Get tag
    //EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_GET_TAG, taglen, tag):
    //Writes taglen bytes of the tag value to the buffer indicated by tag.
    //This call can only be made when encrypting data and after all
    //data has been processed (e.g. after an EVP_EncryptFinal() call).
    //For OCB, taglen must either be 16 or the value previously set via EVP_CTRL_AEAD_SET_TAG.
    // return value unclear in docs
    if (!EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_GET_TAG, EVP_GCM_TLS_TAG_LEN, tagOut)) { //The length of the authentication tag, prior to any CBOR encoding, MUST be 128 bits.
        ERR_print_errors_fp(stderr);
        return false;
    }

    
    cipherTextOutSize = static_cast<uint64_t>(cipherTextOut - cipherTextOutBase);

    return true;
}

bool BPSecManager::AesGcmDecrypt(EvpCipherCtxWrapper& ctxWrapper,
    const uint8_t* encryptedData, const uint64_t encryptedDataLength,
    const uint8_t* key, const uint64_t keyLength,
    const uint8_t* iv, const uint64_t ivLength,
    const uint8_t* aad, const uint64_t aadLength,
    const uint8_t* tag,
    uint8_t* decryptedDataOut, uint64_t& decryptedDataOutSize)
{
    //EVP_CIPHER_CTX_new() returns a pointer to a newly created EVP_CIPHER_CTX for success and NULL for failure.
    EVP_CIPHER_CTX* ctx = ctxWrapper.m_ctx;

    decryptedDataOutSize = 0;
    uint8_t* const decryptedDataOutBase = decryptedDataOut;

    //The constant EVP_MAX_BLOCK_LENGTH is also the maximum block length for all ciphers.

    if (ctx == NULL) {
        return false;
    }

    // Set cipher type and mode
    //per https://www.openssl.org/docs/man1.1.1/man3/EVP_EncryptUpdate.html
    // New code should use EVP_EncryptInit_ex(), EVP_EncryptFinal_ex(), EVP_DecryptInit_ex(), EVP_DecryptFinal_ex(),
    // EVP_CipherInit_ex() and EVP_CipherFinal_ex() because they can reuse an existing context without allocating and freeing it up on each call.
    const EVP_CIPHER* cipherPtr;
    if (keyLength == 16) {
        cipherPtr = EVP_aes_128_gcm();
    }
    else if (keyLength == 32) {
        cipherPtr = EVP_aes_256_gcm();
    }
    else {
        printf("Error Incorrect Key length!!\n");
        return false;
    }
    //EVP_EncryptInit_ex(), EVP_EncryptUpdate() and EVP_EncryptFinal_ex() return 1 for success and 0 for failure.
    if (!EVP_DecryptInit_ex(ctx, cipherPtr, NULL, NULL, NULL)) {
        ERR_print_errors_fp(stderr);
        return false;
    }

    //Set IV length, omit for 96 bits
    // return value unclear in docs
    if (!EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_IVLEN, (int)ivLength, NULL)) {
        ERR_print_errors_fp(stderr);
        return false;
    }

    // Specify key and IV 
    //EVP_DecryptInit_ex() and EVP_DecryptUpdate() return 1 for success and 0 for failure.
    if (!EVP_DecryptInit_ex(ctx, NULL, NULL, key, iv)) {
        ERR_print_errors_fp(stderr);
        return false;
    }

    // Zero or more calls to specify any AAD
    //AEAD Interface
    //The EVP interface for Authenticated Encryption with Associated Data (AEAD) modes are subtly
    // altered and several additional ctrl operations are supported depending on the mode specified.
    // To specify additional authenticated data (AAD), a call to EVP_CipherUpdate(), EVP_EncryptUpdate() or EVP_DecryptUpdate()
    // should be made with the output parameter out set to NULL.
    int tmpOutLength;
    //EVP_DecryptInit_ex() and EVP_DecryptUpdate() return 1 for success and 0 for failure.
    if (!EVP_DecryptUpdate(ctx, NULL, &tmpOutLength, aad, (int)aadLength)) {
        ERR_print_errors_fp(stderr);
        return false;
    }

    //inplace encryption (perhaps decryption) should work per: https://github.com/openssl/openssl/issues/6498#issuecomment-397865986
    // Decrypt plaintext 
    //EVP_DecryptInit_ex() and EVP_DecryptUpdate() return 1 for success and 0 for failure.
    if (!EVP_DecryptUpdate(ctx, decryptedDataOut, &tmpOutLength, encryptedData, (int)encryptedDataLength)) {
        ERR_print_errors_fp(stderr);
        return false;
    }
    decryptedDataOut += tmpOutLength;

    // Set expected tag value.
    // AEAD Interface:
    //  EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_TAG, taglen, tag)
    //  When decrypting, this call sets the expected tag to taglen bytes from tag.
    //  taglen must be between 1 and 16 inclusive.
    //  The tag must be set prior to any call to EVP_DecryptFinal() or EVP_DecryptFinal_ex().
    // return value unclear in docs
    if (!EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_TAG, EVP_GCM_TLS_TAG_LEN, (void*)tag)) {
        ERR_print_errors_fp(stderr);
        return false;
    }

    // Finalise: note get no output for GCM 
    // AEAD Interface: The tag must be set prior to any call to EVP_DecryptFinal() or EVP_DecryptFinal_ex().
    // EVP_DecryptFinal_ex() returns 0 if the decrypt failed or 1 for success.
    if (!EVP_DecryptFinal_ex(ctx, decryptedDataOut, &tmpOutLength)) {
        return false;
    }

    decryptedDataOutSize = static_cast<uint64_t>(decryptedDataOut - decryptedDataOutBase);

    return true;
}

bool BPSecManager::AesWrapKey(
    const uint8_t* keyEncryptionKey, const unsigned int keyEncryptionKeyLength,
    const uint8_t* keyToWrap, const unsigned int keyToWrapLength,
    uint8_t* wrappedKeyOut, unsigned int& wrappedKeyOutSize)
{
    AES_KEY aesKey;

    //AES_set_encrypt_key() and AES_set_decrypt_key() return 0 for success, -1 if userKey or key is NULL, or -2 if the number of bits is unsupported.
    if (AES_set_encrypt_key(keyEncryptionKey, static_cast<int>(keyEncryptionKeyLength << 3), &aesKey)) {
        return false;
    }

    //definition https://docs.huihoo.com/doxygen/openssl/1.0.1c/aes__wrap_8c_source.html
    //2nd parameter of NULL uses default iv of 0xA6, 0xA6, 0xA6, 0xA6, 0xA6, 0xA6, 0xA6, 0xA6
    //success should return keyEncryptionKeyLength + 8, or -1 if failure
    const int wrappedKeyLength = AES_wrap_key(&aesKey, NULL, wrappedKeyOut, keyToWrap, keyToWrapLength);
    wrappedKeyOutSize = static_cast<unsigned int>(wrappedKeyLength);
    return wrappedKeyLength == (keyEncryptionKeyLength + 8);
}

bool BPSecManager::AesUnwrapKey(
    const uint8_t* keyEncryptionKey, const unsigned int keyEncryptionKeyLength,
    const uint8_t* keyToUnwrap, const unsigned int keyToUnwrapLength,
    uint8_t* unwrappedKeyOut, unsigned int& unwrappedKeyOutSize)
{
    AES_KEY aesKey;

    //AES_set_encrypt_key() and AES_set_decrypt_key() return 0 for success, -1 if userKey or key is NULL, or -2 if the number of bits is unsupported.
    if (AES_set_decrypt_key(keyEncryptionKey, static_cast<int>(keyEncryptionKeyLength << 3), &aesKey)) {
        return false;
    }

    //definition https://docs.huihoo.com/doxygen/openssl/1.0.1c/aes__wrap_8c_source.html
    //2nd parameter of NULL uses default iv of 0xA6, 0xA6, 0xA6, 0xA6, 0xA6, 0xA6, 0xA6, 0xA6
    //success should return keyEncryptionKeyLength, or -1 if failure
    const int unwrappedKeyLength = AES_unwrap_key(&aesKey, NULL, unwrappedKeyOut, keyToUnwrap, keyToUnwrapLength);
    unwrappedKeyOutSize = static_cast<unsigned int>(unwrappedKeyLength);
    return unwrappedKeyLength == keyEncryptionKeyLength;
}

//User of this function provided KEK (key encryption key) and AAD.
//Bundle provides AES wrapped key, AES variant, IV, tag, and cipherText.
//This function must unwrap key with KEK to get the DEK (data encryption key), then decrypt cipherText.
void BPSecManager::TryDecryptBundle(EvpCipherCtxWrapper& ctxWrapper,
    BundleViewV7& bv,
    const uint8_t* keyEncryptionKey, const unsigned int keyEncryptionKeyLength,
    const uint8_t* aad, const uint64_t aadLength,
    bool& hadError, bool& decryptionSuccessful)
{
    hadError = false;
    decryptionSuccessful = false;
    std::vector<BundleViewV7::Bpv7CanonicalBlockView*> blocks;
    bv.GetCanonicalBlocksByType(BPV7_BLOCK_TYPE_CODE::CONFIDENTIALITY, blocks);
    for (std::size_t i = 0; i < blocks.size(); ++i) {
        BundleViewV7::Bpv7CanonicalBlockView& blockView = *(blocks[i]);
        Bpv7BlockConfidentialityBlock* bcbPtr = dynamic_cast<Bpv7BlockConfidentialityBlock*>(blockView.headerPtr.get());
        if (!bcbPtr) {
            hadError = true;
            return;
        }
        std::vector<std::vector<uint8_t>*> patPtrs = bcbPtr->GetAllPayloadAuthenticationTagPtrs();
        if (patPtrs.size() != 1) {
            hadError = true;
            return;
        }
        const std::vector<uint8_t>& tag = *(patPtrs[0]);
        
        bool success;
        COSE_ALGORITHMS variant = bcbPtr->GetSecurityParameterAesVariant(success);
        if (!success) {
            hadError = true;
            return;
        }
        if ((variant == COSE_ALGORITHMS::A128GCM) || (variant == COSE_ALGORITHMS::A256GCM)) {
            //ok to continue
        }
        else {
            hadError = true;
            return;
        }
        const std::vector<uint8_t>* ivPtr = bcbPtr->GetInitializationVectorPtr();
        if (!ivPtr) {
            hadError = true;
            return;
        }
        
        const std::vector<uint8_t>* wrappedKeyPtr = bcbPtr->GetAesWrappedKeyPtr();
        if (!wrappedKeyPtr) {
            hadError = true;
            return;
        }

        //unwrap key: 
        uint8_t unwrappedKeyBytes[32 + 10]; //32 worse case for 32*8=256bit
        unsigned int unwrappedKeyOutSize;
        if (!BPSecManager::AesUnwrapKey(
            keyEncryptionKey, keyEncryptionKeyLength,
            wrappedKeyPtr->data(), static_cast<const unsigned int>(wrappedKeyPtr->size()),
            unwrappedKeyBytes, unwrappedKeyOutSize))
        {
            hadError = true;
            return;
        }
        const Bpv7AbstractSecurityBlock::security_targets_t& targets = bcbPtr->m_securityTargets;
        for (Bpv7AbstractSecurityBlock::security_targets_t::const_iterator stIt = targets.cbegin(); stIt != targets.cend(); ++stIt) {
            const uint64_t target = *stIt;
            BundleViewV7::Bpv7CanonicalBlockView* targetCanonicalBlockPtr = bv.GetCanonicalBlockByBlockNumber(target);
            if (!targetCanonicalBlockPtr) {
                hadError = true;
                return;
            }
            Bpv7CanonicalBlock& canonicalHeader = *(targetCanonicalBlockPtr->headerPtr);
            if (canonicalHeader.m_blockTypeCode == BPV7_BLOCK_TYPE_CODE::PAYLOAD) { //this blockView type will never be marked "dirty" and modifications will be done manually
                //overwrite cyphertext with plaintext in-place and compute crc
                uint64_t decryptedDataOutSize;
                //not inplace test (separate in and out buffers)
                if (!BPSecManager::AesGcmDecrypt(ctxWrapper,
                    canonicalHeader.m_dataPtr, canonicalHeader.m_dataLength,
                    unwrappedKeyBytes, unwrappedKeyOutSize,
                    ivPtr->data(), ivPtr->size(),
                    aad, aadLength, //affects tag only
                    tag.data(),
                    canonicalHeader.m_dataPtr, decryptedDataOutSize))
                {
                    hadError = true;
                    return;
                }
                int cborLengthFieldEncodedSizeIncrease = 0;
                const unsigned int cborLengthFieldEncodedSizeBefore = CborGetNumBytesRequiredToEncode(canonicalHeader.m_dataLength);
                const unsigned int cborLengthFieldEncodedSizeAfter = CborGetNumBytesRequiredToEncode(decryptedDataOutSize);
                const int64_t decryptSizeIncrease = (static_cast<int64_t>(decryptedDataOutSize)) - (static_cast<int64_t>(canonicalHeader.m_dataLength));
                if (cborLengthFieldEncodedSizeBefore == cborLengthFieldEncodedSizeAfter) { //in place will work
                    //this should be the most common case
                }
                else { //need to shift the payload data left or right by 1 byte because the cbor encoded length field grew or shrank by 1 byte
                    cborLengthFieldEncodedSizeIncrease = (static_cast<const int>(cborLengthFieldEncodedSizeAfter)) - (static_cast<const int>(cborLengthFieldEncodedSizeBefore));
                    //move decrypted data by one byte
                    std::memmove(canonicalHeader.m_dataPtr + cborLengthFieldEncodedSizeIncrease, canonicalHeader.m_dataPtr, canonicalHeader.m_dataLength);
                    //encode length field which is immediately before the "byte string"
                    CborEncodeU64(canonicalHeader.m_dataPtr - cborLengthFieldEncodedSizeBefore, decryptedDataOutSize, cborLengthFieldEncodedSizeAfter);
                }
                //change block serialization size
                const uint8_t* const blockSerializedBegin = (const uint8_t*)targetCanonicalBlockPtr->actualSerializedBlockPtr.data(); //won't change
                const std::size_t blockSerializedSize = targetCanonicalBlockPtr->actualSerializedBlockPtr.size() + cborLengthFieldEncodedSizeIncrease + decryptSizeIncrease;
                targetCanonicalBlockPtr->actualSerializedBlockPtr = boost::asio::buffer(blockSerializedBegin, blockSerializedSize);

                //recompute crc at end
                canonicalHeader.RecomputeCrcAfterDataModification(
                    (uint8_t*)targetCanonicalBlockPtr->actualSerializedBlockPtr.data(),
                    targetCanonicalBlockPtr->actualSerializedBlockPtr.size()); //recompute crc

                decryptionSuccessful = true;
            }
        }
        blockView.markedForDeletion = true;
    }
    if (decryptionSuccessful) { //at least one bcb was marked for deletion, so rerender
        if (!bv.RenderInPlace(128)) { //todo make PADDING_ELEMENTS_BEFORE
            hadError = true;
            return;
        }
    }
}

